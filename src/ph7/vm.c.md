# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 6915/8819 lines (78.41%)

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
|   920466 |   140 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |   141 |  |
|   920468 |   142 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       35 |   143 | `		return TRUE;` |
|        - |   144 | `	}` |
|   920434 |   145 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       11 |   146 | `		return TRUE;` |
|        - |   147 | `	}` |
|   920424 |   148 | `	return FALSE;` |
|   460257 |   149 |  |
|        - |   150 | `/*` |
|        - |   151 | ` * Return TRUE if the value should take the Perl-style string-increment path:` |
|        - |   152 | ` * any MEMOBJ_STRING that is empty, or whose contents are not a complete` |
|        - |   153 | ` * number (matching PHP's is_numeric semantics — the whole string must parse` |
|        - |   154 | ` * as a number, with optional surrounding whitespace).  Strings with a` |
|        - |   155 | ` * numeric prefix followed by non-whitespace bytes (e.g. "5foo") take the` |
|        - |   156 | ` * Perl path, like PHP.  Strict numeric strings ("5", "1.5", "5e2", "  5  ")` |
|        - |   157 | ` * still go through the existing numeric coercion.` |
|        - |   158 | ` */` |
|   335982 |   159 | `static int VmStringWantsPerlIncr(ph7_value *pVal)` |
|        2 |   160 |  |
|        - |   161 | `	SyString sStr;` |
|   335984 |   162 | `	sxu8 bReal = FALSE;` |
|   335984 |   163 | `	const char *zTail = 0;` |
|        - |   164 | `	const char *zEnd;` |
|   335984 |   165 | `	if( (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|   335914 |   166 | `		return FALSE;` |
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
|   168015 |   183 |  |
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
|        2 |   208 |  |
|        - |   209 | `	ph7_constant *pCons;` |
|        - |   210 | `	SyHashEntry *pEntry;` |
|        - |   211 | `	char *zDupName;` |
|        - |   212 | `	sxi32 rc;` |
|   634830 |   213 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   634830 |   214 | `	if( pEntry ){` |
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
|   634826 |   230 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   634826 |   231 | `	if( pCons == 0 ){` |
|      ! 0 |   232 | `		return 0;` |
|        - |   233 | `	}` |
|        - |   234 | `	/* Duplicate constant name */` |
|   634826 |   235 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   634826 |   236 | `	if( zDupName == 0 ){` |
|      ! 0 |   237 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   238 | `		return 0;` |
|        - |   239 | `	}` |
|        - |   240 | `	/* Install the constant */` |
|   634826 |   241 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   634826 |   242 | `	pCons->xExpand = xExpand;` |
|   634826 |   243 | `	pCons->pUserData = pUserData;` |
|   634826 |   244 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   634826 |   245 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   246 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   247 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   248 | `		return rc;` |
|        - |   249 | `	}` |
|        - |   250 | `	/* All done,constant can be invoked from PHP code */` |
|   634826 |   251 | `	return SXRET_OK;` |
|   317416 |   252 |  |
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
|        2 |   267 |  |
|        - |   268 | `	ph7_user_func *pFunc;` |
|        - |   269 | `	char *zDup;` |
|        - |   270 | `	/* Allocate a new user function */` |
|  1403130 |   271 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  1403130 |   272 | `	if( pFunc == 0 ){` |
|      ! 0 |   273 | `		return SXERR_MEM;` |
|        - |   274 | `	}` |
|        - |   275 | `	/* Duplicate function name */` |
|  1403130 |   276 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1403130 |   277 | `	if( zDup == 0 ){` |
|      ! 0 |   278 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   279 | `		return SXERR_MEM;` |
|        - |   280 | `	}` |
|        - |   281 | `	/* Zero the structure */` |
|  1403130 |   282 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   283 | `	/* Initialize structure fields */` |
|  1403130 |   284 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  1403130 |   285 | `	pFunc->pVm   = pVm;` |
|  1403130 |   286 | `	pFunc->xFunc = xFunc;` |
|  1403130 |   287 | `	pFunc->pUserData = pUserData;` |
|  1403130 |   288 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   289 | `	/* Write a pointer to the new function */` |
|  1403130 |   290 | `	*ppOut = pFunc;` |
|  1403130 |   291 | `	return SXRET_OK;` |
|   701566 |   292 |  |
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
|        2 |   307 |  |
|        - |   308 | `	ph7_user_func *pFunc;` |
|        - |   309 | `	SyHashEntry *pEntry;` |
|        - |   310 | `	sxi32 rc;` |
|        - |   311 | `	/* Overwrite any previously registered function with the same name */` |
|  1405964 |   312 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  1405964 |   313 | `	if( pEntry ){` |
|     2836 |   314 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2836 |   315 | `		pFunc->pUserData = pUserData;` |
|     2836 |   316 | `		pFunc->xFunc = xFunc;` |
|     2836 |   317 | `		SySetReset(&pFunc->aAux);` |
|     2836 |   318 | `		return SXRET_OK;` |
|        - |   319 | `	}` |
|        - |   320 | `	/* Create a new user function */` |
|  1403130 |   321 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  1403130 |   322 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   323 | `		return rc;` |
|        - |   324 | `	}` |
|        - |   325 | `	/* Install the function in the corresponding hashtable */` |
|  1403130 |   326 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  1403130 |   327 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   328 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   329 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   330 | `		return rc;` |
|        - |   331 | `	}` |
|        - |   332 | `	/* User function successfully installed */` |
|  1403130 |   333 | `	return SXRET_OK;` |
|   702983 |   334 |  |
|        - |   335 | `/*` |
|        - |   336 | ` * Initialize a VM function.` |
|        - |   337 | ` */` |
|   279032 |   338 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   339 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   340 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   341 | `	const char *zName,  /* Function name */` |
|        - |   342 | `	sxu32 nByte,        /* zName length */` |
|        - |   343 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   344 | `	void *pUserData     /* Function private data */` |
|        - |   345 | `	)` |
|        2 |   346 |  |
|        - |   347 | `	/* Zero the structure */` |
|   279034 |   348 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   349 | `	/* Initialize structure fields */` |
|        - |   350 | `	/* Arguments container */` |
|   279034 |   351 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   352 | `	/* Static variable container */` |
|   279034 |   353 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   354 | `	/* Bytecode container */` |
|   279034 |   355 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   356 | `    /* Preallocate some instruction slots */` |
|   279034 |   357 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   358 | `	/* Closure environment */` |
|   279034 |   359 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        - |   360 | `	/* Return-type union alternatives (empty unless declared as a union) */` |
|   279034 |   361 | `	SySetInit(&pFunc->aReturnUnion,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|   279034 |   362 | `	pFunc->iFlags = iFlags;` |
|   279034 |   363 | `	pFunc->pUserData = pUserData;` |
|        - |   364 | `	/* Capture the defining file's strict_types mode. PHP scopes return-type` |
|        - |   365 | `	 * coercion by the callee's file, so we freeze it at definition time. */` |
|   279034 |   366 | `	pFunc->bStrictTypes = (sxu8)(pVm->sCodeGen.bStrictTypes ? 1 : 0);` |
|   279034 |   367 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   279034 |   368 | `	return SXRET_OK;` |
|        2 |   369 |  |
|        - |   370 | `/*` |
|        - |   371 | ` * Namespace-aware function lookup.` |
|        - |   372 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   373 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   374 | ` */` |
|        - |   375 | `/*` |
|        - |   376 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   377 | ` */` |
|  1461128 |   378 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   379 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   380 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   381 | `	SyString *pName     /* Function name */` |
|        - |   382 | `	)` |
|        2 |   383 |  |
|        - |   384 | `	SyHashEntry *pEntry;` |
|        - |   385 | `	sxi32 rc;` |
|  1461130 |   386 | `	if( pName == 0 ){` |
|        - |   387 | `		/* Use the built-in name */` |
|    42064 |   388 | `		pName = &pFunc->sName;` |
|    21031 |   389 | `	}` |
|        - |   390 | `	/* Check for duplicates (functions with the same name) first */` |
|  1461130 |   391 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|  1461130 |   392 | `	if( pEntry ){` |
|  1264232 |   393 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|  1264232 |   394 | `		if( pLink != pFunc ){` |
|        - |   395 | `			/* Link */` |
|      188 |   396 | `			pFunc->pNextName = pLink;` |
|      188 |   397 | `			pEntry->pUserData = pFunc;` |
|       93 |   398 | `		}` |
|  1264232 |   399 | `		return SXRET_OK;` |
|        - |   400 | `	}` |
|        - |   401 | `	/* First time seen */` |
|   196900 |   402 | `	pFunc->pNextName = 0;` |
|   196900 |   403 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   196900 |   404 | `	return rc;` |
|   730566 |   405 |  |
|        - |   406 | `/*` |
|        - |   407 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   408 | ` */` |
|   120652 |   409 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   410 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   411 | `	ph7_class *pClass /* Target Class */` |
|        - |   412 | `	)` |
|        2 |   413 |  |
|   120654 |   414 | `	SyString *pName = &pClass->sName;` |
|        - |   415 | `	SyHashEntry *pEntry;` |
|        - |   416 | `	sxi32 rc;` |
|        - |   417 | `	/* Check for duplicates */` |
|   120654 |   418 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|   120654 |   419 | `	if( pEntry ){` |
|       31 |   420 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   421 | `		/* Link entry with the same name */` |
|       31 |   422 | `		pClass->pNextName = pLink;` |
|       31 |   423 | `		pEntry->pUserData = pClass;` |
|       31 |   424 | `		return SXRET_OK;` |
|        - |   425 | `	}` |
|   120624 |   426 | `	pClass->pNextName = 0;` |
|        - |   427 | `	/* Perform a simple hashtable insertion */` |
|   120624 |   428 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|   120624 |   429 | `	return rc;` |
|    60328 |   430 |  |
|        - |   431 | `/*` |
|        - |   432 | ` * Instruction builder interface.` |
|        - |   433 | ` */` |
|  4271196 |   434 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
|        - |   435 | `	ph7_vm *pVm,  /* Target VM */` |
|        - |   436 | `	sxi32 iOp,    /* Operation to perform */` |
|        - |   437 | `	sxi32 iP1,    /* First operand */` |
|        - |   438 | `	sxu32 iP2,    /* Second operand */` |
|        - |   439 | `	void *p3,     /* Third operand */` |
|        - |   440 | `	sxu32 *pIndex /* Instruction index. NULL otherwise */` |
|        - |   441 | `	)` |
|        2 |   442 |  |
|        - |   443 | `	VmInstr sInstr;` |
|        - |   444 | `	sxi32 rc;` |
|        - |   445 | `	/* Fill the VM instruction */` |
|  4271198 |   446 | `	sInstr.iOp = (sxu8)iOp;` |
|  4271198 |   447 | `	sInstr.iP1 = iP1;` |
|  4271198 |   448 | `	sInstr.iP2 = iP2;` |
|  4271198 |   449 | `	sInstr.p3  = p3;` |
|  4271198 |   450 | `	if( pIndex ){` |
|        - |   451 | `		/* Instruction index in the bytecode array */` |
|   231948 |   452 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|   115973 |   453 | `	}` |
|        - |   454 | `	/* Finally,record the instruction */` |
|  4271198 |   455 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  4271198 |   456 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   457 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   458 | `		/* Fall throw */` |
|      ! 0 |   459 | `	}` |
|  4271198 |   460 | `	return rc;` |
|        2 |   461 |  |
|        - |   462 | `/*` |
|        - |   463 | ` * Swap the current bytecode container with the given one.` |
|        - |   464 | ` */` |
|   554224 |   465 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   466 |  |
|   554226 |   467 | `	if( pContainer == 0 ){` |
|        - |   468 | `		/* Point to the default container */` |
|      ! 0 |   469 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   470 | `	}else{` |
|        - |   471 | `		/* Change container */` |
|   554226 |   472 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   473 | `	}` |
|   554226 |   474 | `	return SXRET_OK;` |
|        2 |   475 |  |
|        - |   476 | `/*` |
|        - |   477 | ` * Return the current bytecode container.` |
|        - |   478 | ` */` |
|   277112 |   479 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   480 |  |
|   277114 |   481 | `	return pVm->pByteContainer;` |
|        2 |   482 |  |
|        - |   483 | `/*` |
|        - |   484 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   485 | ` */` |
|   228722 |   486 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   487 |  |
|        - |   488 | `	VmInstr *pInstr;` |
|   228724 |   489 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   228724 |   490 | `	return pInstr;` |
|        2 |   491 |  |
|        - |   492 | `/*` |
|        - |   493 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   494 | ` */` |
|  1282790 |   495 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   496 |  |
|  1282792 |   497 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   498 |  |
|        - |   499 | `/*` |
|        - |   500 | ` * Pop the last VM instruction.` |
|        - |   501 | ` */` |
|   211548 |   502 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   503 |  |
|   211550 |   504 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   505 |  |
|        - |   506 | `/*` |
|        - |   507 | ` * Peek the last VM instruction.` |
|        - |   508 | ` */` |
|   841046 |   509 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   510 |  |
|   841048 |   511 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   512 |  |
|    33592 |   513 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   514 |  |
|        - |   515 | `	VmInstr *aInstr;` |
|        - |   516 | `	sxu32 n;` |
|    33594 |   517 | `	n = SySetUsed(pVm->pByteContainer);` |
|    33594 |   518 | `	if( n < 2 ){` |
|      ! 0 |   519 | `		return 0;` |
|        - |   520 | `	}` |
|    33594 |   521 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    33594 |   522 | `	return &aInstr[n - 2];` |
|    16798 |   523 |  |
|        - |   524 | `/*` |
|        - |   525 | ` * Allocate a new virtual machine frame.` |
|        - |   526 | ` */` |
|    23066 |   527 | `static VmFrame * VmNewFrame(` |
|        - |   528 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   529 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   530 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   531 | `	)` |
|        2 |   532 |  |
|        - |   533 | `	VmFrame *pFrame;` |
|        - |   534 | `	/* Allocate a new vm frame */` |
|    23068 |   535 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    23068 |   536 | `	if( pFrame == 0 ){` |
|      ! 0 |   537 | `		return 0;` |
|        - |   538 | `	}` |
|        - |   539 | `	/* Zero the structure */` |
|    23068 |   540 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   541 | `	/* Initialize frame fields */` |
|    23068 |   542 | `	pFrame->pUserData = pUserData;` |
|    23068 |   543 | `	pFrame->pThis = pThis;` |
|    23068 |   544 | `	pFrame->pVm = pVm;` |
|    23068 |   545 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    23068 |   546 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    23068 |   547 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    23068 |   548 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    23068 |   549 | `	return pFrame;` |
|    11535 |   550 |  |
|        - |   551 | `/* Forward declaration */` |
|        - |   552 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   553 | `/*` |
|        - |   554 | ` * Enter a VM frame.` |
|        - |   555 | ` */` |
|    22994 |   556 | `static sxi32 VmEnterFrame(` |
|        - |   557 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   558 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   559 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   560 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   561 | `	)` |
|        2 |   562 |  |
|        - |   563 | `	VmFrame *pFrame;` |
|        - |   564 | `	/* Allocate a new frame */` |
|    22996 |   565 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    22996 |   566 | `	if( pFrame == 0 ){` |
|      ! 0 |   567 | `		return SXERR_MEM;` |
|        - |   568 | `	}` |
|        - |   569 | `	/* Link to the list of active VM frame */` |
|    22996 |   570 | `	pFrame->pParent = pVm->pFrame;` |
|    22996 |   571 | `	pVm->pFrame = pFrame;` |
|    22996 |   572 | `	if( ppFrame ){` |
|        - |   573 | `		/* Write a pointer to the new VM frame */` |
|    19842 |   574 | `		*ppFrame = pFrame;` |
|     9920 |   575 | `	}` |
|    22996 |   576 | `	return SXRET_OK;` |
|    11499 |   577 |  |
|        - |   578 | `/*` |
|        - |   579 | ` * Link a foreign variable with the TOP most active frame.` |
|        - |   580 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|        - |   581 | ` * information.` |
|        - |   582 | ` */` |
|       70 |   583 | `static sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|        2 |   584 |  |
|        - |   585 | `	VmFrame *pTarget,*pFrame;` |
|       72 |   586 | `	SyHashEntry *pEntry = 0;` |
|        - |   587 | `	sxi32 rc;` |
|        - |   588 | `	/* Point to the upper frame */` |
|       72 |   589 | `	pFrame = pVm->pFrame;` |
|       72 |   590 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       72 |   591 | `	pTarget = pFrame;` |
|       72 |   592 | `	pFrame = pTarget->pParent;` |
|       72 |   593 | `	while( pFrame ){` |
|       72 |   594 | `		if( (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   595 | `			/* Query the current frame */` |
|       72 |   596 | `			pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|       72 |   597 | `			if( pEntry ){` |
|        - |   598 | `				/* Variable found */` |
|       72 |   599 | `				break;` |
|        - |   600 | `			}` |
|      ! 0 |   601 | `		}` |
|        - |   602 | `		/* Point to the upper frame */` |
|      ! 0 |   603 | `		pFrame = pFrame->pParent;` |
|      ! 0 |   604 | `	}` |
|       72 |   605 | `	if( pEntry == 0 ){` |
|        - |   606 | `		/* Inexistant variable */` |
|      ! 0 |   607 | `		return SXERR_NOTFOUND;` |
|        - |   608 | `	}` |
|        - |   609 | `	/* Link to the current frame */` |
|       72 |   610 | `	rc = SyHashInsert(&pTarget->hVar,pEntry->pKey,pEntry->nKeyLen,pEntry->pUserData);` |
|       72 |   611 | `	if( rc == SXRET_OK ){` |
|        - |   612 | `		sxu32 nIdx;` |
|       72 |   613 | `		nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|       72 |   614 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pTarget->hVar),0,0);` |
|       35 |   615 | `	}` |
|       72 |   616 | `	return rc;` |
|       37 |   617 |  |
|        - |   618 | `/*` |
|        - |   619 | ` * Leave the top-most active frame.` |
|        - |   620 | ` */` |
|    19836 |   621 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   622 |  |
|    19838 |   623 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    19838 |   624 | `	if( pCurFrame ){` |
|        - |   625 | `		/* Unlink from the list of active VM frame */` |
|    19838 |   626 | `		pVm->pFrame = pCurFrame->pParent;` |
|    19838 |   627 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   628 | `			VmSlot  *aSlot;` |
|        - |   629 | `			sxu32 n;` |
|        - |   630 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    19444 |   631 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|   125740 |   632 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   633 | `				/* Unset the local variable */` |
|   106298 |   634 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    53150 |   635 | `			}` |
|        - |   636 | `			/* Remove local reference */` |
|    19444 |   637 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|   125814 |   638 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|   106372 |   639 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    53187 |   640 | `			}` |
|     9721 |   641 | `		}` |
|        - |   642 | `		/* Release internal containers */` |
|    19838 |   643 | `		SyHashRelease(&pCurFrame->hVar);` |
|    19838 |   644 | `		SySetRelease(&pCurFrame->sArg);` |
|    19838 |   645 | `		SySetRelease(&pCurFrame->sLocal);` |
|    19838 |   646 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   647 | `		/* Release the whole structure */` |
|    19838 |   648 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     9918 |   649 | `	}` |
|    19838 |   650 |  |
|        - |   651 | `/*` |
|        - |   652 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   653 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   654 | ` * should be skipped when looking for the real execution context.` |
|        - |   655 | ` */` |
|  7143766 |   656 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   657 |  |
|  7146028 |   658 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|     2262 |   659 | `		pFrame = pFrame->pParent;` |
|        2 |   660 | `	}` |
|  7143768 |   661 | `	return pFrame;` |
|        2 |   662 |  |
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
|        - |   683 | ` * Compare two functions signature and return the comparison result.` |
|        - |   684 | ` */` |
|      836 |   685 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|        1 |   686 |  |
|      837 |   687 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|      837 |   688 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|      837 |   689 | `	const char *zSin = pSecond->zString;` |
|      837 |   690 | `	const char *zFin = pFirst->zString;` |
|      837 |   691 | `	const char *zPtr = zFin;` |
|      421 |   692 | `	for(;;){` |
|      843 |   693 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|      413 |   694 | `			break;` |
|        - |   695 | `		}` |
|       19 |   696 | `		if( zFin[0] != zSin[0] ){` |
|        - |   697 | `			/* mismatch */` |
|       13 |   698 | `			break;` |
|        - |   699 | `		}` |
|        7 |   700 | `		zFin++;` |
|        7 |   701 | `		zSin++;` |
|        1 |   702 | `	}` |
|      837 |   703 | `	return (int)(zFin-zPtr);` |
|        1 |   704 |  |
|        - |   705 | `/*` |
|        - |   706 | ` * Select the appropriate VM function for the current call context.` |
|        - |   707 | ` * This is the implementation of the powerful 'function overloading' feature` |
|        - |   708 | ` * introduced by the version 2 of the PH7 engine.` |
|        - |   709 | ` * Refer to the official documentation for more information.` |
|        - |   710 | ` */` |
|      138 |   711 | `static ph7_vm_func * VmOverload(` |
|        - |   712 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |   713 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|        - |   714 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|        - |   715 | `	int nArg             /* Total number of passed arguments  */` |
|        - |   716 | `	)` |
|        2 |   717 |  |
|        - |   718 | `	int iTarget,i,j,iCur,iMax;` |
|        - |   719 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|        - |   720 | `	ph7_vm_func *pLink;` |
|        - |   721 | `	SyString sArgSig;` |
|        - |   722 | `	SyBlob sSig;` |
|        - |   723 |  |
|      140 |   724 | `	pLink = pList;` |
|      140 |   725 | `	i = 0;` |
|        - |   726 | `	/* Put functions expecting the same number of passed arguments */` |
|     1086 |   727 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|     1024 |   728 | `		if( pLink == 0 ){` |
|       78 |   729 | `			break;` |
|        - |   730 | `		}` |
|      948 |   731 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|        - |   732 | `			/* Candidate for overloading */` |
|      902 |   733 | `			apSet[i++] = pLink;` |
|      450 |   734 | `		}` |
|        - |   735 | `		/* Point to the next entry */` |
|      948 |   736 | `		pLink = pLink->pNextName;` |
|        2 |   737 | `	}` |
|      140 |   738 | `	if( i < 1 ){` |
|        - |   739 | `		/* No candidates,return the head of the list */` |
|      ! 0 |   740 | `		return pList;` |
|        - |   741 | `	}` |
|      140 |   742 | `	if( nArg < 1 \|\| i < 2 ){` |
|        - |   743 | `		/* Return the only candidate */` |
|       32 |   744 | `		return apSet[0];` |
|        - |   745 | `	}` |
|        - |   746 | `	/* Calculate function signature */` |
|      109 |   747 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|      367 |   748 | `	for( j = 0 ; j < nArg ; j++ ){` |
|      259 |   749 | `		int c = 'n'; /* null */` |
|      259 |   750 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|        - |   751 | `			/* Hashmap */` |
|       45 |   752 | `			c = 'h';` |
|      237 |   753 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|        - |   754 | `			/* bool */` |
|      ! 0 |   755 | `			c = 'b';` |
|      215 |   756 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|        - |   757 | `			/* int */` |
|        7 |   758 | `			c = 'i';` |
|      212 |   759 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|        - |   760 | `			/* String */` |
|      107 |   761 | `			c = 's';` |
|      156 |   762 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|        - |   763 | `			/* Float */` |
|      ! 0 |   764 | `			c = 'f';` |
|      103 |   765 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|        - |   766 | `			/* Class instance — prefix with 'o' to match formal object/class signatures */` |
|        3 |   767 | `			int marker = 'o';` |
|        3 |   768 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|        3 |   769 | `			SyString *pName = &pClass->sName;` |
|        3 |   770 | `			SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|        3 |   771 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|        3 |   772 | `			c = -1;` |
|        1 |   773 | `		}` |
|      259 |   774 | `		if( c > 0 ){` |
|      257 |   775 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|      128 |   776 | `		}` |
|      130 |   777 | `	}` |
|      109 |   778 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|      109 |   779 | `	iTarget = 0;` |
|      109 |   780 | `	iMax = -1;` |
|        - |   781 | `	/* Select the appropriate function */` |
|      945 |   782 | `	for( j = 0 ; j < i ; j++ ){` |
|        - |   783 | `		/* Compare the two signatures */` |
|      837 |   784 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|      837 |   785 | `		if( iCur > iMax ){` |
|      113 |   786 | `			iMax = iCur;` |
|      113 |   787 | `			iTarget = j;` |
|       56 |   788 | `		}` |
|      419 |   789 | `	}` |
|      109 |   790 | `	SyBlobRelease(&sSig);` |
|        - |   791 | `	/* Appropriate function for the current call context */` |
|      109 |   792 | `	return apSet[iTarget];` |
|       71 |   793 |  |
|        - |   794 | `/* Forward declaration */` |
|        - |   795 | `/* VmLocalExec and VmErrorFormat forward declarations removed - now PH7_PRIVATE in ph7int.h */` |
|        - |   796 | `/*` |
|        - |   797 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|        - |   798 | ` * it can be instanciated from the executed PHP script.` |
|        - |   799 | ` */` |
|        - |   800 | `/*` |
|        - |   801 | ` * Reserve and initialize the static/constant attribute slots of a class.` |
|        - |   802 | ` * This is the per-execution part of mounting a class: every static/const` |
|        - |   803 | ` * attribute gets a fresh memory object, its default initializer is run, the` |
|        - |   804 | ` * slot is pinned in the reference table (VM_REF_IDX_KEEP) and typed static` |
|        - |   805 | ` * properties register their enforcement slot. It is factored out of` |
|        - |   806 | ` * VmMountUserClass() so that ph7_vm_reset() can rebuild these slots on a VM` |
|        - |   807 | ` * reuse without re-installing the (compile-time) methods.` |
|        - |   808 | ` */` |
|   355908 |   809 | `static sxi32 VmMountUserClassAttrs(` |
|        - |   810 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   811 | `	ph7_class *pClass /* Class whose static/const attributes are mounted */` |
|        - |   812 | `	)` |
|        2 |   813 |  |
|        - |   814 | `	ph7_class_attr *pAttr;` |
|        - |   815 | `	SyHashEntry *pEntry;` |
|        - |   816 | `	/* Reset the loop cursor */` |
|   355910 |   817 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   818 | `	/* Process only static and constant attribute */` |
|  1405818 |   819 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   820 | `		/* Extract the current attribute */` |
|   871956 |   821 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   871956 |   822 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   823 | `			ph7_value *pMemObj;` |
|        - |   824 | `			/* Reserve a memory object for this constant/static attribute */` |
|     1844 |   825 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1844 |   826 | `			if( pMemObj == 0 ){` |
|      ! 0 |   827 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   828 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   829 | `					&pClass->sName,&pAttr->sName` |
|        - |   830 | `					);` |
|      ! 0 |   831 | `				return SXERR_MEM;` |
|        - |   832 | `			}` |
|     1844 |   833 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   834 | `				/* Initialize attribute default value (any complex expression) */` |
|     1840 |   835 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      919 |   836 | `			}` |
|        - |   837 | `			/* Record attribute index */` |
|     1844 |   838 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   839 | `			/* Install static attribute in the reference table */` |
|     1844 |   840 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   841 | `			/* If this is a typed static property, register the slot so the` |
|        - |   842 | `			 * STORE path can enforce the declared type. We allocate a tiny` |
|        - |   843 | `			 * VmClassAttr to uniformize with instance properties; the key` |
|        - |   844 | `			 * points at its own nIdx field (stable for the VM lifetime). */` |
|     1844 |   845 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|       16 |   846 | `				VmClassAttr *pVmAttrS = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|       16 |   847 | `				if( pVmAttrS == 0 ){` |
|      ! 0 |   848 | `					return SXERR_MEM;` |
|        - |   849 | `				}` |
|       16 |   850 | `				pVmAttrS->pAttr = pAttr;` |
|       16 |   851 | `				pVmAttrS->nIdx = pMemObj->nIdx;` |
|       16 |   852 | `				pVmAttrS->iState = 0;` |
|       16 |   853 | `				pVmAttrS->pOwner = pClass;` |
|        - |   854 | `				/* Static typed property with no default starts uninitialized */` |
|       14 |   855 | `				if( SySetUsed(&pAttr->aByteCode) == 0` |
|       11 |   856 | `				 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        6 |   857 | `					pVmAttrS->iState \|= VM_CLASS_ATTR_UNINIT;` |
|        2 |   858 | `				}` |
|       16 |   859 | `				if( SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttrS->nIdx,sizeof(sxu32),pVmAttrS) != SXRET_OK ){` |
|      ! 0 |   860 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttrS);` |
|      ! 0 |   861 | `					return SXERR_MEM;` |
|        - |   862 | `				}` |
|        7 |   863 | `			}` |
|      921 |   864 | `		}` |
|        2 |   865 | `	}` |
|   355910 |   866 | `	return SXRET_OK;` |
|   177956 |   867 |  |
|   355676 |   868 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   869 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   870 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   871 | `	)` |
|        2 |   872 |  |
|        - |   873 | `	ph7_class_method *pMeth;` |
|        - |   874 | `	SyHashEntry *pEntry;` |
|        - |   875 | `	sxi32 rc;` |
|        - |   876 | `	/* Reserve/initialize the static and constant attribute slots */` |
|   355678 |   877 | `	rc = VmMountUserClassAttrs(&(*pVm),pClass);` |
|   355678 |   878 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   879 | `		return rc;` |
|        - |   880 | `	}` |
|        - |   881 | `	/* Install class methods */` |
|   355678 |   882 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   883 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   884 | `		 */` |
|   193296 |   885 | `		return SXRET_OK;` |
|        - |   886 | `	}` |
|        - |   887 | `	/* Create constructor alias if not yet done */` |
|   162384 |   888 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   889 | `		/* User constructor with the same base class name */` |
|     6704 |   890 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|     6704 |   891 | `		if( pEntry ){` |
|      ! 0 |   892 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   893 | `			/* Create the alias */` |
|      ! 0 |   894 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   895 | `		}` |
|     3351 |   896 | `	}` |
|        - |   897 | `	/* Install the methods now */` |
|   162384 |   898 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  1662649 |   899 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  1419076 |   900 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  1419076 |   901 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|  1419068 |   902 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|  1419068 |   903 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   904 | `				return rc;` |
|        - |   905 | `			}` |
|   709533 |   906 | `		}` |
|        2 |   907 | `	}` |
|        - |   908 | `	/* Mark class as mounted to avoid redundant mounting */` |
|   162384 |   909 | `	pClass->bMounted = TRUE;` |
|   162384 |   910 | `	return SXRET_OK;` |
|   177840 |   911 |  |
|        - |   912 | `/*` |
|        - |   913 | ` * Allocate a private frame for attributes of the given` |
|        - |   914 | ` * class instance (Object in the PHP jargon).` |
|        - |   915 | ` */` |
|     2164 |   916 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   917 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   918 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   919 | `	)` |
|        2 |   920 |  |
|     2166 |   921 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   922 | `	ph7_class_attr *pAttr;` |
|        - |   923 | `	SyHashEntry *pEntry;` |
|        - |   924 | `	sxi32 rc;` |
|        - |   925 | `	/* Install class attribute in the private frame associated with this instance */` |
|     2166 |   926 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     8938 |   927 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   928 | `		VmClassAttr *pVmAttr;` |
|        - |   929 | `		/* Extract the current attribute */` |
|     6774 |   930 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     6774 |   931 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     6774 |   932 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   933 | `			return SXERR_MEM;` |
|        - |   934 | `		}` |
|     6774 |   935 | `		pVmAttr->pAttr = pAttr;` |
|     6774 |   936 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   937 | `			ph7_value *pMemObj;` |
|        - |   938 | `			/* Reserve a memory object for this attribute */` |
|     6748 |   939 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     6748 |   940 | `			if( pMemObj == 0 ){` |
|      ! 0 |   941 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   942 | `				return SXERR_MEM;` |
|        - |   943 | `			}` |
|     6748 |   944 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     6748 |   945 | `			pVmAttr->iState = 0;` |
|     6748 |   946 | `			pVmAttr->pOwner = pClass;` |
|     6748 |   947 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   948 | `				/* Initialize attribute default value (any complex expression) */` |
|     2320 |   949 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|     5589 |   950 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        - |   951 | `				/* Typed property without a default: mark uninitialized. Reading` |
|        - |   952 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|       74 |   953 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|       36 |   954 | `			}` |
|     6748 |   955 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     6748 |   956 | `			if( rc != SXRET_OK ){` |
|        - |   957 | `				VmSlot sSlot;` |
|        - |   958 | `				/* Restore memory object */` |
|      ! 0 |   959 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   960 | `				sSlot.pUserData = 0;` |
|      ! 0 |   961 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   962 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   963 | `				return SXERR_MEM;` |
|        - |   964 | `			}` |
|        - |   965 | `			/* Install attribute in the reference table */` |
|     6748 |   966 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   967 | `			/* Register typed property slot for assignment-time enforcement.` |
|        - |   968 | `			 * On failure roll back the just-installed hAttr entry and the` |
|        - |   969 | `			 * reserved memobj so the caller sees a consistent instance. */` |
|     6748 |   970 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      182 |   971 | `				rc = SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr);` |
|      182 |   972 | `				if( rc != SXRET_OK ){` |
|        - |   973 | `					VmSlot sSlot;` |
|      ! 0 |   974 | `					SyHashDeleteEntry(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),0);` |
|      ! 0 |   975 | `					sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   976 | `					sSlot.pUserData = 0;` |
|      ! 0 |   977 | `					SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   978 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   979 | `					return SXERR_MEM;` |
|        - |   980 | `				}` |
|       90 |   981 | `			}` |
|     3375 |   982 | `		}else{` |
|        - |   983 | `			/* Install static/constant attribute */` |
|       28 |   984 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|       28 |   985 | `			pVmAttr->iState = 0;` |
|       28 |   986 | `			pVmAttr->pOwner = pClass;` |
|       28 |   987 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|       28 |   988 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   989 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   990 | `				return SXERR_MEM;` |
|        - |   991 | `			}` |
|        - |   992 | `		}` |
|        2 |   993 | `	}` |
|     2166 |   994 | `	return SXRET_OK;` |
|     1084 |   995 |  |
|        - |   996 | `/* Forward declaration */` |
|        - |   997 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx);` |
|        - |   998 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef);` |
|        - |   999 | `/*` |
|        - |  1000 | ` * Dummy read-only buffer used for slot reservation.` |
|        - |  1001 | ` */` |
|        - |  1002 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|        - |  1003 | `/*` |
|        - |  1004 | ` * Reserve a constant memory object.` |
|        - |  1005 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1006 | ` */` |
|   457518 |  1007 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |  1008 |  |
|        - |  1009 | `	ph7_value *pObj;` |
|        - |  1010 | `	sxi32 rc;` |
|   457520 |  1011 | `	if( pIndex ){` |
|        - |  1012 | `		/* Object index in the object table */` |
|   448076 |  1013 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   224037 |  1014 | `	}` |
|        - |  1015 | `	/* Reserve a slot for the new object */` |
|   457520 |  1016 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   457520 |  1017 | `	if( rc != SXRET_OK ){` |
|        - |  1018 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  1019 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  1020 | `		 */` |
|      ! 0 |  1021 | `		return 0;` |
|        - |  1022 | `	}` |
|   457520 |  1023 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   457520 |  1024 | `	return pObj;` |
|   228761 |  1025 |  |
|        - |  1026 | `/*` |
|        - |  1027 | ` * Reserve a memory object.` |
|        - |  1028 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1029 | ` */` |
|  2152104 |  1030 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |  1031 |  |
|        - |  1032 | `	ph7_value *pObj;` |
|        - |  1033 | `	sxi32 rc;` |
|  2152106 |  1034 | `	if( pIndex ){` |
|        - |  1035 | `		/* Object index in the object table */` |
|  2152106 |  1036 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1076052 |  1037 | `	}` |
|        - |  1038 | `	/* Reserve a slot for the new object */` |
|  2152106 |  1039 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2152106 |  1040 | `	if( rc != SXRET_OK ){` |
|        - |  1041 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  1042 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  1043 | `		 */` |
|      ! 0 |  1044 | `		return 0;` |
|        - |  1045 | `	}` |
|  2152106 |  1046 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2152106 |  1047 | `	return pObj;` |
|  1076054 |  1048 |  |
|        - |  1049 | `/* Forward declaration */` |
|        - |  1050 | `static sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);` |
|        - |  1051 | `/* Forward declarations for Fiber C functions */` |
|        - |  1052 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1053 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1054 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1055 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1056 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1057 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1058 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1059 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1060 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1061 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1062 | `/* Forward declarations for Fiber/Generator infrastructure */` |
|        - |  1063 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc);` |
|        - |  1064 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |  1065 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |  1066 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg);` |
|        - |  1067 | `static sxi32 VmCallClassMethodWithMap(ph7_vm *pVm, ph7_class_instance *pThis,` |
|        - |  1068 | `	ph7_class_method *pMethod, ph7_value *pResult, int nArg,` |
|        - |  1069 | `	ph7_value **apArg, VmCallArgMap *pMap);` |
|        - |  1070 | `static sxi32 VmCallObjectInvoke(ph7_vm *pVm, ph7_class_instance *pThis,` |
|        - |  1071 | `	int nArg, ph7_value **apArg, ph7_value *pResult, VmCallArgMap *pMap);` |
|        - |  1072 | `static sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis);` |
|        - |  1073 | `/* Forward declarations for Generator helpers and C functions */` |
|        - |  1074 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |  1075 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen);` |
|        - |  1076 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1077 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1078 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1079 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1080 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1081 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1082 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1083 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1084 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1085 | `/*` |
|        - |  1086 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |  1087 | ` * directly as foreign functions.` |
|        - |  1088 | ` */` |
|        - |  1089 | `#define PH7_BUILTIN_LIB \` |
|        - |  1090 | `	"interface Throwable {"\` |
|        - |  1091 | `	"public function getMessage();"\` |
|        - |  1092 | `	"public function getCode();"\` |
|        - |  1093 | `	"public function getFile();"\` |
|        - |  1094 | `	"public function getLine();"\` |
|        - |  1095 | `	"public function getTrace();"\` |
|        - |  1096 | `	"public function getTraceAsString();"\` |
|        - |  1097 | `	"public function getPrevious();"\` |
|        - |  1098 | `	"public function __toString();"\` |
|        - |  1099 | `	"}"\` |
|        - |  1100 | `	"interface Traversable {}"\` |
|        - |  1101 | `	"interface ArrayAccess {"\` |
|        - |  1102 | `	"public function offsetExists($offset);"\` |
|        - |  1103 | `	"public function offsetGet($offset);"\` |
|        - |  1104 | `	"public function offsetSet($offset, $value);"\` |
|        - |  1105 | `	"public function offsetUnset($offset);"\` |
|        - |  1106 | `	"}"\` |
|        - |  1107 | `	"interface Countable {"\` |
|        - |  1108 | `	"public function count();"\` |
|        - |  1109 | `	"}"\` |
|        - |  1110 | `	"interface Stringable {"\` |
|        - |  1111 | `	"public function __toString();"\` |
|        - |  1112 | `	"}"\` |
|        - |  1113 | `	"interface JsonSerializable {"\` |
|        - |  1114 | `	"public function jsonSerialize();"\` |
|        - |  1115 | `	"}"\` |
|        - |  1116 | `	"interface UnitEnum {"\` |
|        - |  1117 | `	"public static function cases();"\` |
|        - |  1118 | `	"}"\` |
|        - |  1119 | `	"interface BackedEnum extends UnitEnum {"\` |
|        - |  1120 | `	"public static function from($value);"\` |
|        - |  1121 | `	"public static function tryFrom($value);"\` |
|        - |  1122 | `	"}"\` |
|        - |  1123 | `	"class Exception implements Throwable { "\` |
|        - |  1124 | `    "protected $message = '';"\` |
|        - |  1125 | `    "protected $code = 0;"\` |
|        - |  1126 | `    "protected $file;"\` |
|        - |  1127 | `    "protected $line;"\` |
|        - |  1128 | `    "protected $trace;"\` |
|        - |  1129 | `    "protected $previous;"\` |
|        - |  1130 | `	"public function __construct($message = null, $code = 0, Throwable $previous = null){"\` |
|        - |  1131 | `	"   if( isset($message) ){"\` |
|        - |  1132 | `	"	  $this->message = $message;"\` |
|        - |  1133 | `	"   }"\` |
|        - |  1134 | `	"   $this->code = $code;"\` |
|        - |  1135 | `	"   $this->file = __FILE__;"\` |
|        - |  1136 | `	"   $this->line = __LINE__;"\` |
|        - |  1137 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1138 | `	"   if( isset($previous) ){"\` |
|        - |  1139 | `	"     $this->previous = $previous;"\` |
|        - |  1140 | `	"   }"\` |
|        - |  1141 | `	"}"\` |
|        - |  1142 | `	"public function getMessage(){"\` |
|        - |  1143 | `	"   return $this->message;"\` |
|        - |  1144 | `	"}"\` |
|        - |  1145 | `	" public function getCode(){"\` |
|        - |  1146 | `	"  return $this->code;"\` |
|        - |  1147 | `	"}"\` |
|        - |  1148 | `	"public function getFile(){"\` |
|        - |  1149 | `	"  return $this->file;"\` |
|        - |  1150 | `	"}"\` |
|        - |  1151 | `	"public function getLine(){"\` |
|        - |  1152 | `	"  return $this->line;"\` |
|        - |  1153 | `	"}"\` |
|        - |  1154 | `	"public function getTrace(){"\` |
|        - |  1155 | `	"   return $this->trace;"\` |
|        - |  1156 | `	"}"\` |
|        - |  1157 | `	"public function getTraceAsString(){"\` |
|        - |  1158 | `	"  return debug_string_backtrace();"\` |
|        - |  1159 | `	"}"\` |
|        - |  1160 | `	"public function getPrevious(){"\` |
|        - |  1161 | `	"    return $this->previous;"\` |
|        - |  1162 | `	"}"\` |
|        - |  1163 | `	"public function __toString(){"\` |
|        - |  1164 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |  1165 | `    "}"\` |
|        - |  1166 | `	"}"\` |
|        - |  1167 | `	"class Error implements Throwable { "\` |
|        - |  1168 | `    "protected $message = '';"\` |
|        - |  1169 | `    "protected $code = 0;"\` |
|        - |  1170 | `    "protected $file;"\` |
|        - |  1171 | `    "protected $line;"\` |
|        - |  1172 | `    "protected $trace;"\` |
|        - |  1173 | `    "protected $previous;"\` |
|        - |  1174 | `	"public function __construct($message = null, $code = 0, Throwable $previous = null){"\` |
|        - |  1175 | `	"   if( isset($message) ){"\` |
|        - |  1176 | `	"	  $this->message = $message;"\` |
|        - |  1177 | `	"   }"\` |
|        - |  1178 | `	"   $this->code = $code;"\` |
|        - |  1179 | `	"   $this->file = __FILE__;"\` |
|        - |  1180 | `	"   $this->line = __LINE__;"\` |
|        - |  1181 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1182 | `	"   if( isset($previous) ){"\` |
|        - |  1183 | `	"     $this->previous = $previous;"\` |
|        - |  1184 | `	"   }"\` |
|        - |  1185 | `	"}"\` |
|        - |  1186 | `	"public function getMessage(){"\` |
|        - |  1187 | `	"   return $this->message;"\` |
|        - |  1188 | `	"}"\` |
|        - |  1189 | `	"public function getCode(){"\` |
|        - |  1190 | `	"  return $this->code;"\` |
|        - |  1191 | `	"}"\` |
|        - |  1192 | `	"public function getFile(){"\` |
|        - |  1193 | `	"  return $this->file;"\` |
|        - |  1194 | `	"}"\` |
|        - |  1195 | `	"public function getLine(){"\` |
|        - |  1196 | `	"  return $this->line;"\` |
|        - |  1197 | `	"}"\` |
|        - |  1198 | `	"public function getTrace(){"\` |
|        - |  1199 | `	"   return $this->trace;"\` |
|        - |  1200 | `	"}"\` |
|        - |  1201 | `	"public function getTraceAsString(){"\` |
|        - |  1202 | `	"  return debug_string_backtrace();"\` |
|        - |  1203 | `	"}"\` |
|        - |  1204 | `	"public function getPrevious(){"\` |
|        - |  1205 | `	"    return $this->previous;"\` |
|        - |  1206 | `	"}"\` |
|        - |  1207 | `	"public function __toString(){"\` |
|        - |  1208 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |  1209 | `	"}"\` |
|        - |  1210 | `	"}"\` |
|        - |  1211 | `	"class TypeError extends Error { }"\` |
|        - |  1212 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |  1213 | `	"class ValueError extends Error { }"\` |
|        - |  1214 | `	"class FiberError extends Error { }"\` |
|        - |  1215 | `	"class AssertionError extends Error { }"\` |
|        - |  1216 | `	"class ArithmeticError extends Error { }"\` |
|        - |  1217 | `	"class DivisionByZeroError extends ArithmeticError { }"\` |
|        - |  1218 | `	"class ErrorException extends Exception { "\` |
|        - |  1219 | `	"protected $severity;"\` |
|        - |  1220 | `	"public function __construct(string $message = null,"\` |
|        - |  1221 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Throwable $previous = null){"\` |
|        - |  1222 | `	"   if( isset($message) ){"\` |
|        - |  1223 | `	"	  $this->message = $message;"\` |
|        - |  1224 | `	"   }"\` |
|        - |  1225 | `	"   $this->severity = $severity;"\` |
|        - |  1226 | `	"   $this->code = $code;"\` |
|        - |  1227 | `	"   $this->file = $filename;"\` |
|        - |  1228 | `	"   $this->line = $lineno;"\` |
|        - |  1229 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1230 | `	"   if( isset($previous) ){"\` |
|        - |  1231 | `	"     $this->previous = $previous;"\` |
|        - |  1232 | `	"   }"\` |
|        - |  1233 | `	"}"\` |
|        - |  1234 | `	"public function getSeverity(){"\` |
|        - |  1235 | `	"   return $this->severity;"\` |
|        - |  1236 | `    "}"\` |
|        - |  1237 | `	"}"\` |
|        - |  1238 | `	"/* SPL exceptions: thin tree, inherit Exception's ctor+getters. Roots first. */"\` |
|        - |  1239 | `	"class LogicException extends Exception { }"\` |
|        - |  1240 | `	"class RuntimeException extends Exception { }"\` |
|        - |  1241 | `	"class BadFunctionCallException extends LogicException { }"\` |
|        - |  1242 | `	"class BadMethodCallException extends BadFunctionCallException { }"\` |
|        - |  1243 | `	"class DomainException extends LogicException { }"\` |
|        - |  1244 | `	"class InvalidArgumentException extends LogicException { }"\` |
|        - |  1245 | `	"class LengthException extends LogicException { }"\` |
|        - |  1246 | `	"class OutOfRangeException extends LogicException { }"\` |
|        - |  1247 | `	"class OutOfBoundsException extends RuntimeException { }"\` |
|        - |  1248 | `	"class OverflowException extends RuntimeException { }"\` |
|        - |  1249 | `	"class RangeException extends RuntimeException { }"\` |
|        - |  1250 | `	"class UnderflowException extends RuntimeException { }"\` |
|        - |  1251 | `	"class UnexpectedValueException extends RuntimeException { }"\` |
|        - |  1252 | `	"interface Iterator extends Traversable {"\` |
|        - |  1253 | `	"public function current();"\` |
|        - |  1254 | `	"public function key();"\` |
|        - |  1255 | `	"public function next();"\` |
|        - |  1256 | `	"public function rewind();"\` |
|        - |  1257 | `	"public function valid();"\` |
|        - |  1258 | `	"}"\` |
|        - |  1259 | `	"interface IteratorAggregate extends Traversable {"\` |
|        - |  1260 | `	"public function getIterator();"\` |
|        - |  1261 | `	"}"\` |
|        - |  1262 | `	"interface Serializable {"\` |
|        - |  1263 | `	"public function serialize();"\` |
|        - |  1264 | `	"public function unserialize(string $serialized);"\` |
|        - |  1265 | `	"}"\` |
|        - |  1266 | `	"/* Directory releated IO */"\` |
|        - |  1267 | `	"class Directory {"\` |
|        - |  1268 | `	"public $handle = null;"\` |
|        - |  1269 | `	"public $path  = null;"\` |
|        - |  1270 | `	"public function __construct(string $path)"\` |
|        - |  1271 | `	"{"\` |
|        - |  1272 | `	"   $this->handle = opendir($path);"\` |
|        - |  1273 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |  1274 | `	"      $this->path = $path;"\` |
|        - |  1275 | `	"   }"\` |
|        - |  1276 | `	"}"\` |
|        - |  1277 | `	"public function __destruct()"\` |
|        - |  1278 | `	"{"\` |
|        - |  1279 | `	"  if( $this->handle != null ){"\` |
|        - |  1280 | `	"       closedir($this->handle);"\` |
|        - |  1281 | `	"  }"\` |
|        - |  1282 | `	"}"\` |
|        - |  1283 | `	"public function read()"\` |
|        - |  1284 | `	"{"\` |
|        - |  1285 | `	"    return readdir($this->handle);"\` |
|        - |  1286 | `	"}"\` |
|        - |  1287 | `	"public function rewind()"\` |
|        - |  1288 | `	"{"\` |
|        - |  1289 | `	"    rewinddir($this->handle);"\` |
|        - |  1290 | `	"}"\` |
|        - |  1291 | `	"public function close()"\` |
|        - |  1292 | `	"{"\` |
|        - |  1293 | `	"    closedir($this->handle);"\` |
|        - |  1294 | `	"    $this->handle = null;"\` |
|        - |  1295 | `	"}"\` |
|        - |  1296 | `	"}"\` |
|        - |  1297 | `	"class Fiber {"\` |
|        - |  1298 | `	"  private $__ctx;"\` |
|        - |  1299 | `	"  private $__callable;"\` |
|        - |  1300 | `	"  public function __construct($callable){ __fiber_construct($this,$callable); }"\` |
|        - |  1301 | `	"  public function start(){ return __fiber_start($this, func_get_args()); }"\` |
|        - |  1302 | `	"  public function resume($value = null){ return __fiber_resume($this,$value); }"\` |
|        - |  1303 | `	"  public function getReturn(){ return __fiber_getReturn($this); }"\` |
|        - |  1304 | `	"  public function isStarted(){ return __fiber_isStarted($this); }"\` |
|        - |  1305 | `	"  public function isRunning(){ return __fiber_isRunning($this); }"\` |
|        - |  1306 | `	"  public function isSuspended(){ return __fiber_isSuspended($this); }"\` |
|        - |  1307 | `	"  public function isTerminated(){ return __fiber_isTerminated($this); }"\` |
|        - |  1308 | `	"  public static function suspend($value = null){ return __fiber_suspend($value); }"\` |
|        - |  1309 | `	"  public function __destruct(){ __fiber_destruct($this); }"\` |
|        - |  1310 | `	"}"\` |
|        - |  1311 | `	"class Generator implements Iterator {"\` |
|        - |  1312 | `	"  private $__ctx;"\` |
|        - |  1313 | `	"  public function current(){ return __gen_current($this); }"\` |
|        - |  1314 | `	"  public function key(){ return __gen_key($this); }"\` |
|        - |  1315 | `	"  public function next(){ return __gen_next($this); }"\` |
|        - |  1316 | `	"  public function rewind(){ return __gen_rewind($this); }"\` |
|        - |  1317 | `	"  public function valid(){ return __gen_valid($this); }"\` |
|        - |  1318 | `	"  public function send($value = null){ return __gen_send($this,$value); }"\` |
|        - |  1319 | `	"  public function throw($exception){ return __gen_throw($this,$exception); }"\` |
|        - |  1320 | `	"  public function getReturn(){ return __gen_getReturn($this); }"\` |
|        - |  1321 | `	"  public function __destruct(){ __gen_destruct($this); }"\` |
|        - |  1322 | `	"}"\` |
|        - |  1323 | `	"class stdClass{"\` |
|        - |  1324 | `	"  public $value;"\` |
|        - |  1325 | `	" /* Magic methods */"\` |
|        - |  1326 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1327 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1328 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1329 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1330 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1331 | `	"}"\` |
|        - |  1332 | `	"function dir(string $path){"\` |
|        - |  1333 | `	"   return new Directory($path);"\` |
|        - |  1334 | `	"}"\` |
|        - |  1335 | `	"function Dir(string $path){"\` |
|        - |  1336 | `	"   return new Directory($path);"\` |
|        - |  1337 | `	"}"\` |
|        - |  1338 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1339 | `    "{"\` |
|        - |  1340 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1341 | `	"  $aDir = array();"\` |
|        - |  1342 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1343 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1344 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1345 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1346 | `	"   }"\` |
|        - |  1347 | `	"  closedir($pHandle);"\` |
|        - |  1348 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1349 | `	"      rsort($aDir);"\` |
|        - |  1350 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1351 | `	"      sort($aDir);"\` |
|        - |  1352 | `	"  }"\` |
|        - |  1353 | `	"  return $aDir;"\` |
|        - |  1354 | `	"}"\` |
|        - |  1355 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1356 | `	"/* Open the target directory */"\` |
|        - |  1357 | `	"$zDir = dirname($pattern);"\` |
|        - |  1358 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1359 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1360 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1361 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1362 | `	"	return FALSE;"\` |
|        - |  1363 | `	"}"\` |
|        - |  1364 | `	"$pattern = basename($pattern);"\` |
|        - |  1365 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1366 | `	"/* Loop throw available entries */"\` |
|        - |  1367 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1368 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1369 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1370 | `	"	if( $rc ){"\` |
|        - |  1371 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1372 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1373 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1374 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1375 | `	"		  }"\` |
|        - |  1376 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1377 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1378 | `	"		 continue;"\` |
|        - |  1379 | `	"	   }"\` |
|        - |  1380 | `	"	   /* Add the entry */"\` |
|        - |  1381 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1382 | `	"	}"\` |
|        - |  1383 | `	" }"\` |
|        - |  1384 | `	"/* Close the handle */"\` |
|        - |  1385 | `	"closedir($pHandle);"\` |
|        - |  1386 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1387 | `	"  /* Sort the array */"\` |
|        - |  1388 | `	"  sort($pArray);"\` |
|        - |  1389 | `	"}"\` |
|        - |  1390 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1391 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1392 | `	"  $pArray[] = $pattern;"\` |
|        - |  1393 | `	"}"\` |
|        - |  1394 | `	"/* Return the created array */"\` |
|        - |  1395 | `	"return $pArray;"\` |
|        - |  1396 | `   "}"\` |
|        - |  1397 | `   "/* Creates a temporary file */"\` |
|        - |  1398 | `   "function tmpfile(){"\` |
|        - |  1399 | `   "  /* Extract the temp directory */"\` |
|        - |  1400 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1401 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1402 | `   "    /* Use the current dir */"\` |
|        - |  1403 | `   "    $zTempDir = '.';"\` |
|        - |  1404 | `   "  }"\` |
|        - |  1405 | `   "  /* Create the file */"\` |
|        - |  1406 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1407 | `   "  return $pHandle;"\` |
|        - |  1408 | `   "}"\` |
|        - |  1409 | `   "/* Creates a temporary filename */"\` |
|        - |  1410 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1411 | `   "{"\` |
|        - |  1412 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1413 | `   "}"\` |
|        - |  1414 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1415 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1416 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1417 | `   "/* Copy arguments */"\` |
|        - |  1418 | `   "$nArgs = func_num_args();"\` |
|        - |  1419 | `   "$pNew = array();"\` |
|        - |  1420 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1421 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1422 | `    "}"\` |
|        - |  1423 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1424 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1425 | `	"/* Erase */"\` |
|        - |  1426 | `	"array_erase($pArray);"\` |
|        - |  1427 | `	"/* Unshift */"\` |
|        - |  1428 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1429 | `	"return sizeof($pArray);"\` |
|        - |  1430 | `    "}"\` |
|        - |  1431 | `	"function array_merge_recursive(){"\` |
|        - |  1432 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1433 | `    "$arrays = func_get_args();"\` |
|        - |  1434 | `    "$narrays = count($arrays);"\` |
|        - |  1435 | `    "$ret = array();"\` |
|        - |  1436 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1437 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1438 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1439 | `	 " }"\` |
|        - |  1440 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1441 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1442 | `     "  if( $keyIsInt ) {"\` |
|        - |  1443 | `     "   $ret[] = $value;"\` |
|        - |  1444 | `     "  } else {"\` |
|        - |  1445 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1446 | `     "    $cur = $ret[$key];"\` |
|        - |  1447 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1448 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1449 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1450 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1451 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1452 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1453 | `     "    } else {"\` |
|        - |  1454 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1455 | `     "    }"\` |
|        - |  1456 | `     "   } else {"\` |
|        - |  1457 | `     "    $ret[$key] = $value;"\` |
|        - |  1458 | `     "   }"\` |
|        - |  1459 | `     "  }"\` |
|        - |  1460 | `     " }"\` |
|        - |  1461 | `	 " }"\` |
|        - |  1462 | `	 " return $ret;"\` |
|        - |  1463 | `    "}"\` |
|        - |  1464 | `	"function max(){"\` |
|        - |  1465 | `    "  $pArgs = func_get_args();"\` |
|        - |  1466 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1467 | `	"  return null;"\` |
|        - |  1468 | `    " }"\` |
|        - |  1469 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1470 | `    " $pArg = $pArgs[0];"\` |
|        - |  1471 | `	" if( !is_array($pArg) ){"\` |
|        - |  1472 | `	"   return $pArg; "\` |
|        - |  1473 | `	" }"\` |
|        - |  1474 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1475 | `	"   return null;"\` |
|        - |  1476 | `	" }"\` |
|        - |  1477 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1478 | `	" reset($pArg);"\` |
|        - |  1479 | `	" $max = current($pArg);"\` |
|        - |  1480 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1481 | `	"   if( $val > $max ){"\` |
|        - |  1482 | `	"     $max = $val;"\` |
|        - |  1483 | `    " }"\` |
|        - |  1484 | `	" }"\` |
|        - |  1485 | `	" return $max;"\` |
|        - |  1486 | `    " }"\` |
|        - |  1487 | `    " $max = $pArgs[0];"\` |
|        - |  1488 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1489 | `    " $val = $pArgs[$i];"\` |
|        - |  1490 | `	"if( $val > $max ){"\` |
|        - |  1491 | `	" $max = $val;"\` |
|        - |  1492 | `	"}"\` |
|        - |  1493 | `    " }"\` |
|        - |  1494 | `	" return $max;"\` |
|        - |  1495 | `    "}"\` |
|        - |  1496 | `	"function min(){"\` |
|        - |  1497 | `    "  $pArgs = func_get_args();"\` |
|        - |  1498 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1499 | `	"  return null;"\` |
|        - |  1500 | `    " }"\` |
|        - |  1501 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1502 | `    " $pArg = $pArgs[0];"\` |
|        - |  1503 | `	" if( !is_array($pArg) ){"\` |
|        - |  1504 | `	"   return $pArg; "\` |
|        - |  1505 | `	" }"\` |
|        - |  1506 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1507 | `	"   return null;"\` |
|        - |  1508 | `	" }"\` |
|        - |  1509 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1510 | `	" reset($pArg);"\` |
|        - |  1511 | `	" $min = current($pArg);"\` |
|        - |  1512 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1513 | `	"   if( $val < $min ){"\` |
|        - |  1514 | `	"     $min = $val;"\` |
|        - |  1515 | `    " }"\` |
|        - |  1516 | `	" }"\` |
|        - |  1517 | `	" return $min;"\` |
|        - |  1518 | `    " }"\` |
|        - |  1519 | `    " $min = $pArgs[0];"\` |
|        - |  1520 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1521 | `    " $val = $pArgs[$i];"\` |
|        - |  1522 | `	"if( $val < $min ){"\` |
|        - |  1523 | `	" $min = $val;"\` |
|        - |  1524 | `	" }"\` |
|        - |  1525 | `    " }"\` |
|        - |  1526 | `	" return $min;"\` |
|        - |  1527 | `	"}"\` |
|        - |  1528 | `	"function fileowner(string $file){"\` |
|        - |  1529 | `    " $a = stat($file);"\` |
|        - |  1530 | `	" if( !is_array($a) ){"\` |
|        - |  1531 | `	"	return false;"\` |
|        - |  1532 | `	" }"\` |
|        - |  1533 | `	" return $a['uid'];"\` |
|        - |  1534 | `    "}"\` |
|        - |  1535 | `    "function filegroup(string $file){"\` |
|        - |  1536 | `	" $a = stat($file);"\` |
|        - |  1537 | `	" if( !is_array($a) ){"\` |
|        - |  1538 | `	"	return false;"\` |
|        - |  1539 | `	" }"\` |
|        - |  1540 | `	" return $a['gid'];"\` |
|        - |  1541 | `    "}"\` |
|        - |  1542 | `	 "function fileinode(string $file){"\` |
|        - |  1543 | `	" $a = stat($file);"\` |
|        - |  1544 | `	" if( !is_array($a) ){"\` |
|        - |  1545 | `	"	return false;"\` |
|        - |  1546 | `	" }"\` |
|        - |  1547 | `	" return $a['ino'];"\` |
|        - |  1548 | `    "}"` |
|        - |  1549 |  |
|        - |  1550 | `/*` |
|        - |  1551 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1552 | ` * start compiling the target PHP program.` |
|        - |  1553 | ` */` |
|     3148 |  1554 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1555 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1556 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1557 | `	 )` |
|        2 |  1558 |  |
|        - |  1559 | `	SyString sBuiltin;` |
|        - |  1560 | `	ph7_value *pObj;` |
|        - |  1561 | `	sxi32 rc;` |
|        - |  1562 | `	/* Zero the structure */` |
|     3150 |  1563 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1564 | `	/* Initialize VM fields */` |
|     3150 |  1565 | `	pVm->pEngine = &(*pEngine);` |
|     3150 |  1566 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1567 | `	/* Instructions containers */` |
|     3150 |  1568 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     3150 |  1569 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     3150 |  1570 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1571 | `	/* Object containers */` |
|     3150 |  1572 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     3150 |  1573 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1574 | `	/* Virtual machine internal containers */` |
|     3150 |  1575 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     3150 |  1576 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     3150 |  1577 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     3150 |  1578 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     3150 |  1579 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     3150 |  1580 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     3150 |  1581 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     3150 |  1582 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     3150 |  1583 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     3150 |  1584 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|     3150 |  1585 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     3150 |  1586 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     3150 |  1587 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     3150 |  1588 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     3150 |  1589 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     3150 |  1590 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     3150 |  1591 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     3150 |  1592 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|     3150 |  1593 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|     3150 |  1594 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|     3150 |  1595 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     3150 |  1596 | `	pVm->pPendingException = 0;` |
|        - |  1597 | `	/* Configuration containers */` |
|     3150 |  1598 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     3150 |  1599 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     3150 |  1600 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     3150 |  1601 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     3150 |  1602 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     3150 |  1603 | `	pVm->iResponseStatus = 200;` |
|     3150 |  1604 | `	pVm->bHeadersSent = 0;` |
|     3150 |  1605 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1606 | `	/* Error callbacks containers */` |
|     3150 |  1607 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     3150 |  1608 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     3150 |  1609 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     3150 |  1610 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     3150 |  1611 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1612 | `	/* Set a default recursion limit */` |
|        - |  1613 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     3150 |  1614 | `	pVm->nMaxDepth = 32;` |
|        - |  1615 | `#else` |
|        - |  1616 | `	pVm->nMaxDepth = 16;` |
|        - |  1617 | `#endif` |
|        - |  1618 | `	/* Default assertion flags */` |
|     3150 |  1619 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1620 | `	/* JSON return status */` |
|     3150 |  1621 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1622 | `	/* PRNG context */` |
|     3150 |  1623 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1624 | `	/* Install the null constant */` |
|     3150 |  1625 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3150 |  1626 | `	if( pObj == 0 ){` |
|      ! 0 |  1627 | `		rc = SXERR_MEM;` |
|      ! 0 |  1628 | `		goto Err;` |
|        - |  1629 | `	}` |
|     3150 |  1630 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1631 | `	/* Install the boolean TRUE constant */` |
|     3150 |  1632 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3150 |  1633 | `	if( pObj == 0 ){` |
|      ! 0 |  1634 | `		rc = SXERR_MEM;` |
|      ! 0 |  1635 | `		goto Err;` |
|        - |  1636 | `	}` |
|     3150 |  1637 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1638 | `	/* Install the boolean FALSE constant */` |
|     3150 |  1639 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3150 |  1640 | `	if( pObj == 0 ){` |
|      ! 0 |  1641 | `		rc = SXERR_MEM;` |
|      ! 0 |  1642 | `		goto Err;` |
|        - |  1643 | `	}` |
|     3150 |  1644 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1645 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1646 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1647 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     3150 |  1648 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     3150 |  1649 | `	if( pObj == 0 ){` |
|      ! 0 |  1650 | `		rc = SXERR_MEM;` |
|      ! 0 |  1651 | `		goto Err;` |
|        - |  1652 | `	}` |
|     3150 |  1653 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1654 | `	/* Create the global frame */` |
|     3150 |  1655 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     3150 |  1656 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1657 | `		goto Err;` |
|        - |  1658 | `	}` |
|        - |  1659 | `	/* Initialize the code generator */` |
|     3150 |  1660 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     3150 |  1661 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1662 | `		goto Err;` |
|        - |  1663 | `	}` |
|        - |  1664 | `	/* VM correctly initialized,set the magic number */` |
|     3150 |  1665 | `	pVm->nMagic = PH7_VM_INIT;` |
|     3150 |  1666 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1667 | `	/* Compile the built-in library */` |
|     3150 |  1668 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1669 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     3150 |  1670 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - |  1671 | `	/* Cache built-in interface pointers used on hot dispatch paths */` |
|     3150 |  1672 | `	pVm->pArrayAccessClass = PH7_VmExtractClass(pVm,"ArrayAccess",sizeof("ArrayAccess")-1,0,0);` |
|     3150 |  1673 | `	pVm->pCountableClass   = PH7_VmExtractClass(pVm,"Countable",sizeof("Countable")-1,0,0);` |
|     3150 |  1674 | `	pVm->pStringableClass  = PH7_VmExtractClass(pVm,"Stringable",sizeof("Stringable")-1,0,0);` |
|     3150 |  1675 | `	pVm->pJsonSerializableClass = PH7_VmExtractClass(pVm,"JsonSerializable",sizeof("JsonSerializable")-1,0,0);` |
|     3150 |  1676 | `	pVm->pTraversableClass = PH7_VmExtractClass(pVm,"Traversable",sizeof("Traversable")-1,0,0);` |
|        - |  1677 | `	/* Initialize null-coalesce-assign scratch slot */` |
|     3150 |  1678 | `	pVm->pCoalesceObj = 0;` |
|     3150 |  1679 | `	pVm->bCoalesceArmed = 0;` |
|     3150 |  1680 | `	PH7_MemObjInit(pVm,&pVm->sCoalesceKey);` |
|        - |  1681 | `	/* Register Fiber internal C functions */` |
|     3150 |  1682 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     3150 |  1683 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     3150 |  1684 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     3150 |  1685 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     3150 |  1686 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     3150 |  1687 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     3150 |  1688 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     3150 |  1689 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     3150 |  1690 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     3150 |  1691 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - |  1692 | `	/* Cache the Generator class pointer and register generator functions */` |
|     3150 |  1693 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     3150 |  1694 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     3150 |  1695 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     3150 |  1696 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     3150 |  1697 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     3150 |  1698 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     3150 |  1699 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     3150 |  1700 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     3150 |  1701 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     3150 |  1702 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - |  1703 | `	/* Reset the code generator */` |
|     3150 |  1704 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     3150 |  1705 | `	return SXRET_OK;` |
|      ! 0 |  1706 | `Err:` |
|      ! 0 |  1707 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1708 | `	return rc;` |
|     1576 |  1709 |  |
|        - |  1710 | `/*` |
|        - |  1711 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1712 | ` * routine which store the output in an internal blob.` |
|        - |  1713 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1714 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1715 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1716 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1717 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1718 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1719 | ` * to finish executing and extracting the output.` |
|        - |  1720 | ` */` |
|       56 |  1721 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1722 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1723 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1724 | `	void *pUserData     /* User private data */` |
|        - |  1725 | `	)` |
|      ! 0 |  1726 |  |
|        - |  1727 | `	 sxi32 rc;` |
|        - |  1728 | `	 /* Store the output in an internal BLOB */` |
|       56 |  1729 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       56 |  1730 | `	 return rc;` |
|      ! 0 |  1731 |  |
|        - |  1732 | `/*` |
|        - |  1733 | ` * Track output length and mark headers as sent when output reaches` |
|        - |  1734 | ` * a real external consumer (not the internal blob or OB buffer).` |
|        - |  1735 | ` */` |
|    20790 |  1736 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        2 |  1737 |  |
|    20792 |  1738 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    20792 |  1739 | `	if( xCons != VmObConsumer ){` |
|     8264 |  1740 | `		pVm->nOutputLen += nLen;` |
|     8264 |  1741 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|     1028 |  1742 | `			pVm->bHeadersSent = 1;` |
|      513 |  1743 | `		}` |
|     4131 |  1744 | `	}` |
|    20792 |  1745 |  |
|        - |  1746 | `#define VM_STACK_GUARD 16` |
|        - |  1747 | `/*` |
|        - |  1748 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1749 | ` * our compiled PHP program.` |
|        - |  1750 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1751 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1752 | ` */` |
|    46246 |  1753 | `static ph7_value * VmNewOperandStack(` |
|        - |  1754 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1755 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1756 | `	)` |
|        2 |  1757 |  |
|        - |  1758 | `	ph7_value *pStack;` |
|        - |  1759 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1760 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1761 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1762 | `  ** on the maximum stack depth required.` |
|        - |  1763 | `  **` |
|        - |  1764 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1765 | `  */` |
|    46248 |  1766 | `	nInstr += VM_STACK_GUARD;` |
|    46248 |  1767 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    46248 |  1768 | `	if( pStack == 0 ){` |
|      ! 0 |  1769 | `		return 0;` |
|        - |  1770 | `	}` |
|        - |  1771 | `	/* Initialize the operand stack */` |
|  3069366 |  1772 | `	while( nInstr > 0 ){` |
|  3023120 |  1773 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  3023120 |  1774 | `		--nInstr;` |
|        2 |  1775 | `	}` |
|        - |  1776 | `	/* Ready for bytecode execution */` |
|    46248 |  1777 | `	return pStack;` |
|    23125 |  1778 |  |
|        - |  1779 | `/* Forward declaration */` |
|        - |  1780 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1781 | `/*` |
|        - |  1782 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1783 | ` * This routine gets called by the PH7 engine after` |
|        - |  1784 | ` * successful compilation of the target PHP program.` |
|        - |  1785 | ` */` |
|     2834 |  1786 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1787 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1788 | `	)` |
|        2 |  1789 |  |
|        - |  1790 | `	SyHashEntry *pEntry;` |
|        - |  1791 | `	sxi32 rc;` |
|     2836 |  1792 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1793 | `		/* Initialize your VM first */` |
|      ! 0 |  1794 | `		return SXERR_CORRUPT;` |
|        - |  1795 | `	}` |
|        - |  1796 | `	/* Mark the VM ready for byte-code execution */` |
|     2836 |  1797 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1798 | `	/* Release the code generator now we have compiled our program */` |
|     2836 |  1799 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1800 | `	/* Emit the DONE instruction */` |
|     2836 |  1801 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2836 |  1802 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1803 | `		return SXERR_MEM;` |
|        - |  1804 | `	}` |
|        - |  1805 | `	/* Script return value */` |
|     2836 |  1806 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1807 | `	/* Allocate a new operand stack */` |
|     2836 |  1808 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2836 |  1809 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1810 | `		return SXERR_MEM;` |
|        - |  1811 | `	}` |
|        - |  1812 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1813 | `	 * private data. */` |
|     2836 |  1814 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2836 |  1815 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1816 | `	/* Allocate the reference table */` |
|     2836 |  1817 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2836 |  1818 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2836 |  1819 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1820 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1821 | `		return SXERR_MEM;` |
|        - |  1822 | `	}` |
|        - |  1823 | `	/* Zero the reference table */` |
|     2836 |  1824 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1825 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2836 |  1826 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2836 |  1827 | `	if( rc != SXRET_OK ){` |
|        - |  1828 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1829 | `		return rc;` |
|        - |  1830 | `	}` |
|        - |  1831 | `	/* Snapshot the runtime object-pool watermark. Everything reserved from this` |
|        - |  1832 | `	 * index up (the $GLOBALS array, the superglobals, class static/const slots and` |
|        - |  1833 | `	 * every object/variable created during execution) is per-exec state that` |
|        - |  1834 | `	 * ph7_vm_reset() releases and truncates away before rebuilding; everything` |
|        - |  1835 | `	 * below it is compile-time/init state that survives a reset. */` |
|     2836 |  1836 | `	pVm->nSuperBaseline = SySetUsed(&pVm->aMemObj);` |
|        - |  1837 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2836 |  1838 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2836 |  1839 | `	if( rc != SXRET_OK ){` |
|        - |  1840 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1841 | `		return rc;` |
|        - |  1842 | `	}` |
|        - |  1843 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2836 |  1844 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1845 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2836 |  1846 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1847 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     2836 |  1848 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - |  1849 | `#ifdef PH7_ENABLE_PCRE` |
|        - |  1850 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|     2836 |  1851 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|     2836 |  1852 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - |  1853 | `#endif` |
|        - |  1854 | `	/* Initialize and install static and constants class attributes.` |
|        - |  1855 | `	 * NOTE: the per-exec object graph created from nSuperBaseline onward (the` |
|        - |  1856 | `	 * global frame via VmEnterFrame above, the superglobals via CreateSuper, and` |
|        - |  1857 | `	 * these class static/const slots) is rebuilt on every ph7_vm_reset() — keep` |
|        - |  1858 | `	 * that function in sync when changing what is reserved here. */` |
|     2836 |  1859 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|   110874 |  1860 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   108040 |  1861 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|   108040 |  1862 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1863 | `			return rc;` |
|        - |  1864 | `		}` |
|        2 |  1865 | `	}` |
|        - |  1866 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2836 |  1867 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1868 | `	/* VM is ready for bytecode execution */` |
|     2836 |  1869 | `	return SXRET_OK;` |
|     1419 |  1870 |  |
|        - |  1871 | `/*` |
|        - |  1872 | ` * Tear down the whole reference table. Unlinks every referenced object,` |
|        - |  1873 | ` * deleting the hash entries (frame variables) and array nodes it points at.` |
|        - |  1874 | ` * Called by ph7_vm_reset() while the frames and the object pool are still` |
|        - |  1875 | ` * intact: doing it first means a later release of a by-ref array does not leave` |
|        - |  1876 | ` * a dangling node pointer in some other object's reference record.` |
|        - |  1877 | ` */` |
|        6 |  1878 | `static void VmResetRefTable(ph7_vm *pVm)` |
|      ! 0 |  1879 |  |
|        - |  1880 | `	/* VmRefObjUnlink splices each node out of its apRefObj bucket and decrements` |
|        - |  1881 | `	 * nRefUsed, so draining the list leaves the bucket array empty and nRefUsed` |
|        - |  1882 | `	 * at 0 — no extra clearing needed. The bucket array and nRefSize survive. */` |
|      204 |  1883 | `	while( pVm->pRefList ){` |
|      198 |  1884 | `		VmRefObjUnlink(&(*pVm),pVm->pRefList);` |
|      ! 0 |  1885 | `	}` |
|        6 |  1886 |  |
|        - |  1887 | `/*` |
|        - |  1888 | ` * Release a standing per-exec ph7_value slot and re-initialise it to NULL.` |
|        - |  1889 | ` * The reset idiom for the VM's long-lived value fields (return value, the` |
|        - |  1890 | ` * error/exception handler callbacks, the assertion callback, the coalesce key).` |
|        - |  1891 | ` */` |
|       42 |  1892 | `static void VmReinitMemObj(ph7_vm *pVm,ph7_value *pObj)` |
|      ! 0 |  1893 |  |
|       42 |  1894 | `	PH7_MemObjRelease(pObj);` |
|       42 |  1895 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|       42 |  1896 |  |
|        - |  1897 | `/*` |
|        - |  1898 | ` * Reset a function's static-variable sentinels to SXU32_HIGH so the next call` |
|        - |  1899 | ` * re-reserves their slots and re-runs the initializers (PHP's per-request reset` |
|        - |  1900 | ` * of statics).` |
|        - |  1901 | ` */` |
|      380 |  1902 | `static void VmResetFuncStatics(ph7_vm_func *pFunc)` |
|      ! 0 |  1903 |  |
|      380 |  1904 | `	ph7_vm_func_static_var *aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|        - |  1905 | `	sxu32 k;` |
|      384 |  1906 | `	for( k = 0 ; k < SySetUsed(&pFunc->aStatic) ; ++k ){` |
|        4 |  1907 | `		aStatic[k].nIdx = SXU32_HIGH;` |
|        2 |  1908 | `	}` |
|      380 |  1909 |  |
|        - |  1910 | `/*` |
|        - |  1911 | ` * Reset per-execution function-table state in a single pass over hFunction:` |
|        - |  1912 | ` *  - run-time closures (VM_FUNC_CLOSURE) are freed. Closure templates are never` |
|        - |  1913 | ` *    installed in hFunction (see compile.c) and closure names are unique, so any` |
|        - |  1914 | ` *    such entry is a standalone instance created by OP_LOAD_CLOSURE; it owns its` |
|        - |  1915 | ` *    captured environment values, its name buffer and its structure (the` |
|        - |  1916 | ` *    bytecode/args/static sets are shared with the template and must NOT be` |
|        - |  1917 | ` *    freed). Its template-shared static sentinels are reset too.` |
|        - |  1918 | ` *  - every other function (and its pNextName overloads, including class methods)` |
|        - |  1919 | ` *    has its static sentinels reset.` |
|        - |  1920 | ` * The head flag of each entry fully classifies it, so one walk handles both.` |
|        - |  1921 | ` * Deleting the just-returned entry mid-walk is safe: SyHashGetNextEntry advances` |
|        - |  1922 | ` * the cursor past it before returning and the delete never touches the cursor.` |
|        - |  1923 | ` */` |
|        6 |  1924 | `static void VmResetFunctionState(ph7_vm *pVm)` |
|      ! 0 |  1925 |  |
|        - |  1926 | `	SyHashEntry *pEntry;` |
|        6 |  1927 | `	SyHashResetLoopCursor(&pVm->hFunction);` |
|      386 |  1928 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hFunction)) != 0 ){` |
|      380 |  1929 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      380 |  1930 | `		if( pFunc && (pFunc->iFlags & VM_FUNC_CLOSURE) ){` |
|        - |  1931 | `			/* Standalone run-time closure: reset its (template-shared) statics,` |
|        - |  1932 | `			 * release its captured-by-value environment, then free the entry,` |
|        - |  1933 | `			 * name buffer and structure. */` |
|        4 |  1934 | `			ph7_vm_func_closure_env *aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        4 |  1935 | `			const char *zName = SyStringData(&pFunc->sName);` |
|        - |  1936 | `			sxu32 k;` |
|        4 |  1937 | `			VmResetFuncStatics(pFunc);` |
|        8 |  1938 | `			for( k = 0 ; k < SySetUsed(&pFunc->aClosureEnv) ; ++k ){` |
|        4 |  1939 | `				PH7_MemObjRelease(&aEnv[k].sValue);` |
|        2 |  1940 | `			}` |
|        4 |  1941 | `			SySetRelease(&pFunc->aClosureEnv);` |
|        - |  1942 | `			/* SyHashDeleteEntry2 frees only the entry, not the key buffer. */` |
|        4 |  1943 | `			SyHashDeleteEntry2(pEntry);` |
|        4 |  1944 | `			if( zName ){` |
|        4 |  1945 | `				SyMemBackendFree(&pVm->sAllocator,(void *)zName);` |
|        2 |  1946 | `			}` |
|        4 |  1947 | `			SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|        4 |  1948 | `			continue;` |
|        - |  1949 | `		}` |
|        - |  1950 | `		/* Named function: reset statics for every overload sharing this name. */` |
|      752 |  1951 | `		while( pFunc ){` |
|      376 |  1952 | `			VmResetFuncStatics(pFunc);` |
|      376 |  1953 | `			pFunc = pFunc->pNextName;` |
|      ! 0 |  1954 | `		}` |
|      ! 0 |  1955 | `	}` |
|        6 |  1956 | `	pVm->closure_cnt = 0;` |
|        6 |  1957 |  |
|        - |  1958 | `/*` |
|        - |  1959 | ` * Free the typed-property enforcement slots left in hTypedSlot. Instance slots` |
|        - |  1960 | ` * are already gone (each object's destructor removed its own during the object` |
|        - |  1961 | ` * pool release above), so only the class *static* typed-property slots remain;` |
|        - |  1962 | ` * the class re-mount registers fresh ones.` |
|        - |  1963 | ` */` |
|        6 |  1964 | `static void VmResetTypedSlots(ph7_vm *pVm)` |
|      ! 0 |  1965 |  |
|        - |  1966 | `	SyHashEntry *pEntry;` |
|        - |  1967 | `	/* Common case: no class static typed properties — table already empty. */` |
|        6 |  1968 | `	if( SyHashTotalEntry(&pVm->hTypedSlot) == 0 ){` |
|        2 |  1969 | `		return;` |
|        - |  1970 | `	}` |
|        - |  1971 | `	/* Free each VmClassAttr payload in a plain walk (no entry deletion), then` |
|        - |  1972 | `	 * drop and re-init the table — SyHashRelease frees the entries themselves. */` |
|        4 |  1973 | `	SyHashResetLoopCursor(&pVm->hTypedSlot);` |
|       10 |  1974 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hTypedSlot)) != 0 ){` |
|        4 |  1975 | `		if( pEntry->pUserData ){` |
|        4 |  1976 | `			SyMemBackendPoolFree(&pVm->sAllocator,pEntry->pUserData);` |
|        2 |  1977 | `		}` |
|      ! 0 |  1978 | `	}` |
|        4 |  1979 | `	SyHashRelease(&pVm->hTypedSlot);` |
|        4 |  1980 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|        3 |  1981 |  |
|        - |  1982 | `/*` |
|        - |  1983 | ` * Reset a Virtual Machine to its post-compile (PH7_VmMakeReady) state so the` |
|        - |  1984 | ` * same compiled program can be executed again (compile-once / execute-many).` |
|        - |  1985 | ` *` |
|        - |  1986 | ` * Definitions are preserved (treated like compile-time state): the bytecode,` |
|        - |  1987 | ` * the operand stack, the function/class/interface tables, user-defined constants` |
|        - |  1988 | ` * (a re-run define() overwrites the value in place), included-file markers` |
|        - |  1989 | ` * (so include_once/require_once stay satisfied — definitions and their` |
|        - |  1990 | ` * define()s survive without re-compiling), the literal pool, the cached` |
|        - |  1991 | ` * interface pointers, the output-consumer configuration and the IO streams.` |
|        - |  1992 | ` *` |
|        - |  1993 | ` * Per-execution state is cleared: global variables and the global frame, the` |
|        - |  1994 | ` * superglobals (re-fed afterwards via PH7_VM_CONFIG_HTTP_REQUEST), function and` |
|        - |  1995 | ` * class statics, run-time closures, the output buffers and response headers, the` |
|        - |  1996 | ` * exception/error-handler state, the reference table and every object/array` |
|        - |  1997 | ` * reserved during the run.` |
|        - |  1998 | ` *` |
|        - |  1999 | ` * Object __destruct methods are NOT run during reset (see bInReset) — releasing` |
|        - |  2000 | ` * the pool runs engine-level teardown only, matching PH7's prior behaviour where` |
|        - |  2001 | ` * global-scope destructors never fired.` |
|        - |  2002 | ` */` |
|        6 |  2003 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  2004 |  |
|        - |  2005 | `	sxu32 nWater,n;` |
|        6 |  2006 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  2007 | `		return SXERR_CORRUPT;` |
|        - |  2008 | `	}` |
|        6 |  2009 | `	nWater = pVm->nSuperBaseline;` |
|        - |  2010 | `	/* The $GLOBALS array is normally protected from deletion; drop the guard so` |
|        - |  2011 | `	 * its hashmap is actually released below, then rebuilt by CreateSuper. */` |
|        6 |  2012 | `	pVm->pGlobal = 0;` |
|        - |  2013 | `	/* Suppress user __destruct while we tear down the per-exec object pool: the` |
|        - |  2014 | `	 * reference table is gone and $GLOBALS is nulled, so running arbitrary PHP` |
|        - |  2015 | `	 * here is unsafe (and could realloc aMemObj mid-release). Engine memory is` |
|        - |  2016 | `	 * still reclaimed. Mirrors prior behaviour (global destructors never ran). */` |
|        6 |  2017 | `	pVm->bInReset = 1;` |
|        - |  2018 | `	/* (1) Unlink the whole reference table while frames and objects are intact. */` |
|        6 |  2019 | `	VmResetRefTable(&(*pVm));` |
|        - |  2020 | `	/* (2) Free run-time closures and reset every function/method static sentinel` |
|        - |  2021 | `	 * in a single pass over hFunction. User-defined constants are treated like` |
|        - |  2022 | `	 * function/class registrations and intentionally persist across reuse (a` |
|        - |  2023 | `	 * re-run define() overwrites the value in place). */` |
|        6 |  2024 | `	VmResetFunctionState(&(*pVm));` |
|        - |  2025 | `	/* (3) Release every object/variable reserved during the run. Re-reading the` |
|        - |  2026 | `	 * used count each iteration tolerates a destructor reserving a fresh slot. */` |
|      218 |  2027 | `	for( n = nWater ; n < SySetUsed(&pVm->aMemObj) ; ++n ){` |
|      212 |  2028 | `		ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      212 |  2029 | `		if( pObj ){` |
|      212 |  2030 | `			PH7_MemObjRelease(pObj);` |
|      106 |  2031 | `		}` |
|      106 |  2032 | `	}` |
|        - |  2033 | `	/* (4) Free the class static typed-property slots (instance ones are already` |
|        - |  2034 | `	 * gone — object release in step 3 removes each instance's own slot). */` |
|        6 |  2035 | `	VmResetTypedSlots(&(*pVm));` |
|        - |  2036 | `	/* (5) Unwind any active frames back to none. */` |
|       12 |  2037 | `	while( pVm->pFrame ){` |
|        6 |  2038 | `		VmLeaveFrame(&(*pVm));` |
|      ! 0 |  2039 | `	}` |
|        - |  2040 | `	/* Object teardown is complete; user __destruct may run normally again. */` |
|        6 |  2041 | `	pVm->bInReset = 0;` |
|        - |  2042 | `	/* (6) Truncate the object pool back to the watermark and forget stale free` |
|        - |  2043 | `	 * slots (their indices no longer exist). */` |
|        6 |  2044 | `	SySetTruncate(&pVm->aMemObj,nWater);` |
|        6 |  2045 | `	SySetReset(&pVm->aFreeObj);` |
|        - |  2046 | `	/* (7) Reset the superglobal name table and namespace scratch. */` |
|        6 |  2047 | `	SyHashRelease(&pVm->hSuper);` |
|        6 |  2048 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|        - |  2049 | `	/* (8) Drain remaining per-exec containers. */` |
|        6 |  2050 | `	SySetReset(&pVm->aSelf);` |
|        - |  2051 | `	/* Shutdown callbacks are normally drained+released by VmInvokeShutdownCallbacks` |
|        - |  2052 | `	 * at the end of exec; release any that survived an abandoned run (e.g. exit()` |
|        - |  2053 | `	 * inside a shutdown callback) so their owned callback/arg values don't leak. */` |
|        6 |  2054 | `	for( n = 0 ; n < SySetUsed(&pVm->aShutdown) ; ++n ){` |
|      ! 0 |  2055 | `		VmShutdownCB *pCB = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|      ! 0 |  2056 | `		if( pCB ){` |
|        - |  2057 | `			int iArg;` |
|      ! 0 |  2058 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|      ! 0 |  2059 | `			for( iArg = 0 ; iArg < pCB->nArg ; ++iArg ){` |
|      ! 0 |  2060 | `				PH7_MemObjRelease(&pCB->aArg[iArg]);` |
|      ! 0 |  2061 | `			}` |
|      ! 0 |  2062 | `		}` |
|      ! 0 |  2063 | `	}` |
|        6 |  2064 | `	SySetReset(&pVm->aShutdown);` |
|        6 |  2065 | `	SySetReset(&pVm->aException);` |
|        6 |  2066 | `	pVm->pPendingException = 0;` |
|        6 |  2067 | `	pVm->nExceptDepth = 0;` |
|        - |  2068 | `	/* spl_autoload_register() callbacks are per request */` |
|        6 |  2069 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|      ! 0 |  2070 | `		VmAutoloadCB *pCB = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|      ! 0 |  2071 | `		if( pCB ){` |
|      ! 0 |  2072 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|      ! 0 |  2073 | `		}` |
|      ! 0 |  2074 | `	}` |
|        6 |  2075 | `	SySetReset(&pVm->aAutoload);` |
|        - |  2076 | `	/* The reentrancy guard is empty outside an active autoload (the common case);` |
|        - |  2077 | `	 * only rebuild the table when an aborted autoload left entries behind. */` |
|        6 |  2078 | `	if( SyHashTotalEntry(&pVm->hAutoloadActive) ){` |
|      ! 0 |  2079 | `		SyHashRelease(&pVm->hAutoloadActive);` |
|      ! 0 |  2080 | `		SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|      ! 0 |  2081 | `	}` |
|        - |  2082 | `	/* Output buffers */` |
|        6 |  2083 | `	for( n = 0 ; n < SySetUsed(&pVm->aOB) ; ++n ){` |
|      ! 0 |  2084 | `		VmObEntry *pOb = (VmObEntry *)SySetAt(&pVm->aOB,n);` |
|      ! 0 |  2085 | `		if( pOb ){` |
|      ! 0 |  2086 | `			PH7_MemObjRelease(&pOb->sCallback);` |
|      ! 0 |  2087 | `			SyBlobRelease(&pOb->sOB);` |
|      ! 0 |  2088 | `		}` |
|      ! 0 |  2089 | `	}` |
|        6 |  2090 | `	SySetReset(&pVm->aOB);` |
|        6 |  2091 | `	pVm->nObDepth = 0;` |
|        - |  2092 | `	/* (9) Rebuild the global frame and the superglobals. */` |
|        - |  2093 | `	{` |
|        6 |  2094 | `		sxi32 rc = VmEnterFrame(&(*pVm),0,0,0);` |
|        6 |  2095 | `		if( rc == SXRET_OK ){` |
|        6 |  2096 | `			rc = PH7_HashmapCreateSuper(&(*pVm));` |
|        3 |  2097 | `		}` |
|        6 |  2098 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  2099 | `			return rc;` |
|        - |  2100 | `		}` |
|        - |  2101 | `	}` |
|        - |  2102 | `	/* (10) Re-mount the static/const attribute slots of every class. */` |
|        - |  2103 | `	{` |
|        - |  2104 | `		SyHashEntry *pEntry;` |
|        6 |  2105 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|      238 |  2106 | `		while( (pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|      232 |  2107 | `			sxi32 rc = VmMountUserClassAttrs(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|      232 |  2108 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  2109 | `				return rc;` |
|        - |  2110 | `			}` |
|      ! 0 |  2111 | `		}` |
|        - |  2112 | `	}` |
|        - |  2113 | `	/* (11) Reset the remaining scalar/per-exec fields. */` |
|        6 |  2114 | `	SyBlobReset(&pVm->sConsumer);` |
|        6 |  2115 | `	pVm->nOutputLen = 0;` |
|        6 |  2116 | `	VmReinitMemObj(&(*pVm),&pVm->sExec);` |
|        6 |  2117 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|        6 |  2118 | `	pVm->iResponseStatus = 200;` |
|        6 |  2119 | `	pVm->bHeadersSent = 0;` |
|        6 |  2120 | `	pVm->bHttpContext = 0;` |
|        6 |  2121 | `	VmReinitMemObj(&(*pVm),&pVm->aExceptionCB[0]);` |
|        6 |  2122 | `	VmReinitMemObj(&(*pVm),&pVm->aExceptionCB[1]);` |
|        6 |  2123 | `	VmReinitMemObj(&(*pVm),&pVm->aErrCB[0]);` |
|        6 |  2124 | `	VmReinitMemObj(&(*pVm),&pVm->aErrCB[1]);` |
|        6 |  2125 | `	VmReinitMemObj(&(*pVm),&pVm->sAssertCallback);` |
|        6 |  2126 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  2127 | `#ifdef PH7_ENABLE_PCRE` |
|        6 |  2128 | `	pVm->iPcreLastError = 0;` |
|        - |  2129 | `#endif` |
|        6 |  2130 | `	pVm->iCmpCallbackExc = 0;` |
|        6 |  2131 | `	pVm->bHaltRequested = 0;` |
|        6 |  2132 | `	pVm->iExitStatus = 0;` |
|        6 |  2133 | `	pVm->iSpreadExtra = 0;` |
|        6 |  2134 | `	pVm->nRecursionDepth = 0;` |
|        6 |  2135 | `	pVm->pActiveCtx = 0;` |
|        6 |  2136 | `	pVm->pCoalesceObj = 0;` |
|        6 |  2137 | `	pVm->bCoalesceArmed = 0;` |
|        6 |  2138 | `	VmReinitMemObj(&(*pVm),&pVm->sCoalesceKey);` |
|        - |  2139 | `	/* Re-roll the uniqid() seed, matching PH7_VmMakeReady(). */` |
|        6 |  2140 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  2141 | `	/* Set the ready flag */` |
|        6 |  2142 | `	pVm->nMagic = PH7_VM_RUN;` |
|        6 |  2143 | `	return SXRET_OK;` |
|        3 |  2144 |  |
|        - |  2145 | `/*` |
|        - |  2146 | ` * Release a Virtual Machine.` |
|        - |  2147 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  2148 | ` */` |
|     2834 |  2149 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  2150 |  |
|        - |  2151 | `	/* Set the stale magic number */` |
|     2836 |  2152 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  2153 | `	/* Release the private memory subsystem */` |
|     2836 |  2154 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2836 |  2155 | `	return SXRET_OK;` |
|        2 |  2156 |  |
|        - |  2157 | `/*` |
|        - |  2158 | ` * Initialize a foreign function call context.` |
|        - |  2159 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  2160 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  2161 | ` * functions.` |
|        - |  2162 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  2163 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  2164 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  2165 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  2166 | ` */` |
|   701410 |  2167 | `static sxi32 VmInitCallContext(` |
|        - |  2168 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  2169 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  2170 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  2171 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  2172 | `	sxi32 iFlags          /* Control flags */` |
|        - |  2173 | `	)` |
|        2 |  2174 |  |
|   701412 |  2175 | `	pOut->pFunc = pFunc;` |
|   701412 |  2176 | `	pOut->pVm   = pVm;` |
|   701412 |  2177 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   701412 |  2178 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  2179 | `	/* Assume a null return value */` |
|   701412 |  2180 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   701412 |  2181 | `	pOut->pRet = pRet;` |
|   701412 |  2182 | `	pOut->iFlags = iFlags;` |
|   701412 |  2183 | `	return SXRET_OK;` |
|        2 |  2184 |  |
|        - |  2185 | `/*` |
|        - |  2186 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  2187 | ` * left behind.` |
|        - |  2188 | ` */` |
|   701410 |  2189 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  2190 |  |
|        - |  2191 | `	sxu32 n;` |
|   701412 |  2192 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     8710 |  2193 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    25452 |  2194 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    16744 |  2195 | `			if( apObj[n] == 0 ){` |
|        - |  2196 | `				/* Already released */` |
|      384 |  2197 | `				continue;` |
|        - |  2198 | `			}` |
|    16362 |  2199 | `			PH7_MemObjRelease(apObj[n]);` |
|    16362 |  2200 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     8182 |  2201 | `		}` |
|     8710 |  2202 | `		SySetRelease(&pCtx->sVar);` |
|     4354 |  2203 | `	}` |
|   701412 |  2204 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  2205 | `		ph7_aux_data *aAux;` |
|        - |  2206 | `		void *pChunk;` |
|        - |  2207 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  2208 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  2209 | `		 */` |
|        9 |  2210 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  2211 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  2212 | `			pChunk = aAux[n].pAuxData;` |
|        - |  2213 | `			/* Release the chunk */` |
|       25 |  2214 | `			if( pChunk ){` |
|       25 |  2215 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  2216 | `			}` |
|       13 |  2217 | `		}` |
|        9 |  2218 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  2219 | `	}` |
|   701412 |  2220 |  |
|        - |  2221 | `/*` |
|        - |  2222 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  2223 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  2224 | ` */` |
|      382 |  2225 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  2226 | `	ph7_context *pCtx, /* Call context */` |
|        - |  2227 | `	ph7_value *pValue  /* Release this value */` |
|        - |  2228 | `	)` |
|        2 |  2229 |  |
|      384 |  2230 | `	if( pValue == 0 ){` |
|        - |  2231 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  2232 | `		return;` |
|        - |  2233 | `	}` |
|      384 |  2234 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      384 |  2235 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  2236 | `		sxu32 n;` |
|     1282 |  2237 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     1282 |  2238 | `			if( apObj[n] == pValue ){` |
|      384 |  2239 | `				PH7_MemObjRelease(pValue);` |
|      384 |  2240 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  2241 | `				/* Mark as released */` |
|      384 |  2242 | `				apObj[n] = 0;` |
|      384 |  2243 | `				break;` |
|        - |  2244 | `			}` |
|      451 |  2245 | `		}` |
|      191 |  2246 | `	}` |
|      193 |  2247 |  |
|        - |  2248 | `/*` |
|        - |  2249 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  2250 | ` */` |
|  3968344 |  2251 | `static void VmPopOperand(` |
|        - |  2252 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  2253 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  2254 | `	)` |
|        2 |  2255 |  |
|  3968346 |  2256 | `	ph7_value *pTos = *ppTos;` |
|  8455848 |  2257 | `	while( nPop > 0 ){` |
|  4487504 |  2258 | `		PH7_MemObjRelease(pTos);` |
|  4487504 |  2259 | `		pTos--;` |
|  4487504 |  2260 | `		nPop--;` |
|        2 |  2261 | `	}` |
|        - |  2262 | `	/* Top of the stack */` |
|  3968346 |  2263 | `	*ppTos = pTos;` |
|  3968346 |  2264 |  |
|        - |  2265 | `/*` |
|        - |  2266 | ` * Reserve a memory object.` |
|        - |  2267 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  2268 | ` */` |
|  3215816 |  2269 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  2270 |  |
|  3215818 |  2271 | `	ph7_value *pObj = 0;` |
|        - |  2272 | `	VmSlot *pSlot;` |
|        - |  2273 | `	sxu32 nIdx;` |
|        - |  2274 | `	/* Check for a free slot */` |
|  3215818 |  2275 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3215818 |  2276 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3215818 |  2277 | `	if( pSlot ){` |
|  1063722 |  2278 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|  1063722 |  2279 | `		nIdx = pSlot->nIdx;` |
|   531860 |  2280 | `	}` |
|  3215818 |  2281 | `	if( pObj == 0 ){` |
|        - |  2282 | `		/* Reserve a new memory object */` |
|  2152098 |  2283 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2152098 |  2284 | `		if( pObj == 0 ){` |
|      ! 0 |  2285 | `			return 0;` |
|        - |  2286 | `		}` |
|  1076048 |  2287 | `	}` |
|        - |  2288 | `	/* Set a null default value */` |
|  3215818 |  2289 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3215818 |  2290 | `	pObj->nIdx = nIdx;` |
|  3215818 |  2291 | `	return pObj;` |
|  1607910 |  2292 |  |
|        - |  2293 | `/*` |
|        - |  2294 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  2295 | ` */` |
|    35490 |  2296 | `static sxi32 VmHashmapRefInsert(` |
|        - |  2297 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  2298 | `	const char *zKey,  /* Entry key */` |
|        - |  2299 | `	sxu32 nByte,       /* Key length */` |
|        - |  2300 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  2301 | `	)` |
|        2 |  2302 |  |
|        - |  2303 | `	ph7_value sKey;` |
|        - |  2304 | `	sxi32 rc;` |
|    35492 |  2305 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    35492 |  2306 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  2307 | `	/* Perform the insertion */` |
|    35492 |  2308 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    35492 |  2309 | `	PH7_MemObjRelease(&sKey);` |
|    35492 |  2310 | `	return rc;` |
|        2 |  2311 |  |
|        - |  2312 | `/*` |
|        - |  2313 | ` * Extract a variable value from the top active VM frame.` |
|        - |  2314 | ` * Return a pointer to the variable value on success.` |
|        - |  2315 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  2316 | ` */` |
|  3685044 |  2317 | `static ph7_value * VmExtractMemObj(` |
|        - |  2318 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  2319 | `	const SyString *pName, /* Variable name */` |
|        - |  2320 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  2321 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  2322 | `	)` |
|        2 |  2323 |  |
|  3685046 |  2324 | `	int bNullify = FALSE;` |
|        - |  2325 | `	SyHashEntry *pEntry;` |
|        - |  2326 | `	VmFrame *pFrame;` |
|        - |  2327 | `	ph7_value *pObj;` |
|        - |  2328 | `	sxu32 nIdx;` |
|        - |  2329 | `	sxi32 rc;` |
|        - |  2330 | `	/* Point to the top active frame */` |
|  3685046 |  2331 | `	pFrame = pVm->pFrame;` |
|  3685046 |  2332 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  2333 | `	/* Perform the lookup */` |
|  3685046 |  2334 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  2335 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  2336 | `		pName = &sAnnon;` |
|        - |  2337 | `		/* Always nullify the object */` |
|      ! 0 |  2338 | `		bNullify = TRUE;` |
|      ! 0 |  2339 | `		bDup = FALSE;` |
|      ! 0 |  2340 | `	}` |
|        - |  2341 | `	/* Check the superglobals table first */` |
|  3685046 |  2342 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3685046 |  2343 | `	if( pEntry == 0 ){` |
|        - |  2344 | `		/* Query the top active frame */` |
|  3685000 |  2345 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3685000 |  2346 | `		if( pEntry == 0 ){` |
|   114388 |  2347 | `			char *zName = (char *)pName->zString;` |
|        - |  2348 | `			VmSlot sLocal;` |
|   114388 |  2349 | `			if( !bCreate ){` |
|        - |  2350 | `				/* Do not create the variable,return NULL instead */` |
|      986 |  2351 | `				return 0;` |
|        - |  2352 | `			}` |
|        - |  2353 | `			/* No such variable,automatically create a new one and install` |
|        - |  2354 | `			 * it in the current frame.` |
|        - |  2355 | `			 */` |
|   113404 |  2356 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|   113404 |  2357 | `			if( pObj == 0 ){` |
|      ! 0 |  2358 | `				return 0;` |
|        - |  2359 | `			}` |
|   113404 |  2360 | `			nIdx = pObj->nIdx;` |
|   113404 |  2361 | `			if( bDup ){` |
|        - |  2362 | `				/* Duplicate name */` |
|      230 |  2363 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      230 |  2364 | `				if( zName == 0 ){` |
|      ! 0 |  2365 | `					return 0;` |
|        - |  2366 | `				}` |
|      114 |  2367 | `			}` |
|        - |  2368 | `			/* Link to the top active VM frame */` |
|   113404 |  2369 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|   113404 |  2370 | `			if( rc != SXRET_OK ){` |
|        - |  2371 | `				/* Return the slot to the free pool */` |
|      ! 0 |  2372 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  2373 | `				sLocal.pUserData = 0;` |
|      ! 0 |  2374 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  2375 | `				return 0;` |
|        - |  2376 | `			}` |
|   113404 |  2377 | `			if( pFrame->pParent != 0 ){` |
|        - |  2378 | `				/* Local variable */` |
|   106346 |  2379 | `				sLocal.nIdx = nIdx;` |
|   106346 |  2380 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    53174 |  2381 | `			}else{` |
|        - |  2382 | `				/* Register in the $GLOBALS array */` |
|     7060 |  2383 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  2384 | `			}` |
|        - |  2385 | `			/* Install in the reference table */` |
|   113404 |  2386 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  2387 | `			/* Save object index */` |
|   113404 |  2388 | `			pObj->nIdx = nIdx;` |
|    56703 |  2389 | `		}else{` |
|        - |  2390 | `			/* Extract variable contents */` |
|  3570614 |  2391 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3570614 |  2392 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3570614 |  2393 | `			if( bNullify && pObj ){` |
|      ! 0 |  2394 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  2395 | `			}` |
|        - |  2396 | `		}` |
|  1842119 |  2397 | `	}else{` |
|        - |  2398 | `		/* Superglobal */` |
|       48 |  2399 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       48 |  2400 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  2401 | `	}` |
|  3684062 |  2402 | `	return pObj;` |
|  1842634 |  2403 |  |
|        - |  2404 | `/*` |
|        - |  2405 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  2406 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  2407 | ` */` |
|     3264 |  2408 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  2409 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  2410 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  2411 | `	sxu32 nByte        /* zName length */` |
|        - |  2412 | `	)` |
|        2 |  2413 |  |
|        - |  2414 | `	SyHashEntry *pEntry;` |
|        - |  2415 | `	ph7_value *pValue;` |
|        - |  2416 | `	sxu32 nIdx;` |
|        - |  2417 | `	/* Query the superglobal table */` |
|     3266 |  2418 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     3266 |  2419 | `	if( pEntry == 0 ){` |
|        - |  2420 | `		/* No such entry */` |
|      ! 0 |  2421 | `		return 0;` |
|        - |  2422 | `	}` |
|        - |  2423 | `	/* Extract the superglobal index in the global object pool */` |
|     3266 |  2424 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2425 | `	/* Extract the variable value  */` |
|     3266 |  2426 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     3266 |  2427 | `	return pValue;` |
|     1634 |  2428 |  |
|        - |  2429 | `/*` |
|        - |  2430 | ` * Perform a raw hashmap insertion.` |
|        - |  2431 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  2432 | ` */` |
|     3306 |  2433 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  2434 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  2435 | `	const char *zKey,   /* Entry key */` |
|        - |  2436 | `	int nKeylen,        /* zKey length*/` |
|        - |  2437 | `	const char *zData,  /* Entry data */` |
|        - |  2438 | `	int nLen            /* zData length */` |
|        - |  2439 | `	)` |
|        2 |  2440 |  |
|        - |  2441 | `	ph7_value sKey,sValue;` |
|        - |  2442 | `	sxi32 rc;` |
|     3308 |  2443 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     3308 |  2444 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     3308 |  2445 | `	if( zKey ){` |
|     3286 |  2446 | `		if( nKeylen < 0 ){` |
|     3204 |  2447 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1601 |  2448 | `		}` |
|     3286 |  2449 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1642 |  2450 | `	}` |
|     3308 |  2451 | `	if( zData ){` |
|     3308 |  2452 | `		if( nLen < 0 ){` |
|        - |  2453 | `			/* Compute length automatically */` |
|      198 |  2454 | `			nLen = (int)SyStrlen(zData);` |
|       99 |  2455 | `		}` |
|     3308 |  2456 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1653 |  2457 | `	}` |
|        - |  2458 | `	/* Perform the insertion */` |
|     3308 |  2459 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     3308 |  2460 | `	PH7_MemObjRelease(&sKey);` |
|     3308 |  2461 | `	PH7_MemObjRelease(&sValue);` |
|     3308 |  2462 | `	return rc;` |
|        2 |  2463 |  |
|        - |  2464 | `/*` |
|        - |  2465 | ` * Configure a working virtual machine instance.` |
|        - |  2466 | ` *` |
|        - |  2467 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  2468 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  2469 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  2470 | ` * The second argument to this function is an integer configuration option` |
|        - |  2471 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  2472 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  2473 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  2474 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  2475 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  2476 | ` */` |
|    45872 |  2477 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  2478 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  2479 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  2480 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  2481 | `	)` |
|        2 |  2482 |  |
|    45874 |  2483 | `	sxi32 rc = SXRET_OK;` |
|    45874 |  2484 | `	switch(nOp){` |
|     1409 |  2485 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2820 |  2486 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2820 |  2487 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  2488 | `		/* VM output consumer callback */` |
|        - |  2489 | `#ifdef UNTRUST` |
|        - |  2490 | `		if( xConsumer == 0 ){` |
|        - |  2491 | `			rc = SXERR_CORRUPT;` |
|        - |  2492 | `			break;` |
|        - |  2493 | `		}` |
|        - |  2494 | `#endif` |
|        - |  2495 | `		/* Install the output consumer */` |
|     2820 |  2496 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2820 |  2497 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2820 |  2498 | `		break;` |
|        - |  2499 | `							   }` |
|     1417 |  2500 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  2501 | `		/* Import path */` |
|        - |  2502 | `		  const char *zPath;` |
|        - |  2503 | `		  SyString sPath;` |
|     2836 |  2504 | `		  zPath = va_arg(ap,const char *);` |
|        - |  2505 | `#if defined(UNTRUST)` |
|        - |  2506 | `		  if( zPath == 0 ){` |
|        - |  2507 | `			  rc = SXERR_EMPTY;` |
|        - |  2508 | `			  break;` |
|        - |  2509 | `		  }` |
|        - |  2510 | `#endif` |
|     2836 |  2511 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  2512 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  2513 | `#ifdef __WINNT__` |
|        2 |  2514 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  2515 | `#endif` |
|     5670 |  2516 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  2517 | `		  /* Remove leading and trailing white spaces */` |
|     2836 |  2518 | `		  SyStringFullTrim(&sPath);` |
|     2836 |  2519 | `		  if( sPath.nByte > 0 ){` |
|        - |  2520 | `			  /* Store the path in the corresponding conatiner */` |
|     2836 |  2521 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1417 |  2522 | `		  }` |
|     2836 |  2523 | `		  break;` |
|        - |  2524 | `									 }` |
|     1420 |  2525 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  2526 | `		/* Run-Time Error report */` |
|     2842 |  2527 | `		pVm->bErrReport = 1;` |
|     2842 |  2528 | `		break;` |
|      ! 0 |  2529 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  2530 | `		/* Recursion depth */` |
|      ! 0 |  2531 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  2532 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  2533 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  2534 | `		}` |
|      ! 0 |  2535 | `		break;` |
|        - |  2536 | `									   }` |
|      ! 0 |  2537 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  2538 | `		/* VM output length in bytes */` |
|      ! 0 |  2539 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  2540 | `#ifdef UNTRUST` |
|        - |  2541 | `		if( pOut == 0 ){` |
|        - |  2542 | `			rc = SXERR_CORRUPT;` |
|        - |  2543 | `			break;` |
|        - |  2544 | `		}` |
|        - |  2545 | `#endif` |
|      ! 0 |  2546 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  2547 | `		break;` |
|        - |  2548 | `							   }` |
|        - |  2549 |  |
|    14200 |  2550 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  2551 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  2552 | `		/* Create a new superglobal/global variable */` |
|    28402 |  2553 | `		const char *zName = va_arg(ap,const char *);` |
|    28402 |  2554 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  2555 | `		SyHashEntry *pEntry;` |
|        - |  2556 | `		ph7_value *pObj;` |
|        - |  2557 | `		sxu32 nByte;` |
|        - |  2558 | `		sxu32 nIdx;` |
|        - |  2559 | `#ifdef UNTRUST` |
|        - |  2560 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  2561 | `			rc = SXERR_CORRUPT;` |
|        - |  2562 | `			break;` |
|        - |  2563 | `		}` |
|        - |  2564 | `#endif` |
|    28402 |  2565 | `		nByte = SyStrlen(zName);` |
|    28402 |  2566 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2567 | `			/* Check if the superglobal is already installed */` |
|    28402 |  2568 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    14202 |  2569 | `		}else{` |
|        - |  2570 | `			/* Query the top active VM frame */` |
|      ! 0 |  2571 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  2572 | `		}` |
|    28402 |  2573 | `		if( pEntry ){` |
|        - |  2574 | `			/* Variable already installed */` |
|      ! 0 |  2575 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2576 | `			/* Extract contents */` |
|      ! 0 |  2577 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  2578 | `			if( pObj ){` |
|        - |  2579 | `				/* Overwrite old contents */` |
|      ! 0 |  2580 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  2581 | `			}` |
|      ! 0 |  2582 | `		}else{` |
|        - |  2583 | `			/* Install a new variable */` |
|    28402 |  2584 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    28402 |  2585 | `			if( pObj == 0 ){` |
|      ! 0 |  2586 | `				rc = SXERR_MEM;` |
|      ! 0 |  2587 | `				break;` |
|        - |  2588 | `			}` |
|    28402 |  2589 | `			nIdx = pObj->nIdx;` |
|        - |  2590 | `			/* Copy value */` |
|    28402 |  2591 | `			PH7_MemObjStore(pValue,pObj);` |
|    28402 |  2592 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2593 | `				/* Install the superglobal */` |
|    28402 |  2594 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    14202 |  2595 | `			}else{` |
|        - |  2596 | `				/* Install in the current frame */` |
|      ! 0 |  2597 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  2598 | `			}` |
|    28402 |  2599 | `			if( rc == SXRET_OK ){` |
|        - |  2600 | `				SyHashEntry *pRef;` |
|    28402 |  2601 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    28402 |  2602 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    14202 |  2603 | `				}else{` |
|      ! 0 |  2604 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  2605 | `				}` |
|        - |  2606 | `				/* Install in the reference table */` |
|    28402 |  2607 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    28402 |  2608 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  2609 | `					/* Register in the $GLOBALS array */` |
|    28402 |  2610 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    14200 |  2611 | `				}` |
|    14200 |  2612 | `			}` |
|        - |  2613 | `		}` |
|    28402 |  2614 | `		break;` |
|        - |  2615 | `									}` |
|     1601 |  2616 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  2617 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  2618 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  2619 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  2620 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  2621 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  2622 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     3204 |  2623 | `		const char *zKey   = va_arg(ap,const char *);` |
|     3204 |  2624 | `		const char *zValue = va_arg(ap,const char *);` |
|     3204 |  2625 | `		int nLen = va_arg(ap,int);` |
|        - |  2626 | `		ph7_hashmap *pMap;` |
|        - |  2627 | `		ph7_value *pValue;` |
|     3204 |  2628 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  2629 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2630 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     3203 |  2631 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2632 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2633 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     3202 |  2634 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2635 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2636 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     3202 |  2637 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2638 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2639 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     3202 |  2640 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2641 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2642 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     3202 |  2643 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2644 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2645 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2646 | `		}else{` |
|        - |  2647 | `			/* Extract the $_SERVER superglobal */` |
|     3202 |  2648 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2649 | `		}` |
|     3204 |  2650 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2651 | `			/* No such entry */` |
|      ! 0 |  2652 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2653 | `			break;` |
|        - |  2654 | `		}` |
|        - |  2655 | `		/* Point to the hashmap */` |
|     3204 |  2656 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2657 | `		/* Perform the insertion */` |
|     3204 |  2658 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     3204 |  2659 | `		break;` |
|        - |  2660 | `								   }` |
|       11 |  2661 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2662 | `		/* Script arguments */` |
|       24 |  2663 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2664 | `		ph7_hashmap *pMap;` |
|        - |  2665 | `		ph7_value *pValue;` |
|        - |  2666 | `		sxu32 n;` |
|       24 |  2667 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2668 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2669 | `			break;` |
|        - |  2670 | `		}` |
|        - |  2671 | `		/* Extract the $argv array */` |
|       24 |  2672 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  2673 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2674 | `			/* No such entry */` |
|      ! 0 |  2675 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2676 | `			break;` |
|        - |  2677 | `		}` |
|        - |  2678 | `		/* Point to the hashmap */` |
|       24 |  2679 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2680 | `		/* Perform the insertion */` |
|       24 |  2681 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  2682 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  2683 | `		if( rc == SXRET_OK ){` |
|       24 |  2684 | `			if( pMap->nEntry > 1 ){` |
|        - |  2685 | `				/* Append space separator first */` |
|       18 |  2686 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2687 | `			}` |
|       24 |  2688 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2689 | `		}` |
|       24 |  2690 | `		break;` |
|        - |  2691 | `								  }` |
|      ! 0 |  2692 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2693 | `		/* error_log() consumer */` |
|      ! 0 |  2694 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2695 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2696 | `		break;` |
|        - |  2697 | `										}` |
|      ! 0 |  2698 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2699 | `		/* Script return value */` |
|      ! 0 |  2700 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2701 | `#ifdef UNTRUST` |
|        - |  2702 | `		if( ppValue == 0 ){` |
|        - |  2703 | `			rc = SXERR_CORRUPT;` |
|        - |  2704 | `			break;` |
|        - |  2705 | `		}` |
|        - |  2706 | `#endif` |
|      ! 0 |  2707 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2708 | `		break;` |
|        - |  2709 | `								   }` |
|     2834 |  2710 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2711 | `		/* Register an IO stream device */` |
|     5670 |  2712 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2713 | `		/* Make sure we are dealing with a valid IO stream */` |
|     8502 |  2714 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     5670 |  2715 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2716 | `				/* Invalid stream */` |
|      ! 0 |  2717 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2718 | `				break;` |
|        - |  2719 | `		}` |
|     5670 |  2720 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2721 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2836 |  2722 | `			pVm->pDefStream = pStream;` |
|     1417 |  2723 | `		}` |
|        - |  2724 | `		/* Insert in the appropriate container */` |
|     5670 |  2725 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     5670 |  2726 | `		break;` |
|        - |  2727 | `								  }` |
|       11 |  2728 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2729 | `		/* Point to the VM internal output consumer buffer */` |
|       22 |  2730 | `		const void **ppOut = va_arg(ap,const void **);` |
|       22 |  2731 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2732 | `#ifdef UNTRUST` |
|        - |  2733 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2734 | `			rc = SXERR_CORRUPT;` |
|        - |  2735 | `			break;` |
|        - |  2736 | `		}` |
|        - |  2737 | `#endif` |
|       22 |  2738 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|       22 |  2739 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|       22 |  2740 | `		break;` |
|        - |  2741 | `									   }` |
|       11 |  2742 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2743 | `		/* Raw HTTP request*/` |
|       22 |  2744 | `		const char *zRequest = va_arg(ap,const char *);` |
|       22 |  2745 | `		int nByte = va_arg(ap,int);` |
|       22 |  2746 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2747 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2748 | `			break;` |
|        - |  2749 | `		}` |
|       22 |  2750 | `		if( nByte < 0 ){` |
|        - |  2751 | `			/* Compute length automatically */` |
|      ! 0 |  2752 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2753 | `		}` |
|        - |  2754 | `		/* Process the request */` |
|       22 |  2755 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|        - |  2756 | `		/* Mark this VM as operating in HTTP context only on success */` |
|       22 |  2757 | `		if( rc == SXRET_OK ){` |
|       22 |  2758 | `			pVm->bHttpContext = 1;` |
|       11 |  2759 | `		}` |
|       22 |  2760 | `		break;` |
|        - |  2761 | `									}` |
|       11 |  2762 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|        - |  2763 | `		/* Extract HTTP response status code */` |
|       22 |  2764 | `		int *pStatus = va_arg(ap, int *);` |
|       22 |  2765 | `		if( pStatus ){` |
|       22 |  2766 | `			*pStatus = pVm->iResponseStatus;` |
|       11 |  2767 | `		}` |
|       22 |  2768 | `		break;` |
|        - |  2769 | `										}` |
|       11 |  2770 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|        - |  2771 | `		/* Iterate response headers via callback */` |
|        - |  2772 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|       22 |  2773 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|       22 |  2774 | `		void *pUserData = va_arg(ap, void *);` |
|       22 |  2775 | `		if( xCallback ){` |
|       22 |  2776 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|       22 |  2777 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       34 |  2778 | `			for( k = 0; k < nHdr; k++ ){` |
|       18 |  2779 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|       12 |  2780 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        6 |  2781 | `							   pUserData);` |
|       12 |  2782 | `				if( rc != PH7_OK ){` |
|      ! 0 |  2783 | `					break;` |
|        - |  2784 | `				}` |
|        6 |  2785 | `			}` |
|       11 |  2786 | `		}` |
|       22 |  2787 | `		break;` |
|        - |  2788 | `										 }` |
|      ! 0 |  2789 | `	default:` |
|        - |  2790 | `		/* Unknown configuration option */` |
|      ! 0 |  2791 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2792 | `		break;` |
|        - |  2793 | `	}` |
|    45874 |  2794 | `	return rc;` |
|        2 |  2795 |  |
|        - |  2796 | `/* Forward declaration */` |
|        - |  2797 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2798 | `/*` |
|        - |  2799 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2800 | ` * format.` |
|        - |  2801 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2802 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2803 | ` * (STDOUT).` |
|        - |  2804 | ` */` |
|        2 |  2805 | `static sxi32 VmByteCodeDump(` |
|        - |  2806 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2807 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2808 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2809 | `	)` |
|        1 |  2810 |  |
|        - |  2811 | `	static const char zDump[] = {` |
|        - |  2812 | `		"====================================================\n"` |
|        - |  2813 | `		"PH7 VM Dump\n"` |
|        - |  2814 | `		"====================================================\n"` |
|        - |  2815 | `	};` |
|        - |  2816 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2817 | `	sxi32 rc = SXRET_OK;` |
|        - |  2818 | `	sxu32 n;` |
|        - |  2819 | `	/* Point to the PH7 instructions */` |
|        3 |  2820 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2821 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2822 | `	n = 0;` |
|        3 |  2823 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2824 | `	/* Dump instructions */` |
|        7 |  2825 | `	for(;;){` |
|       15 |  2826 | `		if( pInstr >= pEnd ){` |
|        - |  2827 | `			/* No more instructions */` |
|        3 |  2828 | `			break;` |
|        - |  2829 | `		}` |
|        - |  2830 | `		/* Format and call the consumer callback */` |
|       19 |  2831 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       12 |  2832 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 |  2833 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       13 |  2834 | `		if( rc != SXRET_OK ){` |
|        - |  2835 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2836 | `			return rc;` |
|        - |  2837 | `		}` |
|       13 |  2838 | `		++n;` |
|       13 |  2839 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2840 | `	}` |
|        3 |  2841 | `	return rc;` |
|        2 |  2842 |  |
|        - |  2843 | `/* Forward declaration */` |
|        - |  2844 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2845 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2846 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2847 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2848 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2849 | `/*` |
|        - |  2850 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2851 | ` * consumer callback.` |
|        - |  2852 | ` */` |
|      604 |  2853 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2854 |  |
|      605 |  2855 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      605 |  2856 | `	sxi32 rc = SXRET_OK;` |
|        - |  2857 | `	/* Append a new line */` |
|        - |  2858 | `#ifdef __WINNT__` |
|        1 |  2859 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2860 | `#else` |
|      604 |  2861 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2862 | `#endif` |
|        - |  2863 | `	/* Invoke the output consumer callback */` |
|      605 |  2864 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      605 |  2865 | `	VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|      605 |  2866 | `	return rc;` |
|        1 |  2867 |  |
|        - |  2868 | `/*` |
|        - |  2869 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2870 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2871 | ` * information.` |
|        - |  2872 | ` */` |
|      152 |  2873 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2874 |  |
|      154 |  2875 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2876 | `		ph7_value apArg[4];` |
|        - |  2877 | `		ph7_value *apArgPtr[4];` |
|        - |  2878 | `		ph7_value sResult;` |
|        - |  2879 | `		SyString sErr;` |
|        - |  2880 | `		/* Prepare arguments */` |
|       76 |  2881 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2882 | `			/* use explicit message length to avoid reading past buffer */` |
|       76 |  2883 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       76 |  2884 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       76 |  2885 | `		if( pFile ){` |
|       76 |  2886 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       76 |  2887 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       39 |  2888 | `		}else{` |
|      ! 0 |  2889 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2890 | `		}` |
|       76 |  2891 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       76 |  2892 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2893 | `		/* Set up pointer array */` |
|       76 |  2894 | `		apArgPtr[0] = &apArg[0];` |
|       76 |  2895 | `		apArgPtr[1] = &apArg[1];` |
|       76 |  2896 | `		apArgPtr[2] = &apArg[2];` |
|       76 |  2897 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2898 | `		/* Call the handler */` |
|       76 |  2899 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2900 | `		/* Check return value */` |
|       76 |  2901 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2902 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2903 | `		}` |
|        - |  2904 | `		/* Release */` |
|       76 |  2905 | `		PH7_MemObjRelease(&apArg[0]);` |
|       76 |  2906 | `		PH7_MemObjRelease(&apArg[1]);` |
|       76 |  2907 | `		PH7_MemObjRelease(&apArg[2]);` |
|       76 |  2908 | `		PH7_MemObjRelease(&apArg[3]);` |
|       76 |  2909 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2910 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2911 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       76 |  2912 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2913 | `	}` |
|        - |  2914 | `	/* No handler, always call error handler */` |
|       79 |  2915 | `	return TRUE;` |
|       78 |  2916 |  |
|      110 |  2917 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2918 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2919 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2920 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2921 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2922 | `	)` |
|        2 |  2923 |  |
|      112 |  2924 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2925 | `	SyString *pFile;` |
|        - |  2926 | `	char *zErr;` |
|      112 |  2927 | `	sxi32 rc = SXRET_OK;` |
|      112 |  2928 | `	if( !pVm->bErrReport ){` |
|        - |  2929 | `		/* Don't bother reporting errors */` |
|        3 |  2930 | `		return SXRET_OK;` |
|        - |  2931 | `	}` |
|        - |  2932 | `	/* Reset the working buffer */` |
|      110 |  2933 | `	SyBlobReset(pWorker);` |
|        - |  2934 | `	/* Peek the processed file if available */` |
|      110 |  2935 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      110 |  2936 | `	if( pFile ){` |
|        - |  2937 | `		/* Append file name */` |
|      110 |  2938 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|      110 |  2939 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       54 |  2940 | `	}` |
|        - |  2941 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2942 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2943 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2944 | `	 * E_DEPRECATED). */` |
|      110 |  2945 | `	zErr = "Error:  ";` |
|      110 |  2946 | `	switch(iErr){` |
|       19 |  2947 | `	case PH7_CTX_WARNING:` |
|       40 |  2948 | `		zErr = "Warning:  ";` |
|       40 |  2949 | `		break;` |
|        6 |  2950 | `	case PH7_CTX_NOTICE:` |
|       14 |  2951 | `		zErr = "Notice:  ";` |
|       12 |  2952 | `		break;` |
|       29 |  2953 | `	default:` |
|        - |  2954 | `		/* keep iErr unchanged */` |
|       58 |  2955 | `		break;` |
|        - |  2956 | `	}` |
|      110 |  2957 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|      110 |  2958 | `	if( pFuncName ){` |
|        - |  2959 | `		/* Append function name first */` |
|       23 |  2960 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       23 |  2961 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       11 |  2962 | `	}` |
|      110 |  2963 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2964 | `	/* Check for user error handler.  compute length of C string */` |
|      110 |  2965 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       49 |  2966 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       24 |  2967 | `	}` |
|      110 |  2968 | `	return rc;` |
|       57 |  2969 |  |
|        - |  2970 | `/*` |
|        - |  2971 | ` * Raise an out-of-memory fatal and request a clean VM halt.` |
|        - |  2972 | ` *` |
|        - |  2973 | ` * This is the single choke point for surfacing an allocation failure that would` |
|        - |  2974 | ` * otherwise produce a silently-wrong result (a truncated string/array returned` |
|        - |  2975 | ` * with a success status). It mirrors PHP's non-catchable OOM fatal: it emits a` |
|        - |  2976 | ` * fatal-level diagnostic, sets a nonzero process exit status, and requests a` |
|        - |  2977 | ` * VM-wide halt that unwinds via the OP_CALL/abort path — which still runs` |
|        - |  2978 | ` * register_shutdown_function() callbacks (see PH7_VmByteCodeExec). Callers` |
|        - |  2979 | `` * return the value of this function (PH7_ABORT) directly, or `goto Abort` after`` |
|        - |  2980 | ` * calling it from a VM op.` |
|        - |  2981 | ` */` |
|      ! 0 |  2982 | `PH7_PRIVATE sxi32 PH7_VmMemoryError(ph7_vm *pVm)` |
|      ! 0 |  2983 |  |
|      ! 0 |  2984 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - |  2985 | `	/* Non-catchable, terminate with a PHP-like fatal exit status */` |
|      ! 0 |  2986 | `	pVm->iExitStatus = 255;` |
|      ! 0 |  2987 | `	pVm->bHaltRequested = 1;` |
|      ! 0 |  2988 | `	return PH7_ABORT;` |
|      ! 0 |  2989 |  |
|        - |  2990 | `/*` |
|        - |  2991 | ` * Context wrapper around PH7_VmMemoryError() for foreign/builtin functions.` |
|        - |  2992 | ` */` |
|      ! 0 |  2993 | `PH7_PRIVATE sxi32 PH7_ContextMemoryError(ph7_context *pCtx)` |
|      ! 0 |  2994 |  |
|      ! 0 |  2995 | `	return PH7_VmMemoryError(pCtx->pVm);` |
|      ! 0 |  2996 |  |
|        - |  2997 | `/*` |
|        - |  2998 | ` * Single source of truth for the call-recursion cap policy. Each recursion` |
|        - |  2999 | ` * entry point (OP_CALL, eval/include, fibers/generators) tests this before` |
|        - |  3000 | ` * descending another native C frame; the control flow on a hit differs per` |
|        - |  3001 | ` * site, but the rule itself lives here.` |
|        - |  3002 | ` */` |
|    32202 |  3003 | `static int VmRecursionExceeded(ph7_vm *pVm)` |
|        2 |  3004 |  |
|    32204 |  3005 | `	return pVm->nRecursionDepth > pVm->nMaxDepth;` |
|        2 |  3006 |  |
|        - |  3007 | `/*` |
|        - |  3008 | ` * Raise the recursion-limit fatal and request a clean VM halt. Mirrors` |
|        - |  3009 | ` * PH7_VmMemoryError and PHP 8.3's non-catchable "Maximum call stack size` |
|        - |  3010 | ` * reached": a catchable Error can't be used here because PH7 runs the catch` |
|        - |  3011 | ` * body (and renders an uncaught exception) inline at the throw-site depth —` |
|        - |  3012 | ` * which is already over the cap, so getMessage()/__toString()/the catch body` |
|        - |  3013 | ` * would re-trip the limit and recurse forever. A clean fatal removes the old` |
|        - |  3014 | ` * silent "return NULL and continue" hazard while keeping the promise that deep` |
|        - |  3015 | ` * recursion never panics: it unwinds via the abort path and still runs` |
|        - |  3016 | ` * register_shutdown_function() callbacks. Used by every recursion path —` |
|        - |  3017 | ` * OP_CALL, eval()/include/require (VmEvalChunk) and fibers/generators` |
|        - |  3018 | ` * (VmStartCtx/VmResumeCtx).` |
|        - |  3019 | ` *` |
|        - |  3020 | ` * Halt is requested BEFORE emitting the diagnostic, and a re-entry guard makes` |
|        - |  3021 | ` * this idempotent, so an error handler that itself recurses past the cap can't` |
|        - |  3022 | ` * re-enter and loop.` |
|        - |  3023 | ` */` |
|        6 |  3024 | `static sxi32 VmRecursionFatal(ph7_vm *pVm)` |
|        1 |  3025 |  |
|        7 |  3026 | `	if( pVm->bHaltRequested ){` |
|      ! 0 |  3027 | `		return PH7_ABORT;` |
|        - |  3028 | `	}` |
|        7 |  3029 | `	pVm->iExitStatus = 255;` |
|        7 |  3030 | `	pVm->bHaltRequested = 1;` |
|        7 |  3031 | `	VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Maximum recursion depth of %d reached",pVm->nMaxDepth);` |
|        7 |  3032 | `	return PH7_ABORT;` |
|        4 |  3033 |  |
|        - |  3034 | `/*` |
|        - |  3035 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3036 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3037 | ` * information.` |
|        - |  3038 | ` */` |
|       44 |  3039 | `static sxi32 VmThrowErrorAp(` |
|        - |  3040 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  3041 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  3042 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  3043 | `	const char *zFormat, /* Format message */` |
|        - |  3044 | `	va_list ap           /* Variable list of arguments */` |
|        - |  3045 | `	)` |
|        2 |  3046 |  |
|       46 |  3047 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  3048 | `	SyBlob sMsg;` |
|        - |  3049 | `	SyString *pFile;` |
|        - |  3050 | `	char *zErr;` |
|       46 |  3051 | `	sxi32 rc = SXRET_OK;` |
|       46 |  3052 | `	if( !pVm->bErrReport ){` |
|        - |  3053 | `		/* Don't bother reporting errors */` |
|      ! 0 |  3054 | `		return SXRET_OK;` |
|        - |  3055 | `	}` |
|        - |  3056 | `	/* Reset the working buffer */` |
|       46 |  3057 | `	SyBlobReset(pWorker);` |
|        - |  3058 | `	/* Peek the processed file if available */` |
|       46 |  3059 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       46 |  3060 | `	if( pFile ){` |
|        - |  3061 | `		/* Append file name */` |
|       46 |  3062 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       46 |  3063 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       22 |  3064 | `	}` |
|        - |  3065 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  3066 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  3067 | `	 * the correct errno value. */` |
|       46 |  3068 | `	zErr = "Error:  ";` |
|       46 |  3069 | `	switch(iErr){` |
|        4 |  3070 | `	case PH7_CTX_WARNING:` |
|        9 |  3071 | `		zErr = "Warning:  ";` |
|        9 |  3072 | `		break;` |
|        3 |  3073 | `	case PH7_CTX_NOTICE:` |
|        7 |  3074 | `		zErr = "Notice:  ";` |
|        6 |  3075 | `		break;` |
|       15 |  3076 | `	default:` |
|        - |  3077 | `		/* do not change iErr */` |
|       30 |  3078 | `		break;` |
|        - |  3079 | `	}` |
|       46 |  3080 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       46 |  3081 | `	if( pFuncName ){` |
|        - |  3082 | `		/* Append function name first */` |
|       26 |  3083 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  3084 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  3085 | `	}` |
|        - |  3086 | `	/* Format the raw message */` |
|       46 |  3087 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       46 |  3088 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  3089 | `	/* Check if a user error handler is installed */` |
|       46 |  3090 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  3091 | `		/* No handler or handler returned TRUE, normal processing */` |
|       31 |  3092 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       31 |  3093 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       15 |  3094 | `	}` |
|       46 |  3095 | `	SyBlobRelease(&sMsg);` |
|       46 |  3096 | `	return rc;` |
|       24 |  3097 |  |
|        - |  3098 | `/*` |
|        - |  3099 | ` * Throw a PHP-compatible TypeError whose message describes a failed typed` |
|        - |  3100 | ` * property assignment. Called from the STORE path when coercion is not` |
|        - |  3101 | ` * possible.` |
|        - |  3102 | ` */` |
|       42 |  3103 | `static sxi32 VmThrowPropertyTypeError(ph7_vm *pVm,VmClassAttr *pVmAttr,const char *zGiven)` |
|        2 |  3104 |  |
|        - |  3105 | `	ph7_class *pClass;` |
|       44 |  3106 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|        - |  3107 | `	ph7_class_instance *pThis;` |
|        - |  3108 | `	ph7_class_method *pCons;` |
|        - |  3109 | `	ph7_value sArg;` |
|        - |  3110 | `	ph7_value *apArg[1];` |
|        - |  3111 | `	SyBlob sMsg;` |
|        - |  3112 | `	SyString sMsgStr;` |
|        - |  3113 | `	VmFrame *pFrame;` |
|        - |  3114 | `	sxi32 rc;` |
|       44 |  3115 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       44 |  3116 | `	if( pClass == 0 ){` |
|      ! 0 |  3117 | `		return PH7_ABORT;` |
|        - |  3118 | `	}` |
|       44 |  3119 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       44 |  3120 | `	if( pThis == 0 ){` |
|      ! 0 |  3121 | `		return PH7_ABORT;` |
|        - |  3122 | `	}` |
|       44 |  3123 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  3124 | `	/* Prefer the declaring class over the runtime instance class so that an` |
|        - |  3125 | `	 * inherited typed property reports its original owner, matching PHP. */` |
|        - |  3126 | `	{` |
|       44 |  3127 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|       44 |  3128 | `		if( pOwner ){` |
|       44 |  3129 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %z",` |
|       21 |  3130 | `				zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);` |
|       23 |  3131 | `		}else{` |
|      ! 0 |  3132 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property $%z of type %z",` |
|      ! 0 |  3133 | `				zGiven,&pAttr->sName,&pAttr->sTypeName);` |
|        - |  3134 | `		}` |
|        - |  3135 | `	}` |
|       44 |  3136 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       44 |  3137 | `	if( pCons ){` |
|       44 |  3138 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       44 |  3139 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       44 |  3140 | `		apArg[0] = &sArg;` |
|       44 |  3141 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       44 |  3142 | `		PH7_MemObjRelease(&sArg);` |
|       21 |  3143 | `	}` |
|       44 |  3144 | `	SyBlobRelease(&sMsg);` |
|       44 |  3145 | `	pFrame = pVm->pFrame;` |
|       44 |  3146 | `	if( pFrame ){` |
|       44 |  3147 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       44 |  3148 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       21 |  3149 | `	}` |
|       44 |  3150 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       44 |  3151 | `	PH7_ClassInstanceUnref(pThis);` |
|       44 |  3152 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  3153 | `		return PH7_ABORT;` |
|        - |  3154 | `	}` |
|       44 |  3155 | `	return PH7_EXCEPTION;` |
|       23 |  3156 |  |
|        - |  3157 |  |
|        - |  3158 | `/*` |
|        - |  3159 | ` * Throw a PHP-compatible Error for reading an uninitialized typed property.` |
|        - |  3160 | ` */` |
|        4 |  3161 | `static sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 |  3162 |  |
|        - |  3163 | `	ph7_class *pErrClass;` |
|        - |  3164 | `	ph7_class_instance *pThis;` |
|        - |  3165 | `	ph7_class_method *pCons;` |
|        - |  3166 | `	ph7_value sArg;` |
|        - |  3167 | `	ph7_value *apArg[1];` |
|        - |  3168 | `	SyBlob sMsg;` |
|        - |  3169 | `	SyString sMsgStr;` |
|        - |  3170 | `	VmFrame *pFrame;` |
|        - |  3171 | `	sxi32 rc;` |
|        5 |  3172 | `	pErrClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        5 |  3173 | `	if( pErrClass == 0 ){` |
|      ! 0 |  3174 | `		return PH7_ABORT;` |
|        - |  3175 | `	}` |
|        5 |  3176 | `	pThis = PH7_NewClassInstance(&(*pVm),pErrClass);` |
|        5 |  3177 | `	if( pThis == 0 ){` |
|      ! 0 |  3178 | `		return PH7_ABORT;` |
|        - |  3179 | `	}` |
|        5 |  3180 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  3181 | `	{` |
|        5 |  3182 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        5 |  3183 | `		const char *zKind = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? "static property" : "property";` |
|        5 |  3184 | `		SyBlobFormat(&sMsg,"Typed %s %z::$%z must not be accessed before initialization",` |
|        2 |  3185 | `			zKind,&pOwner->sName,&pAttr->sName);` |
|        - |  3186 | `	}` |
|        5 |  3187 | `	pCons = PH7_ClassExtractMethod(pErrClass,"__construct",sizeof("__construct")-1);` |
|        5 |  3188 | `	if( pCons ){` |
|        5 |  3189 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        5 |  3190 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|        5 |  3191 | `		apArg[0] = &sArg;` |
|        5 |  3192 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|        5 |  3193 | `		PH7_MemObjRelease(&sArg);` |
|        2 |  3194 | `	}` |
|        5 |  3195 | `	SyBlobRelease(&sMsg);` |
|        5 |  3196 | `	pFrame = pVm->pFrame;` |
|        5 |  3197 | `	if( pFrame ){` |
|        5 |  3198 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 |  3199 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        2 |  3200 | `	}` |
|        5 |  3201 | `	rc = VmThrowException(&(*pVm),pThis);` |
|        5 |  3202 | `	PH7_ClassInstanceUnref(pThis);` |
|        5 |  3203 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  3204 | `		return PH7_ABORT;` |
|        - |  3205 | `	}` |
|        5 |  3206 | `	return PH7_EXCEPTION;` |
|        3 |  3207 |  |
|        - |  3208 |  |
|        - |  3209 | `/*` |
|        - |  3210 | ` * Enforce a typed-property assignment. On entry pValue holds the incoming` |
|        - |  3211 | ` * value. For scalar types it may be coerced in place (PHP 7.4 weak mode).` |
|        - |  3212 | ` * For class types, instanceof is verified.` |
|        - |  3213 | ` *` |
|        - |  3214 | ` * Returns SXRET_OK on success (value may have been coerced), PH7_EXCEPTION` |
|        - |  3215 | ` * after throwing TypeError, or PH7_ABORT on fatal error.` |
|        - |  3216 | ` */` |
|        - |  3217 | `/*` |
|        - |  3218 | ` * PHP-strict numeric-string check used by typed-property enforcement.` |
|        - |  3219 | ` * Returns TRUE only if the entire string (optionally surrounded by` |
|        - |  3220 | ` * whitespace, with optional sign) is a valid numeric literal. Unlike the` |
|        - |  3221 | ` * permissive is_numeric() implementation which accepts leading-numeric` |
|        - |  3222 | ` * strings like "43x", this mirrors PHP's rules for coercing to int/float.` |
|        - |  3223 | ` */` |
|       22 |  3224 | `static int VmStringIsStrictNumeric(ph7_value *pValue)` |
|        2 |  3225 |  |
|        - |  3226 | `	const char *z, *zEnd, *zTail;` |
|        - |  3227 | `	sxu32 n;` |
|        - |  3228 | `	sxu8 bReal;` |
|        - |  3229 | `	sxi32 rc;` |
|       24 |  3230 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3231 | `		return 0;` |
|        - |  3232 | `	}` |
|       24 |  3233 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       24 |  3234 | `	n = SyBlobLength(&pValue->sBlob);` |
|       24 |  3235 | `	zEnd = z + n;` |
|       24 |  3236 | `	if( n == 0 ){` |
|      ! 0 |  3237 | `		return 0;` |
|        - |  3238 | `	}` |
|       24 |  3239 | `	zTail = 0;` |
|       24 |  3240 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       24 |  3241 | `	if( rc != SXRET_OK \|\| zTail == 0 ){` |
|        7 |  3242 | `		return 0;` |
|        - |  3243 | `	}` |
|        - |  3244 | `	/* Trailing whitespace is allowed by PHP, trailing anything else is not. */` |
|       18 |  3245 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ){` |
|      ! 0 |  3246 | `		zTail++;` |
|      ! 0 |  3247 | `	}` |
|       18 |  3248 | `	return zTail == zEnd ? 1 : 0;` |
|       13 |  3249 |  |
|        - |  3250 |  |
|        - |  3251 | `/*` |
|        - |  3252 | ` * Numeric-string classification used by union weak-mode coercion. Returns:` |
|        - |  3253 | ` *   1 if the string is a strictly-numeric integer (no fraction, no exponent)` |
|        - |  3254 | ` *   2 if it's strictly numeric with a fractional/exponent part (i.e. float)` |
|        - |  3255 | ` *   0 if it's not strictly numeric.` |
|        - |  3256 | ` */` |
|       16 |  3257 | `static int VmStringNumericKind(ph7_value *pValue)` |
|        2 |  3258 |  |
|        - |  3259 | `	const char *z, *zEnd, *zTail;` |
|        - |  3260 | `	sxu32 n;` |
|       18 |  3261 | `	sxu8 bReal = 0;` |
|        - |  3262 | `	sxi32 rc;` |
|       18 |  3263 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3264 | `		return 0;` |
|        - |  3265 | `	}` |
|       18 |  3266 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       18 |  3267 | `	n = SyBlobLength(&pValue->sBlob);` |
|       18 |  3268 | `	zEnd = z + n;` |
|       18 |  3269 | `	if( n == 0 ) return 0;` |
|       18 |  3270 | `	zTail = 0;` |
|       18 |  3271 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       18 |  3272 | `	if( rc != SXRET_OK \|\| zTail == 0 ) return 0;` |
|       19 |  3273 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ) zTail++;` |
|       15 |  3274 | `	if( zTail != zEnd ) return 0;` |
|       15 |  3275 | `	return bReal ? 2 : 1;` |
|       10 |  3276 |  |
|        - |  3277 |  |
|        - |  3278 | `/*` |
|        - |  3279 | ` * Check a value against a "pseudo-type" stored as an SXU32_HIGH class-name atom.` |
|        - |  3280 | `` * PH7 parses `true`/`false`/`iterable`/`mixed` as class-name atoms (they are not`` |
|        - |  3281 | ` * scalar keywords), so without this every enforcement site — return, parameter,` |
|        - |  3282 | ` * property, union alternative — would have to string-match the name itself.` |
|        - |  3283 | ` * Centralising it here keeps the four sites consistent and is the single place` |
|        - |  3284 | ` * to extend when another literal/pseudo type is added.` |
|        - |  3285 | ` *   returns  1 : recognised pseudo-type AND the value satisfies it` |
|        - |  3286 | ` *            0 : recognised pseudo-type AND the value does NOT satisfy it` |
|        - |  3287 | ` *           -1 : not a pseudo-type (caller should treat sClass as a real class)` |
|        - |  3288 | ` */` |
|      160 |  3289 | `static int VmCheckPseudoType(ph7_vm *pVm, ph7_value *pValue, const SyString *pClass)` |
|        2 |  3290 |  |
|      162 |  3291 | `	const char *z = pClass->zString;` |
|      162 |  3292 | `	sxu32 n = pClass->nByte;` |
|      162 |  3293 | `	if( n == 5 && SyStrnicmp(z,"mixed",5) == 0 ){` |
|       51 |  3294 | ``		return 1; /* `mixed` accepts any value, including null */`` |
|        - |  3295 | `	}` |
|      112 |  3296 | `	if( n == 4 && SyStrnicmp(z,"true",4) == 0 ){` |
|       15 |  3297 | `		return ( (pValue->iFlags & MEMOBJ_BOOL) && pValue->x.iVal != 0 ) ? 1 : 0;` |
|        - |  3298 | `	}` |
|       98 |  3299 | `	if( n == 5 && SyStrnicmp(z,"false",5) == 0 ){` |
|        3 |  3300 | `		return ( (pValue->iFlags & MEMOBJ_BOOL) && pValue->x.iVal == 0 ) ? 1 : 0;` |
|        - |  3301 | `	}` |
|       96 |  3302 | `	if( n == 8 && SyStrnicmp(z,"iterable",8) == 0 ){` |
|        - |  3303 | `		/* iterable === array \| Traversable */` |
|       17 |  3304 | `		if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        7 |  3305 | `			return 1;` |
|        - |  3306 | `		}` |
|       11 |  3307 | `		if( (pValue->iFlags & MEMOBJ_OBJ) && pVm->pTraversableClass ){` |
|        5 |  3308 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|        5 |  3309 | `			if( PH7_VmInstanceOf(pInst->pClass,pVm->pTraversableClass) ){` |
|        5 |  3310 | `				return 1;` |
|        - |  3311 | `			}` |
|      ! 0 |  3312 | `		}` |
|        7 |  3313 | `		return 0;` |
|        - |  3314 | `	}` |
|       80 |  3315 | `	return -1;` |
|       82 |  3316 |  |
|        - |  3317 | `/*` |
|        - |  3318 | ` * Try to coerce *pValue* to fit one of the alternatives in *pAlts*. When` |
|        - |  3319 | ` * *bStrict* is zero this applies PHP 8 weak-mode union semantics (permissive` |
|        - |  3320 | ` * scalar coercion). When bStrict is non-zero, only exact type matches are` |
|        - |  3321 | ` * accepted, plus the single implicit widening int -> float (so an int value` |
|        - |  3322 | `` * against a `float\|X` union succeeds; string -> int does not).`` |
|        - |  3323 | ` * Returns SXRET_OK on accept (pValue may have been mutated by the cast),` |
|        - |  3324 | ` * SXERR_INVALID on reject. Caller is responsible for the actual TypeError` |
|        - |  3325 | ` * throw.` |
|        - |  3326 | ` *` |
|        - |  3327 | ` * The class match for object values consults the active VM self-stack to` |
|        - |  3328 | `` * resolve `self`/`parent` aliases when present.`` |
|        - |  3329 | ` */` |
|      102 |  3330 | `static sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable, int bStrict)` |
|        2 |  3331 |  |
|        - |  3332 | `	sxu32 i;` |
|        - |  3333 | `	ph7_type_alt *aAlts;` |
|        - |  3334 | `	int bHasArray, bHasObjAlt, bHasClassAlt;` |
|        - |  3335 | `	int bHasInt, bHasFloat, bHasString, bHasBool;` |
|      104 |  3336 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       14 |  3337 | `		return bNullable ? SXRET_OK : SXERR_INVALID;` |
|        - |  3338 | `	}` |
|       92 |  3339 | `	aAlts = (ph7_type_alt *)SySetBasePtr(pAlts);` |
|        - |  3340 | ``	/* Pseudo-type alternatives (true/false/iterable; `mixed` never unions) are`` |
|        - |  3341 | `	 * stored as SXU32_HIGH name atoms and need value-checking, not instanceof.` |
|        - |  3342 | ``	 * A match on any one accepts the value (handles e.g. `true\|int`, `?true`,`` |
|        - |  3343 | ``	 * `iterable\|Foo`). */`` |
|      268 |  3344 | `	for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      178 |  3345 | `		if( aAlts[i].nType == SXU32_HIGH` |
|      105 |  3346 | `		 && VmCheckPseudoType(pVm, pValue, &aAlts[i].sClass) == 1 ){` |
|        3 |  3347 | `			return SXRET_OK;` |
|        - |  3348 | `		}` |
|       90 |  3349 | `	}` |
|       90 |  3350 | `	bHasArray = bHasObjAlt = bHasClassAlt = 0;` |
|       90 |  3351 | `	bHasInt = bHasFloat = bHasString = bHasBool = 0;` |
|      266 |  3352 | `	for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      178 |  3353 | `		if( aAlts[i].nType == SXU32_HIGH ) bHasClassAlt = 1;` |
|      152 |  3354 | `		else if( aAlts[i].nType == MEMOBJ_OBJ ) bHasObjAlt = 1;` |
|      152 |  3355 | `		else if( aAlts[i].nType == MEMOBJ_HASHMAP ) bHasArray = 1;` |
|      152 |  3356 | `		else if( aAlts[i].nType == MEMOBJ_INT ) bHasInt = 1;` |
|       76 |  3357 | `		else if( aAlts[i].nType == MEMOBJ_REAL ) bHasFloat = 1;` |
|       48 |  3358 | `		else if( aAlts[i].nType == MEMOBJ_STRING ) bHasString = 1;` |
|      ! 0 |  3359 | `		else if( aAlts[i].nType == MEMOBJ_BOOL ) bHasBool = 1;` |
|       90 |  3360 | `	}` |
|        - |  3361 | `	/* Object handling */` |
|       90 |  3362 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       18 |  3363 | `		if( bHasObjAlt ) return SXRET_OK;` |
|       18 |  3364 | `		if( bHasClassAlt ){` |
|       14 |  3365 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       14 |  3366 | `			ph7_class *pSelfNow = 0;` |
|       14 |  3367 | `			if( SySetUsed(&pVm->aSelf) > 0 ){` |
|      ! 0 |  3368 | `				ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|      ! 0 |  3369 | `				pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|      ! 0 |  3370 | `			}` |
|       26 |  3371 | `			for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        - |  3372 | `				ph7_class *pExpected;` |
|        - |  3373 | `				SyString *pCN;` |
|       22 |  3374 | `				if( aAlts[i].nType != SXU32_HIGH ) continue;` |
|       22 |  3375 | `				pCN = &aAlts[i].sClass;` |
|       22 |  3376 | `				if( pCN->nByte == 4 && SyMemcmp(pCN->zString,"self",4) == 0 ){` |
|      ! 0 |  3377 | `					pExpected = pSelfNow;` |
|       22 |  3378 | `				}else if( pCN->nByte == 6 && SyMemcmp(pCN->zString,"parent",6) == 0 ){` |
|      ! 0 |  3379 | `					pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3380 | `				}else{` |
|       22 |  3381 | `					pExpected = PH7_VmExtractClass(pVm,pCN->zString,pCN->nByte,TRUE,0);` |
|        - |  3382 | `				}` |
|       22 |  3383 | `				if( pExpected && PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        9 |  3384 | `					return SXRET_OK;` |
|        - |  3385 | `				}` |
|        8 |  3386 | `			}` |
|        2 |  3387 | `		}` |
|        9 |  3388 | `		return SXERR_INVALID;` |
|        - |  3389 | `	}` |
|        - |  3390 | `	/* Array handling */` |
|       74 |  3391 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        7 |  3392 | `		return bHasArray ? SXRET_OK : SXERR_INVALID;` |
|        - |  3393 | `	}` |
|        - |  3394 | `	/* Scalar handling — exact match first */` |
|       68 |  3395 | `	if( pValue->iFlags & MEMOBJ_INT ){` |
|       28 |  3396 | `		if( bHasInt ) return SXRET_OK;` |
|      ! 0 |  3397 | `	}` |
|       42 |  3398 | `	if( pValue->iFlags & MEMOBJ_REAL ){` |
|        5 |  3399 | `		if( bHasFloat ) return SXRET_OK;` |
|      ! 0 |  3400 | `	}` |
|       38 |  3401 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       38 |  3402 | `		if( bHasString ) return SXRET_OK;` |
|        8 |  3403 | `	}` |
|       18 |  3404 | `	if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  3405 | `		if( bHasBool ) return SXRET_OK;` |
|      ! 0 |  3406 | `	}` |
|       18 |  3407 | `	if( bStrict ){` |
|        - |  3408 | `		/* Strict mode: only int -> float widening is allowed implicitly. */` |
|      ! 0 |  3409 | `		if( (pValue->iFlags & MEMOBJ_INT) && bHasFloat ){` |
|      ! 0 |  3410 | `			PH7_MemObjToReal(pValue);` |
|      ! 0 |  3411 | `			return SXRET_OK;` |
|        - |  3412 | `		}` |
|      ! 0 |  3413 | `		return SXERR_INVALID;` |
|        - |  3414 | `	}` |
|        - |  3415 | `	/* Weak coercion preference order: int > float > string > bool.` |
|        - |  3416 | `	 * Numeric-string handling distinguishes integer-shaped from float-shaped` |
|        - |  3417 | `	 * to match PHP's union RFC. */` |
|        - |  3418 | `	{` |
|       18 |  3419 | `		int kind = VmStringNumericKind(pValue);` |
|       18 |  3420 | `		if( bHasInt ){` |
|        - |  3421 | `			/* int target accepts: bool, int (already exact), float w/o fraction,` |
|        - |  3422 | `			 * numeric-string-int. Float→int with fraction loses info → skip. */` |
|       18 |  3423 | `			if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  3424 | `				PH7_MemObjToInteger(pValue);` |
|      ! 0 |  3425 | `				return SXRET_OK;` |
|        - |  3426 | `			}` |
|       18 |  3427 | `			if( pValue->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3428 | `				ph7_real r = pValue->rVal;` |
|      ! 0 |  3429 | `				if( r == (ph7_real)(sxi64)r ){` |
|      ! 0 |  3430 | `					PH7_MemObjToInteger(pValue);` |
|      ! 0 |  3431 | `					return SXRET_OK;` |
|        - |  3432 | `				}` |
|      ! 0 |  3433 | `			}` |
|       18 |  3434 | `			if( kind == 1 ){` |
|        9 |  3435 | `				PH7_MemObjToInteger(pValue);` |
|        9 |  3436 | `				return SXRET_OK;` |
|        - |  3437 | `			}` |
|        4 |  3438 | `		}` |
|       10 |  3439 | `		if( bHasFloat ){` |
|       10 |  3440 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT) ){` |
|      ! 0 |  3441 | `				PH7_MemObjToReal(pValue);` |
|      ! 0 |  3442 | `				return SXRET_OK;` |
|        - |  3443 | `			}` |
|       10 |  3444 | `			if( kind == 1 \|\| kind == 2 ){` |
|        7 |  3445 | `				PH7_MemObjToReal(pValue);` |
|        7 |  3446 | `				return SXRET_OK;` |
|        - |  3447 | `			}` |
|        1 |  3448 | `		}` |
|        3 |  3449 | `		if( bHasString ){` |
|      ! 0 |  3450 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      ! 0 |  3451 | `				PH7_MemObjToString(pValue);` |
|      ! 0 |  3452 | `				return SXRET_OK;` |
|        - |  3453 | `			}` |
|      ! 0 |  3454 | `		}` |
|        3 |  3455 | `		if( bHasBool ){` |
|      ! 0 |  3456 | `			if( pValue->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING) ){` |
|      ! 0 |  3457 | `				PH7_MemObjToBool(pValue);` |
|      ! 0 |  3458 | `				return SXRET_OK;` |
|        - |  3459 | `			}` |
|      ! 0 |  3460 | `		}` |
|        - |  3461 | `	}` |
|        3 |  3462 | `	return SXERR_INVALID;` |
|       53 |  3463 |  |
|        - |  3464 |  |
|        - |  3465 | `/*` |
|        - |  3466 | ` * Enforce a scalar type hint on a single argument/return value under the` |
|        - |  3467 | ` * current strict-types mode. Pre: *pVal* does not already match *nType*,` |
|        - |  3468 | ` * and *nType* is a scalar MEMOBJ_* flag (not SXU32_HIGH, not MEMOBJ_OBJ).` |
|        - |  3469 | ` * Returns SXRET_OK after coercion/widening, or SXERR_INVALID if strict` |
|        - |  3470 | ` * mode rejects the value. Callers throw the TypeError on rejection.` |
|        - |  3471 | ` */` |
|       38 |  3472 | `static sxi32 VmEnforceScalarType(ph7_value *pVal, sxu32 nType, int bStrict)` |
|        2 |  3473 |  |
|        - |  3474 | ``	/* A standalone `null` type is not a weak-coercion target: only an actual`` |
|        - |  3475 | `	 * null value satisfies it (and a null value matches via the flag test` |
|        - |  3476 | `	 * before this is ever called, so pVal is non-null here). Reject rather than` |
|        - |  3477 | ``	 * casting the value to null — otherwise a `null`-typed parameter would`` |
|        - |  3478 | `	 * silently swallow any argument. */` |
|       40 |  3479 | `	if( nType == MEMOBJ_NULL ){` |
|        3 |  3480 | `		return SXERR_INVALID;` |
|        - |  3481 | `	}` |
|       38 |  3482 | `	if( bStrict ){` |
|        - |  3483 | `		/* Only int -> float widening is allowed implicitly. */` |
|       12 |  3484 | `		if( nType == MEMOBJ_REAL && (pVal->iFlags & MEMOBJ_INT) ){` |
|        3 |  3485 | `			PH7_MemObjToReal(pVal);` |
|        3 |  3486 | `			return SXRET_OK;` |
|        - |  3487 | `		}` |
|       10 |  3488 | `		return SXERR_INVALID;` |
|        - |  3489 | `	}` |
|        - |  3490 | `	{` |
|       28 |  3491 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(nType);` |
|       28 |  3492 | `		if( xCast ) xCast(pVal);` |
|        - |  3493 | `	}` |
|       28 |  3494 | `	return SXRET_OK;` |
|       21 |  3495 |  |
|        - |  3496 |  |
|        - |  3497 | `/*` |
|        - |  3498 | ` * Render a scalar-type name suitable for the "Argument ... must be of type X"` |
|        - |  3499 | ` * TypeError message. Prefers the declared textual form when available.` |
|        - |  3500 | ` *` |
|        - |  3501 | ` * The declared SyString is length-delimited, not necessarily NUL-terminated,` |
|        - |  3502 | ` * so we bounded-copy it into the caller's *zBuf* before returning it as a` |
|        - |  3503 | ` * C string safe for "%s" formatting. If no declared text is present we fall` |
|        - |  3504 | ` * back to a static literal and ignore zBuf entirely.` |
|        - |  3505 | ` */` |
|       12 |  3506 | `static const char *VmScalarTypeName(sxu32 nType, SyString *pDeclared, char *zBuf, sxu32 nBuf)` |
|        2 |  3507 |  |
|       14 |  3508 | `	if( pDeclared && SyStringLength(pDeclared) > 0 && zBuf && nBuf > 0 ){` |
|       14 |  3509 | `		sxu32 nCopy = SyStringLength(pDeclared);` |
|       14 |  3510 | `		if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|       14 |  3511 | `		if( pDeclared->zString && nCopy > 0 ){` |
|       14 |  3512 | `			SyMemcpy(pDeclared->zString, zBuf, nCopy);` |
|        6 |  3513 | `		}` |
|       14 |  3514 | `		zBuf[nCopy] = 0;` |
|       14 |  3515 | `		return zBuf;` |
|        - |  3516 | `	}` |
|      ! 0 |  3517 | `	switch( nType ){` |
|      ! 0 |  3518 | `		case MEMOBJ_INT:     return "int";` |
|      ! 0 |  3519 | `		case MEMOBJ_REAL:    return "float";` |
|      ! 0 |  3520 | `		case MEMOBJ_STRING:  return "string";` |
|      ! 0 |  3521 | `		case MEMOBJ_BOOL:    return "bool";` |
|      ! 0 |  3522 | `		case MEMOBJ_HASHMAP: return "array";` |
|      ! 0 |  3523 | `		case MEMOBJ_OBJ:     return "object";` |
|      ! 0 |  3524 | `		default:             return "scalar";` |
|        - |  3525 | `	}` |
|        8 |  3526 |  |
|        - |  3527 |  |
|        - |  3528 | `/*` |
|        - |  3529 | ` * Format the class name of an object-typed ph7_value into a small caller` |
|        - |  3530 | ` * buffer, for use in TypeError messages. Returns the buffer pointer.` |
|        - |  3531 | ` */` |
|       18 |  3532 | `static const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf)` |
|        1 |  3533 |  |
|       19 |  3534 | `	ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       28 |  3535 | `	SyBufferFormat(zBuf,nBuf,"%.*s",` |
|       18 |  3536 | `		(int)pInst->pClass->sName.nByte,pInst->pClass->sName.zString);` |
|       19 |  3537 | `	return zBuf;` |
|        1 |  3538 |  |
|        - |  3539 |  |
|     6544 |  3540 | `static sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|        2 |  3541 |  |
|        - |  3542 | `	SyHashEntry *pSlot;` |
|        - |  3543 | `	VmClassAttr *pVmAttr;` |
|        - |  3544 | `	ph7_class_attr *pAttr;` |
|     6546 |  3545 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|     6546 |  3546 | `	if( pSlot == 0 ){` |
|     6320 |  3547 | `		return SXRET_OK; /* Not a typed slot */` |
|        - |  3548 | `	}` |
|      228 |  3549 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      228 |  3550 | `	pAttr = pVmAttr->pAttr;` |
|      228 |  3551 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 |  3552 | `		return SXRET_OK;` |
|        - |  3553 | `	}` |
|        - |  3554 | `	/* Union type: dispatch to the shared coercion helper. Typed properties` |
|        - |  3555 | `	 * are always evaluated in weak mode regardless of declare(strict_types),` |
|        - |  3556 | `	 * matching PHP's documented behavior. */` |
|      228 |  3557 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       23 |  3558 | `		sxi32 rc = VmCoerceToUnion(pVm, pValue, &pAttr->aUnionAlts,` |
|       14 |  3559 | `			(pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0,` |
|        - |  3560 |  |
|       16 |  3561 | `		if( rc == SXRET_OK ){` |
|        9 |  3562 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        9 |  3563 | `			return SXRET_OK;` |
|        - |  3564 | `		}` |
|        7 |  3565 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  3566 | `			char zBuf[128];` |
|        4 |  3567 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        1 |  3568 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3569 | `		}` |
|        5 |  3570 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3571 | `	}` |
|        - |  3572 | ``	/* NULL handling: allowed if the type is nullable, or is `mixed` (which`` |
|        - |  3573 | `	 * includes null). */` |
|      214 |  3574 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       15 |  3575 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE)` |
|       11 |  3576 | `		 \|\| (pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|        2 |  3577 | `		     && SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0) ){` |
|       14 |  3578 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       14 |  3579 | `			return SXRET_OK;` |
|        - |  3580 | `		}` |
|        3 |  3581 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,"null");` |
|        - |  3582 | `	}` |
|        - |  3583 | ``	/* standalone `null` property type (PHP 8.2): a null value was already`` |
|        - |  3584 | `	 * accepted by the nullable check above, so any non-null value here is a` |
|        - |  3585 | `	 * type error. */` |
|      200 |  3586 | `	if( pAttr->nType == MEMOBJ_NULL ){` |
|      ! 0 |  3587 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3588 | `	}` |
|        - |  3589 | `	/* Bare 'object' type hint: accept any class instance, reject non-objects.` |
|        - |  3590 | `	 * Must be checked before the generic scalar branch since MEMOBJ_OBJ is` |
|        - |  3591 | `	 * otherwise treated as "scalar, not array" and would be rejected. */` |
|      200 |  3592 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|       12 |  3593 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        5 |  3594 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        5 |  3595 | `			return SXRET_OK;` |
|        - |  3596 | `		}` |
|        7 |  3597 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3598 | `	}` |
|        - |  3599 | ``	/* Pseudo-types stored as class-name atoms: `iterable` (array\|Traversable),`` |
|        - |  3600 | ``	 * `true`/`false` (matching bool), `mixed` (any value — its null case is`` |
|        - |  3601 | `	 * handled by the nullable check above). Checked by value before the generic` |
|        - |  3602 | `	 * class-instanceof branch, which would resolve no such class and then` |
|        - |  3603 | `	 * wrongly accept any object / reject arrays. */` |
|      190 |  3604 | `	if( pAttr->nType == SXU32_HIGH ){` |
|       38 |  3605 | `		int rcPseudo = VmCheckPseudoType(pVm, pValue, &pAttr->sClass);` |
|       38 |  3606 | `		if( rcPseudo == 1 ){` |
|       11 |  3607 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       11 |  3608 | `			return SXRET_OK;` |
|        - |  3609 | `		}` |
|       28 |  3610 | `		if( rcPseudo == 0 ){` |
|        3 |  3611 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3612 | `		}` |
|        - |  3613 | `		/* rcPseudo == -1: real class — fall through to the instanceof branch. */` |
|       12 |  3614 | `	}` |
|      178 |  3615 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        - |  3616 | `		/* Class / interface type. Resolve self/parent relative to the class` |
|        - |  3617 | `		 * currently active on the self-stack. */` |
|       26 |  3618 | `		ph7_class *pExpected = 0;` |
|       26 |  3619 | `		SyString *pClassName = &pAttr->sClass;` |
|       26 |  3620 | `		ph7_class *pSelfNow = 0;` |
|       26 |  3621 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|        3 |  3622 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|        3 |  3623 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        1 |  3624 | `		}` |
|       26 |  3625 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        5 |  3626 | `			pExpected = pSelfNow;` |
|       24 |  3627 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  3628 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3629 | `		}else{` |
|       22 |  3630 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  3631 | `		}` |
|       26 |  3632 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  3633 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3634 | `		}` |
|       26 |  3635 | `		if( pExpected ){` |
|       22 |  3636 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       22 |  3637 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        - |  3638 | `				char zBuf[128];` |
|        7 |  3639 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        2 |  3640 | `					VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3641 | `			}` |
|        8 |  3642 | `		}` |
|       22 |  3643 | `		pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       22 |  3644 | `		return SXRET_OK;` |
|        - |  3645 | `	}` |
|        - |  3646 | `	/* Scalar type. PHP 7.4 weak mode: attempt coercion using the same cast` |
|        - |  3647 | `	 * helpers used by function-argument hints. Reject object→scalar. */` |
|      154 |  3648 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  3649 | `		char zBuf[128];` |
|       10 |  3650 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        3 |  3651 | `			VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3652 | `	}` |
|      148 |  3653 | `	if( (pValue->iFlags & pAttr->nType) == 0 ){` |
|       28 |  3654 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(pAttr->nType);` |
|       28 |  3655 | `		if( xCast ){` |
|        - |  3656 | `			/* Reject array<->scalar coercion to match PHP strictness */` |
|       28 |  3657 | `			if( pAttr->nType == MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  3658 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3659 | `			}` |
|       26 |  3660 | `			if( pAttr->nType != MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|        5 |  3661 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3662 | `			}` |
|        - |  3663 | `			/* PHP weak mode: reject string->int/float unless the string is` |
|        - |  3664 | `			 * strictly numeric. Silent coercion of "abc" or "43x" to 0/43` |
|        - |  3665 | `			 * would hide bugs and diverges from PHP's TypeError. */` |
|       29 |  3666 | `			if( (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL)` |
|       19 |  3667 | `			 && (pValue->iFlags & MEMOBJ_STRING)` |
|       21 |  3668 | `			 && !VmStringIsStrictNumeric(pValue) ){` |
|       12 |  3669 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,"string");` |
|        - |  3670 | `			}` |
|       12 |  3671 | `			xCast(pValue);` |
|        5 |  3672 | `		}` |
|        5 |  3673 | `	}` |
|      132 |  3674 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      132 |  3675 | `	return SXRET_OK;` |
|     3274 |  3676 |  |
|        - |  3677 |  |
|        - |  3678 | `/*` |
|        - |  3679 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3680 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3681 | ` * information.` |
|        - |  3682 | ` * ------------------------------------` |
|        - |  3683 | ` * Simple boring wrapper function.` |
|        - |  3684 | ` * ------------------------------------` |
|        - |  3685 | ` */` |
|       20 |  3686 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  3687 |  |
|        - |  3688 | `	va_list ap;` |
|        - |  3689 | `	sxi32 rc;` |
|       21 |  3690 | `	va_start(ap,zFormat);` |
|       21 |  3691 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       21 |  3692 | `	va_end(ap);` |
|       21 |  3693 | `	return rc;` |
|        1 |  3694 |  |
|        - |  3695 | `/*` |
|        - |  3696 | ` * Throw a TypeError exception from within the VM execution loop.` |
|        - |  3697 | ` * Used for user-defined function type hint violations (e.g. object type hint).` |
|        - |  3698 | ` */` |
|       42 |  3699 | `static sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,SyString *pFuncName,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)` |
|        2 |  3700 |  |
|        - |  3701 | `	ph7_class *pClass;` |
|        - |  3702 | `	ph7_class_instance *pThis;` |
|        - |  3703 | `	ph7_class_method *pCons;` |
|        - |  3704 | `	ph7_value sArg;` |
|        - |  3705 | `	ph7_value *apArg[1];` |
|        - |  3706 | `	SyBlob sMsg;` |
|        - |  3707 | `	SyString sMsgStr;` |
|        - |  3708 | `	VmFrame *pFrame;` |
|        - |  3709 | `	sxi32 rc;` |
|       44 |  3710 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       44 |  3711 | `	if( pClass == 0 ){` |
|      ! 0 |  3712 | `		return PH7_ABORT;` |
|        - |  3713 | `	}` |
|       44 |  3714 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       44 |  3715 | `	if( pThis == 0 ){` |
|      ! 0 |  3716 | `		return PH7_ABORT;` |
|        - |  3717 | `	}` |
|       44 |  3718 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       44 |  3719 | `	SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) must be of type %s, %s given",` |
|       21 |  3720 | `		pFuncName,nArg,pArgName,zExpected,zGiven);` |
|       44 |  3721 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       44 |  3722 | `	if( pCons ){` |
|       44 |  3723 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       44 |  3724 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       44 |  3725 | `		apArg[0] = &sArg;` |
|       44 |  3726 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       44 |  3727 | `		PH7_MemObjRelease(&sArg);` |
|       21 |  3728 | `	}` |
|       44 |  3729 | `	SyBlobRelease(&sMsg);` |
|       44 |  3730 | `	pFrame = pVm->pFrame;` |
|       44 |  3731 | `	if( pFrame ){` |
|       44 |  3732 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       44 |  3733 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       21 |  3734 | `	}` |
|       44 |  3735 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       44 |  3736 | `	PH7_ClassInstanceUnref(pThis);` |
|       44 |  3737 | `	if( rc == SXERR_ABORT ){` |
|        5 |  3738 | `		return PH7_ABORT;` |
|        - |  3739 | `	}` |
|       40 |  3740 | `	return PH7_EXCEPTION;` |
|       23 |  3741 |  |
|        - |  3742 | `/*` |
|        - |  3743 | ` * Throw a PHP-compatible TypeError describing a return-value type mismatch.` |
|        - |  3744 | ` * Message format: "funcname(): Return value must be of type X, Y returned".` |
|        - |  3745 | ` */` |
|       12 |  3746 | `static sxi32 VmThrowTypeErrorForReturn(ph7_vm *pVm,SyString *pFuncName,const char *zExpected,const char *zGiven)` |
|        2 |  3747 |  |
|        - |  3748 | `	ph7_class *pClass;` |
|        - |  3749 | `	ph7_class_instance *pThis;` |
|        - |  3750 | `	ph7_class_method *pCons;` |
|        - |  3751 | `	ph7_value sArg;` |
|        - |  3752 | `	ph7_value *apArg[1];` |
|        - |  3753 | `	SyBlob sMsg;` |
|        - |  3754 | `	SyString sMsgStr;` |
|        - |  3755 | `	VmFrame *pFrame;` |
|        - |  3756 | `	sxi32 rc;` |
|       14 |  3757 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       14 |  3758 | `	if( pClass == 0 ){` |
|      ! 0 |  3759 | `		return PH7_ABORT;` |
|        - |  3760 | `	}` |
|       14 |  3761 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       14 |  3762 | `	if( pThis == 0 ){` |
|      ! 0 |  3763 | `		return PH7_ABORT;` |
|        - |  3764 | `	}` |
|       14 |  3765 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       14 |  3766 | `	SyBlobFormat(&sMsg,"%z(): Return value must be of type %s, %s returned",` |
|        6 |  3767 | `		pFuncName,zExpected,zGiven);` |
|       14 |  3768 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       14 |  3769 | `	if( pCons ){` |
|       14 |  3770 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       14 |  3771 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       14 |  3772 | `		apArg[0] = &sArg;` |
|       14 |  3773 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       14 |  3774 | `		PH7_MemObjRelease(&sArg);` |
|        6 |  3775 | `	}` |
|       14 |  3776 | `	SyBlobRelease(&sMsg);` |
|       14 |  3777 | `	pFrame = pVm->pFrame;` |
|       14 |  3778 | `	if( pFrame ){` |
|       14 |  3779 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       14 |  3780 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        6 |  3781 | `	}` |
|       14 |  3782 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       14 |  3783 | `	PH7_ClassInstanceUnref(pThis);` |
|       14 |  3784 | `	if( rc == SXERR_ABORT ){` |
|        7 |  3785 | `		return PH7_ABORT;` |
|        - |  3786 | `	}` |
|        7 |  3787 | `	return PH7_EXCEPTION;` |
|        8 |  3788 |  |
|        - |  3789 | `/*` |
|        - |  3790 | ` * Format the "X given" portion of error messages following PHP's value-name` |
|        - |  3791 | ` * convention: "true"/"false" for booleans, class name for objects, otherwise` |
|        - |  3792 | ` * the bare type name. zBuf must hold at least 64 bytes.` |
|        - |  3793 | ` */` |
|       28 |  3794 | `static const char * VmValueGivenName(ph7_value *pVal,char *zBuf,sxu32 nBuf)` |
|        2 |  3795 |  |
|       30 |  3796 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|       10 |  3797 | `		return pVal->x.iVal ? "true" : "false";` |
|        - |  3798 | `	}` |
|       22 |  3799 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|        8 |  3800 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|        8 |  3801 | `		if( pThis && pThis->pClass ){` |
|        8 |  3802 | `			SyString *pName = &pThis->pClass->sName;` |
|        8 |  3803 | `			sxu32 n = pName->nByte;` |
|        8 |  3804 | `			if( n >= nBuf ){` |
|      ! 0 |  3805 | `				n = nBuf - 1;` |
|      ! 0 |  3806 | `			}` |
|        8 |  3807 | `			SyMemcpy(pName->zString,zBuf,n);` |
|        8 |  3808 | `			zBuf[n] = 0;` |
|        8 |  3809 | `			return zBuf;` |
|        - |  3810 | `		}` |
|      ! 0 |  3811 | `		return "object";` |
|        - |  3812 | `	}` |
|       16 |  3813 | `	return ph7_type_name(pVal);` |
|       16 |  3814 |  |
|        - |  3815 | `/*` |
|        - |  3816 | ` * Throw a PHP-compatible Error when array unpacking ('...$expr') receives a` |
|        - |  3817 | ` * non-array value at runtime. Matches the message and class PHP raises` |
|        - |  3818 | ` * ("Only arrays and Traversables can be unpacked, X given"). The class is` |
|        - |  3819 | ` * \TypeError for objects, \Error otherwise — matching PHP's distinction.` |
|        - |  3820 | ` */` |
|       18 |  3821 | `static sxi32 VmThrowSpreadError(ph7_vm *pVm,ph7_value *pBad)` |
|        2 |  3822 |  |
|        - |  3823 | `	ph7_class *pClass;` |
|        - |  3824 | `	ph7_class_instance *pThis;` |
|        - |  3825 | `	ph7_class_method *pCons;` |
|        - |  3826 | `	ph7_value sArg;` |
|        - |  3827 | `	ph7_value *apArg[1];` |
|        - |  3828 | `	SyBlob sMsg;` |
|        - |  3829 | `	SyString sMsgStr;` |
|        - |  3830 | `	VmFrame *pFrame;` |
|        - |  3831 | `	sxi32 rc;` |
|       20 |  3832 | `	const char *zErrClass = (pBad->iFlags & MEMOBJ_OBJ) ? "TypeError" : "Error";` |
|        - |  3833 | `	char zNameBuf[64];` |
|       20 |  3834 | `	const char *zGiven = VmValueGivenName(pBad,zNameBuf,sizeof(zNameBuf));` |
|       20 |  3835 | `	pClass = PH7_VmExtractClass(&(*pVm),zErrClass,SyStrlen(zErrClass),TRUE,0);` |
|       20 |  3836 | `	if( pClass == 0 ){` |
|      ! 0 |  3837 | `		return PH7_ABORT;` |
|        - |  3838 | `	}` |
|       20 |  3839 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       20 |  3840 | `	if( pThis == 0 ){` |
|      ! 0 |  3841 | `		return PH7_ABORT;` |
|        - |  3842 | `	}` |
|       20 |  3843 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       20 |  3844 | `	SyBlobFormat(&sMsg,"Only arrays and Traversables can be unpacked, %s given",zGiven);` |
|       20 |  3845 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       20 |  3846 | `	if( pCons ){` |
|       20 |  3847 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       20 |  3848 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       20 |  3849 | `		apArg[0] = &sArg;` |
|       20 |  3850 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       20 |  3851 | `		PH7_MemObjRelease(&sArg);` |
|        9 |  3852 | `	}` |
|       20 |  3853 | `	SyBlobRelease(&sMsg);` |
|       20 |  3854 | `	pFrame = pVm->pFrame;` |
|       20 |  3855 | `	if( pFrame ){` |
|       20 |  3856 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       20 |  3857 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        9 |  3858 | `	}` |
|       20 |  3859 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       20 |  3860 | `	PH7_ClassInstanceUnref(pThis);` |
|       20 |  3861 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  3862 | `		return PH7_ABORT;` |
|        - |  3863 | `	}` |
|       20 |  3864 | `	return PH7_EXCEPTION;` |
|       11 |  3865 |  |
|        - |  3866 | `/*` |
|        - |  3867 | ` * Enforce the declared return type of *pFunc* against the value returned` |
|        - |  3868 | ` * (or NULL if the function returned without a value). Mutates *pValue* to` |
|        - |  3869 | ` * perform allowed widening (int->float) or weak-mode coercion. On` |
|        - |  3870 | ` * violation, throws TypeError and returns PH7_EXCEPTION.` |
|        - |  3871 | ` */` |
|        - |  3872 | `/*` |
|        - |  3873 | ` * Bounded-copy *pStr* into *zBuf* (NUL-terminated, max nBuf-1 bytes). The` |
|        - |  3874 | ` * caller's buffer is then safe to pass through "%s" formatters. An empty or` |
|        - |  3875 | ` * null SyString yields an empty C string. Returns zBuf.` |
|        - |  3876 | ` */` |
|       34 |  3877 | `static const char *VmSyStringToCStr(const SyString *pStr, char *zBuf, sxu32 nBuf)` |
|        2 |  3878 |  |
|        - |  3879 | `	sxu32 nCopy;` |
|       36 |  3880 | `	if( nBuf == 0 ) return "";` |
|       36 |  3881 | `	if( pStr == 0 \|\| pStr->zString == 0 ){` |
|      ! 0 |  3882 | `		zBuf[0] = 0;` |
|      ! 0 |  3883 | `		return zBuf;` |
|        - |  3884 | `	}` |
|       36 |  3885 | `	nCopy = SyStringLength(pStr);` |
|       36 |  3886 | `	if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|       36 |  3887 | `	if( nCopy > 0 ) SyMemcpy(pStr->zString, zBuf, nCopy);` |
|       36 |  3888 | `	zBuf[nCopy] = 0;` |
|       36 |  3889 | `	return zBuf;` |
|       19 |  3890 |  |
|        - |  3891 |  |
|      474 |  3892 | `static sxi32 VmEnforceReturnType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue)` |
|        2 |  3893 |  |
|      476 |  3894 | `	int bStrict = pFunc->bStrictTypes ? 1 : 0;` |
|        - |  3895 | `	const char *zGiven;` |
|        - |  3896 | `	char zBuf[128];` |
|        - |  3897 | `	char zTypeBuf[128];` |
|        - |  3898 | `	/* Untyped function: no enforcement. */` |
|      476 |  3899 | `	if( pFunc->nReturnType == 0 ){` |
|      ! 0 |  3900 | `		return SXRET_OK;` |
|        - |  3901 | `	}` |
|        - |  3902 | `	/* void return type: the function must not produce a value. */` |
|      476 |  3903 | `	if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|      154 |  3904 | `		if( pValue == 0 ){` |
|      152 |  3905 | `			return SXRET_OK;` |
|        - |  3906 | `		}` |
|        - |  3907 | ``		/* PHP allows `return;` but rejects `return null;` — iP1=1 with NULL`` |
|        - |  3908 | `		 * still counts as "returned a value" here. */` |
|        3 |  3909 | `		zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|        3 |  3910 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,"void",zGiven);` |
|        - |  3911 | `	}` |
|        - |  3912 | `	/* Function fell off the end without an explicit return: PHP implicitly` |
|        - |  3913 | ``	 * returns null. For a typed non-nullable return (including `mixed`, which`` |
|        - |  3914 | `	 * requires an explicit returned value), that's a TypeError. */` |
|      324 |  3915 | `	if( pValue == 0 ){` |
|      ! 0 |  3916 | `		const char *zExpected = "value";` |
|      ! 0 |  3917 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      ! 0 |  3918 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  3919 | `		}` |
|      ! 0 |  3920 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,"null");` |
|        - |  3921 | `	}` |
|        - |  3922 | ``	/* standalone `null` return type (PHP 8.2): an explicit non-null return is a`` |
|        - |  3923 | `	 * TypeError. (Falling off the end is handled by the generic check above,` |
|        - |  3924 | `	 * matching how every other typed return reports a missing value.) */` |
|      324 |  3925 | `	if( pFunc->nReturnType == MEMOBJ_NULL ){` |
|        5 |  3926 | `		if( pValue->iFlags & MEMOBJ_NULL ){` |
|        3 |  3927 | `			return SXRET_OK;` |
|        - |  3928 | `		}` |
|        4 |  3929 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,"null",` |
|        1 |  3930 | `			VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3931 | `	}` |
|        - |  3932 | ``	/* Pseudo-types parsed as class-name atoms: `mixed` (any value),`` |
|        - |  3933 | ``	 * `true`/`false` (the matching bool literal), `iterable` (array\|Traversable).`` |
|        - |  3934 | `	 * Check by value before the real-class instanceof branch below. */` |
|      320 |  3935 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|       64 |  3936 | `		int rcPseudo = VmCheckPseudoType(pVm, pValue, &pFunc->sReturnClass);` |
|       64 |  3937 | `		if( rcPseudo == 1 ){` |
|       53 |  3938 | `			return SXRET_OK;` |
|        - |  3939 | `		}` |
|       12 |  3940 | `		if( rcPseudo == 0 ){` |
|        9 |  3941 | `			return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        4 |  3942 | `				VmSyStringToCStr(&pFunc->sReturnClass,zTypeBuf,sizeof(zTypeBuf)),` |
|        2 |  3943 | `				VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3944 | `		}` |
|        - |  3945 | `		/* rcPseudo == -1: a real class — fall through to the instanceof branch. */` |
|        3 |  3946 | `	}` |
|        - |  3947 | `	/* Union return type — delegate. The function has no flag for nullable` |
|        - |  3948 | `	 * unions; a null alternative is represented inside aReturnUnion, so pass` |
|        - |  3949 | `	 * bNullable=0 here. */` |
|      264 |  3950 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|        - |  3951 | `		sxi32 rcU;` |
|      ! 0 |  3952 | `		int bNullable = 0;` |
|      ! 0 |  3953 | `		const char *zExpected = "union";` |
|        - |  3954 | ``		/* Scan alternatives for MEMOBJ_NULL, which serves as `T\|null`. */`` |
|        - |  3955 | `		{` |
|        - |  3956 | `			sxu32 i;` |
|      ! 0 |  3957 | `			ph7_type_alt *aAlts = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|      ! 0 |  3958 | `			for( i = 0; i < SySetUsed(&pFunc->aReturnUnion); i++ ){` |
|      ! 0 |  3959 | `				if( aAlts[i].nType == MEMOBJ_NULL ){ bNullable = 1; break; }` |
|      ! 0 |  3960 | `			}` |
|        - |  3961 | `		}` |
|      ! 0 |  3962 | `		rcU = VmCoerceToUnion(pVm, pValue, &pFunc->aReturnUnion, bNullable, bStrict);` |
|      ! 0 |  3963 | `		if( rcU == SXRET_OK ){` |
|      ! 0 |  3964 | `			return SXRET_OK;` |
|        - |  3965 | `		}` |
|      ! 0 |  3966 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  3967 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3968 | `		}else if( pValue->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  3969 | `			zGiven = "null";` |
|      ! 0 |  3970 | `		}else{` |
|      ! 0 |  3971 | `			zGiven = ph7_type_name(pValue);` |
|        - |  3972 | `		}` |
|      ! 0 |  3973 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      ! 0 |  3974 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  3975 | `		}` |
|      ! 0 |  3976 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3977 | `	}` |
|        - |  3978 | `	/* Class return type — instanceof check. The class name is a length-` |
|        - |  3979 | `	 * delimited SyString; copy it into a local buffer before formatting` |
|        - |  3980 | `	 * it into the TypeError message. */` |
|      264 |  3981 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|        8 |  3982 | `		SyString *pClassName = &pFunc->sReturnClass;` |
|        - |  3983 | `		const char *zExpected;` |
|        - |  3984 | `		ph7_class *pExpected;` |
|        8 |  3985 | `		ph7_class *pSelfNow = 0;` |
|        8 |  3986 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|        8 |  3987 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|        8 |  3988 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        3 |  3989 | `		}` |
|        8 |  3990 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        3 |  3991 | `			pExpected = pSelfNow;` |
|        6 |  3992 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  3993 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3994 | `		}else{` |
|        5 |  3995 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  3996 | `		}` |
|        8 |  3997 | `		zExpected = VmSyStringToCStr(pClassName, zTypeBuf, sizeof(zTypeBuf));` |
|        8 |  3998 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  3999 | `			zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|      ! 0 |  4000 | `			return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  4001 | `		}` |
|        8 |  4002 | `		if( pExpected ){` |
|        6 |  4003 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|        6 |  4004 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|      ! 0 |  4005 | `				zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  4006 | `				return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  4007 | `			}` |
|        2 |  4008 | `		}` |
|        8 |  4009 | `		return SXRET_OK;` |
|        - |  4010 | `	}` |
|        - |  4011 | `	/* Scalar return type. Allow null pass-through if the function is` |
|        - |  4012 | `	 * nullable (textual "?T" gets that flag, though union+null is handled` |
|        - |  4013 | `	 * above). There's no explicit nullable flag on ph7_vm_func, so detect` |
|        - |  4014 | `	 * via the type-text leading '?'. */` |
|      258 |  4015 | `	if( (pValue->iFlags & MEMOBJ_NULL) ){` |
|        6 |  4016 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0` |
|        8 |  4017 | `		 && pFunc->sReturnTypeName.zString[0] == '?' ){` |
|        8 |  4018 | `			return SXRET_OK;` |
|        - |  4019 | `		}` |
|      ! 0 |  4020 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  4021 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - |  4022 | `			"null");` |
|        - |  4023 | `	}` |
|        - |  4024 | `	/* Exact match? Done. */` |
|      252 |  4025 | `	if( pValue->iFlags & pFunc->nReturnType ){` |
|      246 |  4026 | `		return SXRET_OK;` |
|        - |  4027 | `	}` |
|        - |  4028 | `	/* Object->scalar is never compatible. */` |
|        8 |  4029 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  4030 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  4031 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  4032 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  4033 | `			zGiven);` |
|        - |  4034 | `	}` |
|        - |  4035 | `	/* Array <-> scalar is never compatible. */` |
|        8 |  4036 | `	if( ((sxu32)(pValue->iFlags) & MEMOBJ_HASHMAP) != (pFunc->nReturnType & MEMOBJ_HASHMAP) ){` |
|      ! 0 |  4037 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  4038 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  4039 | `			ph7_type_name(pValue));` |
|        - |  4040 | `	}` |
|        - |  4041 | `	/* PHP's weak-mode rule: string -> int/float is allowed only if the` |
|        - |  4042 | `	 * string is strictly numeric. Silently coercing "abc"/"43x" to 0/43` |
|        - |  4043 | `	 * would hide the bug and diverges from PHP. Strict mode falls through` |
|        - |  4044 | `	 * to VmEnforceScalarType below which rejects string->int outright. */` |
|        8 |  4045 | `	if( !bStrict` |
|        5 |  4046 | `	 && (pFunc->nReturnType == MEMOBJ_INT \|\| pFunc->nReturnType == MEMOBJ_REAL)` |
|        4 |  4047 | `	 && (pValue->iFlags & MEMOBJ_STRING)` |
|        6 |  4048 | `	 && !VmStringIsStrictNumeric(pValue) ){` |
|        4 |  4049 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        1 |  4050 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - |  4051 | `			"string");` |
|        - |  4052 | `	}` |
|        6 |  4053 | `	if( VmEnforceScalarType(pValue, pFunc->nReturnType, bStrict) == SXRET_OK ){` |
|        3 |  4054 | `		return SXRET_OK;` |
|        - |  4055 | `	}` |
|        4 |  4056 | `	return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        1 |  4057 | `		VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        1 |  4058 | `		ph7_type_name(pValue));` |
|      239 |  4059 |  |
|        - |  4060 | `/*` |
|        - |  4061 | ` * Report a fatal named-argument error.` |
|        - |  4062 | ` * Outputs a PHP-compatible "Uncaught Error:" message and aborts execution.` |
|        - |  4063 | ` */` |
|        6 |  4064 | `static sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
|        1 |  4065 |  |
|        7 |  4066 | `	const char *zFunc = 0;` |
|        7 |  4067 | `	int nFunc = 0;` |
|        7 |  4068 | `	VmGetFrameContext(pVm,&zFunc,&nFunc);` |
|        7 |  4069 | `	return VmReportUncaughtException(pVm,"Error",5,zMsg,nMsg,zFunc,nFunc);` |
|        1 |  4070 |  |
|        - |  4071 | `/*` |
|        - |  4072 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  4073 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  4074 | ` * information.` |
|        - |  4075 | ` * ------------------------------------` |
|        - |  4076 | ` * Simple boring wrapper function.` |
|        - |  4077 | ` * ------------------------------------` |
|        - |  4078 | ` */` |
|       24 |  4079 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  4080 |  |
|        - |  4081 | `	sxi32 rc;` |
|       26 |  4082 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  4083 | `	return rc;` |
|        2 |  4084 |  |
|        - |  4085 | `/*` |
|        - |  4086 | ` * Resolve function context from the current frame.` |
|        - |  4087 | ` */` |
|     1018 |  4088 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  4089 |  |
|        - |  4090 | `	VmFrame *pFrame;` |
|        - |  4091 | `	ph7_vm_func *pFunc;` |
|     1019 |  4092 | `	*pzFuncName = 0;` |
|     1019 |  4093 | `	*pnFuncLen = 0;` |
|     1019 |  4094 | `	pFrame = pVm->pFrame;` |
|     1019 |  4095 | `	if( pFrame == 0 ){` |
|      ! 0 |  4096 | `		return;` |
|        - |  4097 | `	}` |
|     1019 |  4098 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|     1019 |  4099 | `	if( pFrame->pParent == 0 ){` |
|      995 |  4100 | `		return;` |
|        - |  4101 | `	}` |
|       25 |  4102 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       25 |  4103 | `	if( pFunc == 0 ){` |
|      ! 0 |  4104 | `		return;` |
|        - |  4105 | `	}` |
|       25 |  4106 | `	*pzFuncName = pFunc->sName.zString;` |
|       25 |  4107 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      510 |  4108 |  |
|        - |  4109 | `/*` |
|        - |  4110 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  4111 | ` */` |
|      524 |  4112 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  4113 |  |
|        - |  4114 | `	SyBlob sOut;` |
|        - |  4115 | `	SyString *pFile;` |
|      525 |  4116 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  4117 | `		return PH7_OK;` |
|        - |  4118 | `	}` |
|      525 |  4119 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  4120 | `		zClass = "Exception";` |
|      ! 0 |  4121 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  4122 | `	}` |
|      525 |  4123 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      503 |  4124 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      251 |  4125 | `	}` |
|      525 |  4126 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      525 |  4127 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      525 |  4128 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      525 |  4129 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      525 |  4130 | `	if( zMsg && nMsg > 0 ){` |
|      525 |  4131 | `		SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      525 |  4132 | `		SyBlobAppend(&sOut,zMsg,nMsg);` |
|      262 |  4133 | `	}` |
|      525 |  4134 | `	if( pFile ){` |
|      525 |  4135 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      525 |  4136 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      525 |  4137 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      262 |  4138 | `	}` |
|      525 |  4139 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      525 |  4140 | `	if( pFile ){` |
|      525 |  4141 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      525 |  4142 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      525 |  4143 | `		if( zFuncName && nFuncLen > 0 ){` |
|       25 |  4144 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|       13 |  4145 | `		}else{` |
|      501 |  4146 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  4147 | `		}` |
|      262 |  4148 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  4149 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  4150 | `	}else{` |
|      ! 0 |  4151 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  4152 | `	}` |
|      525 |  4153 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      525 |  4154 | `	if( pFile ){` |
|      525 |  4155 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      525 |  4156 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      525 |  4157 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      525 |  4158 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      262 |  4159 | `	}` |
|      525 |  4160 | `	VmCallErrorHandler(pVm,&sOut);` |
|      525 |  4161 | `	SyBlobRelease(&sOut);` |
|      525 |  4162 | `	return PH7_ABORT;` |
|      263 |  4163 |  |
|        - |  4164 | `/*` |
|        - |  4165 | ` * Disarm the null-coalesce-assign scratch slot armed by LOAD_IDX iP2=3.` |
|        - |  4166 | ` *` |
|        - |  4167 | ` * Arming holds a ref on the cached ArrayAccess instance so it survives the` |
|        - |  4168 | ` * intervening RHS evaluation until NULLC_STORE consumes it. Anything that` |
|        - |  4169 | ` * abandons that store path before NULLC_STORE runs — an exception thrown` |
|        - |  4170 | ` * while evaluating the RHS, a re-arm for a different target — must disarm` |
|        - |  4171 | ` * here, both to release the leaked instance ref/key and to stop a later` |
|        - |  4172 | ` * unrelated NULLC_STORE from dispatching offsetSet() on the stale slot.` |
|        - |  4173 | ` */` |
|      890 |  4174 | `static void VmCoalesceDisarm(ph7_vm *pVm)` |
|        2 |  4175 |  |
|      892 |  4176 | `	if( pVm->bCoalesceArmed ){` |
|        7 |  4177 | `		if( pVm->pCoalesceObj ){` |
|        7 |  4178 | `			PH7_ClassInstanceUnref(pVm->pCoalesceObj);` |
|        3 |  4179 | `		}` |
|        7 |  4180 | `		PH7_MemObjRelease(&pVm->sCoalesceKey);` |
|        7 |  4181 | `		pVm->pCoalesceObj = 0;` |
|        7 |  4182 | `		pVm->bCoalesceArmed = 0;` |
|        3 |  4183 | `	}` |
|      892 |  4184 |  |
|        - |  4185 | `/*` |
|        - |  4186 | ` * Throw a PHP-compatible exception of the named class from inside the VM` |
|        - |  4187 | ` * bytecode dispatch loop (where no ph7_context is available). The message` |
|        - |  4188 | ` * is a literal, non-formatted string; callers that need formatting should` |
|        - |  4189 | ` * build the SyBlob themselves and pass its data + length.` |
|        - |  4190 | ` *` |
|        - |  4191 | ` * Returns SXERR_ABORT if the throw machinery itself fails, SXRET_OK on` |
|        - |  4192 | `` * successful throw (the caller should typically `goto Abort` afterwards if`` |
|        - |  4193 | ` * the surrounding opcode cannot continue). Mirrors the inline pattern in` |
|        - |  4194 | ` * PH7_OP_THROW (see "case PH7_OP_THROW").` |
|        - |  4195 | ` */` |
|        4 |  4196 | `static sxi32 VmThrowFromVm(` |
|        - |  4197 | `	ph7_vm *pVm,` |
|        - |  4198 | `	const char *zClass,` |
|        - |  4199 | `	const char *zMsg,` |
|        - |  4200 | `	sxu32 nMsg` |
|        1 |  4201 | `){` |
|        - |  4202 | `	ph7_class *pClass;` |
|        - |  4203 | `	ph7_class_instance *pThis;` |
|        - |  4204 | `	ph7_class_method *pCons;` |
|        - |  4205 | `	VmFrame *pFrame;` |
|        - |  4206 | `	sxi32 rc;` |
|        5 |  4207 | `	pClass = PH7_VmExtractClass(pVm,zClass,SyStrlen(zClass),TRUE,0);` |
|        5 |  4208 | `	if( pClass == 0 ){` |
|      ! 0 |  4209 | `		return SXERR_ABORT;` |
|        - |  4210 | `	}` |
|        5 |  4211 | `	pThis = PH7_NewClassInstance(pVm,pClass);` |
|        5 |  4212 | `	if( pThis == 0 ){` |
|      ! 0 |  4213 | `		return SXERR_ABORT;` |
|        - |  4214 | `	}` |
|        5 |  4215 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|        5 |  4216 | `	if( pCons ){` |
|        - |  4217 | `		ph7_value sArg;` |
|        - |  4218 | `		ph7_value *apArg[1];` |
|        - |  4219 | `		SyString sMsgStr;` |
|        5 |  4220 | `		SyStringInitFromBuf(&sMsgStr,zMsg,nMsg);` |
|        5 |  4221 | `		PH7_MemObjInit(pVm,&sArg);` |
|        5 |  4222 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|        5 |  4223 | `		apArg[0] = &sArg;` |
|        5 |  4224 | `		PH7_VmCallClassMethod(pVm,pThis,pCons,0,1,apArg);` |
|        5 |  4225 | `		PH7_MemObjRelease(&sArg);` |
|        2 |  4226 | `	}` |
|        5 |  4227 | `	pFrame = pVm->pFrame;` |
|        5 |  4228 | `	if( pFrame ){` |
|        5 |  4229 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 |  4230 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        2 |  4231 | `	}` |
|        5 |  4232 | `	rc = VmThrowException(pVm,pThis);` |
|        5 |  4233 | `	PH7_ClassInstanceUnref(pThis);` |
|        5 |  4234 | `	return rc;` |
|        3 |  4235 |  |
|        - |  4236 | `/*` |
|        - |  4237 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  4238 | ` */` |
|      574 |  4239 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  4240 |  |
|        - |  4241 | `	ph7_vm *pVm;` |
|        - |  4242 | `	ph7_class *pClass;` |
|        - |  4243 | `	ph7_class_instance *pThis;` |
|        - |  4244 | `	ph7_class_method *pCons;` |
|        - |  4245 | `	ph7_value sArg;` |
|        - |  4246 | `	ph7_value *apArg[1];` |
|        - |  4247 | `	SyBlob sMsg;` |
|        - |  4248 | `	SyString sMsgStr;` |
|        - |  4249 | `	VmFrame *pFrame;` |
|        - |  4250 | `	va_list ap;` |
|        - |  4251 | `	sxi32 rc;` |
|        - |  4252 |  |
|      576 |  4253 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  4254 | `		return PH7_ABORT;` |
|        - |  4255 | `	}` |
|      576 |  4256 | `	pVm = pCtx->pVm;` |
|      576 |  4257 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  4258 | `		zClass = "Error";` |
|      ! 0 |  4259 | `	}` |
|      576 |  4260 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      576 |  4261 | `	if( pClass == 0 ){` |
|      ! 0 |  4262 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  4263 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  4264 | `			zClass` |
|        - |  4265 | `			);` |
|        - |  4266 | `	}` |
|      576 |  4267 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      576 |  4268 | `	if( pThis == 0 ){` |
|      ! 0 |  4269 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  4270 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  4271 | `			);` |
|        - |  4272 | `	}` |
|        - |  4273 |  |
|      576 |  4274 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      576 |  4275 | `	va_start(ap,zFormat);` |
|      576 |  4276 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      576 |  4277 | `	va_end(ap);` |
|        - |  4278 |  |
|      576 |  4279 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      576 |  4280 | `	if( pCons ){` |
|      576 |  4281 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      576 |  4282 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      576 |  4283 | `		apArg[0] = &sArg;` |
|      576 |  4284 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      576 |  4285 | `		PH7_MemObjRelease(&sArg);` |
|      287 |  4286 | `	}` |
|      576 |  4287 | `	SyBlobRelease(&sMsg);` |
|        - |  4288 |  |
|      576 |  4289 | `	pFrame = pVm->pFrame;` |
|      576 |  4290 | `	if( pFrame ){` |
|      576 |  4291 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      576 |  4292 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      287 |  4293 | `	}` |
|      576 |  4294 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      576 |  4295 | `	PH7_ClassInstanceUnref(pThis);` |
|      576 |  4296 | `	if( rc == SXERR_ABORT ){` |
|      491 |  4297 | `		return PH7_ABORT;` |
|        - |  4298 | `	}` |
|       86 |  4299 | `	return PH7_EXCEPTION;` |
|      289 |  4300 |  |
|        - |  4301 | `/*` |
|        - |  4302 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  4303 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  4304 | ` */` |
|      ! 0 |  4305 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  4306 |  |
|        - |  4307 | `	ph7_vm *pVm;` |
|        - |  4308 | `	SyBlob sMsg;` |
|      ! 0 |  4309 | `	const char *zFuncName = 0;` |
|      ! 0 |  4310 | `	int nFuncLen = 0;` |
|        - |  4311 | `	va_list ap;` |
|        - |  4312 | `	sxi32 rc;` |
|        - |  4313 |  |
|      ! 0 |  4314 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  4315 | `		return PH7_OK;` |
|        - |  4316 | `	}` |
|      ! 0 |  4317 | `	pVm = pCtx->pVm;` |
|      ! 0 |  4318 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  4319 | `		zClass = "Error";` |
|      ! 0 |  4320 | `	}` |
|        - |  4321 |  |
|      ! 0 |  4322 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  4323 |  |
|      ! 0 |  4324 | `	va_start(ap,zFormat);` |
|      ! 0 |  4325 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  4326 | `	va_end(ap);` |
|        - |  4327 |  |
|      ! 0 |  4328 | `	if( pCtx->pFunc ){` |
|      ! 0 |  4329 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  4330 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  4331 | `	}` |
|      ! 0 |  4332 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  4333 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  4334 | `	}` |
|      ! 0 |  4335 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  4336 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  4337 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  4338 | `	return rc;` |
|      ! 0 |  4339 |  |
|        - |  4340 | `/*` |
|        - |  4341 | ` * Save the execution state of a fiber/generator context.` |
|        - |  4342 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|        - |  4343 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|        - |  4344 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|        - |  4345 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|        - |  4346 | ` * when VmByteCodeExec returns.` |
|        - |  4347 | ` */` |
|      200 |  4348 | `static sxi32 VmSuspendCtx(` |
|        - |  4349 | `	ph7_vm *pVm,` |
|        - |  4350 | `	ph7_exec_ctx *pCtx,` |
|        - |  4351 | `	sxi32 pc,` |
|        - |  4352 | `	sxi32 nTos` |
|        - |  4353 | `	)` |
|        2 |  4354 |  |
|      100 |  4355 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      202 |  4356 | `	pCtx->pc = pc;` |
|      202 |  4357 | `	pCtx->nTos = nTos;` |
|      202 |  4358 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      202 |  4359 | `	return PH7_SUSPEND;` |
|        2 |  4360 |  |
|        - |  4361 | `/*` |
|        - |  4362 | ` * Resolve named-argument mapping.` |
|        - |  4363 | ` *` |
|        - |  4364 | ` * For each actual argument in the call, determine which formal parameter it` |
|        - |  4365 | ` * maps to (by name or by position).  On success, aSlot[i] contains the` |
|        - |  4366 | ` * formal-parameter index for actual arg i, -1 if it overflows into the` |
|        - |  4367 | ` * variadic collector, or -2 if still unresolved.  aUsed[k] is set to 1 for` |
|        - |  4368 | ` * every formal parameter that received a value.` |
|        - |  4369 | ` *` |
|        - |  4370 | ` * Returns SXRET_OK on success.  On error (duplicate, unknown parameter,` |
|        - |  4371 | ` * positional-overlaps-named) it calls VmThrowNamedArgError and returns` |
|        - |  4372 | ` * PH7_ABORT so the caller can jump to its Abort label.` |
|        - |  4373 | ` */` |
|       98 |  4374 | `static sxi32 VmResolveNamedArgs(` |
|        - |  4375 | `	ph7_vm *pVm,` |
|        - |  4376 | `	VmCallArgMap *pMap,           /* Named-arg metadata from the instruction */` |
|        - |  4377 | `	ph7_vm_func_arg *aFormalArg,  /* Formal parameter array */` |
|        - |  4378 | `	sxu32 nNonVariadic,           /* Number of non-variadic formal params */` |
|        - |  4379 | `	sxi32 iVariadicIdx,           /* Index of the variadic param, or -1 */` |
|        - |  4380 | `	sxu32 nActual,                /* Number of actual arguments on the stack */` |
|        - |  4381 | `	sxi32 *aSlot,                 /* OUT: mapping actual->formal */` |
|        - |  4382 | `	sxu8  *aUsed                  /* OUT: which formals are used */` |
|        - |  4383 |  |
|        2 |  4384 |  |
|      100 |  4385 | `	sxi32 posIdx = 0;` |
|        - |  4386 | `	sxu32 i;` |
|        - |  4387 | `	char zErrMsg[256];` |
|      100 |  4388 | `	SyZero(aUsed, nNonVariadic * sizeof(sxu8));` |
|      296 |  4389 | `	for( i = 0; i < nActual; i++ ){` |
|      198 |  4390 | `		aSlot[i] = -2;` |
|      100 |  4391 | `	}` |
|      290 |  4392 | `	for( i = 0; i < nActual; i++ ){` |
|      286 |  4393 | `		if( i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|        - |  4394 | `			/* Named argument — find formal by name */` |
|      184 |  4395 | `			int found = 0;` |
|        - |  4396 | `			sxu32 k;` |
|      304 |  4397 | `			for( k = 0; k < nNonVariadic; k++ ){` |
|      290 |  4398 | `				if( aFormalArg[k].sName.nByte == pMap->aNames[i].nByte` |
|      281 |  4399 | `					&& SyMemcmp(aFormalArg[k].sName.zString,` |
|      268 |  4400 | `						pMap->aNames[i].zString,` |
|      402 |  4401 | `						pMap->aNames[i].nByte) == 0 ){` |
|      172 |  4402 | `					if( aUsed[k] ){` |
|        7 |  4403 | `						SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  4404 | `							"Named parameter $%.*s overwrites previous argument",` |
|        4 |  4405 | `							(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        5 |  4406 | `						VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        5 |  4407 | `						return PH7_ABORT;` |
|        - |  4408 | `					}` |
|      168 |  4409 | `					aSlot[i] = (sxi32)k;` |
|      168 |  4410 | `					aUsed[k] = 1;` |
|      168 |  4411 | `					found = 1;` |
|      168 |  4412 | `					break;` |
|        - |  4413 | `				}` |
|       62 |  4414 | `			}` |
|      180 |  4415 | `			if( !found ){` |
|       14 |  4416 | `				if( iVariadicIdx >= 0 ){` |
|       11 |  4417 | `					aSlot[i] = -1; /* goes to variadic with string key */` |
|        6 |  4418 | `				}else{` |
|        4 |  4419 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  4420 | `						"Unknown named parameter $%.*s",` |
|        2 |  4421 | `						(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        3 |  4422 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        3 |  4423 | `					return PH7_ABORT;` |
|        - |  4424 | `				}` |
|        5 |  4425 | `			}` |
|       90 |  4426 | `		}else{` |
|        - |  4427 | `			/* Positional argument */` |
|       16 |  4428 | `			if( (sxu32)posIdx < nNonVariadic ){` |
|       16 |  4429 | `				if( aUsed[posIdx] ){` |
|      ! 0 |  4430 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  4431 | `						"Named parameter $%.*s overwrites previous argument",` |
|      ! 0 |  4432 | `						(int)aFormalArg[posIdx].sName.nByte,aFormalArg[posIdx].sName.zString);` |
|      ! 0 |  4433 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|      ! 0 |  4434 | `					return PH7_ABORT;` |
|        - |  4435 | `				}` |
|       16 |  4436 | `				aSlot[i] = posIdx;` |
|       16 |  4437 | `				aUsed[posIdx] = 1;` |
|        7 |  4438 | `			}else if( iVariadicIdx >= 0 ){` |
|      ! 0 |  4439 | `				aSlot[i] = -1; /* overflow to variadic */` |
|      ! 0 |  4440 | `			}` |
|       16 |  4441 | `			posIdx++;` |
|        - |  4442 | `		}` |
|       97 |  4443 | `	}` |
|       93 |  4444 | `	return SXRET_OK;` |
|       51 |  4445 |  |
|        - |  4446 | `/*` |
|        - |  4447 | ` * Is this value an object implementing Traversable (Iterator / IteratorAggregate` |
|        - |  4448 | ` * / Generator)? Used by the spread sites to decide whether to unpack it.` |
|        - |  4449 | ` */` |
|       42 |  4450 | `static int VmValueIsTraversable(ph7_vm *pVm, ph7_value *pVal)` |
|        2 |  4451 |  |
|       44 |  4452 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pTraversableClass == 0 ){` |
|       32 |  4453 | `		return 0;` |
|        - |  4454 | `	}` |
|       14 |  4455 | `	return PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass, pVm->pTraversableClass);` |
|       23 |  4456 |  |
|        - |  4457 | `/*` |
|        - |  4458 | `` * PH7_VmIteratorWalk step for array-literal Traversable spread `[...$it]`:`` |
|        - |  4459 | ` * merge each element with PHP 8.1 array-unpack key rules — string keys are` |
|        - |  4460 | ` * preserved (later wins), integer keys are renumbered.` |
|        - |  4461 | ` */` |
|       10 |  4462 | `static sxi32 VmSpreadMergeStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|        1 |  4463 |  |
|       11 |  4464 | `	ph7_hashmap *pMap = (ph7_hashmap *)pUserData;` |
|        5 |  4465 | `	(void)pVm;` |
|       11 |  4466 | `	PH7_HashmapInsert(pMap, (pKey->iFlags & MEMOBJ_STRING) ? pKey : 0 /* auto-index */, pValue);` |
|       11 |  4467 | `	return SXRET_OK;` |
|        1 |  4468 |  |
|        - |  4469 | `/*` |
|        - |  4470 | `` * PH7_VmIteratorWalk step for call-argument Traversable spread `f(...$it)`:`` |
|        - |  4471 | ` * collect values positionally (keys ignored) into a temp array.` |
|        - |  4472 | ` */` |
|        6 |  4473 | `static sxi32 VmSpreadValuesStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|        1 |  4474 |  |
|        3 |  4475 | `	(void)pVm; (void)pKey;` |
|        7 |  4476 | `	PH7_HashmapInsert((ph7_hashmap *)pUserData, 0 /* auto-index */, pValue);` |
|        7 |  4477 | `	return SXRET_OK;` |
|        1 |  4478 |  |
|        - |  4479 | `/*` |
|        - |  4480 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  4481 | ` *` |
|        - |  4482 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  4483 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  4484 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  4485 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  4486 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  4487 | ` * then the program execution is halted.` |
|        - |  4488 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  4489 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  4490 | ` * or to reset the VM to it's initial state.` |
|        - |  4491 | ` */` |
|    46398 |  4492 | `static sxi32 VmByteCodeExec(` |
|        - |  4493 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  4494 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  4495 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  4496 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  4497 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  4498 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  4499 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  4500 | `	sxi32 nPc,           /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  4501 | `	ph7_vm_func *pEnforceRetFunc /* NULL except when this invocation is a user-fn body; when set, the terminating OP_DONE validates the return value against pEnforceRetFunc's declared type. */` |
|        - |  4502 | `	)` |
|        2 |  4503 |  |
|        - |  4504 | `	VmInstr *pInstr;` |
|        - |  4505 | `	ph7_value *pTos;` |
|        - |  4506 | `	SySet aArg;` |
|        - |  4507 | `	sxu32 nExceptionBase; /* Exception stack depth at entry (for finally drain guard) */` |
|        - |  4508 | `	sxi32 pc;` |
|        - |  4509 | `	sxi32 rc;` |
|        - |  4510 | `	/* Argument container */` |
|    46400 |  4511 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    46400 |  4512 | `	if( nTos < 0 ){` |
|    42802 |  4513 | `		pTos = &pStack[-1];` |
|    21402 |  4514 | `	}else{` |
|     3600 |  4515 | `		pTos = &pStack[nTos];` |
|        - |  4516 | `	}` |
|    46400 |  4517 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    46400 |  4518 | `	pc = nPc;` |
|        - |  4519 | `/*` |
|        - |  4520 | ` * Typed-property enforcement helper for compound stores. Called before` |
|        - |  4521 | ` * PH7_MemObjStore writes into a member memobj slot. On failure throws a` |
|        - |  4522 | ` * PHP TypeError and either jumps to the nearest catch block or propagates` |
|        - |  4523 | ` * out of the VM loop. Must be used inside a case of the main switch.` |
|        - |  4524 | ` */` |
|        - |  4525 | `#define PH7_ENFORCE_TYPED_STORE(nIdxArg, pSrcArg) \` |
|        - |  4526 | `	{ \` |
|        - |  4527 | `		sxi32 _rcT = VmEnforcePropertyTypeOnStore(&(*pVm),(nIdxArg),(pSrcArg)); \` |
|        - |  4528 | `		if( _rcT == PH7_ABORT ){ goto Abort; } \` |
|        - |  4529 | `		if( _rcT == PH7_EXCEPTION ){ \` |
|        - |  4530 | `			VmFrame *_pFrmT = pVm->pFrame; \` |
|        - |  4531 | `			if( _pFrmT && (_pFrmT->iFlags & VM_FRAME_EXCEPTION) && _pFrmT->iExceptionJump > 0 ){ \` |
|        - |  4532 | `				pc = _pFrmT->iExceptionJump - 1; \` |
|        - |  4533 | `				break; \` |
|        - |  4534 | `			} \` |
|        - |  4535 | `			goto Exception; \` |
|        - |  4536 | `		} \` |
|        - |  4537 | `	}` |
|        - |  4538 | `	/* Execute as much as we can */` |
|  5933631 |  4539 | `	for(;;){` |
|        - |  4540 | `		/* Fetch the instruction to execute */` |
| 11866560 |  4541 | `		pInstr = &aInstr[pc];` |
| 11866560 |  4542 | `		rc = SXRET_OK;` |
|        - |  4543 | `/*` |
|        - |  4544 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  4545 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  4546 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  4547 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  4548 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  4549 | ` */` |
| 11866560 |  4550 | `		switch(pInstr->iOp){` |
|        - |  4551 | `/*` |
|        - |  4552 | ` * DONE: P1 * *` |
|        - |  4553 | ` *` |
|        - |  4554 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  4555 | ` * and return immediately.` |
|        - |  4556 | ` */` |
|    22722 |  4557 | `case PH7_OP_DONE:` |
|        - |  4558 | `	/* Return-type enforcement: only the user-function CALL handler (and` |
|        - |  4559 | `	 * the fiber start/resume paths) set pEnforceRetFunc, so this branch is` |
|        - |  4560 | `	 * skipped for default-value bytecode, class-method mini-programs,` |
|        - |  4561 | `	 * callback trampolines, and the main script. */` |
|    45444 |  4562 | `	if( pEnforceRetFunc && pEnforceRetFunc->nReturnType > 0` |
|      482 |  4563 | `	 && !(VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW) ){` |
|        - |  4564 | `		/* The VM_FRAME_THROW guard skips enforcement when the function is` |
|        - |  4565 | `		 * unwinding because an exception was thrown (the compiler routes an` |
|        - |  4566 | `		 * uncaught throw to this terminal OP_DONE): PHP does not type-check a` |
|        - |  4567 | `		 * value the function never actually returned, so enforcing here would` |
|        - |  4568 | `		 * raise a spurious "Return value must be of type X" over the real` |
|        - |  4569 | `		 * exception. */` |
|      476 |  4570 | `		ph7_value *pRetVal = 0;` |
|      476 |  4571 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|      326 |  4572 | `			pRetVal = pTos;` |
|      162 |  4573 | `		}` |
|      476 |  4574 | `		rc = VmEnforceReturnType(&(*pVm), pEnforceRetFunc, pRetVal);` |
|      476 |  4575 | `		if( rc == PH7_ABORT ) goto Abort;` |
|      470 |  4576 | `		if( rc == PH7_EXCEPTION ){` |
|        7 |  4577 | `			if( pInstr->iP1 && pTos >= pStack ){` |
|        7 |  4578 | `				PH7_MemObjRelease(pTos);` |
|        7 |  4579 | `				pTos--;` |
|        3 |  4580 | `			}` |
|        7 |  4581 | `			goto Exception;` |
|        - |  4582 | `		}` |
|        - |  4583 | `		/* Don't enforce twice if the function loops through multiple` |
|        - |  4584 | `		 * OP_DONEs (it shouldn't — compilers emit one terminal DONE — but` |
|        - |  4585 | `		 * defensively we clear the pointer after a successful check). */` |
|      464 |  4586 | `		pEnforceRetFunc = 0;` |
|      231 |  4587 | `	}` |
|    45434 |  4588 | `	if( pInstr->iP1 ){` |
|        - |  4589 | `#ifdef UNTRUST` |
|        - |  4590 | `		if( pTos < pStack ){` |
|        - |  4591 | `			goto Abort;` |
|        - |  4592 | `		}` |
|        - |  4593 | `#endif` |
|    27880 |  4594 | `		if( pLastRef ){` |
|    16838 |  4595 | `			*pLastRef = pTos->nIdx;` |
|     8418 |  4596 | `		}` |
|    27880 |  4597 | `		if( pResult ){` |
|        - |  4598 | `			/* Execution result */` |
|    26264 |  4599 | `			PH7_MemObjStore(pTos,pResult);` |
|    13131 |  4600 | `		}` |
|    27880 |  4601 | `		VmPopOperand(&pTos,1);` |
|    31495 |  4602 | `	}else if( pLastRef ){` |
|        - |  4603 | `		/* Nothing referenced */` |
|     2034 |  4604 | `		*pLastRef = SXU32_HIGH;` |
|     1016 |  4605 | `	}` |
|        - |  4606 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  4607 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  4608 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  4609 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  4610 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  4611 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  4612 | `	 * block can override it.` |
|        - |  4613 | `	 */` |
|    45436 |  4614 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
|        3 |  4615 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        3 |  4616 | `		ph7_exception *pExc = apExc[SySetUsed(&pVm->aException) - 1];` |
|        3 |  4617 | `		(void)SySetPop(&pVm->aException);` |
|        3 |  4618 | `		pExc->pFrame = 0;` |
|        3 |  4619 | `		VmLeaveFrame(&(*pVm));` |
|        3 |  4620 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|        3 |  4621 | `			pExc->iFinallyDone = 1;` |
|        - |  4622 | `			/* Pass pResult so that 'return' inside finally can override the value */` |
|        3 |  4623 | `			rc = VmLocalExec(&(*pVm),&pExc->sFinally,pResult);` |
|        3 |  4624 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4625 | `				goto Abort;` |
|        - |  4626 | `			}` |
|        1 |  4627 | `		}` |
|        1 |  4628 | `	}` |
|    45434 |  4629 | `	goto Done;` |
|        - |  4630 | `/*` |
|        - |  4631 | ` * HALT: P1 * *` |
|        - |  4632 | ` *` |
|        - |  4633 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  4634 | ` * and abort immediately.` |
|        - |  4635 | ` */` |
|        7 |  4636 | `case PH7_OP_HALT:` |
|       15 |  4637 | `	if( pInstr->iP1 ){` |
|        - |  4638 | `#ifdef UNTRUST` |
|        - |  4639 | `		if( pTos < pStack ){` |
|        - |  4640 | `			goto Abort;` |
|        - |  4641 | `		}` |
|        - |  4642 | `#endif` |
|       15 |  4643 | `		if( pLastRef ){` |
|        3 |  4644 | `			*pLastRef = pTos->nIdx;` |
|        1 |  4645 | `		}` |
|       15 |  4646 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|       11 |  4647 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  4648 | `				/* Output the exit message */` |
|       16 |  4649 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        5 |  4650 | `					pVm->sVmConsumer.pUserData);` |
|       11 |  4651 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|        6 |  4652 | `			}` |
|       10 |  4653 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  4654 | `			/* Record exit status */` |
|        5 |  4655 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  4656 | `		}` |
|       15 |  4657 | `		VmPopOperand(&pTos,1);` |
|        7 |  4658 | `	}else if( pLastRef ){` |
|        - |  4659 | `		/* Nothing referenced */` |
|      ! 0 |  4660 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  4661 | `	}` |
|        - |  4662 | `	/* Request a VM-wide halt so the abort cascades out of any enclosing` |
|        - |  4663 | `	 * include/require/eval execution unit; shutdown callbacks then run` |
|        - |  4664 | `	 * at the top level (PHP semantics) instead of hard-exiting here.` |
|        - |  4665 | `	 */` |
|       15 |  4666 | `	pVm->bHaltRequested = 1;` |
|       15 |  4667 | `	goto Abort;` |
|        - |  4668 | `/*` |
|        - |  4669 | ` * JMP: * P2 *` |
|        - |  4670 | ` *` |
|        - |  4671 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  4672 | ` * the one at index P2 from the beginning of the program.` |
|        - |  4673 | ` */` |
|   252742 |  4674 | `case PH7_OP_JMP:` |
|   505530 |  4675 | `	pc = pInstr->iP2 - 1;` |
|   505530 |  4676 | `	break;` |
|        - |  4677 | `/*` |
|        - |  4678 | ` * JZ: P1 P2 *` |
|        - |  4679 | ` *` |
|        - |  4680 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  4681 | ` * entry in the stack if P1 is zero.` |
|        - |  4682 | ` */` |
|   600172 |  4683 | `case PH7_OP_JZ:` |
|        - |  4684 | `#ifdef UNTRUST` |
|        - |  4685 | `	if( pTos < pStack ){` |
|        - |  4686 | `		goto Abort;` |
|        - |  4687 | `	}` |
|        - |  4688 | `#endif` |
|        - |  4689 | `	/* Get a boolean value */` |
|  1200434 |  4690 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      174 |  4691 | `		PH7_MemObjToBool(pTos);` |
|       86 |  4692 | `	}` |
|  1200434 |  4693 | `	if( !pTos->x.iVal ){` |
|        - |  4694 | `		/* Take the jump */` |
|   617754 |  4695 | `		pc = pInstr->iP2 - 1;` |
|   308876 |  4696 | `	}` |
|  1200434 |  4697 | `	if( !pInstr->iP1 ){` |
|   950714 |  4698 | `		VmPopOperand(&pTos,1);` |
|   475378 |  4699 | `	}` |
|  1200434 |  4700 | `	break;` |
|        - |  4701 | `/*` |
|        - |  4702 | ` * JNZ: P1 P2 *` |
|        - |  4703 | ` *` |
|        - |  4704 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  4705 | ` * entry in the stack if P1 is zero.` |
|        - |  4706 | ` */` |
|    61446 |  4707 | `case PH7_OP_JNZ:` |
|        - |  4708 | `#ifdef UNTRUST` |
|        - |  4709 | `	if( pTos < pStack ){` |
|        - |  4710 | `		goto Abort;` |
|        - |  4711 | `	}` |
|        - |  4712 | `#endif` |
|        - |  4713 | `	/* Get a boolean value */` |
|   122894 |  4714 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4715 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4716 | `	}` |
|   122894 |  4717 | `	if( pTos->x.iVal ){` |
|        - |  4718 | `		/* Take the jump */` |
|     5634 |  4719 | `		pc = pInstr->iP2 - 1;` |
|     2816 |  4720 | `	}` |
|   122894 |  4721 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  4722 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4723 | `	}` |
|   122894 |  4724 | `	break;` |
|        - |  4725 | `/*` |
|        - |  4726 | ` * NOOP: * * *` |
|        - |  4727 | ` *` |
|        - |  4728 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  4729 | ` * destination.` |
|        - |  4730 | ` */` |
|      ! 0 |  4731 | `case PH7_OP_NOOP:` |
|      ! 0 |  4732 | `	break;` |
|        - |  4733 | `/*` |
|        - |  4734 | ` * POP: P1 * *` |
|        - |  4735 | ` *` |
|        - |  4736 | ` * Pop P1 elements from the operand stack.` |
|        - |  4737 | ` */` |
|   465120 |  4738 | `case PH7_OP_POP: {` |
|   930286 |  4739 | `	sxi32 n = pInstr->iP1;` |
|   930286 |  4740 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  4741 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|       50 |  4742 | `		n = (sxi32)(pTos - pStack);` |
|       24 |  4743 | `	}` |
|   930286 |  4744 | `	VmPopOperand(&pTos,n);` |
|   930286 |  4745 | `	break;` |
|        - |  4746 | `				 }` |
|        - |  4747 | `/*` |
|        - |  4748 | ` * DUP: * * *` |
|        - |  4749 | ` *` |
|        - |  4750 | ` * Duplicate the top of the stack.` |
|        - |  4751 | ` */` |
|       41 |  4752 | `case PH7_OP_DUP:` |
|        - |  4753 | `#ifdef UNTRUST` |
|        - |  4754 | `	if( pTos < pStack ){` |
|        - |  4755 | `		goto Abort;` |
|        - |  4756 | `	}` |
|        - |  4757 | `#endif` |
|       84 |  4758 | `	pTos++;` |
|       84 |  4759 | `	PH7_MemObjInit(pVm,pTos);` |
|       84 |  4760 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       84 |  4761 | `	break;` |
|        - |  4762 | `/*` |
|        - |  4763 | ` * NSSWITCH: * * P3` |
|        - |  4764 | ` *` |
|        - |  4765 | ` * Switch the active namespace at runtime.` |
|        - |  4766 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  4767 | ` */` |
|     7887 |  4768 | `case PH7_OP_NSSWITCH:` |
|    15776 |  4769 | `	SyBlobReset(&pVm->sNamespace);` |
|    15776 |  4770 | `	if( pInstr->p3 ){` |
|      100 |  4771 | `		const char *zNs = (const char *)pInstr->p3;` |
|      100 |  4772 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       49 |  4773 | `	}` |
|        - |  4774 | `	/* Clear namespace-scoped use-const imports */` |
|    15776 |  4775 | `	SyHashRelease(&pVm->hUseConstImports);` |
|    15776 |  4776 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|    15776 |  4777 | `	break;` |
|        - |  4778 | `/* OP_USECONST P1 * P3` |
|        - |  4779 | ` * Register a use-const import at runtime. P1 is the alias length,` |
|        - |  4780 | ` * P3 points to a two-pointer array: [0]=alias, [1]=FQN.` |
|        - |  4781 | ` * This is namespace-scoped: NSSWITCH clears all imports.` |
|        - |  4782 | ` */` |
|        7 |  4783 | `case PH7_OP_USECONST: {` |
|       16 |  4784 | `	char **azPair = (char **)pInstr->p3;` |
|       16 |  4785 | `	if( azPair ){` |
|       16 |  4786 | `		SyHashInsert(&pVm->hUseConstImports,azPair[0],(sxu32)pInstr->iP1,azPair[1]);` |
|        7 |  4787 | `	}` |
|       16 |  4788 | `	break;` |
|        - |  4789 | `				}` |
|        - |  4790 | `/*` |
|        - |  4791 | ` * CVT_INT: * * *` |
|        - |  4792 | ` *` |
|        - |  4793 | ` * Force the top of the stack to be an integer.` |
|        - |  4794 | ` */` |
|       80 |  4795 | `case PH7_OP_CVT_INT:` |
|        - |  4796 | `#ifdef UNTRUST` |
|        - |  4797 | `	if( pTos < pStack ){` |
|        - |  4798 | `		goto Abort;` |
|        - |  4799 | `	}` |
|        - |  4800 | `#endif` |
|      162 |  4801 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      111 |  4802 | `		PH7_MemObjToInteger(pTos);` |
|       55 |  4803 | `	}` |
|        - |  4804 | `	/* Invalidate any prior representation */` |
|      162 |  4805 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|      162 |  4806 | `	break;` |
|        - |  4807 | `/*` |
|        - |  4808 | ` * CVT_REAL: * * *` |
|        - |  4809 | ` *` |
|        - |  4810 | ` * Force the top of the stack to be a real.` |
|        - |  4811 | ` */` |
|        7 |  4812 | `case PH7_OP_CVT_REAL:` |
|        - |  4813 | `#ifdef UNTRUST` |
|        - |  4814 | `	if( pTos < pStack ){` |
|        - |  4815 | `		goto Abort;` |
|        - |  4816 | `	}` |
|        - |  4817 | `#endif` |
|       15 |  4818 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       11 |  4819 | `		PH7_MemObjToReal(pTos);` |
|        5 |  4820 | `	}` |
|        - |  4821 | `	/* Invalidate any prior representation */` |
|       15 |  4822 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|       15 |  4823 | `	break;` |
|        - |  4824 | `/*` |
|        - |  4825 | ` * CVT_STR: * * *` |
|        - |  4826 | ` *` |
|        - |  4827 | ` * Force the top of the stack to be a string.` |
|        - |  4828 | ` */` |
|      163 |  4829 | `case PH7_OP_CVT_STR:` |
|        - |  4830 | `#ifdef UNTRUST` |
|        - |  4831 | `	if( pTos < pStack ){` |
|        - |  4832 | `		goto Abort;` |
|        - |  4833 | `	}` |
|        - |  4834 | `#endif` |
|      328 |  4835 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      308 |  4836 | `		PH7_MemObjToString(pTos);` |
|      153 |  4837 | `	}` |
|      328 |  4838 | `	break;` |
|        - |  4839 | `/*` |
|        - |  4840 | ` * CVT_BOOL: * * *` |
|        - |  4841 | ` *` |
|        - |  4842 | ` * Force the top of the stack to be a boolean.` |
|        - |  4843 | ` */` |
|        5 |  4844 | `case PH7_OP_CVT_BOOL:` |
|        - |  4845 | `#ifdef UNTRUST` |
|        - |  4846 | `	if( pTos < pStack ){` |
|        - |  4847 | `		goto Abort;` |
|        - |  4848 | `	}` |
|        - |  4849 | `#endif` |
|       11 |  4850 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  4851 | `		PH7_MemObjToBool(pTos);` |
|        3 |  4852 | `	}` |
|       11 |  4853 | `	break;` |
|        - |  4854 | `/*` |
|        - |  4855 | ` * CVT_NULL: * * *` |
|        - |  4856 | ` *` |
|        - |  4857 | ` * Nullify the top of the stack.` |
|        - |  4858 | ` */` |
|        3 |  4859 | `case PH7_OP_CVT_NULL:` |
|        - |  4860 | `#ifdef UNTRUST` |
|        - |  4861 | `	if( pTos < pStack ){` |
|        - |  4862 | `		goto Abort;` |
|        - |  4863 | `	}` |
|        - |  4864 | `#endif` |
|        7 |  4865 | `	PH7_MemObjRelease(pTos);` |
|        7 |  4866 | `	break;` |
|        - |  4867 | `/*` |
|        - |  4868 | ` * CVT_NUMC: * * *` |
|        - |  4869 | ` *` |
|        - |  4870 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  4871 | ` */` |
|      ! 0 |  4872 | `case PH7_OP_CVT_NUMC:` |
|        - |  4873 | `#ifdef UNTRUST` |
|        - |  4874 | `	if( pTos < pStack ){` |
|        - |  4875 | `		goto Abort;` |
|        - |  4876 | `	}` |
|        - |  4877 | `#endif` |
|        - |  4878 | `	/* Force a numeric cast */` |
|      ! 0 |  4879 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  4880 | `	break;` |
|        - |  4881 | `/*` |
|        - |  4882 | ` * CVT_ARRAY: * * *` |
|        - |  4883 | ` *` |
|        - |  4884 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  4885 | ` */` |
|       10 |  4886 | `case PH7_OP_CVT_ARRAY:` |
|        - |  4887 | `#ifdef UNTRUST` |
|        - |  4888 | `	if( pTos < pStack ){` |
|        - |  4889 | `		goto Abort;` |
|        - |  4890 | `	}` |
|        - |  4891 | `#endif` |
|        - |  4892 | `	/* Force a hashmap cast */` |
|       21 |  4893 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  4894 | `	if( rc != SXRET_OK ){` |
|        - |  4895 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  4896 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  4897 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  4898 | `	}` |
|       21 |  4899 | `	break;` |
|        - |  4900 | `/*` |
|        - |  4901 | ` * CVT_OBJ: * * *` |
|        - |  4902 | ` *` |
|        - |  4903 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  4904 | ` */` |
|        8 |  4905 | `case PH7_OP_CVT_OBJ:` |
|        - |  4906 | `#ifdef UNTRUST` |
|        - |  4907 | `	if( pTos < pStack ){` |
|        - |  4908 | `		goto Abort;` |
|        - |  4909 | `	}` |
|        - |  4910 | `#endif` |
|       17 |  4911 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  4912 | `		/* Force a 'stdClass()' cast */` |
|       17 |  4913 | `		PH7_MemObjToObject(pTos);` |
|        8 |  4914 | `	}` |
|       17 |  4915 | `	break;` |
|        - |  4916 | `/*` |
|        - |  4917 | ` * ERR_CTRL * * *` |
|        - |  4918 | ` *` |
|        - |  4919 | ` * Error control operator.` |
|        - |  4920 | ` */` |
|    16173 |  4921 | `case PH7_OP_ERR_CTRL:` |
|        - |  4922 | `	/*` |
|        - |  4923 | `	 * TICKET 1433-038:` |
|        - |  4924 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  4925 | `	 * use the public API,to control error output.` |
|        - |  4926 | `	 */` |
|    32346 |  4927 | `	break;` |
|        - |  4928 | `/*` |
|        - |  4929 | ` * IS_A * * *` |
|        - |  4930 | ` *` |
|        - |  4931 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  4932 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  4933 | ` * holding a class name or an object).` |
|        - |  4934 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  4935 | ` */` |
|       77 |  4936 | `case PH7_OP_IS_A:{` |
|      156 |  4937 | `	ph7_value *pNos = &pTos[-1];` |
|      156 |  4938 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  4939 | `#ifdef UNTRUST` |
|        - |  4940 | `	if( pNos < pStack ){` |
|        - |  4941 | `		goto Abort;` |
|        - |  4942 | `	}` |
|        - |  4943 | `#endif` |
|      156 |  4944 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|      154 |  4945 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      154 |  4946 | `		ph7_class *pClass = 0;` |
|        - |  4947 | `		/* Extract the target class */` |
|      154 |  4948 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  4949 | `			/* Instance already loaded */` |
|      ! 0 |  4950 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      154 |  4951 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|      154 |  4952 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|      154 |  4953 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  4954 | `			/* Handle self/static/parent keywords */` |
|      154 |  4955 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        5 |  4956 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|      152 |  4957 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  4958 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      151 |  4959 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        5 |  4960 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        5 |  4961 | `				if( pSelf && pSelf->pBase ){` |
|        5 |  4962 | `					pClass = pSelf->pBase;` |
|        2 |  4963 | `				}` |
|        3 |  4964 | `			}else{` |
|      144 |  4965 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  4966 | `			}` |
|       76 |  4967 | `		}` |
|      154 |  4968 | `		if( pClass ){` |
|        - |  4969 | `			/* Perform the query */` |
|      154 |  4970 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       76 |  4971 | `		}` |
|       76 |  4972 | `	}` |
|        - |  4973 | `	/* Push result */` |
|      156 |  4974 | `	VmPopOperand(&pTos,1);` |
|      156 |  4975 | `	PH7_MemObjRelease(pTos);` |
|      156 |  4976 | `	pTos->x.iVal = iRes;` |
|      156 |  4977 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      156 |  4978 | `	break;` |
|        - |  4979 | `				 }` |
|        - |  4980 |  |
|        - |  4981 | `/*` |
|        - |  4982 | ` * LOADC P1 P2 *` |
|        - |  4983 | ` *` |
|        - |  4984 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  4985 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  4986 | ` */` |
|  1022546 |  4987 | `case PH7_OP_LOADC: {` |
|        - |  4988 | `	ph7_value *pObj;` |
|        - |  4989 | `	/* Reserve a room */` |
|  2045138 |  4990 | `	pTos++;` |
|  3057786 |  4991 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  2045138 |  4992 | `		if( (pInstr->iP1 & PH7_LOADC_EXPAND) && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  4993 | `			SyHashEntry *pEntry;` |
|        - |  4994 | `			/* Check use const imports first — imports take precedence */` |
|        - |  4995 | `			{` |
|        - |  4996 | `				SyHashEntry *pConstImport;` |
|    29834 |  4997 | `				pConstImport = SyHashGet(&pVm->hUseConstImports,` |
|    19888 |  4998 | `					SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    19890 |  4999 | `				if( pConstImport ){` |
|       11 |  5000 | `					const char *zFQN = (const char *)pConstImport->pUserData;` |
|       11 |  5001 | `					pEntry = SyHashGet(&pVm->hConstant,zFQN,SyStrlen(zFQN));` |
|       11 |  5002 | `					if( pEntry ){` |
|       11 |  5003 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|       11 |  5004 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 |  5005 | `						SyBlobReset(&pTos->sBlob);` |
|       11 |  5006 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|       11 |  5007 | `						pTos->nIdx = SXU32_HIGH;` |
|       11 |  5008 | `						break;` |
|        - |  5009 | `					}` |
|        - |  5010 | `					/* Import found but constant not defined — fall through */` |
|      ! 0 |  5011 | `				}` |
|        - |  5012 | `			}` |
|        - |  5013 | `			/* Candidate for expansion via user defined callbacks */` |
|    19880 |  5014 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    19880 |  5015 | `			if( pEntry ){` |
|    19874 |  5016 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  5017 | `				/* Set a NULL default value */` |
|    19874 |  5018 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    19874 |  5019 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  5020 | `				/* Invoke the callback and deal with the expanded value */` |
|    19874 |  5021 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  5022 | `				/* Mark as constant */` |
|    19874 |  5023 | `				pTos->nIdx = SXU32_HIGH;` |
|    19874 |  5024 | `				break;` |
|        - |  5025 | `			}` |
|        - |  5026 | `			/* Constant not found by bare name.  If a namespace is active and` |
|        - |  5027 | `			 * the name is unqualified, try namespace\name (PHP resolution order:` |
|        - |  5028 | `			 * use-const imports → current NS → global → string fallback).` |
|        - |  5029 | `			 * Absolute references (\NAME) skip the NS fallback too. */` |
|        - |  5030 | `			{` |
|        8 |  5031 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        8 |  5032 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  5033 | `				sxu32 j;` |
|        8 |  5034 | `				int isQualified = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       24 |  5035 | `				for( j = 0; !isQualified && j < nLit; j++ ){` |
|       18 |  5036 | `					if( zLit[j] == '\\' ){ isQualified = 1; break; }` |
|       10 |  5037 | `				}` |
|        8 |  5038 | `				if( !isQualified && SyBlobLength(&pVm->sNamespace) > 0 ){` |
|        - |  5039 | `					/* Try current_namespace\name */` |
|      ! 0 |  5040 | `					SyBlobReset(&pVm->sWorker);` |
|      ! 0 |  5041 | `					SyBlobAppend(&pVm->sWorker,SyBlobData(&pVm->sNamespace),SyBlobLength(&pVm->sNamespace));` |
|      ! 0 |  5042 | `					SyBlobAppend(&pVm->sWorker,"\\",1);` |
|      ! 0 |  5043 | `					SyBlobAppend(&pVm->sWorker,zLit,nLit);` |
|      ! 0 |  5044 | `					pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pVm->sWorker),SyBlobLength(&pVm->sWorker));` |
|      ! 0 |  5045 | `					if( pEntry ){` |
|      ! 0 |  5046 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|      ! 0 |  5047 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  5048 | `						SyBlobReset(&pTos->sBlob);` |
|      ! 0 |  5049 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|      ! 0 |  5050 | `						pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5051 | `						break;` |
|        - |  5052 | `					}` |
|        - |  5053 | `					/* Not in current namespace either — fall through to global/string */` |
|      ! 0 |  5054 | `				}` |
|        8 |  5055 | `				if( isQualified ){` |
|        - |  5056 | `					/* Qualified name: must be a real constant. */` |
|        3 |  5057 | `					SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  5058 | `					SyBlob sErr;` |
|        3 |  5059 | `					SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  5060 | `					SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  5061 | `					if( pErrFile ){` |
|        3 |  5062 | `						SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  5063 | `					}` |
|        3 |  5064 | `					SyBlobAppend(&sErr,"\n",1);` |
|        3 |  5065 | `					VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  5066 | `					SyBlobRelease(&sErr);` |
|        3 |  5067 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  5068 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 |  5069 | `					goto LoadC_Done;` |
|        - |  5070 | `				}` |
|        - |  5071 | `			}` |
|        2 |  5072 | `		}` |
|  2025254 |  5073 | `		PH7_MemObjLoad(pObj,pTos);` |
|  1012650 |  5074 | `	}else{` |
|        - |  5075 | `		/* Set a NULL value */` |
|      ! 0 |  5076 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  5077 | `	}` |
|  1012605 |  5078 | `LoadC_Done:` |
|        - |  5079 | `	/* Mark as constant */` |
|  2025256 |  5080 | `	pTos->nIdx = SXU32_HIGH;` |
|  2025256 |  5081 | `	break;` |
|        - |  5082 | `				  }` |
|        - |  5083 | `/*` |
|        - |  5084 | ` * LOAD: P1 * P3` |
|        - |  5085 | ` *` |
|        - |  5086 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  5087 | ` * from the P3 operand.` |
|        - |  5088 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  5089 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  5090 | ` */` |
|  1582554 |  5091 | `case PH7_OP_LOAD:{` |
|        - |  5092 | `	ph7_value *pObj;` |
|        - |  5093 | `	SyString sName;` |
|  3165330 |  5094 | `	if( pInstr->p3 == 0 ){` |
|        - |  5095 | `		/* Take the variable name from the top of the stack */` |
|        - |  5096 | `#ifdef UNTRUST` |
|        - |  5097 | `		if( pTos < pStack ){` |
|        - |  5098 | `			goto Abort;` |
|        - |  5099 | `		}` |
|        - |  5100 | `#endif` |
|        - |  5101 | `		/* Force a string cast */` |
|       19 |  5102 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  5103 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5104 | `		}` |
|       19 |  5105 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  5106 | `	}else{` |
|  3165312 |  5107 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5108 | `		/* Reserve a room for the target object */` |
|  3165312 |  5109 | `		pTos++;` |
|        - |  5110 | `	}` |
|        - |  5111 | `	/* Extract the requested memory object */` |
|  3165330 |  5112 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  3165330 |  5113 | `	if( pObj == 0 ){` |
|      858 |  5114 | `		if( pInstr->iP1 ){` |
|        - |  5115 | `			/* Variable not found,load NULL */` |
|      858 |  5116 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5117 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5118 | `			}else{` |
|      858 |  5119 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  5120 | `			}` |
|      858 |  5121 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1582984 |  5122 | `			break;` |
|      ! 0 |  5123 | `		}else{` |
|        - |  5124 | `			/* Fatal error */` |
|      ! 0 |  5125 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  5126 | `			goto Abort;` |
|        - |  5127 | `		}` |
|        - |  5128 | `	}` |
|        - |  5129 | `	/* Load variable contents */` |
|  3164474 |  5130 | `	PH7_MemObjLoad(pObj,pTos);` |
|  3164474 |  5131 | `	pTos->nIdx = pObj->nIdx;` |
|  3164474 |  5132 | `	break;` |
|        - |  5133 | `				   }` |
|        - |  5134 | `/*` |
|        - |  5135 | ` * LOAD_MAP P1 * *` |
|        - |  5136 | ` *` |
|        - |  5137 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  5138 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  5139 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  5140 | ` */` |
|    22964 |  5141 | `case PH7_OP_LOAD_MAP: {` |
|        - |  5142 | `	ph7_hashmap *pMap;` |
|        - |  5143 | `	/* Allocate a new hashmap instance */` |
|    45930 |  5144 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    45930 |  5145 | `	if( pMap == 0 ){` |
|      ! 0 |  5146 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  5147 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  5148 | `		goto Abort;` |
|        - |  5149 | `	}` |
|    45930 |  5150 | `	if( pInstr->iP1 > 0 ){` |
|     2808 |  5151 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|     2808 |  5152 | `		sxi32 rcSpread = SXRET_OK;` |
|        - |  5153 | `		/* Perform the insertion */` |
|     8568 |  5154 | `		while( pEntry < pTos ){` |
|     5780 |  5155 | `			if( pEntry[1].iFlags & MEMOBJ_AUX_SPREAD ){` |
|        - |  5156 | `				/* Array unpacking: '...$expr'. Merge entries with PHP 8.1` |
|        - |  5157 | `				 * semantics — string keys preserved (later wins), int keys` |
|        - |  5158 | `				 * renumbered. Same routine that backs array_merge. */` |
|       76 |  5159 | `				if( pEntry[1].iFlags & MEMOBJ_HASHMAP ){` |
|       53 |  5160 | `					sxi32 rcMerge = PH7_HashmapMerge((ph7_hashmap *)pEntry[1].x.pOther,pMap);` |
|       53 |  5161 | `					if( rcMerge != SXRET_OK ){` |
|        - |  5162 | `						/* Merge failure (OOM): match the PH7_NewHashmap OOM` |
|        - |  5163 | `						 * path — emit fatal and abort, leaving no partial` |
|        - |  5164 | `						 * map dangling. */` |
|      ! 0 |  5165 | `						VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  5166 | `							"Fatal, PH7 engine is running out of memory while spreading array at instruction #:%d",pc);` |
|      ! 0 |  5167 | `						rcSpread = PH7_ABORT;` |
|      ! 0 |  5168 | `						break;` |
|        1 |  5169 | `					}` |
|       50 |  5170 | `				}else if( VmValueIsTraversable(pVm,&pEntry[1]) ){` |
|        - |  5171 | `					/* Traversable unpacking (PHP 8.1): walk it into the map using the` |
|        - |  5172 | `					 * same key rules as array spread (string keys kept, int renumbered). */` |
|        5 |  5173 | `					sxi32 rcW = PH7_VmIteratorWalk(&(*pVm),&pEntry[1],VmSpreadMergeStep,pMap);` |
|        5 |  5174 | `					if( rcW == PH7_EXCEPTION \|\| rcW == PH7_ABORT ){` |
|      ! 0 |  5175 | `						rcSpread = rcW;` |
|      ! 0 |  5176 | `						break;` |
|        - |  5177 | `					}` |
|        3 |  5178 | `				}else{` |
|        - |  5179 | `					/* Throw a catchable Error matching PHP semantics. */` |
|       20 |  5180 | `					rcSpread = VmThrowSpreadError(&(*pVm),&pEntry[1]);` |
|       20 |  5181 | `					break;` |
|        1 |  5182 | `				}` |
|     5734 |  5183 | `			}else if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  5184 | `				/* Insertion by reference */` |
|      151 |  5185 | `				PH7_HashmapInsertByRef(pMap,` |
|      100 |  5186 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|      100 |  5187 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  5188 | `					);` |
|       51 |  5189 | `			}else{` |
|        - |  5190 | `				/* Standard insertion */` |
|     8408 |  5191 | `				PH7_HashmapInsert(pMap,` |
|     5604 |  5192 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2802 |  5193 | `					&pEntry[1]` |
|        - |  5194 | `				);` |
|        - |  5195 | `			}` |
|        - |  5196 | `			/* Next pair on the stack */` |
|     5762 |  5197 | `			pEntry += 2;` |
|        2 |  5198 | `		}` |
|        - |  5199 | `		/* Pop P1 elements */` |
|     2808 |  5200 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     2808 |  5201 | `		if( rcSpread != SXRET_OK ){` |
|        - |  5202 | `			/* Discard the partially-built map and propagate the exception. */` |
|       20 |  5203 | `			PH7_HashmapRelease(pMap,TRUE);` |
|       20 |  5204 | `			if( rcSpread == PH7_ABORT ){` |
|      ! 0 |  5205 | `				goto Abort;` |
|        - |  5206 | `			}` |
|        - |  5207 | `			{` |
|       20 |  5208 | `				VmFrame *pFrm2 = pVm->pFrame;` |
|       20 |  5209 | `				if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        6 |  5210 | `					pc = pFrm2->iExceptionJump - 1;` |
|        6 |  5211 | `					break;` |
|        - |  5212 | `				}` |
|        - |  5213 | `			}` |
|       15 |  5214 | `			goto Exception;` |
|        - |  5215 | `		}` |
|     1394 |  5216 | `	}` |
|        - |  5217 | `	/* Push the hashmap */` |
|    45912 |  5218 | `	pTos++;` |
|    45912 |  5219 | `	pTos->nIdx = SXU32_HIGH;` |
|    45912 |  5220 | `	pTos->x.pOther = pMap;` |
|    45912 |  5221 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    45912 |  5222 | `	break;` |
|        - |  5223 | `					  }` |
|        - |  5224 | `/*` |
|        - |  5225 | ` * LOAD_LIST: P1 * *` |
|        - |  5226 | ` *` |
|        - |  5227 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  5228 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  5229 | ` * Caveats:` |
|        - |  5230 | ` *  This implementation support only a single nesting level.` |
|        - |  5231 | ` */` |
|       48 |  5232 | `case PH7_OP_LOAD_LIST: {` |
|        - |  5233 | `	ph7_value *pEntry;` |
|       98 |  5234 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  5235 | `		/* Empty list,break immediately */` |
|      ! 0 |  5236 | `		break;` |
|        - |  5237 | `	}` |
|       98 |  5238 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  5239 | `#ifdef UNTRUST` |
|        - |  5240 | `	if( &pEntry[-1] < pStack ){` |
|        - |  5241 | `		goto Abort;` |
|        - |  5242 | `	}` |
|        - |  5243 | `#endif` |
|       98 |  5244 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       91 |  5245 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  5246 | `		ph7_hashmap_node *pNode;` |
|        - |  5247 | `		ph7_value sKey,*pObj;` |
|        - |  5248 | `		/* Start Copying */` |
|       91 |  5249 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      283 |  5250 | `		while( pEntry <= pTos ){` |
|      193 |  5251 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|      165 |  5252 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|      165 |  5253 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|      165 |  5254 | `					if( rc == SXRET_OK ){` |
|        - |  5255 | `						/* Store node value */` |
|      165 |  5256 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       83 |  5257 | `					}else{` |
|        - |  5258 | `						/* Undefined array key */` |
|        - |  5259 | `						char zMsg[128];` |
|      ! 0 |  5260 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|      ! 0 |  5261 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  5262 | `						PH7_MemObjRelease(pObj);` |
|        - |  5263 | `					}` |
|       82 |  5264 | `				}` |
|       82 |  5265 | `			}` |
|      193 |  5266 | `			sKey.x.iVal++; /* Next numeric index */` |
|      193 |  5267 | `			pEntry++;` |
|        1 |  5268 | `		}` |
|       46 |  5269 | `	}else{` |
|        - |  5270 | `		/* Source is not an array */` |
|        - |  5271 | `		ph7_value *pObj;` |
|       18 |  5272 | `		while( pEntry <= pTos ){` |
|       12 |  5273 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|       12 |  5274 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       12 |  5275 | `					PH7_MemObjRelease(pObj);` |
|        5 |  5276 | `				}` |
|        5 |  5277 | `			}` |
|       12 |  5278 | `			pEntry++;` |
|        2 |  5279 | `		}` |
|        8 |  5280 | `		if( (pTos[-pInstr->iP1].iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - |  5281 | `			/* Emit PHP-compatible warning with type name */` |
|        3 |  5282 | `			const char *zType = "unknown";` |
|        3 |  5283 | `			sxi32 iFlags = pTos[-pInstr->iP1].iFlags;` |
|        - |  5284 | `			char zMsg[256];` |
|        3 |  5285 | `			if( iFlags & MEMOBJ_STRING ){` |
|        3 |  5286 | `				zType = "string";` |
|        1 |  5287 | `			}else if( iFlags & MEMOBJ_INT ){` |
|      ! 0 |  5288 | `				zType = "int";` |
|      ! 0 |  5289 | `			}else if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5290 | `				zType = "float";` |
|      ! 0 |  5291 | `			}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  5292 | `				zType = "object";` |
|      ! 0 |  5293 | `			}else if( iFlags & MEMOBJ_RES ){` |
|      ! 0 |  5294 | `				zType = "resource";` |
|      ! 0 |  5295 | `			}` |
|        3 |  5296 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|        3 |  5297 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|        1 |  5298 | `		}` |
|        - |  5299 | `	}` |
|       98 |  5300 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       98 |  5301 | `	break;` |
|        - |  5302 | `					   }` |
|        - |  5303 | `/*` |
|        - |  5304 | ` * LOAD_IDX: P1 P2 *` |
|        - |  5305 | ` *` |
|        - |  5306 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  5307 | ` * from the stack.` |
|        - |  5308 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  5309 | ` * instead.` |
|        - |  5310 | ` */` |
|   251351 |  5311 | `case PH7_OP_LOAD_IDX: {` |
|   502748 |  5312 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   502748 |  5313 | `	ph7_hashmap *pMap = 0;` |
|        - |  5314 | `	ph7_value *pIdx;` |
|   502748 |  5315 | `	pIdx = 0;` |
|   502748 |  5316 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  5317 | `		if( !pInstr->iP2){` |
|        - |  5318 | `			/* No available index,load NULL */` |
|      ! 0 |  5319 | `			if( pTos >= pStack ){` |
|      ! 0 |  5320 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5321 | `			}else{` |
|        - |  5322 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  5323 | `				pTos++;` |
|      ! 0 |  5324 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  5325 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  5326 | `			}` |
|        - |  5327 | `			/* Emit a notice */` |
|      ! 0 |  5328 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  5329 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  5330 | `			break;` |
|        - |  5331 | `		}` |
|      ! 0 |  5332 | `	}else{` |
|   502748 |  5333 | `		pIdx = pTos;` |
|   502748 |  5334 | `		pTos--;` |
|        - |  5335 | `	}` |
|   502748 |  5336 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  5337 | `		/* String access */` |
|   388236 |  5338 | `		if( pIdx ){` |
|        - |  5339 | `			sxu32 nOfft;` |
|   388236 |  5340 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  5341 | `				/* Force an int cast */` |
|      ! 0 |  5342 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  5343 | `			}` |
|   388236 |  5344 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   388236 |  5345 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  5346 | `				/* Invalid offset,load null */` |
|      ! 0 |  5347 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5348 | `			}else{` |
|   388236 |  5349 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   388236 |  5350 | `				int c = zData[nOfft];` |
|   388236 |  5351 | `				PH7_MemObjRelease(pTos);` |
|   388236 |  5352 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   388236 |  5353 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  5354 | `			}` |
|   194141 |  5355 | `		}else{` |
|        - |  5356 | `			/* No available index,load NULL */` |
|      ! 0 |  5357 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  5358 | `		}` |
|   388236 |  5359 | `		break;` |
|        - |  5360 | `	}` |
|   114514 |  5361 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5362 | `		/* Object subscript: ArrayAccess dispatch.` |
|        - |  5363 | `		 * iP2 codes:` |
|        - |  5364 | `		 *   0 = read       → offsetGet` |
|        - |  5365 | `		 *   3 = ?? peek    → offsetExists; offsetGet on hit; arm coalesce` |
|        - |  5366 | `		 *                    target on miss for the upcoming NULLC_STORE` |
|        - |  5367 | `		 *   4 = isset()    → offsetExists` |
|        - |  5368 | `		 *   5 = unset()    → offsetUnset` |
|        - |  5369 | `		 *   6 = empty()    → offsetExists, then offsetGet on hit */` |
|      126 |  5370 | `		ph7_class_instance *pInst = (ph7_class_instance *)pTos->x.pOther;` |
|      126 |  5371 | `		ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|      126 |  5372 | `		if( pArrayAccess && pInst && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|        - |  5373 | `			ph7_class_method *pMeth;` |
|        - |  5374 | `			ph7_value sResult;` |
|        - |  5375 | `			ph7_value *apArg[1];` |
|      124 |  5376 | `			if( (pInstr->iP2 == 0 \|\| pInstr->iP2 == 3) && pIdx == 0 ){` |
|        - |  5377 | ``				/* `$obj[]` read — PHP rejects this. */`` |
|      ! 0 |  5378 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  5379 | `					"Cannot use [] for reading");` |
|      ! 0 |  5380 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5381 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5382 | `				break;` |
|        - |  5383 | `			}` |
|      124 |  5384 | `			PH7_MemObjInit(&(*pVm),&sResult);` |
|      124 |  5385 | `			if( pInstr->iP2 == 4 \|\| pInstr->iP2 == 6 \|\| pInstr->iP2 == 3 ){` |
|        - |  5386 | `				/* isset, empty, and ??= all start with offsetExists. */` |
|       51 |  5387 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5388 | `					"offsetExists",sizeof("offsetExists")-1);` |
|       51 |  5389 | `				apArg[0] = pIdx;` |
|       51 |  5390 | `				if( pMeth ){` |
|       51 |  5391 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|       26 |  5392 | `				}` |
|       99 |  5393 | `			}else if( pInstr->iP2 == 5 ){` |
|        9 |  5394 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5395 | `					"offsetUnset",sizeof("offsetUnset")-1);` |
|        9 |  5396 | `				apArg[0] = pIdx;` |
|        9 |  5397 | `				if( pMeth ){` |
|        9 |  5398 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|        4 |  5399 | `				}` |
|        5 |  5400 | `			}else{` |
|       66 |  5401 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5402 | `					"offsetGet",sizeof("offsetGet")-1);` |
|       66 |  5403 | `				apArg[0] = pIdx;` |
|       66 |  5404 | `				if( pMeth ){` |
|       66 |  5405 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|       32 |  5406 | `				}` |
|        - |  5407 | `			}` |
|      124 |  5408 | `			if( pInstr->iP2 == 4 ){` |
|        - |  5409 | `				/* isset: push MEMOBJ_BOOL so vm_builtin_isset reports the` |
|        - |  5410 | `				 * right truth value AND skips its "Expecting a variable not` |
|        - |  5411 | `				 * a constant" warning (keyed on MEMOBJ_BOOL). */` |
|       33 |  5412 | `				int bExists = ph7_value_to_bool(&sResult);` |
|       33 |  5413 | `				PH7_MemObjRelease(pTos);` |
|       33 |  5414 | `				pTos->nIdx = SXU32_HIGH;` |
|       33 |  5415 | `				if( bExists ){` |
|       17 |  5416 | `					MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       17 |  5417 | `					pTos->x.iVal = 1;` |
|        9 |  5418 | `				}else{` |
|       17 |  5419 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        1 |  5420 | `				}` |
|      108 |  5421 | `			}else if( pInstr->iP2 == 5 ){` |
|        - |  5422 | `				/* offsetUnset return is discarded; push NULL so the trailing` |
|        - |  5423 | `				 * vm_builtin_unset is a harmless no-op. */` |
|        9 |  5424 | `				PH7_MemObjRelease(pTos);` |
|        9 |  5425 | `				pTos->nIdx = SXU32_HIGH;` |
|        9 |  5426 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|       88 |  5427 | `			}else if( pInstr->iP2 == 6 ){` |
|        - |  5428 | `				/* empty: if offsetExists is false, push NULL so empty=true` |
|        - |  5429 | `				 * without calling offsetGet. If true, call offsetGet and` |
|        - |  5430 | `				 * push the value so PH7_builtin_empty evaluates emptiness. */` |
|       11 |  5431 | `				int bExists = ph7_value_to_bool(&sResult);` |
|       11 |  5432 | `				PH7_MemObjRelease(&sResult);` |
|       11 |  5433 | `				PH7_MemObjRelease(pTos);` |
|       11 |  5434 | `				pTos->nIdx = SXU32_HIGH;` |
|       11 |  5435 | `				if( !bExists ){` |
|        3 |  5436 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        2 |  5437 | `				}else{` |
|        9 |  5438 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5439 | `						"offsetGet",sizeof("offsetGet")-1);` |
|        - |  5440 | `					ph7_value sValue;` |
|        9 |  5441 | `					PH7_MemObjInit(&(*pVm),&sValue);` |
|        9 |  5442 | `					apArg[0] = pIdx;` |
|        9 |  5443 | `					if( pGet ){` |
|        9 |  5444 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|        4 |  5445 | `					}` |
|        9 |  5446 | `					PH7_MemObjStore(&sValue,pTos);` |
|        9 |  5447 | `					PH7_MemObjRelease(&sValue);` |
|        - |  5448 | `				}` |
|       11 |  5449 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|       11 |  5450 | `				break; /* skip the duplicate sResult release below */` |
|       74 |  5451 | `			}else if( pInstr->iP2 == 3 ){` |
|        - |  5452 | `				/* ?? null-coalesce peek: emulate PHP semantics —` |
|        - |  5453 | `				 *   if !offsetExists OR offsetGet() === null → arm` |
|        - |  5454 | `				 *     coalesce slot (NULLC_STORE will call offsetSet)` |
|        - |  5455 | `				 *     and push NULL.` |
|        - |  5456 | `				 *   else → push offsetGet's value (NULLC_JMP skips). */` |
|        9 |  5457 | `				int bExists = ph7_value_to_bool(&sResult);` |
|        9 |  5458 | `				int bShouldArm = !bExists;` |
|        - |  5459 | `				ph7_value sValue;` |
|        9 |  5460 | `				PH7_MemObjRelease(&sResult);` |
|        - |  5461 | `				/* Reset any prior arming defensively */` |
|        9 |  5462 | `				VmCoalesceDisarm(pVm);` |
|        9 |  5463 | `				PH7_MemObjInit(&(*pVm),&sValue);` |
|        9 |  5464 | `				if( bExists ){` |
|        5 |  5465 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5466 | `						"offsetGet",sizeof("offsetGet")-1);` |
|        5 |  5467 | `					apArg[0] = pIdx;` |
|        5 |  5468 | `					if( pGet ){` |
|        5 |  5469 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|        2 |  5470 | `					}` |
|        5 |  5471 | `					if( sValue.iFlags & MEMOBJ_NULL ){` |
|        3 |  5472 | `						bShouldArm = 1;` |
|        1 |  5473 | `					}` |
|        2 |  5474 | `				}` |
|        9 |  5475 | `				PH7_MemObjRelease(pTos);` |
|        9 |  5476 | `				pTos->nIdx = SXU32_HIGH;` |
|        9 |  5477 | `				if( bShouldArm ){` |
|        - |  5478 | `					/* Arm: remember (object, key) so NULLC_STORE dispatches` |
|        - |  5479 | `					 * to offsetSet. Hold a ref on the instance to survive` |
|        - |  5480 | `					 * intervening expression evaluation. */` |
|        7 |  5481 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        7 |  5482 | `					if( pIdx ){` |
|        7 |  5483 | `						PH7_MemObjStore(pIdx,&pVm->sCoalesceKey);` |
|        3 |  5484 | `					}` |
|        7 |  5485 | `					pVm->pCoalesceObj = pInst;` |
|        7 |  5486 | `					pInst->iRef++;` |
|        7 |  5487 | `					pVm->bCoalesceArmed = 1;` |
|        4 |  5488 | `				}else{` |
|        3 |  5489 | `					PH7_MemObjStore(&sValue,pTos);` |
|        - |  5490 | `				}` |
|        9 |  5491 | `				PH7_MemObjRelease(&sValue);` |
|        9 |  5492 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|        9 |  5493 | `				break;` |
|      ! 0 |  5494 | `			}else{` |
|        - |  5495 | `				/* offsetGet: replace pTos with the returned value. */` |
|       66 |  5496 | `				PH7_MemObjRelease(pTos);` |
|       66 |  5497 | `				PH7_MemObjStore(&sResult,pTos);` |
|       66 |  5498 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  5499 | `			}` |
|      106 |  5500 | `			PH7_MemObjRelease(&sResult);` |
|      106 |  5501 | `			if( pIdx ){` |
|      106 |  5502 | `				PH7_MemObjRelease(pIdx);` |
|       52 |  5503 | `			}` |
|      106 |  5504 | `			break;` |
|        - |  5505 | `		}` |
|        - |  5506 | `		/* Object without ArrayAccess: PHP throws fatal Error in all subscript` |
|        - |  5507 | `		 * contexts (read, isset, unset, empty). Match it. */` |
|        3 |  5508 | `		if( pInst ){` |
|        - |  5509 | `			char zMsg[256];` |
|        3 |  5510 | `			SyString *pName = &pInst->pClass->sName;` |
|        4 |  5511 | `			sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - |  5512 | `				"Cannot use object of type %.*s as array",` |
|        2 |  5513 | `				(int)pName->nByte,pName->zString);` |
|        3 |  5514 | `			rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|        3 |  5515 | `			if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|        3 |  5516 | `			PH7_MemObjRelease(pTos);` |
|        3 |  5517 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  5518 | `			if( rc == SXERR_ABORT ){ goto Abort; }` |
|        3 |  5519 | `			break;` |
|        - |  5520 | `		}` |
|      ! 0 |  5521 | `	}` |
|   114390 |  5522 | `	if( (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  5523 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  5524 | `			ph7_value *pObj;` |
|        3 |  5525 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  5526 | `				PH7_MemObjToHashmap(pObj);` |
|        3 |  5527 | `				PH7_MemObjLoad(pObj,pTos);` |
|        1 |  5528 | `			}` |
|        1 |  5529 | `		}` |
|        1 |  5530 | `	}` |
|   114390 |  5531 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|   114390 |  5532 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|   114390 |  5533 | `		if( pInstr->iP2 == 1 \|\| pInstr->iP2 == 5 ){` |
|        - |  5534 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  5535 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  5536 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  5537 | `			 * NOT separate — that would defeat COW on every element read.` |
|        - |  5538 | `			 * iP2=5 is unset-context, treated like iP2=1 for arrays so the` |
|        - |  5539 | `			 * trailing unset() builtin can drop the slot via pTos->nIdx. */` |
|      896 |  5540 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      447 |  5541 | `		}` |
|        - |  5542 | `		/* Point to the hashmap */` |
|   114390 |  5543 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   114390 |  5544 | `		if( pIdx ){` |
|        - |  5545 | `			/* Load the desired entry */` |
|   114390 |  5546 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    57194 |  5547 | `		}` |
|   114390 |  5548 | `		if( pInstr->iP2 == 3 ){` |
|        - |  5549 | `			/* Null coalescing assign peek mode: separate only when we will` |
|        - |  5550 | `			 * actually write back. If the looked-up value is non-null, the` |
|        - |  5551 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|        - |  5552 | `			 * the parent can stay shared. If the value is null or the key is` |
|        - |  5553 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|        - |  5554 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|        - |  5555 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|        - |  5556 | `			 * correct for the outermost write. */` |
|       19 |  5557 | `			int needWrite = (rc != SXRET_OK);` |
|       19 |  5558 | `			if( !needWrite && pNode ){` |
|       13 |  5559 | `				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|       13 |  5560 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|        7 |  5561 | `					needWrite = 1;` |
|        3 |  5562 | `				}` |
|        6 |  5563 | `			}` |
|       19 |  5564 | `			if( needWrite ){` |
|       13 |  5565 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|       13 |  5566 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|        - |  5567 | `					/* The map was actually copied — re-lookup so pNode points` |
|        - |  5568 | `					 * into the new map's storage. */` |
|        7 |  5569 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        7 |  5570 | `					if( pIdx ){` |
|        7 |  5571 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|        3 |  5572 | `					}` |
|        3 |  5573 | `				}` |
|        6 |  5574 | `			}` |
|        9 |  5575 | `		}` |
|   114390 |  5576 | `		if( rc != SXRET_OK && (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) ){` |
|        - |  5577 | `			/* Create a new empty entry */` |
|      273 |  5578 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      273 |  5579 | `			if( rc == SXRET_OK ){` |
|        - |  5580 | `				/* Point to the last inserted entry */` |
|      273 |  5581 | `				pNode = pMap->pLast;` |
|      136 |  5582 | `			}` |
|      136 |  5583 | `		}` |
|    57194 |  5584 | `	}` |
|   114390 |  5585 | `	if( rc != SXRET_OK && pInstr->iP2 == 2 && pIdx ){` |
|        - |  5586 | `		/* List destructuring context: emit PHP-compatible warning for missing key */` |
|        - |  5587 | `		char zMsg[128];` |
|      ! 0 |  5588 | `		if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5589 | `			PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  5590 | `		}` |
|      ! 0 |  5591 | `		SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)pIdx->x.iVal);` |
|      ! 0 |  5592 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  5593 | `	}` |
|   114390 |  5594 | `	if( pIdx ){` |
|   114390 |  5595 | `		PH7_MemObjRelease(pIdx);` |
|    57194 |  5596 | `	}` |
|   114390 |  5597 | `	if( rc == SXRET_OK ){` |
|        - |  5598 | `		/* Load entry contents */` |
|    50684 |  5599 | `		if( pMap->iRef < 2 ){` |
|        - |  5600 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  5601 | `			 * of the entry value,rather than pointing to it.` |
|        - |  5602 | `			 */` |
|       28 |  5603 | `			pTos->nIdx = SXU32_HIGH;` |
|       28 |  5604 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       15 |  5605 | `		}else{` |
|    50658 |  5606 | `			pTos->nIdx = pNode->nValIdx;` |
|    50658 |  5607 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    50658 |  5608 | `			PH7_HashmapUnref(pMap);` |
|        - |  5609 | `		}` |
|    25343 |  5610 | `	}else{` |
|        - |  5611 | `		/* No such entry,load NULL */` |
|    63708 |  5612 | `		PH7_MemObjRelease(pTos);` |
|    63708 |  5613 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  5614 | `	}` |
|   114390 |  5615 | `	break;` |
|        - |  5616 | `					  }` |
|        - |  5617 | `/*` |
|        - |  5618 | ` * LOAD_CLOSURE * * P3` |
|        - |  5619 | ` *` |
|        - |  5620 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  5621 | ` * name in the stack.` |
|        - |  5622 | ` */` |
|       64 |  5623 | `case PH7_OP_LOAD_CLOSURE:{` |
|      130 |  5624 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|      130 |  5625 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  5626 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  5627 | `		ph7_vm_func *pClosure;` |
|        - |  5628 | `		char *zName;` |
|        - |  5629 | `		sxu32 mLen;` |
|        - |  5630 | `		sxu32 n;` |
|        - |  5631 | `		/* Create a new VM function */` |
|      130 |  5632 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  5633 | `		/* Generate an unique closure name */` |
|      130 |  5634 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|      130 |  5635 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  5636 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  5637 | `			goto Abort;` |
|        - |  5638 | `		}` |
|      130 |  5639 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      130 |  5640 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  5641 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  5642 | `		}` |
|        - |  5643 | `		/* Zero the stucture */` |
|      130 |  5644 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  5645 | `		/* Perform a structure assignment on read-only items */` |
|      130 |  5646 | `		pClosure->aArgs = pFunc->aArgs;` |
|      130 |  5647 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|      130 |  5648 | `		pClosure->aStatic = pFunc->aStatic;` |
|      130 |  5649 | `		pClosure->iFlags = pFunc->iFlags;` |
|      130 |  5650 | `		pClosure->pUserData = pFunc->pUserData;` |
|      130 |  5651 | `		pClosure->sSignature = pFunc->sSignature;` |
|      130 |  5652 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|      130 |  5653 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|      130 |  5654 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|      130 |  5655 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|      130 |  5656 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  5657 | `		/* Register the closure */` |
|      130 |  5658 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  5659 | `		/* Set up closure environment */` |
|      130 |  5660 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|      130 |  5661 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      324 |  5662 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  5663 | `			ph7_value *pValue;` |
|      196 |  5664 | `			pEnv = &aEnv[n];` |
|      196 |  5665 | `			sEnv.sName  = pEnv->sName;` |
|      196 |  5666 | `			sEnv.iFlags = pEnv->iFlags;` |
|      196 |  5667 | `			sEnv.nIdx = SXU32_HIGH;` |
|      196 |  5668 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|      196 |  5669 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  5670 | `				/* Pass by reference */` |
|      ! 0 |  5671 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  5672 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  5673 | `					);` |
|      ! 0 |  5674 | `			}` |
|        - |  5675 | `			/* Standard pass by value */` |
|      196 |  5676 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|      196 |  5677 | `			if( pValue ){` |
|        - |  5678 | `				/* Copy imported value */` |
|       72 |  5679 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|       35 |  5680 | `			}` |
|        - |  5681 | `			/* Insert the imported variable */` |
|      196 |  5682 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|       99 |  5683 | `		}` |
|        - |  5684 | `		/* Finally,load the closure name on the stack */` |
|      130 |  5685 | `		pTos++;` |
|      130 |  5686 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|       64 |  5687 | `	}` |
|      130 |  5688 | `	break;` |
|        - |  5689 | `						 }` |
|        - |  5690 | `/*` |
|        - |  5691 | ` * STORE * P2 P3` |
|        - |  5692 | ` *` |
|        - |  5693 | ` * Perform a store (Assignment) operation.` |
|        - |  5694 | ` */` |
|   147145 |  5695 | `case PH7_OP_STORE: {` |
|        - |  5696 | `	ph7_value *pObj;` |
|        - |  5697 | `	SyString sName;` |
|        - |  5698 | `#ifdef UNTRUST` |
|        - |  5699 | `	if( pTos < pStack ){` |
|        - |  5700 | `		goto Abort;` |
|        - |  5701 | `	}` |
|        - |  5702 | `#endif` |
|   294292 |  5703 | `	if( pInstr->iP2 ){` |
|        - |  5704 | `		sxu32 nIdx;` |
|        - |  5705 | `		sxi32 rcT;` |
|        - |  5706 | `		/* Member store operation */` |
|     5398 |  5707 | `		nIdx = pTos->nIdx;` |
|     5398 |  5708 | `		VmPopOperand(&pTos,1);` |
|     5398 |  5709 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  5710 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5711 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  5712 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  5713 | `		}else{` |
|        - |  5714 | `			/* Enforce typed property declaration if any. May coerce the` |
|        - |  5715 | `			 * incoming value in place (weak mode) or throw TypeError. */` |
|     5394 |  5716 | `			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos);` |
|     5394 |  5717 | `			if( rcT == PH7_ABORT ){` |
|      ! 0 |  5718 | `				goto Abort;` |
|        - |  5719 | `			}` |
|     5394 |  5720 | `			if( rcT == PH7_EXCEPTION ){` |
|        - |  5721 | `				/* TypeError was thrown. Pop the rejected rvalue and hand` |
|        - |  5722 | `				 * control to the nearest catch block if any, otherwise` |
|        - |  5723 | `				 * propagate out of the VM loop. */` |
|       40 |  5724 | `				VmPopOperand(&pTos,1);` |
|        - |  5725 | `				{` |
|       40 |  5726 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       40 |  5727 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|       40 |  5728 | `						pc = pFrm2->iExceptionJump - 1;` |
|   147166 |  5729 | `						break;` |
|        - |  5730 | `					}` |
|        - |  5731 | `				}` |
|      ! 0 |  5732 | `				goto Exception;` |
|        - |  5733 | `			}` |
|        - |  5734 | `			/* Point to the desired memory object */` |
|     5356 |  5735 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     5356 |  5736 | `			if( pObj ){` |
|        - |  5737 | `				/* Perform the store operation */` |
|     5356 |  5738 | `				PH7_MemObjStore(pTos,pObj);` |
|     2677 |  5739 | `			}` |
|        - |  5740 | `		}` |
|     5360 |  5741 | `		break;` |
|   288896 |  5742 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  5743 | `		/* Take the variable name from the next on the stack */` |
|        7 |  5744 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5745 | `			/* Force a string cast */` |
|      ! 0 |  5746 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5747 | `		}` |
|        7 |  5748 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  5749 | `		pTos--;` |
|        - |  5750 | `#ifdef UNTRUST` |
|        - |  5751 | `		if( pTos < pStack  ){` |
|        - |  5752 | `			goto Abort;` |
|        - |  5753 | `		}` |
|        - |  5754 | `#endif` |
|        4 |  5755 | `	}else{` |
|   288890 |  5756 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5757 | `	}` |
|        - |  5758 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   288896 |  5759 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   288896 |  5760 | `	if( pObj == 0 ){` |
|      ! 0 |  5761 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5762 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  5763 | `		goto Abort;` |
|        - |  5764 | `	}` |
|   288896 |  5765 | `	if( !pInstr->p3 ){` |
|        7 |  5766 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  5767 | `	}` |
|        - |  5768 | `	/* Perform the store operation */` |
|   288896 |  5769 | `	PH7_MemObjStore(pTos,pObj);` |
|   288896 |  5770 | `	break;` |
|        - |  5771 | `				   }` |
|        - |  5772 | `/*` |
|        - |  5773 | ` * STORE_IDX:   P1 * P3` |
|        - |  5774 | ` * STORE_IDX_R: P1 * P3` |
|        - |  5775 | ` *` |
|        - |  5776 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  5777 | ` */` |
|    97627 |  5778 | `case PH7_OP_STORE_IDX:` |
|        - |  5779 | `case PH7_OP_STORE_IDX_REF: {` |
|   195256 |  5780 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  5781 | `	ph7_value *pKey;` |
|        - |  5782 | `	sxu32 nIdx;` |
|   195256 |  5783 | `	if( pInstr->iP1 ){` |
|        - |  5784 | `		/* Key is next on stack */` |
|    63534 |  5785 | `		pKey = pTos;` |
|    63534 |  5786 | `		pTos--;` |
|    31768 |  5787 | `	}else{` |
|   131724 |  5788 | `		pKey = 0;` |
|        - |  5789 | `	}` |
|   195256 |  5790 | `	nIdx = pTos->nIdx;` |
|        - |  5791 | `	{` |
|        - |  5792 | `		/* ArrayAccess::offsetSet dispatch.` |
|        - |  5793 | `		 * Container may be on the stack as MEMOBJ_OBJ, or referenced via` |
|        - |  5794 | `		 * the backing variable slot at nIdx. */` |
|   195256 |  5795 | `		ph7_class_instance *pInst = 0;` |
|   195256 |  5796 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       34 |  5797 | `			pInst = (ph7_class_instance *)pTos->x.pOther;` |
|   195240 |  5798 | `		}else if( nIdx != SXU32_HIGH ){` |
|   195224 |  5799 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   195224 |  5800 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_OBJ) ){` |
|      ! 0 |  5801 | `				pInst = (ph7_class_instance *)pBacking->x.pOther;` |
|      ! 0 |  5802 | `			}` |
|    97611 |  5803 | `		}` |
|   195256 |  5804 | `		if( pInst ){` |
|       34 |  5805 | `			ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|       34 |  5806 | `			if( pArrayAccess && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|        - |  5807 | `				ph7_class_method *pMeth;` |
|        - |  5808 | `				ph7_value sNullKey;` |
|        - |  5809 | `				ph7_value *apArg[2];` |
|       32 |  5810 | `				if( pInstr->iOp == PH7_OP_STORE_IDX_REF ){` |
|      ! 0 |  5811 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5812 | `						"Cannot assign by reference to overloaded object");` |
|      ! 0 |  5813 | `					if( pKey ){ PH7_MemObjRelease(pKey); }` |
|      ! 0 |  5814 | `					VmPopOperand(&pTos,2); /* container + value */` |
|      ! 0 |  5815 | `					break;` |
|        - |  5816 | `				}` |
|       32 |  5817 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5818 | `					"offsetSet",sizeof("offsetSet")-1);` |
|        - |  5819 | `				/* Pop container; pTos now points to the value */` |
|       32 |  5820 | `				VmPopOperand(&pTos,1);` |
|       32 |  5821 | `				if( pKey == 0 ){` |
|        7 |  5822 | `					PH7_MemObjInit(&(*pVm),&sNullKey);` |
|        7 |  5823 | `					apArg[0] = &sNullKey;` |
|        4 |  5824 | `				}else{` |
|       26 |  5825 | `					apArg[0] = pKey;` |
|        - |  5826 | `				}` |
|       32 |  5827 | `				apArg[1] = pTos;` |
|       32 |  5828 | `				if( pMeth ){` |
|       32 |  5829 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,0,2,apArg);` |
|       15 |  5830 | `				}` |
|       32 |  5831 | `				if( pKey ){` |
|       26 |  5832 | `					PH7_MemObjRelease(pKey);` |
|       14 |  5833 | `				}else{` |
|        7 |  5834 | `					PH7_MemObjRelease(&sNullKey);` |
|        - |  5835 | `				}` |
|        - |  5836 | `				/* Pop the value */` |
|       32 |  5837 | `				VmPopOperand(&pTos,1);` |
|       32 |  5838 | `				break;` |
|        - |  5839 | `			}` |
|        - |  5840 | `			/* Object without ArrayAccess: PHP throws a fatal Error rather` |
|        - |  5841 | `			 * than silently coercing the object into a hashmap (which is` |
|        - |  5842 | `			 * what the legacy PH7 fall-through would do via MemObjToHashmap` |
|        - |  5843 | `			 * a few lines below). Match PHP. */` |
|        - |  5844 | `			{` |
|        - |  5845 | `				char zMsg[256];` |
|        3 |  5846 | `				SyString *pName = &pInst->pClass->sName;` |
|        4 |  5847 | `				sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - |  5848 | `					"Cannot use object of type %.*s as array",` |
|        2 |  5849 | `					(int)pName->nByte,pName->zString);` |
|        3 |  5850 | `				rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|        3 |  5851 | `				if( pKey ){ PH7_MemObjRelease(pKey); }` |
|        3 |  5852 | `				VmPopOperand(&pTos,2); /* container + value */` |
|        3 |  5853 | `				if( rc == SXERR_ABORT ){ goto Abort; }` |
|        3 |  5854 | `				break;` |
|        - |  5855 | `			}` |
|        - |  5856 | `		}` |
|        - |  5857 | `	}` |
|   195224 |  5858 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5859 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  5860 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  5861 | `		 * checking true sharing count, then re-add after separation. */` |
|   195172 |  5862 | `		if( nIdx != SXU32_HIGH ){` |
|   195172 |  5863 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   292757 |  5864 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   195172 |  5865 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5866 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  5867 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  5868 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  5869 | `				 * refcounts if the backing array was already separated. */` |
|   195172 |  5870 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   195172 |  5871 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   195172 |  5872 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   195172 |  5873 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   195172 |  5874 | `					pTos->x.pOther = pMap;` |
|    97587 |  5875 | `				}else{` |
|        - |  5876 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  5877 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  5878 | `					pMap = pCur;` |
|        - |  5879 | `				}` |
|    97587 |  5880 | `			}else{` |
|      ! 0 |  5881 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5882 | `			}` |
|    97587 |  5883 | `		}else{` |
|      ! 0 |  5884 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5885 | `		}` |
|   195172 |  5886 | `		if( pMap->iRef < 2 ){` |
|        - |  5887 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  5888 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  5889 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  5890 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  5891 | `			pMap->iRef = 2;` |
|      ! 0 |  5892 | `		}` |
|    97587 |  5893 | `	}else{` |
|        - |  5894 | `		ph7_value *pObj;` |
|       53 |  5895 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  5896 | `		if( pObj == 0 ){` |
|      ! 0 |  5897 | `			if( pKey ){` |
|      ! 0 |  5898 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  5899 | `			}` |
|      ! 0 |  5900 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5901 | `			break;` |
|        - |  5902 | `		}` |
|        - |  5903 | `		/* Phase#1: Load the array */` |
|       53 |  5904 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  5905 | `			VmPopOperand(&pTos,1);` |
|       53 |  5906 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  5907 | `				/* Force a string cast */` |
|      ! 0 |  5908 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  5909 | `			}` |
|       53 |  5910 | `			if( pKey == 0 ){` |
|        - |  5911 | `				/* Append string */` |
|        3 |  5912 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  5913 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  5914 | `				}` |
|        2 |  5915 | `			}else{` |
|        - |  5916 | `				sxu32 nOfft;` |
|       51 |  5917 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  5918 | `					/* Force an int cast */` |
|       51 |  5919 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  5920 | `				}` |
|       51 |  5921 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  5922 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  5923 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  5924 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  5925 | `					zData[nOfft] = zBlob[0];` |
|       26 |  5926 | `				}else{` |
|      ! 0 |  5927 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  5928 | `						/* Perform an append operation */` |
|      ! 0 |  5929 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  5930 | `					}` |
|        - |  5931 | `				}` |
|        - |  5932 | `			}` |
|       53 |  5933 | `			if( pKey ){` |
|       51 |  5934 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  5935 | `			}` |
|       53 |  5936 | `			break;` |
|      ! 0 |  5937 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  5938 | `			/* Force a hashmap cast  */` |
|      ! 0 |  5939 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  5940 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  5941 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  5942 | `				goto Abort;` |
|        - |  5943 | `			}` |
|      ! 0 |  5944 | `		}` |
|        - |  5945 | `		/* COW separate the backing variable before mutation */` |
|      ! 0 |  5946 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|        - |  5947 | `	}` |
|   195172 |  5948 | `	VmPopOperand(&pTos,1);` |
|        - |  5949 | `	/* Phase#2: Perform the insertion */` |
|   195172 |  5950 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  5951 | `		/* Insertion by reference */` |
|       15 |  5952 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  5953 | `	}else{` |
|   195158 |  5954 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  5955 | `	}` |
|   195172 |  5956 | `	if( pKey ){` |
|    63458 |  5957 | `		PH7_MemObjRelease(pKey);` |
|    31728 |  5958 | `	}` |
|   195172 |  5959 | `	break;` |
|        - |  5960 | `					   }` |
|        - |  5961 | `/*` |
|        - |  5962 | ` * INCR: P1 * *` |
|        - |  5963 | ` *` |
|        - |  5964 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  5965 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  5966 | ` * the stack and increment after that.` |
|        - |  5967 | ` */` |
|   167956 |  5968 | `case PH7_OP_INCR:` |
|        - |  5969 | `#ifdef UNTRUST` |
|        - |  5970 | `	if( pTos < pStack ){` |
|        - |  5971 | `		goto Abort;` |
|        - |  5972 | `	}` |
|        - |  5973 | `#endif` |
|   335958 |  5974 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   335958 |  5975 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  5976 | `			ph7_value *pObj;` |
|   335958 |  5977 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|   335958 |  5978 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - |  5979 | `					/* Perl-style string increment.` |
|        - |  5980 | `					 * Post-increment: pTos may alias pObj's buffer via SXBLOB_RDONLY` |
|        - |  5981 | `					 * (set by PH7_MemObjLoad).  Force ownership so the upcoming` |
|        - |  5982 | `					 * mutation of pObj doesn't bleed into pTos's old-value view. */` |
|       49 |  5983 | `					if( pInstr->iP1 == 0 ){` |
|       45 |  5984 | `						SyBlobNullAppend(&pTos->sBlob);` |
|       22 |  5985 | `					}` |
|       49 |  5986 | `					PH7_MemObjStringIncrement(pObj);` |
|       49 |  5987 | `					if( pInstr->iP1 ){` |
|        - |  5988 | `						/* Pre-increment: deep-copy pObj into pTos. */` |
|        5 |  5989 | `						PH7_MemObjStore(pObj,pTos);` |
|        2 |  5990 | `					}` |
|       25 |  5991 | `				}else{` |
|        - |  5992 | `					/* Numeric coercion. Post-increment must preserve pTos's` |
|        - |  5993 | `					 * original value: pTos may alias pObj's blob via` |
|        - |  5994 | `					 * SXBLOB_RDONLY (set by PH7_MemObjLoad), and` |
|        - |  5995 | `					 * PH7_MemObjToNumeric calls SyBlobRelease on a STRING` |
|        - |  5996 | `					 * pObj. Force pTos to take ownership of its blob first` |
|        - |  5997 | `					 * so its old-value view survives the coercion. */` |
|   335910 |  5998 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|       13 |  5999 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        6 |  6000 | `					}` |
|        - |  6001 | `					/* Force a numeric cast on the variable */` |
|   335910 |  6002 | `					PH7_MemObjToNumeric(pObj);` |
|   335910 |  6003 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        9 |  6004 | `						pObj->rVal++;` |
|        - |  6005 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - |  6006 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - |  6007 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - |  6008 | `						 * integer-valued real. */` |
|        9 |  6009 | `						PH7_MemObjTryInteger(pObj);` |
|        5 |  6010 | `					}else{` |
|   335902 |  6011 | `						pObj->x.iVal++;` |
|        - |  6012 | `					}` |
|   335910 |  6013 | `					if( pInstr->iP1 ){` |
|        - |  6014 | `						/* Pre-increment: result is the new value. */` |
|       83 |  6015 | `						PH7_MemObjStore(pObj,pTos);` |
|       41 |  6016 | `					}` |
|        - |  6017 | `					/* Post-increment: pTos retains the old value (a string` |
|        - |  6018 | `					 * for "5"++, an int/float for direct numeric operands). */` |
|        - |  6019 | `				}` |
|   168000 |  6020 | `			}` |
|   168002 |  6021 | `		}else{` |
|      ! 0 |  6022 | `			if( pInstr->iP1 ){` |
|      ! 0 |  6023 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|      ! 0 |  6024 | `					PH7_MemObjStringIncrement(pTos);` |
|      ! 0 |  6025 | `				}else{` |
|        - |  6026 | `					/* Force a numeric cast */` |
|      ! 0 |  6027 | `					PH7_MemObjToNumeric(pTos);` |
|        - |  6028 | `					/* Pre-increment */` |
|      ! 0 |  6029 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  6030 | `						pTos->rVal++;` |
|        - |  6031 | `						/* Try to get an integer representation */` |
|      ! 0 |  6032 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  6033 | `					}else{` |
|      ! 0 |  6034 | `						pTos->x.iVal++;` |
|      ! 0 |  6035 | `						MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  6036 | `					}` |
|        - |  6037 | `				}` |
|      ! 0 |  6038 | `			}` |
|        - |  6039 | `		}` |
|   168000 |  6040 | `	}` |
|   335958 |  6041 | `	break;` |
|        - |  6042 | `/*` |
|        - |  6043 | ` * DECR: P1 * *` |
|        - |  6044 | ` *` |
|        - |  6045 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  6046 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  6047 | ` * and decrement after that.` |
|        - |  6048 | ` */` |
|       14 |  6049 | `case PH7_OP_DECR:` |
|        - |  6050 | `#ifdef UNTRUST` |
|        - |  6051 | `	if( pTos < pStack ){` |
|        - |  6052 | `		goto Abort;` |
|        - |  6053 | `	}` |
|        - |  6054 | `#endif` |
|        - |  6055 | ``	/* NULL stays excluded: PHP leaves `--` on null untouched (no-op). */`` |
|       29 |  6056 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|       27 |  6057 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  6058 | `			ph7_value *pObj;` |
|       27 |  6059 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       27 |  6060 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - |  6061 | ``					/* PHP has no string decrement: `--` on a non-numeric string`` |
|        - |  6062 | ``					 * is a no-op (unlike `++`, which is Perl-style). Leave pObj`` |
|        - |  6063 | `					 * unchanged; the result is simply that unchanged value. */` |
|        7 |  6064 | `					if( pInstr->iP1 ){` |
|        - |  6065 | `						/* Pre-decrement: result is the (unchanged) value. */` |
|      ! 0 |  6066 | `						PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  6067 | `					}` |
|        - |  6068 | `					/* Post-decrement: pTos already holds the old value. */` |
|        4 |  6069 | `				}else{` |
|        - |  6070 | `					/* Numeric coercion. Mirror INCR's aliasing care: a` |
|        - |  6071 | `					 * post-decrement must preserve pTos's original value, which` |
|        - |  6072 | `					 * may alias pObj's blob via SXBLOB_RDONLY (PH7_MemObjLoad).` |
|        - |  6073 | `					 * Force pTos to own its blob before coercing pObj. */` |
|       21 |  6074 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        5 |  6075 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        2 |  6076 | `					}` |
|       21 |  6077 | `					PH7_MemObjToNumeric(pObj);` |
|       21 |  6078 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        9 |  6079 | `						pObj->rVal--;` |
|        - |  6080 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - |  6081 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - |  6082 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - |  6083 | `						 * integer-valued real. */` |
|        9 |  6084 | `						PH7_MemObjTryInteger(pObj);` |
|        5 |  6085 | `					}else{` |
|       13 |  6086 | `						pObj->x.iVal--;` |
|        - |  6087 | `					}` |
|       21 |  6088 | `					if( pInstr->iP1 ){` |
|        - |  6089 | `						/* Pre-decrement: result is the new value. */` |
|        3 |  6090 | `						PH7_MemObjStore(pObj,pTos);` |
|        1 |  6091 | `					}` |
|        - |  6092 | `					/* Post-decrement: pTos retains the old value. */` |
|        - |  6093 | `				}` |
|       13 |  6094 | `			}` |
|       14 |  6095 | `		}else{` |
|      ! 0 |  6096 | `			if( pInstr->iP1 ){` |
|      ! 0 |  6097 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|        - |  6098 | `					/* Non-numeric string, no lvalue: no-op (value unchanged). */` |
|      ! 0 |  6099 | `				}else{` |
|        - |  6100 | `					/* Force a numeric cast */` |
|      ! 0 |  6101 | `					PH7_MemObjToNumeric(pTos);` |
|        - |  6102 | `					/* Pre-decrement */` |
|      ! 0 |  6103 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  6104 | `						pTos->rVal--;` |
|        - |  6105 | `						/* Keep the cached int consistent with the new rVal. */` |
|      ! 0 |  6106 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  6107 | `					}else{` |
|      ! 0 |  6108 | `						pTos->x.iVal--;` |
|      ! 0 |  6109 | `						MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  6110 | `					}` |
|        - |  6111 | `				}` |
|      ! 0 |  6112 | `			}` |
|        - |  6113 | `		}` |
|       13 |  6114 | `	}` |
|       29 |  6115 | `	break;` |
|        - |  6116 | `/*` |
|        - |  6117 | ` * UMINUS: * * *` |
|        - |  6118 | ` *` |
|        - |  6119 | ` * Perform a unary minus operation.` |
|        - |  6120 | ` */` |
|    29929 |  6121 | `case PH7_OP_UMINUS:` |
|        - |  6122 | `#ifdef UNTRUST` |
|        - |  6123 | `	if( pTos < pStack ){` |
|        - |  6124 | `		goto Abort;` |
|        - |  6125 | `	}` |
|        - |  6126 | `#endif` |
|        - |  6127 | `	/* Force a numeric (integer,real or both) cast */` |
|    59860 |  6128 | `	PH7_MemObjToNumeric(pTos);` |
|    59860 |  6129 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       31 |  6130 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  6131 | `	}` |
|    59860 |  6132 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    59830 |  6133 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    29914 |  6134 | `	}` |
|    59860 |  6135 | `	break;` |
|        - |  6136 | `/*` |
|        - |  6137 | ` * UPLUS: * * *` |
|        - |  6138 | ` *` |
|        - |  6139 | ` * Perform a unary plus operation.` |
|        - |  6140 | ` */` |
|       18 |  6141 | `case PH7_OP_UPLUS:` |
|        - |  6142 | `#ifdef UNTRUST` |
|        - |  6143 | `	if( pTos < pStack ){` |
|        - |  6144 | `		goto Abort;` |
|        - |  6145 | `	}` |
|        - |  6146 | `#endif` |
|        - |  6147 | `	/* Force a numeric (integer,real or both) cast */` |
|       37 |  6148 | `	PH7_MemObjToNumeric(pTos);` |
|       37 |  6149 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  6150 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  6151 | `	}` |
|       37 |  6152 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       37 |  6153 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       18 |  6154 | `	}` |
|       37 |  6155 | `	break;` |
|        - |  6156 | `/*` |
|        - |  6157 | ` * OP_LNOT: * * *` |
|        - |  6158 | ` *` |
|        - |  6159 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  6160 | ` * with its complement.` |
|        - |  6161 | ` */` |
|    45059 |  6162 | `case PH7_OP_LNOT:` |
|        - |  6163 | `#ifdef UNTRUST` |
|        - |  6164 | `	if( pTos < pStack ){` |
|        - |  6165 | `		goto Abort;` |
|        - |  6166 | `	}` |
|        - |  6167 | `#endif` |
|        - |  6168 | `	/* Force a boolean cast */` |
|    90164 |  6169 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       23 |  6170 | `		PH7_MemObjToBool(pTos);` |
|       11 |  6171 | `	}` |
|    90164 |  6172 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    90164 |  6173 | `	break;` |
|        - |  6174 | `/*` |
|        - |  6175 | ` * OP_BITNOT: * * *` |
|        - |  6176 | ` *` |
|        - |  6177 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  6178 | ` * with its ones-complement.` |
|        - |  6179 | ` */` |
|       14 |  6180 | `case PH7_OP_BITNOT:` |
|        - |  6181 | `#ifdef UNTRUST` |
|        - |  6182 | `	if( pTos < pStack ){` |
|        - |  6183 | `		goto Abort;` |
|        - |  6184 | `	}` |
|        - |  6185 | `#endif` |
|        - |  6186 | `	/* Force an integer cast */` |
|       30 |  6187 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6188 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6189 | `	}` |
|       30 |  6190 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       30 |  6191 | `	break;` |
|        - |  6192 | `/* OP_MUL * * *` |
|        - |  6193 | ` * OP_MUL_STORE * * *` |
|        - |  6194 | ` *` |
|        - |  6195 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  6196 | ` * and push the result back onto the stack.` |
|        - |  6197 | ` */` |
|     1296 |  6198 | `case PH7_OP_MUL:` |
|        - |  6199 | `case PH7_OP_MUL_STORE: {` |
|     2594 |  6200 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6201 | `	/* Force the operand to be numeric */` |
|        - |  6202 | `#ifdef UNTRUST` |
|        - |  6203 | `	if( pNos < pStack ){` |
|        - |  6204 | `		goto Abort;` |
|        - |  6205 | `	}` |
|        - |  6206 | `#endif` |
|     2594 |  6207 | `	PH7_MemObjToNumeric(pTos);` |
|     2594 |  6208 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  6209 | `	/* Perform the requested operation */` |
|     2594 |  6210 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  6211 | `		/* Floating point arithemic */` |
|        - |  6212 | `		ph7_real a,b,r;` |
|       21 |  6213 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        9 |  6214 | `			PH7_MemObjToReal(pTos);` |
|        4 |  6215 | `		}` |
|       21 |  6216 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  6217 | `			PH7_MemObjToReal(pNos);` |
|        3 |  6218 | `		}` |
|       21 |  6219 | `		a = pNos->rVal;` |
|       21 |  6220 | `		b = pTos->rVal;` |
|       21 |  6221 | `		r = a * b;` |
|        - |  6222 | `		/* Push the result */` |
|       21 |  6223 | `		pNos->rVal = r;` |
|       21 |  6224 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6225 | `		/* Try to get an integer representation */` |
|       21 |  6226 | `		PH7_MemObjTryInteger(pNos);` |
|       11 |  6227 | `	}else{` |
|        - |  6228 | `		/* Integer arithmetic */` |
|        - |  6229 | `		sxi64 a,b,r;` |
|     2574 |  6230 | `		a = pNos->x.iVal;` |
|     2574 |  6231 | `		b = pTos->x.iVal;` |
|     2574 |  6232 | `		r = a * b;` |
|        - |  6233 | `		/* Push the result */` |
|     2574 |  6234 | `		pNos->x.iVal = r;` |
|     2574 |  6235 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  6236 | `	}` |
|     2594 |  6237 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  6238 | `		ph7_value *pObj;` |
|       32 |  6239 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6240 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       32 |  6241 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       32 |  6242 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       32 |  6243 | `			PH7_MemObjStore(pNos,pObj);` |
|       15 |  6244 | `		}` |
|       15 |  6245 | `	}` |
|     2594 |  6246 | `	VmPopOperand(&pTos,1);` |
|     2594 |  6247 | `	break;` |
|        - |  6248 | `				 }` |
|        - |  6249 | `/* OP_POW * * *` |
|        - |  6250 | ` * OP_POW_STORE * * *` |
|        - |  6251 | ` *` |
|        - |  6252 | ` * Pop the top two elements from the stack, raise the second to the` |
|        - |  6253 | ` * power of the first, and push the result. PHP semantics: int**int` |
|        - |  6254 | ` * stays integer iff the exponent is non-negative and the exact result` |
|        - |  6255 | ` * fits in sxi64; otherwise the result is a double.` |
|        - |  6256 | ` */` |
|       67 |  6257 | `case PH7_OP_POW:` |
|        - |  6258 | `case PH7_OP_POW_STORE: {` |
|      135 |  6259 | `	ph7_value *pNos = &pTos[-1];` |
|      135 |  6260 | `	int bStore = (pInstr->iOp == PH7_OP_POW_STORE);` |
|        - |  6261 | `	/* Operand order convention (matches DIV/SUB_STORE):` |
|        - |  6262 | `	 *   POW:       base = pNos (evaluated first),   exp = pTos` |
|        - |  6263 | `	 *   POW_STORE: base = pTos (lvalue, last),       exp = pNos` |
|        - |  6264 | `	 */` |
|      135 |  6265 | `	ph7_value *pBase = bStore ? pTos : pNos;` |
|      135 |  6266 | `	ph7_value *pExp  = bStore ? pNos : pTos;` |
|        - |  6267 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |  6268 | `	int bBothInt;` |
|      135 |  6269 | `	int usedInt = 0;` |
|        - |  6270 | `	ph7_real a, b, r;` |
|        - |  6271 | `#endif` |
|      135 |  6272 | `	sxi64 base_i = 0, exp_i = 0;` |
|        - |  6273 | `#ifdef UNTRUST` |
|        - |  6274 | `	if( pNos < pStack ){` |
|        - |  6275 | `		goto Abort;` |
|        - |  6276 | `	}` |
|        - |  6277 | `#endif` |
|      135 |  6278 | `	PH7_MemObjToNumeric(pTos);` |
|      135 |  6279 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  6280 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      265 |  6281 | `	bBothInt = ((pTos->iFlags & MEMOBJ_REAL) == 0) &&` |
|      130 |  6282 | `	           ((pNos->iFlags & MEMOBJ_REAL) == 0);` |
|      135 |  6283 | `	if( bBothInt ){` |
|      123 |  6284 | `		base_i = pBase->x.iVal;` |
|      123 |  6285 | `		exp_i  = pExp->x.iVal;` |
|       61 |  6286 | `	}` |
|      135 |  6287 | `	if( (pBase->iFlags & MEMOBJ_REAL) == 0 ){` |
|      125 |  6288 | `		PH7_MemObjToReal(pBase);` |
|       62 |  6289 | `	}` |
|      135 |  6290 | `	if( (pExp->iFlags & MEMOBJ_REAL) == 0 ){` |
|      133 |  6291 | `		PH7_MemObjToReal(pExp);` |
|       66 |  6292 | `	}` |
|      135 |  6293 | `	a = pBase->rVal;` |
|      135 |  6294 | `	b = pExp->rVal;` |
|      135 |  6295 | `	r = pow(a, b);` |
|        - |  6296 | `	/* Match PHP: int**non-negative-int stays int when the exact result` |
|        - |  6297 | `	 * fits in sxi64. Use exponentiation by squaring with overflow checks` |
|        - |  6298 | `	 * rather than casting the double back, because the boundary 2^63 is` |
|        - |  6299 | `	 * representable as double but not as signed int64. */` |
|      135 |  6300 | `	if( bBothInt && exp_i >= 0 ){` |
|      117 |  6301 | `		sxi64 result_i = 1;` |
|      117 |  6302 | `		sxi64 cur_base = base_i;` |
|      117 |  6303 | `		sxi64 cur_exp  = exp_i;` |
|      117 |  6304 | `		int overflow = 0;` |
|      401 |  6305 | `		while( cur_exp > 0 ){` |
|      289 |  6306 | `			if( cur_exp & 1 ){` |
|      189 |  6307 | `				if( VmMulOverflow64(result_i, cur_base, &result_i) ){` |
|        3 |  6308 | `					overflow = 1;` |
|        3 |  6309 | `					break;` |
|        - |  6310 | `				}` |
|       93 |  6311 | `			}` |
|      287 |  6312 | `			cur_exp >>= 1;` |
|      287 |  6313 | `			if( cur_exp > 0 ){` |
|      181 |  6314 | `				if( VmMulOverflow64(cur_base, cur_base, &cur_base) ){` |
|        3 |  6315 | `					overflow = 1;` |
|        3 |  6316 | `					break;` |
|        - |  6317 | `				}` |
|       89 |  6318 | `			}` |
|        1 |  6319 | `		}` |
|      117 |  6320 | `		if( !overflow ){` |
|      113 |  6321 | `			pNos->x.iVal = result_i;` |
|      113 |  6322 | `			MemObjSetType(pNos, MEMOBJ_INT);` |
|      113 |  6323 | `			usedInt = 1;` |
|       56 |  6324 | `		}` |
|       58 |  6325 | `	}` |
|      135 |  6326 | `	if( !usedInt ){` |
|       23 |  6327 | `		pNos->rVal = r;` |
|       23 |  6328 | `		MemObjSetType(pNos, MEMOBJ_REAL);` |
|       11 |  6329 | `	}` |
|        - |  6330 | `#else` |
|        - |  6331 | `	/* PH7_OMIT_FLOATING_POINT: integer-only build. No libm / no pow().` |
|        - |  6332 | `	 * Exponentiation by squaring with silent wrap on overflow, matching` |
|        - |  6333 | `	 * the integer-wrap semantics of PH7_OP_MUL in the same build mode.` |
|        - |  6334 | `	 * Negative exponents yield 0 since fractional results cannot be` |
|        - |  6335 | `	 * represented. */` |
|        - |  6336 | `	base_i = pBase->x.iVal;` |
|        - |  6337 | `	exp_i  = pExp->x.iVal;` |
|        - |  6338 | `	{` |
|        - |  6339 | `		sxi64 result_i = 1;` |
|        - |  6340 | `		sxi64 cur_base = base_i;` |
|        - |  6341 | `		sxi64 cur_exp  = exp_i;` |
|        - |  6342 | `		if( cur_exp < 0 ){` |
|        - |  6343 | `			result_i = 0;` |
|        - |  6344 | `		}else{` |
|        - |  6345 | `			while( cur_exp > 0 ){` |
|        - |  6346 | `				if( cur_exp & 1 ){` |
|        - |  6347 | `					result_i *= cur_base;` |
|        - |  6348 | `				}` |
|        - |  6349 | `				cur_exp >>= 1;` |
|        - |  6350 | `				if( cur_exp > 0 ){` |
|        - |  6351 | `					cur_base *= cur_base;` |
|        - |  6352 | `				}` |
|        - |  6353 | `			}` |
|        - |  6354 | `		}` |
|        - |  6355 | `		pNos->x.iVal = result_i;` |
|        - |  6356 | `		MemObjSetType(pNos, MEMOBJ_INT);` |
|        - |  6357 | `	}` |
|        - |  6358 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      135 |  6359 | `	if( bStore ){` |
|        - |  6360 | `		ph7_value *pObj;` |
|       23 |  6361 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6362 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       23 |  6363 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       23 |  6364 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       23 |  6365 | `			PH7_MemObjStore(pNos,pObj);` |
|       11 |  6366 | `		}` |
|       11 |  6367 | `	}` |
|      135 |  6368 | `	VmPopOperand(&pTos,1);` |
|      135 |  6369 | `	break;` |
|        - |  6370 | `				 }` |
|        - |  6371 | `/* OP_ADD * * *` |
|        - |  6372 | ` *` |
|        - |  6373 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  6374 | ` * and push the result back onto the stack.` |
|        - |  6375 | ` */` |
|      536 |  6376 | `case PH7_OP_ADD:{` |
|     1074 |  6377 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6378 | `#ifdef UNTRUST` |
|        - |  6379 | `	if( pNos < pStack ){` |
|        - |  6380 | `		goto Abort;` |
|        - |  6381 | `	}` |
|        - |  6382 | `#endif` |
|        - |  6383 | `	/* Perform the addition */` |
|     1074 |  6384 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|     1074 |  6385 | `	VmPopOperand(&pTos,1);` |
|     1074 |  6386 | `	break;` |
|        - |  6387 | `				}` |
|        - |  6388 | `/*` |
|        - |  6389 | ` * OP_ADD_STORE * * *` |
|        - |  6390 | ` *` |
|        - |  6391 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  6392 | ` * and push the result back onto the stack.` |
|        - |  6393 | ` */` |
|      502 |  6394 | `case PH7_OP_ADD_STORE:{` |
|     1006 |  6395 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6396 | `	ph7_value *pObj;` |
|        - |  6397 | `	sxu32 nIdx;` |
|        - |  6398 | `#ifdef UNTRUST` |
|        - |  6399 | `	if( pNos < pStack ){` |
|        - |  6400 | `		goto Abort;` |
|        - |  6401 | `	}` |
|        - |  6402 | `#endif` |
|        - |  6403 | `	/* Perform the addition */` |
|     1006 |  6404 | `	nIdx = pTos->nIdx;` |
|     1006 |  6405 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  6406 | `	/* Peform the store operation */` |
|     1006 |  6407 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  6408 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     1006 |  6409 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|     1006 |  6410 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|     1006 |  6411 | `		PH7_MemObjStore(pTos,pObj);` |
|      502 |  6412 | `	}` |
|        - |  6413 | `	/* Ticket 1433-35: Perform a stack dup */` |
|     1006 |  6414 | `	PH7_MemObjStore(pTos,pNos);` |
|     1006 |  6415 | `	VmPopOperand(&pTos,1);` |
|     1006 |  6416 | `	break;` |
|        - |  6417 | `				}` |
|        - |  6418 | `/* OP_SUB * * *` |
|        - |  6419 | ` *` |
|        - |  6420 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  6421 | ` * first (what was next on the stack) from the second (the` |
|        - |  6422 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  6423 | ` */` |
|      352 |  6424 | `case PH7_OP_SUB: {` |
|      706 |  6425 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6426 | `#ifdef UNTRUST` |
|        - |  6427 | `	if( pNos < pStack ){` |
|        - |  6428 | `		goto Abort;` |
|        - |  6429 | `	}` |
|        - |  6430 | `#endif` |
|      706 |  6431 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  6432 | `		/* Floating point arithemic */` |
|        - |  6433 | `		ph7_real a,b,r;` |
|      103 |  6434 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  6435 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  6436 | `		}` |
|      103 |  6437 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  6438 | `			PH7_MemObjToReal(pNos);` |
|        2 |  6439 | `		}` |
|      103 |  6440 | `		a = pNos->rVal;` |
|      103 |  6441 | `		b = pTos->rVal;` |
|      103 |  6442 | `		r = a - b;` |
|        - |  6443 | `		/* Push the result */` |
|      103 |  6444 | `		pNos->rVal = r;` |
|      103 |  6445 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6446 | `		/* Try to get an integer representation */` |
|      103 |  6447 | `		PH7_MemObjTryInteger(pNos);` |
|       52 |  6448 | `	}else{` |
|        - |  6449 | `		/* Integer arithmetic */` |
|        - |  6450 | `		sxi64 a,b,r;` |
|      604 |  6451 | `		a = pNos->x.iVal;` |
|      604 |  6452 | `		b = pTos->x.iVal;` |
|      604 |  6453 | `		r = a - b;` |
|        - |  6454 | `		/* Push the result */` |
|      604 |  6455 | `		pNos->x.iVal = r;` |
|      604 |  6456 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  6457 | `	}` |
|      706 |  6458 | `	VmPopOperand(&pTos,1);` |
|      706 |  6459 | `	break;` |
|        - |  6460 | `				 }` |
|        - |  6461 | `/* OP_SUB_STORE * * *` |
|        - |  6462 | ` *` |
|        - |  6463 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  6464 | ` * first (what was next on the stack) from the second (the` |
|        - |  6465 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  6466 | ` */` |
|        4 |  6467 | `case PH7_OP_SUB_STORE: {` |
|       10 |  6468 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6469 | `	ph7_value *pObj;` |
|        - |  6470 | `#ifdef UNTRUST` |
|        - |  6471 | `	if( pNos < pStack ){` |
|        - |  6472 | `		goto Abort;` |
|        - |  6473 | `	}` |
|        - |  6474 | `#endif` |
|       10 |  6475 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  6476 | `		/* Floating point arithemic */` |
|        - |  6477 | `		ph7_real a,b,r;` |
|      ! 0 |  6478 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  6479 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  6480 | `		}` |
|      ! 0 |  6481 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  6482 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  6483 | `		}` |
|      ! 0 |  6484 | `		a = pTos->rVal;` |
|      ! 0 |  6485 | `		b = pNos->rVal;` |
|      ! 0 |  6486 | `		r = a - b;` |
|        - |  6487 | `		/* Push the result */` |
|      ! 0 |  6488 | `		pNos->rVal = r;` |
|      ! 0 |  6489 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6490 | `		/* Try to get an integer representation */` |
|      ! 0 |  6491 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  6492 | `	}else{` |
|        - |  6493 | `		/* Integer arithmetic */` |
|        - |  6494 | `		sxi64 a,b,r;` |
|       10 |  6495 | `		a = pTos->x.iVal;` |
|       10 |  6496 | `		b = pNos->x.iVal;` |
|       10 |  6497 | `		r = a - b;` |
|        - |  6498 | `		/* Push the result */` |
|       10 |  6499 | `		pNos->x.iVal = r;` |
|       10 |  6500 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  6501 | `	}` |
|       10 |  6502 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6503 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       10 |  6504 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       10 |  6505 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       10 |  6506 | `		PH7_MemObjStore(pNos,pObj);` |
|        4 |  6507 | `	}` |
|       10 |  6508 | `	VmPopOperand(&pTos,1);` |
|       10 |  6509 | `	break;` |
|        - |  6510 | `				 }` |
|        - |  6511 |  |
|        - |  6512 | `/*` |
|        - |  6513 | ` * OP_MOD * * *` |
|        - |  6514 | ` *` |
|        - |  6515 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6516 | ` * first (what was next on the stack) from the second (the` |
|        - |  6517 | ` * top of the stack) and push the remainder after division` |
|        - |  6518 | ` * onto the stack.` |
|        - |  6519 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  6520 | ` */` |
|      309 |  6521 | `case PH7_OP_MOD:{` |
|      620 |  6522 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6523 | `	sxi64 a,b,r;` |
|        - |  6524 | `#ifdef UNTRUST` |
|        - |  6525 | `	if( pNos < pStack ){` |
|        - |  6526 | `		goto Abort;` |
|        - |  6527 | `	}` |
|        - |  6528 | `#endif` |
|        - |  6529 | `	/* Force the operands to be integer */` |
|      620 |  6530 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6531 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6532 | `	}` |
|      620 |  6533 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  6534 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  6535 | `	}` |
|        - |  6536 | `	/* Perform the requested operation */` |
|      620 |  6537 | `	a = pNos->x.iVal;` |
|      620 |  6538 | `	b = pTos->x.iVal;` |
|      620 |  6539 | `	if( b == 0 ){` |
|        3 |  6540 | `		r = 0;` |
|        3 |  6541 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  6542 | `		/* goto Abort; */` |
|        2 |  6543 | `	}else{` |
|      617 |  6544 | `		r = a%b;` |
|        - |  6545 | `	}` |
|        - |  6546 | `	/* Push the result */` |
|      620 |  6547 | `	pNos->x.iVal = r;` |
|      620 |  6548 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      620 |  6549 | `	VmPopOperand(&pTos,1);` |
|      620 |  6550 | `	break;` |
|        - |  6551 | `				}` |
|        - |  6552 | `/*` |
|        - |  6553 | ` * OP_MOD_STORE * * *` |
|        - |  6554 | ` *` |
|        - |  6555 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6556 | ` * first (what was next on the stack) from the second (the` |
|        - |  6557 | ` * top of the stack) and push the remainder after division` |
|        - |  6558 | ` * onto the stack.` |
|        - |  6559 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  6560 | ` */` |
|        1 |  6561 | `case PH7_OP_MOD_STORE: {` |
|        3 |  6562 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6563 | `	ph7_value *pObj;` |
|        - |  6564 | `	sxi64 a,b,r;` |
|        - |  6565 | `#ifdef UNTRUST` |
|        - |  6566 | `	if( pNos < pStack ){` |
|        - |  6567 | `		goto Abort;` |
|        - |  6568 | `	}` |
|        - |  6569 | `#endif` |
|        - |  6570 | `	/* Force the operands to be integer */` |
|        3 |  6571 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6572 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6573 | `	}` |
|        3 |  6574 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6575 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6576 | `	}` |
|        - |  6577 | `	/* Perform the requested operation */` |
|        3 |  6578 | `	a = pTos->x.iVal;` |
|        3 |  6579 | `	b = pNos->x.iVal;` |
|        3 |  6580 | `	if( b == 0 ){` |
|      ! 0 |  6581 | `		r = 0;` |
|      ! 0 |  6582 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  6583 | `		/* goto Abort; */` |
|      ! 0 |  6584 | `	}else{` |
|        3 |  6585 | `		r = a%b;` |
|        - |  6586 | `	}` |
|        - |  6587 | `	/* Push the result */` |
|        3 |  6588 | `	pNos->x.iVal = r;` |
|        3 |  6589 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  6590 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6591 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  6592 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  6593 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        3 |  6594 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  6595 | `	}` |
|        3 |  6596 | `	VmPopOperand(&pTos,1);` |
|        3 |  6597 | `	break;` |
|        - |  6598 | `				}` |
|        - |  6599 | `/*` |
|        - |  6600 | ` * OP_DIV * * *` |
|        - |  6601 | ` *` |
|        - |  6602 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6603 | ` * first (what was next on the stack) from the second (the` |
|        - |  6604 | ` * top of the stack) and push the result onto the stack.` |
|        - |  6605 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  6606 | ` */` |
|       33 |  6607 | `case PH7_OP_DIV:{` |
|       68 |  6608 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6609 | `	ph7_real a,b,r;` |
|        - |  6610 | `#ifdef UNTRUST` |
|        - |  6611 | `	if( pNos < pStack ){` |
|        - |  6612 | `		goto Abort;` |
|        - |  6613 | `	}` |
|        - |  6614 | `#endif` |
|        - |  6615 | `	/* Force the operands to be real */` |
|       68 |  6616 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       62 |  6617 | `		PH7_MemObjToReal(pTos);` |
|       30 |  6618 | `	}` |
|       68 |  6619 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       28 |  6620 | `		PH7_MemObjToReal(pNos);` |
|       13 |  6621 | `	}` |
|        - |  6622 | `	/* Perform the requested operation */` |
|       68 |  6623 | `	a = pNos->rVal;` |
|       68 |  6624 | `	b = pTos->rVal;` |
|       68 |  6625 | `	if( b == 0 ){` |
|        - |  6626 | `		/* Division by zero */` |
|        3 |  6627 | `		pNos->rVal = 0;` |
|        3 |  6628 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  6629 | `		/* goto Abort; */` |
|        2 |  6630 | `	}else{` |
|       65 |  6631 | `		r = a/b;` |
|        - |  6632 | `		/* Push the result */` |
|       65 |  6633 | `		pNos->rVal = r;` |
|       65 |  6634 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6635 | `		/* Try to get an integer representation */` |
|       65 |  6636 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  6637 | `	}` |
|       68 |  6638 | `	VmPopOperand(&pTos,1);` |
|       68 |  6639 | `	break;` |
|        - |  6640 | `				}` |
|        - |  6641 | `/*` |
|        - |  6642 | ` * OP_DIV_STORE * * *` |
|        - |  6643 | ` *` |
|        - |  6644 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6645 | ` * first (what was next on the stack) from the second (the` |
|        - |  6646 | ` * top of the stack) and push the result onto the stack.` |
|        - |  6647 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  6648 | ` */` |
|        2 |  6649 | `case PH7_OP_DIV_STORE:{` |
|        5 |  6650 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6651 | `	ph7_value *pObj;` |
|        - |  6652 | `	ph7_real a,b,r;` |
|        - |  6653 | `#ifdef UNTRUST` |
|        - |  6654 | `	if( pNos < pStack ){` |
|        - |  6655 | `		goto Abort;` |
|        - |  6656 | `	}` |
|        - |  6657 | `#endif` |
|        - |  6658 | `	/* Force the operands to be real */` |
|        5 |  6659 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  6660 | `		PH7_MemObjToReal(pTos);` |
|        2 |  6661 | `	}` |
|        5 |  6662 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  6663 | `		PH7_MemObjToReal(pNos);` |
|        2 |  6664 | `	}` |
|        - |  6665 | `	/* Perform the requested operation */` |
|        5 |  6666 | `	a = pTos->rVal;` |
|        5 |  6667 | `	b = pNos->rVal;` |
|        5 |  6668 | `	if( b == 0 ){` |
|        - |  6669 | `		/* Division by zero */` |
|      ! 0 |  6670 | `		r = 0;` |
|      ! 0 |  6671 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  6672 | `		/* goto Abort; */` |
|      ! 0 |  6673 | `	}else{` |
|        5 |  6674 | `		r = a/b;` |
|        - |  6675 | `		/* Push the result */` |
|        5 |  6676 | `		pNos->rVal = r;` |
|        5 |  6677 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6678 | `		/* Try to get an integer representation */` |
|        5 |  6679 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  6680 | `	}` |
|        5 |  6681 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6682 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        5 |  6683 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        5 |  6684 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        5 |  6685 | `		PH7_MemObjStore(pNos,pObj);` |
|        2 |  6686 | `	}` |
|        5 |  6687 | `	VmPopOperand(&pTos,1);` |
|        5 |  6688 | `	break;` |
|        - |  6689 | `				}` |
|        - |  6690 | `/* OP_BAND * * *` |
|        - |  6691 | ` *` |
|        - |  6692 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6693 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  6694 | ` * two elements.` |
|        - |  6695 | `*/` |
|        - |  6696 | `/* OP_BOR * * *` |
|        - |  6697 | ` *` |
|        - |  6698 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6699 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  6700 | ` * two elements.` |
|        - |  6701 | ` */` |
|        - |  6702 | `/* OP_BXOR * * *` |
|        - |  6703 | ` *` |
|        - |  6704 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6705 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  6706 | ` * two elements.` |
|        - |  6707 | ` */` |
|       43 |  6708 | `case PH7_OP_BAND:` |
|        - |  6709 | `case PH7_OP_BOR:` |
|        - |  6710 | `case PH7_OP_BXOR:{` |
|       88 |  6711 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6712 | `	sxi64 a,b,r;` |
|        - |  6713 | `#ifdef UNTRUST` |
|        - |  6714 | `	if( pNos < pStack ){` |
|        - |  6715 | `		goto Abort;` |
|        - |  6716 | `	}` |
|        - |  6717 | `#endif` |
|        - |  6718 | `	/* Force the operands to be integer */` |
|       88 |  6719 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6720 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6721 | `	}` |
|       88 |  6722 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6723 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6724 | `	}` |
|        - |  6725 | `	/* Perform the requested operation */` |
|       88 |  6726 | `	a = pNos->x.iVal;` |
|       88 |  6727 | `	b = pTos->x.iVal;` |
|       88 |  6728 | `	switch(pInstr->iOp){` |
|        7 |  6729 | `	case PH7_OP_BOR_STORE:` |
|       15 |  6730 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        7 |  6731 | `	case PH7_OP_BXOR_STORE:` |
|       15 |  6732 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       29 |  6733 | `	case PH7_OP_BAND_STORE:` |
|       29 |  6734 | `	case PH7_OP_BAND:` |
|       60 |  6735 | `	default:          r = a&b; break;` |
|        - |  6736 | `	}` |
|        - |  6737 | `	/* Push the result */` |
|       88 |  6738 | `	pNos->x.iVal = r;` |
|       88 |  6739 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       88 |  6740 | `	VmPopOperand(&pTos,1);` |
|       88 |  6741 | `	break;` |
|        - |  6742 | `				 }` |
|        - |  6743 | `/* OP_BAND_STORE * * *` |
|        - |  6744 | ` *` |
|        - |  6745 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6746 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  6747 | ` * two elements.` |
|        - |  6748 | `*/` |
|        - |  6749 | `/* OP_BOR_STORE * * *` |
|        - |  6750 | ` *` |
|        - |  6751 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6752 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  6753 | ` * two elements.` |
|        - |  6754 | ` */` |
|        - |  6755 | `/* OP_BXOR_STORE * * *` |
|        - |  6756 | ` *` |
|        - |  6757 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6758 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  6759 | ` * two elements.` |
|        - |  6760 | ` */` |
|       10 |  6761 | `case PH7_OP_BAND_STORE:` |
|        - |  6762 | `case PH7_OP_BOR_STORE:` |
|        - |  6763 | `case PH7_OP_BXOR_STORE:{` |
|       21 |  6764 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6765 | `	ph7_value *pObj;` |
|        - |  6766 | `	sxi64 a,b,r;` |
|        - |  6767 | `#ifdef UNTRUST` |
|        - |  6768 | `	if( pNos < pStack ){` |
|        - |  6769 | `		goto Abort;` |
|        - |  6770 | `	}` |
|        - |  6771 | `#endif` |
|        - |  6772 | `	/* Force the operands to be integer */` |
|       21 |  6773 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6774 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6775 | `	}` |
|       21 |  6776 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6777 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6778 | `	}` |
|        - |  6779 | `	/* Perform the requested operation */` |
|       21 |  6780 | `	a = pTos->x.iVal;` |
|       21 |  6781 | `	b = pNos->x.iVal;` |
|       21 |  6782 | `	switch(pInstr->iOp){` |
|        3 |  6783 | `	case PH7_OP_BOR_STORE:` |
|        7 |  6784 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        4 |  6785 | `	case PH7_OP_BXOR_STORE:` |
|        9 |  6786 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        3 |  6787 | `	case PH7_OP_BAND_STORE:` |
|        3 |  6788 | `	case PH7_OP_BAND:` |
|        7 |  6789 | `	default:          r = a&b; break;` |
|        - |  6790 | `	}` |
|        - |  6791 | `	/* Push the result */` |
|       21 |  6792 | `	pNos->x.iVal = r;` |
|       21 |  6793 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       21 |  6794 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6795 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       21 |  6796 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       21 |  6797 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       21 |  6798 | `		PH7_MemObjStore(pNos,pObj);` |
|       10 |  6799 | `	}` |
|       21 |  6800 | `	VmPopOperand(&pTos,1);` |
|       21 |  6801 | `	break;` |
|        - |  6802 | `				 }` |
|        - |  6803 | `/* OP_SHL * * *` |
|        - |  6804 | ` *` |
|        - |  6805 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6806 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6807 | ` * left by N bits where N is the top element on the stack.` |
|        - |  6808 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6809 | ` */` |
|        - |  6810 | `/* OP_SHR * * *` |
|        - |  6811 | ` *` |
|        - |  6812 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6813 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6814 | ` * right by N bits where N is the top element on the stack.` |
|        - |  6815 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6816 | ` */` |
|       12 |  6817 | `case PH7_OP_SHL:` |
|        - |  6818 | `case PH7_OP_SHR: {` |
|       25 |  6819 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6820 | `	sxi64 a,r;` |
|        - |  6821 | `	sxi32 b;` |
|        - |  6822 | `#ifdef UNTRUST` |
|        - |  6823 | `	if( pNos < pStack ){` |
|        - |  6824 | `		goto Abort;` |
|        - |  6825 | `	}` |
|        - |  6826 | `#endif` |
|        - |  6827 | `	/* Force the operands to be integer */` |
|       25 |  6828 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6829 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6830 | `	}` |
|       25 |  6831 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6832 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6833 | `	}` |
|        - |  6834 | `	/* Perform the requested operation */` |
|       25 |  6835 | `	a = pNos->x.iVal;` |
|       25 |  6836 | `	b = (sxi32)pTos->x.iVal;` |
|       25 |  6837 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       15 |  6838 | `		r = a << b;` |
|        8 |  6839 | `	}else{` |
|       11 |  6840 | `		r = a >> b;` |
|        - |  6841 | `	}` |
|        - |  6842 | `	/* Push the result */` |
|       25 |  6843 | `	pNos->x.iVal = r;` |
|       25 |  6844 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       25 |  6845 | `	VmPopOperand(&pTos,1);` |
|       25 |  6846 | `	break;` |
|        - |  6847 | `				 }` |
|        - |  6848 | `/*  OP_SHL_STORE * * *` |
|        - |  6849 | ` *` |
|        - |  6850 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6851 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6852 | ` * left by N bits where N is the top element on the stack.` |
|        - |  6853 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6854 | ` */` |
|        - |  6855 | `/* OP_SHR_STORE * * *` |
|        - |  6856 | ` *` |
|        - |  6857 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6858 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6859 | ` * right by N bits where N is the top element on the stack.` |
|        - |  6860 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6861 | ` */` |
|        9 |  6862 | `case PH7_OP_SHL_STORE:` |
|        - |  6863 | `case PH7_OP_SHR_STORE: {` |
|       19 |  6864 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6865 | `	ph7_value *pObj;` |
|        - |  6866 | `	sxi64 a,r;` |
|        - |  6867 | `	sxi32 b;` |
|        - |  6868 | `#ifdef UNTRUST` |
|        - |  6869 | `	if( pNos < pStack ){` |
|        - |  6870 | `		goto Abort;` |
|        - |  6871 | `	}` |
|        - |  6872 | `#endif` |
|        - |  6873 | `	/* Force the operands to be integer */` |
|       19 |  6874 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6875 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6876 | `	}` |
|       19 |  6877 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6878 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6879 | `	}` |
|        - |  6880 | `	/* Perform the requested operation */` |
|       19 |  6881 | `	a = pTos->x.iVal;` |
|       19 |  6882 | `	b = (sxi32)pNos->x.iVal;` |
|       19 |  6883 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        9 |  6884 | `		r = a << b;` |
|        5 |  6885 | `	}else{` |
|       11 |  6886 | `		r = a >> b;` |
|        - |  6887 | `	}` |
|        - |  6888 | `	/* Push the result */` |
|       19 |  6889 | `	pNos->x.iVal = r;` |
|       19 |  6890 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  6891 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6892 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  6893 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  6894 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       19 |  6895 | `		PH7_MemObjStore(pNos,pObj);` |
|        9 |  6896 | `	}` |
|       19 |  6897 | `	VmPopOperand(&pTos,1);` |
|       19 |  6898 | `	break;` |
|        - |  6899 | `				 }` |
|        - |  6900 | `/* CAT:  P1 * *` |
|        - |  6901 | ` *` |
|        - |  6902 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  6903 | ` * back.` |
|        - |  6904 | ` */` |
|    72141 |  6905 | `case PH7_OP_CAT:{` |
|        - |  6906 | `	ph7_value *pNos,*pCur;` |
|   144284 |  6907 | `	if( pInstr->iP1 < 1 ){` |
|   116798 |  6908 | `		pNos = &pTos[-1];` |
|    58400 |  6909 | `	}else{` |
|    27488 |  6910 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  6911 | `	}` |
|        - |  6912 | `#ifdef UNTRUST` |
|        - |  6913 | `	if( pNos < pStack ){` |
|        - |  6914 | `		goto Abort;` |
|        - |  6915 | `	}` |
|        - |  6916 | `#endif` |
|        - |  6917 | `	/* Force a string cast */` |
|   144284 |  6918 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1672 |  6919 | `		PH7_MemObjToString(pNos);` |
|      835 |  6920 | `	}` |
|   144284 |  6921 | `	pCur = &pNos[1];` |
|   291296 |  6922 | `	while( pCur <= pTos ){` |
|   147014 |  6923 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50962 |  6924 | `			PH7_MemObjToString(pCur);` |
|    25480 |  6925 | `		}` |
|        - |  6926 | `		/* Perform the concatenation */` |
|   147014 |  6927 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   146970 |  6928 | `			if( PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob)) != SXRET_OK ){` |
|        - |  6929 | `				/* Allocation failure: raise a fatal instead of a truncated concat */` |
|      ! 0 |  6930 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 |  6931 | `				goto Abort;` |
|        - |  6932 | `			}` |
|    73484 |  6933 | `		}` |
|   147014 |  6934 | `		SyBlobRelease(&pCur->sBlob);` |
|   147014 |  6935 | `		pCur++;` |
|        2 |  6936 | `	}` |
|   144284 |  6937 | `	pTos = pNos;` |
|   144284 |  6938 | `	break;` |
|        - |  6939 | `				}` |
|        - |  6940 | `/*  CAT_STORE: * * *` |
|        - |  6941 | ` *` |
|        - |  6942 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  6943 | ` * back.` |
|        - |  6944 | ` */` |
|     4149 |  6945 | `case PH7_OP_CAT_STORE:{` |
|     8300 |  6946 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6947 | `	ph7_value *pObj;` |
|        - |  6948 | `	sxu32 nIdx;` |
|        - |  6949 | `#ifdef UNTRUST` |
|        - |  6950 | `	if( pNos < pStack ){` |
|        - |  6951 | `		goto Abort;` |
|        - |  6952 | `	}` |
|        - |  6953 | `#endif` |
|        - |  6954 | `	/* The right operand must be a string to append it */` |
|     8300 |  6955 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  6956 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  6957 | `	}` |
|     8300 |  6958 | `	nIdx = pTos->nIdx;` |
|        - |  6959 | `	/* Fast path: append straight into the lvalue's own (geometrically grown) buffer` |
|        - |  6960 | `	 * instead of copy-on-write-dup'ing the read-only-aliased stack value and then` |
|        - |  6961 | ``	 * storing the whole buffer back twice. This turns `$s .= ...` (and the`` |
|        - |  6962 | `	 * $a[$i] .= / $obj->prop .= forms) from O(n^2) into amortized O(1).` |
|        - |  6963 | `	 * Guards: a real owned slot; the right operand must NOT alias that same slot` |
|        - |  6964 | ``	 * (`$s .= $s`, or a reference to it, would realloc the buffer out from under`` |
|        - |  6965 | `	 * the source we copy from — references share the slot index, so one check` |
|        - |  6966 | `	 * covers both); and not a typed property, whose store-time type check/coercion` |
|        - |  6967 | `	 * must run before any mutation (left to the slow path).` |
|        - |  6968 | ``	 * NOTE: the explicit `$s = $s . x` form (OP_CAT + OP_STORE) is not covered here`` |
|        - |  6969 | `	 * and remains O(n^2) by design. */` |
|     8301 |  6970 | `	if( nIdx != SXU32_HIGH` |
|     8298 |  6971 | `	 && nIdx != pNos->nIdx` |
|     8294 |  6972 | `	 && (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0` |
|     8292 |  6973 | `	 && (SyHashTotalEntry(&pVm->hTypedSlot) == 0` |
|     4148 |  6974 | `	     \|\| SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32)) == 0) ){` |
|     8286 |  6975 | `		if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6976 | `			/* e.g. $x = 5; $x .= "a";  ->  "5a" */` |
|        3 |  6977 | `			PH7_MemObjToString(pObj);` |
|        1 |  6978 | `		}` |
|     8286 |  6979 | `		if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     8284 |  6980 | `			if( PH7_MemObjStringAppend(pObj,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - |  6981 | `				/* Allocation failure: the grow happens before the copy, so pObj` |
|        - |  6982 | `				 * keeps its prior valid contents — raise the fatal uncorrupted. */` |
|      ! 0 |  6983 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 |  6984 | `				goto Abort;` |
|        - |  6985 | `			}` |
|     4141 |  6986 | `		}` |
|        - |  6987 | ``		/* Produce the expression result. A `.=` result is a temporary, never an`` |
|        - |  6988 | ``		 * addressable lvalue, so nIdx is SXU32_HIGH (otherwise `f($s .= "x")` with a`` |
|        - |  6989 | ``		 * by-ref param, or `&($s .= "x")`, would alias the live variable).`` |
|        - |  6990 | ``		 * In the dominant statement form `$s .= "x";` the result is discarded by the`` |
|        - |  6991 | `		 * very next opcode (OP_POP), so we skip building it and leave the (harmless)` |
|        - |  6992 | `		 * RHS operand for the POP to drop — keeping the hot path allocation-free.` |
|        - |  6993 | `		 * Otherwise the result is consumed, so materialize an INDEPENDENT owned copy` |
|        - |  6994 | `		 * of the updated value: a read-only alias into pObj's buffer would dangle if` |
|        - |  6995 | `		 * the same slot is appended to again later in the statement` |
|        - |  6996 | ``		 * (e.g. `($s .= "a") . ($s .= "b")` reallocs the buffer the first result`` |
|        - |  6997 | `		 * still points at). Peeking pInstr+1 is safe: the compiler always emits a` |
|        - |  6998 | `		 * terminating OP_DONE, so it is in-bounds inside any non-DONE opcode. */` |
|     8286 |  6999 | `		if( (pInstr+1)->iOp != PH7_OP_POP ){` |
|        9 |  7000 | `			PH7_MemObjStore(pObj,pNos);` |
|        4 |  7001 | `		}` |
|     8286 |  7002 | `		pNos->nIdx = SXU32_HIGH;` |
|     8286 |  7003 | `		VmPopOperand(&pTos,1);` |
|     8293 |  7004 | `		break;` |
|        - |  7005 | `	}` |
|        - |  7006 | `	/* Slow path: read-only/typed/constant-attribute/self-aliasing lvalues. */` |
|       16 |  7007 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7008 | `		/* Force a string cast */` |
|        6 |  7009 | `		PH7_MemObjToString(pTos);` |
|        2 |  7010 | `	}` |
|        - |  7011 | `	/* Perform the concatenation (Reverse order) */` |
|       16 |  7012 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|       16 |  7013 | `		if( PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - |  7014 | `			/* Allocation failure: raise a fatal before committing the store so` |
|        - |  7015 | `			 * no partially-concatenated value is written to the lvalue. */` |
|      ! 0 |  7016 | `			PH7_VmMemoryError(&(*pVm));` |
|      ! 0 |  7017 | `			goto Abort;` |
|        - |  7018 | `		}` |
|        7 |  7019 | `	}` |
|        - |  7020 | `	/* Perform the store operation */` |
|       16 |  7021 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  7022 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       16 |  7023 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       16 |  7024 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);` |
|       11 |  7025 | `		PH7_MemObjStore(pTos,pObj);` |
|        5 |  7026 | `	}` |
|       11 |  7027 | `	PH7_MemObjStore(pTos,pNos);` |
|       11 |  7028 | `	VmPopOperand(&pTos,1);` |
|       11 |  7029 | `	break;` |
|        - |  7030 | `				}` |
|        - |  7031 | `/* OP_AND: * * *` |
|        - |  7032 | ` *` |
|        - |  7033 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  7034 | ` * two values and push the resulting boolean value back onto the` |
|        - |  7035 | ` * stack.` |
|        - |  7036 | ` */` |
|        - |  7037 | `/* OP_OR: * * *` |
|        - |  7038 | ` *` |
|        - |  7039 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  7040 | ` * two values and push the resulting boolean value back onto the` |
|        - |  7041 | ` * stack.` |
|        - |  7042 | ` */` |
|   108585 |  7043 | `case PH7_OP_LAND:` |
|        - |  7044 | `case PH7_OP_LOR: {` |
|   217216 |  7045 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7046 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  7047 | `#ifdef UNTRUST` |
|        - |  7048 | `	if( pNos < pStack ){` |
|        - |  7049 | `		goto Abort;` |
|        - |  7050 | `	}` |
|        - |  7051 | `#endif` |
|        - |  7052 | `	/* Force a boolean cast */` |
|   217216 |  7053 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  7054 | `		PH7_MemObjToBool(pTos);` |
|        1 |  7055 | `	}` |
|   217216 |  7056 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  7057 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  7058 | `	}` |
|   217216 |  7059 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   217216 |  7060 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   217216 |  7061 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  7062 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    99958 |  7063 | `		v1 = and_logic[v1*3+v2];` |
|    50002 |  7064 | `	}else{` |
|        - |  7065 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   117260 |  7066 | `		v1 = or_logic[v1*3+v2];` |
|        - |  7067 | `	}` |
|   217216 |  7068 | `	if( v1 == 2 ){` |
|      ! 0 |  7069 | `		v1 = 1;` |
|      ! 0 |  7070 | `	}` |
|   217216 |  7071 | `	VmPopOperand(&pTos,1);` |
|   217216 |  7072 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   217216 |  7073 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   217216 |  7074 | `	break;` |
|        - |  7075 | `				 }` |
|        - |  7076 | `/*` |
|        - |  7077 | ` * OP_NULLC: * * *` |
|        - |  7078 | ` * Null coalescing operator '??'.` |
|        - |  7079 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - |  7080 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - |  7081 | ` */` |
|        - |  7082 | `/*` |
|        - |  7083 | ` * OP_NULLC: * P2 *` |
|        - |  7084 | ` * Short-circuit null coalescing '??'.` |
|        - |  7085 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - |  7086 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - |  7087 | ` */` |
|       99 |  7088 | `case PH7_OP_NULLC: {` |
|        - |  7089 | `#ifdef UNTRUST` |
|        - |  7090 | `	if( pTos < pStack ){` |
|        - |  7091 | `		goto Abort;` |
|        - |  7092 | `	}` |
|        - |  7093 | `#endif` |
|      200 |  7094 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  7095 | `		/* Left is not null — keep it and skip the RHS */` |
|      120 |  7096 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       61 |  7097 | `	}else{` |
|        - |  7098 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|       82 |  7099 | `		VmPopOperand(&pTos, 1);` |
|        - |  7100 | `	}` |
|      200 |  7101 | `	break;` |
|        - |  7102 |  |
|        - |  7103 | `/*` |
|        - |  7104 | ` * OP_NULLC_JMP: * P2 *` |
|        - |  7105 | ` * Null coalescing assignment short-circuit.` |
|        - |  7106 | ` * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).` |
|        - |  7107 | ` * If TOS IS null, fall through with TOS retained — it carries the LHS's` |
|        - |  7108 | ` * nIdx so the upcoming NULLC_STORE can write back into the variable slot.` |
|        - |  7109 | ` */` |
|       28 |  7110 | `case PH7_OP_NULLC_JMP: {` |
|        - |  7111 | `#ifdef UNTRUST` |
|        - |  7112 | `	if( pTos < pStack ){` |
|        - |  7113 | `		goto Abort;` |
|        - |  7114 | `	}` |
|        - |  7115 | `#endif` |
|       58 |  7116 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       22 |  7117 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       10 |  7118 | `	}` |
|       58 |  7119 | `	break;` |
|        - |  7120 |  |
|        - |  7121 | `/*` |
|        - |  7122 | ` * OP_NULLC_STORE: * * *` |
|        - |  7123 | ` * Null coalescing assignment store.` |
|        - |  7124 | ` * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],` |
|        - |  7125 | ` * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the` |
|        - |  7126 | ` * expression result.` |
|        - |  7127 | ` */` |
|        - |  7128 | `/*` |
|        - |  7129 | ` * OP_NULLSAFE_JMP: * P2 *` |
|        - |  7130 | `` * Nullsafe object operator short-circuit (PHP 8.0 `?->`).`` |
|        - |  7131 | ` * Peek TOS (the object operand): if it is null, jump to P2 leaving NULL` |
|        - |  7132 | ` * on the stack as the result of the entire containing postfix chain. If` |
|        - |  7133 | ` * non-null, fall through without modifying the stack so the following` |
|        - |  7134 | ` * PH7_OP_MEMBER can consume the object as usual.` |
|        - |  7135 | ` */` |
|       51 |  7136 | `case PH7_OP_NULLSAFE_JMP: {` |
|        - |  7137 | `#ifdef UNTRUST` |
|        - |  7138 | `	if( pTos < pStack ){` |
|        - |  7139 | `		goto Abort;` |
|        - |  7140 | `	}` |
|        - |  7141 | `#endif` |
|      104 |  7142 | `	if( (pTos->iFlags & MEMOBJ_NULL) \|\| pTos->iFlags == 0 ){` |
|        - |  7143 | `		/* Object operand is NULL (or uninitialized) — short-circuit. The` |
|        - |  7144 | `		 * NULL slot already on TOS becomes the chain's final value. */` |
|       41 |  7145 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       20 |  7146 | `	}` |
|      104 |  7147 | `	break;` |
|        - |  7148 |  |
|       17 |  7149 | `case PH7_OP_NULLC_STORE: {` |
|       36 |  7150 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7151 | `	ph7_value *pObj;` |
|        - |  7152 | `	sxu32 nIdx;` |
|        - |  7153 | `#ifdef UNTRUST` |
|        - |  7154 | `	if( pNos < pStack ){` |
|        - |  7155 | `		goto Abort;` |
|        - |  7156 | `	}` |
|        - |  7157 | `#endif` |
|        - |  7158 | `	/* ArrayAccess null-coalesce-assign target: the preceding LOAD_IDX iP2=3` |
|        - |  7159 | `	 * armed pVm with the (object, key) on a missing key. Dispatch to` |
|        - |  7160 | `	 * offsetSet instead of writing through the synthetic pNos->nIdx. */` |
|       36 |  7161 | `	if( pVm->bCoalesceArmed && pVm->pCoalesceObj ){` |
|        5 |  7162 | `		ph7_class_instance *pInst = pVm->pCoalesceObj;` |
|        5 |  7163 | `		ph7_class_method *pSet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  7164 | `			"offsetSet",sizeof("offsetSet")-1);` |
|        - |  7165 | `		ph7_value *apArg[2];` |
|        5 |  7166 | `		apArg[0] = &pVm->sCoalesceKey;` |
|        5 |  7167 | `		apArg[1] = pTos;` |
|        5 |  7168 | `		if( pSet ){` |
|        5 |  7169 | `			PH7_VmCallClassMethod(&(*pVm),pInst,pSet,0,2,apArg);` |
|        2 |  7170 | `		}` |
|        - |  7171 | `		/* Leave RHS as the expression result (replace pNos with pTos). */` |
|        5 |  7172 | `		PH7_MemObjStore(pTos,pNos);` |
|        5 |  7173 | `		VmPopOperand(&pTos,1);` |
|        - |  7174 | `		/* Disarm and release the cached instance ref + key. */` |
|        5 |  7175 | `		VmCoalesceDisarm(pVm);` |
|        5 |  7176 | `		break;` |
|        - |  7177 | `	}` |
|       32 |  7178 | `	nIdx = pNos->nIdx;` |
|       32 |  7179 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  7180 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7181 | `			"Cannot perform assignment on a constant class attribute");` |
|       32 |  7182 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|       32 |  7183 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|       32 |  7184 | `		PH7_MemObjStore(pTos,pObj);` |
|       15 |  7185 | `	}` |
|       32 |  7186 | `	PH7_MemObjStore(pTos,pNos);` |
|       32 |  7187 | `	VmPopOperand(&pTos,1);` |
|       32 |  7188 | `	break;` |
|        - |  7189 |  |
|        - |  7190 | `/*` |
|        - |  7191 | ` * OP_SPREAD: * * *` |
|        - |  7192 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - |  7193 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - |  7194 | ` * Accumulates the net stack growth in pVm->iSpreadExtra so the next CALL` |
|        - |  7195 | ` * can adjust its argument count (the CALL may not be the next instruction).` |
|        - |  7196 | ` */` |
|       10 |  7197 | `case PH7_OP_SPREAD: {` |
|        - |  7198 | `#ifdef UNTRUST` |
|        - |  7199 | `	if( pTos < pStack ){` |
|        - |  7200 | `		goto Abort;` |
|        - |  7201 | `	}` |
|        - |  7202 | `#endif` |
|        - |  7203 | `	/* Traversable argument unpacking f(...$it): materialize the iterator into a` |
|        - |  7204 | `	 * temp array (positional values), then expand it onto the operand stack` |
|        - |  7205 | `	 * like an array. Materialising first leaves the stack untouched until the` |
|        - |  7206 | `	 * walk succeeds; values are deep-copied (PH7_MemObjStore) so the temp can` |
|        - |  7207 | `	 * be freed immediately. */` |
|       22 |  7208 | `	if( VmValueIsTraversable(pVm,pTos) ){` |
|        3 |  7209 | `		ph7_hashmap *pTmpMap = PH7_NewHashmap(&(*pVm),0,0);` |
|        - |  7210 | `		sxi32 rcW;` |
|        - |  7211 | `		sxu32 nEnt;` |
|        3 |  7212 | `		if( pTmpMap == 0 ){ goto Abort; }` |
|        3 |  7213 | `		rcW = PH7_VmIteratorWalk(&(*pVm),pTos,VmSpreadValuesStep,pTmpMap);` |
|        3 |  7214 | `		if( rcW == PH7_EXCEPTION \|\| rcW == PH7_ABORT ){` |
|      ! 0 |  7215 | `			PH7_HashmapRelease(pTmpMap,TRUE);` |
|      ! 0 |  7216 | `			if( rcW == PH7_ABORT ){ goto Abort; }` |
|      ! 0 |  7217 | `			goto Exception;` |
|        - |  7218 | `		}` |
|        3 |  7219 | `		nEnt = pTmpMap->nEntry;` |
|        3 |  7220 | `		if( nEnt == 0 ){` |
|      ! 0 |  7221 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7222 | `			pVm->iSpreadExtra--;` |
|        3 |  7223 | `		}else if( pVm->iSpreadExtra + (sxi32)(nEnt - 1) >= VM_STACK_GUARD ){` |
|      ! 0 |  7224 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - |  7225 | `				"Argument unpacking: cumulative expansion exceeds stack guard (%d)", VM_STACK_GUARD);` |
|      ! 0 |  7226 | `		}else{` |
|        3 |  7227 | `			ph7_hashmap_node *pNodeT = pTmpMap->pFirst;` |
|        - |  7228 | `			ph7_value *pElemT;` |
|        - |  7229 | `			sxu32 iT;` |
|        3 |  7230 | `			pElemT = (ph7_value *)SySetAt(&pVm->aMemObj, pNodeT->nValIdx);` |
|        3 |  7231 | `			if( pElemT ){ PH7_MemObjStore(pElemT, pTos); }else{ PH7_MemObjRelease(pTos); }` |
|        3 |  7232 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  7233 | `			pNodeT = pNodeT->pPrev;` |
|        7 |  7234 | `			for( iT = 1; iT < nEnt; iT++ ){` |
|        5 |  7235 | `				pTos++;` |
|        5 |  7236 | `				PH7_MemObjInit(pVm, pTos);` |
|        5 |  7237 | `				pTos->nIdx = SXU32_HIGH;` |
|        5 |  7238 | `				pElemT = (ph7_value *)SySetAt(&pVm->aMemObj, pNodeT->nValIdx);` |
|        5 |  7239 | `				if( pElemT ){ PH7_MemObjStore(pElemT, pTos); }` |
|        5 |  7240 | `				pNodeT = pNodeT->pPrev;` |
|        3 |  7241 | `			}` |
|        3 |  7242 | `			pVm->iSpreadExtra += (sxi32)(nEnt - 1);` |
|        - |  7243 | `		}` |
|        3 |  7244 | `		PH7_HashmapRelease(pTmpMap,TRUE);` |
|        3 |  7245 | `		break;` |
|        - |  7246 | `	}` |
|       20 |  7247 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       20 |  7248 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       20 |  7249 | `		sxu32 nEntry = pMap->nEntry;` |
|       20 |  7250 | `		if( nEntry == 0 ){` |
|        - |  7251 | `			/* Empty array — remove from stack */` |
|        3 |  7252 | `			VmPopOperand(&pTos, 1);` |
|        3 |  7253 | `			pVm->iSpreadExtra--; /* One expression produced zero args */` |
|       19 |  7254 | `		}else if( pVm->iSpreadExtra + (sxi32)(nEntry - 1) >= VM_STACK_GUARD ){` |
|        - |  7255 | `			/* Safety: refuse to expand beyond the stack guard margin */` |
|      ! 0 |  7256 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - |  7257 | `				"Argument unpacking: cumulative expansion exceeds stack guard (%d)",` |
|        - |  7258 | `				VM_STACK_GUARD);` |
|      ! 0 |  7259 | `		}else{` |
|        - |  7260 | `			ph7_hashmap_node *pNode2;` |
|        - |  7261 | `			ph7_value *pElem;` |
|        - |  7262 | `			sxu32 i;` |
|        - |  7263 | `			/* Overwrite TOS with first element */` |
|       18 |  7264 | `			pNode2 = pMap->pFirst;` |
|       18 |  7265 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       18 |  7266 | `			PH7_MemObjRelease(pTos);` |
|       18 |  7267 | `			if( pElem ){` |
|       18 |  7268 | `				PH7_MemObjLoad(pElem, pTos);` |
|        8 |  7269 | `			}` |
|       18 |  7270 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  7271 | `			/* Traverse in insertion order (pPrev is the forward link` |
|        - |  7272 | `			 * in PHL's circular doubly-linked hashmap node list). */` |
|       18 |  7273 | `			pNode2 = pNode2->pPrev;` |
|        - |  7274 | `			/* Push remaining elements */` |
|       44 |  7275 | `			for( i = 1; i < nEntry; i++ ){` |
|       28 |  7276 | `				pTos++;` |
|       28 |  7277 | `				PH7_MemObjInit(pVm, pTos);` |
|       28 |  7278 | `				pTos->nIdx = SXU32_HIGH;` |
|       28 |  7279 | `				pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       28 |  7280 | `				if( pElem ){` |
|       28 |  7281 | `					PH7_MemObjLoad(pElem, pTos);` |
|       13 |  7282 | `				}` |
|       28 |  7283 | `				pNode2 = pNode2->pPrev;` |
|       15 |  7284 | `			}` |
|       18 |  7285 | `			pVm->iSpreadExtra += (sxi32)(nEntry - 1);` |
|        - |  7286 | `		}` |
|        9 |  7287 | `	}` |
|        - |  7288 | `	/* else: not an array — leave as-is (single arg) */` |
|       20 |  7289 | `	break;` |
|        - |  7290 |  |
|        - |  7291 | `/*` |
|        - |  7292 | ` * OP_FLAG_SPREAD: * * *` |
|        - |  7293 | ` * Mark the value at TOS as a spread source for the next LOAD_MAP.` |
|        - |  7294 | ` * Used by array literal unpacking '[...$arr]'.` |
|        - |  7295 | ` */` |
|       37 |  7296 | `case PH7_OP_FLAG_SPREAD: {` |
|        - |  7297 | `#ifdef UNTRUST` |
|        - |  7298 | `	if( pTos < pStack ){` |
|        - |  7299 | `		goto Abort;` |
|        - |  7300 | `	}` |
|        - |  7301 | `#endif` |
|       76 |  7302 | `	pTos->iFlags \|= MEMOBJ_AUX_SPREAD;` |
|       76 |  7303 | `	break;` |
|        - |  7304 |  |
|        - |  7305 | `/* OP_LXOR: * * *` |
|        - |  7306 | ` *` |
|        - |  7307 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  7308 | ` * two values and push the resulting boolean value back onto the` |
|        - |  7309 | ` * stack.` |
|        - |  7310 | ` * According to the PHP language reference manual:` |
|        - |  7311 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  7312 | ` *  TRUE,but not both.` |
|        - |  7313 | ` */` |
|        5 |  7314 | `case PH7_OP_LXOR:{` |
|       11 |  7315 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  7316 | `	sxi32 v = 0;` |
|        - |  7317 | `#ifdef UNTRUST` |
|        - |  7318 | `	if( pNos < pStack ){` |
|        - |  7319 | `		goto Abort;` |
|        - |  7320 | `	}` |
|        - |  7321 | `#endif` |
|        - |  7322 | `	/* Force a boolean cast */` |
|       11 |  7323 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  7324 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  7325 | `	}` |
|       11 |  7326 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  7327 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  7328 | `	}` |
|       11 |  7329 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  7330 | `		v = 1;` |
|        3 |  7331 | `	}` |
|       11 |  7332 | `	VmPopOperand(&pTos,1);` |
|       11 |  7333 | `	pTos->x.iVal = v;` |
|       11 |  7334 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  7335 | `	break;` |
|        - |  7336 | `				 }` |
|        - |  7337 | `/* OP_EQ P1 P2 P3` |
|        - |  7338 | ` *` |
|        - |  7339 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  7340 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  7341 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7342 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7343 | ` */` |
|        - |  7344 | `/* OP_NEQ P1 P2 P3` |
|        - |  7345 | ` *` |
|        - |  7346 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  7347 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  7348 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7349 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7350 | ` */` |
|     4611 |  7351 | `case PH7_OP_EQ:` |
|        - |  7352 | `case PH7_OP_NEQ: {` |
|     9224 |  7353 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7354 | `	/* Perform the comparison and act accordingly */` |
|        - |  7355 | `#ifdef UNTRUST` |
|        - |  7356 | `	if( pNos < pStack ){` |
|        - |  7357 | `		goto Abort;` |
|        - |  7358 | `	}` |
|        - |  7359 | `#endif` |
|     9224 |  7360 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     9224 |  7361 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       19 |  7362 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     9215 |  7363 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     9180 |  7364 | `		rc = rc == 0;` |
|     4591 |  7365 | `	}else{` |
|       28 |  7366 | `		rc = rc != 0;` |
|        - |  7367 | `	}` |
|     9224 |  7368 | `	VmPopOperand(&pTos,1);` |
|     9224 |  7369 | `	if( !pInstr->iP2 ){` |
|        - |  7370 | `		/* Push comparison result without taking the jump */` |
|     9224 |  7371 | `		PH7_MemObjRelease(pTos);` |
|     9224 |  7372 | `		pTos->x.iVal = rc;` |
|        - |  7373 | `		/* Invalidate any prior representation */` |
|     9224 |  7374 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     4613 |  7375 | `	}else{` |
|      ! 0 |  7376 | `		if( rc ){` |
|        - |  7377 | `			/* Jump to the desired location */` |
|      ! 0 |  7378 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7379 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7380 | `		}` |
|        - |  7381 | `	}` |
|     9224 |  7382 | `	break;` |
|        - |  7383 | `				 }` |
|        - |  7384 | `/* OP_TEQ P1 P2 *` |
|        - |  7385 | ` *` |
|        - |  7386 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  7387 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  7388 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7389 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7390 | ` */` |
|   162373 |  7391 | `case PH7_OP_TEQ: {` |
|   324748 |  7392 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7393 | `	/* Perform the comparison and act accordingly */` |
|        - |  7394 | `#ifdef UNTRUST` |
|        - |  7395 | `	if( pNos < pStack ){` |
|        - |  7396 | `		goto Abort;` |
|        - |  7397 | `	}` |
|        - |  7398 | `#endif` |
|   324748 |  7399 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   324748 |  7400 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  7401 | `		rc = 0;` |
|        2 |  7402 | `	}else{` |
|   324746 |  7403 | `		rc = rc == 0;` |
|        - |  7404 | `	}` |
|   324748 |  7405 | `	VmPopOperand(&pTos,1);` |
|   324748 |  7406 | `	if( !pInstr->iP2 ){` |
|        - |  7407 | `		/* Push comparison result without taking the jump */` |
|   324748 |  7408 | `		PH7_MemObjRelease(pTos);` |
|   324748 |  7409 | `		pTos->x.iVal = rc;` |
|        - |  7410 | `		/* Invalidate any prior representation */` |
|   324748 |  7411 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   162375 |  7412 | `	}else{` |
|      ! 0 |  7413 | `		if( rc ){` |
|        - |  7414 | `			/* Jump to the desired location */` |
|      ! 0 |  7415 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7416 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7417 | `		}` |
|        - |  7418 | `	}` |
|   324748 |  7419 | `	break;` |
|        - |  7420 | `				 }` |
|        - |  7421 | `/* OP_TNE P1 P2 *` |
|        - |  7422 | ` *` |
|        - |  7423 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  7424 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  7425 | ` * instruction.` |
|        - |  7426 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7427 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7428 | ` *` |
|        - |  7429 | ` */` |
|   124879 |  7430 | `case PH7_OP_TNE: {` |
|   249760 |  7431 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7432 | `	/* Perform the comparison and act accordingly */` |
|        - |  7433 | `#ifdef UNTRUST` |
|        - |  7434 | `	if( pNos < pStack ){` |
|        - |  7435 | `		goto Abort;` |
|        - |  7436 | `	}` |
|        - |  7437 | `#endif` |
|   249760 |  7438 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   249760 |  7439 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  7440 | `		rc = 1;` |
|        2 |  7441 | `	}else{` |
|   249758 |  7442 | `		rc = rc != 0;` |
|        - |  7443 | `	}` |
|   249760 |  7444 | `	VmPopOperand(&pTos,1);` |
|   249760 |  7445 | `	if( !pInstr->iP2 ){` |
|        - |  7446 | `		/* Push comparison result without taking the jump */` |
|   249760 |  7447 | `		PH7_MemObjRelease(pTos);` |
|   249760 |  7448 | `		pTos->x.iVal = rc;` |
|        - |  7449 | `		/* Invalidate any prior representation */` |
|   249760 |  7450 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   124881 |  7451 | `	}else{` |
|      ! 0 |  7452 | `		if( rc ){` |
|        - |  7453 | `			/* Jump to the desired location */` |
|      ! 0 |  7454 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7455 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7456 | `		}` |
|        - |  7457 | `	}` |
|   249760 |  7458 | `	break;` |
|        - |  7459 | `				 }` |
|        - |  7460 | `/* OP_LT P1 P2 P3` |
|        - |  7461 | ` *` |
|        - |  7462 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  7463 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  7464 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  7465 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7466 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7467 | ` *` |
|        - |  7468 | ` */` |
|        - |  7469 | `/* OP_LE P1 P2 P3` |
|        - |  7470 | ` *` |
|        - |  7471 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  7472 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  7473 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  7474 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7475 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7476 | ` *` |
|        - |  7477 | ` */` |
|   112622 |  7478 | `case PH7_OP_LT:` |
|        - |  7479 | `case PH7_OP_LE: {` |
|   225290 |  7480 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7481 | `	/* Perform the comparison and act accordingly */` |
|        - |  7482 | `#ifdef UNTRUST` |
|        - |  7483 | `	if( pNos < pStack ){` |
|        - |  7484 | `		goto Abort;` |
|        - |  7485 | `	}` |
|        - |  7486 | `#endif` |
|   225290 |  7487 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   225290 |  7488 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  7489 | `		rc = 0;` |
|   225286 |  7490 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|     1608 |  7491 | `		rc = rc < 1;` |
|      805 |  7492 | `	}else{` |
|   223676 |  7493 | `		rc = rc < 0;` |
|        - |  7494 | `	}` |
|   225290 |  7495 | `	VmPopOperand(&pTos,1);` |
|   225290 |  7496 | `	if( !pInstr->iP2 ){` |
|        - |  7497 | `		/* Push comparison result without taking the jump */` |
|   225290 |  7498 | `		PH7_MemObjRelease(pTos);` |
|   225290 |  7499 | `		pTos->x.iVal = rc;` |
|        - |  7500 | `		/* Invalidate any prior representation */` |
|   225290 |  7501 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   112668 |  7502 | `	}else{` |
|      ! 0 |  7503 | `		if( rc ){` |
|        - |  7504 | `			/* Jump to the desired location */` |
|      ! 0 |  7505 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7506 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7507 | `		}` |
|        - |  7508 | `	}` |
|   225290 |  7509 | `	break;` |
|        - |  7510 | `				}` |
|        - |  7511 | `/* OP_GT P1 P2 P3` |
|        - |  7512 | ` *` |
|        - |  7513 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  7514 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  7515 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  7516 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7517 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7518 | ` *` |
|        - |  7519 | ` */` |
|        - |  7520 | `/* OP_GE P1 P2 P3` |
|        - |  7521 | ` *` |
|        - |  7522 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  7523 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  7524 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  7525 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7526 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7527 | ` *` |
|        - |  7528 | ` */` |
|    55701 |  7529 | `case PH7_OP_GT:` |
|        - |  7530 | `case PH7_OP_GE: {` |
|   111404 |  7531 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7532 | `	/* Perform the comparison and act accordingly */` |
|        - |  7533 | `#ifdef UNTRUST` |
|        - |  7534 | `	if( pNos < pStack ){` |
|        - |  7535 | `		goto Abort;` |
|        - |  7536 | `	}` |
|        - |  7537 | `#endif` |
|   111404 |  7538 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   111404 |  7539 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  7540 | `		rc = 0;` |
|   111400 |  7541 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|   110968 |  7542 | `		rc = rc >= 0;` |
|    55485 |  7543 | `	}else{` |
|      430 |  7544 | `		rc = rc > 0;` |
|        - |  7545 | `	}` |
|   111404 |  7546 | `	VmPopOperand(&pTos,1);` |
|   111404 |  7547 | `	if( !pInstr->iP2 ){` |
|        - |  7548 | `		/* Push comparison result without taking the jump */` |
|   111404 |  7549 | `		PH7_MemObjRelease(pTos);` |
|   111404 |  7550 | `		pTos->x.iVal = rc;` |
|        - |  7551 | `		/* Invalidate any prior representation */` |
|   111404 |  7552 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    55703 |  7553 | `	}else{` |
|      ! 0 |  7554 | `		if( rc ){` |
|        - |  7555 | `			/* Jump to the desired location */` |
|      ! 0 |  7556 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7557 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7558 | `		}` |
|        - |  7559 | `	}` |
|   111404 |  7560 | `	break;` |
|        - |  7561 | `				}` |
|        - |  7562 | `/* OP_SPACESHIP * * *` |
|        - |  7563 | ` *` |
|        - |  7564 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - |  7565 | ` *   -1 if left < right` |
|        - |  7566 | ` *    0 if left == right` |
|        - |  7567 | ` *    1 if left > right` |
|        - |  7568 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - |  7569 | ` */` |
|       25 |  7570 | `case PH7_OP_SPACESHIP: {` |
|       51 |  7571 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7572 | `#ifdef UNTRUST` |
|        - |  7573 | `	if( pNos < pStack ){` |
|        - |  7574 | `		goto Abort;` |
|        - |  7575 | `	}` |
|        - |  7576 | `#endif` |
|       51 |  7577 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|       51 |  7578 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        - |  7579 | `		/* NaN involved: PHP returns 1 for all NaN spaceship comparisons */` |
|        7 |  7580 | `		rc = 1;` |
|        4 |  7581 | `	}else{` |
|        - |  7582 | `		/* Normalize to exactly -1, 0, or 1 */` |
|       45 |  7583 | `		rc = (rc > 0) - (rc < 0);` |
|        - |  7584 | `	}` |
|       51 |  7585 | `	VmPopOperand(&pTos,1);` |
|       51 |  7586 | `	PH7_MemObjRelease(pTos);` |
|       51 |  7587 | `	pTos->x.iVal = rc;` |
|       51 |  7588 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       51 |  7589 | `	break;` |
|        - |  7590 | `				}` |
|        - |  7591 | `/* OP_SEQ P1 P2 *` |
|        - |  7592 | ` * Strict string comparison.` |
|        - |  7593 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  7594 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  7595 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  7596 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  7597 | ` * use PH7_OP_EQ.` |
|        - |  7598 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7599 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7600 | ` */` |
|        - |  7601 | `/* OP_SNE P1 P2 *` |
|        - |  7602 | ` * Strict string comparison.` |
|        - |  7603 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  7604 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  7605 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  7606 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  7607 | ` * use PH7_OP_EQ.` |
|        - |  7608 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7609 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7610 | ` */` |
|       18 |  7611 | `case PH7_OP_SEQ:` |
|        - |  7612 | `case PH7_OP_SNE: {` |
|       38 |  7613 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7614 | `	SyString s1,s2;` |
|        - |  7615 | `	/* Perform the comparison and act accordingly */` |
|        - |  7616 | `#ifdef UNTRUST` |
|        - |  7617 | `	if( pNos < pStack ){` |
|        - |  7618 | `		goto Abort;` |
|        - |  7619 | `	}` |
|        - |  7620 | `#endif` |
|        - |  7621 | `	/* Force a string cast */` |
|       38 |  7622 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  7623 | `		PH7_MemObjToString(pTos);` |
|        2 |  7624 | `	}` |
|       38 |  7625 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  7626 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  7627 | `	}` |
|       38 |  7628 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  7629 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  7630 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  7631 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  7632 | `		rc = rc != 0;` |
|      ! 0 |  7633 | `	}else{` |
|       38 |  7634 | `		rc = rc == 0;` |
|        - |  7635 | `	}` |
|       38 |  7636 | `	VmPopOperand(&pTos,1);` |
|       38 |  7637 | `	if( !pInstr->iP2 ){` |
|        - |  7638 | `		/* Push comparison result without taking the jump */` |
|       38 |  7639 | `		PH7_MemObjRelease(pTos);` |
|       38 |  7640 | `		pTos->x.iVal = rc;` |
|        - |  7641 | `		/* Invalidate any prior representation */` |
|       38 |  7642 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  7643 | `	}else{` |
|      ! 0 |  7644 | `		if( rc ){` |
|        - |  7645 | `			/* Jump to the desired location */` |
|      ! 0 |  7646 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7647 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7648 | `		}` |
|        - |  7649 | `	}` |
|       38 |  7650 | `	break;` |
|        - |  7651 | `				 }` |
|        - |  7652 | `/*` |
|        - |  7653 | ` * OP_LOAD_REF * * *` |
|        - |  7654 | ` * Push the index of a referenced object on the stack.` |
|        - |  7655 | ` */` |
|       60 |  7656 | `case PH7_OP_LOAD_REF: {` |
|        - |  7657 | `	sxu32 nIdx;` |
|        - |  7658 | `#ifdef UNTRUST` |
|        - |  7659 | `	if( pTos < pStack ){` |
|        - |  7660 | `		goto Abort;` |
|        - |  7661 | `	}` |
|        - |  7662 | `#endif` |
|        - |  7663 | `	/* Extract memory object index */` |
|      121 |  7664 | `	nIdx = pTos->nIdx;` |
|      121 |  7665 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  7666 | `		/* Nullify the object */` |
|      101 |  7667 | `		PH7_MemObjRelease(pTos);` |
|        - |  7668 | `		/* Mark as constant and store the index on the top of the stack */` |
|      101 |  7669 | `		pTos->x.iVal = (sxi64)nIdx;` |
|      101 |  7670 | `		pTos->nIdx = SXU32_HIGH;` |
|      101 |  7671 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       50 |  7672 | `	}` |
|      121 |  7673 | `	break;` |
|        - |  7674 | `					  }` |
|        - |  7675 | `/*` |
|        - |  7676 | ` * OP_STORE_REF * * P3` |
|        - |  7677 | ` * Perform an assignment operation by reference.` |
|        - |  7678 | ` */` |
|       18 |  7679 | ` case PH7_OP_STORE_REF: {` |
|       38 |  7680 | `	 SyString sName = { 0 , 0 };` |
|        - |  7681 | `	 VmFrame *pFrameLocal;` |
|        - |  7682 | `	SyHashEntry *pEntry;` |
|        - |  7683 | `	sxu32 nIdx;` |
|        - |  7684 | `#ifdef UNTRUST` |
|        - |  7685 | `	if( pTos < pStack ){` |
|        - |  7686 | `		goto Abort;` |
|        - |  7687 | `	}` |
|        - |  7688 | `#endif` |
|       38 |  7689 | `	if( pInstr->p3 == 0 ){` |
|        - |  7690 | `		char *zName;` |
|        - |  7691 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  7692 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7693 | `			/* Force a string cast */` |
|      ! 0 |  7694 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  7695 | `		}` |
|      ! 0 |  7696 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  7697 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  7698 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7699 | `			if( zName ){` |
|      ! 0 |  7700 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7701 | `			}` |
|      ! 0 |  7702 | `		}` |
|      ! 0 |  7703 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  7704 | `		pTos--;` |
|      ! 0 |  7705 | `	}else{` |
|       38 |  7706 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  7707 | `	}` |
|       38 |  7708 | `	nIdx = pTos->nIdx;` |
|       38 |  7709 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  7710 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  7711 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7712 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  7713 | `		}else{` |
|        - |  7714 | `			ph7_value *pObj;` |
|        - |  7715 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  7716 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  7717 | `			if( pObj == 0 ){` |
|      ! 0 |  7718 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7719 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  7720 | `				goto Abort;` |
|        - |  7721 | `			}` |
|        - |  7722 | `			/* Perform the store operation */` |
|      ! 0 |  7723 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  7724 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  7725 | `		}` |
|       38 |  7726 | `	}else if( sName.nByte > 0){` |
|       38 |  7727 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  7728 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  7729 | `		}else{` |
|       38 |  7730 | `			pFrameLocal = pVm->pFrame;` |
|       38 |  7731 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  7732 | `			/* Query the local frame */` |
|       38 |  7733 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       38 |  7734 | `			if( pEntry ){` |
|      ! 0 |  7735 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  7736 | `			}else{` |
|       38 |  7737 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       38 |  7738 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  7739 | `					/* Insert in the $GLOBALS array */` |
|       34 |  7740 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       16 |  7741 | `				}` |
|       38 |  7742 | `				if( rc == SXRET_OK ){` |
|       38 |  7743 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       18 |  7744 | `				}` |
|        - |  7745 | `			}` |
|        - |  7746 | `		}` |
|       18 |  7747 | `	}` |
|       38 |  7748 | `	break;` |
|        - |  7749 | `				 }` |
|        - |  7750 | `/*` |
|        - |  7751 | ` * OP_UPLINK P1 * *` |
|        - |  7752 | ` * Link a variable to the top active VM frame.` |
|        - |  7753 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  7754 | ` */` |
|       30 |  7755 | `case PH7_OP_UPLINK: {` |
|       62 |  7756 | `	if( pVm->pFrame->pParent ){` |
|       62 |  7757 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  7758 | `		SyString sName;` |
|        - |  7759 | `		/* Perform the link */` |
|      132 |  7760 | `		while( pLink <= pTos ){` |
|       72 |  7761 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7762 | `				/* Force a string cast */` |
|      ! 0 |  7763 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  7764 | `			}` |
|       72 |  7765 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       72 |  7766 | `			if( sName.nByte > 0 ){` |
|       72 |  7767 | `				VmFrameLink(&(*pVm),&sName);` |
|       35 |  7768 | `			}` |
|       72 |  7769 | `			pLink++;` |
|        2 |  7770 | `		}` |
|       30 |  7771 | `	}` |
|       62 |  7772 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       62 |  7773 | `	break;` |
|        - |  7774 | `					}` |
|        - |  7775 | `/*` |
|        - |  7776 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  7777 | ` * Push an exception in the corresponding container so that` |
|        - |  7778 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  7779 | ` */` |
|      194 |  7780 | `case PH7_OP_LOAD_EXCEPTION: {` |
|      390 |  7781 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  7782 | `	VmFrame *pFrameLocal;` |
|        - |  7783 | `	/* Reset per-entry state so finally runs on each iteration */` |
|      390 |  7784 | `	pException->iFinallyDone = 0;` |
|      390 |  7785 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  7786 | `	/* Create the exception frame */` |
|      390 |  7787 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|      390 |  7788 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7789 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  7790 | `		goto Abort;` |
|        - |  7791 | `	}` |
|        - |  7792 | `	/* Mark the special frame */` |
|      390 |  7793 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|      390 |  7794 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  7795 | `	/* Point to the frame that trigger the exception */` |
|      390 |  7796 | `	pFrameLocal = pFrameLocal->pParent;` |
|      390 |  7797 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      390 |  7798 | `	pException->pFrame = pFrameLocal;` |
|      390 |  7799 | `	break;` |
|        - |  7800 | `							}` |
|        - |  7801 | `/*` |
|        - |  7802 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  7803 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  7804 | ` */` |
|      193 |  7805 | `case PH7_OP_POP_EXCEPTION: {` |
|      388 |  7806 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|      388 |  7807 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  7808 | `		ph7_exception **apException;` |
|        - |  7809 | `		/* Pop the loaded exception */` |
|       32 |  7810 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       32 |  7811 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       30 |  7812 | `			(void)SySetPop(&pVm->aException);` |
|       14 |  7813 | `		}` |
|       15 |  7814 | `	}` |
|      388 |  7815 | `	pException->pFrame = 0;` |
|        - |  7816 | `	/* Leave the exception frame */` |
|      388 |  7817 | `	VmLeaveFrame(&(*pVm));` |
|        - |  7818 | `	/* Execute the finally block if present and not already executed by catch path */` |
|      388 |  7819 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  7820 | `		sxi32 rcFinally;` |
|       20 |  7821 | `		pException->iFinallyDone = 1;` |
|       20 |  7822 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       20 |  7823 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  7824 | `			goto Abort;` |
|        - |  7825 | `		}` |
|        9 |  7826 | `	}` |
|      388 |  7827 | `	break;` |
|        - |  7828 | `							}` |
|        - |  7829 |  |
|        - |  7830 | `/*` |
|        - |  7831 | ` * OP_THROW * P2 *` |
|        - |  7832 | ` * Throw an user exception.` |
|        - |  7833 | ` */` |
|       80 |  7834 | `case PH7_OP_THROW: {` |
|      162 |  7835 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|      162 |  7836 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  7837 | `#ifdef UNTRUST` |
|        - |  7838 | `	if( pTos < pStack ){` |
|        - |  7839 | `		goto Abort;` |
|        - |  7840 | `	}` |
|        - |  7841 | `#endif` |
|      162 |  7842 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  7843 | `	/* Tell the upper layer that an exception was thrown */` |
|      162 |  7844 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|      162 |  7845 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|      162 |  7846 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7847 | `		ph7_class *pThrowable;` |
|        - |  7848 | `		/* Thrown object must implement the Throwable interface (PHP 7+). */` |
|      162 |  7849 | `		pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|      163 |  7850 | `		if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|        - |  7851 | `			/* Not a Throwable: replace with Error(msg) matching PHP behavior.` |
|        - |  7852 | `			 * Error::__construct is defined in the built-in library and` |
|        - |  7853 | `			 * cannot realistically fail, so we do not check its return. */` |
|        3 |  7854 | `			ph7_class *pErrorClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        3 |  7855 | `			ph7_class_instance *pErrInst = 0;` |
|        3 |  7856 | `			if( pErrorClass ){` |
|        3 |  7857 | `				pErrInst = PH7_NewClassInstance(&(*pVm),pErrorClass);` |
|        1 |  7858 | `			}` |
|        3 |  7859 | `			if( pErrInst ){` |
|        - |  7860 | `				ph7_class_method *pCons;` |
|        3 |  7861 | `				pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|        3 |  7862 | `				if( pCons ){` |
|        - |  7863 | `					ph7_value sArg;` |
|        - |  7864 | `					ph7_value *apArg[1];` |
|        - |  7865 | `					SyString sMsgStr;` |
|        - |  7866 | `					static const char zErrMsg[] =` |
|        - |  7867 | `						"Cannot throw objects that do not implement Throwable";` |
|        3 |  7868 | `					SyStringInitFromBuf(&sMsgStr,zErrMsg,sizeof(zErrMsg)-1);` |
|        3 |  7869 | `					PH7_MemObjInit(pVm,&sArg);` |
|        3 |  7870 | `					PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|        3 |  7871 | `					apArg[0] = &sArg;` |
|        3 |  7872 | `					PH7_VmCallClassMethod(&(*pVm),pErrInst,pCons,0,1,apArg);` |
|        3 |  7873 | `					PH7_MemObjRelease(&sArg);` |
|        1 |  7874 | `				}` |
|        3 |  7875 | `				rc = VmThrowException(&(*pVm),pErrInst);` |
|        3 |  7876 | `				PH7_ClassInstanceUnref(pErrInst);` |
|        3 |  7877 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7878 | `					goto Abort;` |
|        - |  7879 | `				}` |
|        2 |  7880 | `			}else{` |
|        - |  7881 | `				/* Bootstrap failure — fall back to uncaught reporting */` |
|      ! 0 |  7882 | `				rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  7883 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7884 | `					goto Abort;` |
|        - |  7885 | `				}` |
|        - |  7886 | `			}` |
|        2 |  7887 | `		}else{` |
|        - |  7888 | `			/* Throw the exception */` |
|      160 |  7889 | `			rc = VmThrowException(&(*pVm),pThis);` |
|      160 |  7890 | `			if( rc == SXERR_ABORT ){` |
|        - |  7891 | `				/* Abort processing immediately */` |
|       11 |  7892 | `				goto Abort;` |
|        - |  7893 | `			}` |
|        - |  7894 | `		}` |
|       77 |  7895 | `	}else{` |
|        - |  7896 | `		/* Expecting a class instance */` |
|      ! 0 |  7897 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  7898 | `		if( rc == SXERR_ABORT ){` |
|        - |  7899 | `			/* Abort processing immediately */` |
|      ! 0 |  7900 | `			goto Abort;` |
|        - |  7901 | `		}` |
|        - |  7902 | `	}` |
|        - |  7903 | `	/* Pop the top entry */` |
|      152 |  7904 | `	VmPopOperand(&pTos,1);` |
|        - |  7905 | `	/* Perform an unconditional jump */` |
|      152 |  7906 | `	pc = nJump - 1;` |
|      152 |  7907 | `	break;` |
|        - |  7908 | `				   }` |
|        - |  7909 | `/*` |
|        - |  7910 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  7911 | ` * Prepare a foreach step.` |
|        - |  7912 | ` */` |
|     6220 |  7913 | `case PH7_OP_FOREACH_INIT: {` |
|    12442 |  7914 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  7915 | `	void *pName;` |
|        - |  7916 | `#ifdef UNTRUST` |
|        - |  7917 | `	if( pTos < pStack ){` |
|        - |  7918 | `		goto Abort;` |
|        - |  7919 | `	}` |
|        - |  7920 | `#endif` |
|    12442 |  7921 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  7922 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  7923 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7924 | `			/* Force a string cast */` |
|      ! 0 |  7925 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  7926 | `		}` |
|        - |  7927 | `		/* Duplicate name */` |
|      ! 0 |  7928 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  7929 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7930 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7931 | `		}` |
|      ! 0 |  7932 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  7933 | `	}` |
|    12442 |  7934 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  7935 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7936 | `			/* Force a string cast */` |
|      ! 0 |  7937 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  7938 | `		}` |
|        - |  7939 | `		/* Duplicate name */` |
|      ! 0 |  7940 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  7941 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7942 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7943 | `		}` |
|      ! 0 |  7944 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  7945 | `	}` |
|        - |  7946 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|    12442 |  7947 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  7948 | `		/* Jump out of the loop */` |
|      ! 0 |  7949 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  7950 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  7951 | `		}` |
|      ! 0 |  7952 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  7953 | `	}else{` |
|        - |  7954 | `		ph7_foreach_step *pStep;` |
|    12442 |  7955 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|    12442 |  7956 | `		if( pStep == 0 ){` |
|      ! 0 |  7957 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  7958 | `			/* Jump out of the loop */` |
|      ! 0 |  7959 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7960 | `		}else{` |
|        - |  7961 | `			/* Zero the structure */` |
|    12442 |  7962 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  7963 | `			/* Prepare the step */` |
|    12442 |  7964 | `			pStep->iFlags = pInfo->iFlags;` |
|    12442 |  7965 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  7966 | `				ph7_hashmap *pMap;` |
|        - |  7967 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  7968 | `				 * source array so mutations don't affect other sharers. */` |
|    12408 |  7969 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|        9 |  7970 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|        9 |  7971 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|        9 |  7972 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  7973 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  7974 | `						 * variable still points at the same hashmap as` |
|        - |  7975 | `						 * the stack value. */` |
|        9 |  7976 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|        9 |  7977 | `							pCur->iRef--;` |
|        - |  7978 | `							/* Use the returned map, not pBacking->x.pOther: PH7_HashmapDup` |
|        - |  7979 | `							 * inside CowSeparate can reallocate (move) pVm->aMemObj and leave` |
|        - |  7980 | `							 * pBacking dangling. The return value is the post-separation map. */` |
|        9 |  7981 | `							pTos->x.pOther = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|        9 |  7982 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  7983 | `						}` |
|        4 |  7984 | `					}` |
|        4 |  7985 | `				}` |
|    12408 |  7986 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  7987 | `				/* Reset the internal loop cursor */` |
|    12408 |  7988 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  7989 | `				/* Mark the step */` |
|    12408 |  7990 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|    12408 |  7991 | `				pStep->xIter.pMap = pMap;` |
|    12408 |  7992 | `				pMap->iRef++;` |
|     6205 |  7993 | `			}else{` |
|       36 |  7994 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7995 | `				ph7_class *pIteratorClass;` |
|        - |  7996 | `				/* Check if the object implements Iterator */` |
|       36 |  7997 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       47 |  7998 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  7999 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  8000 | `					ph7_class_method *pRewind;` |
|       24 |  8001 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       24 |  8002 | `					pStep->xIter.pThis = pThis;` |
|       24 |  8003 | `					pThis->iRef++;` |
|       24 |  8004 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       24 |  8005 | `					if( pRewind ){` |
|       24 |  8006 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|       11 |  8007 | `					}` |
|       13 |  8008 | `				}else{` |
|        - |  8009 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  8010 | `					ph7_class *pIterAggClass;` |
|       14 |  8011 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  8012 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       15 |  8013 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  8014 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  8015 | `						ph7_class_method *pGetIter;` |
|        3 |  8016 | `						int iterAggOk = 0;` |
|        3 |  8017 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  8018 | `						if( pGetIter ){` |
|        - |  8019 | `							ph7_value sResult;` |
|        3 |  8020 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  8021 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  8022 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  8023 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  8024 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  8025 | `									ph7_class_method *pRewind;` |
|        3 |  8026 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  8027 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  8028 | `									pIterObj->iRef++;` |
|        - |  8029 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  8030 | `									pStep->pOwner = pThis;` |
|        3 |  8031 | `									pThis->iRef++;` |
|        3 |  8032 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  8033 | `									if( pRewind ){` |
|        3 |  8034 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  8035 | `									}` |
|        3 |  8036 | `									iterAggOk = 1;` |
|        1 |  8037 | `								}` |
|        1 |  8038 | `							}` |
|        3 |  8039 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  8040 | `						}` |
|        3 |  8041 | `						if( !iterAggOk ){` |
|        - |  8042 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  8043 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8044 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  8045 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  8046 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  8047 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  8048 | `						}` |
|        2 |  8049 | `					}else{` |
|        - |  8050 | `						/* Plain object iteration via hAttr */` |
|       12 |  8051 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|       12 |  8052 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|       12 |  8053 | `						pStep->xIter.pThis = pThis;` |
|       12 |  8054 | `						pThis->iRef++;` |
|        - |  8055 | `					}` |
|        - |  8056 | `				}` |
|        - |  8057 | `			}` |
|        - |  8058 | `		}` |
|    12442 |  8059 | `		if( pStep ){` |
|    12442 |  8060 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  8061 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  8062 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  8063 | `				/* Jump out of the loop */` |
|      ! 0 |  8064 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  8065 | `			}` |
|     6220 |  8066 | `		}` |
|        - |  8067 | `	}` |
|    12442 |  8068 | `	VmPopOperand(&pTos,1);` |
|    12442 |  8069 | `	break;` |
|        - |  8070 | `						  }` |
|        - |  8071 | `/*` |
|        - |  8072 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  8073 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  8074 | ` */` |
|   102230 |  8075 | `case PH7_OP_FOREACH_STEP: {` |
|   204462 |  8076 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  8077 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  8078 | `	ph7_value *pValue;` |
|        - |  8079 | `	VmFrame *pFrameLocal;` |
|        - |  8080 | `	/* Peek the last step */` |
|   204462 |  8081 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   204462 |  8082 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   204462 |  8083 | `	pFrameLocal = pVm->pFrame;` |
|   204462 |  8084 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   204462 |  8085 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   204328 |  8086 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  8087 | `		ph7_hashmap_node *pNode;` |
|        - |  8088 | `		/* Extract the current node value */` |
|   204328 |  8089 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   204328 |  8090 | `		if( pNode == 0 ){` |
|        - |  8091 | `			/* No more entry to process */` |
|    12406 |  8092 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|    12406 |  8093 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  8094 | `				/* Break the reference with the last element */` |
|        7 |  8095 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  8096 | `			}` |
|        - |  8097 | `			/* Automatically reset the loop cursor */` |
|    12406 |  8098 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  8099 | `			/* Cleanup the mess left behind */` |
|    12406 |  8100 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    12406 |  8101 | `			SySetPop(&pInfo->aStep);` |
|    12406 |  8102 | `			PH7_HashmapUnref(pMap);` |
|     6204 |  8103 | `		}else{` |
|   191924 |  8104 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      528 |  8105 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      528 |  8106 | `				if( pKey ){` |
|      528 |  8107 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      263 |  8108 | `				}` |
|      263 |  8109 | `			}` |
|   191924 |  8110 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  8111 | `				SyHashEntry *pEntry;` |
|        - |  8112 | `				/* Pass by reference */` |
|       23 |  8113 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       23 |  8114 | `				if( pEntry ){` |
|       21 |  8115 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       11 |  8116 | `				}else{` |
|        4 |  8117 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|        2 |  8118 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  8119 | `				}` |
|       12 |  8120 | `			}else{` |
|        - |  8121 | `				/* Make a copy of the entry value */` |
|   191902 |  8122 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   191902 |  8123 | `				if( pValue ){` |
|   191902 |  8124 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    95950 |  8125 | `				}` |
|        - |  8126 | `			}` |
|        2 |  8127 | `		}` |
|   102299 |  8128 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  8129 | `		/* Iterator-based iteration.` |
|        - |  8130 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  8131 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  8132 | `		 */` |
|      106 |  8133 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  8134 | `		ph7_class_method *pMethod;` |
|        - |  8135 | `		ph7_value sResult;` |
|      106 |  8136 | `		int isValid = 0;` |
|        - |  8137 | `		/* Call next() to advance — but skip on the first iteration */` |
|      106 |  8138 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       26 |  8139 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       14 |  8140 | `		}else{` |
|       82 |  8141 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       82 |  8142 | `			if( pMethod ){` |
|       82 |  8143 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       40 |  8144 | `			}` |
|        - |  8145 | `		}` |
|        - |  8146 | `		/* Call valid() */` |
|      106 |  8147 | `		PH7_MemObjInit(pVm,&sResult);` |
|      106 |  8148 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|      106 |  8149 | `		if( pMethod ){` |
|      106 |  8150 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|      106 |  8151 | `			PH7_MemObjToBool(&sResult);` |
|      106 |  8152 | `			isValid = (sResult.x.iVal != 0);` |
|       52 |  8153 | `		}` |
|      106 |  8154 | `		PH7_MemObjRelease(&sResult);` |
|      106 |  8155 | `		if( !isValid ){` |
|        - |  8156 | `			/* Iterator exhausted */` |
|       24 |  8157 | `			pc = pInstr->iP2 - 1;` |
|        - |  8158 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       24 |  8159 | `			if( pStep->pOwner ){` |
|        3 |  8160 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  8161 | `			}` |
|       24 |  8162 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       24 |  8163 | `			SySetPop(&pInfo->aStep);` |
|       24 |  8164 | `			PH7_ClassInstanceUnref(pThis);` |
|       13 |  8165 | `		}else{` |
|        - |  8166 | `			/* Call current() to get value */` |
|       84 |  8167 | `			PH7_MemObjInit(pVm,&sResult);` |
|       84 |  8168 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       84 |  8169 | `			if( pMethod ){` |
|       84 |  8170 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       41 |  8171 | `			}` |
|       84 |  8172 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       84 |  8173 | `			if( pValue ){` |
|       84 |  8174 | `				PH7_MemObjStore(&sResult,pValue);` |
|       41 |  8175 | `			}` |
|       84 |  8176 | `			PH7_MemObjRelease(&sResult);` |
|        - |  8177 | `			/* Call key() if needed */` |
|       84 |  8178 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  8179 | `				ph7_value sKey;` |
|       35 |  8180 | `				PH7_MemObjInit(pVm,&sKey);` |
|       35 |  8181 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       35 |  8182 | `				if( pMethod ){` |
|       35 |  8183 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  8184 | `				}` |
|       35 |  8185 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       35 |  8186 | `				if( pValue ){` |
|       35 |  8187 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  8188 | `				}` |
|       35 |  8189 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  8190 | `			}` |
|        - |  8191 | `		}` |
|       54 |  8192 | `	}else{` |
|       32 |  8193 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       32 |  8194 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  8195 | `		SyHashEntry *pEntry;` |
|        - |  8196 | `		/* Point to the next attribute */` |
|       36 |  8197 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       26 |  8198 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  8199 | `			/* Check access permission */` |
|       38 |  8200 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       24 |  8201 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       22 |  8202 | `					break; /* Access is granted */` |
|        - |  8203 | `			}` |
|        1 |  8204 | `		}` |
|       32 |  8205 | `		if( pEntry == 0 ){` |
|        - |  8206 | `			/* Clean up the mess left behind */` |
|       12 |  8207 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|       12 |  8208 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  8209 | `				/* Break the reference with the last element */` |
|        3 |  8210 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  8211 | `			}` |
|       12 |  8212 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       12 |  8213 | `			SySetPop(&pInfo->aStep);` |
|       12 |  8214 | `			PH7_ClassInstanceUnref(pThis);` |
|        7 |  8215 | `		}else{` |
|       22 |  8216 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  8217 | `			ph7_value *pAttrValue;` |
|       22 |  8218 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  8219 | `				/* Fill with the current attribute name */` |
|       22 |  8220 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       22 |  8221 | `				if( pKey ){` |
|       22 |  8222 | `					SyBlobReset(&pKey->sBlob);` |
|       22 |  8223 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       22 |  8224 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|       10 |  8225 | `				}` |
|       10 |  8226 | `			}` |
|        - |  8227 | `			/* Extract attribute value */` |
|       22 |  8228 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       22 |  8229 | `			if( pAttrValue ){` |
|       22 |  8230 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  8231 | `					/* Pass by reference */` |
|        3 |  8232 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  8233 | `					if( pEntry ){` |
|        3 |  8234 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  8235 | `					}else{` |
|      ! 0 |  8236 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  8237 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  8238 | `					}` |
|        2 |  8239 | `				}else{` |
|        - |  8240 | `					/* Make a copy of the attribute value */` |
|       20 |  8241 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       20 |  8242 | `					if( pValue ){` |
|       20 |  8243 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        9 |  8244 | `					}` |
|        - |  8245 | `				}` |
|       10 |  8246 | `			}` |
|        - |  8247 | `		}` |
|        - |  8248 | `	}` |
|   204462 |  8249 | `	break;` |
|        - |  8250 | `						  }` |
|        - |  8251 | `/*` |
|        - |  8252 | ` * OP_MEMBER P1 P2` |
|        - |  8253 | ` * Load class attribute/method on the stack.` |
|        - |  8254 | ` */` |
|     4150 |  8255 | `case PH7_OP_MEMBER: {` |
|        - |  8256 | `	ph7_class_instance *pThis;` |
|        - |  8257 | `	ph7_value *pNos;` |
|        - |  8258 | `	SyString sName;` |
|     8302 |  8259 | `	if( !pInstr->iP1 ){` |
|     8062 |  8260 | `		pNos = &pTos[-1];` |
|        - |  8261 | `#ifdef UNTRUST` |
|        - |  8262 | `		if( pNos < pStack ){` |
|        - |  8263 | `			goto Abort;` |
|        - |  8264 | `		}` |
|        - |  8265 | `#endif` |
|     8062 |  8266 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  8267 | `			ph7_class *pClass;` |
|        - |  8268 | `			/* Class already instantiated */` |
|     8060 |  8269 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  8270 | `			/* Point to the instantiated class */` |
|     8060 |  8271 | `			pClass = pThis->pClass;` |
|        - |  8272 | `			/* Extract attribute name first */` |
|     8060 |  8273 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     8060 |  8274 | `			if( pInstr->iP2 ){` |
|        - |  8275 | `				/* Method call */` |
|      794 |  8276 | `				ph7_class_method *pMeth = 0;` |
|      794 |  8277 | `				if( sName.nByte > 0 ){` |
|        - |  8278 | `					/* Extract the target method */` |
|      794 |  8279 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      396 |  8280 | `				}` |
|      794 |  8281 | `				if( pMeth == 0 ){` |
|      ! 0 |  8282 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  8283 | `						&pClass->sName,&sName` |
|        - |  8284 | `						);` |
|        - |  8285 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  8286 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  8287 | `					/* Pop the method name from the stack */` |
|      ! 0 |  8288 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  8289 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  8290 | `				}else{` |
|        - |  8291 | `					/* Push method name on the stack */` |
|      794 |  8292 | `					PH7_MemObjRelease(pTos);` |
|      794 |  8293 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      794 |  8294 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  8295 | `				}` |
|      794 |  8296 | `				pTos->nIdx = SXU32_HIGH;` |
|      398 |  8297 | `			}else{` |
|        - |  8298 | `				/* Attribute access */` |
|     7268 |  8299 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  8300 | `				SyHashEntry *pEntry;` |
|        - |  8301 | `				/* Extract the target attribute */` |
|     7268 |  8302 | `				if( sName.nByte > 0 ){` |
|     7268 |  8303 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     7268 |  8304 | `					if( pEntry ){` |
|        - |  8305 | `						/* Point to the attribute value */` |
|     7266 |  8306 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     3632 |  8307 | `					}` |
|     3633 |  8308 | `				}` |
|     7268 |  8309 | `				if( pObjAttr == 0 ){` |
|        - |  8310 | `					/* No such attribute,load null */` |
|        4 |  8311 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  8312 | `						&pClass->sName,&sName);` |
|        - |  8313 | `					/* Call the __get magic method if available */` |
|        3 |  8314 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  8315 | `				}` |
|     7268 |  8316 | `				VmPopOperand(&pTos,1);` |
|        - |  8317 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  8318 | `				 * This is due to the following case:` |
|        - |  8319 | `				 *     (new TestClass())->foo;` |
|        - |  8320 | `				 */` |
|     7268 |  8321 | `				pThis->iRef++;` |
|     7268 |  8322 | `				PH7_MemObjRelease(pTos);` |
|     7268 |  8323 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     7268 |  8324 | `				if( pObjAttr ){` |
|     7266 |  8325 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  8326 | `					/* Check attribute access */` |
|     7266 |  8327 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){` |
|        - |  8328 | `						/* PHP 7.4+: reading an uninitialized typed property is an Error.` |
|        - |  8329 | `						 * We can only raise it on a real read, not when the slot is the` |
|        - |  8330 | `						 * LHS of an assignment — peek at the next instruction to decide.` |
|        - |  8331 | `						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so` |
|        - |  8332 | `						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */` |
|     7264 |  8333 | `						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|     3674 |  8334 | `						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|       82 |  8335 | `							VmInstr *pNext = pInstr + 1;` |
|       82 |  8336 | `							int bIsLhs = 0;` |
|       82 |  8337 | `							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|       80 |  8338 | `								bIsLhs = 1;` |
|       39 |  8339 | `							}` |
|       82 |  8340 | `							if( !bIsLhs ){` |
|        3 |  8341 | `								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);` |
|        3 |  8342 | `								PH7_ClassInstanceUnref(pThis);` |
|        3 |  8343 | `								if( rcU == PH7_ABORT ){` |
|      ! 0 |  8344 | `									goto Abort;` |
|        - |  8345 | `								}` |
|        - |  8346 | `								{` |
|        3 |  8347 | `									VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  8348 | `									if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  8349 | `										pc = pFrm2->iExceptionJump - 1;` |
|     4150 |  8350 | `										break;` |
|        - |  8351 | `									}` |
|        - |  8352 | `								}` |
|      ! 0 |  8353 | `								goto Exception;` |
|        - |  8354 | `							}` |
|       39 |  8355 | `						}` |
|        - |  8356 | `						/* Load attribute */` |
|     7264 |  8357 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     7264 |  8358 | `						if( pValue ){` |
|     7264 |  8359 | `							if( pThis->iRef < 2 ){` |
|        - |  8360 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  8361 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  8362 | `								 */` |
|        7 |  8363 | `								PH7_MemObjStore(pValue,pTos);` |
|        4 |  8364 | `							}else{` |
|        - |  8365 | `								/* Simple load */` |
|     7258 |  8366 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  8367 | `							}` |
|     7264 |  8368 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     7262 |  8369 | `								if( pThis->iRef > 1 ){` |
|        - |  8370 | `									/* Load attribute index */` |
|     7256 |  8371 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     3627 |  8372 | `								}` |
|     3630 |  8373 | `							}` |
|     3631 |  8374 | `						}` |
|     3633 |  8375 | `					}else{` |
|        - |  8376 | `						/* Throw Error exception (PHP-compatible).` |
|        - |  8377 | `						 * Build message before unref — pObjAttr belongs to pThis->hAttr. */` |
|        - |  8378 | `						char zMsg[256];` |
|      ! 0 |  8379 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  8380 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  8381 | `							zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  8382 | `							(int)pObjAttr->pAttr->sName.nByte,pObjAttr->pAttr->sName.zString);` |
|      ! 0 |  8383 | `						PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8384 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  8385 | `						goto Abort;` |
|        - |  8386 | `					}` |
|     3631 |  8387 | `				}` |
|        - |  8388 | `				/* Safely unreference the object */` |
|     7266 |  8389 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  8390 | `			}` |
|     4030 |  8391 | `		}else{` |
|        3 |  8392 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|        3 |  8393 | `			VmPopOperand(&pTos,1);` |
|        3 |  8394 | `			PH7_MemObjRelease(pTos);` |
|        3 |  8395 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  8396 | `		}` |
|     4031 |  8397 | `	}else{` |
|        - |  8398 | `		/* Static member access using class name */` |
|      242 |  8399 | `		pNos = pTos;` |
|      242 |  8400 | `		pThis = 0;` |
|      242 |  8401 | `		if( !pInstr->p3 ){` |
|      192 |  8402 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      192 |  8403 | `			pNos--;` |
|        - |  8404 | `#ifdef UNTRUST` |
|        - |  8405 | `			if( pNos < pStack ){` |
|        - |  8406 | `				goto Abort;` |
|        - |  8407 | `			}` |
|        - |  8408 | `#endif` |
|       97 |  8409 | `		}else{` |
|        - |  8410 | `			/* Attribute name already computed */` |
|       52 |  8411 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  8412 | `		}` |
|      242 |  8413 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      242 |  8414 | `			ph7_class *pClass = 0;` |
|      242 |  8415 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  8416 | `				/* Class already instantiated */` |
|        5 |  8417 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        5 |  8418 | `				pClass = pThis->pClass;` |
|        5 |  8419 | `				pThis->iRef++; /* Deffer garbage collection */` |
|        3 |  8420 | `			}else{` |
|        - |  8421 | `				/* Try to extract the target class */` |
|      238 |  8422 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      238 |  8423 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      238 |  8424 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  8425 | `					/* Handle self/static/parent keywords */` |
|      238 |  8426 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       62 |  8427 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       62 |  8428 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  8429 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  8430 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  8431 | `						}` |
|      208 |  8432 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       28 |  8433 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      178 |  8434 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       28 |  8435 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       28 |  8436 | `						if( pSelf && pSelf->pBase ){` |
|       28 |  8437 | `							pClass = pSelf->pBase;` |
|       13 |  8438 | `						}` |
|       15 |  8439 | `					}else{` |
|      126 |  8440 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  8441 | `					}` |
|      118 |  8442 | `				}` |
|        - |  8443 | `			}` |
|      242 |  8444 | `			if( pClass == 0 ){` |
|        - |  8445 | `				/* Undefined class */` |
|      ! 0 |  8446 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  8447 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  8448 | `					);` |
|      ! 0 |  8449 | `				if( !pInstr->p3 ){` |
|      ! 0 |  8450 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  8451 | `				}` |
|      ! 0 |  8452 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  8453 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  8454 | `			}else{` |
|      242 |  8455 | `				if( pInstr->iP2 ){` |
|        - |  8456 | `					/* Method call */` |
|       86 |  8457 | `					ph7_class_method *pMeth = 0;` |
|       86 |  8458 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  8459 | `						/* Extract the target method */` |
|       86 |  8460 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       42 |  8461 | `					}` |
|       86 |  8462 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  8463 | `						if( pMeth ){` |
|      ! 0 |  8464 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  8465 | `								&pClass->sName,&sName` |
|        - |  8466 | `								);` |
|      ! 0 |  8467 | `						}else{` |
|      ! 0 |  8468 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  8469 | `								&pClass->sName,&sName` |
|        - |  8470 | `								);` |
|        - |  8471 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  8472 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  8473 | `						}` |
|        - |  8474 | `						/* Pop the method name from the stack */` |
|      ! 0 |  8475 | `						if( !pInstr->p3 ){` |
|      ! 0 |  8476 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  8477 | `						}` |
|      ! 0 |  8478 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  8479 | `					}else{` |
|        - |  8480 | `						/* Push method name on the stack */` |
|       86 |  8481 | `						PH7_MemObjRelease(pTos);` |
|       86 |  8482 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       86 |  8483 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  8484 | `					}` |
|       86 |  8485 | `					pTos->nIdx = SXU32_HIGH;` |
|       44 |  8486 | `				}else{` |
|        - |  8487 | `					/* Attribute access */` |
|      158 |  8488 | `					ph7_class_attr *pAttr = 0;` |
|        - |  8489 | `					/* Check for special ::class pseudo-constant */` |
|      204 |  8490 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       92 |  8491 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  8492 | `						/* ::class returns the fully qualified class name */` |
|        - |  8493 | `						/* Pop the attribute name from the stack */` |
|       60 |  8494 | `						if( !pInstr->p3 ){` |
|       60 |  8495 | `							VmPopOperand(&pTos,1);` |
|       29 |  8496 | `						}` |
|       60 |  8497 | `						PH7_MemObjRelease(pTos);` |
|        - |  8498 | `						/* Load the class name */` |
|       60 |  8499 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       60 |  8500 | `						pTos->nIdx = SXU32_HIGH;` |
|       31 |  8501 | `					}else{` |
|        - |  8502 | `						/* Extract the target attribute */` |
|      100 |  8503 | `						if( sName.nByte > 0 ){` |
|      100 |  8504 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|       49 |  8505 | `						}` |
|      100 |  8506 | `						if( pAttr == 0 ){` |
|        - |  8507 | `							/* No such attribute,load null */` |
|      ! 0 |  8508 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  8509 | `								&pClass->sName,&sName);` |
|        - |  8510 | `							/* Call the __get magic method if available */` |
|      ! 0 |  8511 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  8512 | `						}` |
|        - |  8513 | `						/* Pop the attribute name from the stack */` |
|      100 |  8514 | `						if( !pInstr->p3 ){` |
|       50 |  8515 | `							VmPopOperand(&pTos,1);` |
|       24 |  8516 | `						}` |
|      100 |  8517 | `						PH7_MemObjRelease(pTos);` |
|      100 |  8518 | `						pTos->nIdx = SXU32_HIGH;` |
|      100 |  8519 | `						if( pAttr ){` |
|      100 |  8520 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  8521 | `								/* Access to a non static attribute */` |
|      ! 0 |  8522 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  8523 | `									&pClass->sName,&pAttr->sName` |
|        - |  8524 | `									);` |
|      ! 0 |  8525 | `							}else{` |
|        - |  8526 | `								ph7_value *pValue;` |
|        - |  8527 | `								/* Check if the access to the attribute is allowed */` |
|      100 |  8528 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        - |  8529 | `									/* PHP 7.4+: uninitialized typed static read.` |
|        - |  8530 | `									 * Same LHS-of-store peek as the instance path. */` |
|       94 |  8531 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|       68 |  8532 | `									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       59 |  8533 | `										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,` |
|       38 |  8534 | `											(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|       40 |  8535 | `										if( pS ){` |
|       40 |  8536 | `											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;` |
|       40 |  8537 | `											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){` |
|        8 |  8538 | `												VmInstr *pNext = pInstr + 1;` |
|        8 |  8539 | `												int bIsLhs = 0;` |
|        8 |  8540 | `												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|        6 |  8541 | `													bIsLhs = 1;` |
|        2 |  8542 | `												}` |
|        8 |  8543 | `												if( !bIsLhs ){` |
|        3 |  8544 | `													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);` |
|        3 |  8545 | `													if( pThis ){` |
|      ! 0 |  8546 | `														PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8547 | `													}` |
|        3 |  8548 | `													if( rcU == PH7_ABORT ){` |
|      ! 0 |  8549 | `														goto Abort;` |
|        - |  8550 | `													}` |
|        - |  8551 | `													{` |
|        3 |  8552 | `														VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  8553 | `														if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  8554 | `															pc = pFrm2->iExceptionJump - 1;` |
|        3 |  8555 | `															break;` |
|        - |  8556 | `														}` |
|        - |  8557 | `													}` |
|      ! 0 |  8558 | `													goto Exception;` |
|        - |  8559 | `												}` |
|        2 |  8560 | `											}` |
|       18 |  8561 | `										}` |
|       18 |  8562 | `									}` |
|        - |  8563 | `									/* Load the desired attribute */` |
|       94 |  8564 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       94 |  8565 | `									if( pValue ){` |
|       94 |  8566 | `										PH7_MemObjLoad(pValue,pTos);` |
|       94 |  8567 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  8568 | `											/* Load index number */` |
|       50 |  8569 | `											pTos->nIdx = pAttr->nIdx;` |
|       24 |  8570 | `										}` |
|       46 |  8571 | `									}` |
|       48 |  8572 | `								}else{` |
|        - |  8573 | `									/* Throw Error exception (PHP-compatible) */` |
|        - |  8574 | `									char zMsg[256];` |
|        5 |  8575 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|        5 |  8576 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|        7 |  8577 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",` |
|        4 |  8578 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|        4 |  8579 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        3 |  8580 | `									}else{` |
|      ! 0 |  8581 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  8582 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  8583 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        - |  8584 | `									}` |
|        5 |  8585 | `									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|        5 |  8586 | `									goto Abort;` |
|        - |  8587 | `								}` |
|        - |  8588 | `							}` |
|       46 |  8589 | `						}` |
|        - |  8590 | `					}` |
|        - |  8591 | `				}` |
|      236 |  8592 | `				if( pThis ){` |
|        - |  8593 | `					/* Safely unreference the object */` |
|        5 |  8594 | `					PH7_ClassInstanceUnref(pThis);` |
|        2 |  8595 | `				}` |
|        - |  8596 | `			}` |
|      119 |  8597 | `		}else{` |
|        - |  8598 | `			/* Pop operands */` |
|      ! 0 |  8599 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  8600 | `			if( !pInstr->p3 ){` |
|      ! 0 |  8601 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  8602 | `			}` |
|      ! 0 |  8603 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  8604 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  8605 | `		}` |
|        - |  8606 | `	}` |
|     8294 |  8607 | `	break;` |
|        - |  8608 | `					}` |
|        - |  8609 | `/*` |
|        - |  8610 | ` * OP_NEW P1 * * *` |
|        - |  8611 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  8612 | ` */` |
|      673 |  8613 | `case PH7_OP_NEW: {` |
|     1348 |  8614 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|     1348 |  8615 | `	ph7_class *pClass = 0;` |
|        - |  8616 | `	ph7_class_instance *pNew;` |
|     1348 |  8617 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  8618 | `		/* Try to extract the desired class */` |
|     2021 |  8619 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|     1346 |  8620 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      673 |  8621 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  8622 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  8623 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  8624 | `	}` |
|     1348 |  8625 | `	if( pClass == 0 ){` |
|        - |  8626 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  8627 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  8628 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  8629 | `			);` |
|        - |  8630 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  8631 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  8632 | `		if( pInstr->iP1 > 0 ){` |
|        - |  8633 | `			/* Pop given arguments */` |
|      ! 0 |  8634 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  8635 | `		}` |
|      ! 0 |  8636 | `		goto Abort;` |
|      ! 0 |  8637 | `	}else{` |
|        - |  8638 | `		ph7_class_method *pCons;` |
|        - |  8639 | `		/* Create a new class instance */` |
|     1348 |  8640 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|     1348 |  8641 | `		if( pNew == 0 ){` |
|      ! 0 |  8642 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8643 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  8644 | `				&pClass->sName` |
|        - |  8645 | `			);` |
|      ! 0 |  8646 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  8647 | `			if( pInstr->iP1 > 0 ){` |
|        - |  8648 | `				/* Pop given arguments */` |
|      ! 0 |  8649 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  8650 | `			}` |
|      ! 0 |  8651 | `			break;` |
|        - |  8652 | `		}` |
|        - |  8653 | `		/* Check if a constructor is available */` |
|     1348 |  8654 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|     1348 |  8655 | `		if( pCons == 0 ){` |
|      948 |  8656 | `			SyString *pName = &pClass->sName;` |
|        - |  8657 | `			/* Check for a constructor with the same base class name */` |
|      948 |  8658 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      473 |  8659 | `		}` |
|     1348 |  8660 | `		if( pCons ){` |
|        - |  8661 | `			/* Call the class constructor.  Collect args in stack order and` |
|        - |  8662 | `			 * forward any VmCallArgMap from the NEW instruction so the` |
|        - |  8663 | `			 * receiving OP_CALL path runs its named-argument matching` |
|        - |  8664 | `			 * (including variadic string-key packing). */` |
|      402 |  8665 | `			VmCallArgMap *pNewMap = (VmCallArgMap *)pInstr->p3;` |
|        - |  8666 | `			sxi32 rcCons;` |
|      402 |  8667 | `			SySetReset(&aArg);` |
|      786 |  8668 | `			while( pArg < pTos ){` |
|      386 |  8669 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      386 |  8670 | `				pArg++;` |
|        2 |  8671 | `			}` |
|      402 |  8672 | `			if( pVm->bErrReport && !(pNewMap && pNewMap->bHasNamed) ){` |
|        - |  8673 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  8674 | `				sxu32 n;` |
|      118 |  8675 | `				n = SySetUsed(&aArg);` |
|        - |  8676 | `				/* Emit a notice for missing arguments (positional-only:` |
|        - |  8677 | `				 * for named args the missing-arg check happens downstream` |
|        - |  8678 | `				 * after resolution). */` |
|      234 |  8679 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|      118 |  8680 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|      118 |  8681 | `					if( pFuncArg ){` |
|      118 |  8682 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  8683 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  8684 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  8685 | `						}` |
|       58 |  8686 | `					}` |
|      118 |  8687 | `					n++;` |
|        2 |  8688 | `				}` |
|       58 |  8689 | `			}` |
|      402 |  8690 | `			rcCons = VmCallClassMethodWithMap(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pNewMap);` |
|        - |  8691 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      402 |  8692 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  8693 | `				pNew->iRef = 1;` |
|      ! 0 |  8694 | `			}` |
|      402 |  8695 | `			if( rcCons == PH7_ABORT \|\| rcCons == PH7_EXCEPTION ){` |
|        - |  8696 | `				/* The constructor raised: the half-constructed object must not` |
|        - |  8697 | `				 * become the NEW result. Drop our reference so it is destroyed.` |
|        - |  8698 | `				 * The class-name operand (and any leftover args) are released by` |
|        - |  8699 | `				 * the Abort/Exception unwind, or explicitly on the resume path. */` |
|        - |  8700 | `				sxi32 iResumePc;` |
|        5 |  8701 | `				PH7_ClassInstanceUnref(pNew);` |
|        5 |  8702 | `				if( rcCons == PH7_ABORT ){` |
|      ! 0 |  8703 | `					goto Abort;` |
|        - |  8704 | `				}` |
|        5 |  8705 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        - |  8706 | `					/* This frame's own try caught it in-place: tidy the stack` |
|        - |  8707 | `					 * (pop ctor args + release the class-name slot) and resume. */` |
|        5 |  8708 | `					if( pInstr->iP1 > 0 ){` |
|        3 |  8709 | `						VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  8710 | `					}` |
|        5 |  8711 | `					PH7_MemObjRelease(pTos);` |
|        5 |  8712 | `					pc = iResumePc;` |
|        5 |  8713 | `					break;` |
|        - |  8714 | `				}` |
|      ! 0 |  8715 | `				goto Exception;` |
|        - |  8716 | `			}` |
|      198 |  8717 | `		}` |
|     1344 |  8718 | `		if( pInstr->iP1 > 0 ){` |
|        - |  8719 | `			/* Pop given arguments */` |
|      316 |  8720 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      157 |  8721 | `		}` |
|     1344 |  8722 | `		PH7_MemObjRelease(pTos);` |
|     1344 |  8723 | `		pTos->x.pOther = pNew;` |
|     1344 |  8724 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  8725 | `	}` |
|     1344 |  8726 | `	break;` |
|        - |  8727 | `				 }` |
|        - |  8728 | `/*` |
|        - |  8729 | ` * OP_CLONE * * *` |
|        - |  8730 | ` * Perfome a clone operation.` |
|        - |  8731 | ` */` |
|       24 |  8732 | `case PH7_OP_CLONE: {` |
|        - |  8733 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  8734 | `#ifdef UNTRUST` |
|        - |  8735 | `	if( pTos < pStack ){` |
|        - |  8736 | `		goto Abort;` |
|        - |  8737 | `	}` |
|        - |  8738 | `#endif` |
|        - |  8739 | `	/* Make sure we are dealing with a class instance */` |
|       50 |  8740 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  8741 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8742 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  8743 | `		PH7_MemObjRelease(pTos);` |
|        5 |  8744 | `		break;` |
|        - |  8745 | `	}` |
|        - |  8746 | `	/* Point to the source */` |
|       46 |  8747 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  8748 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       46 |  8749 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  8750 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8751 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  8752 | `			&pSrc->pClass->sName);` |
|      ! 0 |  8753 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  8754 | `		break;` |
|        - |  8755 | `	}` |
|        - |  8756 | `	/* Perform the clone operation */` |
|       46 |  8757 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       46 |  8758 | `	PH7_MemObjRelease(pTos);` |
|       46 |  8759 | `	if( pClone == 0 ){` |
|      ! 0 |  8760 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8761 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  8762 | `	}else{` |
|        - |  8763 | `		/* Load the cloned object */` |
|       46 |  8764 | `		pTos->x.pOther = pClone;` |
|       46 |  8765 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  8766 | `	}` |
|       46 |  8767 | `	break;` |
|        - |  8768 | `				   }` |
|        - |  8769 | `/*` |
|        - |  8770 | ` * OP_SWITCH * * P3` |
|        - |  8771 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  8772 | ` */` |
|       26 |  8773 | `case PH7_OP_SWITCH: {` |
|       54 |  8774 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  8775 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  8776 | `	ph7_value sValue,sCaseValue;` |
|        - |  8777 | `	sxu32 n,nEntry;` |
|        - |  8778 | `#ifdef UNTRUST` |
|        - |  8779 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  8780 | `		goto Abort;` |
|        - |  8781 | `	}` |
|        - |  8782 | `#endif` |
|        - |  8783 | `	/* Point to the case table  */` |
|       54 |  8784 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       54 |  8785 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  8786 | `	/* Select the appropriate case block to execute */` |
|       54 |  8787 | `	PH7_MemObjInit(pVm,&sValue);` |
|       54 |  8788 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|      132 |  8789 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|      130 |  8790 | `		pCase = &aCase[n];` |
|      130 |  8791 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  8792 | `		/* Execute the case expression first */` |
|      130 |  8793 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  8794 | `		/* Compare the two expression */` |
|      130 |  8795 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|      130 |  8796 | `		PH7_MemObjRelease(&sValue);` |
|      130 |  8797 | `		PH7_MemObjRelease(&sCaseValue);` |
|      130 |  8798 | `		if( rc == 0 ){` |
|        - |  8799 | `			/* Value match,jump to this block */` |
|       52 |  8800 | `			pc = pCase->nStart - 1;` |
|       52 |  8801 | `			break;` |
|        - |  8802 | `		}` |
|       41 |  8803 | `	}` |
|       54 |  8804 | `	VmPopOperand(&pTos,1);` |
|       54 |  8805 | `	if( n >= nEntry ){` |
|        - |  8806 | `		/* No approprite case to execute,jump to the default case */` |
|        3 |  8807 | `		if( pSwitch->nDefault > 0 ){` |
|        3 |  8808 | `			pc = pSwitch->nDefault - 1;` |
|        2 |  8809 | `		}else{` |
|        - |  8810 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  8811 | `			pc = pSwitch->nOut - 1;` |
|        - |  8812 | `		}` |
|        1 |  8813 | `	}` |
|       54 |  8814 | `	break;` |
|        - |  8815 | `					}` |
|        - |  8816 | `/*` |
|        - |  8817 | ` * OP_MATCH * * P3` |
|        - |  8818 | ` *  PHP 8.0 match expression. P3 points to a ph7_match struct holding` |
|        - |  8819 | ` *  the compiled arms. On entry, the subject is on top of the stack.` |
|        - |  8820 | ` *  On exit, the stack slot holds the matched arm's result value.` |
|        - |  8821 | ` *  Comparison is strict (===). No fallthrough. When no arm matches and` |
|        - |  8822 | ` *  no default is present, a fatal UnhandledMatchError is raised.` |
|        - |  8823 | ` */` |
|       54 |  8824 | `case PH7_OP_MATCH: {` |
|      110 |  8825 | `	ph7_match *pMatch = (ph7_match *)pInstr->p3;` |
|      110 |  8826 | `	ph7_match_arm *aArm,*pArm,*pDefault = 0;` |
|        - |  8827 | `	ph7_value sSubject,sCond,sResult;` |
|        - |  8828 | `	sxu32 i,j,nArm,nCond;` |
|      110 |  8829 | `	int matched = 0;` |
|        - |  8830 | `#ifdef UNTRUST` |
|        - |  8831 | `	if( pMatch == 0 \|\| pTos < pStack ){` |
|        - |  8832 | `		goto Abort;` |
|        - |  8833 | `	}` |
|        - |  8834 | `#endif` |
|      110 |  8835 | `	aArm = (ph7_match_arm *)SySetBasePtr(&pMatch->aArms);` |
|      110 |  8836 | `	nArm = SySetUsed(&pMatch->aArms);` |
|      110 |  8837 | `	PH7_MemObjInit(pVm,&sSubject);` |
|      110 |  8838 | `	PH7_MemObjInit(pVm,&sCond);` |
|      110 |  8839 | `	PH7_MemObjInit(pVm,&sResult);` |
|      110 |  8840 | `	PH7_MemObjLoad(pTos,&sSubject);` |
|      348 |  8841 | `	for( i = 0; i < nArm && !matched; ++i ){` |
|      240 |  8842 | `		pArm = &aArm[i];` |
|      240 |  8843 | `		if( pArm->bDefault ){` |
|       13 |  8844 | `			pDefault = pArm;` |
|       13 |  8845 | `			continue;` |
|        - |  8846 | `		}` |
|      228 |  8847 | `		nCond = SySetUsed(&pArm->aConds);` |
|      394 |  8848 | `		for( j = 0; j < nCond; ++j ){` |
|      260 |  8849 | `			SySet *pCondBc = (SySet *)SySetAt(&pArm->aConds,j);` |
|      260 |  8850 | `			if( pCondBc == 0 ){` |
|      ! 0 |  8851 | `				continue;` |
|        - |  8852 | `			}` |
|      260 |  8853 | `			VmLocalExec(pVm,pCondBc,&sCond);` |
|      260 |  8854 | `			rc = PH7_MemObjCmp(&sSubject,&sCond,TRUE /* strict */,0);` |
|      260 |  8855 | `			PH7_MemObjRelease(&sCond);` |
|      260 |  8856 | `			if( rc == 0 ){` |
|       93 |  8857 | `				VmLocalExec(pVm,&pArm->aResult,&sResult);` |
|       93 |  8858 | `				matched = 1;` |
|       93 |  8859 | `				break;` |
|        - |  8860 | `			}` |
|       85 |  8861 | `		}` |
|      115 |  8862 | `	}` |
|      110 |  8863 | `	if( !matched && pDefault ){` |
|       13 |  8864 | `		VmLocalExec(pVm,&pDefault->aResult,&sResult);` |
|       13 |  8865 | `		matched = 1;` |
|        6 |  8866 | `	}` |
|      110 |  8867 | `	if( !matched ){` |
|        5 |  8868 | `		const char *zType = "unknown";` |
|        - |  8869 | `		char zMsg[128];` |
|        - |  8870 | `		sxu32 nMsg;` |
|        5 |  8871 | `		switch(sSubject.iFlags & MEMOBJ_ALL){` |
|      ! 0 |  8872 | `		case MEMOBJ_NULL:   zType = "null";   break;` |
|      ! 0 |  8873 | `		case MEMOBJ_BOOL:   zType = "bool";   break;` |
|        5 |  8874 | `		case MEMOBJ_INT:    zType = "int";    break;` |
|      ! 0 |  8875 | `		case MEMOBJ_REAL:   zType = "float";  break;` |
|      ! 0 |  8876 | `		case MEMOBJ_STRING: zType = "string"; break;` |
|      ! 0 |  8877 | `		case MEMOBJ_HASHMAP:zType = "array";  break;` |
|      ! 0 |  8878 | `		case MEMOBJ_OBJ:    zType = "object"; break;` |
|      ! 0 |  8879 | `		case MEMOBJ_RES:    zType = "resource"; break;` |
|      ! 0 |  8880 | `		default: break;` |
|        - |  8881 | `		}` |
|        7 |  8882 | `		nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        2 |  8883 | `			"Unhandled match case of type %s",zType);` |
|        7 |  8884 | `		VmReportUncaughtException(&(*pVm),"UnhandledMatchError",` |
|        2 |  8885 | `			sizeof("UnhandledMatchError")-1,zMsg,nMsg,0,0);` |
|        5 |  8886 | `		PH7_MemObjRelease(&sSubject);` |
|        5 |  8887 | `		PH7_MemObjRelease(&sResult);` |
|        5 |  8888 | `		goto Abort;` |
|        - |  8889 | `	}` |
|      105 |  8890 | `	PH7_MemObjRelease(&sSubject);` |
|        - |  8891 | `	/* Replace subject on TOS with the arm result */` |
|      105 |  8892 | `	PH7_MemObjStore(&sResult,pTos);` |
|      105 |  8893 | `	PH7_MemObjRelease(&sResult);` |
|      105 |  8894 | `	break;` |
|        - |  8895 | `					}` |
|        - |  8896 | `/*` |
|        - |  8897 | ` * OP_YIELD P1 P2 *` |
|        - |  8898 | ` *  Yield a value from a generator function.` |
|        - |  8899 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  8900 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  8901 | ` */` |
|       62 |  8902 | `case PH7_OP_YIELD: {` |
|        - |  8903 | `	ph7_generator *pGen;` |
|      126 |  8904 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  8905 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  8906 | `		goto Abort;` |
|        - |  8907 | `	}` |
|      126 |  8908 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|      126 |  8909 | `	if( pInstr->iP2 ){` |
|        - |  8910 | `		/* yield $key => $value: value on top, key below */` |
|        - |  8911 | `#ifdef UNTRUST` |
|        - |  8912 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  8913 | `#endif` |
|       20 |  8914 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       20 |  8915 | `		VmPopOperand(&pTos, 1);` |
|       20 |  8916 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|       20 |  8917 | `		VmPopOperand(&pTos, 1);` |
|        - |  8918 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|       20 |  8919 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  8920 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  8921 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  8922 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  8923 | `			}` |
|        2 |  8924 | `		}` |
|      117 |  8925 | `	}else if( pInstr->iP1 ){` |
|        - |  8926 | `		/* yield $value */` |
|        - |  8927 | `#ifdef UNTRUST` |
|        - |  8928 | `		if( pTos < pStack ) goto Abort;` |
|        - |  8929 | `#endif` |
|      108 |  8930 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|      108 |  8931 | `		VmPopOperand(&pTos, 1);` |
|        - |  8932 | `		/* Auto-increment key */` |
|      108 |  8933 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      108 |  8934 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      108 |  8935 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       55 |  8936 | `	}else{` |
|        - |  8937 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  8938 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  8939 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  8940 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  8941 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  8942 | `	}` |
|        - |  8943 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|      126 |  8944 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|      126 |  8945 | `	goto Suspend;` |
|        - |  8946 |  |
|        - |  8947 | `/*` |
|        - |  8948 | ` * OP_CALL P1 * *` |
|        - |  8949 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  8950 | ` *  function on the stack.` |
|        - |  8951 | ` */` |
|   360301 |  8952 | `case PH7_OP_CALL: {` |
|   720648 |  8953 | `	sxi32 nCallArgs = pInstr->iP1 + pVm->iSpreadExtra;` |
|        - |  8954 | `	ph7_value *pArg;` |
|   720648 |  8955 | `	pVm->iSpreadExtra = 0; /* Always reset, even if zero */` |
|   720648 |  8956 | `	pArg = &pTos[-nCallArgs];` |
|        - |  8957 | `	SyHashEntry *pEntry;` |
|        - |  8958 | `	SyString sName;` |
|        - |  8959 | `	/* Extract function name */` |
|   720648 |  8960 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|       86 |  8961 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  8962 | `			ph7_value sResult;` |
|        - |  8963 | `			sxi32 rcArr;` |
|        3 |  8964 | `			SySetReset(&aArg);` |
|        3 |  8965 | `			while( pArg < pTos ){` |
|      ! 0 |  8966 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  8967 | `				pArg++;` |
|      ! 0 |  8968 | `			}` |
|        3 |  8969 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  8970 | `			/* May be a class instance and it's static method */` |
|        3 |  8971 | `			rcArr = PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|        3 |  8972 | `			SySetReset(&aArg);` |
|        - |  8973 | `			/* Pop given arguments */` |
|        3 |  8974 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8975 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8976 | `			}` |
|        3 |  8977 | `			if( rcArr == PH7_ABORT ){` |
|      ! 0 |  8978 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8979 | `				goto Abort;` |
|        - |  8980 | `			}` |
|        3 |  8981 | `			if( rcArr == PH7_EXCEPTION ){` |
|        - |  8982 | `				/* An array callable ([$obj,'m']()) raised: resume after this frame's` |
|        - |  8983 | `				 * try if it caught the exception in-place, otherwise propagate. */` |
|        - |  8984 | `				sxi32 iResumePc;` |
|        3 |  8985 | `				PH7_MemObjRelease(&sResult);` |
|        3 |  8986 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        3 |  8987 | `					PH7_MemObjRelease(pTos);` |
|        3 |  8988 | `					pc = iResumePc;` |
|        3 |  8989 | `					break;` |
|        - |  8990 | `				}` |
|      ! 0 |  8991 | `				goto Exception;` |
|        - |  8992 | `			}` |
|        - |  8993 | `			/* Copy result */` |
|      ! 0 |  8994 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  8995 | `			PH7_MemObjRelease(&sResult);` |
|       84 |  8996 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       84 |  8997 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  8998 | `			ph7_value sResult;` |
|        - |  8999 | `			sxi32 rcInv;` |
|       84 |  9000 | `			SySetReset(&aArg);` |
|      200 |  9001 | `			while( pArg < pTos ){` |
|      118 |  9002 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      118 |  9003 | `				pArg++;` |
|        2 |  9004 | `			}` |
|       84 |  9005 | `			PH7_MemObjInit(pVm,&sResult);` |
|      125 |  9006 | `			rcInv = VmCallObjectInvoke(&(*pVm),pThis,` |
|       82 |  9007 | `				(int)SySetUsed(&aArg),` |
|       82 |  9008 | `				(ph7_value **)SySetBasePtr(&aArg),` |
|        - |  9009 | `				&sResult,` |
|       82 |  9010 | `				(VmCallArgMap *)pInstr->p3);` |
|       84 |  9011 | `			SySetReset(&aArg);` |
|       84 |  9012 | `			if( nCallArgs > 0 ){` |
|       76 |  9013 | `				VmPopOperand(&pTos,nCallArgs);` |
|       37 |  9014 | `			}` |
|       84 |  9015 | `			if( rcInv == SXERR_INVALID ){` |
|        - |  9016 | `				/* No __invoke: raise a catchable Error and route through try/catch.` |
|        - |  9017 | `				 * sResult was already released by VmCallObjectInvoke.` |
|        - |  9018 | `				 * Pin pThis: the release below would otherwise drop the last ref` |
|        - |  9019 | `				 * to a temporary callable like (new Plain())(...), freeing the` |
|        - |  9020 | `				 * instance before VmRaiseNotCallable reads its class name. */` |
|       13 |  9021 | `				pThis->iRef++;` |
|       13 |  9022 | `				PH7_MemObjRelease(pTos);` |
|       13 |  9023 | `				rc = VmRaiseNotCallable(&(*pVm),pThis);` |
|       13 |  9024 | `				PH7_ClassInstanceUnref(pThis);` |
|       13 |  9025 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9026 | `					goto Abort;` |
|        - |  9027 | `				}` |
|        - |  9028 | `				{` |
|       13 |  9029 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       12 |  9030 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION)` |
|       13 |  9031 | `					 && pFrm2->iExceptionJump > 0 ){` |
|       13 |  9032 | `						pc = pFrm2->iExceptionJump - 1;` |
|       15 |  9033 | `						break;` |
|        - |  9034 | `					}` |
|        - |  9035 | `				}` |
|      ! 0 |  9036 | `				goto Exception;` |
|        - |  9037 | `			}` |
|       72 |  9038 | `			if( rcInv == PH7_ABORT ){` |
|      ! 0 |  9039 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 |  9040 | `				goto Abort;` |
|        - |  9041 | `			}` |
|       72 |  9042 | `			if( rcInv == PH7_EXCEPTION ){` |
|        - |  9043 | `				/* __invoke raised. The catch body (if any) already ran in-place` |
|        - |  9044 | `				 * inside VmThrowException. If THIS frame's own try caught it,` |
|        - |  9045 | `				 * resume after the try/catch; otherwise propagate so the` |
|        - |  9046 | `				 * exception unwinds through intermediate frames with no handler. */` |
|        - |  9047 | `				sxi32 iResumePc;` |
|        7 |  9048 | `				PH7_MemObjRelease(&sResult);` |
|        7 |  9049 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        5 |  9050 | `					PH7_MemObjRelease(pTos);` |
|        5 |  9051 | `					pc = iResumePc;` |
|        5 |  9052 | `					break;` |
|        - |  9053 | `				}` |
|        3 |  9054 | `				goto Exception;` |
|        - |  9055 | `			}` |
|       66 |  9056 | `			PH7_MemObjStore(&sResult,pTos);` |
|       66 |  9057 | `			PH7_MemObjRelease(&sResult);` |
|       34 |  9058 | `		}else{` |
|        - |  9059 | `			/* Raise exception: Invalid function name */` |
|      ! 0 |  9060 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  9061 | `			/* Pop given arguments */` |
|      ! 0 |  9062 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  9063 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  9064 | `			}` |
|        - |  9065 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  9066 | `			PH7_MemObjRelease(pTos);` |
|        - |  9067 | `		}` |
|       66 |  9068 | `		break;` |
|        - |  9069 | `	}` |
|   720564 |  9070 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  9071 | `	/* Check for a compiled function first.` |
|        - |  9072 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  9073 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   720564 |  9074 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  9075 | `	/* If the compiler qualified this call with a namespace, and the namespaced` |
|        - |  9076 | `	 * function is not found, retry with the global name (strip the namespace` |
|        - |  9077 | `	 * prefix up to the last backslash) before falling back to host functions.` |
|        - |  9078 | `	 * This mirrors PHP's lookup order for unqualified function calls inside` |
|        - |  9079 | `	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */` |
|        - |  9080 | `	{` |
|   720564 |  9081 | `	VmCallArgMap *pCallMap = (VmCallArgMap *)pInstr->p3;` |
|   720564 |  9082 | `	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){` |
|        - |  9083 | `		const char *zFunc;` |
|        - |  9084 | `		const char *zEnd;` |
|        - |  9085 | `		const char *z;` |
|        - |  9086 | `		SyString sGlobal;` |
|       22 |  9087 | `		zFunc = sName.zString;` |
|       22 |  9088 | `		zEnd  = zFunc + sName.nByte;` |
|       22 |  9089 | `		z = zEnd;` |
|        - |  9090 | `		/* Find last namespace separator */` |
|      194 |  9091 | `		while( z > zFunc ){` |
|      194 |  9092 | `			if( z[-1] == '\\' ){` |
|       22 |  9093 | `				break;` |
|        - |  9094 | `			}` |
|      174 |  9095 | `			z--;` |
|        2 |  9096 | `		}` |
|       22 |  9097 | `		if( z > zFunc && z < zEnd ){` |
|        - |  9098 | `			/* Retry lookup using the unqualified/global function name */` |
|       22 |  9099 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       22 |  9100 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|       10 |  9101 | `		}` |
|       10 |  9102 | `	}` |
|        - |  9103 | `	} /* end VmCallArgMap namespace scope */` |
|   720564 |  9104 | `	if( pEntry ){` |
|        - |  9105 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  9106 | `		ph7_class_instance *pThis;` |
|        - |  9107 | `		ph7_value *pFrameStack;` |
|        - |  9108 | `		ph7_vm_func *pVmFunc;` |
|        - |  9109 | `		ph7_class *pSelf;` |
|        - |  9110 | `		VmFrame *pFrame;` |
|        - |  9111 | `		ph7_value *pObj;` |
|        - |  9112 | `		VmSlot sArg;` |
|        - |  9113 | `		sxu32 n;` |
|        - |  9114 | `		/* initialize fields */` |
|    19150 |  9115 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    19150 |  9116 | `		pThis = 0;` |
|    19150 |  9117 | `		pSelf = 0;` |
|    19150 |  9118 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  9119 | `			ph7_class_method *pMeth;` |
|        - |  9120 | `			/* Class method call */` |
|     3704 |  9121 | `			ph7_value *pTarget = &pTos[-1];` |
|     3704 |  9122 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  9123 | `				/* Extract the 'this' pointer */` |
|     3704 |  9124 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  9125 | `					/* Instance already loaded */` |
|     3614 |  9126 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     3614 |  9127 | `					pThis->iRef++;` |
|     3614 |  9128 | `					pSelf = pThis->pClass;` |
|     1806 |  9129 | `				}` |
|     3704 |  9130 | `				if( pSelf == 0 ){` |
|       92 |  9131 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  9132 | `						/* "Late Static Binding" class name */` |
|      128 |  9133 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       42 |  9134 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       42 |  9135 | `					}` |
|       92 |  9136 | `					if( pSelf == 0 ){` |
|       21 |  9137 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|       10 |  9138 | `					}` |
|       45 |  9139 | `				}` |
|     3704 |  9140 | `				if( pThis == 0  ){` |
|       92 |  9141 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       92 |  9142 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       92 |  9143 | `					if( pFrameLocal->pParent ){` |
|        - |  9144 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       66 |  9145 | `						pThis = pFrameLocal->pThis;` |
|       66 |  9146 | `						if( pThis ){` |
|       21 |  9147 | `							pThis->iRef++;` |
|       10 |  9148 | `						}` |
|       32 |  9149 | `					}` |
|       45 |  9150 | `				}` |
|     3704 |  9151 | `				VmPopOperand(&pTos,1);` |
|     3704 |  9152 | `				PH7_MemObjRelease(pTos);` |
|        - |  9153 | `				/* Synchronize pointers */` |
|     3704 |  9154 | `				pArg = &pTos[-nCallArgs];` |
|        - |  9155 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  9156 | `				 * user have already computed the random generated unique class method name` |
|        - |  9157 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  9158 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  9159 | `				 */` |
|     3704 |  9160 | `				while( pArg < pStack ){` |
|      ! 0 |  9161 | `					pArg++;` |
|      ! 0 |  9162 | `				}` |
|     3704 |  9163 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  9164 | `					/* Check if the call is allowed */` |
|     3704 |  9165 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     3704 |  9166 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       14 |  9167 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - |  9168 | `							/* Throw Error exception (PHP-compatible) */` |
|        - |  9169 | `							char zMsg[256];` |
|      ! 0 |  9170 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  9171 | `							SyBufferFormat(zMsg,sizeof(zMsg),"Call to %s method %.*s::%.*s() from global scope",` |
|      ! 0 |  9172 | `								zVis,(int)pSelf->sName.nByte,pSelf->sName.zString,` |
|      ! 0 |  9173 | `								(int)pVmFunc->sName.nByte,pVmFunc->sName.zString);` |
|        - |  9174 | `							/* Pop given arguments */` |
|      ! 0 |  9175 | `							if( nCallArgs > 0 ){` |
|      ! 0 |  9176 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  9177 | `							}` |
|      ! 0 |  9178 | `							VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  9179 | `							goto Abort;` |
|        - |  9180 | `						}` |
|        6 |  9181 | `					}` |
|     1851 |  9182 | `				}` |
|     1851 |  9183 | `			}` |
|     1851 |  9184 | `		}` |
|        - |  9185 | `		/* Check The recursion limit. Hitting it raises a clean, non-catchable` |
|        - |  9186 | `		 * fatal (was: silently set NULL and continue) and halts. The check is` |
|        - |  9187 | `		 * before VmEnterFrame/the recursive VmByteCodeExec below, so a` |
|        - |  9188 | `		 * correctly-set cap also keeps deep recursion off the native stack. */` |
|    19150 |  9189 | `		if( VmRecursionExceeded(pVm) ){` |
|        - |  9190 | `			/* Args and the function-name slot are released by the Abort label,` |
|        - |  9191 | `			 * which walks the whole operand stack — don't release them here. */` |
|        5 |  9192 | `			VmRecursionFatal(&(*pVm));` |
|        5 |  9193 | `			goto Abort;` |
|        - |  9194 | `		}` |
|    19146 |  9195 | `		if( pVmFunc->pNextName ){` |
|        - |  9196 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      140 |  9197 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       69 |  9198 | `		}` |
|    19146 |  9199 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  9200 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  9201 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  9202 | `			ph7_generator *pGenerator;` |
|        - |  9203 | `			ph7_class_instance *pGenObj;` |
|        - |  9204 | `			ph7_value *pCtxAttr;` |
|        - |  9205 | `			SyString sAttrName;` |
|        - |  9206 | `			ph7_value **apCallArgs;` |
|        - |  9207 | `			int nGenArgs, iArg;` |
|        - |  9208 | `			/* Collect arguments from the operand stack */` |
|       50 |  9209 | `			nGenArgs = (int)(pTos - pArg);` |
|       50 |  9210 | `			apCallArgs = 0;` |
|       50 |  9211 | `			if( nGenArgs > 0 ){` |
|       14 |  9212 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  9213 | `					nGenArgs * sizeof(ph7_value *));` |
|       10 |  9214 | `				if( apCallArgs == 0 ){` |
|        - |  9215 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  9216 | `					nGenArgs = 0;` |
|      ! 0 |  9217 | `				}else{` |
|       10 |  9218 | `					VmCallArgMap *pGenMap = (VmCallArgMap *)pInstr->p3;` |
|       10 |  9219 | `					int didReorder = 0;` |
|       10 |  9220 | `					if( pGenMap && pGenMap->bHasNamed ){` |
|        - |  9221 | `						/* Named-argument reordering for generator */` |
|        5 |  9222 | `						ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        5 |  9223 | `						sxu32 nF = SySetUsed(&pVmFunc->aArgs);` |
|        5 |  9224 | `						sxu32 nNV = nF;` |
|        5 |  9225 | `						sxi32 iVIdx = -1;` |
|        - |  9226 | `						sxi32 *aGSlot;` |
|        - |  9227 | `						sxu8 *aGUsed;` |
|        - |  9228 | `						sxu32 gi;` |
|       13 |  9229 | `						for( gi = 0; gi < nF; gi++ ){` |
|        9 |  9230 | `							if( aFA[gi].iFlags & VM_FUNC_ARG_VARIADIC ){ nNV = gi; iVIdx = (sxi32)gi; break; }` |
|        5 |  9231 | `						}` |
|        7 |  9232 | `						aGSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  9233 | `							(sxu32)nGenArgs * sizeof(sxi32) + nNV * sizeof(sxu8));` |
|        5 |  9234 | `						if( aGSlot ){` |
|        5 |  9235 | `							aGUsed = (sxu8 *)&aGSlot[nGenArgs];` |
|        7 |  9236 | `							rc = VmResolveNamedArgs(&(*pVm),pGenMap,aFA,nNV,iVIdx,` |
|        2 |  9237 | `								(sxu32)nGenArgs,aGSlot,aGUsed);` |
|        5 |  9238 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  9239 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 |  9240 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  9241 | `								goto Abort;` |
|        - |  9242 | `							}` |
|        - |  9243 | `							/* Build apCallArgs in formal-parameter order, then` |
|        - |  9244 | `							 * append overflow (variadic / positional beyond` |
|        - |  9245 | `							 * formals) so downstream sees every argument. */` |
|        - |  9246 | `							{` |
|        5 |  9247 | `								int nOut = 0;` |
|       13 |  9248 | `								for( gi = 0; gi < nNV; gi++ ){` |
|        - |  9249 | `									sxu32 gj;` |
|       13 |  9250 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       13 |  9251 | `										if( aGSlot[gj] == (sxi32)gi ){` |
|        9 |  9252 | `											apCallArgs[nOut++] = &pArg[gj];` |
|        9 |  9253 | `											break;` |
|        - |  9254 | `										}` |
|        3 |  9255 | `									}` |
|        5 |  9256 | `								}` |
|       13 |  9257 | `								for( gi = 0; gi < (sxu32)nGenArgs; gi++ ){` |
|        9 |  9258 | `									if( aGSlot[gi] == -1 \|\| aGSlot[gi] == -2 ){` |
|      ! 0 |  9259 | `										apCallArgs[nOut++] = &pArg[gi];` |
|      ! 0 |  9260 | `									}` |
|        5 |  9261 | `								}` |
|        5 |  9262 | `								nGenArgs = nOut;` |
|        - |  9263 | `							}` |
|        5 |  9264 | `							SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        5 |  9265 | `							didReorder = 1;` |
|        2 |  9266 | `						}` |
|        - |  9267 | `						/* If aGSlot allocation failed, fall through to` |
|        - |  9268 | `						 * positional fill below — preserves arg order rather` |
|        - |  9269 | `						 * than passing an uninitialized apCallArgs. */` |
|        2 |  9270 | `					}` |
|       10 |  9271 | `					if( !didReorder ){` |
|       12 |  9272 | `						for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|        8 |  9273 | `							apCallArgs[iArg] = &pArg[iArg];` |
|        5 |  9274 | `						}` |
|        2 |  9275 | `					}` |
|        - |  9276 | `				}` |
|        4 |  9277 | `			}` |
|        - |  9278 | `			/* Create execution context and generator wrapper */` |
|       50 |  9279 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       50 |  9280 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  9281 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  9282 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  9283 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  9284 | `				break;` |
|        - |  9285 | `			}` |
|       50 |  9286 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       50 |  9287 | `			if( pGenerator == 0 ){` |
|      ! 0 |  9288 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  9289 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  9290 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  9291 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  9292 | `				break;` |
|        - |  9293 | `			}` |
|        - |  9294 | `			/* Set up the frame with arguments, closure env, $this */` |
|       50 |  9295 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       50 |  9296 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       50 |  9297 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs);` |
|       50 |  9298 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       50 |  9299 | `			pExecCtx->pFrame->pParent = 0;` |
|       50 |  9300 | `			if( apCallArgs ){` |
|       10 |  9301 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        4 |  9302 | `			}` |
|       50 |  9303 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  9304 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  9305 | `				if( pThis ){` |
|      ! 0 |  9306 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  9307 | `				}` |
|      ! 0 |  9308 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9309 | `					goto Abort;` |
|        - |  9310 | `				}` |
|      ! 0 |  9311 | `				break;` |
|        - |  9312 | `			}` |
|        - |  9313 | `			/* Create Generator class instance */` |
|       50 |  9314 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       50 |  9315 | `			if( pGenObj == 0 ){` |
|      ! 0 |  9316 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  9317 | `				break;` |
|        - |  9318 | `			}` |
|        - |  9319 | `			/* Store generator in __ctx attribute */` |
|       50 |  9320 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       50 |  9321 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       50 |  9322 | `			if( pCtxAttr ){` |
|       50 |  9323 | `				pCtxAttr->x.pOther = pGenerator;` |
|       50 |  9324 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       24 |  9325 | `			}` |
|        - |  9326 | `			/* Pop args and function name, push Generator object */` |
|       50 |  9327 | `			PH7_MemObjRelease(pTos);` |
|       50 |  9328 | `			pTos = &pTos[-nCallArgs];` |
|       50 |  9329 | `			pTos->x.pOther = pGenObj;` |
|       50 |  9330 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       50 |  9331 | `			pGenObj->iRef++;` |
|       50 |  9332 | `			if( pThis ){` |
|      ! 0 |  9333 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  9334 | `			}` |
|       50 |  9335 | `			break;` |
|        - |  9336 | `		}` |
|        - |  9337 | `		/* Extract the formal argument set */` |
|    19098 |  9338 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  9339 | `		/* Create a new VM frame  */` |
|    19098 |  9340 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    19098 |  9341 | `		if( rc != SXRET_OK ){` |
|        - |  9342 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  9343 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  9344 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  9345 | `				&pVmFunc->sName);` |
|        - |  9346 | `			/* Pop given arguments */` |
|      ! 0 |  9347 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  9348 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  9349 | `			}` |
|        - |  9350 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  9351 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  9352 | `			break;` |
|        - |  9353 | `		}` |
|    19098 |  9354 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  9355 | `			/* Install the '$this' variable */` |
|        - |  9356 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     3632 |  9357 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     3632 |  9358 | `			if( pObj ){` |
|        - |  9359 | `				/* Reflect the change */` |
|     3632 |  9360 | `				pObj->x.pOther = pThis;` |
|     3632 |  9361 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|     1815 |  9362 | `			}` |
|     1815 |  9363 | `		}` |
|    19098 |  9364 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  9365 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  9366 | `			/* Install static variables */` |
|       13 |  9367 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|       25 |  9368 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|       13 |  9369 | `				pStatic = &aStatic[n];` |
|       13 |  9370 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  9371 | `					/* Initialize the static variables */` |
|        9 |  9372 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|        9 |  9373 | `					if( pObj ){` |
|        - |  9374 | `						/* Assume a NULL initialization value */` |
|        9 |  9375 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|        9 |  9376 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  9377 | `							/* Evaluate initialization expression (Any complex expression) */` |
|        9 |  9378 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|        4 |  9379 | `						}` |
|        9 |  9380 | `						pObj->nIdx = pStatic->nIdx;` |
|        5 |  9381 | `					}else{` |
|      ! 0 |  9382 | `						continue;` |
|        - |  9383 | `					}` |
|        4 |  9384 | `				}` |
|        - |  9385 | `				/* Install in the current frame */` |
|       19 |  9386 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|       12 |  9387 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|        7 |  9388 | `			}` |
|        6 |  9389 | `		}` |
|        - |  9390 | `		/* Push arguments in the local frame */` |
|        - |  9391 | `		{` |
|    19098 |  9392 | `		VmCallArgMap *pCallMap3 = (VmCallArgMap *)pInstr->p3;` |
|        - |  9393 | `		/* Caller file's strict_types mode — governs parameter coercion` |
|        - |  9394 | `		 * (but NOT return coercion, which uses the callee's file). */` |
|    19098 |  9395 | `		int bCallIsStrict = (pCallMap3 && pCallMap3->bStrict) ? 1 : 0;` |
|    19098 |  9396 | `		if( pCallMap3 && pCallMap3->bHasNamed ){` |
|        - |  9397 | `			/* ============================================================` |
|        - |  9398 | `			 * Named-argument matching path (PHP 8.0)` |
|        - |  9399 | `			 *` |
|        - |  9400 | `			 * Resolve each actual argument to its formal parameter by name` |
|        - |  9401 | `			 * or position, then install them in the frame.` |
|        - |  9402 | `			 * ============================================================ */` |
|       96 |  9403 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|       96 |  9404 | `			sxu32 nActual = (sxu32)(pTos - pArg);` |
|       96 |  9405 | `			sxi32 iVariadicIdx = -1;` |
|        - |  9406 | `			sxu32 nNonVariadic;` |
|        - |  9407 | `			sxi32 *aSlot;` |
|        - |  9408 | `			sxu8  *aUsed;` |
|        - |  9409 | `			sxu32 i;` |
|        - |  9410 | `			/* Find variadic parameter index */` |
|      292 |  9411 | `			for( i = 0; i < nFormal; i++ ){` |
|      206 |  9412 | `				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        9 |  9413 | `					iVariadicIdx = (sxi32)i;` |
|        9 |  9414 | `					break;` |
|        - |  9415 | `				}` |
|      100 |  9416 | `			}` |
|       96 |  9417 | `			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;` |
|        - |  9418 | `			/* Allocate mapping arrays */` |
|      143 |  9419 | `			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|       94 |  9420 | `				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));` |
|       96 |  9421 | `			if( aSlot == 0 ){` |
|      ! 0 |  9422 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");` |
|      ! 0 |  9423 | `				goto Abort;` |
|        - |  9424 | `			}` |
|       96 |  9425 | `			aUsed = (sxu8 *)&aSlot[nActual];` |
|        - |  9426 | `			/* Resolve named arguments to formal parameters */` |
|      143 |  9427 | `			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,` |
|       47 |  9428 | `				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);` |
|       96 |  9429 | `			if( rc == PH7_ABORT ){` |
|        7 |  9430 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        7 |  9431 | `				goto Abort;` |
|        - |  9432 | `			}` |
|        - |  9433 | `			/* Pass 2: install arguments into the frame by formal parameter order */` |
|      275 |  9434 | `			for( n = 0; n < nNonVariadic; n++ ){` |
|        - |  9435 | `				/* Find the stack arg mapped to formal n */` |
|      187 |  9436 | `				sxi32 iSrc = -1;` |
|      309 |  9437 | `				for( i = 0; i < nActual; i++ ){` |
|      291 |  9438 | `					if( aSlot[i] == (sxi32)n ){` |
|      169 |  9439 | `						iSrc = (sxi32)i;` |
|      169 |  9440 | `						break;` |
|        - |  9441 | `					}` |
|       62 |  9442 | `				}` |
|      187 |  9443 | `				if( iSrc >= 0 ){` |
|        - |  9444 | `					/* Argument was provided — install with type checking */` |
|      169 |  9445 | `					ph7_value *pVal = &pArg[iSrc];` |
|        - |  9446 | `					/* NULL-to-default redirect (existing behavior) */` |
|      168 |  9447 | `					if( (pVal->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|        5 |  9448 | `						&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|      ! 0 |  9449 | `						rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pVal);` |
|      ! 0 |  9450 | `						if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9451 | `					}` |
|        - |  9452 | `					/* Type checking: union types */` |
|      169 |  9453 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       19 |  9454 | `						sxi32 rcU = VmCoerceToUnion(pVm, pVal, &aFormalArg[n].aUnionAlts,` |
|       12 |  9455 | `							(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        6 |  9456 | `							bCallIsStrict);` |
|       13 |  9457 | `						if( rcU != SXRET_OK ){` |
|        - |  9458 | `							const char *zGiven;` |
|      ! 0 |  9459 | `							const char *zExpected = "union";` |
|        - |  9460 | `							char zBuf[128];` |
|        - |  9461 | `							char zTypeBuf[128];` |
|      ! 0 |  9462 | `							if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  9463 | `								zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|      ! 0 |  9464 | `							}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  9465 | `								zGiven = "null";` |
|      ! 0 |  9466 | `							}else{` |
|      ! 0 |  9467 | `								zGiven = ph7_type_name(pVal);` |
|        - |  9468 | `							}` |
|      ! 0 |  9469 | `							if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|      ! 0 |  9470 | `								zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  9471 | `							}` |
|      ! 0 |  9472 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9473 | `								&aFormalArg[n].sName, zExpected, zGiven);` |
|      ! 0 |  9474 | `							if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9475 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  9476 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  9477 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9478 | `							pFrameStack = 0;` |
|      ! 0 |  9479 | `							rc = PH7_EXCEPTION;` |
|      ! 0 |  9480 | `							goto SkipFuncBody;` |
|        - |  9481 | `						}` |
|      171 |  9482 | `					}else if( aFormalArg[n].nType > 0` |
|       91 |  9483 | `						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|        - |  9484 | `						/* Scalar/class type checking */` |
|       17 |  9485 | `						if( aFormalArg[n].nType == SXU32_HIGH ){` |
|      ! 0 |  9486 | `							SyString *pName = &aFormalArg[n].sClass;` |
|        - |  9487 | `							ph7_class *pClass;` |
|      ! 0 |  9488 | `							int rcPseudo = VmCheckPseudoType(&(*pVm),pVal,pName);` |
|      ! 0 |  9489 | `							if( rcPseudo == 0 ){` |
|        - |  9490 | `								/* Recognised pseudo-type (true/false/iterable); value mismatches */` |
|        - |  9491 | `								char zTypeBuf[128],zGivenBuf[128];` |
|      ! 0 |  9492 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9493 | `									&aFormalArg[n].sName,` |
|      ! 0 |  9494 | `									VmSyStringToCStr(pName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  9495 | `									VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|      ! 0 |  9496 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9497 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  9498 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  9499 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9500 | `								pFrameStack = 0;` |
|      ! 0 |  9501 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  9502 | `								goto SkipFuncBody;` |
|        - |  9503 | `							}` |
|        - |  9504 | `							/* rcPseudo==1 -> matched pseudo-type (accept); -1 -> real class */` |
|      ! 0 |  9505 | `							pClass = (rcPseudo == 1) ? 0 : PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  9506 | `							if( pClass ){` |
|      ! 0 |  9507 | `								if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9508 | `									if( (pVal->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  9509 | `										VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9510 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9511 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9512 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  9513 | `									}` |
|      ! 0 |  9514 | `								}else{` |
|      ! 0 |  9515 | `									ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  9516 | `									if( !PH7_VmInstanceOf(pInst->pClass,pClass) ){` |
|      ! 0 |  9517 | `										VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  9518 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9519 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9520 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  9521 | `									}` |
|        - |  9522 | `								}` |
|      ! 0 |  9523 | `							}` |
|       17 |  9524 | `						}else if( (pVal->iFlags & aFormalArg[n].nType) == 0 ){` |
|        7 |  9525 | `							if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|      ! 0 |  9526 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9527 | `									&aFormalArg[n].sName,"object",ph7_type_name(pVal));` |
|      ! 0 |  9528 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9529 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  9530 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  9531 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9532 | `								pFrameStack = 0;` |
|      ! 0 |  9533 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  9534 | `								goto SkipFuncBody;` |
|        7 |  9535 | `							}else if( VmEnforceScalarType(pVal, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  9536 | `								char zTypeBuf[128];` |
|      ! 0 |  9537 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9538 | `									&aFormalArg[n].sName,` |
|      ! 0 |  9539 | `									VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  9540 | `									ph7_type_name(pVal));` |
|      ! 0 |  9541 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9542 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  9543 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  9544 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9545 | `								pFrameStack = 0;` |
|      ! 0 |  9546 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  9547 | `								goto SkipFuncBody;` |
|        - |  9548 | `							}` |
|        3 |  9549 | `						}` |
|        8 |  9550 | `					}` |
|        - |  9551 | `					/* Install: by reference or by value */` |
|      169 |  9552 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        5 |  9553 | `						if( pVal->nIdx == SXU32_HIGH ){` |
|      ! 0 |  9554 | `							if( (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|      ! 0 |  9555 | `								VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9556 | `									"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  9557 | `									"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  9558 | `							}` |
|      ! 0 |  9559 | `							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  9560 | `						}else{` |
|        7 |  9561 | `							SyHashEntry *pRefEntry = SyHashGet(&pFrame->hVar,` |
|        4 |  9562 | `								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|        5 |  9563 | `							if( pRefEntry == 0 ){` |
|        7 |  9564 | `								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|        4 |  9565 | `									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));` |
|        5 |  9566 | `								sArg.nIdx = pVal->nIdx;` |
|        5 |  9567 | `								sArg.pUserData = 0;` |
|        5 |  9568 | `								SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        2 |  9569 | `							}` |
|        5 |  9570 | `							pObj = 0;` |
|        - |  9571 | `						}` |
|        3 |  9572 | `					}else{` |
|      165 |  9573 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  9574 | `					}` |
|      169 |  9575 | `					if( pObj ){` |
|      165 |  9576 | `						PH7_MemObjStore(pVal,pObj);` |
|      165 |  9577 | `						sArg.nIdx = pObj->nIdx;` |
|      165 |  9578 | `						sArg.pUserData = 0;` |
|      165 |  9579 | `						SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       82 |  9580 | `					}` |
|       85 |  9581 | `				}else{` |
|        - |  9582 | `					/* Argument was NOT provided — use default or leave unset */` |
|       19 |  9583 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  9584 | `						/* Should not reach here; variadic handled separately below */` |
|       19 |  9585 | `					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|       19 |  9586 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       19 |  9587 | `						if( pObj ){` |
|       19 |  9588 | `							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|       19 |  9589 | `							if( rc == PH7_ABORT ) goto Abort;` |
|       19 |  9590 | `							sArg.nIdx = pObj->nIdx;` |
|       19 |  9591 | `							sArg.pUserData = 0;` |
|       19 |  9592 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  9593 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|        1 |  9594 | `								&& (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  9595 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  9596 | `								if( xCast ) xCast(pObj);` |
|      ! 0 |  9597 | `							}` |
|        9 |  9598 | `						}` |
|        9 |  9599 | `					}` |
|        - |  9600 | `					/* else: required param missing — leave unset (matches existing behavior) */` |
|        - |  9601 | `				}` |
|       94 |  9602 | `			}` |
|        - |  9603 | `			/* Handle variadic parameter */` |
|       89 |  9604 | `			if( iVariadicIdx >= 0 ){` |
|        9 |  9605 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);` |
|        9 |  9606 | `				if( pObj ){` |
|        9 |  9607 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  9608 | `					{` |
|        9 |  9609 | `						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;` |
|       31 |  9610 | `						for( i = 0; i < nActual; i++ ){` |
|       23 |  9611 | `							if( aSlot[i] == -1 ){` |
|       16 |  9612 | `								if( i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0 ){` |
|        - |  9613 | `									/* Named variadic entry: insert with string key */` |
|        - |  9614 | `									ph7_value sKey;` |
|       11 |  9615 | `									PH7_MemObjInit(pVm, &sKey);` |
|       11 |  9616 | `									PH7_MemObjStringAppend(&sKey,` |
|       10 |  9617 | `										pCallMap3->aNames[i].zString,` |
|       10 |  9618 | `										(sxu32)pCallMap3->aNames[i].nByte);` |
|       11 |  9619 | `									PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);` |
|       11 |  9620 | `									PH7_MemObjRelease(&sKey);` |
|        6 |  9621 | `								}else{` |
|        - |  9622 | `									/* Positional variadic entry */` |
|      ! 0 |  9623 | `									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);` |
|        - |  9624 | `								}` |
|        5 |  9625 | `							}` |
|       12 |  9626 | `						}` |
|        - |  9627 | `					}` |
|        9 |  9628 | `					sArg.nIdx = pObj->nIdx;` |
|        9 |  9629 | `					sArg.pUserData = 0;` |
|        9 |  9630 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        4 |  9631 | `				}` |
|        5 |  9632 | `			}else{` |
|        - |  9633 | `				/* No variadic — preserve unresolved positional overflow` |
|        - |  9634 | `				 * (aSlot[i] == -2) as anonymous frame args so` |
|        - |  9635 | `				 * func_get_args() / func_num_args() still see them, matching` |
|        - |  9636 | `				 * the positional-only path's behavior. */` |
|       81 |  9637 | `				sxu32 nAnon = nNonVariadic;` |
|      237 |  9638 | `				for( i = 0; i < nActual; i++ ){` |
|      157 |  9639 | `					if( aSlot[i] == -2 ){` |
|        - |  9640 | `						char zAnonBuf[32];` |
|        - |  9641 | `						SyString sAnonName;` |
|      ! 0 |  9642 | `						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),` |
|      ! 0 |  9643 | `							"[%u]apArg",nAnon);` |
|      ! 0 |  9644 | `						sAnonName.zString = zAnonBuf;` |
|      ! 0 |  9645 | `						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);` |
|      ! 0 |  9646 | `						if( pObj ){` |
|      ! 0 |  9647 | `							PH7_MemObjStore(&pArg[i],pObj);` |
|      ! 0 |  9648 | `							sArg.nIdx = pObj->nIdx;` |
|      ! 0 |  9649 | `							sArg.pUserData = 0;` |
|      ! 0 |  9650 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      ! 0 |  9651 | `						}` |
|      ! 0 |  9652 | `						nAnon++;` |
|      ! 0 |  9653 | `					}` |
|       79 |  9654 | `				}` |
|        - |  9655 | `			}` |
|        - |  9656 | `			/* Release all stack arguments */` |
|      267 |  9657 | `			for( i = 0; i < nActual; i++ ){` |
|      179 |  9658 | `				PH7_MemObjRelease(&pArg[i]);` |
|       90 |  9659 | `			}` |
|       89 |  9660 | `			SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        - |  9661 | `			/* Set n to nFormal so the defaults loop below is skipped */` |
|       89 |  9662 | `			n = nFormal;` |
|       45 |  9663 | `		}else{` |
|        - |  9664 | `		/* ============================================================` |
|        - |  9665 | `		 * Positional-only matching path (original)` |
|        - |  9666 | `		 * ============================================================ */` |
|    19004 |  9667 | `		n = 0;` |
|    49798 |  9668 | `		while( pArg < pTos ){` |
|    30876 |  9669 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - |  9670 | `				/* Variadic parameter: collect all remaining args into an array */` |
|       42 |  9671 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       42 |  9672 | `				if( pObj ){` |
|        - |  9673 | `					/* Initialize as empty array */` |
|       42 |  9674 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  9675 | `					{` |
|       42 |  9676 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      158 |  9677 | `						while( pArg < pTos ){` |
|        - |  9678 | `							/* Variadic union type: per-element coercion via the shared helper.` |
|        - |  9679 | `							 *` |
|        - |  9680 | `							 * TODO: PHP reports the runtime element index here` |
|        - |  9681 | `							 * ("Argument #3 must be...") but we report the formal-arg` |
|        - |  9682 | `							 * index (always n+1, the position of the variadic). The` |
|        - |  9683 | `							 * non-union variadic path below has the same limitation;` |
|        - |  9684 | `							 * fixing both wants a separate counter for elements` |
|        - |  9685 | `							 * already packed into the variadic array. */` |
|      120 |  9686 | `							if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       23 |  9687 | `								sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       14 |  9688 | `									(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        7 |  9689 | `									bCallIsStrict);` |
|       16 |  9690 | `								if( rcU != SXRET_OK ){` |
|        - |  9691 | `									const char *zGiven;` |
|        3 |  9692 | `									const char *zExpected = "union";` |
|        - |  9693 | `									char zBuf[128];` |
|        - |  9694 | `									char zTypeBuf[128];` |
|        3 |  9695 | `									if( pArg->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  9696 | `										zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|        3 |  9697 | `									}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  9698 | `										zGiven = "null";` |
|      ! 0 |  9699 | `									}else{` |
|        3 |  9700 | `										zGiven = ph7_type_name(pArg);` |
|        - |  9701 | `									}` |
|        3 |  9702 | `									if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|        3 |  9703 | `										zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        1 |  9704 | `									}` |
|        4 |  9705 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        2 |  9706 | `										&aFormalArg[n].sName, zExpected, zGiven);` |
|        3 |  9707 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9708 | `										goto Abort;` |
|        - |  9709 | `									}` |
|        3 |  9710 | `									PH7_MemObjRelease(pTos);` |
|        3 |  9711 | `									pTos = &pTos[-nCallArgs];` |
|        3 |  9712 | `									pFrameStack = 0;` |
|        3 |  9713 | `									rc = PH7_EXCEPTION;` |
|        3 |  9714 | `									goto SkipFuncBody;` |
|        - |  9715 | `								}` |
|       14 |  9716 | `								PH7_HashmapInsert(pMap, 0, pArg);` |
|       14 |  9717 | `								pArg++;` |
|       14 |  9718 | `								continue;` |
|        - |  9719 | `							}` |
|        - |  9720 | `							/* Apply type coercion to each element if the variadic has a type hint.` |
|        - |  9721 | `							 * Nullable types (?type) allow null through without coercion. */` |
|      120 |  9722 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       42 |  9723 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL))` |
|       41 |  9724 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 |  9725 | `								if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  9726 | `									/* object type hint on variadic: reject non-objects with TypeError */` |
|      ! 0 |  9727 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9728 | `										&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|      ! 0 |  9729 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9730 | `										goto Abort;` |
|        - |  9731 | `									}` |
|        - |  9732 | `									/* Skip function body, route through normal cleanup */` |
|      ! 0 |  9733 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  9734 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9735 | `									pFrameStack = 0;` |
|      ! 0 |  9736 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  9737 | `									goto SkipFuncBody;` |
|       13 |  9738 | `								}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  9739 | `									char zTypeBuf[128];` |
|      ! 0 |  9740 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9741 | `										&aFormalArg[n].sName,` |
|      ! 0 |  9742 | `										VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  9743 | `										ph7_type_name(pArg));` |
|      ! 0 |  9744 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9745 | `										goto Abort;` |
|        - |  9746 | `									}` |
|      ! 0 |  9747 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  9748 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9749 | `									pFrameStack = 0;` |
|      ! 0 |  9750 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  9751 | `									goto SkipFuncBody;` |
|        - |  9752 | `								}` |
|        6 |  9753 | `							}` |
|      106 |  9754 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|      106 |  9755 | `							pArg++;` |
|        2 |  9756 | `						}` |
|        - |  9757 | `					}` |
|       40 |  9758 | `					sArg.nIdx = pObj->nIdx;` |
|       40 |  9759 | `					sArg.pUserData = 0;` |
|       40 |  9760 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       19 |  9761 | `				}` |
|       40 |  9762 | `				break; /* All remaining args consumed */` |
|        - |  9763 | `			}` |
|    30836 |  9764 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    30618 |  9765 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|       41 |  9766 | `					&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|        - |  9767 | `					/* NULL values are redirected to default arguments (but not for nullable types) */` |
|      ! 0 |  9768 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  9769 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  9770 | `						goto Abort;` |
|        - |  9771 | `					}` |
|      ! 0 |  9772 | `				}` |
|        - |  9773 | `				/* Union type: dispatch to the shared coercion helper. */` |
|    30620 |  9774 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       95 |  9775 | `					sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       62 |  9776 | `						(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|       31 |  9777 | `						bCallIsStrict);` |
|       64 |  9778 | `					if( rcU != SXRET_OK ){` |
|        - |  9779 | `						const char *zGiven;` |
|       19 |  9780 | `						const char *zExpected = "union";` |
|        - |  9781 | `						char zBuf[128];` |
|        - |  9782 | `						char zTypeBuf[128];` |
|       19 |  9783 | `						if( pArg->iFlags & MEMOBJ_OBJ ){` |
|        7 |  9784 | `							zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|       16 |  9785 | `						}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|        9 |  9786 | `							zGiven = "null";` |
|        5 |  9787 | `						}else{` |
|        5 |  9788 | `							zGiven = ph7_type_name(pArg);` |
|        - |  9789 | `						}` |
|       19 |  9790 | `						if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|       19 |  9791 | `							zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        9 |  9792 | `						}` |
|       28 |  9793 | `						rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       18 |  9794 | `							&aFormalArg[n].sName, zExpected, zGiven);` |
|       19 |  9795 | `						if( rc == PH7_ABORT ){` |
|      ! 0 |  9796 | `							goto Abort;` |
|        - |  9797 | `						}` |
|       19 |  9798 | `						PH7_MemObjRelease(pTos);` |
|       19 |  9799 | `						pTos = &pTos[-nCallArgs];` |
|       19 |  9800 | `						pFrameStack = 0;` |
|       19 |  9801 | `						rc = PH7_EXCEPTION;` |
|       19 |  9802 | `						goto SkipFuncBody;` |
|        - |  9803 | `					}` |
|       23 |  9804 | `				}else` |
|        - |  9805 | `				/* Make sure the given arguments are of the correct type.` |
|        - |  9806 | `				 * Nullable types (?type) allow null through without coercion. */` |
|    30586 |  9807 | `				if( aFormalArg[n].nType > 0` |
|    16006 |  9808 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|     1424 |  9809 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  9810 | `						/* Argument must be a class instance [i.e: object] */` |
|       36 |  9811 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  9812 | `						ph7_class *pClass;` |
|       36 |  9813 | `						int rcPseudo = VmCheckPseudoType(&(*pVm),pArg,pName);` |
|       36 |  9814 | `						if( rcPseudo == 0 ){` |
|        - |  9815 | `							/* Recognised pseudo-type (true/false/iterable); value mismatches */` |
|        - |  9816 | `							char zTypeBuf[128],zGivenBuf[128];` |
|        7 |  9817 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        4 |  9818 | `								&aFormalArg[n].sName,` |
|        2 |  9819 | `								VmSyStringToCStr(pName,zTypeBuf,sizeof(zTypeBuf)),` |
|        2 |  9820 | `								VmValueGivenName(pArg,zGivenBuf,sizeof(zGivenBuf)));` |
|        5 |  9821 | `							if( rc == PH7_ABORT ) goto Abort;` |
|        5 |  9822 | `							PH7_MemObjRelease(pTos);` |
|        5 |  9823 | `							pTos = &pTos[-nCallArgs];` |
|        5 |  9824 | `							pFrameStack = 0;` |
|        5 |  9825 | `							rc = PH7_EXCEPTION;` |
|        5 |  9826 | `							goto SkipFuncBody;` |
|        - |  9827 | `						}` |
|        - |  9828 | `						/* Try to extract the desired class (rcPseudo==1 accepts; -1 real class) */` |
|       32 |  9829 | `						pClass = (rcPseudo == 1) ? 0 : PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       32 |  9830 | `						if( pClass ){` |
|       22 |  9831 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9832 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  9833 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9834 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9835 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9836 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  9837 | `								}` |
|      ! 0 |  9838 | `							}else{` |
|        - |  9839 | `								/* reuse pThis declared in outer scope */` |
|       22 |  9840 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  9841 | `								/* Make sure the object is an instance of the given class */` |
|       22 |  9842 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  9843 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  9844 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9845 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9846 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  9847 | `								}` |
|        - |  9848 | `							}` |
|       12 |  9849 | `						}` |
|     1405 |  9850 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|       28 |  9851 | `						if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  9852 | `							/* object type hint: reject non-objects with TypeError */` |
|       16 |  9853 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       10 |  9854 | `								&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|       11 |  9855 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  9856 | `								goto Abort;` |
|        - |  9857 | `							}` |
|        - |  9858 | `							/* Skip function body, route through normal cleanup */` |
|       11 |  9859 | `							PH7_MemObjRelease(pTos);` |
|       11 |  9860 | `							pTos = &pTos[-nCallArgs];` |
|       11 |  9861 | `							pFrameStack = 0;` |
|       11 |  9862 | `							rc = PH7_EXCEPTION;` |
|       11 |  9863 | `							goto SkipFuncBody;` |
|       18 |  9864 | `						}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  9865 | `							char zTypeBuf[128];` |
|       14 |  9866 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        8 |  9867 | `								&aFormalArg[n].sName,` |
|        8 |  9868 | `								VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|        4 |  9869 | `								ph7_type_name(pArg));` |
|       10 |  9870 | `							if( rc == PH7_ABORT ){` |
|        5 |  9871 | `								goto Abort;` |
|        - |  9872 | `							}` |
|        5 |  9873 | `							PH7_MemObjRelease(pTos);` |
|        5 |  9874 | `							pTos = &pTos[-nCallArgs];` |
|        5 |  9875 | `							pFrameStack = 0;` |
|        5 |  9876 | `							rc = PH7_EXCEPTION;` |
|        5 |  9877 | `							goto SkipFuncBody;` |
|        - |  9878 | `						}` |
|        4 |  9879 | `					}` |
|      700 |  9880 | `				}` |
|    30580 |  9881 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  9882 | `					/* Pass by reference */` |
|       58 |  9883 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  9884 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  9885 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  9886 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9887 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  9888 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  9889 | `						}` |
|        - |  9890 | `						/* Switch to pass by value */` |
|      ! 0 |  9891 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  9892 | `					}else{` |
|        - |  9893 | `						SyHashEntry *pRefEntry;` |
|        - |  9894 | `						/* Install the referenced variable in the private function frame */` |
|       58 |  9895 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       58 |  9896 | `						if( pRefEntry == 0 ){` |
|       86 |  9897 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       56 |  9898 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       58 |  9899 | `							sArg.nIdx = pArg->nIdx;` |
|       58 |  9900 | `							sArg.pUserData = 0;` |
|       58 |  9901 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       28 |  9902 | `						}` |
|       58 |  9903 | `						pObj = 0;` |
|        - |  9904 | `					}` |
|       30 |  9905 | `				}else{` |
|        - |  9906 | `					/* Pass by value,make a copy of the given argument */` |
|    30524 |  9907 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  9908 | `				}` |
|    15291 |  9909 | `			}else{` |
|        - |  9910 | `				char zName[32];` |
|        - |  9911 | `				SyString sArgName;` |
|        - |  9912 | `				/* Set a dummy name */` |
|      218 |  9913 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      218 |  9914 | `				sArgName.zString = zName;` |
|        - |  9915 | `				/* Annonymous argument */` |
|      218 |  9916 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  9917 | `			}` |
|    30796 |  9918 | `			if( pObj ){` |
|    30740 |  9919 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  9920 | `				/* Insert argument index  */` |
|    30740 |  9921 | `				sArg.nIdx = pObj->nIdx;` |
|    30740 |  9922 | `				sArg.pUserData = 0;` |
|    30740 |  9923 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    15369 |  9924 | `			}` |
|    30796 |  9925 | `			PH7_MemObjRelease(pArg);` |
|    30796 |  9926 | `			pArg++;` |
|    30796 |  9927 | `			++n;` |
|        2 |  9928 | `		}` |
|        - |  9929 | `		} /* end named vs positional branch */` |
|        - |  9930 | `		/* Set up closure environment */` |
|    19050 |  9931 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  9932 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  9933 | `			ph7_value *pValue;` |
|        - |  9934 | `			sxu32 iEnv;` |
|      184 |  9935 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|      434 |  9936 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|      252 |  9937 | `				pEnv = &aEnv[iEnv];` |
|      252 |  9938 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  9939 | `					/* Do not install null value */` |
|      178 |  9940 | `					continue;` |
|        - |  9941 | `				}` |
|       76 |  9942 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       76 |  9943 | `				if( pValue == 0 ){` |
|      ! 0 |  9944 | `					continue;` |
|        - |  9945 | `				}` |
|        - |  9946 | `				/* Invalidate any prior representation */` |
|       76 |  9947 | `				PH7_MemObjRelease(pValue);` |
|        - |  9948 | `				/* Duplicate bound variable value */` |
|       76 |  9949 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|       39 |  9950 | `			}` |
|       91 |  9951 | `		}` |
|        - |  9952 | `		/* Process default values for remaining formal parameters */` |
|    21988 |  9953 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     2988 |  9954 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  9955 | `				/* Variadic parameter with no extra args — create empty array */` |
|       50 |  9956 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       50 |  9957 | `				if( pObj ){` |
|       50 |  9958 | `					PH7_MemObjToHashmap(pObj);` |
|       50 |  9959 | `					sArg.nIdx = pObj->nIdx;` |
|       50 |  9960 | `					sArg.pUserData = 0;` |
|       50 |  9961 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       24 |  9962 | `				}` |
|       50 |  9963 | `				n++;` |
|       50 |  9964 | `				break; /* Variadic is always last */` |
|        - |  9965 | `			}` |
|     2940 |  9966 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     2934 |  9967 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     2934 |  9968 | `				if( pObj ){` |
|        - |  9969 | `					/* Evaluate the default value and extract it's result */` |
|     2934 |  9970 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     2934 |  9971 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  9972 | `						goto Abort;` |
|        - |  9973 | `					}` |
|        - |  9974 | `					/* Insert argument index */` |
|     2934 |  9975 | `					sArg.nIdx = pObj->nIdx;` |
|     2934 |  9976 | `					sArg.pUserData = 0;` |
|     2934 |  9977 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  9978 | `					/* Make sure the default argument is of the correct type */` |
|     2932 |  9979 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|     1892 |  9980 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|        3 |  9981 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  9982 | `						/* Cast to the desired type */` |
|        3 |  9983 | `						xCast(pObj);` |
|        1 |  9984 | `					}` |
|     1466 |  9985 | `				}` |
|     1466 |  9986 | `			}` |
|     2940 |  9987 | `			++n;` |
|        2 |  9988 | `		}` |
|        - |  9989 | `		} /* end VmCallArgMap scope */` |
|        - |  9990 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  9991 | `		 * does not return anything.` |
|        - |  9992 | `		 */` |
|    19050 |  9993 | `		PH7_MemObjRelease(pTos);` |
|    19050 |  9994 | `		pTos = &pTos[-nCallArgs];` |
|        - |  9995 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    19050 |  9996 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    19050 |  9997 | `		if( pFrameStack == 0 ){` |
|        - |  9998 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  9999 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 | 10000 | `				&pVmFunc->sName);` |
|      ! 0 | 10001 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 10002 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 10003 | `			}` |
|      ! 0 | 10004 | `			break;` |
|        - | 10005 | `		}` |
|     9524 | 10006 | `SkipFuncBody:` |
|    19088 | 10007 | `		if( pSelf ){` |
|        - | 10008 | `			/* Push class name */` |
|     3702 | 10009 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|     1850 | 10010 | `		}` |
|        - | 10011 | `		/* Increment nesting level */` |
|    19088 | 10012 | `		pVm->nRecursionDepth++;` |
|    19088 | 10013 | `		if( rc != PH7_EXCEPTION ){` |
|        - | 10014 | `			/* Execute function body */` |
|    28574 | 10015 | `			rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0,` |
|    19048 | 10016 | `				pVmFunc->nReturnType > 0 ? pVmFunc : 0);` |
|     9524 | 10017 | `		}` |
|        - | 10018 | `		/* Decrement nesting level */` |
|    19088 | 10019 | `		pVm->nRecursionDepth--;` |
|    19088 | 10020 | `		if( pSelf ){` |
|        - | 10021 | `			/* Pop class name */` |
|     3702 | 10022 | `			(void)SySetPop(&pVm->aSelf);` |
|     1850 | 10023 | `		}` |
|        - | 10024 | `		/* Cleanup the mess left behind */` |
|    19088 | 10025 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - | 10026 | `			/* Return by reference,reflect that */` |
|        9 | 10027 | `			if( n != SXU32_HIGH ){` |
|        9 | 10028 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - | 10029 | `				sxu32 i;` |
|        - | 10030 | `				/* Make sure the referenced object is not a local variable */` |
|       13 | 10031 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 | 10032 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 | 10033 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 | 10034 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 | 10035 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - | 10036 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 | 10037 | `								&pVmFunc->sName);` |
|      ! 0 | 10038 | `						}` |
|      ! 0 | 10039 | `						n = SXU32_HIGH;` |
|      ! 0 | 10040 | `						break;` |
|        - | 10041 | `					}` |
|        3 | 10042 | `				}` |
|        5 | 10043 | `			}else{` |
|      ! 0 | 10044 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 | 10045 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - | 10046 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 | 10047 | `						&pVmFunc->sName);` |
|      ! 0 | 10048 | `				}` |
|        - | 10049 | `			}` |
|        9 | 10050 | `			pTos->nIdx = n;` |
|        4 | 10051 | `		}` |
|        - | 10052 | `		/* Cleanup the mess left behind */` |
|    19088 | 10053 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - | 10054 | `			/* An exception was throw in this frame */` |
|      116 | 10055 | `			pFrame = pFrame->pParent;` |
|      116 | 10056 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - | 10057 | `				/* Pop the resutlt */` |
|       74 | 10058 | `				VmPopOperand(&pTos,1);` |
|        - | 10059 | `				/* Jump to this destination */` |
|       74 | 10060 | `				pc = pFrame->iExceptionJump - 1;` |
|       74 | 10061 | `				rc = PH7_OK;` |
|       38 | 10062 | `			}else{` |
|       43 | 10063 | `				if( pFrame->pParent ){` |
|       43 | 10064 | `					rc = PH7_EXCEPTION;` |
|       22 | 10065 | `				}else{` |
|        - | 10066 | `					/* Continue normal execution */` |
|      ! 0 | 10067 | `					rc = PH7_OK;` |
|        - | 10068 | `				}` |
|        - | 10069 | `			}` |
|       57 | 10070 | `		}` |
|        - | 10071 | `		/* Free the operand stack (NULL when function body was skipped) */` |
|    19088 | 10072 | `		if( pFrameStack ){` |
|    19050 | 10073 | `			SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|     9524 | 10074 | `		}` |
|        - | 10075 | `		/* Leave the frame */` |
|    19088 | 10076 | `		VmLeaveFrame(&(*pVm));` |
|    19088 | 10077 | `		if( rc == PH7_ABORT ){` |
|        - | 10078 | `			/* Abort processing immeditaley */` |
|      117 | 10079 | `			goto Abort;` |
|    18972 | 10080 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - | 10081 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - | 10082 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - | 10083 | `			 * overwriting the state saved by the inner level.` |
|        - | 10084 | `			 * pTos points to the result slot (not yet written).` |
|        - | 10085 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       40 | 10086 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       40 | 10087 | `			goto Suspend;` |
|    18934 | 10088 | `		}else if( rc == PH7_EXCEPTION ){` |
|       43 | 10089 | `			goto Exception;` |
|        - | 10090 | `		}` |
|     9447 | 10091 | `	}else{` |
|        - | 10092 | `		ph7_user_func *pFunc;` |
|        - | 10093 | `		ph7_context sCtx;` |
|        - | 10094 | `		ph7_value sRet;` |
|        - | 10095 | `		/* Look for an installed foreign function.` |
|        - | 10096 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - | 10097 | `		 * If the compiler namespace-qualified the name, extract the short` |
|        - | 10098 | `		 * name (last component after \) and try that. This implements PHP's` |
|        - | 10099 | `		 * global fallback for unqualified function calls in namespaces. */` |
|   701416 | 10100 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|        - | 10101 | `		{` |
|   701416 | 10102 | `		VmCallArgMap *pCallMap2 = (VmCallArgMap *)pInstr->p3;` |
|   701416 | 10103 | `		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){` |
|        - | 10104 | `			/* Compiler-qualified: try short name as global fallback */` |
|       22 | 10105 | `			const char *zShort = sName.zString;` |
|        - | 10106 | `			sxu32 i;` |
|      334 | 10107 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      314 | 10108 | `				if( sName.zString[i] == '\\' ){` |
|       28 | 10109 | `					zShort = &sName.zString[i + 1];` |
|       13 | 10110 | `				}` |
|      158 | 10111 | `			}` |
|       22 | 10112 | `			if( zShort != sName.zString ){` |
|       22 | 10113 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       22 | 10114 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|       10 | 10115 | `			}` |
|       10 | 10116 | `		}` |
|        - | 10117 | `		} /* end VmCallArgMap namespace scope */` |
|   701416 | 10118 | `		if( pEntry == 0 ){` |
|        - | 10119 | `			/* Call to undefined function */` |
|        5 | 10120 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - | 10121 | `			/* Pop given arguments */` |
|        5 | 10122 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 | 10123 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 | 10124 | `			}` |
|        - | 10125 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 | 10126 | `			PH7_MemObjRelease(pTos);` |
|       60 | 10127 | `			break;` |
|        - | 10128 | `		}` |
|   701412 | 10129 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - | 10130 | `		/* Start collecting function arguments */` |
|   701412 | 10131 | `		SySetReset(&aArg);` |
|  1891138 | 10132 | `		while( pArg < pTos ){` |
|  1189728 | 10133 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  1189728 | 10134 | `			pArg++;` |
|        2 | 10135 | `		}` |
|        - | 10136 | `		/* Assume a null return value */` |
|   701412 | 10137 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - | 10138 | `		/* Init the call context */` |
|   701412 | 10139 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - | 10140 | `		/* Call the foreign function */` |
|   701412 | 10141 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - | 10142 | `		/* Release the call context */` |
|   701412 | 10143 | `		VmReleaseCallContext(&sCtx);` |
|   701412 | 10144 | `		if( rc == PH7_ABORT ){` |
|        - | 10145 | `			/* Release the (possibly partially-built) result slot before unwinding;` |
|        - | 10146 | `			 * the Abort: label only frees the operand stack, not this local` |
|        - | 10147 | `			 * (mirrors the PH7_EXCEPTION branch below). */` |
|      531 | 10148 | `			PH7_MemObjRelease(&sRet);` |
|      531 | 10149 | `			goto Abort;` |
|   700882 | 10150 | `		}else if( rc == PH7_EXCEPTION ){` |
|      116 | 10151 | `			VmFrame *pFrm = pVm->pFrame;` |
|      116 | 10152 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|      116 | 10153 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - | 10154 | `				/* Exception was NOT caught, propagate */` |
|        5 | 10155 | `				goto Exception;` |
|        - | 10156 | `			}` |
|        - | 10157 | `			/* Exception was caught: pop args and the result slot */` |
|      112 | 10158 | `			PH7_MemObjRelease(&sRet);` |
|      112 | 10159 | `			if( pInstr->iP1 > 0 ){` |
|       96 | 10160 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       47 | 10161 | `			}` |
|        - | 10162 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|      112 | 10163 | `			VmPopOperand(&pTos,1);` |
|        - | 10164 | `			/* Jump past the try/catch block via the exception frame */` |
|      112 | 10165 | `			pFrm = pVm->pFrame;` |
|      112 | 10166 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|      112 | 10167 | `				pc = pFrm->iExceptionJump - 1;` |
|       55 | 10168 | `			}` |
|      112 | 10169 | `			break;` |
|        - | 10170 | `		}` |
|   700768 | 10171 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - | 10172 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - | 10173 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - | 10174 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - | 10175 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - | 10176 | `			 * and we need to save state here. If it's a nested call (method` |
|        - | 10177 | `			 * body), the user-function path above will handle re-saving. */` |
|       40 | 10178 | `			PH7_MemObjRelease(&sRet);` |
|       40 | 10179 | `			if( pInstr->iP1 > 0 ){` |
|       40 | 10180 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 | 10181 | `			}` |
|        - | 10182 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - | 10183 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       40 | 10184 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       40 | 10185 | `			goto Suspend;` |
|        - | 10186 | `		}` |
|   700730 | 10187 | `		if( pInstr->iP1 > 0 ){` |
|        - | 10188 | `			/* Pop function name and arguments */` |
|   678572 | 10189 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   339307 | 10190 | `		}` |
|        - | 10191 | `		/* Save foreign function return value */` |
|   700730 | 10192 | `		PH7_MemObjStore(&sRet,pTos);` |
|   700730 | 10193 | `		PH7_MemObjRelease(&sRet);` |
|        - | 10194 | `	}` |
|   719620 | 10195 | `	break;` |
|        - | 10196 | `				  }` |
|        - | 10197 | `/*` |
|        - | 10198 | ` * OP_CONSUME: P1 * *` |
|        - | 10199 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - | 10200 | ` */` |
|    16126 | 10201 | `case PH7_OP_CONSUME: {` |
|    32254 | 10202 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    32254 | 10203 | `	ph7_value *pCur,*pOut = pTos;` |
|        - | 10204 |  |
|    32254 | 10205 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    32254 | 10206 | `	pCur = pOut;` |
|        - | 10207 | `	/* Start the consume process  */` |
|    64548 | 10208 | `	while( pOut <= pTos ){` |
|        - | 10209 | `		/* Force a string cast */` |
|    32296 | 10210 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1074 | 10211 | `			PH7_MemObjToString(pOut);` |
|      536 | 10212 | `		}` |
|    32296 | 10213 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - | 10214 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - | 10215 | `			/* Invoke the output consumer callback */` |
|    19804 | 10216 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    19804 | 10217 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    19804 | 10218 | `			SyBlobRelease(&pOut->sBlob);` |
|    19804 | 10219 | `			if( rc == SXERR_ABORT ){` |
|        - | 10220 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 | 10221 | `				goto Abort;` |
|        - | 10222 | `			}` |
|     9901 | 10223 | `		}` |
|    32296 | 10224 | `		pOut++;` |
|        2 | 10225 | `	}` |
|    32254 | 10226 | `	pTos = &pCur[-1];` |
|    32252 | 10227 | `	break;` |
|        - | 10228 | `					 }` |
|        - | 10229 |  |
|        - | 10230 | `		} /* Switch() */` |
| 11820162 | 10231 | `		pc++; /* Next instruction in the stream */` |
|        2 | 10232 | `	} /* For(;;) */` |
|    22716 | 10233 | `Done:` |
|    45434 | 10234 | `	SySetRelease(&aArg);` |
|    45434 | 10235 | `	return SXRET_OK;` |
|      100 | 10236 | `Suspend:` |
|      202 | 10237 | `	SySetRelease(&aArg);` |
|      202 | 10238 | `	return PH7_SUSPEND;` |
|      349 | 10239 | `Abort:` |
|      699 | 10240 | `	SySetRelease(&aArg);` |
|     2185 | 10241 | `	while( pTos >= pStack ){` |
|     1487 | 10242 | `		PH7_MemObjRelease(pTos);` |
|     1487 | 10243 | `		pTos--;` |
|        1 | 10244 | `	}` |
|      699 | 10245 | `	return PH7_ABORT;` |
|       34 | 10246 | `Exception:` |
|       70 | 10247 | `	SySetRelease(&aArg);` |
|      126 | 10248 | `	while( pTos >= pStack ){` |
|       58 | 10249 | `		PH7_MemObjRelease(pTos);` |
|       58 | 10250 | `		pTos--;` |
|        2 | 10251 | `	}` |
|       70 | 10252 | `	return PH7_EXCEPTION;` |
|    23201 | 10253 |  |
|        - | 10254 | `/*` |
|        - | 10255 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - | 10256 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - | 10257 | ` * See block-comment on that function for additional information.` |
|        - | 10258 | ` */` |
|    20844 | 10259 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 | 10260 |  |
|        - | 10261 | `	ph7_value *pStack;` |
|        - | 10262 | `	sxi32 rc;` |
|        - | 10263 | `	/* Allocate a new operand stack */` |
|    20846 | 10264 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    20846 | 10265 | `	if( pStack == 0 ){` |
|      ! 0 | 10266 | `		return SXERR_MEM;` |
|        - | 10267 | `	}` |
|        - | 10268 | `	/* Execute the program */` |
|    20846 | 10269 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0,0);` |
|        - | 10270 | `	/* Free the operand stack */` |
|    20846 | 10271 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - | 10272 | `	/* Execution result */` |
|    20846 | 10273 | `	return rc;` |
|    10424 | 10274 |  |
|        - | 10275 | `/*` |
|        - | 10276 | ` * Invoke any installed shutdown callbacks.` |
|        - | 10277 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - | 10278 | ` * or more calls to [register_shutdown_function()].` |
|        - | 10279 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - | 10280 | ` * execution ends.` |
|        - | 10281 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - | 10282 | ` * additional information.` |
|        - | 10283 | ` */` |
|     2840 | 10284 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 | 10285 |  |
|        - | 10286 | `	VmShutdownCB *pEntry;` |
|        - | 10287 | `	ph7_value *apArg[10];` |
|        - | 10288 | `	sxu32 n,nEntry;` |
|        - | 10289 | `	int i;` |
|        - | 10290 | `	/* Point to the stack of registered callbacks */` |
|     2842 | 10291 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    31242 | 10292 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    28402 | 10293 | `		apArg[i] = 0;` |
|    14202 | 10294 | `	}` |
|        - | 10295 | `	/* A halt that led us here is consumed; a fresh one set by a callback` |
|        - | 10296 | `	 * (i.e. exit() inside a shutdown function) skips the remaining` |
|        - | 10297 | `	 * callbacks, mirroring PHP.` |
|        - | 10298 | `	 */` |
|     2842 | 10299 | `	pVm->bHaltRequested = 0;` |
|     2854 | 10300 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       14 | 10301 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       14 | 10302 | `		if( pEntry ){` |
|        - | 10303 | `			/* Prepare callback arguments if any */` |
|       14 | 10304 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 | 10305 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 | 10306 | `					break;` |
|        - | 10307 | `				}` |
|      ! 0 | 10308 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 | 10309 | `			}` |
|        - | 10310 | `			/* Invoke the callback */` |
|       14 | 10311 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - | 10312 | `			/*` |
|        - | 10313 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - | 10314 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - | 10315 | `			 */` |
|       14 | 10316 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       14 | 10317 | `			if( pEntry ){` |
|       14 | 10318 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|       14 | 10319 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 | 10320 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 | 10321 | `				}` |
|        6 | 10322 | `			}` |
|       14 | 10323 | `			if( pVm->bHaltRequested ){` |
|        - | 10324 | `				/* exit() inside the callback: skip the remaining callbacks */` |
|      ! 0 | 10325 | `				break;` |
|        - | 10326 | `			}` |
|        6 | 10327 | `		}` |
|        8 | 10328 | `	}` |
|     2842 | 10329 | `	SySetReset(&pVm->aShutdown);` |
|     2842 | 10330 |  |
|        - | 10331 | `/*` |
|        - | 10332 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - | 10333 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - | 10334 | ` * See block-comment on that function for additional information.` |
|        - | 10335 | ` */` |
|     2840 | 10336 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 | 10337 |  |
|        - | 10338 | `	/* Make sure we are ready to execute this program */` |
|     2842 | 10339 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 | 10340 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - | 10341 | `	}` |
|        - | 10342 | `	/* Set the execution magic number  */` |
|     2842 | 10343 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - | 10344 | `	/* Execute the program */` |
|     2842 | 10345 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0,0);` |
|        - | 10346 | `	/* Invoke any shutdown callbacks */` |
|     2842 | 10347 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - | 10348 | `	/*` |
|        - | 10349 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - | 10350 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - | 10351 | `	 * [ph7_vm_reset()] first would fail.` |
|        - | 10352 | `	 */` |
|     2842 | 10353 | `	return SXRET_OK;` |
|     1422 | 10354 |  |
|        - | 10355 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - | 10356 | `/*` |
|        - | 10357 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - | 10358 | ` * The context is in CREATED state and ready to be started.` |
|        - | 10359 | ` */` |
|       72 | 10360 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        2 | 10361 |  |
|        - | 10362 | `	ph7_exec_ctx *pCtx;` |
|        - | 10363 | `	ph7_value *pStack;` |
|        - | 10364 | `	VmFrame *pFrame;` |
|       74 | 10365 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       74 | 10366 | `	if( pCtx == 0 ){` |
|      ! 0 | 10367 | `		return 0;` |
|        - | 10368 | `	}` |
|       74 | 10369 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       74 | 10370 | `	pCtx->pVm = pVm;` |
|       74 | 10371 | `	pCtx->pFunc = pFunc;` |
|       74 | 10372 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       74 | 10373 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       74 | 10374 | `	pCtx->pc = 0;` |
|       74 | 10375 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       74 | 10376 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - | 10377 | `	/* Allocate a private operand stack */` |
|       74 | 10378 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       74 | 10379 | `	if( pStack == 0 ){` |
|      ! 0 | 10380 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 | 10381 | `		return 0;` |
|        - | 10382 | `	}` |
|       74 | 10383 | `	pCtx->pStack = pStack;` |
|        - | 10384 | `	/* Create a detached frame for the fiber */` |
|       74 | 10385 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       74 | 10386 | `	if( pFrame == 0 ){` |
|      ! 0 | 10387 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 | 10388 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 | 10389 | `		return 0;` |
|        - | 10390 | `	}` |
|       74 | 10391 | `	pCtx->pFrame = pFrame;` |
|       74 | 10392 | `	return pCtx;` |
|       38 | 10393 |  |
|        - | 10394 | `/*` |
|        - | 10395 | ` * Start executing a fiber context for the first time.` |
|        - | 10396 | ` */` |
|       68 | 10397 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        2 | 10398 |  |
|        - | 10399 | `	ph7_exec_ctx *pOldCtx;` |
|        - | 10400 | `	sxi32 rc;` |
|       70 | 10401 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10402 | `		return SXERR_INVALID;` |
|        - | 10403 | `	}` |
|        - | 10404 | `	/* Bound fiber/generator nesting under the same cap (each start adds a C` |
|        - | 10405 | `	 * frame); reject before mutating VM state so the abort is clean. */` |
|       70 | 10406 | `	if( VmRecursionExceeded(pVm) ){` |
|      ! 0 | 10407 | `		return VmRecursionFatal(pVm);` |
|        - | 10408 | `	}` |
|        - | 10409 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       70 | 10410 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       70 | 10411 | `	pVm->pFrame = pCtx->pFrame;` |
|        - | 10412 | `	/* Save and set the active context */` |
|       70 | 10413 | `	pOldCtx = pVm->pActiveCtx;` |
|       70 | 10414 | `	pVm->pActiveCtx = pCtx;` |
|       70 | 10415 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       70 | 10416 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       70 | 10417 | `	pVm->nRecursionDepth++;` |
|        - | 10418 | `	/* Execute from the beginning */` |
|       70 | 10419 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       34 | 10420 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0,` |
|       68 | 10421 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0);` |
|       70 | 10422 | `	pVm->nRecursionDepth--;` |
|        - | 10423 | `	/* Restore the previous context */` |
|       70 | 10424 | `	pVm->pActiveCtx = pOldCtx;` |
|       70 | 10425 | `	if( rc == PH7_SUSPEND ){` |
|        - | 10426 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       66 | 10427 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       66 | 10428 | `		pCtx->pFrame->pParent = 0;` |
|       66 | 10429 | `		if( pResult ){` |
|       24 | 10430 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 | 10431 | `		}` |
|       66 | 10432 | `		return SXRET_OK;` |
|        - | 10433 | `	}` |
|        - | 10434 | `	/* Detach frame */` |
|        6 | 10435 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        6 | 10436 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        6 | 10437 | `		pCtx->pFrame->pParent = 0;` |
|        2 | 10438 | `	}` |
|        6 | 10439 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10440 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10441 | `		return PH7_ABORT;` |
|        - | 10442 | `	}` |
|        6 | 10443 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10444 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10445 | `		return PH7_EXCEPTION;` |
|        - | 10446 | `	}` |
|        - | 10447 | `	/* Normal completion */` |
|        6 | 10448 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        6 | 10449 | `	if( pResult ){` |
|        3 | 10450 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 | 10451 | `	}` |
|        6 | 10452 | `	return SXRET_OK;` |
|       36 | 10453 |  |
|        - | 10454 | `/*` |
|        - | 10455 | ` * Resume a suspended fiber context.` |
|        - | 10456 | ` */` |
|      150 | 10457 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        2 | 10458 |  |
|        - | 10459 | `	ph7_exec_ctx *pOldCtx;` |
|        - | 10460 | `	sxi32 rc;` |
|      152 | 10461 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 | 10462 | `		return SXERR_INVALID;` |
|        - | 10463 | `	}` |
|        - | 10464 | `	/* Bound fiber/generator nesting under the same cap; reject before mutating` |
|        - | 10465 | `	 * VM state so the abort is clean. */` |
|      152 | 10466 | `	if( VmRecursionExceeded(pVm) ){` |
|      ! 0 | 10467 | `		return VmRecursionFatal(pVm);` |
|        - | 10468 | `	}` |
|        - | 10469 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - | 10470 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - | 10471 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|      152 | 10472 | `	if( pResumeValue ){` |
|       40 | 10473 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       21 | 10474 | `	}else{` |
|      114 | 10475 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - | 10476 | `	}` |
|      152 | 10477 | `	pCtx->nTos++;` |
|        - | 10478 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|      152 | 10479 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      152 | 10480 | `	pVm->pFrame = pCtx->pFrame;` |
|        - | 10481 | `	/* Save and set the active context */` |
|      152 | 10482 | `	pOldCtx = pVm->pActiveCtx;` |
|      152 | 10483 | `	pVm->pActiveCtx = pCtx;` |
|      152 | 10484 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|      152 | 10485 | `	pVm->nRecursionDepth++;` |
|        - | 10486 | `	/* Resume execution from saved PC */` |
|      152 | 10487 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       75 | 10488 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc,` |
|      150 | 10489 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0);` |
|      152 | 10490 | `	pVm->nRecursionDepth--;` |
|        - | 10491 | `	/* Restore the previous context */` |
|      152 | 10492 | `	pVm->pActiveCtx = pOldCtx;` |
|      152 | 10493 | `	if( rc == PH7_SUSPEND ){` |
|        - | 10494 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|      100 | 10495 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|      100 | 10496 | `		pCtx->pFrame->pParent = 0;` |
|      100 | 10497 | `		if( pResult ){` |
|       18 | 10498 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 | 10499 | `		}` |
|      100 | 10500 | `		return SXRET_OK;` |
|        - | 10501 | `	}` |
|        - | 10502 | `	/* Detach frame */` |
|       54 | 10503 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       54 | 10504 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       54 | 10505 | `		pCtx->pFrame->pParent = 0;` |
|       26 | 10506 | `	}` |
|       54 | 10507 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10508 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10509 | `		return PH7_ABORT;` |
|        - | 10510 | `	}` |
|       54 | 10511 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10512 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10513 | `		return PH7_EXCEPTION;` |
|        - | 10514 | `	}` |
|        - | 10515 | `	/* Normal completion */` |
|       54 | 10516 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       54 | 10517 | `	if( pResult ){` |
|       20 | 10518 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 | 10519 | `	}` |
|       54 | 10520 | `	return SXRET_OK;` |
|       77 | 10521 |  |
|        - | 10522 | `/*` |
|        - | 10523 | ` * Release an execution context and all its resources.` |
|        - | 10524 | ` */` |
|        4 | 10525 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 | 10526 |  |
|        5 | 10527 | `	if( pCtx == 0 ){` |
|      ! 0 | 10528 | `		return;` |
|        - | 10529 | `	}` |
|        5 | 10530 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - | 10531 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 | 10532 | `		return;` |
|        - | 10533 | `	}` |
|        5 | 10534 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - | 10535 | `	/* Release values */` |
|        5 | 10536 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|        5 | 10537 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - | 10538 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|        5 | 10539 | `	if( pCtx->pFrame ){` |
|        - | 10540 | `		VmSlot *aSlot;` |
|        - | 10541 | `		sxu32 n;` |
|        - | 10542 | `		/* Free local variables */` |
|        5 | 10543 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|       11 | 10544 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|        7 | 10545 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|        4 | 10546 | `		}` |
|        - | 10547 | `		/* Remove local references */` |
|        5 | 10548 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|       11 | 10549 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|        7 | 10550 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|        4 | 10551 | `		}` |
|        5 | 10552 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|        5 | 10553 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|        5 | 10554 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|        5 | 10555 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|        5 | 10556 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|        5 | 10557 | `		pCtx->pFrame = 0;` |
|        2 | 10558 | `	}` |
|        - | 10559 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - | 10560 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - | 10561 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|        5 | 10562 | `	if( pCtx->pStack ){` |
|        5 | 10563 | `		if( pCtx->nTos >= 0 ){` |
|        5 | 10564 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|        9 | 10565 | `			while( pTos >= pCtx->pStack ){` |
|        5 | 10566 | `				PH7_MemObjRelease(pTos);` |
|        5 | 10567 | `				pTos--;` |
|        1 | 10568 | `			}` |
|        2 | 10569 | `		}` |
|        5 | 10570 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|        5 | 10571 | `		pCtx->pStack = 0;` |
|        2 | 10572 | `	}` |
|        - | 10573 | `	/* Free the context itself */` |
|        5 | 10574 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|        3 | 10575 |  |
|        - | 10576 | `/*` |
|        - | 10577 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - | 10578 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - | 10579 | ` */` |
|       90 | 10580 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        2 | 10581 |  |
|        - | 10582 | `	ph7_class_instance *pThis;` |
|        - | 10583 | `	SyString sAttr;` |
|        - | 10584 | `	ph7_value *pAttr;` |
|       92 | 10585 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10586 | `		return 0;` |
|        - | 10587 | `	}` |
|       92 | 10588 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       92 | 10589 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 | 10590 | `		return 0;` |
|        - | 10591 | `	}` |
|       92 | 10592 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       92 | 10593 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       92 | 10594 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       32 | 10595 | `		return 0;` |
|        - | 10596 | `	}` |
|       62 | 10597 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       47 | 10598 |  |
|        - | 10599 | `/*` |
|        - | 10600 | ` * Fiber::suspend($value = null) — static method.` |
|        - | 10601 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - | 10602 | ` */` |
|       38 | 10603 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10604 |  |
|       40 | 10605 | `	ph7_vm *pVm = pCtx->pVm;` |
|       40 | 10606 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 | 10607 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10608 | `			"Cannot suspend outside of a fiber");` |
|        - | 10609 | `	}` |
|       40 | 10610 | `	if( nArg > 0 ){` |
|       40 | 10611 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       21 | 10612 | `	}else{` |
|      ! 0 | 10613 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - | 10614 | `	}` |
|       40 | 10615 | `	return PH7_SUSPEND;` |
|       21 | 10616 |  |
|        - | 10617 | `/*` |
|        - | 10618 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - | 10619 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - | 10620 | ` * and closure-environment binding happen with the correct argument context.` |
|        - | 10621 | ` */` |
|       24 | 10622 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10623 |  |
|        - | 10624 | `	ph7_class_instance *pThis;` |
|        - | 10625 | `	ph7_value *pAttr;` |
|        - | 10626 | `	SyString sAttrName;` |
|       26 | 10627 | `	if( nArg < 2 ){` |
|      ! 0 | 10628 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10629 | `			"Fiber::__construct() expects a callable argument");` |
|        - | 10630 | `	}` |
|       26 | 10631 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10632 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10633 | `			"Fiber::__construct(): invalid $this");` |
|        - | 10634 | `	}` |
|       26 | 10635 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       26 | 10636 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 | 10637 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10638 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - | 10639 | `	}` |
|        - | 10640 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       26 | 10641 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 | 10642 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10643 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - | 10644 | `	}` |
|        - | 10645 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       26 | 10646 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 | 10647 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 | 10648 | `	if( pAttr ){` |
|       26 | 10649 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 | 10650 | `	}` |
|       26 | 10651 | `	return PH7_OK;` |
|       14 | 10652 |  |
|        - | 10653 | `/*` |
|        - | 10654 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - | 10655 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - | 10656 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - | 10657 | ` * so that start() can bind it as $this for the closure environment.` |
|        - | 10658 | ` */` |
|       24 | 10659 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - | 10660 | `	ph7_class_instance **ppThis)` |
|        2 | 10661 |  |
|       26 | 10662 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10663 | `	ph7_value *pCallable;` |
|        - | 10664 | `	SyString sAttrName;` |
|       26 | 10665 | `	*ppThis = 0;` |
|       26 | 10666 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 | 10667 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       26 | 10668 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 | 10669 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 | 10670 | `		return 0;` |
|        - | 10671 | `	}` |
|       26 | 10672 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - | 10673 | `		/* String callable — look up in user functions with overload support */` |
|        - | 10674 | `		SyString sName;` |
|        - | 10675 | `		SyHashEntry *pEntry;` |
|        - | 10676 | `		ph7_vm_func *pFunc;` |
|       26 | 10677 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       26 | 10678 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       26 | 10679 | `		if( pEntry == 0 ){` |
|      ! 0 | 10680 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 | 10681 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 | 10682 | `			return 0;` |
|        - | 10683 | `		}` |
|       26 | 10684 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       26 | 10685 | `		return pFunc;` |
|      ! 0 | 10686 | `	}else{` |
|        - | 10687 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 | 10688 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 | 10689 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - | 10690 | `			sizeof("__invoke") - 1);` |
|      ! 0 | 10691 | `		if( pMethod == 0 ){` |
|      ! 0 | 10692 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10693 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 | 10694 | `			return 0;` |
|        - | 10695 | `		}` |
|      ! 0 | 10696 | `		*ppThis = pClosure;` |
|      ! 0 | 10697 | `		return &pMethod->sFunc;` |
|        - | 10698 | `	}` |
|       14 | 10699 |  |
|        - | 10700 | `/*` |
|        - | 10701 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - | 10702 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - | 10703 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - | 10704 | ` */` |
|       72 | 10705 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - | 10706 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        2 | 10707 |  |
|       74 | 10708 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - | 10709 | `	ph7_vm_func_arg *aFormalArg;` |
|        - | 10710 | `	sxu32 nFormal, n;` |
|        - | 10711 | `	VmSlot sSlot;` |
|        - | 10712 | `	sxi32 rc;` |
|        - | 10713 | `	/* Install $this for closure/method callables */` |
|       74 | 10714 | `	if( pClosureThis ){` |
|        - | 10715 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 | 10716 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 | 10717 | `		if( pObj ){` |
|      ! 0 | 10718 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 | 10719 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 | 10720 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 | 10721 | `		}` |
|      ! 0 | 10722 | `	}` |
|        - | 10723 | `	/* Install static variables */` |
|       74 | 10724 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - | 10725 | `		ph7_vm_func_static_var *aStatic;` |
|        - | 10726 | `		ph7_value *pVal;` |
|      ! 0 | 10727 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 | 10728 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 | 10729 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 | 10730 | `			if( pVal ){` |
|      ! 0 | 10731 | `				sSlot.pUserData = 0;` |
|      ! 0 | 10732 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 | 10733 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 | 10734 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 | 10735 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 | 10736 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal);` |
|      ! 0 | 10737 | `				}` |
|      ! 0 | 10738 | `			}` |
|      ! 0 | 10739 | `		}` |
|      ! 0 | 10740 | `	}` |
|        - | 10741 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       74 | 10742 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       74 | 10743 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       92 | 10744 | `	for( n = 0; n < nFormal; n++ ){` |
|        - | 10745 | `		ph7_value *pObj;` |
|       20 | 10746 | `		if( n < (sxu32)nArg ){` |
|        - | 10747 | `			/* Argument provided — install with type casting */` |
|       20 | 10748 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       20 | 10749 | `			if( pObj ){` |
|       20 | 10750 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - | 10751 | `				/* Type casting */` |
|       20 | 10752 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 | 10753 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 | 10754 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 10755 | `						if( xCast ){` |
|      ! 0 | 10756 | `							xCast(pObj);` |
|      ! 0 | 10757 | `						}` |
|      ! 0 | 10758 | `					}` |
|      ! 0 | 10759 | `				}` |
|       20 | 10760 | `				sSlot.nIdx = pObj->nIdx;` |
|       20 | 10761 | `				sSlot.pUserData = 0;` |
|       20 | 10762 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       11 | 10763 | `			}` |
|        9 | 10764 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - | 10765 | `			/* Default value */` |
|      ! 0 | 10766 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 | 10767 | `			if( pObj ){` |
|      ! 0 | 10768 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj);` |
|      ! 0 | 10769 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10770 | `					return rc;` |
|        - | 10771 | `				}` |
|      ! 0 | 10772 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 | 10773 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 | 10774 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 10775 | `						if( xCast ){` |
|      ! 0 | 10776 | `							xCast(pObj);` |
|      ! 0 | 10777 | `						}` |
|      ! 0 | 10778 | `					}` |
|      ! 0 | 10779 | `				}` |
|      ! 0 | 10780 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 | 10781 | `				sSlot.pUserData = 0;` |
|      ! 0 | 10782 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 | 10783 | `			}` |
|      ! 0 | 10784 | `		}` |
|       11 | 10785 | `	}` |
|        - | 10786 | `	/* Install closure environment (captured variables) */` |
|       74 | 10787 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - | 10788 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - | 10789 | `		ph7_value *pValue;` |
|        - | 10790 | `		sxu32 iEnv;` |
|        3 | 10791 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 | 10792 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 | 10793 | `			pEnv = &aEnv[iEnv];` |
|        7 | 10794 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 | 10795 | `				continue;` |
|        - | 10796 | `			}` |
|        5 | 10797 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 | 10798 | `			if( pValue == 0 ){` |
|      ! 0 | 10799 | `				continue;` |
|        - | 10800 | `			}` |
|        5 | 10801 | `			PH7_MemObjRelease(pValue);` |
|        5 | 10802 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 | 10803 | `		}` |
|        1 | 10804 | `	}` |
|       74 | 10805 | `	return SXRET_OK;` |
|       38 | 10806 |  |
|        - | 10807 | `/*` |
|        - | 10808 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - | 10809 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - | 10810 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - | 10811 | ` */` |
|       26 | 10812 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10813 |  |
|       28 | 10814 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10815 | `	ph7_class_instance *pThis;` |
|        - | 10816 | `	ph7_class_instance *pClosureThis;` |
|        - | 10817 | `	ph7_exec_ctx *pExecCtx;` |
|        - | 10818 | `	ph7_vm_func *pFunc;` |
|        - | 10819 | `	ph7_value sResult;` |
|        - | 10820 | `	ph7_value *pCtxAttr;` |
|        - | 10821 | `	SyString sAttrName;` |
|        - | 10822 | `	sxi32 rc;` |
|       28 | 10823 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10824 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - | 10825 | `	}` |
|       28 | 10826 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 10827 | `	/* Check if already started (has a __ctx) */` |
|       28 | 10828 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       28 | 10829 | `	if( pExecCtx != 0 ){` |
|        3 | 10830 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10831 | `			"Cannot start a fiber that has already been started");` |
|        - | 10832 | `	}` |
|        - | 10833 | `	/* Resolve callable */` |
|       26 | 10834 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       26 | 10835 | `	if( pFunc == 0 ){` |
|      ! 0 | 10836 | `		return PH7_EXCEPTION;` |
|        - | 10837 | `	}` |
|        - | 10838 | `	/* Create execution context now that we know the function */` |
|       26 | 10839 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       26 | 10840 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10841 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10842 | `			"Fiber::start(): out of memory");` |
|        - | 10843 | `	}` |
|        - | 10844 | `	/* Store context in $this->__ctx */` |
|       26 | 10845 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       26 | 10846 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 | 10847 | `	if( pCtxAttr ){` |
|       26 | 10848 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       26 | 10849 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 | 10850 | `	}` |
|        - | 10851 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - | 10852 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - | 10853 | `	 * into the fiber's frame, not the caller's. */` |
|       26 | 10854 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       26 | 10855 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - | 10856 | `	/* Unpack the args array and install into the frame */` |
|        - | 10857 | `	{` |
|       26 | 10858 | `		ph7_value **apValues = 0;` |
|       26 | 10859 | `		ph7_value *aStore = 0;` |
|       26 | 10860 | `		int nActual = 0;` |
|       26 | 10861 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       26 | 10862 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - | 10863 | `			ph7_hashmap_node *pNode;` |
|       26 | 10864 | `			sxu32 nCount = pMap->nEntry;` |
|       26 | 10865 | `			if( nCount > 0 ){` |
|        3 | 10866 | `				sxu32 idx = 0;` |
|        4 | 10867 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 | 10868 | `					nCount * sizeof(ph7_value *));` |
|        4 | 10869 | `				aStore = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 | 10870 | `					nCount * sizeof(ph7_value));` |
|        3 | 10871 | `				if( apValues && aStore ){` |
|        3 | 10872 | `					pNode = pMap->pFirst;` |
|        7 | 10873 | `					while( pNode && idx < nCount ){` |
|        - | 10874 | `						/* Snapshot each source into stable storage: VmFiberSetupFrame reserves` |
|        - | 10875 | `						 * memory objects (VmExtractMemObj) before reading the args, which can` |
|        - | 10876 | `						 * reallocate (move) pVm->aMemObj and dangle a raw pool pointer. A` |
|        - | 10877 | `						 * shallow copy is a safe source — the referent and the heap-resident` |
|        - | 10878 | `						 * blob data survive the move (same sSafeVal idiom the hashmap inserters` |
|        - | 10879 | `						 * use); it owns nothing independently, so it needs no release. */` |
|        5 | 10880 | `						ph7_value *pSrc = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 | 10881 | `						if( pSrc ){` |
|        5 | 10882 | `							aStore[idx] = *pSrc;` |
|        3 | 10883 | `						}else{` |
|      ! 0 | 10884 | `							PH7_MemObjInit(pVm, &aStore[idx]);` |
|        - | 10885 | `						}` |
|        5 | 10886 | `						apValues[idx] = &aStore[idx];` |
|        5 | 10887 | `						idx++;` |
|        5 | 10888 | `						pNode = pNode->pPrev;` |
|        1 | 10889 | `					}` |
|        3 | 10890 | `					nActual = (int)idx;` |
|        1 | 10891 | `				}` |
|        1 | 10892 | `			}` |
|       12 | 10893 | `		}` |
|       26 | 10894 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       26 | 10895 | `		if( aStore ){` |
|        3 | 10896 | `			SyMemBackendFree(&pVm->sAllocator, aStore);` |
|        1 | 10897 | `		}` |
|       26 | 10898 | `		if( apValues ){` |
|        3 | 10899 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 | 10900 | `		}` |
|        - | 10901 | `	}` |
|        - | 10902 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       26 | 10903 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       26 | 10904 | `	pExecCtx->pFrame->pParent = 0;` |
|       26 | 10905 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 10906 | `		return PH7_ABORT;` |
|        - | 10907 | `	}` |
|       26 | 10908 | `	PH7_MemObjInit(pVm, &sResult);` |
|       26 | 10909 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       26 | 10910 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10911 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10912 | `		return PH7_ABORT;` |
|        - | 10913 | `	}` |
|       26 | 10914 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10915 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10916 | `		return PH7_EXCEPTION;` |
|        - | 10917 | `	}` |
|       26 | 10918 | `	ph7_result_value(pCtx, &sResult);` |
|       26 | 10919 | `	PH7_MemObjRelease(&sResult);` |
|       26 | 10920 | `	return PH7_OK;` |
|       15 | 10921 |  |
|        - | 10922 | `/*` |
|        - | 10923 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - | 10924 | ` */` |
|       36 | 10925 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10926 |  |
|       38 | 10927 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10928 | `	ph7_exec_ctx *pExecCtx;` |
|        - | 10929 | `	ph7_value sResult;` |
|        - | 10930 | `	ph7_value *pResumeVal;` |
|        - | 10931 | `	sxi32 rc;` |
|       38 | 10932 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10933 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 | 10934 | `		return PH7_OK;` |
|        - | 10935 | `	}` |
|       38 | 10936 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       38 | 10937 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10938 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 | 10939 | `		return PH7_OK;` |
|        - | 10940 | `	}` |
|       38 | 10941 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 | 10942 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10943 | `			"Cannot resume a fiber that is not suspended");` |
|        - | 10944 | `	}` |
|       36 | 10945 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       36 | 10946 | `	PH7_MemObjInit(pVm, &sResult);` |
|       36 | 10947 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       36 | 10948 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10949 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10950 | `		return PH7_ABORT;` |
|        - | 10951 | `	}` |
|       36 | 10952 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10953 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10954 | `		return PH7_EXCEPTION;` |
|        - | 10955 | `	}` |
|       36 | 10956 | `	ph7_result_value(pCtx, &sResult);` |
|       36 | 10957 | `	PH7_MemObjRelease(&sResult);` |
|       36 | 10958 | `	return PH7_OK;` |
|       20 | 10959 |  |
|        - | 10960 | `/*` |
|        - | 10961 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - | 10962 | ` */` |
|        6 | 10963 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10964 |  |
|        8 | 10965 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10966 | `	ph7_exec_ctx *pExecCtx;` |
|        8 | 10967 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10968 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10969 | `		return PH7_OK;` |
|        - | 10970 | `	}` |
|        8 | 10971 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        8 | 10972 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10973 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10974 | `		return PH7_OK;` |
|        - | 10975 | `	}` |
|        8 | 10976 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 | 10977 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10978 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10979 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - | 10980 | `		}` |
|      ! 0 | 10981 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10982 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - | 10983 | `	}` |
|        8 | 10984 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        8 | 10985 | `	return PH7_OK;` |
|        5 | 10986 |  |
|        - | 10987 | `/*` |
|        - | 10988 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - | 10989 | ` */` |
|        6 | 10990 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10991 |  |
|        - | 10992 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 10993 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 10994 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 10995 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 | 10996 | `	return PH7_OK;` |
|        4 | 10997 |  |
|      ! 0 | 10998 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 10999 |  |
|        - | 11000 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 | 11001 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 | 11002 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 11003 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 | 11004 | `	return PH7_OK;` |
|      ! 0 | 11005 |  |
|        6 | 11006 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 11007 |  |
|        - | 11008 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 11009 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 11010 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 11011 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 | 11012 | `	return PH7_OK;` |
|        4 | 11013 |  |
|        6 | 11014 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 11015 |  |
|        - | 11016 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 11017 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 11018 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 11019 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 | 11020 | `	return PH7_OK;` |
|        4 | 11021 |  |
|        - | 11022 | `/*` |
|        - | 11023 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - | 11024 | ` */` |
|        4 | 11025 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 11026 |  |
|        5 | 11027 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11028 | `	ph7_exec_ctx *pExecCtx;` |
|        5 | 11029 | `	if( nArg < 1 ){` |
|      ! 0 | 11030 | `		return PH7_OK;` |
|        - | 11031 | `	}` |
|        5 | 11032 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        5 | 11033 | `	if( pExecCtx ){` |
|        5 | 11034 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - | 11035 | `		/* Clear the attribute so double-free is prevented */` |
|        5 | 11036 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        5 | 11037 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 11038 | `			SyString sAttrName;` |
|        - | 11039 | `			ph7_value *pAttr;` |
|        5 | 11040 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|        5 | 11041 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|        5 | 11042 | `			if( pAttr ){` |
|        5 | 11043 | `				PH7_MemObjRelease(pAttr);` |
|        2 | 11044 | `			}` |
|        2 | 11045 | `		}` |
|        2 | 11046 | `	}` |
|        5 | 11047 | `	return PH7_OK;` |
|        3 | 11048 |  |
|        - | 11049 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 | 11050 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 | 11051 |  |
|        - | 11052 | `	ph7_class_instance *pThis;` |
|      ! 0 | 11053 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 | 11054 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 | 11055 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 | 11056 |  |
|      ! 0 | 11057 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 | 11058 |  |
|        - | 11059 | `	ph7_class_instance *pThis;` |
|      ! 0 | 11060 | `	ph7_class_instance *pClosureThis = 0;` |
|        - | 11061 | `	ph7_exec_ctx *pCtx;` |
|        - | 11062 | `	ph7_vm_func *pFunc;` |
|        - | 11063 | `	ph7_value *pCallable;` |
|        - | 11064 | `	ph7_value *pCtxAttr;` |
|        - | 11065 | `	SyString sAttrName;` |
|        - | 11066 | `	/* Must not already be started */` |
|      ! 0 | 11067 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 11068 | `	if( pCtx != 0 ){` |
|      ! 0 | 11069 | `		return SXERR_INVALID;` |
|        - | 11070 | `	}` |
|      ! 0 | 11071 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 11072 | `		return SXERR_INVALID;` |
|        - | 11073 | `	}` |
|      ! 0 | 11074 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - | 11075 | `	/* Get the callable */` |
|      ! 0 | 11076 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 | 11077 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 11078 | `	if( pCallable == 0 ){` |
|      ! 0 | 11079 | `		return SXERR_INVALID;` |
|        - | 11080 | `	}` |
|        - | 11081 | `	/* Resolve callable */` |
|      ! 0 | 11082 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - | 11083 | `		SyString sName;` |
|        - | 11084 | `		SyHashEntry *pEntry;` |
|      ! 0 | 11085 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 | 11086 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 | 11087 | `		if( pEntry == 0 ){` |
|      ! 0 | 11088 | `			return SXERR_NOTFOUND;` |
|        - | 11089 | `		}` |
|      ! 0 | 11090 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 | 11091 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 11092 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 | 11093 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - | 11094 | `			sizeof("__invoke") - 1);` |
|      ! 0 | 11095 | `		if( pMethod == 0 ){` |
|      ! 0 | 11096 | `			return SXERR_INVALID;` |
|        - | 11097 | `		}` |
|      ! 0 | 11098 | `		pClosureThis = pClosure;` |
|      ! 0 | 11099 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 | 11100 | `	}else{` |
|      ! 0 | 11101 | `		return SXERR_INVALID;` |
|        - | 11102 | `	}` |
|        - | 11103 | `	/* Create context */` |
|      ! 0 | 11104 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 | 11105 | `	if( pCtx == 0 ){` |
|      ! 0 | 11106 | `		return SXERR_MEM;` |
|        - | 11107 | `	}` |
|        - | 11108 | `	/* Store in __ctx */` |
|      ! 0 | 11109 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 | 11110 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 11111 | `	if( pCtxAttr ){` |
|      ! 0 | 11112 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 | 11113 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 | 11114 | `	}` |
|        - | 11115 | `	/* Set up frame with args */` |
|      ! 0 | 11116 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 | 11117 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 | 11118 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 | 11119 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 | 11120 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 | 11121 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 | 11122 |  |
|      ! 0 | 11123 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 | 11124 |  |
|      ! 0 | 11125 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 11126 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 | 11127 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 | 11128 |  |
|      ! 0 | 11129 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 11130 |  |
|      ! 0 | 11131 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 11132 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 | 11133 |  |
|      ! 0 | 11134 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 11135 |  |
|      ! 0 | 11136 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 11137 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 | 11138 |  |
|      ! 0 | 11139 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 11140 |  |
|      ! 0 | 11141 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 11142 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 | 11143 | `	return &pCtx->sRetValue;` |
|      ! 0 | 11144 |  |
|        - | 11145 | `/* ======================== Generator Infrastructure ======================== */` |
|        - | 11146 | `/*` |
|        - | 11147 | ` * Allocate a new generator wrapper around an execution context.` |
|        - | 11148 | ` */` |
|       48 | 11149 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        2 | 11150 |  |
|        - | 11151 | `	ph7_generator *pGen;` |
|       50 | 11152 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       50 | 11153 | `	if( pGen == 0 ){` |
|      ! 0 | 11154 | `		return 0;` |
|        - | 11155 | `	}` |
|       50 | 11156 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       50 | 11157 | `	pGen->pCtx = pCtx;` |
|       50 | 11158 | `	pGen->iImplicitKey = 0;` |
|       50 | 11159 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       50 | 11160 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - | 11161 | `	/* Link the generator back to the exec context */` |
|       50 | 11162 | `	pCtx->pPrivate = pGen;` |
|       50 | 11163 | `	return pGen;` |
|       26 | 11164 |  |
|        - | 11165 | `/*` |
|        - | 11166 | ` * Release a generator and its execution context.` |
|        - | 11167 | ` */` |
|      ! 0 | 11168 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 | 11169 |  |
|      ! 0 | 11170 | `	if( pGen == 0 ){` |
|      ! 0 | 11171 | `		return;` |
|        - | 11172 | `	}` |
|      ! 0 | 11173 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 | 11174 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 | 11175 | `	if( pGen->pCtx ){` |
|      ! 0 | 11176 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 | 11177 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 | 11178 | `		pGen->pCtx = 0;` |
|      ! 0 | 11179 | `	}` |
|      ! 0 | 11180 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 | 11181 |  |
|        - | 11182 | `/*` |
|        - | 11183 | ` * Extract ph7_generator from a Generator class instance.` |
|        - | 11184 | ` */` |
|      496 | 11185 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        2 | 11186 |  |
|        - | 11187 | `	ph7_class_instance *pThis;` |
|        - | 11188 | `	SyString sAttr;` |
|        - | 11189 | `	ph7_value *pAttr;` |
|      498 | 11190 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 11191 | `		return 0;` |
|        - | 11192 | `	}` |
|      498 | 11193 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      498 | 11194 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 | 11195 | `		return 0;` |
|        - | 11196 | `	}` |
|      498 | 11197 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      498 | 11198 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      498 | 11199 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 | 11200 | `		return 0;` |
|        - | 11201 | `	}` |
|      498 | 11202 | `	return (ph7_generator *)pAttr->x.pOther;` |
|      250 | 11203 |  |
|        - | 11204 | `/*` |
|        - | 11205 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - | 11206 | ` */` |
|       44 | 11207 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 11208 |  |
|        - | 11209 | `	ph7_generator *pGen;` |
|        - | 11210 | `	sxi32 rc;` |
|       46 | 11211 | `	if( nArg < 1 ) return PH7_OK;` |
|       46 | 11212 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       46 | 11213 | `	if( pGen == 0 ) return PH7_OK;` |
|       46 | 11214 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       46 | 11215 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       46 | 11216 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       46 | 11217 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       22 | 11218 | `	}` |
|       46 | 11219 | `	return PH7_OK;` |
|       24 | 11220 |  |
|        - | 11221 | `/*` |
|        - | 11222 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - | 11223 | ` */` |
|      142 | 11224 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 11225 |  |
|        - | 11226 | `	ph7_generator *pGen;` |
|      144 | 11227 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      144 | 11228 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      144 | 11229 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|      144 | 11230 | `	return PH7_OK;` |
|       73 | 11231 |  |
|        - | 11232 | `/*` |
|        - | 11233 | ` * Generator::current() — return the last yielded value.` |
|        - | 11234 | ` * Auto-starts the generator on first access (like PHP).` |
|        - | 11235 | ` */` |
|      124 | 11236 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 11237 |  |
|        - | 11238 | `	ph7_generator *pGen;` |
|        - | 11239 | `	sxi32 rc;` |
|      126 | 11240 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      126 | 11241 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      126 | 11242 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      126 | 11243 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 11244 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 | 11245 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 | 11246 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 | 11247 | `	}` |
|      126 | 11248 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|      126 | 11249 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       64 | 11250 | `	}else{` |
|      ! 0 | 11251 | `		ph7_result_null(pCtx);` |
|        - | 11252 | `	}` |
|      126 | 11253 | `	return PH7_OK;` |
|       64 | 11254 |  |
|        - | 11255 | `/*` |
|        - | 11256 | ` * Generator::key() — return the last yielded key.` |
|        - | 11257 | ` * Auto-starts the generator on first access (like PHP).` |
|        - | 11258 | ` */` |
|       68 | 11259 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 11260 |  |
|        - | 11261 | `	ph7_generator *pGen;` |
|        - | 11262 | `	sxi32 rc;` |
|       70 | 11263 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 | 11264 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 | 11265 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 | 11266 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 11267 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 | 11268 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 | 11269 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 | 11270 | `	}` |
|       70 | 11271 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       70 | 11272 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|       36 | 11273 | `	}else{` |
|      ! 0 | 11274 | `		ph7_result_null(pCtx);` |
|        - | 11275 | `	}` |
|       70 | 11276 | `	return PH7_OK;` |
|       36 | 11277 |  |
|        - | 11278 | `/*` |
|        - | 11279 | ` * Generator::next() — advance to the next yield point.` |
|        - | 11280 | ` */` |
|      112 | 11281 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 11282 |  |
|        - | 11283 | `	ph7_generator *pGen;` |
|        - | 11284 | `	sxi32 rc;` |
|      114 | 11285 | `	if( nArg < 1 ) return PH7_OK;` |
|      114 | 11286 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      114 | 11287 | `	if( pGen == 0 ) return PH7_OK;` |
|      114 | 11288 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 11289 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      114 | 11290 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|      114 | 11291 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       58 | 11292 | `	}else{` |
|      ! 0 | 11293 | `		return PH7_OK;` |
|        - | 11294 | `	}` |
|      114 | 11295 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      114 | 11296 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      114 | 11297 | `	return PH7_OK;` |
|       58 | 11298 |  |
|        - | 11299 | `/*` |
|        - | 11300 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - | 11301 | ` */` |
|        4 | 11302 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 11303 |  |
|        - | 11304 | `	ph7_generator *pGen;` |
|        - | 11305 | `	ph7_value *pSendVal;` |
|        - | 11306 | `	sxi32 rc;` |
|        5 | 11307 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 | 11308 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 | 11309 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 | 11310 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 | 11311 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - | 11312 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 | 11313 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 | 11314 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 | 11315 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 | 11316 | `	}else{` |
|      ! 0 | 11317 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11318 | `		return PH7_OK;` |
|        - | 11319 | `	}` |
|        5 | 11320 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 | 11321 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 | 11322 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 | 11323 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 | 11324 | `	}else{` |
|        3 | 11325 | `		ph7_result_null(pCtx);` |
|        - | 11326 | `	}` |
|        5 | 11327 | `	return PH7_OK;` |
|        3 | 11328 |  |
|        - | 11329 | `/*` |
|        - | 11330 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - | 11331 | ` *` |
|        - | 11332 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - | 11333 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - | 11334 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - | 11335 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - | 11336 | ` * the exception to the caller.` |
|        - | 11337 | ` */` |
|      ! 0 | 11338 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 11339 |  |
|        - | 11340 | `	ph7_generator *pGen;` |
|        - | 11341 | `	const char *zMsg;` |
|        - | 11342 | `	int nLen;` |
|      ! 0 | 11343 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 | 11344 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 11345 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 | 11346 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 | 11347 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 | 11348 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - | 11349 | `			"Cannot throw into a closed generator");` |
|        - | 11350 | `	}` |
|        - | 11351 | `	/* Close the generator. Re-throw the exception properly via` |
|        - | 11352 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - | 11353 | `	 * exception dispatch path works correctly. Extract the message` |
|        - | 11354 | `	 * from the passed exception object if possible. */` |
|      ! 0 | 11355 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 11356 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 | 11357 | `	nLen = 0;` |
|      ! 0 | 11358 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - | 11359 | `		/* Try to get the exception's message */` |
|        - | 11360 | `		SyString sAttr;` |
|        - | 11361 | `		ph7_value *pMsgAttr;` |
|      ! 0 | 11362 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 | 11363 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 | 11364 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 | 11365 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 | 11366 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 | 11367 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 | 11368 | `		}` |
|      ! 0 | 11369 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 11370 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 | 11371 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 | 11372 | `	}` |
|      ! 0 | 11373 | `	(void)nLen;` |
|      ! 0 | 11374 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 | 11375 |  |
|        - | 11376 | `/*` |
|        - | 11377 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - | 11378 | ` */` |
|        2 | 11379 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 11380 |  |
|        - | 11381 | `	ph7_generator *pGen;` |
|        3 | 11382 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 | 11383 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 | 11384 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 | 11385 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 | 11386 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - | 11387 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - | 11388 | `	}` |
|        3 | 11389 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 | 11390 | `	return PH7_OK;` |
|        2 | 11391 |  |
|        - | 11392 | `/*` |
|        - | 11393 | ` * Generator::__destruct() — clean up.` |
|        - | 11394 | ` */` |
|      ! 0 | 11395 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 11396 |  |
|        - | 11397 | `	ph7_generator *pGen;` |
|      ! 0 | 11398 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 | 11399 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 11400 | `	if( pGen ){` |
|      ! 0 | 11401 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 | 11402 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 11403 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 11404 | `			SyString sAttrName;` |
|        - | 11405 | `			ph7_value *pAttr;` |
|      ! 0 | 11406 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 | 11407 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 11408 | `			if( pAttr ){` |
|      ! 0 | 11409 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 | 11410 | `			}` |
|      ! 0 | 11411 | `		}` |
|      ! 0 | 11412 | `	}` |
|      ! 0 | 11413 | `	return PH7_OK;` |
|      ! 0 | 11414 |  |
|        - | 11415 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - | 11416 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - | 11417 | `/*` |
|        - | 11418 | ` * Invoke the installed VM output consumer callback to consume` |
|        - | 11419 | ` * the desired message.` |
|        - | 11420 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - | 11421 | ` * in 'api.c' for additional information.` |
|        - | 11422 | ` */` |
|      370 | 11423 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - | 11424 | `	ph7_vm *pVm,      /* Target VM */` |
|        - | 11425 | `	SyString *pString /* Message to output */` |
|        - | 11426 | `	)` |
|        2 | 11427 |  |
|      372 | 11428 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      372 | 11429 | `	sxi32 rc = SXRET_OK;` |
|        - | 11430 | `	/* Call the output consumer */` |
|      372 | 11431 | `	if( pString->nByte > 0 ){` |
|      372 | 11432 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      372 | 11433 | `		VmTrackOutput(pVm, pString->nByte);` |
|      185 | 11434 | `	}` |
|      372 | 11435 | `	return rc;` |
|        2 | 11436 |  |
|        - | 11437 | `/*` |
|        - | 11438 | ` * Format a message and invoke the installed VM output consumer` |
|        - | 11439 | ` * callback to consume the formatted message.` |
|        - | 11440 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - | 11441 | ` * in 'api.c' for additional information.` |
|        - | 11442 | ` */` |
|        2 | 11443 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - | 11444 | `	ph7_vm *pVm,         /* Target VM */` |
|        - | 11445 | `	const char *zFormat, /* Formatted message to output */` |
|        - | 11446 | `	va_list ap           /* Variable list of arguments */` |
|        - | 11447 | `	)` |
|        1 | 11448 |  |
|        3 | 11449 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 | 11450 | `	sxi32 rc = SXRET_OK;` |
|        - | 11451 | `	SyBlob sWorker;` |
|        - | 11452 | `	/* Format the message and call the output consumer */` |
|        3 | 11453 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 | 11454 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 | 11455 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - | 11456 | `		/* Consume the formatted message */` |
|        3 | 11457 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 | 11458 | `	}` |
|        3 | 11459 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - | 11460 | `	/* Release the working buffer */` |
|        3 | 11461 | `	SyBlobRelease(&sWorker);` |
|        3 | 11462 | `	return rc;` |
|        1 | 11463 |  |
|        - | 11464 | `/*` |
|        - | 11465 | ` * Return a string representation of the given PH7 OP code.` |
|        - | 11466 | ` * This function never fail and always return a pointer` |
|        - | 11467 | ` * to a null terminated string.` |
|        - | 11468 | ` */` |
|       12 | 11469 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 | 11470 |  |
|       13 | 11471 | `	const char *zOp = "Unknown     ";` |
|       13 | 11472 | `	switch(nOp){` |
|        3 | 11473 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 | 11474 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 | 11475 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 | 11476 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 | 11477 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 | 11478 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 | 11479 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 | 11480 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 | 11481 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 | 11482 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 | 11483 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 | 11484 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 | 11485 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 | 11486 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 | 11487 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 | 11488 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 | 11489 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 | 11490 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 | 11491 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 | 11492 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 | 11493 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 | 11494 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 | 11495 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 | 11496 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 | 11497 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 | 11498 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 | 11499 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 | 11500 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 | 11501 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 | 11502 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 | 11503 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 | 11504 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 | 11505 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 | 11506 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 | 11507 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 | 11508 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 | 11509 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 | 11510 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 | 11511 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 | 11512 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 | 11513 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 | 11514 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 | 11515 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 | 11516 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 | 11517 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 | 11518 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 | 11519 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 | 11520 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 | 11521 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 | 11522 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 | 11523 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 | 11524 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 | 11525 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|      ! 0 | 11526 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 | 11527 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 | 11528 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 | 11529 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|      ! 0 | 11530 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|      ! 0 | 11531 | `	case PH7_OP_NULLSAFE_JMP:zOp = "NULLSAFE_JMP"; break;` |
|      ! 0 | 11532 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 | 11533 | `	case PH7_OP_FLAG_SPREAD:zOp = "FLAG_SPREAD"; break;` |
|      ! 0 | 11534 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 | 11535 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 | 11536 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 | 11537 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 | 11538 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 | 11539 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 | 11540 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 | 11541 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 | 11542 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 | 11543 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 | 11544 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 | 11545 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 | 11546 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 | 11547 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 | 11548 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 | 11549 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 | 11550 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 | 11551 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 | 11552 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 | 11553 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 | 11554 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 | 11555 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 | 11556 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 | 11557 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 | 11558 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 | 11559 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 | 11560 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 | 11561 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 | 11562 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 | 11563 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 | 11564 | `	case PH7_OP_MATCH:      zOp = "MATCH      "; break;` |
|      ! 0 | 11565 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 | 11566 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 | 11567 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 | 11568 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 | 11569 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 | 11570 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 | 11571 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 | 11572 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 | 11573 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 | 11574 | `	default:` |
|      ! 0 | 11575 | `		break;` |
|        - | 11576 | `	}` |
|       13 | 11577 | `	return zOp;` |
|        1 | 11578 |  |
|        - | 11579 | `/*` |
|        - | 11580 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - | 11581 | ` * The xConsumer() callback which is an used defined function` |
|        - | 11582 | ` * is responsible of consuming the generated dump.` |
|        - | 11583 | ` */` |
|        2 | 11584 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - | 11585 | `	ph7_vm *pVm,            /* Target VM */` |
|        - | 11586 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - | 11587 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - | 11588 | `	)` |
|        1 | 11589 |  |
|        - | 11590 | `	sxi32 rc;` |
|        3 | 11591 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 | 11592 | `	return rc;` |
|        1 | 11593 |  |
|        - | 11594 | `/*` |
|        - | 11595 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - | 11596 | ` * outside a class body [i.e: global or function scope].` |
|        - | 11597 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - | 11598 | ` * in 'compile.c' for additional information.` |
|        - | 11599 | ` */` |
|       14 | 11600 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 | 11601 |  |
|       15 | 11602 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - | 11603 | `	/* Evaluate and expand constant value */` |
|       15 | 11604 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|       15 | 11605 |  |
|        - | 11606 | `/*` |
|        - | 11607 | ` * Section:` |
|        - | 11608 | ` *  Function handling functions.` |
|        - | 11609 | ` * Status:` |
|        - | 11610 | ` *    Stable.` |
|        - | 11611 | ` */` |
|        - | 11612 | `/*` |
|        - | 11613 | ` * int func_num_args(void)` |
|        - | 11614 | ` *   Returns the number of arguments passed to the function.` |
|        - | 11615 | ` * Parameters` |
|        - | 11616 | ` *   None.` |
|        - | 11617 | ` * Return` |
|        - | 11618 | ` *  Total number of arguments passed into the current user-defined function` |
|        - | 11619 | ` *  or -1 if called from the globe scope.` |
|        - | 11620 | ` */` |
|      986 | 11621 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11622 |  |
|        - | 11623 | `	VmFrame *pFrame;` |
|        - | 11624 | `	ph7_vm *pVm;` |
|        - | 11625 | `	/* Point to the target VM */` |
|      988 | 11626 | `	pVm = pCtx->pVm;` |
|        - | 11627 | `	/* Current frame */` |
|      988 | 11628 | `	pFrame = pVm->pFrame;` |
|      988 | 11629 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      988 | 11630 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 | 11631 | `		SXUNUSED(nArg);` |
|      ! 0 | 11632 | `		SXUNUSED(apArg);` |
|        - | 11633 | `		/* Global frame,return -1 */` |
|      ! 0 | 11634 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 | 11635 | `		return SXRET_OK;` |
|        - | 11636 | `	}` |
|        - | 11637 | `	/* Total number of arguments passed to the enclosing function */` |
|      988 | 11638 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      988 | 11639 | `	ph7_result_int(pCtx,nArg);` |
|      988 | 11640 | `	return SXRET_OK;` |
|      495 | 11641 |  |
|        - | 11642 | `/*` |
|        - | 11643 | ` * value func_get_arg(int $arg_num)` |
|        - | 11644 | ` *   Return an item from the argument list.` |
|        - | 11645 | ` * Parameters` |
|        - | 11646 | ` *  Argument number(index start from zero).` |
|        - | 11647 | ` * Return` |
|        - | 11648 | ` *  Returns the specified argument or FALSE on error.` |
|        - | 11649 | ` */` |
|       22 | 11650 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11651 |  |
|       24 | 11652 | `	ph7_value *pObj = 0;` |
|       24 | 11653 | `	VmSlot *pSlot = 0;` |
|        - | 11654 | `	VmFrame *pFrame;` |
|        - | 11655 | `	ph7_vm *pVm;` |
|        - | 11656 | `	/* Point to the target VM */` |
|       24 | 11657 | `	pVm = pCtx->pVm;` |
|        - | 11658 | `	/* Current frame */` |
|       24 | 11659 | `	pFrame = pVm->pFrame;` |
|       24 | 11660 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 | 11661 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - | 11662 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 | 11663 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 | 11664 | `		ph7_result_bool(pCtx,0);` |
|        3 | 11665 | `		return SXRET_OK;` |
|        - | 11666 | `	}` |
|        - | 11667 | `	/* Extract the desired index */` |
|       21 | 11668 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 | 11669 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - | 11670 | `		/* Invalid index,return FALSE */` |
|      ! 0 | 11671 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11672 | `		return SXRET_OK;` |
|        - | 11673 | `	}` |
|        - | 11674 | `	/* Extract the desired argument */` |
|       21 | 11675 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 | 11676 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - | 11677 | `			/* Return the desired argument */` |
|       21 | 11678 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 | 11679 | `		}else{` |
|        - | 11680 | `			/* No such argument,return false */` |
|      ! 0 | 11681 | `			ph7_result_bool(pCtx,0);` |
|        - | 11682 | `		}` |
|       11 | 11683 | `	}else{` |
|        - | 11684 | `		/* CAN'T HAPPEN */` |
|      ! 0 | 11685 | `		ph7_result_bool(pCtx,0);` |
|        - | 11686 | `	}` |
|       21 | 11687 | `	return SXRET_OK;` |
|       13 | 11688 |  |
|        - | 11689 | `/*` |
|        - | 11690 | ` * array func_get_args_byref(void)` |
|        - | 11691 | ` *   Returns an array comprising a function's argument list.` |
|        - | 11692 | ` * Parameters` |
|        - | 11693 | ` *  None.` |
|        - | 11694 | ` * Return` |
|        - | 11695 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - | 11696 | ` *  member of the current user-defined function's argument list.` |
|        - | 11697 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 11698 | ` * NOTE:` |
|        - | 11699 | ` *  Arguments are returned to the array by reference.` |
|        - | 11700 | ` */` |
|        2 | 11701 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11702 |  |
|        - | 11703 | `	ph7_value *pArray;` |
|        - | 11704 | `	VmFrame *pFrame;` |
|        - | 11705 | `	VmSlot *aSlot;` |
|        - | 11706 | `	sxu32 n;` |
|        - | 11707 | `	/* Point to the current frame */` |
|        3 | 11708 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 | 11709 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 | 11710 | `	if( pFrame->pParent == 0 ){` |
|        - | 11711 | `		/* Global frame,return FALSE */` |
|      ! 0 | 11712 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 11713 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11714 | `		return SXRET_OK;` |
|        - | 11715 | `	}` |
|        - | 11716 | `	/* Create a new array */` |
|        3 | 11717 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11718 | `	if( pArray == 0 ){` |
|      ! 0 | 11719 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11720 | `		SXUNUSED(apArg);` |
|      ! 0 | 11721 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11722 | `		return SXRET_OK;` |
|        - | 11723 | `	}` |
|        - | 11724 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 | 11725 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 | 11726 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 | 11727 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 | 11728 | `	}` |
|        - | 11729 | `	/* Return the freshly created array */` |
|        3 | 11730 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11731 | `	return SXRET_OK;` |
|        2 | 11732 |  |
|        - | 11733 | `/*` |
|        - | 11734 | ` * array func_get_args(void)` |
|        - | 11735 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - | 11736 | ` * Parameters` |
|        - | 11737 | ` *  None.` |
|        - | 11738 | ` * Return` |
|        - | 11739 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - | 11740 | ` *  member of the current user-defined function's argument list.` |
|        - | 11741 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 11742 | ` */` |
|       88 | 11743 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11744 |  |
|       90 | 11745 | `	ph7_value *pObj = 0;` |
|        - | 11746 | `	ph7_value *pArray;` |
|        - | 11747 | `	VmFrame *pFrame;` |
|        - | 11748 | `	VmSlot *aSlot;` |
|        - | 11749 | `	sxu32 n;` |
|        - | 11750 | `	/* Point to the current frame */` |
|       90 | 11751 | `	pFrame = pCtx->pVm->pFrame;` |
|       90 | 11752 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       90 | 11753 | `	if( pFrame->pParent == 0 ){` |
|        - | 11754 | `		/* Global frame,return FALSE */` |
|      ! 0 | 11755 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 11756 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11757 | `		return SXRET_OK;` |
|        - | 11758 | `	}` |
|        - | 11759 | `	/* Create a new array */` |
|       90 | 11760 | `	pArray = ph7_context_new_array(pCtx);` |
|       90 | 11761 | `	if( pArray == 0 ){` |
|      ! 0 | 11762 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11763 | `		SXUNUSED(apArg);` |
|      ! 0 | 11764 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11765 | `		return SXRET_OK;` |
|        - | 11766 | `	}` |
|        - | 11767 | `	/* Start filling the array with the given arguments */` |
|       90 | 11768 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      222 | 11769 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 | 11770 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 | 11771 | `		if( pObj ){` |
|      134 | 11772 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 | 11773 | `		}` |
|       68 | 11774 | `	}` |
|        - | 11775 | `	/* Return the freshly created array */` |
|       90 | 11776 | `	ph7_result_value(pCtx,pArray);` |
|       90 | 11777 | `	return SXRET_OK;` |
|       46 | 11778 |  |
|        - | 11779 | `/*` |
|        - | 11780 | ` * bool function_exists(string $name)` |
|        - | 11781 | ` *  Return TRUE if the given function has been defined.` |
|        - | 11782 | ` * Parameters` |
|        - | 11783 | ` *  The name of the desired function.` |
|        - | 11784 | ` * Return` |
|        - | 11785 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - | 11786 | ` */` |
|     1748 | 11787 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11788 |  |
|        - | 11789 | `	const char *zName;` |
|        - | 11790 | `	ph7_vm *pVm;` |
|        - | 11791 | `	int nLen;` |
|        - | 11792 | `	int res;` |
|     1750 | 11793 | `	if( nArg < 1 ){` |
|        - | 11794 | `		/* Missing argument,return FALSE */` |
|      ! 0 | 11795 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11796 | `		return SXRET_OK;` |
|        - | 11797 | `	}` |
|        - | 11798 | `	/* Point to the target VM */` |
|     1750 | 11799 | `	pVm = pCtx->pVm;` |
|        - | 11800 | `	/* Extract the function name */` |
|     1750 | 11801 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 11802 | `	/* Assume the function is not defined */` |
|     1750 | 11803 | `	res = 0;` |
|        - | 11804 | `	/* Perform the lookup */` |
|     2622 | 11805 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1744 | 11806 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 11807 | `			/* Function is defined */` |
|      268 | 11808 | `			res = 1;` |
|      133 | 11809 | `	}` |
|     1750 | 11810 | `	ph7_result_bool(pCtx,res);` |
|     1750 | 11811 | `	return SXRET_OK;` |
|      876 | 11812 |  |
|        - | 11813 | `/*` |
|        - | 11814 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 11815 | ` * [i.e: Whether it is callable or not].` |
|        - | 11816 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - | 11817 | ` */` |
|    24086 | 11818 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 | 11819 |  |
|    24088 | 11820 | `	int res = 0;` |
|    24088 | 11821 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 11822 | `		/* PHP semantics: an object is callable iff its class declares __invoke` |
|        - | 11823 | `		 * (inherited methods count). The CallInvoke flag is unused — it` |
|        - | 11824 | `		 * formerly invoked __invoke as a runtime predicate, which is not` |
|        - | 11825 | `		 * standard PHP behavior. */` |
|       20 | 11826 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|       20 | 11827 | `		if( PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1) ){` |
|       18 | 11828 | `			res = 1;` |
|       10 | 11829 | `		}` |
|        9 | 11830 | `		(void)CallInvoke;` |
|    24079 | 11831 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 | 11832 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 | 11833 | `		if( pMap->nEntry == 2 ){` |
|        - | 11834 | `			ph7_class *pClass;` |
|        - | 11835 | `			ph7_value *pV;` |
|        - | 11836 | `			/* Extract the target class */` |
|       12 | 11837 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 | 11838 | `			if( pV ){` |
|       12 | 11839 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 | 11840 | `				if( pClass ){` |
|        - | 11841 | `					ph7_class_method *pMethod;` |
|        - | 11842 | `					/* Extract the target method */` |
|       10 | 11843 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 | 11844 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - | 11845 | `						/* Perform the lookup */` |
|       10 | 11846 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 | 11847 | `						if( pMethod ){` |
|        - | 11848 | `							/* Method is callable */` |
|        5 | 11849 | `							res = 1;` |
|        2 | 11850 | `						}` |
|        4 | 11851 | `					}` |
|        4 | 11852 | `				}` |
|        5 | 11853 | `			}` |
|        7 | 11854 | `		}` |
|    24057 | 11855 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - | 11856 | `		const char *zName;` |
|        - | 11857 | `		int nLen;` |
|        - | 11858 | `		/* Extract the name */` |
|     5914 | 11859 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - | 11860 | `		/* Perform the lookup */` |
|     5929 | 11861 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 | 11862 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 11863 | `				/* Function is callable */` |
|     5896 | 11864 | `				res = 1;` |
|     2947 | 11865 | `		}` |
|     2956 | 11866 | `	}` |
|    24088 | 11867 | `	return res;` |
|        2 | 11868 |  |
|        - | 11869 | `/*` |
|        - | 11870 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - | 11871 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 11872 | ` * Parameters` |
|        - | 11873 | ` * $name` |
|        - | 11874 | ` *    The callback function to check` |
|        - | 11875 | ` * $syntax_only` |
|        - | 11876 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - | 11877 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - | 11878 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - | 11879 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - | 11880 | ` *    a string.` |
|        - | 11881 | ` * Return` |
|        - | 11882 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - | 11883 | ` */` |
|       20 | 11884 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11885 |  |
|        - | 11886 | `	ph7_vm *pVm;` |
|        - | 11887 | `	int res;` |
|       21 | 11888 | `	if( nArg < 1 ){` |
|        - | 11889 | `		/* Missing arguments,return FALSE */` |
|      ! 0 | 11890 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11891 | `		return SXRET_OK;` |
|        - | 11892 | `	}` |
|        - | 11893 | `	/* Point to the target VM */` |
|       21 | 11894 | `	pVm = pCtx->pVm;` |
|        - | 11895 | `	/* Perform the requested operation */` |
|       21 | 11896 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       21 | 11897 | `	ph7_result_bool(pCtx,res);` |
|       21 | 11898 | `	return SXRET_OK;` |
|       11 | 11899 |  |
|        - | 11900 | `/*` |
|        - | 11901 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - | 11902 | ` * defined below.` |
|        - | 11903 | ` */` |
|     1312 | 11904 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 11905 |  |
|     1313 | 11906 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 11907 | `	ph7_value sName;` |
|        - | 11908 | `	sxi32 rc;` |
|        - | 11909 | `	/* Prepare the function name for insertion */` |
|     1313 | 11910 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1313 | 11911 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 11912 | `	/* Perform the insertion */` |
|     1313 | 11913 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1313 | 11914 | `	PH7_MemObjRelease(&sName);` |
|     1313 | 11915 | `	return rc;` |
|        1 | 11916 |  |
|        - | 11917 | `/*` |
|        - | 11918 | ` * array get_defined_functions(void)` |
|        - | 11919 | ` *  Returns an array of all defined functions.` |
|        - | 11920 | ` * Parameter` |
|        - | 11921 | ` *  None.` |
|        - | 11922 | ` * Return` |
|        - | 11923 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - | 11924 | ` *  both built-in (internal) and user-defined.` |
|        - | 11925 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - | 11926 | ` *  defined ones using $arr["user"].` |
|        - | 11927 | ` * Note:` |
|        - | 11928 | ` *  NULL is returned on failure.` |
|        - | 11929 | ` */` |
|        2 | 11930 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11931 |  |
|        - | 11932 | `	ph7_value *pArray,*pEntry;` |
|        - | 11933 | `	/* NOTE:` |
|        - | 11934 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - | 11935 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - | 11936 | `	 */` |
|        3 | 11937 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11938 | ` 	if( pArray == 0 ){` |
|      ! 0 | 11939 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11940 | `		SXUNUSED(apArg);` |
|        - | 11941 | `		/* Return NULL */` |
|      ! 0 | 11942 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11943 | `		return SXRET_OK;` |
|        - | 11944 | `	}` |
|        3 | 11945 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 11946 | `	if( pEntry == 0 ){` |
|        - | 11947 | `		/* Return NULL */` |
|      ! 0 | 11948 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11949 | `		return SXRET_OK;` |
|        - | 11950 | `	}` |
|        - | 11951 | `	/* Fill with the appropriate information */` |
|        3 | 11952 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - | 11953 | `	/* Create the 'internal' index */` |
|        3 | 11954 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - | 11955 | `	/* Create the user-func array */` |
|        3 | 11956 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 11957 | `	if( pEntry == 0 ){` |
|        - | 11958 | `		/* Return NULL */` |
|      ! 0 | 11959 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11960 | `		return SXRET_OK;` |
|        - | 11961 | `	}` |
|        - | 11962 | `	/* Fill with the appropriate information */` |
|        3 | 11963 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - | 11964 | `	/* Create the 'user' index */` |
|        3 | 11965 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - | 11966 | `	/* Return the multi-dimensional array */` |
|        3 | 11967 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11968 | `	return SXRET_OK;` |
|        2 | 11969 |  |
|        - | 11970 | `/*` |
|        - | 11971 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - | 11972 | ` *  Register a function for execution on shutdown.` |
|        - | 11973 | ` * Note` |
|        - | 11974 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - | 11975 | ` *  be called in the same order as they were registered.` |
|        - | 11976 | ` * Parameters` |
|        - | 11977 | ` *  $callback` |
|        - | 11978 | ` *   The shutdown callback to register.` |
|        - | 11979 | ` * $param` |
|        - | 11980 | ` *  One or more Parameter to pass to the registered callback.` |
|        - | 11981 | ` * Return` |
|        - | 11982 | ` *  Nothing.` |
|        - | 11983 | ` */` |
|       12 | 11984 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11985 |  |
|        - | 11986 | `	VmShutdownCB sEntry;` |
|        - | 11987 | `	int i,j;` |
|       14 | 11988 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 11989 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 | 11990 | `		return PH7_OK;` |
|        - | 11991 | `	}` |
|        - | 11992 | `	/* Zero the Entry */` |
|       14 | 11993 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - | 11994 | `	/* Initialize fields */` |
|       14 | 11995 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - | 11996 | `	/* Save the callback name for later invocation name */` |
|       14 | 11997 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|      134 | 11998 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|      122 | 11999 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       62 | 12000 | `	}` |
|        - | 12001 | `	/* Copy arguments */` |
|       14 | 12002 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 | 12003 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - | 12004 | `			/* Limit reached */` |
|      ! 0 | 12005 | `			break;` |
|        - | 12006 | `		}` |
|      ! 0 | 12007 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 | 12008 | `	}` |
|       14 | 12009 | `	sEntry.nArg = j;` |
|        - | 12010 | `	/* Install the callback */` |
|       14 | 12011 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|       14 | 12012 | `	return PH7_OK;` |
|        8 | 12013 |  |
|        - | 12014 | `/*` |
|        - | 12015 | ` * Section:` |
|        - | 12016 | ` *  Class handling functions.` |
|        - | 12017 | ` * Status:` |
|        - | 12018 | ` *    Stable.` |
|        - | 12019 | ` */` |
|        - | 12020 | `/*` |
|        - | 12021 | ` * Extract the top active class. NULL is returned` |
|        - | 12022 | ` * if the class stack is empty.` |
|        - | 12023 | ` */` |
|     1006 | 12024 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 | 12025 |  |
|     1008 | 12026 | `	SySet *pSet = &pVm->aSelf;` |
|        - | 12027 | `	ph7_class **apClass;` |
|     1008 | 12028 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - | 12029 | `		/* Empty stack,return NULL */` |
|       15 | 12030 | `		return 0;` |
|        - | 12031 | `	}` |
|        - | 12032 | `	/* Peek the last entry */` |
|      994 | 12033 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      994 | 12034 | `	return apClass[pSet->nUsed - 1];` |
|      505 | 12035 |  |
|        - | 12036 | `/*` |
|        - | 12037 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - | 12038 | ` *   Get the class that declared the currently executing method.` |
|        - | 12039 | ` *   This is used for resolving the 'self::' constant.` |
|        - | 12040 | ` *` |
|        - | 12041 | ` * Parameters` |
|        - | 12042 | ` *   pVm: Target VM` |
|        - | 12043 | ` *` |
|        - | 12044 | ` * Return` |
|        - | 12045 | ` *   The declaring class of the current method, or NULL if:` |
|        - | 12046 | ` *   - Not executing within a class method` |
|        - | 12047 | ` *` |
|        - | 12048 | ` * Note` |
|        - | 12049 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - | 12050 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - | 12051 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - | 12052 | ` *   This is found by walking the call frames to locate the method's` |
|        - | 12053 | ` *   declaring class.` |
|        - | 12054 | ` */` |
|       98 | 12055 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 | 12056 |  |
|      100 | 12057 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 12058 | `	ph7_vm_func *pVmFunc;` |
|        - | 12059 |  |
|        - | 12060 | `	/* Skip exception frames to find the actual method frame */` |
|      100 | 12061 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - | 12062 |  |
|        - | 12063 | `	/* Check if we're in a method context */` |
|      100 | 12064 | `	if( pFrame->pParent ){` |
|       96 | 12065 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       96 | 12066 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - | 12067 | `			/* Return the declaring class */` |
|       96 | 12068 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - | 12069 | `		}` |
|      ! 0 | 12070 | `	}` |
|        - | 12071 |  |
|        5 | 12072 | `	return 0;` |
|       51 | 12073 |  |
|        - | 12074 |  |
|        - | 12075 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - | 12076 | `/*` |
|        - | 12077 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - | 12078 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - | 12079 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - | 12080 | ` * return value indicates failure.` |
|        - | 12081 | ` */` |
|        - | 12082 | `/*` |
|        - | 12083 | ` * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap` |
|        - | 12084 | ` * through to the synthetic CALL instruction.  Used by the NEW handler so` |
|        - | 12085 | ` * that constructor calls with named arguments reach the named-arg path` |
|        - | 12086 | ` * (with variadic string-key packing) rather than the positional path.` |
|        - | 12087 | ` */` |
|     2826 | 12088 | `static sxi32 VmCallClassMethodWithMap(` |
|        - | 12089 | `	ph7_vm *pVm,` |
|        - | 12090 | `	ph7_class_instance *pThis,` |
|        - | 12091 | `	ph7_class_method *pMethod,` |
|        - | 12092 | `	ph7_value *pResult,` |
|        - | 12093 | `	int nArg,` |
|        - | 12094 | `	ph7_value **apArg,` |
|        - | 12095 | `	VmCallArgMap *pMap` |
|        - | 12096 | `	)` |
|        2 | 12097 |  |
|        - | 12098 | `	ph7_value *aStack;` |
|        - | 12099 | `	VmInstr aInstr[2];` |
|        - | 12100 | `	int iCursor;` |
|        - | 12101 | `	int i;` |
|        - | 12102 | `	sxi32 rc;` |
|     2828 | 12103 | `	aStack = VmNewOperandStack(&(*pVm),2+nArg);` |
|     2828 | 12104 | `	if( aStack == 0 ){` |
|      ! 0 | 12105 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 12106 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 | 12107 | `		return SXERR_MEM;` |
|        - | 12108 | `	}` |
|     4392 | 12109 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1566 | 12110 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|     1566 | 12111 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      784 | 12112 | `	}` |
|     2828 | 12113 | `	iCursor = nArg + 1;` |
|     2828 | 12114 | `	if( pThis ){` |
|     2822 | 12115 | `		pThis->iRef++;` |
|     2822 | 12116 | `		aStack[i].x.pOther = pThis;` |
|     2822 | 12117 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|     1410 | 12118 | `	}` |
|     2828 | 12119 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     2828 | 12120 | `	i++;` |
|     2828 | 12121 | `	SyBlobReset(&aStack[i].sBlob);` |
|     2828 | 12122 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     2828 | 12123 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     2828 | 12124 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     2828 | 12125 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     2828 | 12126 | `	aInstr[0].iP1 = nArg;` |
|     2828 | 12127 | `	aInstr[0].iP2 = 0;` |
|     2828 | 12128 | `	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */` |
|     2828 | 12129 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     2828 | 12130 | `	aInstr[1].iP1 = 1;` |
|     2828 | 12131 | `	aInstr[1].iP2 = 0;` |
|     2828 | 12132 | `	aInstr[1].p3  = 0;` |
|     2828 | 12133 | `	rc = VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0,0);` |
|     2828 | 12134 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|        - | 12135 | `	/* Propagate the real exec status (PH7_EXCEPTION / PH7_ABORT) so callers` |
|        - | 12136 | `	 * can unwind instead of continuing past a method that raised. */` |
|     2828 | 12137 | `	return rc;` |
|     1415 | 12138 |  |
|     2264 | 12139 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - | 12140 | `	ph7_vm *pVm,               /* Target VM */` |
|        - | 12141 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - | 12142 | `	ph7_class_method *pMethod, /* Method name */` |
|        - | 12143 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - | 12144 | `	int nArg,                  /* Total number of given arguments */` |
|        - | 12145 | `	ph7_value **apArg          /* Method arguments */` |
|        - | 12146 | `	)` |
|        2 | 12147 |  |
|     2266 | 12148 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);` |
|        2 | 12149 |  |
|        - | 12150 | `/*` |
|        - | 12151 | ` * Helper for PH7_VmIteratorWalk: call a zero-arg Iterator method by name,` |
|        - | 12152 | ` * returning its result. Returns the exec status so a method that throws` |
|        - | 12153 | ` * (PH7_EXCEPTION) or aborts (PH7_ABORT) is propagated — unlike the foreach` |
|        - | 12154 | ` * opcode, which discards it.` |
|        - | 12155 | ` */` |
|      324 | 12156 | `static sxi32 VmIterCallMethod(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nLen,ph7_value *pResult)` |
|        1 | 12157 |  |
|      325 | 12158 | `	ph7_class_method *pMethod = PH7_ClassExtractMethod(pThis->pClass,zName,nLen);` |
|      325 | 12159 | `	if( pMethod == 0 ){` |
|      ! 0 | 12160 | `		return SXRET_OK; /* missing method: treat as no-op (mirrors foreach leniency) */` |
|        - | 12161 | `	}` |
|      325 | 12162 | `	return PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,0,0);` |
|      163 | 12163 |  |
|        - | 12164 | `/*` |
|        - | 12165 | ` * Walk a Traversable (Iterator / IteratorAggregate / Generator), invoking xStep` |
|        - | 12166 | ` * for each (key,value) pair. This is the reusable form of the Iterator protocol` |
|        - | 12167 | ` * that the foreach opcode drives inline; it is consumed by iterator_to_array /` |
|        - | 12168 | ` * iterator_count / iterator_apply and by Traversable spread.` |
|        - | 12169 | ` *` |
|        - | 12170 | ` * Returns:` |
|        - | 12171 | ` *   SXRET_OK            walk completed (or xStep stopped early via SXERR_EOF)` |
|        - | 12172 | ` *   SXERR_NOTIMPLEMENTED pObj is not a Traversable (caller raises a TypeError)` |
|        - | 12173 | ` *   PH7_EXCEPTION       an iterator method or the step threw` |
|        - | 12174 | ` *   PH7_ABORT           an iterator method or the step requested a VM halt` |
|        - | 12175 | ` *` |
|        - | 12176 | ` * pKey/pValue handed to xStep are owned by the walk (released after the step` |
|        - | 12177 | ` * returns); xStep must copy what it needs.` |
|        - | 12178 | ` */` |
|       28 | 12179 | `PH7_PRIVATE sxi32 PH7_VmIteratorWalk(ph7_vm *pVm,ph7_value *pObj,ProcIterStep xStep,void *pUserData)` |
|        1 | 12180 |  |
|        - | 12181 | `	ph7_class_instance *pThis;        /* the live Iterator (after aggregate resolution) */` |
|       29 | 12182 | `	ph7_class_instance *pAggregate = 0;` |
|        - | 12183 | `	ph7_class *pIteratorClass;` |
|       29 | 12184 | `	sxi32 rc = SXRET_OK;` |
|       29 | 12185 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 \|\| pObj->x.pOther == 0 ){` |
|      ! 0 | 12186 | `		return SXERR_NOTIMPLEMENTED;` |
|        - | 12187 | `	}` |
|       29 | 12188 | `	pThis = (ph7_class_instance *)pObj->x.pOther;` |
|       29 | 12189 | `	pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       29 | 12190 | `	if( pIteratorClass == 0 ){` |
|      ! 0 | 12191 | `		return SXERR_NOTIMPLEMENTED;` |
|        - | 12192 | `	}` |
|       29 | 12193 | `	if( PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|       27 | 12194 | `		pThis->iRef++; /* keep the iterator alive across the walk */` |
|       14 | 12195 | `	}else{` |
|        - | 12196 | `		/* Maybe an IteratorAggregate: resolve its inner Iterator via getIterator() */` |
|        3 | 12197 | `		ph7_class *pAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",sizeof("IteratorAggregate")-1,FALSE,0);` |
|        - | 12198 | `		ph7_value sInner;` |
|        3 | 12199 | `		int bOk = 0;` |
|        3 | 12200 | `		if( pAggClass == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pAggClass) ){` |
|      ! 0 | 12201 | `			return SXERR_NOTIMPLEMENTED; /* not Traversable at all */` |
|        - | 12202 | `		}` |
|        3 | 12203 | `		PH7_MemObjInit(&(*pVm),&sInner);` |
|        3 | 12204 | `		rc = VmIterCallMethod(pVm,pThis,"getIterator",sizeof("getIterator")-1,&sInner);` |
|        3 | 12205 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|      ! 0 | 12206 | `			PH7_MemObjRelease(&sInner);` |
|      ! 0 | 12207 | `			return rc;` |
|        - | 12208 | `		}` |
|        3 | 12209 | `		if( (sInner.iFlags & MEMOBJ_OBJ) && sInner.x.pOther ){` |
|        3 | 12210 | `			ph7_class_instance *pIter = (ph7_class_instance *)sInner.x.pOther;` |
|        3 | 12211 | `			if( PH7_VmInstanceOf(pIter->pClass,pIteratorClass) ){` |
|        3 | 12212 | `				pAggregate = pThis; pAggregate->iRef++; /* keep the aggregate alive */` |
|        3 | 12213 | `				pThis = pIter; pThis->iRef++;           /* survive release of sInner */` |
|        3 | 12214 | `				bOk = 1;` |
|        1 | 12215 | `			}` |
|        1 | 12216 | `		}` |
|        3 | 12217 | `		PH7_MemObjRelease(&sInner);` |
|        3 | 12218 | `		if( !bOk ){` |
|        - | 12219 | `			/* getIterator() returned a non-Iterator: surface as not-a-Traversable */` |
|      ! 0 | 12220 | `			return SXERR_NOTIMPLEMENTED;` |
|        - | 12221 | `		}` |
|        - | 12222 | `	}` |
|        - | 12223 | `	/* Drive rewind / valid / current / key / step / next */` |
|       29 | 12224 | `	rc = VmIterCallMethod(pVm,pThis,"rewind",sizeof("rewind")-1,0);` |
|       29 | 12225 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ goto done; }` |
|       78 | 12226 | `	for(;;){` |
|        - | 12227 | `		ph7_value sValid,sValue,sKey;` |
|        - | 12228 | `		int isValid;` |
|       93 | 12229 | `		PH7_MemObjInit(&(*pVm),&sValid);` |
|       93 | 12230 | `		rc = VmIterCallMethod(pVm,pThis,"valid",sizeof("valid")-1,&sValid);` |
|       96 | 12231 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValid); goto done; }` |
|       93 | 12232 | `		PH7_MemObjToBool(&sValid);` |
|       93 | 12233 | `		isValid = (sValid.x.iVal != 0);` |
|       93 | 12234 | `		PH7_MemObjRelease(&sValid);` |
|       93 | 12235 | `		if( !isValid ){ rc = SXRET_OK; break; }` |
|       71 | 12236 | `		PH7_MemObjInit(&(*pVm),&sValue);` |
|       71 | 12237 | `		rc = VmIterCallMethod(pVm,pThis,"current",sizeof("current")-1,&sValue);` |
|       71 | 12238 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValue); goto done; }` |
|       69 | 12239 | `		PH7_MemObjInit(&(*pVm),&sKey);` |
|       69 | 12240 | `		rc = VmIterCallMethod(pVm,pThis,"key",sizeof("key")-1,&sKey);` |
|       69 | 12241 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValue); PH7_MemObjRelease(&sKey); goto done; }` |
|       69 | 12242 | `		rc = xStep(&(*pVm),&sKey,&sValue,pUserData);` |
|       69 | 12243 | `		PH7_MemObjRelease(&sValue);` |
|       69 | 12244 | `		PH7_MemObjRelease(&sKey);` |
|       69 | 12245 | `		if( rc != SXRET_OK ){` |
|        5 | 12246 | `			if( rc == SXERR_EOF ){ rc = SXRET_OK; } /* early stop is success */` |
|        5 | 12247 | `			goto done;` |
|        - | 12248 | `		}` |
|       65 | 12249 | `		rc = VmIterCallMethod(pVm,pThis,"next",sizeof("next")-1,0);` |
|       65 | 12250 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ goto done; }` |
|       12 | 12251 | `	}` |
|       14 | 12252 | `done:` |
|       29 | 12253 | `	PH7_ClassInstanceUnref(pThis);` |
|       29 | 12254 | `	if( pAggregate ){ PH7_ClassInstanceUnref(pAggregate); }` |
|       29 | 12255 | `	return rc;` |
|       15 | 12256 |  |
|        - | 12257 | `/*` |
|        - | 12258 | ` * Dispatch a call to an object's __invoke magic method, forwarding arguments` |
|        - | 12259 | ` * and the return value. Used by the PH7_OP_CALL object-callable branch and by` |
|        - | 12260 | ` * PH7_VmCallUserFunction so that $obj(...), call_user_func($obj, ...) and` |
|        - | 12261 | ` * call_user_func_array($obj, [...]) all reach __invoke uniformly.` |
|        - | 12262 | ` *` |
|        - | 12263 | ` * Visibility is intentionally not checked: PHP allows private/protected` |
|        - | 12264 | ` * __invoke to be invoked via $obj() from any scope, and PHL's existing` |
|        - | 12265 | ` * is_callable / closure-invoke paths follow the same rule.` |
|        - | 12266 | ` *` |
|        - | 12267 | ` * pMap forwards the call-site VmCallArgMap so named-argument resolution and` |
|        - | 12268 | ` * strict_types coercion work for $obj(...) the same way they do for normal` |
|        - | 12269 | ` * function calls. Pass 0 from C-API call sites (call_user_func and friends),` |
|        - | 12270 | ` * which receive arguments positionally and don't carry a strict-types context.` |
|        - | 12271 | ` *` |
|        - | 12272 | ` * Returns SXRET_OK on success, SXERR_INVALID if __invoke is missing.` |
|        - | 12273 | ` */` |
|      174 | 12274 | `static sxi32 VmCallObjectInvoke(` |
|        - | 12275 | `	ph7_vm *pVm,` |
|        - | 12276 | `	ph7_class_instance *pThis,` |
|        - | 12277 | `	int nArg,` |
|        - | 12278 | `	ph7_value **apArg,` |
|        - | 12279 | `	ph7_value *pResult,` |
|        - | 12280 | `	VmCallArgMap *pMap` |
|        - | 12281 | `	)` |
|        2 | 12282 |  |
|        - | 12283 | `	ph7_class_method *pMethod;` |
|      176 | 12284 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      176 | 12285 | `	if( pMethod == 0 ){` |
|       13 | 12286 | `		if( pResult ){` |
|       13 | 12287 | `			PH7_MemObjRelease(pResult);` |
|        6 | 12288 | `		}` |
|       13 | 12289 | `		return SXERR_INVALID;` |
|        - | 12290 | `	}` |
|      164 | 12291 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|       89 | 12292 |  |
|        - | 12293 | `/*` |
|        - | 12294 | ` * Raise a catchable Error("Object of type X is not callable") when an object` |
|        - | 12295 | ` * is invoked as a function but lacks __invoke. Mirrors the OP_THROW pattern` |
|        - | 12296 | ` * (vm.c PH7_OP_THROW): build the Error instance, mark the current frame as` |
|        - | 12297 | ` * throwing, dispatch via VmThrowException so the nearest try/catch can handle` |
|        - | 12298 | ` * it. Caller is responsible for the post-throw control flow (iExceptionJump` |
|        - | 12299 | ` * lookup or 'goto Exception').` |
|        - | 12300 | ` *` |
|        - | 12301 | ` * Returns the result of VmThrowException (SXRET_OK on handled exception,` |
|        - | 12302 | ` * SXERR_ABORT on abort), or SXERR_ABORT if the Error class itself cannot` |
|        - | 12303 | ` * be bootstrapped — in which case an uncaught fatal has already been` |
|        - | 12304 | ` * reported.` |
|        - | 12305 | ` */` |
|       12 | 12306 | `static sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis)` |
|        1 | 12307 |  |
|        - | 12308 | `	ph7_class *pErrorClass;` |
|       13 | 12309 | `	ph7_class_instance *pErrInst = 0;` |
|        - | 12310 | `	ph7_class_method *pCons;` |
|        - | 12311 | `	VmFrame *pThrowFrame;` |
|        - | 12312 | `	char zMsg[256];` |
|        - | 12313 | `	int nMsg;` |
|        - | 12314 | `	sxi32 rc;` |
|       25 | 12315 | `	nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - | 12316 | `		"Object of type %.*s is not callable",` |
|       12 | 12317 | `		(int)pThis->pClass->sName.nByte,` |
|       12 | 12318 | `		pThis->pClass->sName.zString);` |
|       13 | 12319 | `	pErrorClass = PH7_VmExtractClass(pVm,"Error",sizeof("Error")-1,TRUE,0);` |
|       13 | 12320 | `	if( pErrorClass ){` |
|       13 | 12321 | `		pErrInst = PH7_NewClassInstance(pVm,pErrorClass);` |
|        6 | 12322 | `	}` |
|       13 | 12323 | `	if( pErrInst == 0 ){` |
|        - | 12324 | `		/* Bootstrap failure: Error class is part of the built-in library and` |
|        - | 12325 | `		 * should always be available, so this branch is effectively unreachable.` |
|        - | 12326 | `		 * Degrade to an uncaught fatal report so the failure is at least` |
|        - | 12327 | `		 * visible to the user. */` |
|      ! 0 | 12328 | `		VmReportUncaughtException(pVm,"Error",5,zMsg,(sxu32)nMsg,0,0);` |
|      ! 0 | 12329 | `		return SXERR_ABORT;` |
|        - | 12330 | `	}` |
|       13 | 12331 | `	pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|       13 | 12332 | `	if( pCons ){` |
|        - | 12333 | `		ph7_value sArg;` |
|        - | 12334 | `		ph7_value *apMsg[1];` |
|        - | 12335 | `		SyString sMsgStr;` |
|       13 | 12336 | `		SyStringInitFromBuf(&sMsgStr,zMsg,(sxu32)nMsg);` |
|       13 | 12337 | `		PH7_MemObjInit(pVm,&sArg);` |
|       13 | 12338 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|       13 | 12339 | `		apMsg[0] = &sArg;` |
|       13 | 12340 | `		PH7_VmCallClassMethod(pVm,pErrInst,pCons,0,1,apMsg);` |
|       13 | 12341 | `		PH7_MemObjRelease(&sArg);` |
|        6 | 12342 | `	}` |
|        - | 12343 | `	/* Else: Error::__construct is part of the built-in library and should` |
|        - | 12344 | `	 * always be present; if it isn't, the thrown exception still surfaces` |
|        - | 12345 | `	 * with an empty getMessage() rather than crashing. */` |
|       13 | 12346 | `	pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|       13 | 12347 | `	if( pThrowFrame ){` |
|       13 | 12348 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|        6 | 12349 | `	}` |
|       13 | 12350 | `	rc = VmThrowException(pVm,pErrInst);` |
|       13 | 12351 | `	PH7_ClassInstanceUnref(pErrInst);` |
|       13 | 12352 | `	return rc;` |
|        7 | 12353 |  |
|        - | 12354 | `/*` |
|        - | 12355 | ` * Call a user defined or foreign function where the name of the function` |
|        - | 12356 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - | 12357 | ` * in the apArg[] array.` |
|        - | 12358 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 12359 | ` * return value indicates failure.` |
|        - | 12360 | ` */` |
|     1238 | 12361 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - | 12362 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 12363 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 12364 | `	int nArg,          /* Total number of given arguments */` |
|        - | 12365 | `	ph7_value **apArg, /* Callback arguments */` |
|        - | 12366 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - | 12367 | `	)` |
|        2 | 12368 |  |
|        - | 12369 | `	ph7_value *aStack;` |
|        - | 12370 | `	VmInstr aInstr[2];` |
|        - | 12371 | `	int i;` |
|     1240 | 12372 | `	if( pFunc->iFlags & MEMOBJ_OBJ ){` |
|        - | 12373 | `		/* Object callable: dispatch through __invoke when available.` |
|        - | 12374 | `		 * No VmCallArgMap: call_user_func / array_map / usort and the C API` |
|        - | 12375 | `		 * pass arguments positionally and inherit no strict-types context. */` |
|      140 | 12376 | `		return VmCallObjectInvoke(&(*pVm),` |
|       92 | 12377 | `			(ph7_class_instance *)pFunc->x.pOther,` |
|       46 | 12378 | `			nArg,apArg,pResult,0);` |
|        - | 12379 | `	}` |
|     1148 | 12380 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 12381 | `		/* Don't bother processing,it's invalid anyway */` |
|      511 | 12382 | `		if( pResult ){` |
|        - | 12383 | `			/* Assume a null return value */` |
|      ! 0 | 12384 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 12385 | `		}` |
|      511 | 12386 | `		return SXERR_INVALID;` |
|        - | 12387 | `	}` |
|      638 | 12388 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 12389 | `		/* Class method */` |
|       15 | 12390 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       15 | 12391 | `		ph7_class_method *pMethod = 0;` |
|       15 | 12392 | `		ph7_class_instance *pThis = 0;` |
|       15 | 12393 | `		ph7_class *pClass = 0;` |
|        - | 12394 | `		ph7_value *pValue;` |
|        - | 12395 | `		sxi32 rc;` |
|       15 | 12396 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - | 12397 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 | 12398 | `			if( pResult ){` |
|        - | 12399 | `				/* Assume a null return value */` |
|      ! 0 | 12400 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 12401 | `			}` |
|      ! 0 | 12402 | `			return SXRET_OK;` |
|        - | 12403 | `		}` |
|        - | 12404 | `		/* Extract the class name or an instance of it */` |
|       15 | 12405 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       15 | 12406 | `		if( pValue ){` |
|       15 | 12407 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        7 | 12408 | `		}` |
|       15 | 12409 | `		if( pClass == 0 ){` |
|        - | 12410 | `			/* No such class,return NULL */` |
|      ! 0 | 12411 | `			if( pResult ){` |
|      ! 0 | 12412 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 12413 | `			}` |
|      ! 0 | 12414 | `			return SXRET_OK;` |
|        - | 12415 | `		}` |
|       15 | 12416 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 12417 | `			/* Point to the class instance */` |
|        9 | 12418 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        4 | 12419 | `		}` |
|        - | 12420 | `		/* Try to extract the method */` |
|       15 | 12421 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       15 | 12422 | `		if( pValue ){` |
|       15 | 12423 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       22 | 12424 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        7 | 12425 | `					SyBlobLength(&pValue->sBlob));` |
|        7 | 12426 | `			}` |
|        7 | 12427 | `		}` |
|       15 | 12428 | `		if( pMethod == 0 ){` |
|        - | 12429 | `			/* No such method,return NULL */` |
|      ! 0 | 12430 | `			if( pResult ){` |
|      ! 0 | 12431 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 12432 | `			}` |
|      ! 0 | 12433 | `			return SXRET_OK;` |
|        - | 12434 | `		}` |
|        - | 12435 | `		/* Call the class method */` |
|       15 | 12436 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       15 | 12437 | `		return rc;` |
|        - | 12438 | `	}` |
|        - | 12439 | `	/* Create a new operand stack */` |
|      624 | 12440 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      624 | 12441 | `	if( aStack == 0 ){` |
|      ! 0 | 12442 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 12443 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 | 12444 | `		if( pResult ){` |
|        - | 12445 | `			/* Assume a null return value */` |
|      ! 0 | 12446 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 12447 | `		}` |
|      ! 0 | 12448 | `		return SXERR_MEM;` |
|        - | 12449 | `	}` |
|        - | 12450 | `	/* Fill the operand stack with the given arguments */` |
|     1934 | 12451 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1312 | 12452 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - | 12453 | `		/*` |
|        - | 12454 | `		 * Symisc eXtension:` |
|        - | 12455 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - | 12456 | `		 */` |
|     1312 | 12457 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      657 | 12458 | `	}` |
|        - | 12459 | `	/* Push the function name */` |
|      624 | 12460 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      624 | 12461 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - | 12462 | `	/* Emit the CALL istruction */` |
|      624 | 12463 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      624 | 12464 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      624 | 12465 | `	aInstr[0].iP2 = 0;` |
|      624 | 12466 | `	aInstr[0].p3  = 0;` |
|        - | 12467 | `	/* Emit the DONE instruction */` |
|      624 | 12468 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      624 | 12469 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      624 | 12470 | `	aInstr[1].iP2 = 0;` |
|      624 | 12471 | `	aInstr[1].p3  = 0;` |
|        - | 12472 | `	/* Execute the function body (if available) */` |
|        - | 12473 | `	{` |
|        - | 12474 | `		sxi32 rcExec;` |
|      624 | 12475 | `		rcExec = VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0,0);` |
|        - | 12476 | `		/* Clean up the mess left behind */` |
|      624 | 12477 | `		SyMemBackendFree(&pVm->sAllocator,aStack);` |
|        - | 12478 | `		/* Propagate PH7_EXCEPTION/PH7_ABORT so a callback that raised unwinds. */` |
|      624 | 12479 | `		return rcExec;` |
|        - | 12480 | `	}` |
|      621 | 12481 |  |
|        - | 12482 | `/*` |
|        - | 12483 | ` * Call a user defined or foreign function whith a varibale number` |
|        - | 12484 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - | 12485 | ` * parameter.` |
|        - | 12486 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 12487 | ` * return value indicates failure.` |
|        - | 12488 | ` */` |
|      240 | 12489 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - | 12490 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 12491 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 12492 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - | 12493 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - | 12494 | `	)` |
|        1 | 12495 |  |
|        - | 12496 | `	ph7_value *pArg;` |
|        - | 12497 | `	SySet aArg;` |
|        - | 12498 | `	va_list ap;` |
|        - | 12499 | `	sxi32 rc;` |
|      241 | 12500 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - | 12501 | `	/* Copy arguments one after one */` |
|      241 | 12502 | `	va_start(ap,pResult);` |
|      399 | 12503 | `	for(;;){` |
|      799 | 12504 | `		pArg = va_arg(ap,ph7_value *);` |
|      799 | 12505 | `		if( pArg == 0 ){` |
|      241 | 12506 | `			break;` |
|        - | 12507 | `		}` |
|      559 | 12508 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 | 12509 | `	}` |
|        - | 12510 | `	/* Call the core routine */` |
|      241 | 12511 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - | 12512 | `	/* Cleanup */` |
|      241 | 12513 | `	SySetRelease(&aArg);` |
|      241 | 12514 | `	return rc;` |
|        1 | 12515 |  |
|        - | 12516 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - | 12517 | `/*` |
|        - | 12518 | ` * bool defined(string $name)` |
|        - | 12519 | ` *  Checks whether a given named constant exists.` |
|        - | 12520 | ` * Parameter:` |
|        - | 12521 | ` *  Name of the desired constant.` |
|        - | 12522 | ` * Return` |
|        - | 12523 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - | 12524 | ` */` |
|       26 | 12525 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12526 |  |
|        - | 12527 | `	const char *zName;` |
|       28 | 12528 | `	int nLen = 0;` |
|       28 | 12529 | `	int res = 0;` |
|       28 | 12530 | `	if( nArg < 1 ){` |
|        - | 12531 | `		/* Missing constant name,return FALSE */` |
|      ! 0 | 12532 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 | 12533 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12534 | `		return SXRET_OK;` |
|        - | 12535 | `	}` |
|        - | 12536 | `	/* Extract constant name */` |
|       28 | 12537 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 12538 | `	/* Perform the lookup */` |
|       28 | 12539 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 12540 | `		/* Already defined */` |
|       26 | 12541 | `		res = 1;` |
|       12 | 12542 | `	}` |
|       28 | 12543 | `	ph7_result_bool(pCtx,res);` |
|       28 | 12544 | `	return SXRET_OK;` |
|       15 | 12545 |  |
|        - | 12546 | `/*` |
|        - | 12547 | ` * Constant expansion callback used by the [define()] function defined` |
|        - | 12548 | ` * below.` |
|        - | 12549 | ` */` |
|       16 | 12550 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 | 12551 |  |
|       18 | 12552 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - | 12553 | `	/* Expand constant value */` |
|       18 | 12554 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       18 | 12555 |  |
|        - | 12556 | `/*` |
|        - | 12557 | ` * bool define(string $constant_name,expression value)` |
|        - | 12558 | ` *  Defines a named constant at runtime.` |
|        - | 12559 | ` * Parameter:` |
|        - | 12560 | ` *  $constant_name` |
|        - | 12561 | ` *   The name of the constant` |
|        - | 12562 | ` *  $value` |
|        - | 12563 | ` *   Constant value` |
|        - | 12564 | ` * Return:` |
|        - | 12565 | ` *   TRUE on success,FALSE on failure.` |
|        - | 12566 | ` */` |
|       14 | 12567 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12568 |  |
|        - | 12569 | `	const char *zName;  /* Constant name */` |
|        - | 12570 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       16 | 12571 | `	int nLen = 0;       /* Name length */` |
|        - | 12572 | `	sxi32 rc;` |
|       16 | 12573 | `	if( nArg < 2 ){` |
|        - | 12574 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 | 12575 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 | 12576 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12577 | `		return SXRET_OK;` |
|        - | 12578 | `	}` |
|       16 | 12579 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 | 12580 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 | 12581 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12582 | `		return SXRET_OK;` |
|        - | 12583 | `	}` |
|        - | 12584 | `	/* Extract constant name */` |
|       16 | 12585 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       16 | 12586 | `	if( nLen < 1 ){` |
|      ! 0 | 12587 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 | 12588 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12589 | `		return SXRET_OK;` |
|        - | 12590 | `	}` |
|        - | 12591 | `	/* Duplicate constant value */` |
|       16 | 12592 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       16 | 12593 | `	if( pValue == 0 ){` |
|      ! 0 | 12594 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 12595 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12596 | `		return SXRET_OK;` |
|        - | 12597 | `	}` |
|        - | 12598 | `	/* Initialize the memory object */` |
|       16 | 12599 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - | 12600 | `	/* Register the constant */` |
|       16 | 12601 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       16 | 12602 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 12603 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 | 12604 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 12605 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12606 | `		return SXRET_OK;` |
|        - | 12607 | `	}` |
|        - | 12608 | `	/* Duplicate constant value */` |
|       16 | 12609 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       16 | 12610 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - | 12611 | `		/* Lower case the constant name */` |
|      ! 0 | 12612 | `		char *zCur = (char *)zName;` |
|      ! 0 | 12613 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 | 12614 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - | 12615 | `				/* UTF-8 stream */` |
|      ! 0 | 12616 | `				zCur++;` |
|      ! 0 | 12617 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 | 12618 | `					zCur++;` |
|      ! 0 | 12619 | `				}` |
|      ! 0 | 12620 | `				continue;` |
|        - | 12621 | `			}` |
|      ! 0 | 12622 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 | 12623 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 | 12624 | `				zCur[0] = (char)c;` |
|      ! 0 | 12625 | `			}` |
|      ! 0 | 12626 | `			zCur++;` |
|      ! 0 | 12627 | `		}` |
|        - | 12628 | `		/* Register the lowercase alias with its OWN value copy (not the same` |
|        - | 12629 | `		 * pValue) so the two entries don't share one object — otherwise freeing` |
|        - | 12630 | `		 * one on a later overwrite would dangle the other. */` |
|        - | 12631 | `		{` |
|      ! 0 | 12632 | `			ph7_value *pAlias = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|      ! 0 | 12633 | `			if( pAlias ){` |
|      ! 0 | 12634 | `				PH7_MemObjInit(pCtx->pVm,pAlias);` |
|      ! 0 | 12635 | `				PH7_MemObjStore(apArg[1],pAlias);` |
|      ! 0 | 12636 | `				ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pAlias);` |
|      ! 0 | 12637 | `			}` |
|        - | 12638 | `		}` |
|      ! 0 | 12639 | `	}` |
|        - | 12640 | `	/* All done,return TRUE */` |
|       16 | 12641 | `	ph7_result_bool(pCtx,1);` |
|       16 | 12642 | `	return SXRET_OK;` |
|        9 | 12643 |  |
|        - | 12644 | `/*` |
|        - | 12645 | ` * value constant(string $name)` |
|        - | 12646 | ` *  Returns the value of a constant` |
|        - | 12647 | ` * Parameter` |
|        - | 12648 | ` *  $name` |
|        - | 12649 | ` *    Name of the constant.` |
|        - | 12650 | ` * Return` |
|        - | 12651 | ` *  Constant value or NULL if not defined.` |
|        - | 12652 | ` */` |
|        8 | 12653 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12654 |  |
|        - | 12655 | `	SyHashEntry *pEntry;` |
|        - | 12656 | `	ph7_constant *pCons;` |
|        - | 12657 | `	const char *zName; /* Constant name */` |
|        - | 12658 | `	ph7_value sVal;    /* Constant value */` |
|        - | 12659 | `	int nLen;` |
|       10 | 12660 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 12661 | `		/* Invallid argument,return NULL */` |
|      ! 0 | 12662 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 | 12663 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12664 | `		return SXRET_OK;` |
|        - | 12665 | `	}` |
|        - | 12666 | `	/* Extract the constant name */` |
|       10 | 12667 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 12668 | `	/* Perform the query */` |
|       10 | 12669 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 | 12670 | `	if( pEntry == 0 ){` |
|        3 | 12671 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 | 12672 | `		ph7_result_null(pCtx);` |
|        3 | 12673 | `		return SXRET_OK;` |
|        - | 12674 | `	}` |
|        8 | 12675 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - | 12676 | `	/* Point to the structure that describe the constant */` |
|        8 | 12677 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - | 12678 | `	/* Extract constant value by calling it's associated callback */` |
|        8 | 12679 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - | 12680 | `	/* Return that value */` |
|        8 | 12681 | `	ph7_result_value(pCtx,&sVal);` |
|        - | 12682 | `	/* Cleanup */` |
|        8 | 12683 | `	PH7_MemObjRelease(&sVal);` |
|        8 | 12684 | `	return SXRET_OK;` |
|        6 | 12685 |  |
|        - | 12686 | `/*` |
|        - | 12687 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - | 12688 | ` * defined below.` |
|        - | 12689 | ` */` |
|      466 | 12690 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 12691 |  |
|      467 | 12692 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 12693 | `	ph7_value sName;` |
|        - | 12694 | `	sxi32 rc;` |
|        - | 12695 | `	/* Prepare the constant name for insertion */` |
|      467 | 12696 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      467 | 12697 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 12698 | `	/* Perform the insertion */` |
|      467 | 12699 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      467 | 12700 | `	PH7_MemObjRelease(&sName);` |
|      467 | 12701 | `	return rc;` |
|        1 | 12702 |  |
|        - | 12703 | `/*` |
|        - | 12704 | ` * array get_defined_constants(void)` |
|        - | 12705 | ` *  Returns an associative array with the names of all defined` |
|        - | 12706 | ` *  constants.` |
|        - | 12707 | ` * Parameters` |
|        - | 12708 | ` *  NONE.` |
|        - | 12709 | ` * Returns` |
|        - | 12710 | ` *  Returns the names of all the constants currently defined.` |
|        - | 12711 | ` */` |
|        2 | 12712 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12713 |  |
|        - | 12714 | `	ph7_value *pArray;` |
|        - | 12715 | `	/* Create the array first*/` |
|        3 | 12716 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 12717 | `	if( pArray == 0 ){` |
|      ! 0 | 12718 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12719 | `		SXUNUSED(apArg);` |
|        - | 12720 | `		/* Return NULL */` |
|      ! 0 | 12721 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12722 | `		return SXRET_OK;` |
|        - | 12723 | `	}` |
|        - | 12724 | `	/* Fill the array with the defined constants */` |
|        3 | 12725 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - | 12726 | `	/* Return the created array */` |
|        3 | 12727 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 12728 | `	return SXRET_OK;` |
|        2 | 12729 |  |
|        - | 12730 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - | 12731 | `/*` |
|        - | 12732 | ` * Section:` |
|        - | 12733 | ` *  Random numbers/string generators.` |
|        - | 12734 | ` * Status:` |
|        - | 12735 | ` *    Stable.` |
|        - | 12736 | ` */` |
|        - | 12737 | `/*` |
|        - | 12738 | ` * Generate a random 32-bit unsigned integer.` |
|        - | 12739 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - | 12740 | ` * implemented in src/sx/sxrand.c).` |
|        - | 12741 | ` */` |
|     2916 | 12742 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 | 12743 |  |
|        - | 12744 | `	sxu32 iNum;` |
|     2918 | 12745 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2918 | 12746 | `	return iNum;` |
|        2 | 12747 |  |
|        - | 12748 | `/*` |
|        - | 12749 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - | 12750 | ` * Note that the generated string is NOT null terminated.` |
|        - | 12751 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - | 12752 | ` * implemented in src/sx/sxrand.c).` |
|        - | 12753 | ` */` |
|   237106 | 12754 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 | 12755 |  |
|        - | 12756 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - | 12757 | `	int i;` |
|        - | 12758 | `	/* Generate a binary string first */` |
|   237108 | 12759 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - | 12760 | `	/* Turn the binary string into english based alphabet */` |
|  2608336 | 12761 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  2371230 | 12762 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|  1185616 | 12763 | `	 }` |
|   237108 | 12764 |  |
|        - | 12765 | `/*` |
|        - | 12766 | ` * int rand()` |
|        - | 12767 | ` * int mt_rand()` |
|        - | 12768 | ` * int rand(int $min,int $max)` |
|        - | 12769 | ` * int mt_rand(int $min,int $max)` |
|        - | 12770 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - | 12771 | ` * Parameter` |
|        - | 12772 | ` *  $min` |
|        - | 12773 | ` *    The lowest value to return (default: 0)` |
|        - | 12774 | ` *  $max` |
|        - | 12775 | ` *   The highest value to return (default: getrandmax())` |
|        - | 12776 | ` * Return` |
|        - | 12777 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - | 12778 | ` * Note:` |
|        - | 12779 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 12780 | ` *  by te SQLite3 library.` |
|        - | 12781 | ` */` |
|       20 | 12782 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12783 |  |
|        - | 12784 | `	sxu32 iNum;` |
|        - | 12785 | `	/* Generate the random number */` |
|       21 | 12786 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 | 12787 | `	if( nArg > 1 ){` |
|        - | 12788 | `		sxu32 iMin,iMax;` |
|        3 | 12789 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 | 12790 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 | 12791 | `		if( iMin < iMax ){` |
|        3 | 12792 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 | 12793 | `			if( iDiv > 0 ){` |
|        3 | 12794 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 | 12795 | `			}` |
|        1 | 12796 | `		}else if(iMax > 0 ){` |
|      ! 0 | 12797 | `			iNum %= iMax;` |
|      ! 0 | 12798 | `		}` |
|        1 | 12799 | `	}` |
|        - | 12800 | `	/* Return the number */` |
|       21 | 12801 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 | 12802 | `	return SXRET_OK;` |
|        1 | 12803 |  |
|        - | 12804 | `/*` |
|        - | 12805 | ` * int getrandmax(void)` |
|        - | 12806 | ` * int mt_getrandmax(void)` |
|        - | 12807 | ` * int rc4_getrandmax(void)` |
|        - | 12808 | ` *   Show largest possible random value` |
|        - | 12809 | ` * Return` |
|        - | 12810 | ` *  The largest possible random value returned by rand() which is in` |
|        - | 12811 | ` *  this implementation 0xFFFFFFFF.` |
|        - | 12812 | ` * Note:` |
|        - | 12813 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 12814 | ` *  by te SQLite3 library.` |
|        - | 12815 | ` */` |
|        4 | 12816 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12817 |  |
|        2 | 12818 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 | 12819 | `	SXUNUSED(apArg);` |
|        5 | 12820 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 | 12821 | `	return SXRET_OK;` |
|        1 | 12822 |  |
|        - | 12823 | `/*` |
|        - | 12824 | ` * string rand_str()` |
|        - | 12825 | ` * string rand_str(int $len)` |
|        - | 12826 | ` *  Generate a random string (English alphabet).` |
|        - | 12827 | ` * Parameter` |
|        - | 12828 | ` *  $len` |
|        - | 12829 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - | 12830 | ` * Return` |
|        - | 12831 | ` *   A pseudo random string.` |
|        - | 12832 | ` * Note:` |
|        - | 12833 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 12834 | ` *  by te SQLite3 library.` |
|        - | 12835 | ` *  This function is a symisc extension.` |
|        - | 12836 | ` */` |
|      120 | 12837 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12838 |  |
|        - | 12839 | `	char zString[1024];` |
|      122 | 12840 | `	int iLen = 0x10;` |
|      122 | 12841 | `	if( nArg > 0 ){` |
|        - | 12842 | `		/* Get the desired length */` |
|      122 | 12843 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 | 12844 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - | 12845 | `			/* Default length */` |
|        3 | 12846 | `			iLen = 0x10;` |
|        1 | 12847 | `		}` |
|       60 | 12848 | `	}` |
|        - | 12849 | `	/* Generate the random string */` |
|      122 | 12850 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - | 12851 | `	/* Return the generated string */` |
|      122 | 12852 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 | 12853 | `	return SXRET_OK;` |
|        2 | 12854 |  |
|        - | 12855 | `/*` |
|        - | 12856 | ` * Reject non-numeric values (array/object/resource and non-numeric strings)` |
|        - | 12857 | ` * the same way intdiv() does. Returns SXRET_OK if the value is acceptable as` |
|        - | 12858 | ` * an int (PHP coerces float and numeric string silently).` |
|        - | 12859 | ` */` |
|      488 | 12860 | `static int VmRandomCheckIntArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgPos,const char *zParamName)` |
|        1 | 12861 |  |
|      488 | 12862 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg)` |
|      483 | 12863 | `		\|\| ph7_value_is_resource(pArg) ){` |
|       10 | 12864 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12865 | `			"TypeError",` |
|        - | 12866 | `			"%s(): Argument #%d (%s) must be of type int, %s given",` |
|        3 | 12867 | `			zFunc,iArgPos,zParamName,` |
|        3 | 12868 | `			ph7_type_name(pArg)` |
|        - | 12869 | `			);` |
|        - | 12870 | `	}` |
|      483 | 12871 | `	if( ph7_value_is_string(pArg) ){` |
|        - | 12872 | `		int len;` |
|        9 | 12873 | `		const char *zStr = ph7_value_to_string(pArg, &len);` |
|        9 | 12874 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|        7 | 12875 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12876 | `				"TypeError",` |
|        - | 12877 | `				"%s(): Argument #%d (%s) must be of type int, string given",` |
|        2 | 12878 | `				zFunc,iArgPos,zParamName` |
|        - | 12879 | `				);` |
|        - | 12880 | `		}` |
|        2 | 12881 | `	}` |
|      479 | 12882 | `	return SXRET_OK;` |
|      245 | 12883 |  |
|        - | 12884 | `/*` |
|        - | 12885 | ` * int random_int(int $min, int $max)` |
|        - | 12886 | ` *  Generate a cryptographically secure pseudo-random integer in [$min, $max].` |
|        - | 12887 | ` *  Mirrors PHP 7.0+ random_int(). Uses the OS CSPRNG via SyOSCSPRNG().` |
|        - | 12888 | ` *  Distribution is uniform via rejection sampling against the smallest` |
|        - | 12889 | ` *  power-of-two mask covering the range.` |
|        - | 12890 | ` */` |
|      242 | 12891 | `static int vm_builtin_random_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12892 |  |
|        - | 12893 | `	sxi64 iMin,iMax;` |
|        - | 12894 | `	sxu64 uRange,uMask,uResult;` |
|        - | 12895 | `	unsigned int nAttempt;` |
|        - | 12896 | `	int rc;` |
|      243 | 12897 | `	if( nArg != 2 ){` |
|       10 | 12898 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12899 | `			"ArgumentCountError",` |
|        - | 12900 | `			"random_int() expects exactly 2 arguments, %d given",` |
|        3 | 12901 | `			nArg` |
|        - | 12902 | `			);` |
|        - | 12903 | `	}` |
|      237 | 12904 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_int",1,"$min");` |
|      237 | 12905 | `	if( rc != SXRET_OK ){ return rc; }` |
|      233 | 12906 | `	rc = VmRandomCheckIntArg(pCtx,apArg[1],"random_int",2,"$max");` |
|      233 | 12907 | `	if( rc != SXRET_OK ){ return rc; }` |
|      231 | 12908 | `	iMin = ph7_value_to_int64(apArg[0]);` |
|      231 | 12909 | `	iMax = ph7_value_to_int64(apArg[1]);` |
|      231 | 12910 | `	if( iMin > iMax ){` |
|        3 | 12911 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12912 | `			"ValueError",` |
|        - | 12913 | `			"random_int(): Argument #1 ($min) must be less than or equal to argument #2 ($max)"` |
|        - | 12914 | `			);` |
|        - | 12915 | `	}` |
|      229 | 12916 | `	if( iMin == iMax ){` |
|        5 | 12917 | `		ph7_result_int64(pCtx,iMin);` |
|        5 | 12918 | `		return SXRET_OK;` |
|        - | 12919 | `	}` |
|      225 | 12920 | `	uRange = (sxu64)iMax - (sxu64)iMin;` |
|      225 | 12921 | `	uMask = uRange;` |
|      225 | 12922 | `	uMask \|= uMask >> 1;` |
|      225 | 12923 | `	uMask \|= uMask >> 2;` |
|      225 | 12924 | `	uMask \|= uMask >> 4;` |
|      225 | 12925 | `	uMask \|= uMask >> 8;` |
|      225 | 12926 | `	uMask \|= uMask >> 16;` |
|      225 | 12927 | `	uMask \|= uMask >> 32;` |
|      225 | 12928 | `	uResult = 0;` |
|      349 | 12929 | `	for( nAttempt = 0 ; nAttempt < 50 ; ++nAttempt ){` |
|        - | 12930 | `		/* Always draw a full 8 bytes so endianness of the cast doesn't matter` |
|        - | 12931 | `		 * (a 4-byte fill into a sxu64 would land in the high half on big-endian` |
|        - | 12932 | `		 * and the low-half mask would always read 0). */` |
|        - | 12933 | `		sxu64 uDraw;` |
|      349 | 12934 | `		if( SyOSCSPRNG(&uDraw,sizeof(uDraw)) != SXRET_OK ){` |
|        - | 12935 | `			/* PHP 8.2+ would throw Random\RandomException here; that class` |
|        - | 12936 | `			 * is not yet registered in PHL (see PLAN.md item 6.13). */` |
|      ! 0 | 12937 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12938 | `				"Exception",` |
|        - | 12939 | `				"Cannot gather sufficient random data"` |
|        - | 12940 | `				);` |
|        - | 12941 | `		}` |
|      349 | 12942 | `		uDraw &= uMask;` |
|      349 | 12943 | `		if( uDraw <= uRange ){` |
|      225 | 12944 | `			uResult = uDraw;` |
|      225 | 12945 | `			break;` |
|        - | 12946 | `		}` |
|       58 | 12947 | `	}` |
|      225 | 12948 | `	if( nAttempt >= 50 ){` |
|      ! 0 | 12949 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12950 | `			"Exception",` |
|        - | 12951 | `			"Cannot gather sufficient random data"` |
|        - | 12952 | `			);` |
|        - | 12953 | `	}` |
|      225 | 12954 | `	ph7_result_int64(pCtx,(sxi64)((sxu64)iMin + uResult));` |
|      225 | 12955 | `	return SXRET_OK;` |
|      122 | 12956 |  |
|        - | 12957 | `/*` |
|        - | 12958 | ` * string random_bytes(int $length)` |
|        - | 12959 | ` *  Generate $length cryptographically secure random bytes via SyOSCSPRNG().` |
|        - | 12960 | ` *  Mirrors PHP 7.0+ random_bytes().` |
|        - | 12961 | ` */` |
|       24 | 12962 | `static int vm_builtin_random_bytes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12963 |  |
|        - | 12964 | `	sxi64 iLen;` |
|        - | 12965 | `	unsigned char zStack[256];` |
|        - | 12966 | `	void *pBuf;` |
|        - | 12967 | `	int rc;` |
|       25 | 12968 | `	int bHeap = 0;` |
|       25 | 12969 | `	if( nArg != 1 ){` |
|        7 | 12970 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12971 | `			"ArgumentCountError",` |
|        - | 12972 | `			"random_bytes() expects exactly 1 argument, %d given",` |
|        2 | 12973 | `			nArg` |
|        - | 12974 | `			);` |
|        - | 12975 | `	}` |
|       21 | 12976 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_bytes",1,"$length");` |
|       21 | 12977 | `	if( rc != SXRET_OK ){ return rc; }` |
|       17 | 12978 | `	iLen = ph7_value_to_int64(apArg[0]);` |
|       17 | 12979 | `	if( iLen < 1 ){` |
|        5 | 12980 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12981 | `			"ValueError",` |
|        - | 12982 | `			"random_bytes(): Argument #1 ($length) must be greater than 0"` |
|        - | 12983 | `			);` |
|        - | 12984 | `	}` |
|        - | 12985 | `	/* The PH7 allocator and ph7_result_string both take sxu32/int sizes,` |
|        - | 12986 | `	 * so we can't honor a length above 2 GiB. Reject early rather than` |
|        - | 12987 | `	 * silently truncating via the (sxu32) cast below. */` |
|       13 | 12988 | `	if( iLen > 0x7FFFFFFF ){` |
|      ! 0 | 12989 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12990 | `			"ValueError",` |
|        - | 12991 | `			"random_bytes(): Argument #1 ($length) is too large"` |
|        - | 12992 | `			);` |
|        - | 12993 | `	}` |
|       13 | 12994 | `	if( iLen <= (sxi64)sizeof(zStack) ){` |
|       13 | 12995 | `		pBuf = zStack;` |
|        7 | 12996 | `	}else{` |
|      ! 0 | 12997 | `		pBuf = SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)iLen);` |
|      ! 0 | 12998 | `		if( pBuf == 0 ){` |
|      ! 0 | 12999 | `			return PH7_VmThrowException(pCtx,` |
|        - | 13000 | `				"Exception",` |
|        - | 13001 | `				"random_bytes(): Failed to allocate %qd bytes",` |
|      ! 0 | 13002 | `				iLen` |
|        - | 13003 | `				);` |
|        - | 13004 | `		}` |
|      ! 0 | 13005 | `		bHeap = 1;` |
|        - | 13006 | `	}` |
|       13 | 13007 | `	if( SyOSCSPRNG(pBuf,(sxu32)iLen) != SXRET_OK ){` |
|      ! 0 | 13008 | `		if( bHeap ){` |
|      ! 0 | 13009 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 13010 | `		}` |
|      ! 0 | 13011 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13012 | `			"Exception",` |
|        - | 13013 | `			"Cannot gather sufficient random data"` |
|        - | 13014 | `			);` |
|        - | 13015 | `	}` |
|       13 | 13016 | `	ph7_result_string(pCtx,(const char *)pBuf,(int)iLen);` |
|       13 | 13017 | `	if( bHeap ){` |
|      ! 0 | 13018 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 13019 | `	}` |
|       13 | 13020 | `	return SXRET_OK;` |
|       13 | 13021 |  |
|        - | 13022 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 13023 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 13024 | `/* Unique ID private data */` |
|        - | 13025 | `struct unique_id_data` |
|        - | 13026 |  |
|        - | 13027 | `	ph7_context *pCtx; /* Call context */` |
|        - | 13028 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - | 13029 | `};` |
|        - | 13030 | `/*` |
|        - | 13031 | ` * Binary to hex consumer callback.` |
|        - | 13032 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - | 13033 | ` * defined below.` |
|        - | 13034 | ` */` |
|      192 | 13035 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 | 13036 |  |
|      193 | 13037 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - | 13038 | `	sxu32 nBuflen;` |
|        - | 13039 | `	/* Extract result buffer length */` |
|      193 | 13040 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 | 13041 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - | 13042 | `			/*` |
|        - | 13043 | `			 * If the more_entropy flag is not set,then the returned` |
|        - | 13044 | `			 * string will be 13 characters long` |
|        - | 13045 | `			 */` |
|       25 | 13046 | `		return SXERR_ABORT;` |
|        - | 13047 | `	}` |
|      169 | 13048 | `	if( nBuflen > 22 ){` |
|      ! 0 | 13049 | `		return SXERR_ABORT;` |
|        - | 13050 | `	}` |
|        - | 13051 | `	/* Safely Consume the hex stream */` |
|      169 | 13052 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 | 13053 | `	return SXRET_OK;` |
|       97 | 13054 |  |
|        - | 13055 | `/*` |
|        - | 13056 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - | 13057 | ` *  Generate a unique ID` |
|        - | 13058 | ` * Parameter` |
|        - | 13059 | ` * $prefix` |
|        - | 13060 | ` *  Append this prefix to the generated unique ID.` |
|        - | 13061 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - | 13062 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - | 13063 | ` * $more_entropy` |
|        - | 13064 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - | 13065 | ` *  that the result will be unique.` |
|        - | 13066 | ` * Return` |
|        - | 13067 | ` *  Returns the unique identifier, as a string.` |
|        - | 13068 | ` */` |
|       24 | 13069 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13070 |  |
|        - | 13071 | `	struct unique_id_data sUniq;` |
|        - | 13072 | `	unsigned char zDigest[20];` |
|       25 | 13073 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13074 | `	const char *zPrefix;` |
|        - | 13075 | `	SHA1Context sCtx;` |
|        - | 13076 | `	char zRandom[7];` |
|        - | 13077 | `	int nPrefix;` |
|        - | 13078 | `	int entropy;` |
|        - | 13079 | `	/* Generate a random string first */` |
|       25 | 13080 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - | 13081 | `	/* Initialize fields */` |
|       25 | 13082 | `	zPrefix = 0;` |
|       25 | 13083 | `	nPrefix = 0;` |
|       25 | 13084 | `	entropy = 0;` |
|       25 | 13085 | `	if( nArg > 0 ){` |
|        - | 13086 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 | 13087 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 | 13088 | `		if( nArg > 1 ){` |
|      ! 0 | 13089 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 13090 | `		}` |
|      ! 0 | 13091 | `	}` |
|       25 | 13092 | `	SHA1Init(&sCtx);` |
|        - | 13093 | `	/* Generate the random ID */` |
|       25 | 13094 | `	if( nPrefix > 0 ){` |
|      ! 0 | 13095 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 | 13096 | `	}` |
|        - | 13097 | `	/* Append the random ID */` |
|       25 | 13098 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - | 13099 | `	/* Append the random string */` |
|       25 | 13100 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - | 13101 | `	/* Increment the number */` |
|       25 | 13102 | `	pVm->unique_id++;` |
|       25 | 13103 | `	SHA1Final(&sCtx,zDigest);` |
|        - | 13104 | `	/* Hexify the digest */` |
|       25 | 13105 | `	sUniq.pCtx = pCtx;` |
|       25 | 13106 | `	sUniq.entropy = entropy;` |
|       25 | 13107 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - | 13108 | `	/* All done */` |
|       25 | 13109 | `	return PH7_OK;` |
|        1 | 13110 |  |
|        - | 13111 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 13112 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 13113 | `/*` |
|        - | 13114 | ` * Section:` |
|        - | 13115 | ` *  Language construct implementation as foreign functions.` |
|        - | 13116 | ` * Status:` |
|        - | 13117 | ` *    Stable.` |
|        - | 13118 | ` */` |
|        - | 13119 | `/*` |
|        - | 13120 | ` * void echo($string...)` |
|        - | 13121 | ` *  Output one or more messages.` |
|        - | 13122 | ` * Parameters` |
|        - | 13123 | ` *  $string` |
|        - | 13124 | ` *   Message to output.` |
|        - | 13125 | ` * Return` |
|        - | 13126 | ` *  NULL.` |
|        - | 13127 | ` */` |
|      ! 0 | 13128 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 13129 |  |
|        - | 13130 | `	const char *zData;` |
|      ! 0 | 13131 | `	int nDataLen = 0;` |
|        - | 13132 | `	ph7_vm *pVm;` |
|        - | 13133 | `	int i,rc;` |
|        - | 13134 | `	/* Point to the target VM */` |
|      ! 0 | 13135 | `	pVm = pCtx->pVm;` |
|        - | 13136 | `	/* Output */` |
|      ! 0 | 13137 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 | 13138 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 | 13139 | `		if( nDataLen > 0 ){` |
|      ! 0 | 13140 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 | 13141 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 | 13142 | `			if( rc == SXERR_ABORT ){` |
|        - | 13143 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 13144 | `				return PH7_ABORT;` |
|        - | 13145 | `			}` |
|      ! 0 | 13146 | `		}` |
|      ! 0 | 13147 | `	}` |
|      ! 0 | 13148 | `	return SXRET_OK;` |
|      ! 0 | 13149 |  |
|        - | 13150 | `/*` |
|        - | 13151 | ` * int print($string...)` |
|        - | 13152 | ` *  Output one or more messages.` |
|        - | 13153 | ` * Parameters` |
|        - | 13154 | ` *  $string` |
|        - | 13155 | ` *   Message to output.` |
|        - | 13156 | ` * Return` |
|        - | 13157 | ` *  1 always.` |
|        - | 13158 | ` */` |
|        2 | 13159 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13160 |  |
|        - | 13161 | `	const char *zData;` |
|        3 | 13162 | `	int nDataLen = 0;` |
|        - | 13163 | `	ph7_vm *pVm;` |
|        - | 13164 | `	int i,rc;` |
|        - | 13165 | `	/* Point to the target VM */` |
|        3 | 13166 | `	pVm = pCtx->pVm;` |
|        - | 13167 | `	/* Output */` |
|        5 | 13168 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 | 13169 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 | 13170 | `		if( nDataLen > 0 ){` |
|        3 | 13171 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 | 13172 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 | 13173 | `			if( rc == SXERR_ABORT ){` |
|        - | 13174 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 13175 | `				return PH7_ABORT;` |
|        - | 13176 | `			}` |
|        1 | 13177 | `		}` |
|        2 | 13178 | `	}` |
|        - | 13179 | `	/* Return 1 */` |
|        3 | 13180 | `	ph7_result_int(pCtx,1);` |
|        3 | 13181 | `	return SXRET_OK;` |
|        2 | 13182 |  |
|        - | 13183 | `/*` |
|        - | 13184 | ` * void exit(string $msg)` |
|        - | 13185 | ` * void exit(int $status)` |
|        - | 13186 | ` * void die(string $ms)` |
|        - | 13187 | ` * void die(int $status)` |
|        - | 13188 | ` *   Output a message and terminate program execution.` |
|        - | 13189 | ` * Parameter` |
|        - | 13190 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - | 13191 | ` *  If status is an integer, that value will be used as the exit status` |
|        - | 13192 | ` *  and not printed` |
|        - | 13193 | ` * Return` |
|        - | 13194 | ` *  NULL` |
|        - | 13195 | ` */` |
|      ! 0 | 13196 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 13197 |  |
|      ! 0 | 13198 | `	if( nArg > 0 ){` |
|      ! 0 | 13199 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - | 13200 | `			const char *zData;` |
|      ! 0 | 13201 | `			int iLen = 0;` |
|        - | 13202 | `			/* Print exit message */` |
|      ! 0 | 13203 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 | 13204 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 | 13205 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - | 13206 | `			sxi32 iExitStatus;` |
|        - | 13207 | `			/* Record exit status code */` |
|      ! 0 | 13208 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 | 13209 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 | 13210 | `		}` |
|      ! 0 | 13211 | `	}` |
|        - | 13212 | `	/* Request a VM-wide halt (see PH7_OP_HALT) and abort processing` |
|        - | 13213 | `	 * immediately; the abort unwinds enclosing frames and execution units.` |
|        - | 13214 | `	 */` |
|      ! 0 | 13215 | `	pCtx->pVm->bHaltRequested = 1;` |
|      ! 0 | 13216 | `	return PH7_ABORT;` |
|      ! 0 | 13217 |  |
|        - | 13218 | `/*` |
|        - | 13219 | ` * bool isset($var,...)` |
|        - | 13220 | ` *  Finds out whether a variable is set.` |
|        - | 13221 | ` * Parameters` |
|        - | 13222 | ` *  One or more variable to check.` |
|        - | 13223 | ` * Return` |
|        - | 13224 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - | 13225 | ` */` |
|    93360 | 13226 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13227 |  |
|        - | 13228 | `	ph7_value *pObj;` |
|    93362 | 13229 | `	int res = 0;` |
|        - | 13230 | `	int i;` |
|    93362 | 13231 | `	if( nArg < 1 ){` |
|        - | 13232 | `		/* Missing arguments,return false */` |
|      ! 0 | 13233 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 | 13234 | `		return SXRET_OK;` |
|        - | 13235 | `	}` |
|        - | 13236 | `	/* Iterate over available arguments */` |
|   122010 | 13237 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    93372 | 13238 | `		pObj = apArg[i];` |
|    93372 | 13239 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|        - | 13240 | `			/* Skip the "expecting a variable" warning for MEMOBJ_BOOL —` |
|        - | 13241 | `			 * synthesized by LOAD_IDX iP2=4 (ArrayAccess::offsetExists) and` |
|        - | 13242 | `			 * by anyone passing a bool literal (rare, harmless). */` |
|    63728 | 13243 | `			if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - | 13244 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 | 13245 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 | 13246 | `			}` |
|    31863 | 13247 | `		}` |
|    93372 | 13248 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    93372 | 13249 | `		if( !res ){` |
|        - | 13250 | `			/* Variable not set,return FALSE */` |
|    64724 | 13251 | `			ph7_result_bool(pCtx,0);` |
|    64724 | 13252 | `			return SXRET_OK;` |
|        - | 13253 | `		}` |
|    14326 | 13254 | `	}` |
|        - | 13255 | `	/* All given variable are set,return TRUE */` |
|    28640 | 13256 | `	ph7_result_bool(pCtx,1);` |
|    28640 | 13257 | `	return SXRET_OK;` |
|    46682 | 13258 |  |
|        - | 13259 | `/*` |
|        - | 13260 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - | 13261 | ` * frame,the reference table and discard it's contents.` |
|        - | 13262 | ` * This function never fail and always return SXRET_OK.` |
|        - | 13263 | ` */` |
|  3171362 | 13264 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 | 13265 |  |
|        - | 13266 | `	ph7_value *pObj;` |
|        - | 13267 | `	VmRefObj *pRef;` |
|  3171364 | 13268 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  3171364 | 13269 | `	if( pObj ){` |
|        - | 13270 | `		/* Release the object */` |
|  3171364 | 13271 | `		PH7_MemObjRelease(pObj);` |
|  1585681 | 13272 | `	}` |
|        - | 13273 | `	/* Remove old reference links */` |
|  3171364 | 13274 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  3171364 | 13275 | `	if( pRef ){` |
|  3171358 | 13276 | `		sxi32 iFlags = pRef->iFlags;` |
|        - | 13277 | `		/* Unlink from the reference table */` |
|  3171358 | 13278 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  3171358 | 13279 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - | 13280 | `			VmSlot sFree;` |
|        - | 13281 | `			/* Restore to the free list */` |
|  3171350 | 13282 | `			sFree.nIdx = nObjIdx;` |
|  3171350 | 13283 | `			sFree.pUserData = 0;` |
|  3171350 | 13284 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1585674 | 13285 | `		}` |
|  1585678 | 13286 | `	}` |
|  3171364 | 13287 | `	return SXRET_OK;` |
|        2 | 13288 |  |
|        - | 13289 | `/*` |
|        - | 13290 | ` * void unset($var,...)` |
|        - | 13291 | ` *   Unset one or more given variable.` |
|        - | 13292 | ` * Parameters` |
|        - | 13293 | ` *  One or more variable to unset.` |
|        - | 13294 | ` * Return` |
|        - | 13295 | ` *  Nothing.` |
|        - | 13296 | ` */` |
|     7586 | 13297 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13298 |  |
|        - | 13299 | `	ph7_value *pObj;` |
|        - | 13300 | `	ph7_vm *pVm;` |
|        - | 13301 | `	int i;` |
|        - | 13302 | `	/* Point to the target VM */` |
|     7588 | 13303 | `	pVm = pCtx->pVm;` |
|        - | 13304 | `	/* Iterate and unset */` |
|    15174 | 13305 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     7588 | 13306 | `		pObj = apArg[i];` |
|     7588 | 13307 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      840 | 13308 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 13309 | `				/* Throw an error */` |
|      ! 0 | 13310 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 | 13311 | `			}` |
|      421 | 13312 | `		}else{` |
|     6750 | 13313 | `			sxu32 nIdx = pObj->nIdx;` |
|        - | 13314 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     6750 | 13315 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     6744 | 13316 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3371 | 13317 | `			}` |
|        - | 13318 | `		}` |
|     3795 | 13319 | `	}` |
|     7588 | 13320 | `	return SXRET_OK;` |
|        2 | 13321 |  |
|        - | 13322 | `/*` |
|        - | 13323 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - | 13324 | ` */` |
|      116 | 13325 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 13326 |  |
|      117 | 13327 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      117 | 13328 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 13329 | `	ph7_value *pObj;` |
|        - | 13330 | `	sxu32 nIdx;` |
|        - | 13331 | `	/* Extract the memory object */` |
|      117 | 13332 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      117 | 13333 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      117 | 13334 | `	if( pObj ){` |
|      117 | 13335 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      115 | 13336 | `			if( pEntry->nKeyLen > 0 ){` |
|        - | 13337 | `				SyString sName;` |
|        - | 13338 | `				ph7_value sKey;` |
|        - | 13339 | `				/* Perform the insertion (pObj may point into pVm->aMemObj; the` |
|        - | 13340 | `				 * inserter snapshots the source before reserving, so the pool may` |
|        - | 13341 | `				 * safely move underneath it — see HashmapInsertIntKey/BlobKey). */` |
|      115 | 13342 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      115 | 13343 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      115 | 13344 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      115 | 13345 | `				PH7_MemObjRelease(&sKey);` |
|       57 | 13346 | `			}` |
|       57 | 13347 | `		}` |
|       58 | 13348 | `	}` |
|      117 | 13349 | `	return SXRET_OK;` |
|        1 | 13350 |  |
|        - | 13351 | `/*` |
|        - | 13352 | ` * array get_defined_vars(void)` |
|        - | 13353 | ` *  Returns an array of all defined variables.` |
|        - | 13354 | ` * Parameter` |
|        - | 13355 | ` *  None` |
|        - | 13356 | ` * Return` |
|        - | 13357 | ` *  An array with all the variables defined in the current scope.` |
|        - | 13358 | ` */` |
|        2 | 13359 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13360 |  |
|        3 | 13361 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13362 | `	ph7_value *pArray;` |
|        - | 13363 | `	/* Create a new array */` |
|        3 | 13364 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 13365 | ` 	if( pArray == 0 ){` |
|      ! 0 | 13366 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 13367 | `		SXUNUSED(apArg);` |
|        - | 13368 | `		/* Return NULL */` |
|      ! 0 | 13369 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13370 | `		return SXRET_OK;` |
|        - | 13371 | `	}` |
|        - | 13372 | `	/* Superglobals first */` |
|        3 | 13373 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - | 13374 | `	/* Then variable defined in the current frame */` |
|        3 | 13375 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - | 13376 | `	/* Finally,return the created array */` |
|        3 | 13377 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 13378 | `	return SXRET_OK;` |
|        2 | 13379 |  |
|        - | 13380 | `/*` |
|        - | 13381 | ` * bool gettype($var)` |
|        - | 13382 | ` *  Get the type of a variable` |
|        - | 13383 | ` * Parameters` |
|        - | 13384 | ` *   $var` |
|        - | 13385 | ` *    The variable being type checked.` |
|        - | 13386 | ` * Return` |
|        - | 13387 | ` *   String representation of the given variable type.` |
|        - | 13388 | ` */` |
|       32 | 13389 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13390 |  |
|       34 | 13391 | `	const char *zType = "Empty";` |
|       34 | 13392 | `	if( nArg > 0 ){` |
|       34 | 13393 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 | 13394 | `	}` |
|        - | 13395 | `	/* Return the variable type */` |
|       34 | 13396 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 | 13397 | `	return SXRET_OK;` |
|        2 | 13398 |  |
|        - | 13399 | `/*` |
|        - | 13400 | ` * string get_resource_type(resource $handle)` |
|        - | 13401 | ` *  This function gets the type of the given resource.` |
|        - | 13402 | ` * Parameters` |
|        - | 13403 | ` *  $handle` |
|        - | 13404 | ` *  The evaluated resource handle.` |
|        - | 13405 | ` * Return` |
|        - | 13406 | ` *  If the given handle is a resource, this function will return a string` |
|        - | 13407 | ` *  representing its type. If the type is not identified by this function` |
|        - | 13408 | ` *  the return value will be the string Unknown.` |
|        - | 13409 | ` *  This function will return FALSE and generate an error if handle` |
|        - | 13410 | ` *  is not a resource.` |
|        - | 13411 | ` */` |
|        2 | 13412 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13413 |  |
|        3 | 13414 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13415 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 | 13416 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13417 | `		return PH7_OK;` |
|        - | 13418 | `	}` |
|        3 | 13419 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 | 13420 | `	return SXRET_OK;` |
|        2 | 13421 |  |
|        - | 13422 | `/*` |
|        - | 13423 | ` * void var_dump(expression,....)` |
|        - | 13424 | ` *   var_dump � Dumps information about a variable` |
|        - | 13425 | ` * Parameters` |
|        - | 13426 | ` *   One or more expression to dump.` |
|        - | 13427 | ` * Returns` |
|        - | 13428 | ` *  Nothing.` |
|        - | 13429 | ` */` |
|      218 | 13430 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13431 |  |
|        - | 13432 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - | 13433 | `	int i;` |
|      220 | 13434 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - | 13435 | `	/* Dump one or more expressions */` |
|      444 | 13436 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 | 13437 | `		ph7_value *pObj = apArg[i];` |
|        - | 13438 | `		/* Reset the working buffer */` |
|      226 | 13439 | `		SyBlobReset(&sDump);` |
|        - | 13440 | `		/* Dump the given expression */` |
|      226 | 13441 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - | 13442 | `		/* Output */` |
|      226 | 13443 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 | 13444 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 | 13445 | `		}` |
|      114 | 13446 | `	}` |
|        - | 13447 | `	/* Release the working buffer */` |
|      220 | 13448 | `	SyBlobRelease(&sDump);` |
|      220 | 13449 | `	return SXRET_OK;` |
|        2 | 13450 |  |
|        - | 13451 | `/*` |
|        - | 13452 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - | 13453 | ` *   print-r - Prints human-readable information about a variable` |
|        - | 13454 | ` * Parameters` |
|        - | 13455 | ` *   expression: Expression to dump` |
|        - | 13456 | ` *   return : If you would like to capture the output of print_r() use` |
|        - | 13457 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - | 13458 | ` *            print_r() will return the information rather than print it.` |
|        - | 13459 | ` * Return` |
|        - | 13460 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - | 13461 | ` *  Otherwise, the return value is TRUE.` |
|        - | 13462 | ` */` |
|       16 | 13463 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13464 |  |
|       17 | 13465 | `	int ret_string = 0;` |
|        - | 13466 | `	SyBlob sDump;` |
|       17 | 13467 | `	if( nArg < 1 ){` |
|        - | 13468 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 13469 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13470 | `		return SXRET_OK;` |
|        - | 13471 | `	}` |
|       17 | 13472 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 | 13473 | `	if ( nArg > 1 ){` |
|        - | 13474 | `		/* Where to redirect output */` |
|       11 | 13475 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 | 13476 | `	}` |
|        - | 13477 | `	/* Generate dump */` |
|       17 | 13478 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 | 13479 | `	if( !ret_string ){` |
|        - | 13480 | `		/* Output dump */` |
|        7 | 13481 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13482 | `		/* Return true */` |
|        7 | 13483 | `		ph7_result_bool(pCtx,1);` |
|        4 | 13484 | `	}else{` |
|        - | 13485 | `		/* Generated dump as return value */` |
|       11 | 13486 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13487 | `	}` |
|        - | 13488 | `	/* Release the working buffer */` |
|       17 | 13489 | `	SyBlobRelease(&sDump);` |
|       17 | 13490 | `	return SXRET_OK;` |
|        9 | 13491 |  |
|        - | 13492 | `/*` |
|        - | 13493 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - | 13494 | ` * Same job as print_r. (see coment above)` |
|        - | 13495 | ` */` |
|        2 | 13496 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13497 |  |
|        3 | 13498 | `	int ret_string = 0;` |
|        - | 13499 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 | 13500 | `	if( nArg < 1 ){` |
|        - | 13501 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 13502 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13503 | `		return SXRET_OK;` |
|        - | 13504 | `	}` |
|        3 | 13505 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 | 13506 | `	if ( nArg > 1 ){` |
|        - | 13507 | `		/* Where to redirect output */` |
|        3 | 13508 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 | 13509 | `	}` |
|        - | 13510 | `	/* Generate dump */` |
|        3 | 13511 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 | 13512 | `	if( !ret_string ){` |
|        - | 13513 | `		/* Output dump */` |
|      ! 0 | 13514 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13515 | `		/* Return NULL */` |
|      ! 0 | 13516 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13517 | `	}else{` |
|        - | 13518 | `		/* Generated dump as return value */` |
|        3 | 13519 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13520 | `	}` |
|        - | 13521 | `	/* Release the working buffer */` |
|        3 | 13522 | `	SyBlobRelease(&sDump);` |
|        3 | 13523 | `	return SXRET_OK;` |
|        2 | 13524 |  |
|        - | 13525 | `/*` |
|        - | 13526 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - | 13527 | ` *  Set/get the various assert flags.` |
|        - | 13528 | ` * Parameter` |
|        - | 13529 | ` * $what` |
|        - | 13530 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - | 13531 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - | 13532 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - | 13533 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - | 13534 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - | 13535 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - | 13536 | ` * $value` |
|        - | 13537 | ` *   An optional new value for the option.` |
|        - | 13538 | ` * Return` |
|        - | 13539 | ` *  Old setting on success or FALSE on failure.` |
|        - | 13540 | ` */` |
|       28 | 13541 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13542 |  |
|       30 | 13543 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13544 | `	int iOption;` |
|        - | 13545 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       30 | 13546 | `	if( nArg < 1 ){` |
|        3 | 13547 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13548 | `			"ArgumentCountError",` |
|        - | 13549 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - | 13550 | `			);` |
|        - | 13551 | `	}` |
|        - | 13552 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       26 | 13553 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       28 | 13554 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 | 13555 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13556 | `			"TypeError",` |
|        - | 13557 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 | 13558 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 | 13559 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - | 13560 | `			);` |
|        - | 13561 | `	}` |
|       28 | 13562 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - | 13563 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - | 13564 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - | 13565 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       28 | 13566 | `	switch( iOption ){` |
|        5 | 13567 | `	case 1: /* ASSERT_ACTIVE */` |
|        - | 13568 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       12 | 13569 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       12 | 13570 | `		if( nArg > 1 ){` |
|        5 | 13571 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 13572 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 | 13573 | `			}else{` |
|        3 | 13574 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - | 13575 | `			}` |
|        2 | 13576 | `		}` |
|       12 | 13577 | `		break;` |
|        1 | 13578 | `	case 2: /* ASSERT_CALLBACK */` |
|        - | 13579 | `		/* Return old callback or null */` |
|        3 | 13580 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 | 13581 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 | 13582 | `		}else{` |
|        3 | 13583 | `			ph7_result_null(pCtx);` |
|        - | 13584 | `		}` |
|        3 | 13585 | `		if( nArg > 1 ){` |
|      ! 0 | 13586 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 | 13587 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 | 13588 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 | 13589 | `			}else{` |
|      ! 0 | 13590 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - | 13591 | `			}` |
|      ! 0 | 13592 | `		}` |
|        3 | 13593 | `		break;` |
|        5 | 13594 | `	case 3: /* ASSERT_BAIL */` |
|       11 | 13595 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 | 13596 | `		if( nArg > 1 ){` |
|        5 | 13597 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 13598 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 | 13599 | `			}else{` |
|        3 | 13600 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - | 13601 | `			}` |
|        2 | 13602 | `		}` |
|       11 | 13603 | `		break;` |
|      ! 0 | 13604 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 | 13605 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 13606 | `		break;` |
|        1 | 13607 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 | 13608 | `		ph7_result_int(pCtx, 1);` |
|        3 | 13609 | `		break;` |
|      ! 0 | 13610 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 | 13611 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 13612 | `		break;` |
|        1 | 13613 | `	default:` |
|        - | 13614 | `		/* PHP 8: ValueError for invalid option */` |
|        3 | 13615 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13616 | `			"ValueError",` |
|        - | 13617 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - | 13618 | `			);` |
|        - | 13619 | `	}` |
|       26 | 13620 | `	return PH7_OK;` |
|       16 | 13621 |  |
|        - | 13622 | `/*` |
|        - | 13623 | ` * bool assert(mixed $assertion)` |
|        - | 13624 | ` *  Checks if assertion is FALSE.` |
|        - | 13625 | ` * Parameter` |
|        - | 13626 | ` *  $assertion` |
|        - | 13627 | ` *    The assertion to test.` |
|        - | 13628 | ` * Return` |
|        - | 13629 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - | 13630 | ` */` |
|       24 | 13631 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13632 |  |
|       26 | 13633 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13634 | `	int iFlags,iResult;` |
|        - | 13635 | `	const char *zDesc;` |
|        - | 13636 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       26 | 13637 | `	if( nArg < 1 ){` |
|        3 | 13638 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13639 | `			"ArgumentCountError",` |
|        - | 13640 | `			"assert() expects at least 1 argument, 0 given"` |
|        - | 13641 | `			);` |
|        - | 13642 | `	}` |
|       24 | 13643 | `	iFlags = pVm->iAssertFlags;` |
|       24 | 13644 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - | 13645 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 | 13646 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 | 13647 | `		return PH7_OK;` |
|        - | 13648 | `	}` |
|        - | 13649 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       24 | 13650 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       24 | 13651 | `	if( !iResult ){` |
|        - | 13652 | `		/* Assertion failed */` |
|        - | 13653 | `		/* Extract optional description */` |
|       13 | 13654 | `		zDesc = 0;` |
|       13 | 13655 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 13656 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 | 13657 | `		}` |
|       13 | 13658 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - | 13659 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - | 13660 | `			ph7_value sFile,sLine;` |
|        - | 13661 | `			ph7_value *apCbArg[3];` |
|        - | 13662 | `			SyString *pFile;` |
|        - | 13663 | `			/* Extract the processed script */` |
|      ! 0 | 13664 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 | 13665 | `			if( pFile == 0 ){` |
|      ! 0 | 13666 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 | 13667 | `			}` |
|        - | 13668 | `			/* Invoke the callback */` |
|      ! 0 | 13669 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 | 13670 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 | 13671 | `			apCbArg[0] = &sFile;` |
|      ! 0 | 13672 | `			apCbArg[1] = &sLine;` |
|      ! 0 | 13673 | `			apCbArg[2] = apArg[0];` |
|      ! 0 | 13674 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - | 13675 | `			/* Clean-up the mess left behind */` |
|      ! 0 | 13676 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 | 13677 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 | 13678 | `		}` |
|       13 | 13679 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - | 13680 | `			/* Abort VM execution immediately */` |
|      ! 0 | 13681 | `			return PH7_ABORT;` |
|        - | 13682 | `		}` |
|        - | 13683 | `		/* PHP 8: throw AssertionError by default */` |
|       13 | 13684 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 | 13685 | `			return PH7_VmThrowException(pCtx,` |
|        - | 13686 | `				"AssertionError",` |
|        - | 13687 | `				"%s",` |
|        1 | 13688 | `				zDesc` |
|        - | 13689 | `				);` |
|      ! 0 | 13690 | `		}else{` |
|       11 | 13691 | `			return PH7_VmThrowException(pCtx,` |
|        - | 13692 | `				"AssertionError",` |
|        - | 13693 | `				"assert(false)"` |
|        - | 13694 | `				);` |
|        - | 13695 | `		}` |
|        - | 13696 | `	}` |
|        - | 13697 | `	/* Assertion passed */` |
|       11 | 13698 | `	ph7_result_bool(pCtx,1);` |
|       11 | 13699 | `	return PH7_OK;` |
|       14 | 13700 |  |
|        - | 13701 | `/*` |
|        - | 13702 | ` * Section:` |
|        - | 13703 | ` *  Error reporting functions.` |
|        - | 13704 | ` * Status:` |
|        - | 13705 | ` *    Stable.` |
|        - | 13706 | ` */` |
|        - | 13707 | `/*` |
|        - | 13708 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - | 13709 | ` *  Generates a user-level error/warning/notice message.` |
|        - | 13710 | ` * Parameters` |
|        - | 13711 | ` *  $error_msg` |
|        - | 13712 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - | 13713 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - | 13714 | ` * $error_type` |
|        - | 13715 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - | 13716 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - | 13717 | ` * Return` |
|        - | 13718 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - | 13719 | ` */` |
|       12 | 13720 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13721 |  |
|       14 | 13722 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 | 13723 | `	int rc = PH7_OK;` |
|       14 | 13724 | `	if( nArg > 0 ){` |
|        - | 13725 | `		const char *zErr;` |
|        - | 13726 | `		int nLen;` |
|        - | 13727 | `		/* Extract the error message */` |
|       12 | 13728 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 | 13729 | `		if( nArg > 1 ){` |
|        - | 13730 | `			/* Extract the error type */` |
|       12 | 13731 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 | 13732 | `			switch( nErr ){` |
|        1 | 13733 | `			case 1:   /* E_ERROR */` |
|        - | 13734 | `			case 16:  /* E_CORE_ERROR */` |
|        - | 13735 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - | 13736 | `			case 256: /* E_USER_ERROR */` |
|        3 | 13737 | `				nErr = PH7_CTX_ERR;` |
|        3 | 13738 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 | 13739 | `				break;` |
|        1 | 13740 | `			case 2:   /* E_WARNING */` |
|        - | 13741 | `			case 32:  /* E_CORE_WARNING */` |
|        - | 13742 | `			case 123: /* E_COMPILE_WARNING */` |
|        - | 13743 | `			case 512: /* E_USER_WARNING */` |
|        3 | 13744 | `				nErr = PH7_CTX_WARNING;` |
|        3 | 13745 | `				break;` |
|        3 | 13746 | `			default:` |
|        8 | 13747 | `				nErr = PH7_CTX_NOTICE;` |
|        6 | 13748 | `				break;` |
|        - | 13749 | `			}` |
|        5 | 13750 | `		}` |
|        - | 13751 | `		/* Report error */` |
|       12 | 13752 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 | 13753 | `		if( rc == PH7_ABORT ){` |
|      ! 0 | 13754 | `			return rc;` |
|        - | 13755 | `		}` |
|        - | 13756 | `		/* Return true */` |
|       12 | 13757 | `		ph7_result_bool(pCtx,1);` |
|        7 | 13758 | `	}else{` |
|        - | 13759 | `		/* Missing arguments,return FALSE */` |
|        3 | 13760 | `		ph7_result_bool(pCtx,0);` |
|        - | 13761 | `	}` |
|       14 | 13762 | `	return rc;` |
|        8 | 13763 |  |
|        - | 13764 | `/*` |
|        - | 13765 | ` * int error_reporting([int $level])` |
|        - | 13766 | ` *  Sets which PHP errors are reported.` |
|        - | 13767 | ` * Parameters` |
|        - | 13768 | ` *  $level` |
|        - | 13769 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - | 13770 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - | 13771 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - | 13772 | ` *   levels will not always behave as expected.` |
|        - | 13773 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - | 13774 | ` *   in the predefined constants.` |
|        - | 13775 | ` * Return` |
|        - | 13776 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - | 13777 | ` *   parameter is given.` |
|        - | 13778 | ` */` |
|       32 | 13779 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13780 |  |
|       34 | 13781 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13782 | `	int nOld;` |
|        - | 13783 | `	/* Extract the old reporting level */` |
|       34 | 13784 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       34 | 13785 | `	if( nArg > 0 ){` |
|        - | 13786 | `		int nNew;` |
|        - | 13787 | `		/* Extract the desired error reporting level */` |
|       28 | 13788 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       28 | 13789 | `		if( !nNew ){` |
|        - | 13790 | `			/* Do not report errors at all */` |
|        5 | 13791 | `			pVm->bErrReport = 0;` |
|        3 | 13792 | `		}else{` |
|        - | 13793 | `			/* Report all errors */` |
|       24 | 13794 | `			pVm->bErrReport = 1;` |
|        - | 13795 | `		}` |
|       13 | 13796 | `	}` |
|        - | 13797 | `	/* Return the old level */` |
|       34 | 13798 | `	ph7_result_int(pCtx,nOld);` |
|       34 | 13799 | `	return PH7_OK;` |
|        2 | 13800 |  |
|        - | 13801 | `/*` |
|        - | 13802 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - | 13803 | ` *  Send an error message somewhere.` |
|        - | 13804 | ` * Parameter` |
|        - | 13805 | ` *  $message` |
|        - | 13806 | ` *   The error message that should be logged.` |
|        - | 13807 | ` *  $message_type` |
|        - | 13808 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - | 13809 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - | 13810 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - | 13811 | ` *       This is the default option.` |
|        - | 13812 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - | 13813 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - | 13814 | ` *    2  No longer an option.` |
|        - | 13815 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - | 13816 | ` *       to the end of the message string.` |
|        - | 13817 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - | 13818 | ` *  $destination` |
|        - | 13819 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - | 13820 | ` *  $extra_headers` |
|        - | 13821 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - | 13822 | ` * Return` |
|        - | 13823 | ` *  TRUE on success or FALSE on failure.` |
|        - | 13824 | ` * NOTE:` |
|        - | 13825 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - | 13826 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - | 13827 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - | 13828 | ` *  Otherwise this function is no-op.` |
|        - | 13829 | ` */` |
|        4 | 13830 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13831 |  |
|        - | 13832 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 | 13833 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 | 13834 | `	int iType = 0;` |
|        5 | 13835 | `	if( nArg < 1 ){` |
|        - | 13836 | `		/* Missing log message,return FALSE */` |
|      ! 0 | 13837 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13838 | `		return PH7_OK;` |
|        - | 13839 | `	}` |
|        5 | 13840 | `	if( pVm->xErrLog  ){` |
|        - | 13841 | `		/* Invoke the user callback */` |
|      ! 0 | 13842 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 | 13843 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 | 13844 | `		if( nArg > 1 ){` |
|      ! 0 | 13845 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 13846 | `			if( nArg > 2 ){` |
|      ! 0 | 13847 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 | 13848 | `				if( nArg > 3 ){` |
|      ! 0 | 13849 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 | 13850 | `				}` |
|      ! 0 | 13851 | `			}` |
|      ! 0 | 13852 | `		}` |
|      ! 0 | 13853 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 | 13854 | `	}` |
|        - | 13855 | `	/* Retun TRUE */` |
|        5 | 13856 | `	ph7_result_bool(pCtx,1);` |
|        5 | 13857 | `	return PH7_OK;` |
|        3 | 13858 |  |
|        - | 13859 | `/*` |
|        - | 13860 | ` * bool restore_exception_handler(void)` |
|        - | 13861 | ` *  Restores the previously defined exception handler function.` |
|        - | 13862 | ` * Parameter` |
|        - | 13863 | ` *  None` |
|        - | 13864 | ` * Return` |
|        - | 13865 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - | 13866 | ` */` |
|        4 | 13867 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13868 |  |
|        5 | 13869 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13870 | `	ph7_value *pOld,*pNew;` |
|        - | 13871 | `	/* Point to the old and the new handler */` |
|        5 | 13872 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 | 13873 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 | 13874 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 13875 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 13876 | `		SXUNUSED(apArg);` |
|        - | 13877 | `		/* No installed handler,return FALSE */` |
|        5 | 13878 | `		ph7_result_bool(pCtx,0);` |
|        5 | 13879 | `		return PH7_OK;` |
|        - | 13880 | `	}` |
|        - | 13881 | `	/* Copy the old handler */` |
|      ! 0 | 13882 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 13883 | `	PH7_MemObjRelease(pOld);` |
|        - | 13884 | `	/* Return TRUE */` |
|      ! 0 | 13885 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 13886 | `	return PH7_OK;` |
|        3 | 13887 |  |
|        - | 13888 | `/*` |
|        - | 13889 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - | 13890 | ` *  Sets a user-defined exception handler function.` |
|        - | 13891 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - | 13892 | ` * NOTE` |
|        - | 13893 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - | 13894 | ` *  the satndard PHP engine.` |
|        - | 13895 | ` * Parameters` |
|        - | 13896 | ` *  $exception_handler` |
|        - | 13897 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - | 13898 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - | 13899 | ` *   that was thrown.` |
|        - | 13900 | ` *  Note:` |
|        - | 13901 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 13902 | ` * Return` |
|        - | 13903 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - | 13904 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 13905 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 13906 | ` */` |
|        4 | 13907 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13908 |  |
|        6 | 13909 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13910 | `	ph7_value *pOld,*pNew;` |
|        - | 13911 | `	/* Point to the old and the new handler */` |
|        6 | 13912 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 | 13913 | `	pNew = &pVm->aExceptionCB[1];` |
|        - | 13914 | `	/* Return the old handler */` |
|        6 | 13915 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 | 13916 | `	if( nArg > 0 ){` |
|        6 | 13917 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 13918 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 | 13919 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 | 13920 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 13921 | `		}else{` |
|        6 | 13922 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 13923 | `			/* Install the new handler */` |
|        6 | 13924 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 13925 | `		}` |
|        2 | 13926 | `	}` |
|        6 | 13927 | `	return PH7_OK;` |
|        2 | 13928 |  |
|        - | 13929 | `/*` |
|        - | 13930 | ` * bool restore_error_handler(void)` |
|        - | 13931 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 13932 | ` * Parameters:` |
|        - | 13933 | ` *  None.` |
|        - | 13934 | ` * Return` |
|        - | 13935 | ` *  Always TRUE.` |
|        - | 13936 | ` */` |
|        6 | 13937 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13938 |  |
|        7 | 13939 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13940 | `	ph7_value *pOld,*pNew;` |
|        - | 13941 | `	/* Point to the old and the new handler */` |
|        7 | 13942 | `	pOld = &pVm->aErrCB[0];` |
|        7 | 13943 | `	pNew = &pVm->aErrCB[1];` |
|        7 | 13944 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        3 | 13945 | `		SXUNUSED(nArg); /* cc warning */` |
|        3 | 13946 | `		SXUNUSED(apArg);` |
|        - | 13947 | `		/* No installed callback,return FALSE */` |
|        7 | 13948 | `		ph7_result_bool(pCtx,0);` |
|        7 | 13949 | `		return PH7_OK;` |
|        - | 13950 | `	}` |
|        - | 13951 | `	/* Copy the old callback */` |
|      ! 0 | 13952 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 13953 | `	PH7_MemObjRelease(pOld);` |
|        - | 13954 | `	/* Return TRUE */` |
|      ! 0 | 13955 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 13956 | `	return PH7_OK;` |
|        4 | 13957 |  |
|        - | 13958 | `/*` |
|        - | 13959 | ` * value set_error_handler(callable $error_handler)` |
|        - | 13960 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 13961 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 13962 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 13963 | ` *  Sets a user-defined error handler function.` |
|        - | 13964 | ` *  This function can be used for defining your own way of handling errors during` |
|        - | 13965 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - | 13966 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - | 13967 | ` *  conditions (using trigger_error()).` |
|        - | 13968 | ` * Parameters` |
|        - | 13969 | ` *  $error_handler` |
|        - | 13970 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - | 13971 | ` *   describing the error.` |
|        - | 13972 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - | 13973 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - | 13974 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - | 13975 | ` *   The function can be shown as:` |
|        - | 13976 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - | 13977 | ` *     errno` |
|        - | 13978 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - | 13979 | ` *   errstr` |
|        - | 13980 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - | 13981 | ` *   errfile` |
|        - | 13982 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - | 13983 | ` *     was raised in, as a string.` |
|        - | 13984 | ` *  Note:` |
|        - | 13985 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 13986 | ` * Return` |
|        - | 13987 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - | 13988 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 13989 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 13990 | ` */` |
|    10928 | 13991 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13992 |  |
|    10930 | 13993 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13994 | `	ph7_value *pOld,*pNew;` |
|        - | 13995 | `	/* Point to the old and the new handler */` |
|    10930 | 13996 | `	pOld = &pVm->aErrCB[0];` |
|    10930 | 13997 | `	pNew = &pVm->aErrCB[1];` |
|        - | 13998 | `	/* Return the old handler */` |
|    10930 | 13999 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|    10930 | 14000 | `	if( nArg > 0 ){` |
|    10930 | 14001 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 14002 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     5459 | 14003 | `			PH7_MemObjRelease(pNew);` |
|     5459 | 14004 | `			ph7_result_bool(pCtx,1);` |
|     2730 | 14005 | `		}else{` |
|     5472 | 14006 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 14007 | `			/* Install the new handler */` |
|     5472 | 14008 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 14009 | `		}` |
|     5464 | 14010 | `	}` |
|    10930 | 14011 | `	return PH7_OK;` |
|        2 | 14012 |  |
|        - | 14013 | `/*` |
|        - | 14014 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - | 14015 | ` *  Generates a backtrace.` |
|        - | 14016 | ` * Paramaeter` |
|        - | 14017 | ` *  $options` |
|        - | 14018 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - | 14019 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - | 14020 | ` *   all the function/method arguments, to save memory.` |
|        - | 14021 | ` * $limit` |
|        - | 14022 | ` *   (Not Used)` |
|        - | 14023 | ` * Return` |
|        - | 14024 | ` *  An array.The possible returned elements are as follows:` |
|        - | 14025 | ` *          Possible returned elements from debug_backtrace()` |
|        - | 14026 | ` *          Name        Type      Description` |
|        - | 14027 | ` *          ------      ------     -----------` |
|        - | 14028 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - | 14029 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - | 14030 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - | 14031 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 14032 | ` *          object      object    The current object.` |
|        - | 14033 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 14034 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 14035 | ` */` |
|      948 | 14036 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14037 |  |
|      950 | 14038 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14039 | `	ph7_value *pArray;` |
|        - | 14040 | `	ph7_class *pClass;` |
|        - | 14041 | `	ph7_value *pValue;` |
|        - | 14042 | `	SyString *pFile;` |
|        - | 14043 | `	/* Create a new array */` |
|      950 | 14044 | `	pArray = ph7_context_new_array(pCtx);` |
|      950 | 14045 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      950 | 14046 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 14047 | `		/* Out of memory,return NULL */` |
|      ! 0 | 14048 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 14049 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14050 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 14051 | `		SXUNUSED(apArg);` |
|      ! 0 | 14052 | `		return PH7_OK;` |
|        - | 14053 | `	}` |
|        - | 14054 | `	/* Dump running function name and it's arguments  */` |
|      950 | 14055 | `	if( pVm->pFrame->pParent ){` |
|      950 | 14056 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 14057 | `		ph7_vm_func *pFunc;` |
|        - | 14058 | `		ph7_value *pArg;` |
|      950 | 14059 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      950 | 14060 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      950 | 14061 | `		if( pFrame->pParent && pFunc ){` |
|      950 | 14062 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      950 | 14063 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      950 | 14064 | `			ph7_value_reset_string_cursor(pValue);` |
|      474 | 14065 | `		}` |
|        - | 14066 | `		/* Function arguments */` |
|      950 | 14067 | `		pArg = ph7_context_new_array(pCtx);` |
|      950 | 14068 | `		if( pArg  ){` |
|        - | 14069 | `			ph7_value *pObj;` |
|        - | 14070 | `			VmSlot *aSlot;` |
|        - | 14071 | `			sxu32 n;` |
|        - | 14072 | `			/* Start filling the array with the given arguments */` |
|      950 | 14073 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     3798 | 14074 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     2850 | 14075 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     2850 | 14076 | `				if( pObj ){` |
|     2850 | 14077 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|     1424 | 14078 | `				}` |
|     1426 | 14079 | `			}` |
|        - | 14080 | `			/* Save the array */` |
|      950 | 14081 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      474 | 14082 | `		}` |
|      474 | 14083 | `	}` |
|      950 | 14084 | `	ph7_value_int(pValue,1);` |
|        - | 14085 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 14086 | `	 * line numbers at run-time. )` |
|        - | 14087 | `	 */` |
|      950 | 14088 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 14089 | `	/* Current processed script */` |
|      950 | 14090 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      950 | 14091 | `	if( pFile ){` |
|      950 | 14092 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      950 | 14093 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      950 | 14094 | `		ph7_value_reset_string_cursor(pValue);` |
|      474 | 14095 | `	}` |
|        - | 14096 | `	/* Top class */` |
|      950 | 14097 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      950 | 14098 | `	if( pClass ){` |
|      946 | 14099 | `		ph7_value_reset_string_cursor(pValue);` |
|      946 | 14100 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      946 | 14101 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      472 | 14102 | `	}` |
|        - | 14103 | `	/* Return the freshly created array */` |
|      950 | 14104 | `	ph7_result_value(pCtx,pArray);` |
|        - | 14105 | `	/*` |
|        - | 14106 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 14107 | `	 * as soon we return from this function.` |
|        - | 14108 | `	 */` |
|      950 | 14109 | `	return PH7_OK;` |
|      476 | 14110 |  |
|        - | 14111 | `/*` |
|        - | 14112 | ` * Generate a small backtrace.` |
|        - | 14113 | ` * Store the generated dump in the given BLOB` |
|        - | 14114 | ` */` |
|        4 | 14115 | `static int VmMiniBacktrace(` |
|        - | 14116 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 14117 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 14118 | `	)` |
|        1 | 14119 |  |
|        5 | 14120 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 14121 | `	ph7_vm_func *pFunc;` |
|        - | 14122 | `	ph7_class *pClass;` |
|        - | 14123 | `	SyString *pFile;` |
|        - | 14124 | `	/* Called function */` |
|        5 | 14125 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 14126 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 14127 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 14128 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 14129 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 14130 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 14131 | `	}else{` |
|      ! 0 | 14132 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 14133 | `	}` |
|        5 | 14134 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 14135 | `	/* Current processed script */` |
|        5 | 14136 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 14137 | `	if( pFile ){` |
|        5 | 14138 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 14139 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 14140 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 14141 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 14142 | `	}` |
|        - | 14143 | `	/* Top class */` |
|        5 | 14144 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 14145 | `	if( pClass ){` |
|      ! 0 | 14146 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 14147 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 14148 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 14149 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 14150 | `	}` |
|        5 | 14151 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 14152 | `	/* All done */` |
|        5 | 14153 | `	return SXRET_OK;` |
|        1 | 14154 |  |
|        - | 14155 | `/*` |
|        - | 14156 | ` * void debug_print_backtrace()` |
|        - | 14157 | ` *  Prints a backtrace` |
|        - | 14158 | ` * Parameters` |
|        - | 14159 | ` * None` |
|        - | 14160 | ` * Return` |
|        - | 14161 | ` * NULL` |
|        - | 14162 | ` */` |
|        2 | 14163 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14164 |  |
|        3 | 14165 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14166 | `	SyBlob sDump;` |
|        3 | 14167 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 14168 | `	/* Generate the backtrace */` |
|        3 | 14169 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 14170 | `	/* Output backtrace */` |
|        3 | 14171 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 14172 | `	/* All done,cleanup */` |
|        3 | 14173 | `	SyBlobRelease(&sDump);` |
|        1 | 14174 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 14175 | `	SXUNUSED(apArg);` |
|        3 | 14176 | `	return PH7_OK;` |
|        1 | 14177 |  |
|        - | 14178 | `/*` |
|        - | 14179 | ` * string debug_string_backtrace()` |
|        - | 14180 | ` *  Generate a backtrace` |
|        - | 14181 | ` * Parameters` |
|        - | 14182 | ` * None` |
|        - | 14183 | ` * Return` |
|        - | 14184 | ` *  A mini backtrace().` |
|        - | 14185 | ` * Note that this is a symisc extension.` |
|        - | 14186 | ` */` |
|        2 | 14187 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14188 |  |
|        3 | 14189 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14190 | `	SyBlob sDump;` |
|        3 | 14191 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 14192 | `	/* Generate the backtrace */` |
|        3 | 14193 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 14194 | `	/* Return the backtrace */` |
|        3 | 14195 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 14196 | `	/* All done,cleanup */` |
|        3 | 14197 | `	SyBlobRelease(&sDump);` |
|        1 | 14198 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 14199 | `	SXUNUSED(apArg);` |
|        3 | 14200 | `	return PH7_OK;` |
|        1 | 14201 |  |
|        - | 14202 | `/*` |
|        - | 14203 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 14204 | ` * exception is triggered.` |
|        - | 14205 | ` */` |
|      512 | 14206 | `static sxi32 VmUncaughtException(` |
|        - | 14207 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 14208 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 14209 | `	)` |
|        1 | 14210 |  |
|        - | 14211 | `	ph7_value *apArg[2],sArg;` |
|      513 | 14212 | `	int nArg = 1;` |
|        - | 14213 | `	sxi32 rc;` |
|      513 | 14214 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 14215 | `		/* Nesting limit reached */` |
|      ! 0 | 14216 | `		return SXRET_OK;` |
|        - | 14217 | `	}` |
|        - | 14218 | `	/* Call any exception handler if available */` |
|      513 | 14219 | `	PH7_MemObjInit(pVm,&sArg);` |
|      513 | 14220 | `	if( pThis ){` |
|        - | 14221 | `		/* Load the exception instance */` |
|      513 | 14222 | `		sArg.x.pOther = pThis;` |
|      513 | 14223 | `		pThis->iRef++;` |
|      513 | 14224 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      257 | 14225 | `	}else{` |
|      ! 0 | 14226 | `		nArg = 0;` |
|        - | 14227 | `	}` |
|      513 | 14228 | `	apArg[0] = &sArg;` |
|        - | 14229 | `	/* Call the exception handler if available */` |
|      513 | 14230 | `	pVm->nExceptDepth++;` |
|      513 | 14231 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      513 | 14232 | `	pVm->nExceptDepth--;` |
|      513 | 14233 | `	if( rc != SXRET_OK ){` |
|        - | 14234 | `		SyBlob sMsgBuf;` |
|      511 | 14235 | `		const char *zClass = "Exception";` |
|      511 | 14236 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 14237 | `		const char *zMsg;` |
|        - | 14238 | `		sxu32 nMsg;` |
|        - | 14239 | `		const char *zFuncName;` |
|        - | 14240 | `		int nFuncLen;` |
|      511 | 14241 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      511 | 14242 | `		if( pThis ){` |
|        - | 14243 | `			ph7_class_method *pGetMessage;` |
|        - | 14244 | `			ph7_value sMsg;` |
|        - | 14245 | `			const char *zTmp;` |
|        - | 14246 | `			int nTmp;` |
|      511 | 14247 | `			zClass = pThis->pClass->sName.zString;` |
|      511 | 14248 | `			nClass = pThis->pClass->sName.nByte;` |
|      511 | 14249 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      511 | 14250 | `			if( pGetMessage ){` |
|      511 | 14251 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      511 | 14252 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      511 | 14253 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      511 | 14254 | `					if( zTmp && nTmp > 0 ){` |
|      511 | 14255 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      255 | 14256 | `					}` |
|      255 | 14257 | `				}` |
|      511 | 14258 | `				PH7_MemObjRelease(&sMsg);` |
|      255 | 14259 | `			}` |
|      255 | 14260 | `		}` |
|      511 | 14261 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      511 | 14262 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      511 | 14263 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      511 | 14264 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      511 | 14265 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 14266 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      511 | 14267 | `		rc = SXERR_ABORT;` |
|      255 | 14268 | `	}` |
|      513 | 14269 | `	PH7_MemObjRelease(&sArg);` |
|      513 | 14270 | `	return rc;` |
|      257 | 14271 |  |
|        - | 14272 | `/*` |
|        - | 14273 | ` * Throw a user exception.` |
|        - | 14274 | ` *` |
|        - | 14275 | ` * Exception dispatch follows this sequence:` |
|        - | 14276 | ` *` |
|        - | 14277 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 14278 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 14279 | ` *` |
|        - | 14280 | ` * 2. If NO catch matches:` |
|        - | 14281 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 14282 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 14283 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 14284 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 14285 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 14286 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 14287 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 14288 | ` *` |
|        - | 14289 | ` * 3. If a catch DOES match:` |
|        - | 14290 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 14291 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 14292 | ` *       inside the catch body from immediately propagating past our` |
|        - | 14293 | ` *       finally block.` |
|        - | 14294 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 14295 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 14296 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 14297 | ` *       in pPendingException (step 2c).` |
|        - | 14298 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 14299 | ` *    d. Run finally (if present).` |
|        - | 14300 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 14301 | ` *       that handlers are restored and finally has run.` |
|        - | 14302 | ` */` |
|      878 | 14303 | `static sxi32 VmThrowException(` |
|        - | 14304 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 14305 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 14306 | `	)` |
|        2 | 14307 |  |
|        - | 14308 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 14309 | `	ph7_exception **apException;` |
|        - | 14310 | `	ph7_exception *pException;` |
|        - | 14311 | `	/* An in-flight throw abandons any pending null-coalesce-assign store:` |
|        - | 14312 | `	 * disarm so the RHS-evaluation throw can't leave the slot live for a` |
|        - | 14313 | `	 * later unrelated NULLC_STORE (stale offsetSet) or leak the instance ref. */` |
|      880 | 14314 | `	VmCoalesceDisarm(pVm);` |
|        - | 14315 | `	/* Point to the stack of loaded exceptions */` |
|      880 | 14316 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      880 | 14317 | `	pException = 0;` |
|      880 | 14318 | `	pCatch = 0;` |
|      880 | 14319 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 14320 | `		ph7_exception_block *aCatch;` |
|        - | 14321 | `		ph7_class *pClass;` |
|        - | 14322 | `		SyString *aNames;` |
|        - | 14323 | `		sxu32 nNames;` |
|        - | 14324 | `		int matched;` |
|        - | 14325 | `		sxu32 j,k;` |
|        - | 14326 | `		/* Locate the appropriate block to execute */` |
|      360 | 14327 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|      360 | 14328 | `		(void)SySetPop(&pVm->aException);` |
|      360 | 14329 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|      368 | 14330 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|        - | 14331 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
|      366 | 14332 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
|      366 | 14333 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
|      366 | 14334 | `			matched = 0;` |
|      392 | 14335 | `			for( k = 0 ; k < nNames ; ++k ){` |
|        - | 14336 | `				/* Extract the target class or interface (iLoadable=FALSE so` |
|        - | 14337 | `				 * interfaces like Throwable are resolvable as catch targets).` |
|        - | 14338 | `				 * Traits are never instance-compatible, so skip them explicitly. */` |
|      384 | 14339 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,FALSE,0);` |
|      384 | 14340 | `				if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - | 14341 | `					/* No such class, or trait — cannot match */` |
|      ! 0 | 14342 | `					continue;` |
|        - | 14343 | `				}` |
|      384 | 14344 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      358 | 14345 | `					matched = 1;` |
|      358 | 14346 | `					break;` |
|        - | 14347 | `				}` |
|       14 | 14348 | `			}` |
|      366 | 14349 | `			if( matched ){` |
|        - | 14350 | `				/* Catch block found,break immediately */` |
|      358 | 14351 | `				pCatch = &aCatch[j];` |
|      358 | 14352 | `				break;` |
|        - | 14353 | `			}` |
|        5 | 14354 | `		}` |
|      179 | 14355 | `	}` |
|        - | 14356 | `	/* Execute the cached block if available */` |
|      880 | 14357 | `	if( pCatch == 0 ){` |
|        - | 14358 | `		sxi32 rc;` |
|        - | 14359 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      524 | 14360 | `		if( pException && pException->iHasFinally ){` |
|        3 | 14361 | `			pException->iFinallyDone = 1;` |
|        3 | 14362 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 | 14363 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 14364 | `				return SXERR_ABORT;` |
|        - | 14365 | `			}` |
|        1 | 14366 | `		}` |
|        - | 14367 | `		/* Check if there is an outer exception handler on the stack */` |
|      524 | 14368 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 14369 | `			/* Re-throw to the outer handler */` |
|        3 | 14370 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 14371 | `		}` |
|        - | 14372 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 14373 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 14374 | `		 * exception instead of reporting it uncaught.` |
|        - | 14375 | `		 */` |
|      522 | 14376 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 14377 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 14378 | `			 * by looking for a catch frame on the stack.` |
|        - | 14379 | `			 */` |
|      522 | 14380 | `			VmFrame *pF = pVm->pFrame;` |
|      522 | 14381 | `			int inCatch = 0;` |
|     1050 | 14382 | `			while( pF ){` |
|      538 | 14383 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        9 | 14384 | `					inCatch = 1;` |
|        9 | 14385 | `					break;` |
|        - | 14386 | `				}` |
|      529 | 14387 | `				pF = pF->pParent;` |
|        1 | 14388 | `			}` |
|      522 | 14389 | `			if( inCatch ){` |
|        - | 14390 | `				/* Defer — will be re-thrown after finally runs */` |
|        9 | 14391 | `				pThis->iRef++;` |
|        9 | 14392 | `				pVm->pPendingException = pThis;` |
|        9 | 14393 | `				return SXRET_OK;` |
|        - | 14394 | `			}` |
|      256 | 14395 | `		}` |
|        - | 14396 | `		/* Truly uncaught */` |
|      513 | 14397 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      513 | 14398 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 14399 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 14400 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 14401 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 14402 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 14403 | `			}` |
|      ! 0 | 14404 | `		}` |
|      513 | 14405 | `		return rc;` |
|      ! 0 | 14406 | `	}else{` |
|      358 | 14407 | `		VmFrame *pFrame = pVm->pFrame;` |
|      358 | 14408 | `		ph7_exception **apSaved = 0;` |
|        - | 14409 | `		sxu32 nSavedCount;` |
|        - | 14410 | `		sxi32 rc;` |
|      358 | 14411 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      358 | 14412 | `		if( pException->pFrame == pFrame ){` |
|      242 | 14413 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      120 | 14414 | `		}` |
|        - | 14415 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 14416 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 14417 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 14418 | `		 */` |
|      358 | 14419 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|      358 | 14420 | `		if( nSavedCount > 0 ){` |
|       16 | 14421 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        5 | 14422 | `				nSavedCount * sizeof(ph7_exception *));` |
|       11 | 14423 | `			if( apSaved ){` |
|       16 | 14424 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        5 | 14425 | `					nSavedCount * sizeof(ph7_exception *));` |
|       11 | 14426 | `				SySetReset(&pVm->aException);` |
|        5 | 14427 | `			}` |
|        5 | 14428 | `		}` |
|        - | 14429 | `		/* Create a private frame first */` |
|      358 | 14430 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|      358 | 14431 | `		if( rc == SXRET_OK ){` |
|      358 | 14432 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|      358 | 14433 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|      358 | 14434 | `			if( pObj ){` |
|      358 | 14435 | `				pThis->iRef++;` |
|      358 | 14436 | `				pObj->x.pOther = pThis;` |
|      358 | 14437 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      178 | 14438 | `			}` |
|        - | 14439 | `			/* Execute the catch block */` |
|      358 | 14440 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - | 14441 | `			/* Leave the frame */` |
|      358 | 14442 | `			VmLeaveFrame(&(*pVm));` |
|      178 | 14443 | `		}` |
|        - | 14444 | `		/* Restore the outer exception handlers */` |
|      358 | 14445 | `		if( apSaved ){` |
|        - | 14446 | `			sxu32 k;` |
|        - | 14447 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 14448 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 14449 | `			 * Restore the original outer entries.` |
|        - | 14450 | `			 */` |
|       11 | 14451 | `			SySetReset(&pVm->aException);` |
|       21 | 14452 | `			for(k = 0; k < nSavedCount; k++){` |
|       11 | 14453 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        6 | 14454 | `			}` |
|       11 | 14455 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        5 | 14456 | `		}` |
|        - | 14457 | `		/* Execute the finally block after catch */` |
|      358 | 14458 | `		if( pException->iHasFinally ){` |
|       16 | 14459 | `			pException->iFinallyDone = 1;` |
|        - | 14460 | `			{` |
|       16 | 14461 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       16 | 14462 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 14463 | `					return SXERR_ABORT;` |
|        - | 14464 | `				}` |
|        - | 14465 | `			}` |
|        7 | 14466 | `		}` |
|      358 | 14467 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 14468 | `			return SXERR_ABORT;` |
|        - | 14469 | `		}` |
|        - | 14470 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 14471 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 14472 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - | 14473 | `		 */` |
|      358 | 14474 | `		if( pVm->pPendingException ){` |
|        9 | 14475 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        9 | 14476 | `			pVm->pPendingException = 0;` |
|        9 | 14477 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - | 14478 | `		}` |
|        - | 14479 | `	}` |
|        - | 14480 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 14481 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 14482 | `	 */` |
|      350 | 14483 | `	return SXRET_OK;` |
|      441 | 14484 |  |
|        - | 14485 | `/*` |
|        - | 14486 | ` * Section:` |
|        - | 14487 | ` *  Version,Credits and Copyright related functions.` |
|        - | 14488 | ` * Status:` |
|        - | 14489 | ` *    Stable.` |
|        - | 14490 | ` */` |
|        - | 14491 | `/*` |
|        - | 14492 | ` * string ph7version(void)` |
|        - | 14493 | ` *  Returns the running version of the PH7 version.` |
|        - | 14494 | ` * Parameters` |
|        - | 14495 | ` *  None` |
|        - | 14496 | ` * Return` |
|        - | 14497 | ` * Current PH7 version.` |
|        - | 14498 | ` */` |
|        2 | 14499 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14500 |  |
|        1 | 14501 | `	SXUNUSED(nArg);` |
|        1 | 14502 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 14503 | `	/* Current engine version */` |
|        3 | 14504 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 14505 | `	return PH7_OK;` |
|        1 | 14506 |  |
|        - | 14507 | `/*` |
|        - | 14508 | ` * string phpversion([ string $extension ])` |
|        - | 14509 | ` *  Returns the PHP-compatibility version PHL advertises (see PHP_COMPAT_VERSION).` |
|        - | 14510 | ` * Parameters` |
|        - | 14511 | ` *  $extension (optional): an extension name. PHL has no extension registry, so any` |
|        - | 14512 | ` *  argument yields NULL (PHP returns FALSE for an unknown extension).` |
|        - | 14513 | ` * Return` |
|        - | 14514 | ` *  The PHP-compat version string, or NULL when called with an extension argument.` |
|        - | 14515 | ` */` |
|        4 | 14516 | `static int vm_builtin_phpversion(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14517 |  |
|        2 | 14518 | `	SXUNUSED(apArg); /* cc warning */` |
|        5 | 14519 | `	if( nArg > 0 ){` |
|      ! 0 | 14520 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14521 | `		return PH7_OK;` |
|        - | 14522 | `	}` |
|        5 | 14523 | `	ph7_result_string(pCtx,PHP_COMPAT_VERSION,(int)sizeof(PHP_COMPAT_VERSION) - 1);` |
|        5 | 14524 | `	return PH7_OK;` |
|        3 | 14525 |  |
|        - | 14526 | `/*` |
|        - | 14527 | ` * string php_sapi_name(void)` |
|        - | 14528 | ` *  Returns the type of interface (SAPI) PHL is running under.` |
|        - | 14529 | ` * Parameters` |
|        - | 14530 | ` *  None` |
|        - | 14531 | ` * Return` |
|        - | 14532 | ` *  "cli-server" while serving an HTTP request via the built-in -S server, "cli" otherwise.` |
|        - | 14533 | ` */` |
|        2 | 14534 | `static int vm_builtin_php_sapi_name(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14535 |  |
|        3 | 14536 | `	const char *zSapi = pCtx->pVm->bHttpContext ? "cli-server" : "cli";` |
|        1 | 14537 | `	SXUNUSED(nArg);` |
|        1 | 14538 | `	SXUNUSED(apArg); /* cc warning */` |
|        3 | 14539 | `	ph7_result_string(pCtx,zSapi,-1);` |
|        3 | 14540 | `	return PH7_OK;` |
|        1 | 14541 |  |
|        - | 14542 | `/*` |
|        - | 14543 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 14544 | ` */` |
|        - | 14545 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 14546 | ` "<html><head>"\` |
|        - | 14547 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 14548 | ` "<style type=\"text/css\">"\` |
|        - | 14549 | ` "div {"\` |
|        - | 14550 | `     "border: 1px solid #cccccc;"\` |
|        - | 14551 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 14552 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 14553 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 14554 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 14555 | `     "-webkit-border-radius: 10px;"\` |
|        - | 14556 | `     "-o-border-radius: 10px;"\` |
|        - | 14557 | `     "border-radius: 10px;"\` |
|        - | 14558 | `     "padding-left: 2em;"\` |
|        - | 14559 | `     "background-color: white;"\` |
|        - | 14560 | `     "margin-left: auto;"\` |
|        - | 14561 | `     "font-family: verdana;"\` |
|        - | 14562 | `     "padding-right: 2em;"\` |
|        - | 14563 | `     "margin-right: auto;"\` |
|        - | 14564 | `     "}"\` |
|        - | 14565 | `     "body {"\` |
|        - | 14566 | `     "padding: 0.2em;"\` |
|        - | 14567 | `     "font-style: normal;"\` |
|        - | 14568 | `     "font-size: medium;"\` |
|        - | 14569 | `     "background-color: #f2f2f2;"\` |
|        - | 14570 | `     "}"\` |
|        - | 14571 | `     "hr {"\` |
|        - | 14572 | `     "border-style: solid none none;"\` |
|        - | 14573 | `     "border-width: 1px medium medium;"\` |
|        - | 14574 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 14575 | `     "height: 1px;"\` |
|        - | 14576 | `     "}"\` |
|        - | 14577 | `     "a {"\` |
|        - | 14578 | `     "color: #3366cc;"\` |
|        - | 14579 | `     "text-decoration: none;"\` |
|        - | 14580 | `     "}"\` |
|        - | 14581 | `     "a:hover {"\` |
|        - | 14582 | `     "color: #999999;"\` |
|        - | 14583 | `     "}"\` |
|        - | 14584 | `     "a:active {"\` |
|        - | 14585 | `     "color: #663399;"\` |
|        - | 14586 | `     "}"\` |
|        - | 14587 | `     "h1 {"\` |
|        - | 14588 | `     "margin: 0;"\` |
|        - | 14589 | `     "padding: 0;"\` |
|        - | 14590 | `     "font-family: Verdana;"\` |
|        - | 14591 | `     "font-weight: bold;"\` |
|        - | 14592 | `     "font-style: normal;"\` |
|        - | 14593 | `     "font-size: medium;"\` |
|        - | 14594 | `     "text-transform: capitalize;"\` |
|        - | 14595 | `     "color: #0a328c;"\` |
|        - | 14596 | `     "}"\` |
|        - | 14597 | `     "p {"\` |
|        - | 14598 | `     "margin: 0 auto;"\` |
|        - | 14599 | `     "font-size: medium;"\` |
|        - | 14600 | `     "font-style: normal;"\` |
|        - | 14601 | `     "font-family: verdana;"\` |
|        - | 14602 | `     "}"\` |
|        - | 14603 | `"</style></head><body>"\` |
|        - | 14604 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 14605 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 14606 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 14607 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 14608 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 14609 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 14610 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 14611 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 14612 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 14613 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 14614 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 14615 |  |
|        - | 14616 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 14617 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 14618 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 14619 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 14620 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 14621 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 14622 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 14623 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 14624 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 14625 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 14626 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 14627 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 14628 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 14629 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 14630 |  |
|        - | 14631 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 14632 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 14633 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 14634 | `"&nbsp;*<br>"\` |
|        - | 14635 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 14636 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 14637 | `"&nbsp;* are met:<br>"\` |
|        - | 14638 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 14639 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 14640 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 14641 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 14642 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 14643 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 14644 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 14645 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 14646 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 14647 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 14648 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 14649 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 14650 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 14651 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 14652 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 14653 | `"&nbsp;*<br>"\` |
|        - | 14654 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 14655 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 14656 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 14657 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 14658 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 14659 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 14660 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 14661 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 14662 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 14663 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 14664 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 14665 | `"&nbsp;*/<br>"\` |
|        - | 14666 | `"</span></small></small></p>"\` |
|        - | 14667 | `"</div></body></html>"` |
|        - | 14668 | `/*` |
|        - | 14669 | ` * bool ph7credits(void)` |
|        - | 14670 | ` * bool ph7info(void)` |
|        - | 14671 | ` * bool ph7copyright(void)` |
|        - | 14672 | ` *  Prints out the credits for PH7 engine` |
|        - | 14673 | ` * Parameters` |
|        - | 14674 | ` *  None` |
|        - | 14675 | ` * Return` |
|        - | 14676 | ` *  Always TRUE` |
|        - | 14677 | ` */` |
|        2 | 14678 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14679 |  |
|        3 | 14680 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 14681 | `	/* Expand the HTML page above*/` |
|        3 | 14682 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 14683 | `	ph7_context_output_format(` |
|        1 | 14684 | `		pCtx,` |
|        - | 14685 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 14686 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 14687 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 14688 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 14689 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 14690 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 14691 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 14692 | `#ifdef __WINNT__` |
|        - | 14693 | `		"Windows NT"` |
|        - | 14694 | `#elif defined(__UNIXES__)` |
|        - | 14695 | `		"UNIX-Like"` |
|        - | 14696 | `#else` |
|        - | 14697 | `		"Other OS"` |
|        - | 14698 | `#endif` |
|        - | 14699 | `		);` |
|        3 | 14700 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 14701 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 14702 | `	SXUNUSED(apArg);` |
|        - | 14703 | `	/* Return TRUE */` |
|        - | 14704 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 14705 | `	return PH7_OK;` |
|        1 | 14706 |  |
|        - | 14707 | `/*` |
|        - | 14708 | ` * Section:` |
|        - | 14709 | ` *    URL related routines.` |
|        - | 14710 | ` * Status:` |
|        - | 14711 | ` *    Stable.` |
|        - | 14712 | ` */` |
|        - | 14713 | `/*` |
|        - | 14714 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 14715 | ` *  Parse a URL and return its fields.` |
|        - | 14716 | ` * Parameters` |
|        - | 14717 | ` *  $url` |
|        - | 14718 | ` *   The URL to parse.` |
|        - | 14719 | ` * $component` |
|        - | 14720 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 14721 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 14722 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 14723 | ` *  in which case the return value will be an integer).` |
|        - | 14724 | ` * Return` |
|        - | 14725 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 14726 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 14727 | ` *  this array are:` |
|        - | 14728 | ` *   scheme - e.g. http` |
|        - | 14729 | ` *   host` |
|        - | 14730 | ` *   port` |
|        - | 14731 | ` *   user` |
|        - | 14732 | ` *   pass` |
|        - | 14733 | ` *   path` |
|        - | 14734 | ` *   query - after the question mark ?` |
|        - | 14735 | ` *   fragment - after the hashmark #` |
|        - | 14736 | ` * Note:` |
|        - | 14737 | ` *  FALSE is returned on failure.` |
|        - | 14738 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 14739 | ` *  with the standard PHP engine.` |
|        - | 14740 | ` */` |
|       28 | 14741 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14742 |  |
|        - | 14743 | `	const char *zStr; /* Input string */` |
|        - | 14744 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 14745 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 14746 | `	int nLen;` |
|        - | 14747 | `	sxi32 rc;` |
|       29 | 14748 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 14749 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 14750 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14751 | `		return PH7_OK;` |
|        - | 14752 | `	}` |
|        - | 14753 | `	/* Extract the given URI */` |
|       29 | 14754 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 14755 | `	if( nLen < 1 ){` |
|        - | 14756 | `		/* Nothing to process,return FALSE */` |
|        3 | 14757 | `		ph7_result_bool(pCtx,0);` |
|        3 | 14758 | `		return PH7_OK;` |
|        - | 14759 | `	}` |
|        - | 14760 | `	/* Get a parse */` |
|       27 | 14761 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 14762 | `	if( rc != SXRET_OK ){` |
|        - | 14763 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 14764 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14765 | `		return PH7_OK;` |
|        - | 14766 | `	}` |
|       27 | 14767 | `	if( nArg > 1 ){` |
|      ! 0 | 14768 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 14769 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 14770 | `		switch(nComponent){` |
|      ! 0 | 14771 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 14772 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 14773 | `			if( pComp->nByte < 1 ){` |
|        - | 14774 | `				/* No available value,return NULL */` |
|      ! 0 | 14775 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14776 | `			}else{` |
|      ! 0 | 14777 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14778 | `			}` |
|      ! 0 | 14779 | `			break;` |
|      ! 0 | 14780 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 14781 | `			pComp = &sURI.sHost;` |
|      ! 0 | 14782 | `			if( pComp->nByte < 1 ){` |
|        - | 14783 | `				/* No available value,return NULL */` |
|      ! 0 | 14784 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14785 | `			}else{` |
|      ! 0 | 14786 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14787 | `			}` |
|      ! 0 | 14788 | `			break;` |
|      ! 0 | 14789 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 14790 | `			pComp = &sURI.sPort;` |
|      ! 0 | 14791 | `			if( pComp->nByte < 1 ){` |
|        - | 14792 | `				/* No available value,return NULL */` |
|      ! 0 | 14793 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14794 | `			}else{` |
|      ! 0 | 14795 | `				int iPort = 0;` |
|        - | 14796 | `				/* Cast the value to integer */` |
|      ! 0 | 14797 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 14798 | `				ph7_result_int(pCtx,iPort);` |
|        - | 14799 | `			}` |
|      ! 0 | 14800 | `			break;` |
|      ! 0 | 14801 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 14802 | `			pComp = &sURI.sUser;` |
|      ! 0 | 14803 | `			if( pComp->nByte < 1 ){` |
|        - | 14804 | `				/* No available value,return NULL */` |
|      ! 0 | 14805 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14806 | `			}else{` |
|      ! 0 | 14807 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14808 | `			}` |
|      ! 0 | 14809 | `			break;` |
|      ! 0 | 14810 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 14811 | `			pComp = &sURI.sPass;` |
|      ! 0 | 14812 | `			if( pComp->nByte < 1 ){` |
|        - | 14813 | `				/* No available value,return NULL */` |
|      ! 0 | 14814 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14815 | `			}else{` |
|      ! 0 | 14816 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14817 | `			}` |
|      ! 0 | 14818 | `			break;` |
|      ! 0 | 14819 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 14820 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 14821 | `			if( pComp->nByte < 1 ){` |
|        - | 14822 | `				/* No available value,return NULL */` |
|      ! 0 | 14823 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14824 | `			}else{` |
|      ! 0 | 14825 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14826 | `			}` |
|      ! 0 | 14827 | `			break;` |
|      ! 0 | 14828 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 14829 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 14830 | `			if( pComp->nByte < 1 ){` |
|        - | 14831 | `				/* No available value,return NULL */` |
|      ! 0 | 14832 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14833 | `			}else{` |
|      ! 0 | 14834 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14835 | `			}` |
|      ! 0 | 14836 | `			break;` |
|      ! 0 | 14837 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 14838 | `			pComp = &sURI.sPath;` |
|      ! 0 | 14839 | `			if( pComp->nByte < 1 ){` |
|        - | 14840 | `				/* No available value,return NULL */` |
|      ! 0 | 14841 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14842 | `			}else{` |
|      ! 0 | 14843 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14844 | `			}` |
|      ! 0 | 14845 | `			break;` |
|      ! 0 | 14846 | `		default:` |
|        - | 14847 | `			/* No such entry,return NULL */` |
|      ! 0 | 14848 | `			ph7_result_null(pCtx);` |
|      ! 0 | 14849 | `			break;` |
|        - | 14850 | `		}` |
|      ! 0 | 14851 | `	}else{` |
|        - | 14852 | `		ph7_value *pArray,*pValue;` |
|        - | 14853 | `		/* Return an associative array */` |
|       27 | 14854 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 14855 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 14856 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 14857 | `			/* Out of memory */` |
|      ! 0 | 14858 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 14859 | `			/* Return false */` |
|      ! 0 | 14860 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 14861 | `			return PH7_OK;` |
|        - | 14862 | `		}` |
|        - | 14863 | `		/* Fill the array */` |
|       27 | 14864 | `		pComp = &sURI.sScheme;` |
|       27 | 14865 | `		if( pComp->nByte > 0 ){` |
|       19 | 14866 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 14867 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 14868 | `		}` |
|        - | 14869 | `		/* Reset the string cursor */` |
|       27 | 14870 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14871 | `		pComp = &sURI.sHost;` |
|       27 | 14872 | `		if( pComp->nByte > 0 ){` |
|       25 | 14873 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 14874 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 14875 | `		}` |
|        - | 14876 | `		/* Reset the string cursor */` |
|       27 | 14877 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14878 | `		pComp = &sURI.sPort;` |
|       27 | 14879 | `		if( pComp->nByte > 0 ){` |
|       11 | 14880 | `			int iPort = 0;/* cc warning */` |
|        - | 14881 | `			/* Convert to integer */` |
|       11 | 14882 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 14883 | `			ph7_value_int(pValue,iPort);` |
|       11 | 14884 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 14885 | `		}` |
|        - | 14886 | `		/* Reset the string cursor */` |
|       27 | 14887 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14888 | `		pComp = &sURI.sUser;` |
|       27 | 14889 | `		if( pComp->nByte > 0 ){` |
|        7 | 14890 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 14891 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 14892 | `		}` |
|        - | 14893 | `		/* Reset the string cursor */` |
|       27 | 14894 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14895 | `		pComp = &sURI.sPass;` |
|       27 | 14896 | `		if( pComp->nByte > 0 ){` |
|        7 | 14897 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 14898 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 14899 | `		}` |
|        - | 14900 | `		/* Reset the string cursor */` |
|       27 | 14901 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14902 | `		pComp = &sURI.sPath;` |
|       27 | 14903 | `		if( pComp->nByte > 0 ){` |
|       17 | 14904 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 14905 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 14906 | `		}` |
|        - | 14907 | `		/* Reset the string cursor */` |
|       27 | 14908 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14909 | `		pComp = &sURI.sQuery;` |
|       27 | 14910 | `		if( pComp->nByte > 0 ){` |
|        5 | 14911 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 14912 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 14913 | `		}` |
|        - | 14914 | `		/* Reset the string cursor */` |
|       27 | 14915 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14916 | `		pComp = &sURI.sFragment;` |
|       27 | 14917 | `		if( pComp->nByte > 0 ){` |
|        5 | 14918 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 14919 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 14920 | `		}` |
|        - | 14921 | `		/* Return the created array */` |
|       27 | 14922 | `		ph7_result_value(pCtx,pArray);` |
|        - | 14923 | `		/* NOTE:` |
|        - | 14924 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 14925 | `		 * automatically as soon we return from this function.` |
|        - | 14926 | `		 */` |
|        - | 14927 | `	}` |
|        - | 14928 | `	/* All done */` |
|       27 | 14929 | `	return PH7_OK;` |
|       15 | 14930 |  |
|        - | 14931 | `/*` |
|        - | 14932 | ` * Section:` |
|        - | 14933 | ` *   Array related routines.` |
|        - | 14934 | ` * Status:` |
|        - | 14935 | ` *    Stable.` |
|        - | 14936 | ` * Note 2012-5-21 01:04:15:` |
|        - | 14937 | ` *  Array related functions that need access to the underlying` |
|        - | 14938 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 14939 | ` */` |
|        - | 14940 | `/*` |
|        - | 14941 | ` * The [compact()] function store it's state information in an instance` |
|        - | 14942 | ` * of the following structure.` |
|        - | 14943 | ` */` |
|        - | 14944 | `struct compact_data` |
|        - | 14945 |  |
|        - | 14946 | `	ph7_value *pArray;  /* Target array */` |
|        - | 14947 | `	int nRecCount;      /* Recursion count */` |
|        - | 14948 | `};` |
|        - | 14949 | `/*` |
|        - | 14950 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 14951 | ` */` |
|      ! 0 | 14952 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 14953 |  |
|      ! 0 | 14954 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 14955 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 14956 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 14957 | `	/* Act according to the hashmap value */` |
|      ! 0 | 14958 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 14959 | `		SyString sVar;` |
|      ! 0 | 14960 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 14961 | `		if( sVar.nByte > 0 ){` |
|        - | 14962 | `			/* Query the current frame */` |
|      ! 0 | 14963 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 14964 | `			/* ^` |
|        - | 14965 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 14966 | `			 */` |
|      ! 0 | 14967 | `			if( pKey ){` |
|        - | 14968 | `				/* Perform the insertion */` |
|      ! 0 | 14969 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 14970 | `			}` |
|      ! 0 | 14971 | `		}` |
|      ! 0 | 14972 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 14973 | `		int rc;` |
|        - | 14974 | `		/* Recursively traverse this array */` |
|      ! 0 | 14975 | `		pData->nRecCount++;` |
|      ! 0 | 14976 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 14977 | `		pData->nRecCount--;` |
|      ! 0 | 14978 | `		return rc;` |
|        - | 14979 | `	}` |
|      ! 0 | 14980 | `	return SXRET_OK;` |
|      ! 0 | 14981 |  |
|        - | 14982 | `/*` |
|        - | 14983 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 14984 | ` *  Create array containing variables and their values.` |
|        - | 14985 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 14986 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 14987 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 14988 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 14989 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 14990 | ` * Parameters` |
|        - | 14991 | ` *  $varname` |
|        - | 14992 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 14993 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 14994 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 14995 | ` *   it recursively.` |
|        - | 14996 | ` * Return` |
|        - | 14997 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 14998 | ` */` |
|        2 | 14999 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15000 |  |
|        - | 15001 | `	ph7_value *pArray,*pObj;` |
|        3 | 15002 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 15003 | `	const char *zName;` |
|        - | 15004 | `	SyString sVar;` |
|        - | 15005 | `	int i,nLen;` |
|        3 | 15006 | `	if( nArg < 1 ){` |
|        - | 15007 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 15008 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15009 | `		return PH7_OK;` |
|        - | 15010 | `	}` |
|        - | 15011 | `	/* Create the array */` |
|        3 | 15012 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 15013 | `	if( pArray == 0 ){` |
|        - | 15014 | `		/* Out of memory */` |
|      ! 0 | 15015 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 15016 | `		/* Return NULL */` |
|      ! 0 | 15017 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15018 | `		return PH7_OK;` |
|        - | 15019 | `	}` |
|        - | 15020 | `	/* Perform the requested operation */` |
|        7 | 15021 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 15022 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 15023 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 15024 | `				struct compact_data sData;` |
|      ! 0 | 15025 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 15026 | `				/* Recursively walk the array */` |
|      ! 0 | 15027 | `				sData.nRecCount = 0;` |
|      ! 0 | 15028 | `				sData.pArray = pArray;` |
|      ! 0 | 15029 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 15030 | `			}` |
|      ! 0 | 15031 | `		}else{` |
|        - | 15032 | `			/* Extract variable name */` |
|        5 | 15033 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 15034 | `			if( nLen > 0 ){` |
|        5 | 15035 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 15036 | `				/* Check if the variable is available in the current frame */` |
|        5 | 15037 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 15038 | `				if( pObj ){` |
|        5 | 15039 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 15040 | `				}` |
|        2 | 15041 | `			}` |
|        - | 15042 | `		}` |
|        3 | 15043 | `	}` |
|        - | 15044 | `	/* Return the array */` |
|        3 | 15045 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 15046 | `	return PH7_OK;` |
|        2 | 15047 |  |
|        - | 15048 | `/*` |
|        - | 15049 | ` * The [extract()] function store it's state information in an instance` |
|        - | 15050 | ` * of the following structure.` |
|        - | 15051 | ` */` |
|        - | 15052 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 15053 | `struct extract_aux_data` |
|        - | 15054 |  |
|        - | 15055 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 15056 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 15057 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 15058 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 15059 | `	int iFlags;           /* Control flags */` |
|        - | 15060 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 15061 | `};` |
|        - | 15062 | `/* Forward declaration */` |
|        - | 15063 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 15064 | `/*` |
|        - | 15065 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 15066 | ` *   Import variables into the current symbol table from an array.` |
|        - | 15067 | ` * Parameters` |
|        - | 15068 | ` * $var_array` |
|        - | 15069 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 15070 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 15071 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 15072 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 15073 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 15074 | ` * $extract_type` |
|        - | 15075 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 15076 | ` *  It can be one of the following values:` |
|        - | 15077 | ` *   EXTR_OVERWRITE` |
|        - | 15078 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 15079 | ` *   EXTR_SKIP` |
|        - | 15080 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 15081 | ` *   EXTR_PREFIX_SAME` |
|        - | 15082 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 15083 | ` *   EXTR_PREFIX_ALL` |
|        - | 15084 | ` *       Prefix all variable names with prefix.` |
|        - | 15085 | ` *   EXTR_PREFIX_INVALID` |
|        - | 15086 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 15087 | ` *   EXTR_IF_EXISTS` |
|        - | 15088 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 15089 | ` *       otherwise do nothing.` |
|        - | 15090 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 15091 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 15092 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 15093 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 15094 | ` *      the current symbol table.` |
|        - | 15095 | ` * $prefix` |
|        - | 15096 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 15097 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 15098 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 15099 | ` *  underscore character.` |
|        - | 15100 | ` * Return` |
|        - | 15101 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 15102 | ` */` |
|        4 | 15103 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15104 |  |
|        - | 15105 | `	extract_aux_data sAux;` |
|        - | 15106 | `	ph7_hashmap *pMap;` |
|        5 | 15107 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 15108 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 15109 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 15110 | `		return PH7_OK;` |
|        - | 15111 | `	}` |
|        - | 15112 | `	/* Point to the target hashmap */` |
|        5 | 15113 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 15114 | `	if( pMap->nEntry < 1 ){` |
|        - | 15115 | `		/* Empty map,return  0 */` |
|      ! 0 | 15116 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 15117 | `		return PH7_OK;` |
|        - | 15118 | `	}` |
|        - | 15119 | `	/* Prepare the aux data */` |
|        5 | 15120 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 15121 | `	if( nArg > 1 ){` |
|        3 | 15122 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 15123 | `		if( nArg > 2 ){` |
|      ! 0 | 15124 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 15125 | `		}` |
|        1 | 15126 | `	}` |
|        5 | 15127 | `	sAux.pVm = pCtx->pVm;` |
|        - | 15128 | `	/* Invoke the worker callback */` |
|        5 | 15129 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 15130 | `	/* Number of variables successfully imported */` |
|        5 | 15131 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 15132 | `	return PH7_OK;` |
|        3 | 15133 |  |
|        - | 15134 | `/*` |
|        - | 15135 | ` * Worker callback for the [extract()] function defined` |
|        - | 15136 | ` * below.` |
|        - | 15137 | ` */` |
|        8 | 15138 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 15139 |  |
|        9 | 15140 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 15141 | `	int iFlags = pAux->iFlags;` |
|        9 | 15142 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 15143 | `	ph7_value *pObj;` |
|        - | 15144 | `	SyString sVar;` |
|        9 | 15145 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 15146 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 15147 | `	}` |
|        - | 15148 | `	/* Perform a string cast */` |
|        9 | 15149 | `	PH7_MemObjToString(pKey);` |
|        9 | 15150 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 15151 | `		/* Unavailable variable name */` |
|      ! 0 | 15152 | `		return SXRET_OK;` |
|        - | 15153 | `	}` |
|        9 | 15154 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 15155 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 15156 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 15157 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 15158 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 15159 | `			);` |
|      ! 0 | 15160 | `	}else{` |
|       13 | 15161 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 15162 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 15163 | `	}` |
|        9 | 15164 | `	sVar.zString = pAux->zWorker;` |
|        - | 15165 | `	/* Try to extract the variable */` |
|        9 | 15166 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 15167 | `	if( pObj ){` |
|        - | 15168 | `		/* Collision */` |
|        5 | 15169 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 15170 | `			return SXRET_OK;` |
|        - | 15171 | `		}` |
|        5 | 15172 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 15173 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 15174 | `				/* Already prefixed */` |
|      ! 0 | 15175 | `				return SXRET_OK;` |
|        - | 15176 | `			}` |
|      ! 0 | 15177 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 15178 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 15179 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 15180 | `				);` |
|      ! 0 | 15181 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 15182 | `		}` |
|        3 | 15183 | `	}else{` |
|        - | 15184 | `		/* Create the variable */` |
|        5 | 15185 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 15186 | `	}` |
|        9 | 15187 | `	if( pObj ){` |
|        - | 15188 | `		/* Overwrite the old value */` |
|        9 | 15189 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 15190 | `		/* Increment counter */` |
|        9 | 15191 | `		pAux->iCount++;` |
|        4 | 15192 | `	}` |
|        9 | 15193 | `	return SXRET_OK;` |
|        5 | 15194 |  |
|        - | 15195 | `/*` |
|        - | 15196 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 15197 | ` * defined below.` |
|        - | 15198 | ` */` |
|        2 | 15199 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 15200 |  |
|        3 | 15201 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 15202 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 15203 | `	ph7_value *pObj;` |
|        - | 15204 | `	SyString sVar;` |
|        - | 15205 | `	/* Perform a string cast */` |
|        3 | 15206 | `	PH7_MemObjToString(pKey);` |
|        3 | 15207 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 15208 | `		/* Unavailable variable name */` |
|      ! 0 | 15209 | `		return SXRET_OK;` |
|        - | 15210 | `	}` |
|        3 | 15211 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 15212 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 15213 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 15214 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 15215 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 15216 | `			);` |
|        2 | 15217 | `	}else{` |
|      ! 0 | 15218 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 15219 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 15220 | `	}` |
|        3 | 15221 | `	sVar.zString = pAux->zWorker;` |
|        - | 15222 | `	/* Extract the variable */` |
|        3 | 15223 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 15224 | `	if( pObj ){` |
|        3 | 15225 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 15226 | `	}` |
|        3 | 15227 | `	return SXRET_OK;` |
|        2 | 15228 |  |
|        - | 15229 | `/*` |
|        - | 15230 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 15231 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 15232 | ` * Parameters` |
|        - | 15233 | ` * $types` |
|        - | 15234 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 15235 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 15236 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 15237 | ` *  POST includes the POST uploaded file information.` |
|        - | 15238 | ` *  Note:` |
|        - | 15239 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 15240 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 15241 | ` * $prefix` |
|        - | 15242 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 15243 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 15244 | ` *  variable named $pref_userid.` |
|        - | 15245 | ` * Return` |
|        - | 15246 | ` *  TRUE on success or FALSE on failure.` |
|        - | 15247 | ` */` |
|        2 | 15248 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15249 |  |
|        - | 15250 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 15251 | `	extract_aux_data sAux;` |
|        - | 15252 | `	int nLen,nPrefixLen;` |
|        - | 15253 | `	ph7_value *pSuper;` |
|        - | 15254 | `	ph7_vm *pVm;` |
|        - | 15255 | `	/* By default import only $_GET variables  */` |
|        3 | 15256 | `	zImport = "G";` |
|        3 | 15257 | `	nLen = (int)sizeof(char);` |
|        3 | 15258 | `	zPrefix = 0;` |
|        3 | 15259 | `	nPrefixLen = 0;` |
|        3 | 15260 | `	if( nArg > 0 ){` |
|        3 | 15261 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 15262 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 15263 | `		}` |
|        3 | 15264 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 15265 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 15266 | `		}` |
|        1 | 15267 | `	}` |
|        - | 15268 | `	/* Point to the underlying VM */` |
|        3 | 15269 | `	pVm = pCtx->pVm;` |
|        - | 15270 | `	/* Initialize the aux data */` |
|        3 | 15271 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 15272 | `	sAux.zPrefix = zPrefix;` |
|        3 | 15273 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 15274 | `	sAux.pVm = pVm;` |
|        - | 15275 | `	/* Extract */` |
|        3 | 15276 | `	zEnd = &zImport[nLen];` |
|        5 | 15277 | `	while( zImport < zEnd ){` |
|        3 | 15278 | `		int c = zImport[0];` |
|        3 | 15279 | `		pSuper = 0;` |
|        3 | 15280 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 15281 | `			/* Import $_GET variables */` |
|        3 | 15282 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 15283 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 15284 | `			/* Import $_POST variables */` |
|      ! 0 | 15285 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 15286 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 15287 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 15288 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 15289 | `		}` |
|        3 | 15290 | `		if( pSuper ){` |
|        - | 15291 | `			/* Iterate throw array entries */` |
|        3 | 15292 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 15293 | `		}` |
|        - | 15294 | `		/* Advance the cursor */` |
|        3 | 15295 | `		zImport++;` |
|        1 | 15296 | `	}` |
|        - | 15297 | `	/* All done,return TRUE*/` |
|        3 | 15298 | `	ph7_result_bool(pCtx,0);` |
|        3 | 15299 | `	return PH7_OK;` |
|        1 | 15300 |  |
|        - | 15301 | `/*` |
|        - | 15302 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 15303 | ` * Refer to the eval() language construct implementation for more` |
|        - | 15304 | ` * information.` |
|        - | 15305 | ` */` |
|    12838 | 15306 | `static sxi32 VmEvalChunk(` |
|        - | 15307 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 15308 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 15309 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 15310 | `	int iFlags,         /* Compile flag */` |
|        - | 15311 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 15312 | `	)` |
|        2 | 15313 |  |
|        - | 15314 | `	SySet *pByteCode,aByteCode;` |
|        - | 15315 | `	SyBlob sSavedNs;` |
|    12840 | 15316 | `	ProcConsumer xErr = 0;` |
|    12840 | 15317 | `	void *pErrData = 0;` |
|        - | 15318 | `	/* Initialize bytecode container */` |
|    12840 | 15319 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    12840 | 15320 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 15321 | `	/* Reset the code generator */` |
|    12840 | 15322 | `	if( bTrueReturn ){` |
|        - | 15323 | `		/* Included file,log compile-time errors */` |
|     9636 | 15324 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     9636 | 15325 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4817 | 15326 | `	}` |
|    12840 | 15327 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 15328 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 15329 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 15330 | `	 * the caller's namespace is restored. */` |
|    12840 | 15331 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    12840 | 15332 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    12840 | 15333 | `	if( bTrueReturn ){` |
|        - | 15334 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     9636 | 15335 | `		SyBlobReset(&pVm->sNamespace);` |
|     4817 | 15336 | `	}` |
|        - | 15337 | `	/* Swap bytecode container */` |
|    12840 | 15338 | `	pByteCode = pVm->pByteContainer;` |
|    12840 | 15339 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 15340 | `	/* Compile the chunk */` |
|    12840 | 15341 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    19258 | 15342 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 15343 | `		/* Compilation error,return false */` |
|        3 | 15344 | `		if( pCtx ){` |
|        3 | 15345 | `			ph7_result_bool(pCtx,0);` |
|        1 | 15346 | `		}` |
|        2 | 15347 | `	}else{` |
|        - | 15348 | `		/* Mount any newly defined classes */` |
|        - | 15349 | `		SyHashEntry *pEntry;` |
|        - | 15350 | `		ph7_class *pClass;` |
|        - | 15351 | `		ph7_value sResult; /* Return value */` |
|        - | 15352 | `		sxi32 rc;` |
|    12838 | 15353 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   995310 | 15354 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   976056 | 15355 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 15356 | `			/* Only mount classes that haven't been mounted yet */` |
|   976056 | 15357 | `			if( !pClass->bMounted ){` |
|   247640 | 15358 | `				rc = VmMountUserClass(pVm,pClass);` |
|   247640 | 15359 | `				if( rc != SXRET_OK ){` |
|        - | 15360 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 15361 | `					if( pCtx ){` |
|      ! 0 | 15362 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 15363 | `					}` |
|      ! 0 | 15364 | `					goto Cleanup;` |
|        - | 15365 | `				}` |
|   123819 | 15366 | `			}` |
|        2 | 15367 | `		}` |
|    12838 | 15368 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 15369 | `			/* Out of memory */` |
|      ! 0 | 15370 | `			if( pCtx ){` |
|      ! 0 | 15371 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 15372 | `			}` |
|      ! 0 | 15373 | `			goto Cleanup;` |
|        - | 15374 | `		}` |
|    12838 | 15375 | `		if( bTrueReturn ){` |
|        - | 15376 | `			/* Assume a boolean true return value */` |
|     9636 | 15377 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4819 | 15378 | `		}else{` |
|        - | 15379 | `			/* Assume a null return value */` |
|     3204 | 15380 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 15381 | `		}` |
|        - | 15382 | `		/* Execute the compiled chunk. eval()/include/require recurse in C here,` |
|        - | 15383 | `		 * a path the OP_CALL cap check can't see; bound it under the same limit` |
|        - | 15384 | `		 * so a recursive include/eval can't overflow the native stack. */` |
|    12838 | 15385 | `		if( VmRecursionExceeded(pVm) ){` |
|        3 | 15386 | `			PH7_MemObjRelease(&sResult);` |
|        3 | 15387 | `			VmRecursionFatal(pVm);` |
|        3 | 15388 | `			goto Cleanup;` |
|        - | 15389 | `		}` |
|    12836 | 15390 | `		pVm->nRecursionDepth++;` |
|    12836 | 15391 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    12836 | 15392 | `		pVm->nRecursionDepth--;` |
|    12836 | 15393 | `		if( pCtx ){` |
|        - | 15394 | `			/* Set the execution result */` |
|     9688 | 15395 | `			ph7_result_value(pCtx,&sResult);` |
|     4843 | 15396 | `		}` |
|    12836 | 15397 | `		PH7_MemObjRelease(&sResult);` |
|        - | 15398 | `	}` |
|     6419 | 15399 | `Cleanup:` |
|        - | 15400 | `	/* Cleanup the mess left behind */` |
|    12840 | 15401 | `	pVm->pByteContainer = pByteCode;` |
|    12840 | 15402 | `	SySetRelease(&aByteCode);` |
|        - | 15403 | `	/* Restore caller's namespace state */` |
|    12840 | 15404 | `	SyBlobReset(&pVm->sNamespace);` |
|    12840 | 15405 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    12840 | 15406 | `	SyBlobRelease(&sSavedNs);` |
|    12840 | 15407 | `	return SXRET_OK;` |
|        2 | 15408 |  |
|        - | 15409 | `/*` |
|        - | 15410 | ` * value eval(string $code)` |
|        - | 15411 | ` *   Evaluate a string as PHP code.` |
|        - | 15412 | ` * Parameter` |
|        - | 15413 | ` *  code: PHP code to evaluate.` |
|        - | 15414 | ` * Return` |
|        - | 15415 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 15416 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 15417 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 15418 | ` */` |
|       58 | 15419 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15420 |  |
|        - | 15421 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       60 | 15422 | `	if( nArg < 1 ){` |
|        - | 15423 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15424 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15425 | `		return SXRET_OK;` |
|        - | 15426 | `	}` |
|        - | 15427 | `	/* Chunk to evaluate */` |
|       60 | 15428 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       60 | 15429 | `	if( sChunk.nByte < 1 ){` |
|        - | 15430 | `		/* Empty string,return NULL */` |
|        3 | 15431 | `		ph7_result_null(pCtx);` |
|        3 | 15432 | `		return SXRET_OK;` |
|        - | 15433 | `	}` |
|        - | 15434 | `	/* Eval the chunk */` |
|       58 | 15435 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       58 | 15436 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15437 | `		/* exit/die inside the evaluated chunk: cascade the halt */` |
|       37 | 15438 | `		return PH7_ABORT;` |
|        - | 15439 | `	}` |
|       22 | 15440 | `	return SXRET_OK;` |
|       31 | 15441 |  |
|        - | 15442 | `/*` |
|        - | 15443 | ` * Check if a file path is already included.` |
|        - | 15444 | ` */` |
|    19266 | 15445 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        2 | 15446 |  |
|        - | 15447 | `	SyString *aEntries;` |
|        - | 15448 | `	sxu32 n;` |
|    19268 | 15449 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 15450 | `	/* Perform a linear search */` |
| 92602952 | 15451 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 92583696 | 15452 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 15453 | `			/* Already included */` |
|       11 | 15454 | `			return TRUE;` |
|        - | 15455 | `		}` |
| 46291844 | 15456 | `	}` |
|    19258 | 15457 | `	return FALSE;` |
|     9635 | 15458 |  |
|        - | 15459 | `/*` |
|        - | 15460 | ` * Push a file path in the appropriate VM container.` |
|        - | 15461 | ` */` |
|    22406 | 15462 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 15463 |  |
|        - | 15464 | `	SyString sPath;` |
|        - | 15465 | `	char *zDup;` |
|        - | 15466 | `#ifdef __WINNT__` |
|        - | 15467 | `	char *zCur;` |
|        - | 15468 | `#endif` |
|        - | 15469 | `	sxi32 rc;` |
|    22408 | 15470 | `	if( nLen < 0 ){` |
|     3142 | 15471 | `		nLen = SyStrlen(zPath);` |
|     1570 | 15472 | `	}` |
|        - | 15473 | `	/* Duplicate the file path first */` |
|    22408 | 15474 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    22408 | 15475 | `	if( zDup == 0 ){` |
|      ! 0 | 15476 | `		return SXERR_MEM;` |
|        - | 15477 | `	}` |
|        - | 15478 | `#ifdef __WINNT__` |
|        - | 15479 | `	/* Normalize path on windows` |
|        - | 15480 | `	 * Example:` |
|        - | 15481 | `	 *    Path/To/File.php` |
|        - | 15482 | `	 * becomes` |
|        - | 15483 | `	 *   path\to\file.php` |
|        - | 15484 | `	 */` |
|        2 | 15485 | `	zCur = zDup;` |
|        2 | 15486 | `	while( zCur[0] != 0 ){` |
|        2 | 15487 | `		if( zCur[0] == '/' ){` |
|        2 | 15488 | `			zCur[0] = '\\';` |
|        2 | 15489 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 15490 | `			int c = SyToLower(zCur[0]);` |
|        1 | 15491 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 15492 | `		}` |
|        2 | 15493 | `		zCur++;` |
|        2 | 15494 | `	}` |
|        - | 15495 | `#endif` |
|        - | 15496 | `	/* Install the file path */` |
|    22408 | 15497 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    22408 | 15498 | `	if( !bMain ){` |
|    19268 | 15499 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 15500 | `			/* Already included */` |
|       11 | 15501 | `			*pNew = 0;` |
|        6 | 15502 | `		}else{` |
|        - | 15503 | `			/* Insert in the corresponding container */` |
|    19258 | 15504 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    19258 | 15505 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 15506 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 15507 | `				return rc;` |
|        - | 15508 | `			}` |
|    19258 | 15509 | `			*pNew = 1;` |
|        - | 15510 | `		}` |
|     9633 | 15511 | `	}` |
|    22408 | 15512 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    22408 | 15513 | `	return SXRET_OK;` |
|    11205 | 15514 |  |
|        - | 15515 | `/*` |
|        - | 15516 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 15517 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 15518 | ` * indicates failure.` |
|        - | 15519 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 15520 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 15521 | ` * operations.` |
|        - | 15522 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 15523 | ` * this function is a no-op.` |
|        - | 15524 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 15525 | ` * constructs for more information.` |
|        - | 15526 | ` */` |
|     9648 | 15527 | `static sxi32 VmExecIncludedFile(` |
|        - | 15528 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 15529 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 15530 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 15531 | `	 )` |
|        2 | 15532 |  |
|        - | 15533 | `	sxi32 rc;` |
|        - | 15534 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 15535 | `	const ph7_io_stream *pStream;` |
|        - | 15536 | `	SyBlob sContents;` |
|        - | 15537 | `	void *pHandle;` |
|        - | 15538 | `	ph7_vm *pVm;` |
|        - | 15539 | `	int isNew;` |
|        - | 15540 | `	/* Initialize fields */` |
|     9650 | 15541 | `	pVm = pCtx->pVm;` |
|     9650 | 15542 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     9650 | 15543 | `	isNew = 0;` |
|        - | 15544 | `	/* Extract the associated stream */` |
|     9650 | 15545 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 15546 | `	/*` |
|        - | 15547 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 15548 | `	 * in a read-only mode.` |
|        - | 15549 | `	 */` |
|     9650 | 15550 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     9650 | 15551 | `	if( pHandle == 0 ){` |
|        8 | 15552 | `		return SXERR_IO;` |
|        - | 15553 | `	}` |
|     9644 | 15554 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     9644 | 15555 | `	if( IncludeOnce && !isNew ){` |
|        - | 15556 | `		/* Already included */` |
|        9 | 15557 | `		rc = SXERR_EXISTS;` |
|        5 | 15558 | `	}else{` |
|        - | 15559 | `		/* Read the whole file contents */` |
|     9636 | 15560 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     9636 | 15561 | `		if( rc == SXRET_OK ){` |
|        - | 15562 | `			SyString sScript;` |
|        - | 15563 | `			/* Compile and execute the script */` |
|     9636 | 15564 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     9636 | 15565 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4817 | 15566 | `		}` |
|        - | 15567 | `	}` |
|        - | 15568 | `	/* Pop from the set of included file */` |
|     9644 | 15569 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 15570 | `	/* Close the handle */` |
|     9644 | 15571 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 15572 | `	/* Release the working buffer */` |
|     9644 | 15573 | `	SyBlobRelease(&sContents);` |
|        - | 15574 | `#else` |
|        - | 15575 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 15576 | `	SXUNUSED(pPath);` |
|        - | 15577 | `	SXUNUSED(IncludeOnce);` |
|        - | 15578 | `	rc = SXERR_IO;` |
|        - | 15579 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     9644 | 15580 | `	return rc;` |
|     4826 | 15581 |  |
|        - | 15582 | `/*` |
|        - | 15583 | ` * string get_include_path(void)` |
|        - | 15584 | ` *  Gets the current include_path configuration option.` |
|        - | 15585 | ` * Parameter` |
|        - | 15586 | ` *  None` |
|        - | 15587 | ` * Return` |
|        - | 15588 | ` *  Included paths as a string` |
|        - | 15589 | ` */` |
|        2 | 15590 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15591 |  |
|        3 | 15592 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 15593 | `	SyString *aEntry;` |
|        - | 15594 | `	int dir_sep;` |
|        - | 15595 | `	sxu32 n;` |
|        - | 15596 | `#ifdef __WINNT__` |
|        1 | 15597 | `	dir_sep = ';';` |
|        - | 15598 | `#else` |
|        - | 15599 | `	/* Assume UNIX path separator */` |
|        2 | 15600 | `	dir_sep = ':';` |
|        - | 15601 | `#endif` |
|        1 | 15602 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 15603 | `	SXUNUSED(apArg);` |
|        - | 15604 | `	/* Point to the list of import paths */` |
|        3 | 15605 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 15606 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 15607 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 15608 | `		if( n > 0 ){` |
|        - | 15609 | `			/* Append dir seprator */` |
|      ! 0 | 15610 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 15611 | `		}` |
|        - | 15612 | `		/* Append path */` |
|        3 | 15613 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 15614 | `	}` |
|        3 | 15615 | `	return PH7_OK;` |
|        1 | 15616 |  |
|        - | 15617 | `/*` |
|        - | 15618 | ` * string get_get_included_files(void)` |
|        - | 15619 | ` *  Gets the current include_path configuration option.` |
|        - | 15620 | ` * Parameter` |
|        - | 15621 | ` *  None` |
|        - | 15622 | ` * Return` |
|        - | 15623 | ` *  Included paths as a string` |
|        - | 15624 | ` */` |
|        2 | 15625 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15626 |  |
|        3 | 15627 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 15628 | `	ph7_value *pArray,*pWorker;` |
|        - | 15629 | `	SyString *pEntry;` |
|        - | 15630 | `	int c,d;` |
|        - | 15631 | `	/* Create an array and a working value */` |
|        3 | 15632 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 15633 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 15634 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 15635 | `		/* Out of memory,return null */` |
|      ! 0 | 15636 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15637 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 15638 | `		SXUNUSED(apArg);` |
|      ! 0 | 15639 | `		return PH7_OK;` |
|        - | 15640 | `	}` |
|        3 | 15641 | `	c = d = '/';` |
|        - | 15642 | `#ifdef __WINNT__` |
|        1 | 15643 | `	d = '\\';` |
|        - | 15644 | `#endif` |
|        - | 15645 | `	/* Iterate throw entries */` |
|        3 | 15646 | `	SySetResetCursor(pFiles);` |
|     3917 | 15647 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 15648 | `		const char *zBase,*zEnd;` |
|        - | 15649 | `		int iLen;` |
|        - | 15650 | `		/* reset the string cursor */` |
|     3915 | 15651 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 15652 | `		/* Extract base name */` |
|     3915 | 15653 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 15654 | `		/* Ignore trailing '/' */` |
|     5872 | 15655 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 15656 | `			zEnd--;` |
|      ! 0 | 15657 | `		}` |
|     3915 | 15658 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   120668 | 15659 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   114797 | 15660 | `			zEnd--;` |
|        1 | 15661 | `		}` |
|     3915 | 15662 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3915 | 15663 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 15664 | `		/* Copy entry name */` |
|     3915 | 15665 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 15666 | `		/* Perform the insertion */` |
|     3915 | 15667 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 15668 | `	}` |
|        - | 15669 | `	/* All done,return the created array */` |
|        3 | 15670 | `	ph7_result_value(pCtx,pArray);` |
|        - | 15671 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 15672 | `	 * by the engine as soon we return from this foreign` |
|        - | 15673 | `	 * function.` |
|        - | 15674 | `	 */` |
|        3 | 15675 | `	return PH7_OK;` |
|        2 | 15676 |  |
|        - | 15677 | `/*` |
|        - | 15678 | ` * include:` |
|        - | 15679 | ` * According to the PHP reference manual.` |
|        - | 15680 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 15681 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 15682 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 15683 | ` *  include() will finally check in the calling script's own directory` |
|        - | 15684 | ` *  and the current working directory before failing. The include()` |
|        - | 15685 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 15686 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 15687 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 15688 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 15689 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 15690 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 15691 | ` *  directory to find the requested file.` |
|        - | 15692 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 15693 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 15694 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 15695 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 15696 | ` */` |
|     9624 | 15697 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15698 |  |
|        - | 15699 | `	SyString sFile;` |
|        - | 15700 | `	sxi32 rc;` |
|     9626 | 15701 | `	if( nArg < 1 ){` |
|        - | 15702 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15703 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15704 | `		return SXRET_OK;` |
|        - | 15705 | `	}` |
|        - | 15706 | `	/* File to include */` |
|     9626 | 15707 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     9626 | 15708 | `	if( sFile.nByte < 1 ){` |
|        - | 15709 | `		/* Empty string,return NULL */` |
|      ! 0 | 15710 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15711 | `		return SXRET_OK;` |
|        - | 15712 | `	}` |
|        - | 15713 | `	/* Open,compile and execute the desired script */` |
|     9626 | 15714 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     9626 | 15715 | `	if( rc != SXRET_OK ){` |
|        - | 15716 | `		/* Emit a warning and return false */` |
|        3 | 15717 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 15718 | `		ph7_result_bool(pCtx,0);` |
|        1 | 15719 | `	}` |
|     9626 | 15720 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15721 | `		/* exit/die inside the included file: cascade the halt */` |
|        5 | 15722 | `		return PH7_ABORT;` |
|        - | 15723 | `	}` |
|     9622 | 15724 | `	return SXRET_OK;` |
|     4814 | 15725 |  |
|        - | 15726 | `/*` |
|        - | 15727 | ` * include_once:` |
|        - | 15728 | ` *  According to the PHP reference manual.` |
|        - | 15729 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 15730 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 15731 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 15732 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 15733 | ` *   just once.` |
|        - | 15734 | ` */` |
|       10 | 15735 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15736 |  |
|        - | 15737 | `	SyString sFile;` |
|        - | 15738 | `	sxi32 rc;` |
|       11 | 15739 | `	if( nArg < 1 ){` |
|        - | 15740 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15741 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15742 | `		return SXRET_OK;` |
|        - | 15743 | `	}` |
|        - | 15744 | `	/* File to include */` |
|       11 | 15745 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|       11 | 15746 | `	if( sFile.nByte < 1 ){` |
|        - | 15747 | `		/* Empty string,return NULL */` |
|      ! 0 | 15748 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15749 | `		return SXRET_OK;` |
|        - | 15750 | `	}` |
|        - | 15751 | `	/* Open,compile and execute the desired script */` |
|       11 | 15752 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|       11 | 15753 | `	if( rc == SXERR_EXISTS ){` |
|        - | 15754 | `		/* File already included,return TRUE */` |
|        7 | 15755 | `		ph7_result_bool(pCtx,1);` |
|        7 | 15756 | `		return SXRET_OK;` |
|        - | 15757 | `	}` |
|        5 | 15758 | `	if( rc != SXRET_OK ){` |
|        - | 15759 | `		/* Emit a warning and return false */` |
|      ! 0 | 15760 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 15761 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15762 | ` 	}` |
|        5 | 15763 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15764 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 15765 | `		return PH7_ABORT;` |
|        - | 15766 | `	}` |
|        5 | 15767 | `	return SXRET_OK;` |
|        6 | 15768 |  |
|        - | 15769 | `/*` |
|        - | 15770 | ` * require.` |
|        - | 15771 | ` *  According to the PHP reference manual.` |
|        - | 15772 | ` *   require() is identical to include() except upon failure it will` |
|        - | 15773 | ` *   also produce a fatal level error.` |
|        - | 15774 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 15775 | ` *   emits a warning  which allows the script to continue.` |
|        - | 15776 | ` */` |
|        6 | 15777 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15778 |  |
|        - | 15779 | `	SyString sFile;` |
|        - | 15780 | `	sxi32 rc;` |
|        8 | 15781 | `	if( nArg < 1 ){` |
|        - | 15782 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15783 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15784 | `		return SXRET_OK;` |
|        - | 15785 | `	}` |
|        - | 15786 | `	/* File to include */` |
|        8 | 15787 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 | 15788 | `	if( sFile.nByte < 1 ){` |
|        - | 15789 | `		/* Empty string,return NULL */` |
|      ! 0 | 15790 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15791 | `		return SXRET_OK;` |
|        - | 15792 | `	}` |
|        - | 15793 | `	/* Open,compile and execute the desired script */` |
|        8 | 15794 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        8 | 15795 | `	if( rc != SXRET_OK ){` |
|        - | 15796 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 15797 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 15798 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15799 | `		return PH7_ABORT;` |
|        - | 15800 | `	}` |
|        8 | 15801 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15802 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 15803 | `		return PH7_ABORT;` |
|        - | 15804 | `	}` |
|        8 | 15805 | `	return SXRET_OK;` |
|        5 | 15806 |  |
|        - | 15807 | `/*` |
|        - | 15808 | ` * require_once:` |
|        - | 15809 | ` *  According to the PHP reference manual.` |
|        - | 15810 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 15811 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 15812 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 15813 | ` *   and how it differs from its non _once siblings.` |
|        - | 15814 | ` */` |
|        4 | 15815 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15816 |  |
|        - | 15817 | `	SyString sFile;` |
|        - | 15818 | `	sxi32 rc;` |
|        5 | 15819 | `	if( nArg < 1 ){` |
|        - | 15820 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15821 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15822 | `		return SXRET_OK;` |
|        - | 15823 | `	}` |
|        - | 15824 | `	/* File to include */` |
|        5 | 15825 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 15826 | `	if( sFile.nByte < 1 ){` |
|        - | 15827 | `		/* Empty string,return NULL */` |
|      ! 0 | 15828 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15829 | `		return SXRET_OK;` |
|        - | 15830 | `	}` |
|        - | 15831 | `	/* Open,compile and execute the desired script */` |
|        5 | 15832 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 15833 | `	if( rc == SXERR_EXISTS ){` |
|        - | 15834 | `		/* File already included,return TRUE */` |
|        3 | 15835 | `		ph7_result_bool(pCtx,1);` |
|        3 | 15836 | `		return SXRET_OK;` |
|        - | 15837 | `	}` |
|        3 | 15838 | `	if( rc != SXRET_OK ){` |
|        - | 15839 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 15840 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 15841 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15842 | `		return PH7_ABORT;` |
|        - | 15843 | `	}` |
|        3 | 15844 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15845 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 15846 | `		return PH7_ABORT;` |
|        - | 15847 | `	}` |
|        3 | 15848 | `	return SXRET_OK;` |
|        3 | 15849 |  |
|        - | 15850 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 15851 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 15852 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 15853 | `/*` |
|        - | 15854 | ` * Section:` |
|        - | 15855 | ` *  SPL Autoloading functions.` |
|        - | 15856 | ` * Status:` |
|        - | 15857 | ` *  Stable.` |
|        - | 15858 | ` */` |
|        - | 15859 | `/*` |
|        - | 15860 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 15861 | ` *  Register given function as __autoload() implementation.` |
|        - | 15862 | ` * Parameters` |
|        - | 15863 | ` *  callback` |
|        - | 15864 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 15865 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 15866 | ` *  throw` |
|        - | 15867 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 15868 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - | 15869 | ` *  prepend` |
|        - | 15870 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 15871 | ` *   autoload stack instead of appending it.` |
|        - | 15872 | ` * Return` |
|        - | 15873 | ` *  TRUE on success, FALSE on failure.` |
|        - | 15874 | ` */` |
|       34 | 15875 | `static int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15876 |  |
|        - | 15877 | `	VmAutoloadCB sEntry;` |
|       36 | 15878 | `	ph7_vm *pVm = pCtx->pVm;` |
|       36 | 15879 | `	int iPrepend = 0;` |
|        - | 15880 | `	sxu32 n;` |
|       36 | 15881 | `	if( nArg < 1 ){` |
|        - | 15882 | `		/* No callback provided — register default spl_autoload.` |
|        - | 15883 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - | 15884 | `		/* Check for duplicates first */` |
|        9 | 15885 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 | 15886 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        6 | 15887 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 | 15888 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 | 15889 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 | 15890 | `				ph7_result_bool(pCtx,1);` |
|        5 | 15891 | `				return SXRET_OK;` |
|        - | 15892 | `			}` |
|      ! 0 | 15893 | `		}` |
|        5 | 15894 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 | 15895 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 | 15896 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 | 15897 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 | 15898 | `		ph7_result_bool(pCtx,1);` |
|        5 | 15899 | `		return SXRET_OK;` |
|        - | 15900 | `	}` |
|        - | 15901 | `	/* Validate that the callback is callable */` |
|       28 | 15902 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 | 15903 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 | 15904 | `		if( nArg >= 2 ){` |
|      ! 0 | 15905 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 15906 | `		}` |
|      ! 0 | 15907 | `		if( iThrow ){` |
|      ! 0 | 15908 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 15909 | `				"Argument is not callable");` |
|      ! 0 | 15910 | `		}` |
|      ! 0 | 15911 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15912 | `		return SXRET_OK;` |
|        - | 15913 | `	}` |
|        - | 15914 | `	/* Check for duplicates */` |
|       46 | 15915 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 | 15916 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 | 15917 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 15918 | `			/* Already registered */` |
|      ! 0 | 15919 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 15920 | `			return SXRET_OK;` |
|        - | 15921 | `		}` |
|       11 | 15922 | `	}` |
|        - | 15923 | `	/* Check prepend flag */` |
|       28 | 15924 | `	if( nArg >= 3 ){` |
|        3 | 15925 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 | 15926 | `	}` |
|        - | 15927 | `	/* Store the callback */` |
|       28 | 15928 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       28 | 15929 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       28 | 15930 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       29 | 15931 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 15932 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 15933 | `		 * We do this by appending first, then rotating the array. */` |
|        3 | 15934 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 15935 | `		VmAutoloadCB *aBase;` |
|        3 | 15936 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 15937 | `		/* Rotate: move last entry to front */` |
|        3 | 15938 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 | 15939 | `		if( aBase ){` |
|        - | 15940 | `			VmAutoloadCB sTemp;` |
|        - | 15941 | `			sxu32 i;` |
|        3 | 15942 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 | 15943 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 | 15944 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 | 15945 | `			}` |
|        3 | 15946 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 | 15947 | `		}` |
|        2 | 15948 | `	}else{` |
|       26 | 15949 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 15950 | `	}` |
|       28 | 15951 | `	ph7_result_bool(pCtx,1);` |
|       28 | 15952 | `	return SXRET_OK;` |
|       19 | 15953 |  |
|        - | 15954 | `/*` |
|        - | 15955 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 15956 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 15957 | ` * Parameters` |
|        - | 15958 | ` *  callback` |
|        - | 15959 | ` *   The autoload function being unregistered.` |
|        - | 15960 | ` * Return` |
|        - | 15961 | ` *  TRUE on success, FALSE on failure.` |
|        - | 15962 | ` */` |
|       32 | 15963 | `static int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15964 |  |
|       34 | 15965 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 15966 | `	sxu32 n,nEntry;` |
|       34 | 15967 | `	if( nArg < 1 ){` |
|      ! 0 | 15968 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15969 | `		return SXRET_OK;` |
|        - | 15970 | `	}` |
|       34 | 15971 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 15972 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       36 | 15973 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       36 | 15974 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 15975 | `			/* Found — remove by shifting remaining entries down */` |
|       32 | 15976 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 15977 | `			sxu32 i;` |
|       32 | 15978 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       46 | 15979 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 | 15980 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 | 15981 | `			}` |
|        - | 15982 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       32 | 15983 | `			SySetPop(&pVm->aAutoload);` |
|       32 | 15984 | `			ph7_result_bool(pCtx,1);` |
|       32 | 15985 | `			return SXRET_OK;` |
|        - | 15986 | `		}` |
|        3 | 15987 | `	}` |
|        3 | 15988 | `	ph7_result_bool(pCtx,0);` |
|        3 | 15989 | `	return SXRET_OK;` |
|       18 | 15990 |  |
|        - | 15991 | `/*` |
|        - | 15992 | ` * array spl_autoload_functions(void)` |
|        - | 15993 | ` *  Return all registered __autoload() functions.` |
|        - | 15994 | ` * Return` |
|        - | 15995 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 15996 | ` *  an empty array is returned.` |
|        - | 15997 | ` */` |
|       20 | 15998 | `static int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15999 |  |
|       21 | 16000 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 16001 | `	ph7_value *pArray;` |
|        - | 16002 | `	sxu32 n,nEntry;` |
|       10 | 16003 | `	SXUNUSED(nArg);` |
|       10 | 16004 | `	SXUNUSED(apArg);` |
|       21 | 16005 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 16006 | `	if( pArray == 0 ){` |
|      ! 0 | 16007 | `		ph7_result_null(pCtx);` |
|      ! 0 | 16008 | `		return SXRET_OK;` |
|        - | 16009 | `	}` |
|       21 | 16010 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 16011 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 16012 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 16013 | `		if( pEntry ){` |
|       15 | 16014 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 16015 | `		}` |
|        8 | 16016 | `	}` |
|       21 | 16017 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 16018 | `	return SXRET_OK;` |
|       11 | 16019 |  |
|        - | 16020 | `/*` |
|        - | 16021 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - | 16022 | ` *  Default implementation of __autoload().` |
|        - | 16023 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 16024 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 16025 | ` * Parameters` |
|        - | 16026 | ` *  class` |
|        - | 16027 | ` *   The class name being searched.` |
|        - | 16028 | ` *  file_extensions` |
|        - | 16029 | ` *   Comma-separated list of file extensions to try.` |
|        - | 16030 | ` */` |
|        2 | 16031 | `static int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 16032 |  |
|        - | 16033 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 16034 | `	SyBlob sPath;` |
|        - | 16035 | `	int nClass;` |
|        - | 16036 | `	sxi32 rc;` |
|        3 | 16037 | `	if( nArg < 1 ){` |
|      ! 0 | 16038 | `		return SXRET_OK;` |
|        - | 16039 | `	}` |
|        3 | 16040 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 16041 | `	if( nClass < 1 ){` |
|      ! 0 | 16042 | `		return SXRET_OK;` |
|        - | 16043 | `	}` |
|        - | 16044 | `	/* Default extensions */` |
|        3 | 16045 | `	zExt = ".php,.inc";` |
|        3 | 16046 | `	if( nArg >= 2 ){` |
|        - | 16047 | `		int nExt;` |
|      ! 0 | 16048 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 16049 | `		if( nExt < 1 ){` |
|      ! 0 | 16050 | `			zExt = ".php,.inc";` |
|      ! 0 | 16051 | `		}` |
|      ! 0 | 16052 | `	}` |
|        3 | 16053 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 16054 | `	/* Iterate over comma-separated extensions */` |
|        3 | 16055 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 | 16056 | `	zCur = zExt;` |
|        7 | 16057 | `	while( zCur < zEnd ){` |
|        - | 16058 | `		const char *zComma;` |
|        - | 16059 | `		SyString sFile;` |
|        - | 16060 | `		int i;` |
|        - | 16061 | `		/* Find next comma or end */` |
|        5 | 16062 | `		zComma = zCur;` |
|       21 | 16063 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 16064 | `			zComma++;` |
|        1 | 16065 | `		}` |
|        - | 16066 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 16067 | `		SyBlobReset(&sPath);` |
|       69 | 16068 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 16069 | `			char c = zClass[i];` |
|       65 | 16070 | `			if( c == '\\' ){` |
|      ! 0 | 16071 | `				c = '/';` |
|       65 | 16072 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 16073 | `				c = c + ('a' - 'A');` |
|        6 | 16074 | `			}` |
|       65 | 16075 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 16076 | `		}` |
|        - | 16077 | `		/* Append extension */` |
|        5 | 16078 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 16079 | `		/* Try to include the file */` |
|        5 | 16080 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 16081 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 16082 | `		if( rc == SXRET_OK ){` |
|        - | 16083 | `			/* File included successfully */` |
|      ! 0 | 16084 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 16085 | `			return SXRET_OK;` |
|        - | 16086 | `		}` |
|        - | 16087 | `		/* Move past the comma */` |
|        5 | 16088 | `		zCur = zComma;` |
|        5 | 16089 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 16090 | `			zCur++;` |
|        1 | 16091 | `		}` |
|        1 | 16092 | `	}` |
|        3 | 16093 | `	SyBlobRelease(&sPath);` |
|        3 | 16094 | `	return SXRET_OK;` |
|        2 | 16095 |  |
|        - | 16096 | `/* Table of built-in VM functions. */` |
|        - | 16097 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 16098 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 16099 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 16100 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 16101 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 16102 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 16103 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 16104 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 16105 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 16106 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 16107 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 16108 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 16109 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 16110 | `	    /* Constants management */` |
|        - | 16111 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 16112 | `	{ "define",   vm_builtin_define               },` |
|        - | 16113 | `	{ "constant", vm_builtin_constant             },` |
|        - | 16114 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 16115 | `	   /* Class/Object functions */` |
|        - | 16116 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 16117 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 16118 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 16119 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 16120 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 16121 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 16122 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 16123 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 16124 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 16125 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 16126 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 16127 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 16128 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 16129 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 16130 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 16131 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 16132 | `	   /* SPL Autoloading */` |
|        - | 16133 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 16134 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 16135 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 16136 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 16137 | `	   /* Random numbers/strings generators */` |
|        - | 16138 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 16139 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 16140 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 16141 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 16142 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 16143 | `	{ "random_int",    vm_builtin_random_int      },` |
|        - | 16144 | `	{ "random_bytes",  vm_builtin_random_bytes    },` |
|        - | 16145 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 16146 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 16147 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 16148 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 16149 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 16150 | `	   /* Language constructs functions */` |
|        - | 16151 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 16152 | `	{ "print", vm_builtin_print                   },` |
|        - | 16153 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 16154 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 16155 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 16156 | `	  /* Variable handling functions */` |
|        - | 16157 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 16158 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 16159 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 16160 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 16161 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 16162 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 16163 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 16164 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 16165 | `	  /* Ouput control functions */` |
|        - | 16166 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 16167 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 16168 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 16169 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 16170 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 16171 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 16172 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 16173 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 16174 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 16175 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 16176 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 16177 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 16178 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 16179 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 16180 | `	  /* Assertion functions */` |
|        - | 16181 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 16182 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 16183 | `	  /* Error reporting functions */` |
|        - | 16184 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 16185 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 16186 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 16187 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 16188 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 16189 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 16190 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 16191 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 16192 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 16193 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 16194 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 16195 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 16196 | `	  /* Release info */` |
|        - | 16197 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 16198 | `	{"phpversion",       vm_builtin_phpversion    },` |
|        - | 16199 | `	{"php_sapi_name",    vm_builtin_php_sapi_name },` |
|        - | 16200 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 16201 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 16202 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 16203 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 16204 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 16205 | `	  /* hashmap */` |
|        - | 16206 | `	{"compact",          vm_builtin_compact       },` |
|        - | 16207 | `	{"extract",          vm_builtin_extract       },` |
|        - | 16208 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 16209 | `	  /* URL related function */` |
|        - | 16210 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 16211 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 16212 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 16213 | `	   /* XML processing functions */` |
|        - | 16214 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 16215 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 16216 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 16217 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 16218 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 16219 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 16220 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 16221 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 16222 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 16223 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 16224 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 16225 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 16226 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 16227 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 16228 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 16229 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 16230 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 16231 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 16232 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 16233 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 16234 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 16235 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 16236 | `	   /* UTF-8 encoding/decoding */` |
|        - | 16237 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 16238 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 16239 | `	   /* Command line processing */` |
|        - | 16240 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 16241 | `	   /* JSON encoding/decoding */` |
|        - | 16242 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 16243 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 16244 | `	{"json_last_error_msg",vm_builtin_json_last_error_msg},` |
|        - | 16245 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 16246 | `	{"json_validate",  vm_builtin_json_validate },` |
|        - | 16247 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 16248 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 16249 | `	   /* Files/URI inclusion facility */` |
|        - | 16250 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 16251 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 16252 | `	{ "include",      vm_builtin_include          },` |
|        - | 16253 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 16254 | `	{ "require",      vm_builtin_require          },` |
|        - | 16255 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 16256 | `};` |
|        - | 16257 | `/*` |
|        - | 16258 | ` * Register the built-in VM functions defined above.` |
|        - | 16259 | ` */` |
|     2834 | 16260 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 16261 |  |
|        - | 16262 | `	sxi32 rc;` |
|        - | 16263 | `	sxu32 n;` |
|   382592 | 16264 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 16265 | `		/* Note that these special functions have access` |
|        - | 16266 | `		 * to the underlying virtual machine as their` |
|        - | 16267 | `		 * private data.` |
|        - | 16268 | `		 */` |
|   379758 | 16269 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   379758 | 16270 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 16271 | `			return rc;` |
|        - | 16272 | `		}` |
|   189880 | 16273 | `	}` |
|     2836 | 16274 | `	return SXRET_OK;` |
|     1419 | 16275 |  |
|        - | 16276 | `/*` |
|        - | 16277 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 16278 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 16279 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 16280 | ` */` |
|   186086 | 16281 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        2 | 16282 |  |
|   186088 | 16283 | `	if( !iLoadable ){` |
|   183964 | 16284 | `		return pClass;` |
|        - | 16285 | `	}` |
|     2132 | 16286 | `	while(pClass){` |
|     2126 | 16287 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     2120 | 16288 | `			return pClass;` |
|        - | 16289 | `		}` |
|        7 | 16290 | `		pClass = pClass->pNextName;` |
|        1 | 16291 | `	}` |
|        7 | 16292 | `	return 0;` |
|    93045 | 16293 |  |
|        - | 16294 | `/*` |
|        - | 16295 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 16296 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 16297 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 16298 | ` * registered in the VM's class table.` |
|        - | 16299 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 16300 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 16301 | ` */` |
|       38 | 16302 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 16303 |  |
|        - | 16304 | `	VmAutoloadCB *pEntry;` |
|        - | 16305 | `	ph7_value sArg,sResult;` |
|        - | 16306 | `	SyHashEntry *pHashEntry;` |
|        - | 16307 | `	ph7_class *pClass;` |
|        - | 16308 | `	sxu32 n,nEntry;` |
|       40 | 16309 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       40 | 16310 | `	if( nEntry < 1 ){` |
|       26 | 16311 | `		return 0;` |
|        - | 16312 | `	}` |
|        - | 16313 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       16 | 16314 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 16315 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 16316 | `	}` |
|        - | 16317 | `	/* Mark this class as being autoloaded */` |
|       14 | 16318 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 16319 | `	/* Prepare the class name argument */` |
|       14 | 16320 | `	PH7_MemObjInit(pVm,&sArg);` |
|       14 | 16321 | `	PH7_MemObjInit(pVm,&sResult);` |
|       14 | 16322 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       14 | 16323 | `	pClass = 0;` |
|       28 | 16324 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 16325 | `		ph7_value *apArg[1];` |
|       24 | 16326 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       24 | 16327 | `		if( pEntry == 0 ){` |
|      ! 0 | 16328 | `			continue;` |
|        - | 16329 | `		}` |
|       24 | 16330 | `		apArg[0] = &sArg;` |
|       24 | 16331 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 16332 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 16333 | `			continue;` |
|        - | 16334 | `		}` |
|        - | 16335 | `		/* Check if the class is now available */` |
|       24 | 16336 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       24 | 16337 | `		if( pHashEntry ){` |
|       10 | 16338 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       10 | 16339 | `			if( pClass ){` |
|       10 | 16340 | `				break;` |
|        - | 16341 | `			}` |
|      ! 0 | 16342 | `		}` |
|        9 | 16343 | `	}` |
|       14 | 16344 | `	PH7_MemObjRelease(&sArg);` |
|       14 | 16345 | `	PH7_MemObjRelease(&sResult);` |
|        - | 16346 | `	/* Remove reentrancy guard */` |
|       14 | 16347 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       14 | 16348 | `	return pClass;` |
|       21 | 16349 |  |
|        - | 16350 | `/*` |
|        - | 16351 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 16352 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 16353 | ` */` |
|       18 | 16354 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 16355 |  |
|       20 | 16356 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        2 | 16357 |  |
|        - | 16358 | `/*` |
|        - | 16359 | ` * Check if the given name refer to an installed class.` |
|        - | 16360 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 16361 | ` */` |
|   186098 | 16362 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 16363 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 16364 | `	const char *zName,  /* Name of the target class */` |
|        - | 16365 | `	sxu32 nByte,        /* zName length */` |
|        - | 16366 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 16367 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 16368 | `						 */` |
|        - | 16369 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 16370 | `	)` |
|        2 | 16371 |  |
|        - | 16372 | `	SyHashEntry *pEntry;` |
|        - | 16373 | `	ph7_class *pClass;` |
|    93049 | 16374 | `	SXUNUSED(iNest);` |
|        - | 16375 | `	/* Exact class lookup.` |
|        - | 16376 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 16377 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   186100 | 16378 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|   186100 | 16379 | `	if( pEntry == 0 ){` |
|        - | 16380 | `		/* Class not found in hash table — try autoload before giving up */` |
|       22 | 16381 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 16382 | `	}` |
|   186080 | 16383 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|   186080 | 16384 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|    93051 | 16385 |  |
|        - | 16386 | `/*` |
|        - | 16387 | ` * Reference Table Implementation` |
|        - | 16388 | ` * Status: stable <chm@symisc.net>` |
|        - | 16389 | ` * Intro` |
|        - | 16390 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 16391 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 16392 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 16393 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 16394 | ` *  Refer to the official for more information on this powerful` |
|        - | 16395 | ` *  extension.` |
|        - | 16396 | ` */` |
|        - | 16397 | `/*` |
|        - | 16398 | ` * Allocate a new reference entry.` |
|        - | 16399 | ` */` |
|  3212976 | 16400 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 16401 |  |
|        - | 16402 | `	VmRefObj *pRef;` |
|        - | 16403 | `	/* Allocate a new instance */` |
|  3212978 | 16404 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3212978 | 16405 | `	if( pRef == 0 ){` |
|      ! 0 | 16406 | `		return 0;` |
|        - | 16407 | `	}` |
|        - | 16408 | `	/* Zero the structure */` |
|  3212978 | 16409 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 16410 | `	/* Initialize fields */` |
|  3212978 | 16411 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3212978 | 16412 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3212978 | 16413 | `	pRef->nIdx = nIdx;` |
|  3212978 | 16414 | `	return pRef;` |
|  1606490 | 16415 |  |
|        - | 16416 | `/*` |
|        - | 16417 | ` * Default hash function used by the reference table` |
|        - | 16418 | ` * for lookup/insertion operations.` |
|        - | 16419 | ` */` |
| 17584401 | 16420 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 16421 |  |
|        - | 16422 | `	/* Calculate the hash based on the memory object index */` |
| 17584403 | 16423 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 16424 |  |
|        - | 16425 | `/*` |
|        - | 16426 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 16427 | ` * in the reference table.` |
|        - | 16428 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 16429 | ` * otherwise.` |
|        - | 16430 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16431 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16432 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16433 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16434 | ` * Refer to the official for more information on this powerful` |
|        - | 16435 | ` * extension.` |
|        - | 16436 | ` */` |
|  9578588 | 16437 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 16438 |  |
|        - | 16439 | `	VmRefObj *pRef;` |
|        - | 16440 | `	sxu32 nBucket;` |
|        - | 16441 | `	/* Point to the appropriate bucket */` |
|  9578590 | 16442 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 16443 | `	/* Perform the lookup */` |
|  9578590 | 16444 | `	pRef = pVm->apRefObj[nBucket];` |
| 21072047 | 16445 | `	for(;;){` |
| 42138721 | 16446 | `		if( pRef == 0 ){` |
|  3319288 | 16447 | `			break;` |
|        - | 16448 | `		}` |
| 38819435 | 16449 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 16450 | `			/* Entry found */` |
|  6259304 | 16451 | `			return pRef;` |
|        - | 16452 | `		}` |
|        - | 16453 | `		/* Point to the next entry */` |
| 32560133 | 16454 | `		pRef = pRef->pNextCollide;` |
|        2 | 16455 | `	}` |
|        - | 16456 | `	/* No such entry,return NULL */` |
|  3319288 | 16457 | `	return 0;` |
|  4789296 | 16458 |  |
|        - | 16459 | `/*` |
|        - | 16460 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 16461 | ` *` |
|        - | 16462 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16463 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16464 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16465 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16466 | ` * Refer to the official for more information on this powerful` |
|        - | 16467 | ` * extension.` |
|        - | 16468 | ` */` |
|  3212976 | 16469 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 16470 |  |
|        - | 16471 | `	sxu32 nBucket;` |
|  3212978 | 16472 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 16473 | `		VmRefObj **apNew;` |
|        - | 16474 | `		sxu32 nNew;` |
|        - | 16475 | `		/* Allocate a larger table */` |
|     4492 | 16476 | `		nNew = pVm->nRefSize << 1;` |
|     4492 | 16477 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     4492 | 16478 | `		if( apNew ){` |
|     4492 | 16479 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 16480 | `			sxu32 n;` |
|        - | 16481 | `			/* Zero the structure */` |
|     4492 | 16482 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 16483 | `			/* Rehash all referenced entries */` |
|  2848166 | 16484 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 16485 | `				/* Remove old collision links */` |
|  2843676 | 16486 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 16487 | `				/* Point to the appropriate bucket */` |
|  2843676 | 16488 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 16489 | `				/* Insert the entry  */` |
|  2843676 | 16490 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2843676 | 16491 | `				if( apNew[nBucket] ){` |
|  2301116 | 16492 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1150557 | 16493 | `				}` |
|  2843676 | 16494 | `				apNew[nBucket] = pEntry;` |
|        - | 16495 | `				/* Point to the next entry */` |
|  2843676 | 16496 | `				pEntry = pEntry->pNext;` |
|  1421839 | 16497 | `			}` |
|        - | 16498 | `			/* Release the old table */` |
|     4492 | 16499 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 16500 | `			/* Install the new one */` |
|     4492 | 16501 | `			pVm->apRefObj = apNew;` |
|     4492 | 16502 | `			pVm->nRefSize = nNew;` |
|     2245 | 16503 | `		}` |
|     2245 | 16504 | `	}` |
|        - | 16505 | `	/* Point to the appropriate bucket */` |
|  3212978 | 16506 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 16507 | `	/* Insert the entry */` |
|  3212978 | 16508 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3212978 | 16509 | `	if( pVm->apRefObj[nBucket] ){` |
|  2621675 | 16510 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1310845 | 16511 | `	}` |
|  3212978 | 16512 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3212978 | 16513 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3212978 | 16514 | `	pVm->nRefUsed++;` |
|  3212978 | 16515 | `	return SXRET_OK;` |
|        2 | 16516 |  |
|        - | 16517 | `/*` |
|        - | 16518 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 16519 | ` * the reference table.` |
|        - | 16520 | ` * This function is invoked when the user perform an unset` |
|        - | 16521 | ` * call [i.e: unset($var); ].` |
|        - | 16522 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16523 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16524 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16525 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16526 | ` * Refer to the official for more information on this powerful` |
|        - | 16527 | ` * extension.` |
|        - | 16528 | ` */` |
|  3171554 | 16529 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 16530 |  |
|        - | 16531 | `	ph7_hashmap_node **apNode;` |
|        - | 16532 | `	SyHashEntry **apEntry;` |
|        - | 16533 | `	sxu32 n;` |
|        - | 16534 | `	/* Point to the reference table */` |
|  3171556 | 16535 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  3171556 | 16536 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 16537 | `	/* Unlink the entry from the reference table */` |
|  3283866 | 16538 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|   112312 | 16539 | `		if( apEntry[n] ){` |
|   112262 | 16540 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    56130 | 16541 | `		}` |
|    56157 | 16542 | `	}` |
|  6230778 | 16543 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  3059224 | 16544 | `		if( apNode[n] ){` |
|     7074 | 16545 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3536 | 16546 | `		}` |
|  1529613 | 16547 | `	}` |
|  3171556 | 16548 | `	if( pRef->pPrevCollide ){` |
|  1222393 | 16549 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   610829 | 16550 | `	}else{` |
|  1949165 | 16551 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 16552 | `	}` |
|  3171556 | 16553 | `	if( pRef->pNextCollide ){` |
|  1808622 | 16554 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   904310 | 16555 | `	}` |
|  3171556 | 16556 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 16557 | `	/* Release the node */` |
|  3171556 | 16558 | `	SySetRelease(&pRef->aReference);` |
|  3171556 | 16559 | `	SySetRelease(&pRef->aArrEntries);` |
|  3171556 | 16560 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  3171556 | 16561 | `	pVm->nRefUsed--;` |
|  3171556 | 16562 | `	return SXRET_OK;` |
|        2 | 16563 |  |
|        - | 16564 | `/*` |
|        - | 16565 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 16566 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16567 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16568 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16569 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16570 | ` * Refer to the official for more information on this powerful` |
|        - | 16571 | ` * extension.` |
|        - | 16572 | ` */` |
|  3248694 | 16573 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 16574 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 16575 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 16576 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 16577 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 16578 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 16579 | `	)` |
|        2 | 16580 |  |
|  3248696 | 16581 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 16582 | `	VmRefObj *pRef;` |
|        - | 16583 | `	/* Check if the referenced object already exists */` |
|  3248696 | 16584 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3248696 | 16585 | `	if( pRef == 0 ){` |
|        - | 16586 | `		/* Create a new entry */` |
|  3212978 | 16587 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3212978 | 16588 | `		if( pRef == 0 ){` |
|      ! 0 | 16589 | `			return SXERR_MEM;` |
|        - | 16590 | `		}` |
|  3212978 | 16591 | `		pRef->iFlags = iFlags;` |
|        - | 16592 | `		/* Install the entry */` |
|  3212978 | 16593 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1606488 | 16594 | `	}` |
|  3248696 | 16595 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3248696 | 16596 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 16597 | `		VmSlot sRef;` |
|        - | 16598 | `		/* Local frame,record referenced entry so that it can` |
|        - | 16599 | `		 * be deleted when we leave this frame.` |
|        - | 16600 | `		 */` |
|   106420 | 16601 | `		sRef.nIdx = nIdx;` |
|   106420 | 16602 | `		sRef.pUserData = pEntry;` |
|   106420 | 16603 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 16604 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 16605 | `		}` |
|    53209 | 16606 | `	}` |
|  3248696 | 16607 | `	if( pEntry ){` |
|        - | 16608 | `		/* Address of the hash-entry */` |
|   141910 | 16609 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    70954 | 16610 | `	}` |
|  3248696 | 16611 | `	if( pMapEntry ){` |
|        - | 16612 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  3098200 | 16613 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1549099 | 16614 | `	}` |
|  3248696 | 16615 | `	return SXRET_OK;` |
|  1624349 | 16616 |  |
|        - | 16617 | `/*` |
|        - | 16618 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 16619 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16620 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16621 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16622 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16623 | ` * Refer to the official for more information on this powerful` |
|        - | 16624 | ` * extension.` |
|        - | 16625 | ` */` |
|  3158532 | 16626 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 16627 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 16628 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 16629 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 16630 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 16631 | `	)` |
|        2 | 16632 |  |
|        - | 16633 | `	VmRefObj *pRef;` |
|        - | 16634 | `	sxu32 n;` |
|        - | 16635 | `	/* Check if the referenced object already exists */` |
|  3158534 | 16636 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3158534 | 16637 | `	if( pRef == 0 ){` |
|        - | 16638 | `		/* Not such entry */` |
|   106306 | 16639 | `		return SXERR_NOTFOUND;` |
|        - | 16640 | `	}` |
|        - | 16641 | `	/* Remove the desired entry */` |
|  3052230 | 16642 | `	if( pEntry ){` |
|        - | 16643 | `		SyHashEntry **apEntry;` |
|       74 | 16644 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      264 | 16645 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      192 | 16646 | `			if( apEntry[n] == pEntry ){` |
|        - | 16647 | `				/* Nullify the entry */` |
|       74 | 16648 | `				apEntry[n] = 0;` |
|        - | 16649 | `				/*` |
|        - | 16650 | `				 * NOTE:` |
|        - | 16651 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 16652 | `				 * we avoid wasting spaces.` |
|        - | 16653 | `				 */` |
|       36 | 16654 | `			}` |
|       97 | 16655 | `		}` |
|       36 | 16656 | `	}` |
|  3052230 | 16657 | `	if( pMapEntry ){` |
|        - | 16658 | `		ph7_hashmap_node **apNode;` |
|  3052158 | 16659 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  6104408 | 16660 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  3052252 | 16661 | `			if( apNode[n] == pMapEntry ){` |
|        - | 16662 | `				/* nullify the entry */` |
|  3052158 | 16663 | `				apNode[n] = 0;` |
|  1526078 | 16664 | `			}` |
|  1526127 | 16665 | `		}` |
|  1526078 | 16666 | `	}` |
|  3052230 | 16667 | `	return SXRET_OK;` |
|  1579268 | 16668 |  |
|        - | 16669 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 16670 | `/*` |
|        - | 16671 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 16672 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 16673 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 16674 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 16675 | ` * For more information on how to register IO stream devices,please` |
|        - | 16676 | ` * refer to the official documentation.` |
|        - | 16677 | ` */` |
|    29298 | 16678 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 16679 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 16680 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 16681 | `	int nByte              /* *pzDevice length*/` |
|        - | 16682 | `	)` |
|        2 | 16683 |  |
|        - | 16684 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 16685 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 16686 | `	SyString sDev,sCur;` |
|        - | 16687 | `	sxu32 n,nEntry;` |
|        - | 16688 | `	int rc;` |
|        - | 16689 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    29300 | 16690 | `	zNext = zCur = zIn = *pzDevice;` |
|    29300 | 16691 | `	zEnd = &zIn[nByte];` |
|  1871149 | 16692 | `	while( zIn < zEnd ){` |
|  1841853 | 16693 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 16694 | `			/* Got one */` |
|        3 | 16695 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 16696 | `			break;` |
|        - | 16697 | `		}` |
|        - | 16698 | `		/* Advance the cursor */` |
|  1841851 | 16699 | `		zIn++;` |
|        2 | 16700 | `	}` |
|    29300 | 16701 | `	if( zIn >= zEnd ){` |
|        - | 16702 | `		/* No such scheme,return the default stream */` |
|    29298 | 16703 | `		return pVm->pDefStream;` |
|        - | 16704 | `	}` |
|        3 | 16705 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 16706 | `	/* Remove leading and trailing white spaces */` |
|        3 | 16707 | `	SyStringFullTrim(&sDev);` |
|        - | 16708 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 16709 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 16710 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 16711 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 16712 | `		pStream = apStream[n];` |
|        3 | 16713 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 16714 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 16715 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 16716 | `		if( rc == 0 ){` |
|        - | 16717 | `			/* Stream device found */` |
|        3 | 16718 | `			*pzDevice = zNext;` |
|        3 | 16719 | `			return pStream;` |
|        - | 16720 | `		}` |
|      ! 0 | 16721 | `	}` |
|        - | 16722 | `	/* No such stream,return NULL */` |
|      ! 0 | 16723 | `	return 0;` |
|    14651 | 16724 |  |
|        - | 16725 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 16726 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 16727 |  |
