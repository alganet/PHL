# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 6805/8693 lines (78.28%)

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
|   919682 |   140 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |   141 |  |
|   919684 |   142 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       35 |   143 | `		return TRUE;` |
|        - |   144 | `	}` |
|   919650 |   145 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       11 |   146 | `		return TRUE;` |
|        - |   147 | `	}` |
|   919640 |   148 | `	return FALSE;` |
|   459865 |   149 |  |
|        - |   150 | `/*` |
|        - |   151 | ` * Return TRUE if the value should take the Perl-style string-increment path:` |
|        - |   152 | ` * any MEMOBJ_STRING that is empty, or whose contents are not a complete` |
|        - |   153 | ` * number (matching PHP's is_numeric semantics — the whole string must parse` |
|        - |   154 | ` * as a number, with optional surrounding whitespace).  Strings with a` |
|        - |   155 | ` * numeric prefix followed by non-whitespace bytes (e.g. "5foo") take the` |
|        - |   156 | ` * Perl path, like PHP.  Strict numeric strings ("5", "1.5", "5e2", "  5  ")` |
|        - |   157 | ` * still go through the existing numeric coercion.` |
|        - |   158 | ` */` |
|   335944 |   159 | `static int VmStringWantsPerlIncr(ph7_value *pVal)` |
|        2 |   160 |  |
|        - |   161 | `	SyString sStr;` |
|   335946 |   162 | `	sxu8 bReal = FALSE;` |
|   335946 |   163 | `	const char *zTail = 0;` |
|        - |   164 | `	const char *zEnd;` |
|   335946 |   165 | `	if( (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|   335876 |   166 | `		return FALSE;` |
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
|   167996 |   183 |  |
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
|  1394626 |   260 | `static sxi32 PH7_NewForeignFunction(` |
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
|  1394628 |   271 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  1394628 |   272 | `	if( pFunc == 0 ){` |
|      ! 0 |   273 | `		return SXERR_MEM;` |
|        - |   274 | `	}` |
|        - |   275 | `	/* Duplicate function name */` |
|  1394628 |   276 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1394628 |   277 | `	if( zDup == 0 ){` |
|      ! 0 |   278 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   279 | `		return SXERR_MEM;` |
|        - |   280 | `	}` |
|        - |   281 | `	/* Zero the structure */` |
|  1394628 |   282 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   283 | `	/* Initialize structure fields */` |
|  1394628 |   284 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  1394628 |   285 | `	pFunc->pVm   = pVm;` |
|  1394628 |   286 | `	pFunc->xFunc = xFunc;` |
|  1394628 |   287 | `	pFunc->pUserData = pUserData;` |
|  1394628 |   288 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   289 | `	/* Write a pointer to the new function */` |
|  1394628 |   290 | `	*ppOut = pFunc;` |
|  1394628 |   291 | `	return SXRET_OK;` |
|   697315 |   292 |  |
|        - |   293 | `/*` |
|        - |   294 | ` * Install a foreign function and it's associated callback so that` |
|        - |   295 | ` * it can be invoked from the target PHP code.` |
|        - |   296 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   297 | ` * return value indicates failure.` |
|        - |   298 | ` * Please refer to the official documentation for an introduction to` |
|        - |   299 | ` * the foreign function mechanism.` |
|        - |   300 | ` */` |
|  1397460 |   301 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|  1397462 |   312 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  1397462 |   313 | `	if( pEntry ){` |
|     2836 |   314 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2836 |   315 | `		pFunc->pUserData = pUserData;` |
|     2836 |   316 | `		pFunc->xFunc = xFunc;` |
|     2836 |   317 | `		SySetReset(&pFunc->aAux);` |
|     2836 |   318 | `		return SXRET_OK;` |
|        - |   319 | `	}` |
|        - |   320 | `	/* Create a new user function */` |
|  1394628 |   321 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  1394628 |   322 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   323 | `		return rc;` |
|        - |   324 | `	}` |
|        - |   325 | `	/* Install the function in the corresponding hashtable */` |
|  1394628 |   326 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  1394628 |   327 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   328 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   329 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   330 | `		return rc;` |
|        - |   331 | `	}` |
|        - |   332 | `	/* User function successfully installed */` |
|  1394628 |   333 | `	return SXRET_OK;` |
|   698732 |   334 |  |
|        - |   335 | `/*` |
|        - |   336 | ` * Initialize a VM function.` |
|        - |   337 | ` */` |
|   278986 |   338 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   339 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   340 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   341 | `	const char *zName,  /* Function name */` |
|        - |   342 | `	sxu32 nByte,        /* zName length */` |
|        - |   343 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   344 | `	void *pUserData     /* Function private data */` |
|        - |   345 | `	)` |
|        2 |   346 |  |
|        - |   347 | `	/* Zero the structure */` |
|   278988 |   348 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   349 | `	/* Initialize structure fields */` |
|        - |   350 | `	/* Arguments container */` |
|   278988 |   351 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   352 | `	/* Static variable container */` |
|   278988 |   353 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   354 | `	/* Bytecode container */` |
|   278988 |   355 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   356 | `    /* Preallocate some instruction slots */` |
|   278988 |   357 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   358 | `	/* Closure environment */` |
|   278988 |   359 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        - |   360 | `	/* Return-type union alternatives (empty unless declared as a union) */` |
|   278988 |   361 | `	SySetInit(&pFunc->aReturnUnion,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|   278988 |   362 | `	pFunc->iFlags = iFlags;` |
|   278988 |   363 | `	pFunc->pUserData = pUserData;` |
|        - |   364 | `	/* Capture the defining file's strict_types mode. PHP scopes return-type` |
|        - |   365 | `	 * coercion by the callee's file, so we freeze it at definition time. */` |
|   278988 |   366 | `	pFunc->bStrictTypes = (sxu8)(pVm->sCodeGen.bStrictTypes ? 1 : 0);` |
|   278988 |   367 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   278988 |   368 | `	return SXRET_OK;` |
|        2 |   369 |  |
|        - |   370 | `/*` |
|        - |   371 | ` * Namespace-aware function lookup.` |
|        - |   372 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   373 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   374 | ` */` |
|        - |   375 | `/*` |
|        - |   376 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   377 | ` */` |
|  1461082 |   378 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   379 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   380 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   381 | `	SyString *pName     /* Function name */` |
|        - |   382 | `	)` |
|        2 |   383 |  |
|        - |   384 | `	SyHashEntry *pEntry;` |
|        - |   385 | `	sxi32 rc;` |
|  1461084 |   386 | `	if( pName == 0 ){` |
|        - |   387 | `		/* Use the built-in name */` |
|    42040 |   388 | `		pName = &pFunc->sName;` |
|    21019 |   389 | `	}` |
|        - |   390 | `	/* Check for duplicates (functions with the same name) first */` |
|  1461084 |   391 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|  1461084 |   392 | `	if( pEntry ){` |
|  1264232 |   393 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|  1264232 |   394 | `		if( pLink != pFunc ){` |
|        - |   395 | `			/* Link */` |
|      188 |   396 | `			pFunc->pNextName = pLink;` |
|      188 |   397 | `			pEntry->pUserData = pFunc;` |
|       93 |   398 | `		}` |
|  1264232 |   399 | `		return SXRET_OK;` |
|        - |   400 | `	}` |
|        - |   401 | `	/* First time seen */` |
|   196854 |   402 | `	pFunc->pNextName = 0;` |
|   196854 |   403 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   196854 |   404 | `	return rc;` |
|   730543 |   405 |  |
|        - |   406 | `/*` |
|        - |   407 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   408 | ` */` |
|   120644 |   409 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   410 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   411 | `	ph7_class *pClass /* Target Class */` |
|        - |   412 | `	)` |
|        2 |   413 |  |
|   120646 |   414 | `	SyString *pName = &pClass->sName;` |
|        - |   415 | `	SyHashEntry *pEntry;` |
|        - |   416 | `	sxi32 rc;` |
|        - |   417 | `	/* Check for duplicates */` |
|   120646 |   418 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|   120646 |   419 | `	if( pEntry ){` |
|       31 |   420 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   421 | `		/* Link entry with the same name */` |
|       31 |   422 | `		pClass->pNextName = pLink;` |
|       31 |   423 | `		pEntry->pUserData = pClass;` |
|       31 |   424 | `		return SXRET_OK;` |
|        - |   425 | `	}` |
|   120616 |   426 | `	pClass->pNextName = 0;` |
|        - |   427 | `	/* Perform a simple hashtable insertion */` |
|   120616 |   428 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|   120616 |   429 | `	return rc;` |
|    60324 |   430 |  |
|        - |   431 | `/*` |
|        - |   432 | ` * Instruction builder interface.` |
|        - |   433 | ` */` |
|  4270366 |   434 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  4270368 |   446 | `	sInstr.iOp = (sxu8)iOp;` |
|  4270368 |   447 | `	sInstr.iP1 = iP1;` |
|  4270368 |   448 | `	sInstr.iP2 = iP2;` |
|  4270368 |   449 | `	sInstr.p3  = p3;` |
|  4270368 |   450 | `	if( pIndex ){` |
|        - |   451 | `		/* Instruction index in the bytecode array */` |
|   231936 |   452 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|   115967 |   453 | `	}` |
|        - |   454 | `	/* Finally,record the instruction */` |
|  4270368 |   455 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  4270368 |   456 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   457 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   458 | `		/* Fall throw */` |
|      ! 0 |   459 | `	}` |
|  4270368 |   460 | `	return rc;` |
|        2 |   461 |  |
|        - |   462 | `/*` |
|        - |   463 | ` * Swap the current bytecode container with the given one.` |
|        - |   464 | ` */` |
|   554112 |   465 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   466 |  |
|   554114 |   467 | `	if( pContainer == 0 ){` |
|        - |   468 | `		/* Point to the default container */` |
|      ! 0 |   469 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   470 | `	}else{` |
|        - |   471 | `		/* Change container */` |
|   554114 |   472 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   473 | `	}` |
|   554114 |   474 | `	return SXRET_OK;` |
|        2 |   475 |  |
|        - |   476 | `/*` |
|        - |   477 | ` * Return the current bytecode container.` |
|        - |   478 | ` */` |
|   277056 |   479 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   480 |  |
|   277058 |   481 | `	return pVm->pByteContainer;` |
|        2 |   482 |  |
|        - |   483 | `/*` |
|        - |   484 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   485 | ` */` |
|   228710 |   486 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   487 |  |
|        - |   488 | `	VmInstr *pInstr;` |
|   228712 |   489 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   228712 |   490 | `	return pInstr;` |
|        2 |   491 |  |
|        - |   492 | `/*` |
|        - |   493 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   494 | ` */` |
|  1282594 |   495 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   496 |  |
|  1282596 |   497 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   498 |  |
|        - |   499 | `/*` |
|        - |   500 | ` * Pop the last VM instruction.` |
|        - |   501 | ` */` |
|   211526 |   502 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   503 |  |
|   211528 |   504 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   505 |  |
|        - |   506 | `/*` |
|        - |   507 | ` * Peek the last VM instruction.` |
|        - |   508 | ` */` |
|   840912 |   509 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   510 |  |
|   840914 |   511 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   512 |  |
|    33564 |   513 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   514 |  |
|        - |   515 | `	VmInstr *aInstr;` |
|        - |   516 | `	sxu32 n;` |
|    33566 |   517 | `	n = SySetUsed(pVm->pByteContainer);` |
|    33566 |   518 | `	if( n < 2 ){` |
|      ! 0 |   519 | `		return 0;` |
|        - |   520 | `	}` |
|    33566 |   521 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    33566 |   522 | `	return &aInstr[n - 2];` |
|    16784 |   523 |  |
|        - |   524 | `/*` |
|        - |   525 | ` * Allocate a new virtual machine frame.` |
|        - |   526 | ` */` |
|    22650 |   527 | `static VmFrame * VmNewFrame(` |
|        - |   528 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   529 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   530 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   531 | `	)` |
|        2 |   532 |  |
|        - |   533 | `	VmFrame *pFrame;` |
|        - |   534 | `	/* Allocate a new vm frame */` |
|    22652 |   535 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    22652 |   536 | `	if( pFrame == 0 ){` |
|      ! 0 |   537 | `		return 0;` |
|        - |   538 | `	}` |
|        - |   539 | `	/* Zero the structure */` |
|    22652 |   540 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   541 | `	/* Initialize frame fields */` |
|    22652 |   542 | `	pFrame->pUserData = pUserData;` |
|    22652 |   543 | `	pFrame->pThis = pThis;` |
|    22652 |   544 | `	pFrame->pVm = pVm;` |
|    22652 |   545 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    22652 |   546 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    22652 |   547 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    22652 |   548 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    22652 |   549 | `	return pFrame;` |
|    11327 |   550 |  |
|        - |   551 | `/* Forward declaration */` |
|        - |   552 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   553 | `/*` |
|        - |   554 | ` * Enter a VM frame.` |
|        - |   555 | ` */` |
|    22600 |   556 | `static sxi32 VmEnterFrame(` |
|        - |   557 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   558 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   559 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   560 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   561 | `	)` |
|        2 |   562 |  |
|        - |   563 | `	VmFrame *pFrame;` |
|        - |   564 | `	/* Allocate a new frame */` |
|    22602 |   565 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    22602 |   566 | `	if( pFrame == 0 ){` |
|      ! 0 |   567 | `		return SXERR_MEM;` |
|        - |   568 | `	}` |
|        - |   569 | `	/* Link to the list of active VM frame */` |
|    22602 |   570 | `	pFrame->pParent = pVm->pFrame;` |
|    22602 |   571 | `	pVm->pFrame = pFrame;` |
|    22602 |   572 | `	if( ppFrame ){` |
|        - |   573 | `		/* Write a pointer to the new VM frame */` |
|    19448 |   574 | `		*ppFrame = pFrame;` |
|     9723 |   575 | `	}` |
|    22602 |   576 | `	return SXRET_OK;` |
|    11302 |   577 |  |
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
|    19442 |   621 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   622 |  |
|    19444 |   623 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    19444 |   624 | `	if( pCurFrame ){` |
|        - |   625 | `		/* Unlink from the list of active VM frame */` |
|    19444 |   626 | `		pVm->pFrame = pCurFrame->pParent;` |
|    19444 |   627 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   628 | `			VmSlot  *aSlot;` |
|        - |   629 | `			sxu32 n;` |
|        - |   630 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    19056 |   631 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|   124816 |   632 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   633 | `				/* Unset the local variable */` |
|   105762 |   634 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    52882 |   635 | `			}` |
|        - |   636 | `			/* Remove local reference */` |
|    19056 |   637 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|   124890 |   638 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|   105836 |   639 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    52919 |   640 | `			}` |
|     9527 |   641 | `		}` |
|        - |   642 | `		/* Release internal containers */` |
|    19444 |   643 | `		SyHashRelease(&pCurFrame->hVar);` |
|    19444 |   644 | `		SySetRelease(&pCurFrame->sArg);` |
|    19444 |   645 | `		SySetRelease(&pCurFrame->sLocal);` |
|    19444 |   646 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   647 | `		/* Release the whole structure */` |
|    19444 |   648 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     9721 |   649 | `	}` |
|    19444 |   650 |  |
|        - |   651 | `/*` |
|        - |   652 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   653 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   654 | ` * should be skipped when looking for the real execution context.` |
|        - |   655 | ` */` |
|  7131958 |   656 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   657 |  |
|  7134198 |   658 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|     2240 |   659 | `		pFrame = pFrame->pParent;` |
|        2 |   660 | `	}` |
|  7131960 |   661 | `	return pFrame;` |
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
|   355632 |   809 | `static sxi32 VmMountUserClassAttrs(` |
|        - |   810 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   811 | `	ph7_class *pClass /* Class whose static/const attributes are mounted */` |
|        - |   812 | `	)` |
|        2 |   813 |  |
|        - |   814 | `	ph7_class_attr *pAttr;` |
|        - |   815 | `	SyHashEntry *pEntry;` |
|        - |   816 | `	/* Reset the loop cursor */` |
|   355634 |   817 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   818 | `	/* Process only static and constant attribute */` |
|  1405402 |   819 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   820 | `		/* Extract the current attribute */` |
|   871954 |   821 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   871954 |   822 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
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
|   355634 |   866 | `	return SXRET_OK;` |
|   177818 |   867 |  |
|   355400 |   868 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   869 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   870 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   871 | `	)` |
|        2 |   872 |  |
|        - |   873 | `	ph7_class_method *pMeth;` |
|        - |   874 | `	SyHashEntry *pEntry;` |
|        - |   875 | `	sxi32 rc;` |
|        - |   876 | `	/* Reserve/initialize the static and constant attribute slots */` |
|   355402 |   877 | `	rc = VmMountUserClassAttrs(&(*pVm),pClass);` |
|   355402 |   878 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   879 | `		return rc;` |
|        - |   880 | `	}` |
|        - |   881 | `	/* Install class methods */` |
|   355402 |   882 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   883 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   884 | `		 */` |
|   193028 |   885 | `		return SXRET_OK;` |
|        - |   886 | `	}` |
|        - |   887 | `	/* Create constructor alias if not yet done */` |
|   162376 |   888 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   889 | `		/* User constructor with the same base class name */` |
|     6696 |   890 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|     6696 |   891 | `		if( pEntry ){` |
|      ! 0 |   892 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   893 | `			/* Create the alias */` |
|      ! 0 |   894 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   895 | `		}` |
|     3347 |   896 | `	}` |
|        - |   897 | `	/* Install the methods now */` |
|   162376 |   898 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  1662615 |   899 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  1419054 |   900 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  1419054 |   901 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|  1419046 |   902 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|  1419046 |   903 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   904 | `				return rc;` |
|        - |   905 | `			}` |
|   709522 |   906 | `		}` |
|        2 |   907 | `	}` |
|        - |   908 | `	/* Mark class as mounted to avoid redundant mounting */` |
|   162376 |   909 | `	pClass->bMounted = TRUE;` |
|   162376 |   910 | `	return SXRET_OK;` |
|   177702 |   911 |  |
|        - |   912 | `/*` |
|        - |   913 | ` * Allocate a private frame for attributes of the given` |
|        - |   914 | ` * class instance (Object in the PHP jargon).` |
|        - |   915 | ` */` |
|     2126 |   916 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   917 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   918 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   919 | `	)` |
|        2 |   920 |  |
|     2128 |   921 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   922 | `	ph7_class_attr *pAttr;` |
|        - |   923 | `	SyHashEntry *pEntry;` |
|        - |   924 | `	sxi32 rc;` |
|        - |   925 | `	/* Install class attribute in the private frame associated with this instance */` |
|     2128 |   926 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     8838 |   927 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   928 | `		VmClassAttr *pVmAttr;` |
|        - |   929 | `		/* Extract the current attribute */` |
|     6712 |   930 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     6712 |   931 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     6712 |   932 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   933 | `			return SXERR_MEM;` |
|        - |   934 | `		}` |
|     6712 |   935 | `		pVmAttr->pAttr = pAttr;` |
|     6712 |   936 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   937 | `			ph7_value *pMemObj;` |
|        - |   938 | `			/* Reserve a memory object for this attribute */` |
|     6686 |   939 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     6686 |   940 | `			if( pMemObj == 0 ){` |
|      ! 0 |   941 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   942 | `				return SXERR_MEM;` |
|        - |   943 | `			}` |
|     6686 |   944 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     6686 |   945 | `			pVmAttr->iState = 0;` |
|     6686 |   946 | `			pVmAttr->pOwner = pClass;` |
|     6686 |   947 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   948 | `				/* Initialize attribute default value (any complex expression) */` |
|     2304 |   949 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|     5535 |   950 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        - |   951 | `				/* Typed property without a default: mark uninitialized. Reading` |
|        - |   952 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|       74 |   953 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|       36 |   954 | `			}` |
|     6686 |   955 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     6686 |   956 | `			if( rc != SXRET_OK ){` |
|        - |   957 | `				VmSlot sSlot;` |
|        - |   958 | `				/* Restore memory object */` |
|      ! 0 |   959 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   960 | `				sSlot.pUserData = 0;` |
|      ! 0 |   961 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   962 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   963 | `				return SXERR_MEM;` |
|        - |   964 | `			}` |
|        - |   965 | `			/* Install attribute in the reference table */` |
|     6686 |   966 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   967 | `			/* Register typed property slot for assignment-time enforcement.` |
|        - |   968 | `			 * On failure roll back the just-installed hAttr entry and the` |
|        - |   969 | `			 * reserved memobj so the caller sees a consistent instance. */` |
|     6686 |   970 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
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
|     3344 |   982 | `		}else{` |
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
|     2128 |   994 | `	return SXRET_OK;` |
|     1065 |   995 |  |
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
|   457322 |  1007 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |  1008 |  |
|        - |  1009 | `	ph7_value *pObj;` |
|        - |  1010 | `	sxi32 rc;` |
|   457324 |  1011 | `	if( pIndex ){` |
|        - |  1012 | `		/* Object index in the object table */` |
|   447880 |  1013 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   223939 |  1014 | `	}` |
|        - |  1015 | `	/* Reserve a slot for the new object */` |
|   457324 |  1016 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   457324 |  1017 | `	if( rc != SXRET_OK ){` |
|        - |  1018 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  1019 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  1020 | `		 */` |
|      ! 0 |  1021 | `		return 0;` |
|        - |  1022 | `	}` |
|   457324 |  1023 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   457324 |  1024 | `	return pObj;` |
|   228663 |  1025 |  |
|        - |  1026 | `/*` |
|        - |  1027 | ` * Reserve a memory object.` |
|        - |  1028 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1029 | ` */` |
|  2152092 |  1030 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |  1031 |  |
|        - |  1032 | `	ph7_value *pObj;` |
|        - |  1033 | `	sxi32 rc;` |
|  2152094 |  1034 | `	if( pIndex ){` |
|        - |  1035 | `		/* Object index in the object table */` |
|  2152094 |  1036 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1076046 |  1037 | `	}` |
|        - |  1038 | `	/* Reserve a slot for the new object */` |
|  2152094 |  1039 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2152094 |  1040 | `	if( rc != SXRET_OK ){` |
|        - |  1041 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  1042 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  1043 | `		 */` |
|      ! 0 |  1044 | `		return 0;` |
|        - |  1045 | `	}` |
|  2152094 |  1046 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2152094 |  1047 | `	return pObj;` |
|  1076048 |  1048 |  |
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
|    20704 |  1736 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        2 |  1737 |  |
|    20706 |  1738 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    20706 |  1739 | `	if( xCons != VmObConsumer ){` |
|     8254 |  1740 | `		pVm->nOutputLen += nLen;` |
|     8254 |  1741 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|     1028 |  1742 | `			pVm->bHeadersSent = 1;` |
|      513 |  1743 | `		}` |
|     4126 |  1744 | `	}` |
|    20706 |  1745 |  |
|        - |  1746 | `#define VM_STACK_GUARD 16` |
|        - |  1747 | `/*` |
|        - |  1748 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1749 | ` * our compiled PHP program.` |
|        - |  1750 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1751 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1752 | ` */` |
|    45426 |  1753 | `static ph7_value * VmNewOperandStack(` |
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
|    45428 |  1766 | `	nInstr += VM_STACK_GUARD;` |
|    45428 |  1767 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    45428 |  1768 | `	if( pStack == 0 ){` |
|      ! 0 |  1769 | `		return 0;` |
|        - |  1770 | `	}` |
|        - |  1771 | `	/* Initialize the operand stack */` |
|  3049740 |  1772 | `	while( nInstr > 0 ){` |
|  3004314 |  1773 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  3004314 |  1774 | `		--nInstr;` |
|        2 |  1775 | `	}` |
|        - |  1776 | `	/* Ready for bytecode execution */` |
|    45428 |  1777 | `	return pStack;` |
|    22715 |  1778 |  |
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
|   700002 |  2167 | `static sxi32 VmInitCallContext(` |
|        - |  2168 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  2169 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  2170 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  2171 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  2172 | `	sxi32 iFlags          /* Control flags */` |
|        - |  2173 | `	)` |
|        2 |  2174 |  |
|   700004 |  2175 | `	pOut->pFunc = pFunc;` |
|   700004 |  2176 | `	pOut->pVm   = pVm;` |
|   700004 |  2177 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   700004 |  2178 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  2179 | `	/* Assume a null return value */` |
|   700004 |  2180 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   700004 |  2181 | `	pOut->pRet = pRet;` |
|   700004 |  2182 | `	pOut->iFlags = iFlags;` |
|   700004 |  2183 | `	return SXRET_OK;` |
|        2 |  2184 |  |
|        - |  2185 | `/*` |
|        - |  2186 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  2187 | ` * left behind.` |
|        - |  2188 | ` */` |
|   700002 |  2189 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  2190 |  |
|        - |  2191 | `	sxu32 n;` |
|   700004 |  2192 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     8676 |  2193 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    25362 |  2194 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    16688 |  2195 | `			if( apObj[n] == 0 ){` |
|        - |  2196 | `				/* Already released */` |
|      384 |  2197 | `				continue;` |
|        - |  2198 | `			}` |
|    16306 |  2199 | `			PH7_MemObjRelease(apObj[n]);` |
|    16306 |  2200 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     8154 |  2201 | `		}` |
|     8676 |  2202 | `		SySetRelease(&pCtx->sVar);` |
|     4337 |  2203 | `	}` |
|   700004 |  2204 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
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
|   700004 |  2220 |  |
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
|  3962440 |  2251 | `static void VmPopOperand(` |
|        - |  2252 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  2253 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  2254 | `	)` |
|        2 |  2255 |  |
|  3962442 |  2256 | `	ph7_value *pTos = *ppTos;` |
|  8443036 |  2257 | `	while( nPop > 0 ){` |
|  4480596 |  2258 | `		PH7_MemObjRelease(pTos);` |
|  4480596 |  2259 | `		pTos--;` |
|  4480596 |  2260 | `		nPop--;` |
|        2 |  2261 | `	}` |
|        - |  2262 | `	/* Top of the stack */` |
|  3962442 |  2263 | `	*ppTos = pTos;` |
|  3962442 |  2264 |  |
|        - |  2265 | `/*` |
|        - |  2266 | ` * Reserve a memory object.` |
|        - |  2267 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  2268 | ` */` |
|  3208662 |  2269 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  2270 |  |
|  3208664 |  2271 | `	ph7_value *pObj = 0;` |
|        - |  2272 | `	VmSlot *pSlot;` |
|        - |  2273 | `	sxu32 nIdx;` |
|        - |  2274 | `	/* Check for a free slot */` |
|  3208664 |  2275 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3208664 |  2276 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3208664 |  2277 | `	if( pSlot ){` |
|  1056578 |  2278 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|  1056578 |  2279 | `		nIdx = pSlot->nIdx;` |
|   528288 |  2280 | `	}` |
|  3208664 |  2281 | `	if( pObj == 0 ){` |
|        - |  2282 | `		/* Reserve a new memory object */` |
|  2152088 |  2283 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2152088 |  2284 | `		if( pObj == 0 ){` |
|      ! 0 |  2285 | `			return 0;` |
|        - |  2286 | `		}` |
|  1076043 |  2287 | `	}` |
|        - |  2288 | `	/* Set a null default value */` |
|  3208664 |  2289 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3208664 |  2290 | `	pObj->nIdx = nIdx;` |
|  3208664 |  2291 | `	return pObj;` |
|  1604333 |  2292 |  |
|        - |  2293 | `/*` |
|        - |  2294 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  2295 | ` */` |
|    35484 |  2296 | `static sxi32 VmHashmapRefInsert(` |
|        - |  2297 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  2298 | `	const char *zKey,  /* Entry key */` |
|        - |  2299 | `	sxu32 nByte,       /* Key length */` |
|        - |  2300 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  2301 | `	)` |
|        2 |  2302 |  |
|        - |  2303 | `	ph7_value sKey;` |
|        - |  2304 | `	sxi32 rc;` |
|    35486 |  2305 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    35486 |  2306 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  2307 | `	/* Perform the insertion */` |
|    35486 |  2308 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    35486 |  2309 | `	PH7_MemObjRelease(&sKey);` |
|    35486 |  2310 | `	return rc;` |
|        2 |  2311 |  |
|        - |  2312 | `/*` |
|        - |  2313 | ` * Extract a variable value from the top active VM frame.` |
|        - |  2314 | ` * Return a pointer to the variable value on success.` |
|        - |  2315 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  2316 | ` */` |
|  3680916 |  2317 | `static ph7_value * VmExtractMemObj(` |
|        - |  2318 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  2319 | `	const SyString *pName, /* Variable name */` |
|        - |  2320 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  2321 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  2322 | `	)` |
|        2 |  2323 |  |
|  3680918 |  2324 | `	int bNullify = FALSE;` |
|        - |  2325 | `	SyHashEntry *pEntry;` |
|        - |  2326 | `	VmFrame *pFrame;` |
|        - |  2327 | `	ph7_value *pObj;` |
|        - |  2328 | `	sxu32 nIdx;` |
|        - |  2329 | `	sxi32 rc;` |
|        - |  2330 | `	/* Point to the top active frame */` |
|  3680918 |  2331 | `	pFrame = pVm->pFrame;` |
|  3680918 |  2332 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  2333 | `	/* Perform the lookup */` |
|  3680918 |  2334 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  2335 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  2336 | `		pName = &sAnnon;` |
|        - |  2337 | `		/* Always nullify the object */` |
|      ! 0 |  2338 | `		bNullify = TRUE;` |
|      ! 0 |  2339 | `		bDup = FALSE;` |
|      ! 0 |  2340 | `	}` |
|        - |  2341 | `	/* Check the superglobals table first */` |
|  3680918 |  2342 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3680918 |  2343 | `	if( pEntry == 0 ){` |
|        - |  2344 | `		/* Query the top active frame */` |
|  3680872 |  2345 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3680872 |  2346 | `		if( pEntry == 0 ){` |
|   113846 |  2347 | `			char *zName = (char *)pName->zString;` |
|        - |  2348 | `			VmSlot sLocal;` |
|   113846 |  2349 | `			if( !bCreate ){` |
|        - |  2350 | `				/* Do not create the variable,return NULL instead */` |
|      986 |  2351 | `				return 0;` |
|        - |  2352 | `			}` |
|        - |  2353 | `			/* No such variable,automatically create a new one and install` |
|        - |  2354 | `			 * it in the current frame.` |
|        - |  2355 | `			 */` |
|   112862 |  2356 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|   112862 |  2357 | `			if( pObj == 0 ){` |
|      ! 0 |  2358 | `				return 0;` |
|        - |  2359 | `			}` |
|   112862 |  2360 | `			nIdx = pObj->nIdx;` |
|   112862 |  2361 | `			if( bDup ){` |
|        - |  2362 | `				/* Duplicate name */` |
|      230 |  2363 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      230 |  2364 | `				if( zName == 0 ){` |
|      ! 0 |  2365 | `					return 0;` |
|        - |  2366 | `				}` |
|      114 |  2367 | `			}` |
|        - |  2368 | `			/* Link to the top active VM frame */` |
|   112862 |  2369 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|   112862 |  2370 | `			if( rc != SXRET_OK ){` |
|        - |  2371 | `				/* Return the slot to the free pool */` |
|      ! 0 |  2372 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  2373 | `				sLocal.pUserData = 0;` |
|      ! 0 |  2374 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  2375 | `				return 0;` |
|        - |  2376 | `			}` |
|   112862 |  2377 | `			if( pFrame->pParent != 0 ){` |
|        - |  2378 | `				/* Local variable */` |
|   105810 |  2379 | `				sLocal.nIdx = nIdx;` |
|   105810 |  2380 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    52906 |  2381 | `			}else{` |
|        - |  2382 | `				/* Register in the $GLOBALS array */` |
|     7054 |  2383 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  2384 | `			}` |
|        - |  2385 | `			/* Install in the reference table */` |
|   112862 |  2386 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  2387 | `			/* Save object index */` |
|   112862 |  2388 | `			pObj->nIdx = nIdx;` |
|    56432 |  2389 | `		}else{` |
|        - |  2390 | `			/* Extract variable contents */` |
|  3567028 |  2391 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3567028 |  2392 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3567028 |  2393 | `			if( bNullify && pObj ){` |
|      ! 0 |  2394 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  2395 | `			}` |
|        - |  2396 | `		}` |
|  1840055 |  2397 | `	}else{` |
|        - |  2398 | `		/* Superglobal */` |
|       48 |  2399 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       48 |  2400 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  2401 | `	}` |
|  3679934 |  2402 | `	return pObj;` |
|  1840570 |  2403 |  |
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
|    31704 |  3003 | `static int VmRecursionExceeded(ph7_vm *pVm)` |
|        2 |  3004 |  |
|    31706 |  3005 | `	return pVm->nRecursionDepth > pVm->nMaxDepth;` |
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
|      134 |  3289 | `static int VmCheckPseudoType(ph7_vm *pVm, ph7_value *pValue, const SyString *pClass)` |
|        2 |  3290 |  |
|      136 |  3291 | `	const char *z = pClass->zString;` |
|      136 |  3292 | `	sxu32 n = pClass->nByte;` |
|      136 |  3293 | `	if( n == 5 && SyStrnicmp(z,"mixed",5) == 0 ){` |
|       27 |  3294 | ``		return 1; /* `mixed` accepts any value, including null */`` |
|        - |  3295 | `	}` |
|      110 |  3296 | `	if( n == 4 && SyStrnicmp(z,"true",4) == 0 ){` |
|       15 |  3297 | `		return ( (pValue->iFlags & MEMOBJ_BOOL) && pValue->x.iVal != 0 ) ? 1 : 0;` |
|        - |  3298 | `	}` |
|       96 |  3299 | `	if( n == 5 && SyStrnicmp(z,"false",5) == 0 ){` |
|        3 |  3300 | `		return ( (pValue->iFlags & MEMOBJ_BOOL) && pValue->x.iVal == 0 ) ? 1 : 0;` |
|        - |  3301 | `	}` |
|       94 |  3302 | `	if( n == 8 && SyStrnicmp(z,"iterable",8) == 0 ){` |
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
|       78 |  3315 | `	return -1;` |
|       69 |  3316 |  |
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
|     6510 |  3540 | `static sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|        2 |  3541 |  |
|        - |  3542 | `	SyHashEntry *pSlot;` |
|        - |  3543 | `	VmClassAttr *pVmAttr;` |
|        - |  3544 | `	ph7_class_attr *pAttr;` |
|     6512 |  3545 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|     6512 |  3546 | `	if( pSlot == 0 ){` |
|     6286 |  3547 | `		return SXRET_OK; /* Not a typed slot */` |
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
|     3257 |  3676 |  |
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
|       26 |  3794 | `static const char * VmValueGivenName(ph7_value *pVal,char *zBuf,sxu32 nBuf)` |
|        2 |  3795 |  |
|       28 |  3796 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|       10 |  3797 | `		return pVal->x.iVal ? "true" : "false";` |
|        - |  3798 | `	}` |
|       20 |  3799 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|        5 |  3800 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|        5 |  3801 | `		if( pThis && pThis->pClass ){` |
|        5 |  3802 | `			SyString *pName = &pThis->pClass->sName;` |
|        5 |  3803 | `			sxu32 n = pName->nByte;` |
|        5 |  3804 | `			if( n >= nBuf ){` |
|      ! 0 |  3805 | `				n = nBuf - 1;` |
|      ! 0 |  3806 | `			}` |
|        5 |  3807 | `			SyMemcpy(pName->zString,zBuf,n);` |
|        5 |  3808 | `			zBuf[n] = 0;` |
|        5 |  3809 | `			return zBuf;` |
|        - |  3810 | `		}` |
|      ! 0 |  3811 | `		return "object";` |
|        - |  3812 | `	}` |
|       16 |  3813 | `	return ph7_type_name(pVal);` |
|       15 |  3814 |  |
|        - |  3815 | `/*` |
|        - |  3816 | ` * Throw a PHP-compatible Error when array unpacking ('...$expr') receives a` |
|        - |  3817 | ` * non-array value at runtime. Matches the message and class PHP raises` |
|        - |  3818 | ` * ("Only arrays and Traversables can be unpacked, X given"). The class is` |
|        - |  3819 | ` * \TypeError for objects, \Error otherwise — matching PHP's distinction.` |
|        - |  3820 | ` */` |
|       16 |  3821 | `static sxi32 VmThrowSpreadError(ph7_vm *pVm,ph7_value *pBad)` |
|        1 |  3822 |  |
|        - |  3823 | `	ph7_class *pClass;` |
|        - |  3824 | `	ph7_class_instance *pThis;` |
|        - |  3825 | `	ph7_class_method *pCons;` |
|        - |  3826 | `	ph7_value sArg;` |
|        - |  3827 | `	ph7_value *apArg[1];` |
|        - |  3828 | `	SyBlob sMsg;` |
|        - |  3829 | `	SyString sMsgStr;` |
|        - |  3830 | `	VmFrame *pFrame;` |
|        - |  3831 | `	sxi32 rc;` |
|       17 |  3832 | `	const char *zErrClass = (pBad->iFlags & MEMOBJ_OBJ) ? "TypeError" : "Error";` |
|        - |  3833 | `	char zNameBuf[64];` |
|       17 |  3834 | `	const char *zGiven = VmValueGivenName(pBad,zNameBuf,sizeof(zNameBuf));` |
|       17 |  3835 | `	pClass = PH7_VmExtractClass(&(*pVm),zErrClass,SyStrlen(zErrClass),TRUE,0);` |
|       17 |  3836 | `	if( pClass == 0 ){` |
|      ! 0 |  3837 | `		return PH7_ABORT;` |
|        - |  3838 | `	}` |
|       17 |  3839 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       17 |  3840 | `	if( pThis == 0 ){` |
|      ! 0 |  3841 | `		return PH7_ABORT;` |
|        - |  3842 | `	}` |
|       17 |  3843 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       17 |  3844 | `	SyBlobFormat(&sMsg,"Only arrays and Traversables can be unpacked, %s given",zGiven);` |
|       17 |  3845 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       17 |  3846 | `	if( pCons ){` |
|       17 |  3847 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       17 |  3848 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       17 |  3849 | `		apArg[0] = &sArg;` |
|       17 |  3850 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       17 |  3851 | `		PH7_MemObjRelease(&sArg);` |
|        8 |  3852 | `	}` |
|       17 |  3853 | `	SyBlobRelease(&sMsg);` |
|       17 |  3854 | `	pFrame = pVm->pFrame;` |
|       17 |  3855 | `	if( pFrame ){` |
|       17 |  3856 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       17 |  3857 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        8 |  3858 | `	}` |
|       17 |  3859 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       17 |  3860 | `	PH7_ClassInstanceUnref(pThis);` |
|       17 |  3861 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  3862 | `		return PH7_ABORT;` |
|        - |  3863 | `	}` |
|       17 |  3864 | `	return PH7_EXCEPTION;` |
|        9 |  3865 |  |
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
|       32 |  3877 | `static const char *VmSyStringToCStr(const SyString *pStr, char *zBuf, sxu32 nBuf)` |
|        2 |  3878 |  |
|        - |  3879 | `	sxu32 nCopy;` |
|       34 |  3880 | `	if( nBuf == 0 ) return "";` |
|       34 |  3881 | `	if( pStr == 0 \|\| pStr->zString == 0 ){` |
|      ! 0 |  3882 | `		zBuf[0] = 0;` |
|      ! 0 |  3883 | `		return zBuf;` |
|        - |  3884 | `	}` |
|       34 |  3885 | `	nCopy = SyStringLength(pStr);` |
|       34 |  3886 | `	if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|       34 |  3887 | `	if( nCopy > 0 ) SyMemcpy(pStr->zString, zBuf, nCopy);` |
|       34 |  3888 | `	zBuf[nCopy] = 0;` |
|       34 |  3889 | `	return zBuf;` |
|       18 |  3890 |  |
|        - |  3891 |  |
|      412 |  3892 | `static sxi32 VmEnforceReturnType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue)` |
|        2 |  3893 |  |
|      414 |  3894 | `	int bStrict = pFunc->bStrictTypes ? 1 : 0;` |
|        - |  3895 | `	const char *zGiven;` |
|        - |  3896 | `	char zBuf[128];` |
|        - |  3897 | `	char zTypeBuf[128];` |
|        - |  3898 | `	/* Untyped function: no enforcement. */` |
|      414 |  3899 | `	if( pFunc->nReturnType == 0 ){` |
|      ! 0 |  3900 | `		return SXRET_OK;` |
|        - |  3901 | `	}` |
|        - |  3902 | `	/* void return type: the function must not produce a value. */` |
|      414 |  3903 | `	if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|      136 |  3904 | `		if( pValue == 0 ){` |
|      134 |  3905 | `			return SXRET_OK;` |
|        - |  3906 | `		}` |
|        - |  3907 | ``		/* PHP allows `return;` but rejects `return null;` — iP1=1 with NULL`` |
|        - |  3908 | `		 * still counts as "returned a value" here. */` |
|        3 |  3909 | `		zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|        3 |  3910 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,"void",zGiven);` |
|        - |  3911 | `	}` |
|        - |  3912 | `	/* Function fell off the end without an explicit return: PHP implicitly` |
|        - |  3913 | ``	 * returns null. For a typed non-nullable return (including `mixed`, which`` |
|        - |  3914 | `	 * requires an explicit returned value), that's a TypeError. */` |
|      280 |  3915 | `	if( pValue == 0 ){` |
|      ! 0 |  3916 | `		const char *zExpected = "value";` |
|      ! 0 |  3917 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      ! 0 |  3918 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  3919 | `		}` |
|      ! 0 |  3920 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,"null");` |
|        - |  3921 | `	}` |
|        - |  3922 | ``	/* standalone `null` return type (PHP 8.2): an explicit non-null return is a`` |
|        - |  3923 | `	 * TypeError. (Falling off the end is handled by the generic check above,` |
|        - |  3924 | `	 * matching how every other typed return reports a missing value.) */` |
|      280 |  3925 | `	if( pFunc->nReturnType == MEMOBJ_NULL ){` |
|        5 |  3926 | `		if( pValue->iFlags & MEMOBJ_NULL ){` |
|        3 |  3927 | `			return SXRET_OK;` |
|        - |  3928 | `		}` |
|        4 |  3929 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,"null",` |
|        1 |  3930 | `			VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3931 | `	}` |
|        - |  3932 | ``	/* Pseudo-types parsed as class-name atoms: `mixed` (any value),`` |
|        - |  3933 | ``	 * `true`/`false` (the matching bool literal), `iterable` (array\|Traversable).`` |
|        - |  3934 | `	 * Check by value before the real-class instanceof branch below. */` |
|      276 |  3935 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|       38 |  3936 | `		int rcPseudo = VmCheckPseudoType(pVm, pValue, &pFunc->sReturnClass);` |
|       38 |  3937 | `		if( rcPseudo == 1 ){` |
|       29 |  3938 | `			return SXRET_OK;` |
|        - |  3939 | `		}` |
|       10 |  3940 | `		if( rcPseudo == 0 ){` |
|        9 |  3941 | `			return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        4 |  3942 | `				VmSyStringToCStr(&pFunc->sReturnClass,zTypeBuf,sizeof(zTypeBuf)),` |
|        2 |  3943 | `				VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3944 | `		}` |
|        - |  3945 | `		/* rcPseudo == -1: a real class — fall through to the instanceof branch. */` |
|        2 |  3946 | `	}` |
|        - |  3947 | `	/* Union return type — delegate. The function has no flag for nullable` |
|        - |  3948 | `	 * unions; a null alternative is represented inside aReturnUnion, so pass` |
|        - |  3949 | `	 * bNullable=0 here. */` |
|      244 |  3950 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
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
|      244 |  3981 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|        6 |  3982 | `		SyString *pClassName = &pFunc->sReturnClass;` |
|        - |  3983 | `		const char *zExpected;` |
|        - |  3984 | `		ph7_class *pExpected;` |
|        6 |  3985 | `		ph7_class *pSelfNow = 0;` |
|        6 |  3986 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|        6 |  3987 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|        6 |  3988 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        2 |  3989 | `		}` |
|        6 |  3990 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        3 |  3991 | `			pExpected = pSelfNow;` |
|        4 |  3992 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  3993 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3994 | `		}else{` |
|        3 |  3995 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  3996 | `		}` |
|        6 |  3997 | `		zExpected = VmSyStringToCStr(pClassName, zTypeBuf, sizeof(zTypeBuf));` |
|        6 |  3998 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  3999 | `			zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|      ! 0 |  4000 | `			return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  4001 | `		}` |
|        6 |  4002 | `		if( pExpected ){` |
|        6 |  4003 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|        6 |  4004 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|      ! 0 |  4005 | `				zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  4006 | `				return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  4007 | `			}` |
|        2 |  4008 | `		}` |
|        6 |  4009 | `		return SXRET_OK;` |
|        - |  4010 | `	}` |
|        - |  4011 | `	/* Scalar return type. Allow null pass-through if the function is` |
|        - |  4012 | `	 * nullable (textual "?T" gets that flag, though union+null is handled` |
|        - |  4013 | `	 * above). There's no explicit nullable flag on ph7_vm_func, so detect` |
|        - |  4014 | `	 * via the type-text leading '?'. */` |
|      240 |  4015 | `	if( (pValue->iFlags & MEMOBJ_NULL) ){` |
|        6 |  4016 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0` |
|        8 |  4017 | `		 && pFunc->sReturnTypeName.zString[0] == '?' ){` |
|        8 |  4018 | `			return SXRET_OK;` |
|        - |  4019 | `		}` |
|      ! 0 |  4020 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  4021 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - |  4022 | `			"null");` |
|        - |  4023 | `	}` |
|        - |  4024 | `	/* Exact match? Done. */` |
|      234 |  4025 | `	if( pValue->iFlags & pFunc->nReturnType ){` |
|      228 |  4026 | `		return SXRET_OK;` |
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
|      208 |  4059 |  |
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
|      884 |  4174 | `static void VmCoalesceDisarm(ph7_vm *pVm)` |
|        2 |  4175 |  |
|      886 |  4176 | `	if( pVm->bCoalesceArmed ){` |
|        7 |  4177 | `		if( pVm->pCoalesceObj ){` |
|        7 |  4178 | `			PH7_ClassInstanceUnref(pVm->pCoalesceObj);` |
|        3 |  4179 | `		}` |
|        7 |  4180 | `		PH7_MemObjRelease(&pVm->sCoalesceKey);` |
|        7 |  4181 | `		pVm->pCoalesceObj = 0;` |
|        7 |  4182 | `		pVm->bCoalesceArmed = 0;` |
|        3 |  4183 | `	}` |
|      886 |  4184 |  |
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
|      144 |  4348 | `static sxi32 VmSuspendCtx(` |
|        - |  4349 | `	ph7_vm *pVm,` |
|        - |  4350 | `	ph7_exec_ctx *pCtx,` |
|        - |  4351 | `	sxi32 pc,` |
|        - |  4352 | `	sxi32 nTos` |
|        - |  4353 | `	)` |
|        2 |  4354 |  |
|       72 |  4355 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      146 |  4356 | `	pCtx->pc = pc;` |
|      146 |  4357 | `	pCtx->nTos = nTos;` |
|      146 |  4358 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      146 |  4359 | `	return PH7_SUSPEND;` |
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
|        - |  4447 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  4448 | ` *` |
|        - |  4449 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  4450 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  4451 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  4452 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  4453 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  4454 | ` * then the program execution is halted.` |
|        - |  4455 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  4456 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  4457 | ` * or to reset the VM to it's initial state.` |
|        - |  4458 | ` */` |
|    45526 |  4459 | `static sxi32 VmByteCodeExec(` |
|        - |  4460 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  4461 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  4462 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  4463 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  4464 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  4465 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  4466 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  4467 | `	sxi32 nPc,           /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  4468 | `	ph7_vm_func *pEnforceRetFunc /* NULL except when this invocation is a user-fn body; when set, the terminating OP_DONE validates the return value against pEnforceRetFunc's declared type. */` |
|        - |  4469 | `	)` |
|        2 |  4470 |  |
|        - |  4471 | `	VmInstr *pInstr;` |
|        - |  4472 | `	ph7_value *pTos;` |
|        - |  4473 | `	SySet aArg;` |
|        - |  4474 | `	sxu32 nExceptionBase; /* Exception stack depth at entry (for finally drain guard) */` |
|        - |  4475 | `	sxi32 pc;` |
|        - |  4476 | `	sxi32 rc;` |
|        - |  4477 | `	/* Argument container */` |
|    45528 |  4478 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    45528 |  4479 | `	if( nTos < 0 ){` |
|    42336 |  4480 | `		pTos = &pStack[-1];` |
|    21169 |  4481 | `	}else{` |
|     3194 |  4482 | `		pTos = &pStack[nTos];` |
|        - |  4483 | `	}` |
|    45528 |  4484 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    45528 |  4485 | `	pc = nPc;` |
|        - |  4486 | `/*` |
|        - |  4487 | ` * Typed-property enforcement helper for compound stores. Called before` |
|        - |  4488 | ` * PH7_MemObjStore writes into a member memobj slot. On failure throws a` |
|        - |  4489 | ` * PHP TypeError and either jumps to the nearest catch block or propagates` |
|        - |  4490 | ` * out of the VM loop. Must be used inside a case of the main switch.` |
|        - |  4491 | ` */` |
|        - |  4492 | `#define PH7_ENFORCE_TYPED_STORE(nIdxArg, pSrcArg) \` |
|        - |  4493 | `	{ \` |
|        - |  4494 | `		sxi32 _rcT = VmEnforcePropertyTypeOnStore(&(*pVm),(nIdxArg),(pSrcArg)); \` |
|        - |  4495 | `		if( _rcT == PH7_ABORT ){ goto Abort; } \` |
|        - |  4496 | `		if( _rcT == PH7_EXCEPTION ){ \` |
|        - |  4497 | `			VmFrame *_pFrmT = pVm->pFrame; \` |
|        - |  4498 | `			if( _pFrmT && (_pFrmT->iFlags & VM_FRAME_EXCEPTION) && _pFrmT->iExceptionJump > 0 ){ \` |
|        - |  4499 | `				pc = _pFrmT->iExceptionJump - 1; \` |
|        - |  4500 | `				break; \` |
|        - |  4501 | `			} \` |
|        - |  4502 | `			goto Exception; \` |
|        - |  4503 | `		} \` |
|        - |  4504 | `	}` |
|        - |  4505 | `	/* Execute as much as we can */` |
|  5925964 |  4506 | `	for(;;){` |
|        - |  4507 | `		/* Fetch the instruction to execute */` |
| 11851226 |  4508 | `		pInstr = &aInstr[pc];` |
| 11851226 |  4509 | `		rc = SXRET_OK;` |
|        - |  4510 | `/*` |
|        - |  4511 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  4512 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  4513 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  4514 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  4515 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  4516 | ` */` |
| 11851226 |  4517 | `		switch(pInstr->iOp){` |
|        - |  4518 | `/*` |
|        - |  4519 | ` * DONE: P1 * *` |
|        - |  4520 | ` *` |
|        - |  4521 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  4522 | ` * and return immediately.` |
|        - |  4523 | ` */` |
|    22316 |  4524 | `case PH7_OP_DONE:` |
|        - |  4525 | `	/* Return-type enforcement: only the user-function CALL handler (and` |
|        - |  4526 | `	 * the fiber start/resume paths) set pEnforceRetFunc, so this branch is` |
|        - |  4527 | `	 * skipped for default-value bytecode, class-method mini-programs,` |
|        - |  4528 | `	 * callback trampolines, and the main script. */` |
|    44632 |  4529 | `	if( pEnforceRetFunc && pEnforceRetFunc->nReturnType > 0` |
|      418 |  4530 | `	 && !(VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW) ){` |
|        - |  4531 | `		/* The VM_FRAME_THROW guard skips enforcement when the function is` |
|        - |  4532 | `		 * unwinding because an exception was thrown (the compiler routes an` |
|        - |  4533 | `		 * uncaught throw to this terminal OP_DONE): PHP does not type-check a` |
|        - |  4534 | `		 * value the function never actually returned, so enforcing here would` |
|        - |  4535 | `		 * raise a spurious "Return value must be of type X" over the real` |
|        - |  4536 | `		 * exception. */` |
|      414 |  4537 | `		ph7_value *pRetVal = 0;` |
|      414 |  4538 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|      282 |  4539 | `			pRetVal = pTos;` |
|      140 |  4540 | `		}` |
|      414 |  4541 | `		rc = VmEnforceReturnType(&(*pVm), pEnforceRetFunc, pRetVal);` |
|      414 |  4542 | `		if( rc == PH7_ABORT ) goto Abort;` |
|      408 |  4543 | `		if( rc == PH7_EXCEPTION ){` |
|        7 |  4544 | `			if( pInstr->iP1 && pTos >= pStack ){` |
|        7 |  4545 | `				PH7_MemObjRelease(pTos);` |
|        7 |  4546 | `				pTos--;` |
|        3 |  4547 | `			}` |
|        7 |  4548 | `			goto Exception;` |
|        - |  4549 | `		}` |
|        - |  4550 | `		/* Don't enforce twice if the function loops through multiple` |
|        - |  4551 | `		 * OP_DONEs (it shouldn't — compilers emit one terminal DONE — but` |
|        - |  4552 | `		 * defensively we clear the pointer after a successful check). */` |
|      402 |  4553 | `		pEnforceRetFunc = 0;` |
|      200 |  4554 | `	}` |
|    44622 |  4555 | `	if( pInstr->iP1 ){` |
|        - |  4556 | `#ifdef UNTRUST` |
|        - |  4557 | `		if( pTos < pStack ){` |
|        - |  4558 | `			goto Abort;` |
|        - |  4559 | `		}` |
|        - |  4560 | `#endif` |
|    27140 |  4561 | `		if( pLastRef ){` |
|    16484 |  4562 | `			*pLastRef = pTos->nIdx;` |
|     8241 |  4563 | `		}` |
|    27140 |  4564 | `		if( pResult ){` |
|        - |  4565 | `			/* Execution result */` |
|    25622 |  4566 | `			PH7_MemObjStore(pTos,pResult);` |
|    12810 |  4567 | `		}` |
|    27140 |  4568 | `		VmPopOperand(&pTos,1);` |
|    31053 |  4569 | `	}else if( pLastRef ){` |
|        - |  4570 | `		/* Nothing referenced */` |
|     2006 |  4571 | `		*pLastRef = SXU32_HIGH;` |
|     1002 |  4572 | `	}` |
|        - |  4573 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  4574 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  4575 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  4576 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  4577 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  4578 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  4579 | `	 * block can override it.` |
|        - |  4580 | `	 */` |
|    44624 |  4581 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
|        3 |  4582 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        3 |  4583 | `		ph7_exception *pExc = apExc[SySetUsed(&pVm->aException) - 1];` |
|        3 |  4584 | `		(void)SySetPop(&pVm->aException);` |
|        3 |  4585 | `		pExc->pFrame = 0;` |
|        3 |  4586 | `		VmLeaveFrame(&(*pVm));` |
|        3 |  4587 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|        3 |  4588 | `			pExc->iFinallyDone = 1;` |
|        - |  4589 | `			/* Pass pResult so that 'return' inside finally can override the value */` |
|        3 |  4590 | `			rc = VmLocalExec(&(*pVm),&pExc->sFinally,pResult);` |
|        3 |  4591 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4592 | `				goto Abort;` |
|        - |  4593 | `			}` |
|        1 |  4594 | `		}` |
|        1 |  4595 | `	}` |
|    44622 |  4596 | `	goto Done;` |
|        - |  4597 | `/*` |
|        - |  4598 | ` * HALT: P1 * *` |
|        - |  4599 | ` *` |
|        - |  4600 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  4601 | ` * and abort immediately.` |
|        - |  4602 | ` */` |
|        7 |  4603 | `case PH7_OP_HALT:` |
|       15 |  4604 | `	if( pInstr->iP1 ){` |
|        - |  4605 | `#ifdef UNTRUST` |
|        - |  4606 | `		if( pTos < pStack ){` |
|        - |  4607 | `			goto Abort;` |
|        - |  4608 | `		}` |
|        - |  4609 | `#endif` |
|       15 |  4610 | `		if( pLastRef ){` |
|        3 |  4611 | `			*pLastRef = pTos->nIdx;` |
|        1 |  4612 | `		}` |
|       15 |  4613 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|       11 |  4614 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  4615 | `				/* Output the exit message */` |
|       16 |  4616 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        5 |  4617 | `					pVm->sVmConsumer.pUserData);` |
|       11 |  4618 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|        6 |  4619 | `			}` |
|       10 |  4620 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  4621 | `			/* Record exit status */` |
|        5 |  4622 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  4623 | `		}` |
|       15 |  4624 | `		VmPopOperand(&pTos,1);` |
|        7 |  4625 | `	}else if( pLastRef ){` |
|        - |  4626 | `		/* Nothing referenced */` |
|      ! 0 |  4627 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  4628 | `	}` |
|        - |  4629 | `	/* Request a VM-wide halt so the abort cascades out of any enclosing` |
|        - |  4630 | `	 * include/require/eval execution unit; shutdown callbacks then run` |
|        - |  4631 | `	 * at the top level (PHP semantics) instead of hard-exiting here.` |
|        - |  4632 | `	 */` |
|       15 |  4633 | `	pVm->bHaltRequested = 1;` |
|       15 |  4634 | `	goto Abort;` |
|        - |  4635 | `/*` |
|        - |  4636 | ` * JMP: * P2 *` |
|        - |  4637 | ` *` |
|        - |  4638 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  4639 | ` * the one at index P2 from the beginning of the program.` |
|        - |  4640 | ` */` |
|   252473 |  4641 | `case PH7_OP_JMP:` |
|   504992 |  4642 | `	pc = pInstr->iP2 - 1;` |
|   504992 |  4643 | `	break;` |
|        - |  4644 | `/*` |
|        - |  4645 | ` * JZ: P1 P2 *` |
|        - |  4646 | ` *` |
|        - |  4647 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  4648 | ` * entry in the stack if P1 is zero.` |
|        - |  4649 | ` */` |
|   599485 |  4650 | `case PH7_OP_JZ:` |
|        - |  4651 | `#ifdef UNTRUST` |
|        - |  4652 | `	if( pTos < pStack ){` |
|        - |  4653 | `		goto Abort;` |
|        - |  4654 | `	}` |
|        - |  4655 | `#endif` |
|        - |  4656 | `	/* Get a boolean value */` |
|  1199060 |  4657 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      174 |  4658 | `		PH7_MemObjToBool(pTos);` |
|       86 |  4659 | `	}` |
|  1199060 |  4660 | `	if( !pTos->x.iVal ){` |
|        - |  4661 | `		/* Take the jump */` |
|   616944 |  4662 | `		pc = pInstr->iP2 - 1;` |
|   308471 |  4663 | `	}` |
|  1199060 |  4664 | `	if( !pInstr->iP1 ){` |
|   949730 |  4665 | `		VmPopOperand(&pTos,1);` |
|   474886 |  4666 | `	}` |
|  1199060 |  4667 | `	break;` |
|        - |  4668 | `/*` |
|        - |  4669 | ` * JNZ: P1 P2 *` |
|        - |  4670 | ` *` |
|        - |  4671 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  4672 | ` * entry in the stack if P1 is zero.` |
|        - |  4673 | ` */` |
|    61433 |  4674 | `case PH7_OP_JNZ:` |
|        - |  4675 | `#ifdef UNTRUST` |
|        - |  4676 | `	if( pTos < pStack ){` |
|        - |  4677 | `		goto Abort;` |
|        - |  4678 | `	}` |
|        - |  4679 | `#endif` |
|        - |  4680 | `	/* Get a boolean value */` |
|   122868 |  4681 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4682 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4683 | `	}` |
|   122868 |  4684 | `	if( pTos->x.iVal ){` |
|        - |  4685 | `		/* Take the jump */` |
|     5624 |  4686 | `		pc = pInstr->iP2 - 1;` |
|     2811 |  4687 | `	}` |
|   122868 |  4688 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  4689 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4690 | `	}` |
|   122868 |  4691 | `	break;` |
|        - |  4692 | `/*` |
|        - |  4693 | ` * NOOP: * * *` |
|        - |  4694 | ` *` |
|        - |  4695 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  4696 | ` * destination.` |
|        - |  4697 | ` */` |
|      ! 0 |  4698 | `case PH7_OP_NOOP:` |
|      ! 0 |  4699 | `	break;` |
|        - |  4700 | `/*` |
|        - |  4701 | ` * POP: P1 * *` |
|        - |  4702 | ` *` |
|        - |  4703 | ` * Pop P1 elements from the operand stack.` |
|        - |  4704 | ` */` |
|   464590 |  4705 | `case PH7_OP_POP: {` |
|   929226 |  4706 | `	sxi32 n = pInstr->iP1;` |
|   929226 |  4707 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  4708 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|       50 |  4709 | `		n = (sxi32)(pTos - pStack);` |
|       24 |  4710 | `	}` |
|   929226 |  4711 | `	VmPopOperand(&pTos,n);` |
|   929226 |  4712 | `	break;` |
|        - |  4713 | `				 }` |
|        - |  4714 | `/*` |
|        - |  4715 | ` * DUP: * * *` |
|        - |  4716 | ` *` |
|        - |  4717 | ` * Duplicate the top of the stack.` |
|        - |  4718 | ` */` |
|       41 |  4719 | `case PH7_OP_DUP:` |
|        - |  4720 | `#ifdef UNTRUST` |
|        - |  4721 | `	if( pTos < pStack ){` |
|        - |  4722 | `		goto Abort;` |
|        - |  4723 | `	}` |
|        - |  4724 | `#endif` |
|       84 |  4725 | `	pTos++;` |
|       84 |  4726 | `	PH7_MemObjInit(pVm,pTos);` |
|       84 |  4727 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       84 |  4728 | `	break;` |
|        - |  4729 | `/*` |
|        - |  4730 | ` * NSSWITCH: * * P3` |
|        - |  4731 | ` *` |
|        - |  4732 | ` * Switch the active namespace at runtime.` |
|        - |  4733 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  4734 | ` */` |
|     7877 |  4735 | `case PH7_OP_NSSWITCH:` |
|    15756 |  4736 | `	SyBlobReset(&pVm->sNamespace);` |
|    15756 |  4737 | `	if( pInstr->p3 ){` |
|      100 |  4738 | `		const char *zNs = (const char *)pInstr->p3;` |
|      100 |  4739 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       49 |  4740 | `	}` |
|        - |  4741 | `	/* Clear namespace-scoped use-const imports */` |
|    15756 |  4742 | `	SyHashRelease(&pVm->hUseConstImports);` |
|    15756 |  4743 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|    15756 |  4744 | `	break;` |
|        - |  4745 | `/* OP_USECONST P1 * P3` |
|        - |  4746 | ` * Register a use-const import at runtime. P1 is the alias length,` |
|        - |  4747 | ` * P3 points to a two-pointer array: [0]=alias, [1]=FQN.` |
|        - |  4748 | ` * This is namespace-scoped: NSSWITCH clears all imports.` |
|        - |  4749 | ` */` |
|        7 |  4750 | `case PH7_OP_USECONST: {` |
|       16 |  4751 | `	char **azPair = (char **)pInstr->p3;` |
|       16 |  4752 | `	if( azPair ){` |
|       16 |  4753 | `		SyHashInsert(&pVm->hUseConstImports,azPair[0],(sxu32)pInstr->iP1,azPair[1]);` |
|        7 |  4754 | `	}` |
|       16 |  4755 | `	break;` |
|        - |  4756 | `				}` |
|        - |  4757 | `/*` |
|        - |  4758 | ` * CVT_INT: * * *` |
|        - |  4759 | ` *` |
|        - |  4760 | ` * Force the top of the stack to be an integer.` |
|        - |  4761 | ` */` |
|       80 |  4762 | `case PH7_OP_CVT_INT:` |
|        - |  4763 | `#ifdef UNTRUST` |
|        - |  4764 | `	if( pTos < pStack ){` |
|        - |  4765 | `		goto Abort;` |
|        - |  4766 | `	}` |
|        - |  4767 | `#endif` |
|      162 |  4768 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      111 |  4769 | `		PH7_MemObjToInteger(pTos);` |
|       55 |  4770 | `	}` |
|        - |  4771 | `	/* Invalidate any prior representation */` |
|      162 |  4772 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|      162 |  4773 | `	break;` |
|        - |  4774 | `/*` |
|        - |  4775 | ` * CVT_REAL: * * *` |
|        - |  4776 | ` *` |
|        - |  4777 | ` * Force the top of the stack to be a real.` |
|        - |  4778 | ` */` |
|        7 |  4779 | `case PH7_OP_CVT_REAL:` |
|        - |  4780 | `#ifdef UNTRUST` |
|        - |  4781 | `	if( pTos < pStack ){` |
|        - |  4782 | `		goto Abort;` |
|        - |  4783 | `	}` |
|        - |  4784 | `#endif` |
|       15 |  4785 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       11 |  4786 | `		PH7_MemObjToReal(pTos);` |
|        5 |  4787 | `	}` |
|        - |  4788 | `	/* Invalidate any prior representation */` |
|       15 |  4789 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|       15 |  4790 | `	break;` |
|        - |  4791 | `/*` |
|        - |  4792 | ` * CVT_STR: * * *` |
|        - |  4793 | ` *` |
|        - |  4794 | ` * Force the top of the stack to be a string.` |
|        - |  4795 | ` */` |
|      163 |  4796 | `case PH7_OP_CVT_STR:` |
|        - |  4797 | `#ifdef UNTRUST` |
|        - |  4798 | `	if( pTos < pStack ){` |
|        - |  4799 | `		goto Abort;` |
|        - |  4800 | `	}` |
|        - |  4801 | `#endif` |
|      328 |  4802 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      308 |  4803 | `		PH7_MemObjToString(pTos);` |
|      153 |  4804 | `	}` |
|      328 |  4805 | `	break;` |
|        - |  4806 | `/*` |
|        - |  4807 | ` * CVT_BOOL: * * *` |
|        - |  4808 | ` *` |
|        - |  4809 | ` * Force the top of the stack to be a boolean.` |
|        - |  4810 | ` */` |
|        5 |  4811 | `case PH7_OP_CVT_BOOL:` |
|        - |  4812 | `#ifdef UNTRUST` |
|        - |  4813 | `	if( pTos < pStack ){` |
|        - |  4814 | `		goto Abort;` |
|        - |  4815 | `	}` |
|        - |  4816 | `#endif` |
|       11 |  4817 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  4818 | `		PH7_MemObjToBool(pTos);` |
|        3 |  4819 | `	}` |
|       11 |  4820 | `	break;` |
|        - |  4821 | `/*` |
|        - |  4822 | ` * CVT_NULL: * * *` |
|        - |  4823 | ` *` |
|        - |  4824 | ` * Nullify the top of the stack.` |
|        - |  4825 | ` */` |
|        3 |  4826 | `case PH7_OP_CVT_NULL:` |
|        - |  4827 | `#ifdef UNTRUST` |
|        - |  4828 | `	if( pTos < pStack ){` |
|        - |  4829 | `		goto Abort;` |
|        - |  4830 | `	}` |
|        - |  4831 | `#endif` |
|        7 |  4832 | `	PH7_MemObjRelease(pTos);` |
|        7 |  4833 | `	break;` |
|        - |  4834 | `/*` |
|        - |  4835 | ` * CVT_NUMC: * * *` |
|        - |  4836 | ` *` |
|        - |  4837 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  4838 | ` */` |
|      ! 0 |  4839 | `case PH7_OP_CVT_NUMC:` |
|        - |  4840 | `#ifdef UNTRUST` |
|        - |  4841 | `	if( pTos < pStack ){` |
|        - |  4842 | `		goto Abort;` |
|        - |  4843 | `	}` |
|        - |  4844 | `#endif` |
|        - |  4845 | `	/* Force a numeric cast */` |
|      ! 0 |  4846 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  4847 | `	break;` |
|        - |  4848 | `/*` |
|        - |  4849 | ` * CVT_ARRAY: * * *` |
|        - |  4850 | ` *` |
|        - |  4851 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  4852 | ` */` |
|       10 |  4853 | `case PH7_OP_CVT_ARRAY:` |
|        - |  4854 | `#ifdef UNTRUST` |
|        - |  4855 | `	if( pTos < pStack ){` |
|        - |  4856 | `		goto Abort;` |
|        - |  4857 | `	}` |
|        - |  4858 | `#endif` |
|        - |  4859 | `	/* Force a hashmap cast */` |
|       21 |  4860 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  4861 | `	if( rc != SXRET_OK ){` |
|        - |  4862 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  4863 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  4864 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  4865 | `	}` |
|       21 |  4866 | `	break;` |
|        - |  4867 | `/*` |
|        - |  4868 | ` * CVT_OBJ: * * *` |
|        - |  4869 | ` *` |
|        - |  4870 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  4871 | ` */` |
|        8 |  4872 | `case PH7_OP_CVT_OBJ:` |
|        - |  4873 | `#ifdef UNTRUST` |
|        - |  4874 | `	if( pTos < pStack ){` |
|        - |  4875 | `		goto Abort;` |
|        - |  4876 | `	}` |
|        - |  4877 | `#endif` |
|       17 |  4878 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  4879 | `		/* Force a 'stdClass()' cast */` |
|       17 |  4880 | `		PH7_MemObjToObject(pTos);` |
|        8 |  4881 | `	}` |
|       17 |  4882 | `	break;` |
|        - |  4883 | `/*` |
|        - |  4884 | ` * ERR_CTRL * * *` |
|        - |  4885 | ` *` |
|        - |  4886 | ` * Error control operator.` |
|        - |  4887 | ` */` |
|    16143 |  4888 | `case PH7_OP_ERR_CTRL:` |
|        - |  4889 | `	/*` |
|        - |  4890 | `	 * TICKET 1433-038:` |
|        - |  4891 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  4892 | `	 * use the public API,to control error output.` |
|        - |  4893 | `	 */` |
|    32286 |  4894 | `	break;` |
|        - |  4895 | `/*` |
|        - |  4896 | ` * IS_A * * *` |
|        - |  4897 | ` *` |
|        - |  4898 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  4899 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  4900 | ` * holding a class name or an object).` |
|        - |  4901 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  4902 | ` */` |
|       77 |  4903 | `case PH7_OP_IS_A:{` |
|      156 |  4904 | `	ph7_value *pNos = &pTos[-1];` |
|      156 |  4905 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  4906 | `#ifdef UNTRUST` |
|        - |  4907 | `	if( pNos < pStack ){` |
|        - |  4908 | `		goto Abort;` |
|        - |  4909 | `	}` |
|        - |  4910 | `#endif` |
|      156 |  4911 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|      154 |  4912 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      154 |  4913 | `		ph7_class *pClass = 0;` |
|        - |  4914 | `		/* Extract the target class */` |
|      154 |  4915 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  4916 | `			/* Instance already loaded */` |
|      ! 0 |  4917 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      154 |  4918 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|      154 |  4919 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|      154 |  4920 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  4921 | `			/* Handle self/static/parent keywords */` |
|      154 |  4922 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        5 |  4923 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|      152 |  4924 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  4925 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      151 |  4926 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        5 |  4927 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        5 |  4928 | `				if( pSelf && pSelf->pBase ){` |
|        5 |  4929 | `					pClass = pSelf->pBase;` |
|        2 |  4930 | `				}` |
|        3 |  4931 | `			}else{` |
|      144 |  4932 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  4933 | `			}` |
|       76 |  4934 | `		}` |
|      154 |  4935 | `		if( pClass ){` |
|        - |  4936 | `			/* Perform the query */` |
|      154 |  4937 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       76 |  4938 | `		}` |
|       76 |  4939 | `	}` |
|        - |  4940 | `	/* Push result */` |
|      156 |  4941 | `	VmPopOperand(&pTos,1);` |
|      156 |  4942 | `	PH7_MemObjRelease(pTos);` |
|      156 |  4943 | `	pTos->x.iVal = iRes;` |
|      156 |  4944 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      156 |  4945 | `	break;` |
|        - |  4946 | `				 }` |
|        - |  4947 |  |
|        - |  4948 | `/*` |
|        - |  4949 | ` * LOADC P1 P2 *` |
|        - |  4950 | ` *` |
|        - |  4951 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  4952 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  4953 | ` */` |
|  1020662 |  4954 | `case PH7_OP_LOADC: {` |
|        - |  4955 | `	ph7_value *pObj;` |
|        - |  4956 | `	/* Reserve a room */` |
|  2041370 |  4957 | `	pTos++;` |
|  3052156 |  4958 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  2041370 |  4959 | `		if( (pInstr->iP1 & PH7_LOADC_EXPAND) && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  4960 | `			SyHashEntry *pEntry;` |
|        - |  4961 | `			/* Check use const imports first — imports take precedence */` |
|        - |  4962 | `			{` |
|        - |  4963 | `				SyHashEntry *pConstImport;` |
|    29768 |  4964 | `				pConstImport = SyHashGet(&pVm->hUseConstImports,` |
|    19844 |  4965 | `					SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    19846 |  4966 | `				if( pConstImport ){` |
|       11 |  4967 | `					const char *zFQN = (const char *)pConstImport->pUserData;` |
|       11 |  4968 | `					pEntry = SyHashGet(&pVm->hConstant,zFQN,SyStrlen(zFQN));` |
|       11 |  4969 | `					if( pEntry ){` |
|       11 |  4970 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|       11 |  4971 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 |  4972 | `						SyBlobReset(&pTos->sBlob);` |
|       11 |  4973 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|       11 |  4974 | `						pTos->nIdx = SXU32_HIGH;` |
|       11 |  4975 | `						break;` |
|        - |  4976 | `					}` |
|        - |  4977 | `					/* Import found but constant not defined — fall through */` |
|      ! 0 |  4978 | `				}` |
|        - |  4979 | `			}` |
|        - |  4980 | `			/* Candidate for expansion via user defined callbacks */` |
|    19836 |  4981 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    19836 |  4982 | `			if( pEntry ){` |
|    19830 |  4983 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  4984 | `				/* Set a NULL default value */` |
|    19830 |  4985 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    19830 |  4986 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  4987 | `				/* Invoke the callback and deal with the expanded value */` |
|    19830 |  4988 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  4989 | `				/* Mark as constant */` |
|    19830 |  4990 | `				pTos->nIdx = SXU32_HIGH;` |
|    19830 |  4991 | `				break;` |
|        - |  4992 | `			}` |
|        - |  4993 | `			/* Constant not found by bare name.  If a namespace is active and` |
|        - |  4994 | `			 * the name is unqualified, try namespace\name (PHP resolution order:` |
|        - |  4995 | `			 * use-const imports → current NS → global → string fallback).` |
|        - |  4996 | `			 * Absolute references (\NAME) skip the NS fallback too. */` |
|        - |  4997 | `			{` |
|        8 |  4998 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        8 |  4999 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  5000 | `				sxu32 j;` |
|        8 |  5001 | `				int isQualified = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       24 |  5002 | `				for( j = 0; !isQualified && j < nLit; j++ ){` |
|       18 |  5003 | `					if( zLit[j] == '\\' ){ isQualified = 1; break; }` |
|       10 |  5004 | `				}` |
|        8 |  5005 | `				if( !isQualified && SyBlobLength(&pVm->sNamespace) > 0 ){` |
|        - |  5006 | `					/* Try current_namespace\name */` |
|      ! 0 |  5007 | `					SyBlobReset(&pVm->sWorker);` |
|      ! 0 |  5008 | `					SyBlobAppend(&pVm->sWorker,SyBlobData(&pVm->sNamespace),SyBlobLength(&pVm->sNamespace));` |
|      ! 0 |  5009 | `					SyBlobAppend(&pVm->sWorker,"\\",1);` |
|      ! 0 |  5010 | `					SyBlobAppend(&pVm->sWorker,zLit,nLit);` |
|      ! 0 |  5011 | `					pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pVm->sWorker),SyBlobLength(&pVm->sWorker));` |
|      ! 0 |  5012 | `					if( pEntry ){` |
|      ! 0 |  5013 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|      ! 0 |  5014 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  5015 | `						SyBlobReset(&pTos->sBlob);` |
|      ! 0 |  5016 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|      ! 0 |  5017 | `						pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5018 | `						break;` |
|        - |  5019 | `					}` |
|        - |  5020 | `					/* Not in current namespace either — fall through to global/string */` |
|      ! 0 |  5021 | `				}` |
|        8 |  5022 | `				if( isQualified ){` |
|        - |  5023 | `					/* Qualified name: must be a real constant. */` |
|        3 |  5024 | `					SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  5025 | `					SyBlob sErr;` |
|        3 |  5026 | `					SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  5027 | `					SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  5028 | `					if( pErrFile ){` |
|        3 |  5029 | `						SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  5030 | `					}` |
|        3 |  5031 | `					SyBlobAppend(&sErr,"\n",1);` |
|        3 |  5032 | `					VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  5033 | `					SyBlobRelease(&sErr);` |
|        3 |  5034 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  5035 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 |  5036 | `					goto LoadC_Done;` |
|        - |  5037 | `				}` |
|        - |  5038 | `			}` |
|        2 |  5039 | `		}` |
|  2021530 |  5040 | `		PH7_MemObjLoad(pObj,pTos);` |
|  1010788 |  5041 | `	}else{` |
|        - |  5042 | `		/* Set a NULL value */` |
|      ! 0 |  5043 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  5044 | `	}` |
|  1010743 |  5045 | `LoadC_Done:` |
|        - |  5046 | `	/* Mark as constant */` |
|  2021532 |  5047 | `	pTos->nIdx = SXU32_HIGH;` |
|  2021532 |  5048 | `	break;` |
|        - |  5049 | `				  }` |
|        - |  5050 | `/*` |
|        - |  5051 | ` * LOAD: P1 * P3` |
|        - |  5052 | ` *` |
|        - |  5053 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  5054 | ` * from the P3 operand.` |
|        - |  5055 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  5056 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  5057 | ` */` |
|  1581135 |  5058 | `case PH7_OP_LOAD:{` |
|        - |  5059 | `	ph7_value *pObj;` |
|        - |  5060 | `	SyString sName;` |
|  3162492 |  5061 | `	if( pInstr->p3 == 0 ){` |
|        - |  5062 | `		/* Take the variable name from the top of the stack */` |
|        - |  5063 | `#ifdef UNTRUST` |
|        - |  5064 | `		if( pTos < pStack ){` |
|        - |  5065 | `			goto Abort;` |
|        - |  5066 | `		}` |
|        - |  5067 | `#endif` |
|        - |  5068 | `		/* Force a string cast */` |
|       19 |  5069 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  5070 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5071 | `		}` |
|       19 |  5072 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  5073 | `	}else{` |
|  3162474 |  5074 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5075 | `		/* Reserve a room for the target object */` |
|  3162474 |  5076 | `		pTos++;` |
|        - |  5077 | `	}` |
|        - |  5078 | `	/* Extract the requested memory object */` |
|  3162492 |  5079 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  3162492 |  5080 | `	if( pObj == 0 ){` |
|      858 |  5081 | `		if( pInstr->iP1 ){` |
|        - |  5082 | `			/* Variable not found,load NULL */` |
|      858 |  5083 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5084 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5085 | `			}else{` |
|      858 |  5086 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  5087 | `			}` |
|      858 |  5088 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1581565 |  5089 | `			break;` |
|      ! 0 |  5090 | `		}else{` |
|        - |  5091 | `			/* Fatal error */` |
|      ! 0 |  5092 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  5093 | `			goto Abort;` |
|        - |  5094 | `		}` |
|        - |  5095 | `	}` |
|        - |  5096 | `	/* Load variable contents */` |
|  3161636 |  5097 | `	PH7_MemObjLoad(pObj,pTos);` |
|  3161636 |  5098 | `	pTos->nIdx = pObj->nIdx;` |
|  3161636 |  5099 | `	break;` |
|        - |  5100 | `				   }` |
|        - |  5101 | `/*` |
|        - |  5102 | ` * LOAD_MAP P1 * *` |
|        - |  5103 | ` *` |
|        - |  5104 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  5105 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  5106 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  5107 | ` */` |
|    22917 |  5108 | `case PH7_OP_LOAD_MAP: {` |
|        - |  5109 | `	ph7_hashmap *pMap;` |
|        - |  5110 | `	/* Allocate a new hashmap instance */` |
|    45836 |  5111 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    45836 |  5112 | `	if( pMap == 0 ){` |
|      ! 0 |  5113 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  5114 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  5115 | `		goto Abort;` |
|        - |  5116 | `	}` |
|    45836 |  5117 | `	if( pInstr->iP1 > 0 ){` |
|     2796 |  5118 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|     2796 |  5119 | `		sxi32 rcSpread = SXRET_OK;` |
|        - |  5120 | `		/* Perform the insertion */` |
|     8534 |  5121 | `		while( pEntry < pTos ){` |
|     5756 |  5122 | `			if( pEntry[1].iFlags & MEMOBJ_AUX_SPREAD ){` |
|        - |  5123 | `				/* Array unpacking: '...$expr'. Merge entries with PHP 8.1` |
|        - |  5124 | `				 * semantics — string keys preserved (later wins), int keys` |
|        - |  5125 | `				 * renumbered. Same routine that backs array_merge. */` |
|       70 |  5126 | `				if( pEntry[1].iFlags & MEMOBJ_HASHMAP ){` |
|       53 |  5127 | `					sxi32 rcMerge = PH7_HashmapMerge((ph7_hashmap *)pEntry[1].x.pOther,pMap);` |
|       53 |  5128 | `					if( rcMerge != SXRET_OK ){` |
|        - |  5129 | `						/* Merge failure (OOM): match the PH7_NewHashmap OOM` |
|        - |  5130 | `						 * path — emit fatal and abort, leaving no partial` |
|        - |  5131 | `						 * map dangling. */` |
|      ! 0 |  5132 | `						VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  5133 | `							"Fatal, PH7 engine is running out of memory while spreading array at instruction #:%d",pc);` |
|      ! 0 |  5134 | `						rcSpread = PH7_ABORT;` |
|      ! 0 |  5135 | `						break;` |
|        - |  5136 | `					}` |
|       27 |  5137 | `				}else{` |
|        - |  5138 | `					/* Throw a catchable Error matching PHP semantics. */` |
|       17 |  5139 | `					rcSpread = VmThrowSpreadError(&(*pVm),&pEntry[1]);` |
|       17 |  5140 | `					break;` |
|        1 |  5141 | `				}` |
|     5714 |  5142 | `			}else if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  5143 | `				/* Insertion by reference */` |
|      151 |  5144 | `				PH7_HashmapInsertByRef(pMap,` |
|      100 |  5145 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|      100 |  5146 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  5147 | `					);` |
|       51 |  5148 | `			}else{` |
|        - |  5149 | `				/* Standard insertion */` |
|     8381 |  5150 | `				PH7_HashmapInsert(pMap,` |
|     5586 |  5151 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2793 |  5152 | `					&pEntry[1]` |
|        - |  5153 | `				);` |
|        - |  5154 | `			}` |
|        - |  5155 | `			/* Next pair on the stack */` |
|     5740 |  5156 | `			pEntry += 2;` |
|        2 |  5157 | `		}` |
|        - |  5158 | `		/* Pop P1 elements */` |
|     2796 |  5159 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     2796 |  5160 | `		if( rcSpread != SXRET_OK ){` |
|        - |  5161 | `			/* Discard the partially-built map and propagate the exception. */` |
|       17 |  5162 | `			PH7_HashmapRelease(pMap,TRUE);` |
|       17 |  5163 | `			if( rcSpread == PH7_ABORT ){` |
|      ! 0 |  5164 | `				goto Abort;` |
|        - |  5165 | `			}` |
|        - |  5166 | `			{` |
|       17 |  5167 | `				VmFrame *pFrm2 = pVm->pFrame;` |
|       17 |  5168 | `				if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  5169 | `					pc = pFrm2->iExceptionJump - 1;` |
|        3 |  5170 | `					break;` |
|        - |  5171 | `				}` |
|        - |  5172 | `			}` |
|       15 |  5173 | `			goto Exception;` |
|        - |  5174 | `		}` |
|     1389 |  5175 | `	}` |
|        - |  5176 | `	/* Push the hashmap */` |
|    45820 |  5177 | `	pTos++;` |
|    45820 |  5178 | `	pTos->nIdx = SXU32_HIGH;` |
|    45820 |  5179 | `	pTos->x.pOther = pMap;` |
|    45820 |  5180 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    45820 |  5181 | `	break;` |
|        - |  5182 | `					  }` |
|        - |  5183 | `/*` |
|        - |  5184 | ` * LOAD_LIST: P1 * *` |
|        - |  5185 | ` *` |
|        - |  5186 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  5187 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  5188 | ` * Caveats:` |
|        - |  5189 | ` *  This implementation support only a single nesting level.` |
|        - |  5190 | ` */` |
|       48 |  5191 | `case PH7_OP_LOAD_LIST: {` |
|        - |  5192 | `	ph7_value *pEntry;` |
|       98 |  5193 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  5194 | `		/* Empty list,break immediately */` |
|      ! 0 |  5195 | `		break;` |
|        - |  5196 | `	}` |
|       98 |  5197 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  5198 | `#ifdef UNTRUST` |
|        - |  5199 | `	if( &pEntry[-1] < pStack ){` |
|        - |  5200 | `		goto Abort;` |
|        - |  5201 | `	}` |
|        - |  5202 | `#endif` |
|       98 |  5203 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       91 |  5204 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  5205 | `		ph7_hashmap_node *pNode;` |
|        - |  5206 | `		ph7_value sKey,*pObj;` |
|        - |  5207 | `		/* Start Copying */` |
|       91 |  5208 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      283 |  5209 | `		while( pEntry <= pTos ){` |
|      193 |  5210 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|      165 |  5211 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|      165 |  5212 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|      165 |  5213 | `					if( rc == SXRET_OK ){` |
|        - |  5214 | `						/* Store node value */` |
|      165 |  5215 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       83 |  5216 | `					}else{` |
|        - |  5217 | `						/* Undefined array key */` |
|        - |  5218 | `						char zMsg[128];` |
|      ! 0 |  5219 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|      ! 0 |  5220 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  5221 | `						PH7_MemObjRelease(pObj);` |
|        - |  5222 | `					}` |
|       82 |  5223 | `				}` |
|       82 |  5224 | `			}` |
|      193 |  5225 | `			sKey.x.iVal++; /* Next numeric index */` |
|      193 |  5226 | `			pEntry++;` |
|        1 |  5227 | `		}` |
|       46 |  5228 | `	}else{` |
|        - |  5229 | `		/* Source is not an array */` |
|        - |  5230 | `		ph7_value *pObj;` |
|       18 |  5231 | `		while( pEntry <= pTos ){` |
|       12 |  5232 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|       12 |  5233 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       12 |  5234 | `					PH7_MemObjRelease(pObj);` |
|        5 |  5235 | `				}` |
|        5 |  5236 | `			}` |
|       12 |  5237 | `			pEntry++;` |
|        2 |  5238 | `		}` |
|        8 |  5239 | `		if( (pTos[-pInstr->iP1].iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - |  5240 | `			/* Emit PHP-compatible warning with type name */` |
|        3 |  5241 | `			const char *zType = "unknown";` |
|        3 |  5242 | `			sxi32 iFlags = pTos[-pInstr->iP1].iFlags;` |
|        - |  5243 | `			char zMsg[256];` |
|        3 |  5244 | `			if( iFlags & MEMOBJ_STRING ){` |
|        3 |  5245 | `				zType = "string";` |
|        1 |  5246 | `			}else if( iFlags & MEMOBJ_INT ){` |
|      ! 0 |  5247 | `				zType = "int";` |
|      ! 0 |  5248 | `			}else if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5249 | `				zType = "float";` |
|      ! 0 |  5250 | `			}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  5251 | `				zType = "object";` |
|      ! 0 |  5252 | `			}else if( iFlags & MEMOBJ_RES ){` |
|      ! 0 |  5253 | `				zType = "resource";` |
|      ! 0 |  5254 | `			}` |
|        3 |  5255 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|        3 |  5256 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|        1 |  5257 | `		}` |
|        - |  5258 | `	}` |
|       98 |  5259 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       98 |  5260 | `	break;` |
|        - |  5261 | `					   }` |
|        - |  5262 | `/*` |
|        - |  5263 | ` * LOAD_IDX: P1 P2 *` |
|        - |  5264 | ` *` |
|        - |  5265 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  5266 | ` * from the stack.` |
|        - |  5267 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  5268 | ` * instead.` |
|        - |  5269 | ` */` |
|   251252 |  5270 | `case PH7_OP_LOAD_IDX: {` |
|   502550 |  5271 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   502550 |  5272 | `	ph7_hashmap *pMap = 0;` |
|        - |  5273 | `	ph7_value *pIdx;` |
|   502550 |  5274 | `	pIdx = 0;` |
|   502550 |  5275 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  5276 | `		if( !pInstr->iP2){` |
|        - |  5277 | `			/* No available index,load NULL */` |
|      ! 0 |  5278 | `			if( pTos >= pStack ){` |
|      ! 0 |  5279 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5280 | `			}else{` |
|        - |  5281 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  5282 | `				pTos++;` |
|      ! 0 |  5283 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  5284 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  5285 | `			}` |
|        - |  5286 | `			/* Emit a notice */` |
|      ! 0 |  5287 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  5288 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  5289 | `			break;` |
|        - |  5290 | `		}` |
|      ! 0 |  5291 | `	}else{` |
|   502550 |  5292 | `		pIdx = pTos;` |
|   502550 |  5293 | `		pTos--;` |
|        - |  5294 | `	}` |
|   502550 |  5295 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  5296 | `		/* String access */` |
|   388236 |  5297 | `		if( pIdx ){` |
|        - |  5298 | `			sxu32 nOfft;` |
|   388236 |  5299 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  5300 | `				/* Force an int cast */` |
|      ! 0 |  5301 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  5302 | `			}` |
|   388236 |  5303 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   388236 |  5304 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  5305 | `				/* Invalid offset,load null */` |
|      ! 0 |  5306 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5307 | `			}else{` |
|   388236 |  5308 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   388236 |  5309 | `				int c = zData[nOfft];` |
|   388236 |  5310 | `				PH7_MemObjRelease(pTos);` |
|   388236 |  5311 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   388236 |  5312 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  5313 | `			}` |
|   194141 |  5314 | `		}else{` |
|        - |  5315 | `			/* No available index,load NULL */` |
|      ! 0 |  5316 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  5317 | `		}` |
|   388236 |  5318 | `		break;` |
|        - |  5319 | `	}` |
|   114316 |  5320 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5321 | `		/* Object subscript: ArrayAccess dispatch.` |
|        - |  5322 | `		 * iP2 codes:` |
|        - |  5323 | `		 *   0 = read       → offsetGet` |
|        - |  5324 | `		 *   3 = ?? peek    → offsetExists; offsetGet on hit; arm coalesce` |
|        - |  5325 | `		 *                    target on miss for the upcoming NULLC_STORE` |
|        - |  5326 | `		 *   4 = isset()    → offsetExists` |
|        - |  5327 | `		 *   5 = unset()    → offsetUnset` |
|        - |  5328 | `		 *   6 = empty()    → offsetExists, then offsetGet on hit */` |
|      126 |  5329 | `		ph7_class_instance *pInst = (ph7_class_instance *)pTos->x.pOther;` |
|      126 |  5330 | `		ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|      126 |  5331 | `		if( pArrayAccess && pInst && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|        - |  5332 | `			ph7_class_method *pMeth;` |
|        - |  5333 | `			ph7_value sResult;` |
|        - |  5334 | `			ph7_value *apArg[1];` |
|      124 |  5335 | `			if( (pInstr->iP2 == 0 \|\| pInstr->iP2 == 3) && pIdx == 0 ){` |
|        - |  5336 | ``				/* `$obj[]` read — PHP rejects this. */`` |
|      ! 0 |  5337 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  5338 | `					"Cannot use [] for reading");` |
|      ! 0 |  5339 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5340 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5341 | `				break;` |
|        - |  5342 | `			}` |
|      124 |  5343 | `			PH7_MemObjInit(&(*pVm),&sResult);` |
|      124 |  5344 | `			if( pInstr->iP2 == 4 \|\| pInstr->iP2 == 6 \|\| pInstr->iP2 == 3 ){` |
|        - |  5345 | `				/* isset, empty, and ??= all start with offsetExists. */` |
|       51 |  5346 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5347 | `					"offsetExists",sizeof("offsetExists")-1);` |
|       51 |  5348 | `				apArg[0] = pIdx;` |
|       51 |  5349 | `				if( pMeth ){` |
|       51 |  5350 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|       26 |  5351 | `				}` |
|       99 |  5352 | `			}else if( pInstr->iP2 == 5 ){` |
|        9 |  5353 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5354 | `					"offsetUnset",sizeof("offsetUnset")-1);` |
|        9 |  5355 | `				apArg[0] = pIdx;` |
|        9 |  5356 | `				if( pMeth ){` |
|        9 |  5357 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|        4 |  5358 | `				}` |
|        5 |  5359 | `			}else{` |
|       66 |  5360 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5361 | `					"offsetGet",sizeof("offsetGet")-1);` |
|       66 |  5362 | `				apArg[0] = pIdx;` |
|       66 |  5363 | `				if( pMeth ){` |
|       66 |  5364 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|       32 |  5365 | `				}` |
|        - |  5366 | `			}` |
|      124 |  5367 | `			if( pInstr->iP2 == 4 ){` |
|        - |  5368 | `				/* isset: push MEMOBJ_BOOL so vm_builtin_isset reports the` |
|        - |  5369 | `				 * right truth value AND skips its "Expecting a variable not` |
|        - |  5370 | `				 * a constant" warning (keyed on MEMOBJ_BOOL). */` |
|       33 |  5371 | `				int bExists = ph7_value_to_bool(&sResult);` |
|       33 |  5372 | `				PH7_MemObjRelease(pTos);` |
|       33 |  5373 | `				pTos->nIdx = SXU32_HIGH;` |
|       33 |  5374 | `				if( bExists ){` |
|       17 |  5375 | `					MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       17 |  5376 | `					pTos->x.iVal = 1;` |
|        9 |  5377 | `				}else{` |
|       17 |  5378 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        1 |  5379 | `				}` |
|      108 |  5380 | `			}else if( pInstr->iP2 == 5 ){` |
|        - |  5381 | `				/* offsetUnset return is discarded; push NULL so the trailing` |
|        - |  5382 | `				 * vm_builtin_unset is a harmless no-op. */` |
|        9 |  5383 | `				PH7_MemObjRelease(pTos);` |
|        9 |  5384 | `				pTos->nIdx = SXU32_HIGH;` |
|        9 |  5385 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|       88 |  5386 | `			}else if( pInstr->iP2 == 6 ){` |
|        - |  5387 | `				/* empty: if offsetExists is false, push NULL so empty=true` |
|        - |  5388 | `				 * without calling offsetGet. If true, call offsetGet and` |
|        - |  5389 | `				 * push the value so PH7_builtin_empty evaluates emptiness. */` |
|       11 |  5390 | `				int bExists = ph7_value_to_bool(&sResult);` |
|       11 |  5391 | `				PH7_MemObjRelease(&sResult);` |
|       11 |  5392 | `				PH7_MemObjRelease(pTos);` |
|       11 |  5393 | `				pTos->nIdx = SXU32_HIGH;` |
|       11 |  5394 | `				if( !bExists ){` |
|        3 |  5395 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        2 |  5396 | `				}else{` |
|        9 |  5397 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5398 | `						"offsetGet",sizeof("offsetGet")-1);` |
|        - |  5399 | `					ph7_value sValue;` |
|        9 |  5400 | `					PH7_MemObjInit(&(*pVm),&sValue);` |
|        9 |  5401 | `					apArg[0] = pIdx;` |
|        9 |  5402 | `					if( pGet ){` |
|        9 |  5403 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|        4 |  5404 | `					}` |
|        9 |  5405 | `					PH7_MemObjStore(&sValue,pTos);` |
|        9 |  5406 | `					PH7_MemObjRelease(&sValue);` |
|        - |  5407 | `				}` |
|       11 |  5408 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|       11 |  5409 | `				break; /* skip the duplicate sResult release below */` |
|       74 |  5410 | `			}else if( pInstr->iP2 == 3 ){` |
|        - |  5411 | `				/* ?? null-coalesce peek: emulate PHP semantics —` |
|        - |  5412 | `				 *   if !offsetExists OR offsetGet() === null → arm` |
|        - |  5413 | `				 *     coalesce slot (NULLC_STORE will call offsetSet)` |
|        - |  5414 | `				 *     and push NULL.` |
|        - |  5415 | `				 *   else → push offsetGet's value (NULLC_JMP skips). */` |
|        9 |  5416 | `				int bExists = ph7_value_to_bool(&sResult);` |
|        9 |  5417 | `				int bShouldArm = !bExists;` |
|        - |  5418 | `				ph7_value sValue;` |
|        9 |  5419 | `				PH7_MemObjRelease(&sResult);` |
|        - |  5420 | `				/* Reset any prior arming defensively */` |
|        9 |  5421 | `				VmCoalesceDisarm(pVm);` |
|        9 |  5422 | `				PH7_MemObjInit(&(*pVm),&sValue);` |
|        9 |  5423 | `				if( bExists ){` |
|        5 |  5424 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5425 | `						"offsetGet",sizeof("offsetGet")-1);` |
|        5 |  5426 | `					apArg[0] = pIdx;` |
|        5 |  5427 | `					if( pGet ){` |
|        5 |  5428 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|        2 |  5429 | `					}` |
|        5 |  5430 | `					if( sValue.iFlags & MEMOBJ_NULL ){` |
|        3 |  5431 | `						bShouldArm = 1;` |
|        1 |  5432 | `					}` |
|        2 |  5433 | `				}` |
|        9 |  5434 | `				PH7_MemObjRelease(pTos);` |
|        9 |  5435 | `				pTos->nIdx = SXU32_HIGH;` |
|        9 |  5436 | `				if( bShouldArm ){` |
|        - |  5437 | `					/* Arm: remember (object, key) so NULLC_STORE dispatches` |
|        - |  5438 | `					 * to offsetSet. Hold a ref on the instance to survive` |
|        - |  5439 | `					 * intervening expression evaluation. */` |
|        7 |  5440 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        7 |  5441 | `					if( pIdx ){` |
|        7 |  5442 | `						PH7_MemObjStore(pIdx,&pVm->sCoalesceKey);` |
|        3 |  5443 | `					}` |
|        7 |  5444 | `					pVm->pCoalesceObj = pInst;` |
|        7 |  5445 | `					pInst->iRef++;` |
|        7 |  5446 | `					pVm->bCoalesceArmed = 1;` |
|        4 |  5447 | `				}else{` |
|        3 |  5448 | `					PH7_MemObjStore(&sValue,pTos);` |
|        - |  5449 | `				}` |
|        9 |  5450 | `				PH7_MemObjRelease(&sValue);` |
|        9 |  5451 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|        9 |  5452 | `				break;` |
|      ! 0 |  5453 | `			}else{` |
|        - |  5454 | `				/* offsetGet: replace pTos with the returned value. */` |
|       66 |  5455 | `				PH7_MemObjRelease(pTos);` |
|       66 |  5456 | `				PH7_MemObjStore(&sResult,pTos);` |
|       66 |  5457 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  5458 | `			}` |
|      106 |  5459 | `			PH7_MemObjRelease(&sResult);` |
|      106 |  5460 | `			if( pIdx ){` |
|      106 |  5461 | `				PH7_MemObjRelease(pIdx);` |
|       52 |  5462 | `			}` |
|      106 |  5463 | `			break;` |
|        - |  5464 | `		}` |
|        - |  5465 | `		/* Object without ArrayAccess: PHP throws fatal Error in all subscript` |
|        - |  5466 | `		 * contexts (read, isset, unset, empty). Match it. */` |
|        3 |  5467 | `		if( pInst ){` |
|        - |  5468 | `			char zMsg[256];` |
|        3 |  5469 | `			SyString *pName = &pInst->pClass->sName;` |
|        4 |  5470 | `			sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - |  5471 | `				"Cannot use object of type %.*s as array",` |
|        2 |  5472 | `				(int)pName->nByte,pName->zString);` |
|        3 |  5473 | `			rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|        3 |  5474 | `			if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|        3 |  5475 | `			PH7_MemObjRelease(pTos);` |
|        3 |  5476 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  5477 | `			if( rc == SXERR_ABORT ){ goto Abort; }` |
|        3 |  5478 | `			break;` |
|        - |  5479 | `		}` |
|      ! 0 |  5480 | `	}` |
|   114192 |  5481 | `	if( (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  5482 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  5483 | `			ph7_value *pObj;` |
|        3 |  5484 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  5485 | `				PH7_MemObjToHashmap(pObj);` |
|        3 |  5486 | `				PH7_MemObjLoad(pObj,pTos);` |
|        1 |  5487 | `			}` |
|        1 |  5488 | `		}` |
|        1 |  5489 | `	}` |
|   114192 |  5490 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|   114192 |  5491 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|   114192 |  5492 | `		if( pInstr->iP2 == 1 \|\| pInstr->iP2 == 5 ){` |
|        - |  5493 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  5494 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  5495 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  5496 | `			 * NOT separate — that would defeat COW on every element read.` |
|        - |  5497 | `			 * iP2=5 is unset-context, treated like iP2=1 for arrays so the` |
|        - |  5498 | `			 * trailing unset() builtin can drop the slot via pTos->nIdx. */` |
|      896 |  5499 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      447 |  5500 | `		}` |
|        - |  5501 | `		/* Point to the hashmap */` |
|   114192 |  5502 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   114192 |  5503 | `		if( pIdx ){` |
|        - |  5504 | `			/* Load the desired entry */` |
|   114192 |  5505 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    57095 |  5506 | `		}` |
|   114192 |  5507 | `		if( pInstr->iP2 == 3 ){` |
|        - |  5508 | `			/* Null coalescing assign peek mode: separate only when we will` |
|        - |  5509 | `			 * actually write back. If the looked-up value is non-null, the` |
|        - |  5510 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|        - |  5511 | `			 * the parent can stay shared. If the value is null or the key is` |
|        - |  5512 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|        - |  5513 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|        - |  5514 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|        - |  5515 | `			 * correct for the outermost write. */` |
|       19 |  5516 | `			int needWrite = (rc != SXRET_OK);` |
|       19 |  5517 | `			if( !needWrite && pNode ){` |
|       13 |  5518 | `				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|       13 |  5519 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|        7 |  5520 | `					needWrite = 1;` |
|        3 |  5521 | `				}` |
|        6 |  5522 | `			}` |
|       19 |  5523 | `			if( needWrite ){` |
|       13 |  5524 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|       13 |  5525 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|        - |  5526 | `					/* The map was actually copied — re-lookup so pNode points` |
|        - |  5527 | `					 * into the new map's storage. */` |
|        7 |  5528 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        7 |  5529 | `					if( pIdx ){` |
|        7 |  5530 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|        3 |  5531 | `					}` |
|        3 |  5532 | `				}` |
|        6 |  5533 | `			}` |
|        9 |  5534 | `		}` |
|   114192 |  5535 | `		if( rc != SXRET_OK && (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) ){` |
|        - |  5536 | `			/* Create a new empty entry */` |
|      273 |  5537 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      273 |  5538 | `			if( rc == SXRET_OK ){` |
|        - |  5539 | `				/* Point to the last inserted entry */` |
|      273 |  5540 | `				pNode = pMap->pLast;` |
|      136 |  5541 | `			}` |
|      136 |  5542 | `		}` |
|    57095 |  5543 | `	}` |
|   114192 |  5544 | `	if( rc != SXRET_OK && pInstr->iP2 == 2 && pIdx ){` |
|        - |  5545 | `		/* List destructuring context: emit PHP-compatible warning for missing key */` |
|        - |  5546 | `		char zMsg[128];` |
|      ! 0 |  5547 | `		if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5548 | `			PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  5549 | `		}` |
|      ! 0 |  5550 | `		SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)pIdx->x.iVal);` |
|      ! 0 |  5551 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  5552 | `	}` |
|   114192 |  5553 | `	if( pIdx ){` |
|   114192 |  5554 | `		PH7_MemObjRelease(pIdx);` |
|    57095 |  5555 | `	}` |
|   114192 |  5556 | `	if( rc == SXRET_OK ){` |
|        - |  5557 | `		/* Load entry contents */` |
|    50606 |  5558 | `		if( pMap->iRef < 2 ){` |
|        - |  5559 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  5560 | `			 * of the entry value,rather than pointing to it.` |
|        - |  5561 | `			 */` |
|       28 |  5562 | `			pTos->nIdx = SXU32_HIGH;` |
|       28 |  5563 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       15 |  5564 | `		}else{` |
|    50580 |  5565 | `			pTos->nIdx = pNode->nValIdx;` |
|    50580 |  5566 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    50580 |  5567 | `			PH7_HashmapUnref(pMap);` |
|        - |  5568 | `		}` |
|    25304 |  5569 | `	}else{` |
|        - |  5570 | `		/* No such entry,load NULL */` |
|    63588 |  5571 | `		PH7_MemObjRelease(pTos);` |
|    63588 |  5572 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  5573 | `	}` |
|   114192 |  5574 | `	break;` |
|        - |  5575 | `					  }` |
|        - |  5576 | `/*` |
|        - |  5577 | ` * LOAD_CLOSURE * * P3` |
|        - |  5578 | ` *` |
|        - |  5579 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  5580 | ` * name in the stack.` |
|        - |  5581 | ` */` |
|       64 |  5582 | `case PH7_OP_LOAD_CLOSURE:{` |
|      130 |  5583 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|      130 |  5584 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  5585 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  5586 | `		ph7_vm_func *pClosure;` |
|        - |  5587 | `		char *zName;` |
|        - |  5588 | `		sxu32 mLen;` |
|        - |  5589 | `		sxu32 n;` |
|        - |  5590 | `		/* Create a new VM function */` |
|      130 |  5591 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  5592 | `		/* Generate an unique closure name */` |
|      130 |  5593 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|      130 |  5594 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  5595 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  5596 | `			goto Abort;` |
|        - |  5597 | `		}` |
|      130 |  5598 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      130 |  5599 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  5600 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  5601 | `		}` |
|        - |  5602 | `		/* Zero the stucture */` |
|      130 |  5603 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  5604 | `		/* Perform a structure assignment on read-only items */` |
|      130 |  5605 | `		pClosure->aArgs = pFunc->aArgs;` |
|      130 |  5606 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|      130 |  5607 | `		pClosure->aStatic = pFunc->aStatic;` |
|      130 |  5608 | `		pClosure->iFlags = pFunc->iFlags;` |
|      130 |  5609 | `		pClosure->pUserData = pFunc->pUserData;` |
|      130 |  5610 | `		pClosure->sSignature = pFunc->sSignature;` |
|      130 |  5611 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|      130 |  5612 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|      130 |  5613 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|      130 |  5614 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|      130 |  5615 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  5616 | `		/* Register the closure */` |
|      130 |  5617 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  5618 | `		/* Set up closure environment */` |
|      130 |  5619 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|      130 |  5620 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      324 |  5621 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  5622 | `			ph7_value *pValue;` |
|      196 |  5623 | `			pEnv = &aEnv[n];` |
|      196 |  5624 | `			sEnv.sName  = pEnv->sName;` |
|      196 |  5625 | `			sEnv.iFlags = pEnv->iFlags;` |
|      196 |  5626 | `			sEnv.nIdx = SXU32_HIGH;` |
|      196 |  5627 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|      196 |  5628 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  5629 | `				/* Pass by reference */` |
|      ! 0 |  5630 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  5631 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  5632 | `					);` |
|      ! 0 |  5633 | `			}` |
|        - |  5634 | `			/* Standard pass by value */` |
|      196 |  5635 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|      196 |  5636 | `			if( pValue ){` |
|        - |  5637 | `				/* Copy imported value */` |
|       72 |  5638 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|       35 |  5639 | `			}` |
|        - |  5640 | `			/* Insert the imported variable */` |
|      196 |  5641 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|       99 |  5642 | `		}` |
|        - |  5643 | `		/* Finally,load the closure name on the stack */` |
|      130 |  5644 | `		pTos++;` |
|      130 |  5645 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|       64 |  5646 | `	}` |
|      130 |  5647 | `	break;` |
|        - |  5648 | `						 }` |
|        - |  5649 | `/*` |
|        - |  5650 | ` * STORE * P2 P3` |
|        - |  5651 | ` *` |
|        - |  5652 | ` * Perform a store (Assignment) operation.` |
|        - |  5653 | ` */` |
|   146891 |  5654 | `case PH7_OP_STORE: {` |
|        - |  5655 | `	ph7_value *pObj;` |
|        - |  5656 | `	SyString sName;` |
|        - |  5657 | `#ifdef UNTRUST` |
|        - |  5658 | `	if( pTos < pStack ){` |
|        - |  5659 | `		goto Abort;` |
|        - |  5660 | `	}` |
|        - |  5661 | `#endif` |
|   293784 |  5662 | `	if( pInstr->iP2 ){` |
|        - |  5663 | `		sxu32 nIdx;` |
|        - |  5664 | `		sxi32 rcT;` |
|        - |  5665 | `		/* Member store operation */` |
|     5364 |  5666 | `		nIdx = pTos->nIdx;` |
|     5364 |  5667 | `		VmPopOperand(&pTos,1);` |
|     5364 |  5668 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  5669 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5670 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  5671 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  5672 | `		}else{` |
|        - |  5673 | `			/* Enforce typed property declaration if any. May coerce the` |
|        - |  5674 | `			 * incoming value in place (weak mode) or throw TypeError. */` |
|     5360 |  5675 | `			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos);` |
|     5360 |  5676 | `			if( rcT == PH7_ABORT ){` |
|      ! 0 |  5677 | `				goto Abort;` |
|        - |  5678 | `			}` |
|     5360 |  5679 | `			if( rcT == PH7_EXCEPTION ){` |
|        - |  5680 | `				/* TypeError was thrown. Pop the rejected rvalue and hand` |
|        - |  5681 | `				 * control to the nearest catch block if any, otherwise` |
|        - |  5682 | `				 * propagate out of the VM loop. */` |
|       40 |  5683 | `				VmPopOperand(&pTos,1);` |
|        - |  5684 | `				{` |
|       40 |  5685 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       40 |  5686 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|       40 |  5687 | `						pc = pFrm2->iExceptionJump - 1;` |
|   146912 |  5688 | `						break;` |
|        - |  5689 | `					}` |
|        - |  5690 | `				}` |
|      ! 0 |  5691 | `				goto Exception;` |
|        - |  5692 | `			}` |
|        - |  5693 | `			/* Point to the desired memory object */` |
|     5322 |  5694 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     5322 |  5695 | `			if( pObj ){` |
|        - |  5696 | `				/* Perform the store operation */` |
|     5322 |  5697 | `				PH7_MemObjStore(pTos,pObj);` |
|     2660 |  5698 | `			}` |
|        - |  5699 | `		}` |
|     5326 |  5700 | `		break;` |
|   288422 |  5701 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  5702 | `		/* Take the variable name from the next on the stack */` |
|        7 |  5703 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5704 | `			/* Force a string cast */` |
|      ! 0 |  5705 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5706 | `		}` |
|        7 |  5707 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  5708 | `		pTos--;` |
|        - |  5709 | `#ifdef UNTRUST` |
|        - |  5710 | `		if( pTos < pStack  ){` |
|        - |  5711 | `			goto Abort;` |
|        - |  5712 | `		}` |
|        - |  5713 | `#endif` |
|        4 |  5714 | `	}else{` |
|   288416 |  5715 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5716 | `	}` |
|        - |  5717 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   288422 |  5718 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   288422 |  5719 | `	if( pObj == 0 ){` |
|      ! 0 |  5720 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5721 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  5722 | `		goto Abort;` |
|        - |  5723 | `	}` |
|   288422 |  5724 | `	if( !pInstr->p3 ){` |
|        7 |  5725 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  5726 | `	}` |
|        - |  5727 | `	/* Perform the store operation */` |
|   288422 |  5728 | `	PH7_MemObjStore(pTos,pObj);` |
|   288422 |  5729 | `	break;` |
|        - |  5730 | `				   }` |
|        - |  5731 | `/*` |
|        - |  5732 | ` * STORE_IDX:   P1 * P3` |
|        - |  5733 | ` * STORE_IDX_R: P1 * P3` |
|        - |  5734 | ` *` |
|        - |  5735 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  5736 | ` */` |
|    97474 |  5737 | `case PH7_OP_STORE_IDX:` |
|        - |  5738 | `case PH7_OP_STORE_IDX_REF: {` |
|   194950 |  5739 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  5740 | `	ph7_value *pKey;` |
|        - |  5741 | `	sxu32 nIdx;` |
|   194950 |  5742 | `	if( pInstr->iP1 ){` |
|        - |  5743 | `		/* Key is next on stack */` |
|    63484 |  5744 | `		pKey = pTos;` |
|    63484 |  5745 | `		pTos--;` |
|    31743 |  5746 | `	}else{` |
|   131468 |  5747 | `		pKey = 0;` |
|        - |  5748 | `	}` |
|   194950 |  5749 | `	nIdx = pTos->nIdx;` |
|        - |  5750 | `	{` |
|        - |  5751 | `		/* ArrayAccess::offsetSet dispatch.` |
|        - |  5752 | `		 * Container may be on the stack as MEMOBJ_OBJ, or referenced via` |
|        - |  5753 | `		 * the backing variable slot at nIdx. */` |
|   194950 |  5754 | `		ph7_class_instance *pInst = 0;` |
|   194950 |  5755 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       34 |  5756 | `			pInst = (ph7_class_instance *)pTos->x.pOther;` |
|   194934 |  5757 | `		}else if( nIdx != SXU32_HIGH ){` |
|   194918 |  5758 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   194918 |  5759 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_OBJ) ){` |
|      ! 0 |  5760 | `				pInst = (ph7_class_instance *)pBacking->x.pOther;` |
|      ! 0 |  5761 | `			}` |
|    97458 |  5762 | `		}` |
|   194950 |  5763 | `		if( pInst ){` |
|       34 |  5764 | `			ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|       34 |  5765 | `			if( pArrayAccess && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|        - |  5766 | `				ph7_class_method *pMeth;` |
|        - |  5767 | `				ph7_value sNullKey;` |
|        - |  5768 | `				ph7_value *apArg[2];` |
|       32 |  5769 | `				if( pInstr->iOp == PH7_OP_STORE_IDX_REF ){` |
|      ! 0 |  5770 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5771 | `						"Cannot assign by reference to overloaded object");` |
|      ! 0 |  5772 | `					if( pKey ){ PH7_MemObjRelease(pKey); }` |
|      ! 0 |  5773 | `					VmPopOperand(&pTos,2); /* container + value */` |
|      ! 0 |  5774 | `					break;` |
|        - |  5775 | `				}` |
|       32 |  5776 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5777 | `					"offsetSet",sizeof("offsetSet")-1);` |
|        - |  5778 | `				/* Pop container; pTos now points to the value */` |
|       32 |  5779 | `				VmPopOperand(&pTos,1);` |
|       32 |  5780 | `				if( pKey == 0 ){` |
|        7 |  5781 | `					PH7_MemObjInit(&(*pVm),&sNullKey);` |
|        7 |  5782 | `					apArg[0] = &sNullKey;` |
|        4 |  5783 | `				}else{` |
|       26 |  5784 | `					apArg[0] = pKey;` |
|        - |  5785 | `				}` |
|       32 |  5786 | `				apArg[1] = pTos;` |
|       32 |  5787 | `				if( pMeth ){` |
|       32 |  5788 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,0,2,apArg);` |
|       15 |  5789 | `				}` |
|       32 |  5790 | `				if( pKey ){` |
|       26 |  5791 | `					PH7_MemObjRelease(pKey);` |
|       14 |  5792 | `				}else{` |
|        7 |  5793 | `					PH7_MemObjRelease(&sNullKey);` |
|        - |  5794 | `				}` |
|        - |  5795 | `				/* Pop the value */` |
|       32 |  5796 | `				VmPopOperand(&pTos,1);` |
|       32 |  5797 | `				break;` |
|        - |  5798 | `			}` |
|        - |  5799 | `			/* Object without ArrayAccess: PHP throws a fatal Error rather` |
|        - |  5800 | `			 * than silently coercing the object into a hashmap (which is` |
|        - |  5801 | `			 * what the legacy PH7 fall-through would do via MemObjToHashmap` |
|        - |  5802 | `			 * a few lines below). Match PHP. */` |
|        - |  5803 | `			{` |
|        - |  5804 | `				char zMsg[256];` |
|        3 |  5805 | `				SyString *pName = &pInst->pClass->sName;` |
|        4 |  5806 | `				sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - |  5807 | `					"Cannot use object of type %.*s as array",` |
|        2 |  5808 | `					(int)pName->nByte,pName->zString);` |
|        3 |  5809 | `				rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|        3 |  5810 | `				if( pKey ){ PH7_MemObjRelease(pKey); }` |
|        3 |  5811 | `				VmPopOperand(&pTos,2); /* container + value */` |
|        3 |  5812 | `				if( rc == SXERR_ABORT ){ goto Abort; }` |
|        3 |  5813 | `				break;` |
|        - |  5814 | `			}` |
|        - |  5815 | `		}` |
|        - |  5816 | `	}` |
|   194918 |  5817 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5818 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  5819 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  5820 | `		 * checking true sharing count, then re-add after separation. */` |
|   194866 |  5821 | `		if( nIdx != SXU32_HIGH ){` |
|   194866 |  5822 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   292298 |  5823 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   194866 |  5824 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5825 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  5826 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  5827 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  5828 | `				 * refcounts if the backing array was already separated. */` |
|   194866 |  5829 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   194866 |  5830 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   194866 |  5831 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   194866 |  5832 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   194866 |  5833 | `					pTos->x.pOther = pMap;` |
|    97434 |  5834 | `				}else{` |
|        - |  5835 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  5836 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  5837 | `					pMap = pCur;` |
|        - |  5838 | `				}` |
|    97434 |  5839 | `			}else{` |
|      ! 0 |  5840 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5841 | `			}` |
|    97434 |  5842 | `		}else{` |
|      ! 0 |  5843 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5844 | `		}` |
|   194866 |  5845 | `		if( pMap->iRef < 2 ){` |
|        - |  5846 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  5847 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  5848 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  5849 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  5850 | `			pMap->iRef = 2;` |
|      ! 0 |  5851 | `		}` |
|    97434 |  5852 | `	}else{` |
|        - |  5853 | `		ph7_value *pObj;` |
|       53 |  5854 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  5855 | `		if( pObj == 0 ){` |
|      ! 0 |  5856 | `			if( pKey ){` |
|      ! 0 |  5857 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  5858 | `			}` |
|      ! 0 |  5859 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5860 | `			break;` |
|        - |  5861 | `		}` |
|        - |  5862 | `		/* Phase#1: Load the array */` |
|       53 |  5863 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  5864 | `			VmPopOperand(&pTos,1);` |
|       53 |  5865 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  5866 | `				/* Force a string cast */` |
|      ! 0 |  5867 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  5868 | `			}` |
|       53 |  5869 | `			if( pKey == 0 ){` |
|        - |  5870 | `				/* Append string */` |
|        3 |  5871 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  5872 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  5873 | `				}` |
|        2 |  5874 | `			}else{` |
|        - |  5875 | `				sxu32 nOfft;` |
|       51 |  5876 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  5877 | `					/* Force an int cast */` |
|       51 |  5878 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  5879 | `				}` |
|       51 |  5880 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  5881 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  5882 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  5883 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  5884 | `					zData[nOfft] = zBlob[0];` |
|       26 |  5885 | `				}else{` |
|      ! 0 |  5886 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  5887 | `						/* Perform an append operation */` |
|      ! 0 |  5888 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  5889 | `					}` |
|        - |  5890 | `				}` |
|        - |  5891 | `			}` |
|       53 |  5892 | `			if( pKey ){` |
|       51 |  5893 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  5894 | `			}` |
|       53 |  5895 | `			break;` |
|      ! 0 |  5896 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  5897 | `			/* Force a hashmap cast  */` |
|      ! 0 |  5898 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  5899 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  5900 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  5901 | `				goto Abort;` |
|        - |  5902 | `			}` |
|      ! 0 |  5903 | `		}` |
|        - |  5904 | `		/* COW separate the backing variable before mutation */` |
|      ! 0 |  5905 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|        - |  5906 | `	}` |
|   194866 |  5907 | `	VmPopOperand(&pTos,1);` |
|        - |  5908 | `	/* Phase#2: Perform the insertion */` |
|   194866 |  5909 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  5910 | `		/* Insertion by reference */` |
|       15 |  5911 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  5912 | `	}else{` |
|   194852 |  5913 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  5914 | `	}` |
|   194866 |  5915 | `	if( pKey ){` |
|    63408 |  5916 | `		PH7_MemObjRelease(pKey);` |
|    31703 |  5917 | `	}` |
|   194866 |  5918 | `	break;` |
|        - |  5919 | `					   }` |
|        - |  5920 | `/*` |
|        - |  5921 | ` * INCR: P1 * *` |
|        - |  5922 | ` *` |
|        - |  5923 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  5924 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  5925 | ` * the stack and increment after that.` |
|        - |  5926 | ` */` |
|   167937 |  5927 | `case PH7_OP_INCR:` |
|        - |  5928 | `#ifdef UNTRUST` |
|        - |  5929 | `	if( pTos < pStack ){` |
|        - |  5930 | `		goto Abort;` |
|        - |  5931 | `	}` |
|        - |  5932 | `#endif` |
|   335920 |  5933 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   335920 |  5934 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  5935 | `			ph7_value *pObj;` |
|   335920 |  5936 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|   335920 |  5937 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - |  5938 | `					/* Perl-style string increment.` |
|        - |  5939 | `					 * Post-increment: pTos may alias pObj's buffer via SXBLOB_RDONLY` |
|        - |  5940 | `					 * (set by PH7_MemObjLoad).  Force ownership so the upcoming` |
|        - |  5941 | `					 * mutation of pObj doesn't bleed into pTos's old-value view. */` |
|       49 |  5942 | `					if( pInstr->iP1 == 0 ){` |
|       45 |  5943 | `						SyBlobNullAppend(&pTos->sBlob);` |
|       22 |  5944 | `					}` |
|       49 |  5945 | `					PH7_MemObjStringIncrement(pObj);` |
|       49 |  5946 | `					if( pInstr->iP1 ){` |
|        - |  5947 | `						/* Pre-increment: deep-copy pObj into pTos. */` |
|        5 |  5948 | `						PH7_MemObjStore(pObj,pTos);` |
|        2 |  5949 | `					}` |
|       25 |  5950 | `				}else{` |
|        - |  5951 | `					/* Numeric coercion. Post-increment must preserve pTos's` |
|        - |  5952 | `					 * original value: pTos may alias pObj's blob via` |
|        - |  5953 | `					 * SXBLOB_RDONLY (set by PH7_MemObjLoad), and` |
|        - |  5954 | `					 * PH7_MemObjToNumeric calls SyBlobRelease on a STRING` |
|        - |  5955 | `					 * pObj. Force pTos to take ownership of its blob first` |
|        - |  5956 | `					 * so its old-value view survives the coercion. */` |
|   335872 |  5957 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|       13 |  5958 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        6 |  5959 | `					}` |
|        - |  5960 | `					/* Force a numeric cast on the variable */` |
|   335872 |  5961 | `					PH7_MemObjToNumeric(pObj);` |
|   335872 |  5962 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        9 |  5963 | `						pObj->rVal++;` |
|        - |  5964 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - |  5965 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - |  5966 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - |  5967 | `						 * integer-valued real. */` |
|        9 |  5968 | `						PH7_MemObjTryInteger(pObj);` |
|        5 |  5969 | `					}else{` |
|   335864 |  5970 | `						pObj->x.iVal++;` |
|        - |  5971 | `					}` |
|   335872 |  5972 | `					if( pInstr->iP1 ){` |
|        - |  5973 | `						/* Pre-increment: result is the new value. */` |
|       83 |  5974 | `						PH7_MemObjStore(pObj,pTos);` |
|       41 |  5975 | `					}` |
|        - |  5976 | `					/* Post-increment: pTos retains the old value (a string` |
|        - |  5977 | `					 * for "5"++, an int/float for direct numeric operands). */` |
|        - |  5978 | `				}` |
|   167981 |  5979 | `			}` |
|   167983 |  5980 | `		}else{` |
|      ! 0 |  5981 | `			if( pInstr->iP1 ){` |
|      ! 0 |  5982 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|      ! 0 |  5983 | `					PH7_MemObjStringIncrement(pTos);` |
|      ! 0 |  5984 | `				}else{` |
|        - |  5985 | `					/* Force a numeric cast */` |
|      ! 0 |  5986 | `					PH7_MemObjToNumeric(pTos);` |
|        - |  5987 | `					/* Pre-increment */` |
|      ! 0 |  5988 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5989 | `						pTos->rVal++;` |
|        - |  5990 | `						/* Try to get an integer representation */` |
|      ! 0 |  5991 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  5992 | `					}else{` |
|      ! 0 |  5993 | `						pTos->x.iVal++;` |
|      ! 0 |  5994 | `						MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  5995 | `					}` |
|        - |  5996 | `				}` |
|      ! 0 |  5997 | `			}` |
|        - |  5998 | `		}` |
|   167981 |  5999 | `	}` |
|   335920 |  6000 | `	break;` |
|        - |  6001 | `/*` |
|        - |  6002 | ` * DECR: P1 * *` |
|        - |  6003 | ` *` |
|        - |  6004 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  6005 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  6006 | ` * and decrement after that.` |
|        - |  6007 | ` */` |
|       14 |  6008 | `case PH7_OP_DECR:` |
|        - |  6009 | `#ifdef UNTRUST` |
|        - |  6010 | `	if( pTos < pStack ){` |
|        - |  6011 | `		goto Abort;` |
|        - |  6012 | `	}` |
|        - |  6013 | `#endif` |
|        - |  6014 | ``	/* NULL stays excluded: PHP leaves `--` on null untouched (no-op). */`` |
|       29 |  6015 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|       27 |  6016 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  6017 | `			ph7_value *pObj;` |
|       27 |  6018 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       27 |  6019 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - |  6020 | ``					/* PHP has no string decrement: `--` on a non-numeric string`` |
|        - |  6021 | ``					 * is a no-op (unlike `++`, which is Perl-style). Leave pObj`` |
|        - |  6022 | `					 * unchanged; the result is simply that unchanged value. */` |
|        7 |  6023 | `					if( pInstr->iP1 ){` |
|        - |  6024 | `						/* Pre-decrement: result is the (unchanged) value. */` |
|      ! 0 |  6025 | `						PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  6026 | `					}` |
|        - |  6027 | `					/* Post-decrement: pTos already holds the old value. */` |
|        4 |  6028 | `				}else{` |
|        - |  6029 | `					/* Numeric coercion. Mirror INCR's aliasing care: a` |
|        - |  6030 | `					 * post-decrement must preserve pTos's original value, which` |
|        - |  6031 | `					 * may alias pObj's blob via SXBLOB_RDONLY (PH7_MemObjLoad).` |
|        - |  6032 | `					 * Force pTos to own its blob before coercing pObj. */` |
|       21 |  6033 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        5 |  6034 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        2 |  6035 | `					}` |
|       21 |  6036 | `					PH7_MemObjToNumeric(pObj);` |
|       21 |  6037 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        9 |  6038 | `						pObj->rVal--;` |
|        - |  6039 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - |  6040 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - |  6041 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - |  6042 | `						 * integer-valued real. */` |
|        9 |  6043 | `						PH7_MemObjTryInteger(pObj);` |
|        5 |  6044 | `					}else{` |
|       13 |  6045 | `						pObj->x.iVal--;` |
|        - |  6046 | `					}` |
|       21 |  6047 | `					if( pInstr->iP1 ){` |
|        - |  6048 | `						/* Pre-decrement: result is the new value. */` |
|        3 |  6049 | `						PH7_MemObjStore(pObj,pTos);` |
|        1 |  6050 | `					}` |
|        - |  6051 | `					/* Post-decrement: pTos retains the old value. */` |
|        - |  6052 | `				}` |
|       13 |  6053 | `			}` |
|       14 |  6054 | `		}else{` |
|      ! 0 |  6055 | `			if( pInstr->iP1 ){` |
|      ! 0 |  6056 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|        - |  6057 | `					/* Non-numeric string, no lvalue: no-op (value unchanged). */` |
|      ! 0 |  6058 | `				}else{` |
|        - |  6059 | `					/* Force a numeric cast */` |
|      ! 0 |  6060 | `					PH7_MemObjToNumeric(pTos);` |
|        - |  6061 | `					/* Pre-decrement */` |
|      ! 0 |  6062 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  6063 | `						pTos->rVal--;` |
|        - |  6064 | `						/* Keep the cached int consistent with the new rVal. */` |
|      ! 0 |  6065 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  6066 | `					}else{` |
|      ! 0 |  6067 | `						pTos->x.iVal--;` |
|      ! 0 |  6068 | `						MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  6069 | `					}` |
|        - |  6070 | `				}` |
|      ! 0 |  6071 | `			}` |
|        - |  6072 | `		}` |
|       13 |  6073 | `	}` |
|       29 |  6074 | `	break;` |
|        - |  6075 | `/*` |
|        - |  6076 | ` * UMINUS: * * *` |
|        - |  6077 | ` *` |
|        - |  6078 | ` * Perform a unary minus operation.` |
|        - |  6079 | ` */` |
|    29879 |  6080 | `case PH7_OP_UMINUS:` |
|        - |  6081 | `#ifdef UNTRUST` |
|        - |  6082 | `	if( pTos < pStack ){` |
|        - |  6083 | `		goto Abort;` |
|        - |  6084 | `	}` |
|        - |  6085 | `#endif` |
|        - |  6086 | `	/* Force a numeric (integer,real or both) cast */` |
|    59760 |  6087 | `	PH7_MemObjToNumeric(pTos);` |
|    59760 |  6088 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       31 |  6089 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  6090 | `	}` |
|    59760 |  6091 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    59730 |  6092 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    29864 |  6093 | `	}` |
|    59760 |  6094 | `	break;` |
|        - |  6095 | `/*` |
|        - |  6096 | ` * UPLUS: * * *` |
|        - |  6097 | ` *` |
|        - |  6098 | ` * Perform a unary plus operation.` |
|        - |  6099 | ` */` |
|       18 |  6100 | `case PH7_OP_UPLUS:` |
|        - |  6101 | `#ifdef UNTRUST` |
|        - |  6102 | `	if( pTos < pStack ){` |
|        - |  6103 | `		goto Abort;` |
|        - |  6104 | `	}` |
|        - |  6105 | `#endif` |
|        - |  6106 | `	/* Force a numeric (integer,real or both) cast */` |
|       37 |  6107 | `	PH7_MemObjToNumeric(pTos);` |
|       37 |  6108 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  6109 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  6110 | `	}` |
|       37 |  6111 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       37 |  6112 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       18 |  6113 | `	}` |
|       37 |  6114 | `	break;` |
|        - |  6115 | `/*` |
|        - |  6116 | ` * OP_LNOT: * * *` |
|        - |  6117 | ` *` |
|        - |  6118 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  6119 | ` * with its complement.` |
|        - |  6120 | ` */` |
|    45041 |  6121 | `case PH7_OP_LNOT:` |
|        - |  6122 | `#ifdef UNTRUST` |
|        - |  6123 | `	if( pTos < pStack ){` |
|        - |  6124 | `		goto Abort;` |
|        - |  6125 | `	}` |
|        - |  6126 | `#endif` |
|        - |  6127 | `	/* Force a boolean cast */` |
|    90128 |  6128 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       23 |  6129 | `		PH7_MemObjToBool(pTos);` |
|       11 |  6130 | `	}` |
|    90128 |  6131 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    90128 |  6132 | `	break;` |
|        - |  6133 | `/*` |
|        - |  6134 | ` * OP_BITNOT: * * *` |
|        - |  6135 | ` *` |
|        - |  6136 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  6137 | ` * with its ones-complement.` |
|        - |  6138 | ` */` |
|       14 |  6139 | `case PH7_OP_BITNOT:` |
|        - |  6140 | `#ifdef UNTRUST` |
|        - |  6141 | `	if( pTos < pStack ){` |
|        - |  6142 | `		goto Abort;` |
|        - |  6143 | `	}` |
|        - |  6144 | `#endif` |
|        - |  6145 | `	/* Force an integer cast */` |
|       30 |  6146 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6147 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6148 | `	}` |
|       30 |  6149 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       30 |  6150 | `	break;` |
|        - |  6151 | `/* OP_MUL * * *` |
|        - |  6152 | ` * OP_MUL_STORE * * *` |
|        - |  6153 | ` *` |
|        - |  6154 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  6155 | ` * and push the result back onto the stack.` |
|        - |  6156 | ` */` |
|     1290 |  6157 | `case PH7_OP_MUL:` |
|        - |  6158 | `case PH7_OP_MUL_STORE: {` |
|     2582 |  6159 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6160 | `	/* Force the operand to be numeric */` |
|        - |  6161 | `#ifdef UNTRUST` |
|        - |  6162 | `	if( pNos < pStack ){` |
|        - |  6163 | `		goto Abort;` |
|        - |  6164 | `	}` |
|        - |  6165 | `#endif` |
|     2582 |  6166 | `	PH7_MemObjToNumeric(pTos);` |
|     2582 |  6167 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  6168 | `	/* Perform the requested operation */` |
|     2582 |  6169 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  6170 | `		/* Floating point arithemic */` |
|        - |  6171 | `		ph7_real a,b,r;` |
|       21 |  6172 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        9 |  6173 | `			PH7_MemObjToReal(pTos);` |
|        4 |  6174 | `		}` |
|       21 |  6175 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  6176 | `			PH7_MemObjToReal(pNos);` |
|        3 |  6177 | `		}` |
|       21 |  6178 | `		a = pNos->rVal;` |
|       21 |  6179 | `		b = pTos->rVal;` |
|       21 |  6180 | `		r = a * b;` |
|        - |  6181 | `		/* Push the result */` |
|       21 |  6182 | `		pNos->rVal = r;` |
|       21 |  6183 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6184 | `		/* Try to get an integer representation */` |
|       21 |  6185 | `		PH7_MemObjTryInteger(pNos);` |
|       11 |  6186 | `	}else{` |
|        - |  6187 | `		/* Integer arithmetic */` |
|        - |  6188 | `		sxi64 a,b,r;` |
|     2562 |  6189 | `		a = pNos->x.iVal;` |
|     2562 |  6190 | `		b = pTos->x.iVal;` |
|     2562 |  6191 | `		r = a * b;` |
|        - |  6192 | `		/* Push the result */` |
|     2562 |  6193 | `		pNos->x.iVal = r;` |
|     2562 |  6194 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  6195 | `	}` |
|     2582 |  6196 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  6197 | `		ph7_value *pObj;` |
|       32 |  6198 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6199 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       32 |  6200 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       32 |  6201 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       32 |  6202 | `			PH7_MemObjStore(pNos,pObj);` |
|       15 |  6203 | `		}` |
|       15 |  6204 | `	}` |
|     2582 |  6205 | `	VmPopOperand(&pTos,1);` |
|     2582 |  6206 | `	break;` |
|        - |  6207 | `				 }` |
|        - |  6208 | `/* OP_POW * * *` |
|        - |  6209 | ` * OP_POW_STORE * * *` |
|        - |  6210 | ` *` |
|        - |  6211 | ` * Pop the top two elements from the stack, raise the second to the` |
|        - |  6212 | ` * power of the first, and push the result. PHP semantics: int**int` |
|        - |  6213 | ` * stays integer iff the exponent is non-negative and the exact result` |
|        - |  6214 | ` * fits in sxi64; otherwise the result is a double.` |
|        - |  6215 | ` */` |
|       67 |  6216 | `case PH7_OP_POW:` |
|        - |  6217 | `case PH7_OP_POW_STORE: {` |
|      135 |  6218 | `	ph7_value *pNos = &pTos[-1];` |
|      135 |  6219 | `	int bStore = (pInstr->iOp == PH7_OP_POW_STORE);` |
|        - |  6220 | `	/* Operand order convention (matches DIV/SUB_STORE):` |
|        - |  6221 | `	 *   POW:       base = pNos (evaluated first),   exp = pTos` |
|        - |  6222 | `	 *   POW_STORE: base = pTos (lvalue, last),       exp = pNos` |
|        - |  6223 | `	 */` |
|      135 |  6224 | `	ph7_value *pBase = bStore ? pTos : pNos;` |
|      135 |  6225 | `	ph7_value *pExp  = bStore ? pNos : pTos;` |
|        - |  6226 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |  6227 | `	int bBothInt;` |
|      135 |  6228 | `	int usedInt = 0;` |
|        - |  6229 | `	ph7_real a, b, r;` |
|        - |  6230 | `#endif` |
|      135 |  6231 | `	sxi64 base_i = 0, exp_i = 0;` |
|        - |  6232 | `#ifdef UNTRUST` |
|        - |  6233 | `	if( pNos < pStack ){` |
|        - |  6234 | `		goto Abort;` |
|        - |  6235 | `	}` |
|        - |  6236 | `#endif` |
|      135 |  6237 | `	PH7_MemObjToNumeric(pTos);` |
|      135 |  6238 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  6239 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      265 |  6240 | `	bBothInt = ((pTos->iFlags & MEMOBJ_REAL) == 0) &&` |
|      130 |  6241 | `	           ((pNos->iFlags & MEMOBJ_REAL) == 0);` |
|      135 |  6242 | `	if( bBothInt ){` |
|      123 |  6243 | `		base_i = pBase->x.iVal;` |
|      123 |  6244 | `		exp_i  = pExp->x.iVal;` |
|       61 |  6245 | `	}` |
|      135 |  6246 | `	if( (pBase->iFlags & MEMOBJ_REAL) == 0 ){` |
|      125 |  6247 | `		PH7_MemObjToReal(pBase);` |
|       62 |  6248 | `	}` |
|      135 |  6249 | `	if( (pExp->iFlags & MEMOBJ_REAL) == 0 ){` |
|      133 |  6250 | `		PH7_MemObjToReal(pExp);` |
|       66 |  6251 | `	}` |
|      135 |  6252 | `	a = pBase->rVal;` |
|      135 |  6253 | `	b = pExp->rVal;` |
|      135 |  6254 | `	r = pow(a, b);` |
|        - |  6255 | `	/* Match PHP: int**non-negative-int stays int when the exact result` |
|        - |  6256 | `	 * fits in sxi64. Use exponentiation by squaring with overflow checks` |
|        - |  6257 | `	 * rather than casting the double back, because the boundary 2^63 is` |
|        - |  6258 | `	 * representable as double but not as signed int64. */` |
|      135 |  6259 | `	if( bBothInt && exp_i >= 0 ){` |
|      117 |  6260 | `		sxi64 result_i = 1;` |
|      117 |  6261 | `		sxi64 cur_base = base_i;` |
|      117 |  6262 | `		sxi64 cur_exp  = exp_i;` |
|      117 |  6263 | `		int overflow = 0;` |
|      401 |  6264 | `		while( cur_exp > 0 ){` |
|      289 |  6265 | `			if( cur_exp & 1 ){` |
|      189 |  6266 | `				if( VmMulOverflow64(result_i, cur_base, &result_i) ){` |
|        3 |  6267 | `					overflow = 1;` |
|        3 |  6268 | `					break;` |
|        - |  6269 | `				}` |
|       93 |  6270 | `			}` |
|      287 |  6271 | `			cur_exp >>= 1;` |
|      287 |  6272 | `			if( cur_exp > 0 ){` |
|      181 |  6273 | `				if( VmMulOverflow64(cur_base, cur_base, &cur_base) ){` |
|        3 |  6274 | `					overflow = 1;` |
|        3 |  6275 | `					break;` |
|        - |  6276 | `				}` |
|       89 |  6277 | `			}` |
|        1 |  6278 | `		}` |
|      117 |  6279 | `		if( !overflow ){` |
|      113 |  6280 | `			pNos->x.iVal = result_i;` |
|      113 |  6281 | `			MemObjSetType(pNos, MEMOBJ_INT);` |
|      113 |  6282 | `			usedInt = 1;` |
|       56 |  6283 | `		}` |
|       58 |  6284 | `	}` |
|      135 |  6285 | `	if( !usedInt ){` |
|       23 |  6286 | `		pNos->rVal = r;` |
|       23 |  6287 | `		MemObjSetType(pNos, MEMOBJ_REAL);` |
|       11 |  6288 | `	}` |
|        - |  6289 | `#else` |
|        - |  6290 | `	/* PH7_OMIT_FLOATING_POINT: integer-only build. No libm / no pow().` |
|        - |  6291 | `	 * Exponentiation by squaring with silent wrap on overflow, matching` |
|        - |  6292 | `	 * the integer-wrap semantics of PH7_OP_MUL in the same build mode.` |
|        - |  6293 | `	 * Negative exponents yield 0 since fractional results cannot be` |
|        - |  6294 | `	 * represented. */` |
|        - |  6295 | `	base_i = pBase->x.iVal;` |
|        - |  6296 | `	exp_i  = pExp->x.iVal;` |
|        - |  6297 | `	{` |
|        - |  6298 | `		sxi64 result_i = 1;` |
|        - |  6299 | `		sxi64 cur_base = base_i;` |
|        - |  6300 | `		sxi64 cur_exp  = exp_i;` |
|        - |  6301 | `		if( cur_exp < 0 ){` |
|        - |  6302 | `			result_i = 0;` |
|        - |  6303 | `		}else{` |
|        - |  6304 | `			while( cur_exp > 0 ){` |
|        - |  6305 | `				if( cur_exp & 1 ){` |
|        - |  6306 | `					result_i *= cur_base;` |
|        - |  6307 | `				}` |
|        - |  6308 | `				cur_exp >>= 1;` |
|        - |  6309 | `				if( cur_exp > 0 ){` |
|        - |  6310 | `					cur_base *= cur_base;` |
|        - |  6311 | `				}` |
|        - |  6312 | `			}` |
|        - |  6313 | `		}` |
|        - |  6314 | `		pNos->x.iVal = result_i;` |
|        - |  6315 | `		MemObjSetType(pNos, MEMOBJ_INT);` |
|        - |  6316 | `	}` |
|        - |  6317 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      135 |  6318 | `	if( bStore ){` |
|        - |  6319 | `		ph7_value *pObj;` |
|       23 |  6320 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6321 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       23 |  6322 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       23 |  6323 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       23 |  6324 | `			PH7_MemObjStore(pNos,pObj);` |
|       11 |  6325 | `		}` |
|       11 |  6326 | `	}` |
|      135 |  6327 | `	VmPopOperand(&pTos,1);` |
|      135 |  6328 | `	break;` |
|        - |  6329 | `				 }` |
|        - |  6330 | `/* OP_ADD * * *` |
|        - |  6331 | ` *` |
|        - |  6332 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  6333 | ` * and push the result back onto the stack.` |
|        - |  6334 | ` */` |
|      536 |  6335 | `case PH7_OP_ADD:{` |
|     1074 |  6336 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6337 | `#ifdef UNTRUST` |
|        - |  6338 | `	if( pNos < pStack ){` |
|        - |  6339 | `		goto Abort;` |
|        - |  6340 | `	}` |
|        - |  6341 | `#endif` |
|        - |  6342 | `	/* Perform the addition */` |
|     1074 |  6343 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|     1074 |  6344 | `	VmPopOperand(&pTos,1);` |
|     1074 |  6345 | `	break;` |
|        - |  6346 | `				}` |
|        - |  6347 | `/*` |
|        - |  6348 | ` * OP_ADD_STORE * * *` |
|        - |  6349 | ` *` |
|        - |  6350 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  6351 | ` * and push the result back onto the stack.` |
|        - |  6352 | ` */` |
|      502 |  6353 | `case PH7_OP_ADD_STORE:{` |
|     1006 |  6354 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6355 | `	ph7_value *pObj;` |
|        - |  6356 | `	sxu32 nIdx;` |
|        - |  6357 | `#ifdef UNTRUST` |
|        - |  6358 | `	if( pNos < pStack ){` |
|        - |  6359 | `		goto Abort;` |
|        - |  6360 | `	}` |
|        - |  6361 | `#endif` |
|        - |  6362 | `	/* Perform the addition */` |
|     1006 |  6363 | `	nIdx = pTos->nIdx;` |
|     1006 |  6364 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  6365 | `	/* Peform the store operation */` |
|     1006 |  6366 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  6367 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     1006 |  6368 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|     1006 |  6369 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|     1006 |  6370 | `		PH7_MemObjStore(pTos,pObj);` |
|      502 |  6371 | `	}` |
|        - |  6372 | `	/* Ticket 1433-35: Perform a stack dup */` |
|     1006 |  6373 | `	PH7_MemObjStore(pTos,pNos);` |
|     1006 |  6374 | `	VmPopOperand(&pTos,1);` |
|     1006 |  6375 | `	break;` |
|        - |  6376 | `				}` |
|        - |  6377 | `/* OP_SUB * * *` |
|        - |  6378 | ` *` |
|        - |  6379 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  6380 | ` * first (what was next on the stack) from the second (the` |
|        - |  6381 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  6382 | ` */` |
|      352 |  6383 | `case PH7_OP_SUB: {` |
|      706 |  6384 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6385 | `#ifdef UNTRUST` |
|        - |  6386 | `	if( pNos < pStack ){` |
|        - |  6387 | `		goto Abort;` |
|        - |  6388 | `	}` |
|        - |  6389 | `#endif` |
|      706 |  6390 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  6391 | `		/* Floating point arithemic */` |
|        - |  6392 | `		ph7_real a,b,r;` |
|      103 |  6393 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  6394 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  6395 | `		}` |
|      103 |  6396 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  6397 | `			PH7_MemObjToReal(pNos);` |
|        2 |  6398 | `		}` |
|      103 |  6399 | `		a = pNos->rVal;` |
|      103 |  6400 | `		b = pTos->rVal;` |
|      103 |  6401 | `		r = a - b;` |
|        - |  6402 | `		/* Push the result */` |
|      103 |  6403 | `		pNos->rVal = r;` |
|      103 |  6404 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6405 | `		/* Try to get an integer representation */` |
|      103 |  6406 | `		PH7_MemObjTryInteger(pNos);` |
|       52 |  6407 | `	}else{` |
|        - |  6408 | `		/* Integer arithmetic */` |
|        - |  6409 | `		sxi64 a,b,r;` |
|      604 |  6410 | `		a = pNos->x.iVal;` |
|      604 |  6411 | `		b = pTos->x.iVal;` |
|      604 |  6412 | `		r = a - b;` |
|        - |  6413 | `		/* Push the result */` |
|      604 |  6414 | `		pNos->x.iVal = r;` |
|      604 |  6415 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  6416 | `	}` |
|      706 |  6417 | `	VmPopOperand(&pTos,1);` |
|      706 |  6418 | `	break;` |
|        - |  6419 | `				 }` |
|        - |  6420 | `/* OP_SUB_STORE * * *` |
|        - |  6421 | ` *` |
|        - |  6422 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  6423 | ` * first (what was next on the stack) from the second (the` |
|        - |  6424 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  6425 | ` */` |
|        4 |  6426 | `case PH7_OP_SUB_STORE: {` |
|       10 |  6427 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6428 | `	ph7_value *pObj;` |
|        - |  6429 | `#ifdef UNTRUST` |
|        - |  6430 | `	if( pNos < pStack ){` |
|        - |  6431 | `		goto Abort;` |
|        - |  6432 | `	}` |
|        - |  6433 | `#endif` |
|       10 |  6434 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  6435 | `		/* Floating point arithemic */` |
|        - |  6436 | `		ph7_real a,b,r;` |
|      ! 0 |  6437 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  6438 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  6439 | `		}` |
|      ! 0 |  6440 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  6441 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  6442 | `		}` |
|      ! 0 |  6443 | `		a = pTos->rVal;` |
|      ! 0 |  6444 | `		b = pNos->rVal;` |
|      ! 0 |  6445 | `		r = a - b;` |
|        - |  6446 | `		/* Push the result */` |
|      ! 0 |  6447 | `		pNos->rVal = r;` |
|      ! 0 |  6448 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6449 | `		/* Try to get an integer representation */` |
|      ! 0 |  6450 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  6451 | `	}else{` |
|        - |  6452 | `		/* Integer arithmetic */` |
|        - |  6453 | `		sxi64 a,b,r;` |
|       10 |  6454 | `		a = pTos->x.iVal;` |
|       10 |  6455 | `		b = pNos->x.iVal;` |
|       10 |  6456 | `		r = a - b;` |
|        - |  6457 | `		/* Push the result */` |
|       10 |  6458 | `		pNos->x.iVal = r;` |
|       10 |  6459 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  6460 | `	}` |
|       10 |  6461 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6462 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       10 |  6463 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       10 |  6464 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       10 |  6465 | `		PH7_MemObjStore(pNos,pObj);` |
|        4 |  6466 | `	}` |
|       10 |  6467 | `	VmPopOperand(&pTos,1);` |
|       10 |  6468 | `	break;` |
|        - |  6469 | `				 }` |
|        - |  6470 |  |
|        - |  6471 | `/*` |
|        - |  6472 | ` * OP_MOD * * *` |
|        - |  6473 | ` *` |
|        - |  6474 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6475 | ` * first (what was next on the stack) from the second (the` |
|        - |  6476 | ` * top of the stack) and push the remainder after division` |
|        - |  6477 | ` * onto the stack.` |
|        - |  6478 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  6479 | ` */` |
|      309 |  6480 | `case PH7_OP_MOD:{` |
|      620 |  6481 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6482 | `	sxi64 a,b,r;` |
|        - |  6483 | `#ifdef UNTRUST` |
|        - |  6484 | `	if( pNos < pStack ){` |
|        - |  6485 | `		goto Abort;` |
|        - |  6486 | `	}` |
|        - |  6487 | `#endif` |
|        - |  6488 | `	/* Force the operands to be integer */` |
|      620 |  6489 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6490 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6491 | `	}` |
|      620 |  6492 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  6493 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  6494 | `	}` |
|        - |  6495 | `	/* Perform the requested operation */` |
|      620 |  6496 | `	a = pNos->x.iVal;` |
|      620 |  6497 | `	b = pTos->x.iVal;` |
|      620 |  6498 | `	if( b == 0 ){` |
|        3 |  6499 | `		r = 0;` |
|        3 |  6500 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  6501 | `		/* goto Abort; */` |
|        2 |  6502 | `	}else{` |
|      617 |  6503 | `		r = a%b;` |
|        - |  6504 | `	}` |
|        - |  6505 | `	/* Push the result */` |
|      620 |  6506 | `	pNos->x.iVal = r;` |
|      620 |  6507 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      620 |  6508 | `	VmPopOperand(&pTos,1);` |
|      620 |  6509 | `	break;` |
|        - |  6510 | `				}` |
|        - |  6511 | `/*` |
|        - |  6512 | ` * OP_MOD_STORE * * *` |
|        - |  6513 | ` *` |
|        - |  6514 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6515 | ` * first (what was next on the stack) from the second (the` |
|        - |  6516 | ` * top of the stack) and push the remainder after division` |
|        - |  6517 | ` * onto the stack.` |
|        - |  6518 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  6519 | ` */` |
|        1 |  6520 | `case PH7_OP_MOD_STORE: {` |
|        3 |  6521 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6522 | `	ph7_value *pObj;` |
|        - |  6523 | `	sxi64 a,b,r;` |
|        - |  6524 | `#ifdef UNTRUST` |
|        - |  6525 | `	if( pNos < pStack ){` |
|        - |  6526 | `		goto Abort;` |
|        - |  6527 | `	}` |
|        - |  6528 | `#endif` |
|        - |  6529 | `	/* Force the operands to be integer */` |
|        3 |  6530 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6531 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6532 | `	}` |
|        3 |  6533 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6534 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6535 | `	}` |
|        - |  6536 | `	/* Perform the requested operation */` |
|        3 |  6537 | `	a = pTos->x.iVal;` |
|        3 |  6538 | `	b = pNos->x.iVal;` |
|        3 |  6539 | `	if( b == 0 ){` |
|      ! 0 |  6540 | `		r = 0;` |
|      ! 0 |  6541 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  6542 | `		/* goto Abort; */` |
|      ! 0 |  6543 | `	}else{` |
|        3 |  6544 | `		r = a%b;` |
|        - |  6545 | `	}` |
|        - |  6546 | `	/* Push the result */` |
|        3 |  6547 | `	pNos->x.iVal = r;` |
|        3 |  6548 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  6549 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6550 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  6551 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  6552 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        3 |  6553 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  6554 | `	}` |
|        3 |  6555 | `	VmPopOperand(&pTos,1);` |
|        3 |  6556 | `	break;` |
|        - |  6557 | `				}` |
|        - |  6558 | `/*` |
|        - |  6559 | ` * OP_DIV * * *` |
|        - |  6560 | ` *` |
|        - |  6561 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6562 | ` * first (what was next on the stack) from the second (the` |
|        - |  6563 | ` * top of the stack) and push the result onto the stack.` |
|        - |  6564 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  6565 | ` */` |
|       33 |  6566 | `case PH7_OP_DIV:{` |
|       68 |  6567 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6568 | `	ph7_real a,b,r;` |
|        - |  6569 | `#ifdef UNTRUST` |
|        - |  6570 | `	if( pNos < pStack ){` |
|        - |  6571 | `		goto Abort;` |
|        - |  6572 | `	}` |
|        - |  6573 | `#endif` |
|        - |  6574 | `	/* Force the operands to be real */` |
|       68 |  6575 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       62 |  6576 | `		PH7_MemObjToReal(pTos);` |
|       30 |  6577 | `	}` |
|       68 |  6578 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       28 |  6579 | `		PH7_MemObjToReal(pNos);` |
|       13 |  6580 | `	}` |
|        - |  6581 | `	/* Perform the requested operation */` |
|       68 |  6582 | `	a = pNos->rVal;` |
|       68 |  6583 | `	b = pTos->rVal;` |
|       68 |  6584 | `	if( b == 0 ){` |
|        - |  6585 | `		/* Division by zero */` |
|        3 |  6586 | `		pNos->rVal = 0;` |
|        3 |  6587 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  6588 | `		/* goto Abort; */` |
|        2 |  6589 | `	}else{` |
|       65 |  6590 | `		r = a/b;` |
|        - |  6591 | `		/* Push the result */` |
|       65 |  6592 | `		pNos->rVal = r;` |
|       65 |  6593 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6594 | `		/* Try to get an integer representation */` |
|       65 |  6595 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  6596 | `	}` |
|       68 |  6597 | `	VmPopOperand(&pTos,1);` |
|       68 |  6598 | `	break;` |
|        - |  6599 | `				}` |
|        - |  6600 | `/*` |
|        - |  6601 | ` * OP_DIV_STORE * * *` |
|        - |  6602 | ` *` |
|        - |  6603 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6604 | ` * first (what was next on the stack) from the second (the` |
|        - |  6605 | ` * top of the stack) and push the result onto the stack.` |
|        - |  6606 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  6607 | ` */` |
|        2 |  6608 | `case PH7_OP_DIV_STORE:{` |
|        5 |  6609 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6610 | `	ph7_value *pObj;` |
|        - |  6611 | `	ph7_real a,b,r;` |
|        - |  6612 | `#ifdef UNTRUST` |
|        - |  6613 | `	if( pNos < pStack ){` |
|        - |  6614 | `		goto Abort;` |
|        - |  6615 | `	}` |
|        - |  6616 | `#endif` |
|        - |  6617 | `	/* Force the operands to be real */` |
|        5 |  6618 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  6619 | `		PH7_MemObjToReal(pTos);` |
|        2 |  6620 | `	}` |
|        5 |  6621 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  6622 | `		PH7_MemObjToReal(pNos);` |
|        2 |  6623 | `	}` |
|        - |  6624 | `	/* Perform the requested operation */` |
|        5 |  6625 | `	a = pTos->rVal;` |
|        5 |  6626 | `	b = pNos->rVal;` |
|        5 |  6627 | `	if( b == 0 ){` |
|        - |  6628 | `		/* Division by zero */` |
|      ! 0 |  6629 | `		r = 0;` |
|      ! 0 |  6630 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  6631 | `		/* goto Abort; */` |
|      ! 0 |  6632 | `	}else{` |
|        5 |  6633 | `		r = a/b;` |
|        - |  6634 | `		/* Push the result */` |
|        5 |  6635 | `		pNos->rVal = r;` |
|        5 |  6636 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6637 | `		/* Try to get an integer representation */` |
|        5 |  6638 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  6639 | `	}` |
|        5 |  6640 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6641 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        5 |  6642 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        5 |  6643 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        5 |  6644 | `		PH7_MemObjStore(pNos,pObj);` |
|        2 |  6645 | `	}` |
|        5 |  6646 | `	VmPopOperand(&pTos,1);` |
|        5 |  6647 | `	break;` |
|        - |  6648 | `				}` |
|        - |  6649 | `/* OP_BAND * * *` |
|        - |  6650 | ` *` |
|        - |  6651 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6652 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  6653 | ` * two elements.` |
|        - |  6654 | `*/` |
|        - |  6655 | `/* OP_BOR * * *` |
|        - |  6656 | ` *` |
|        - |  6657 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6658 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  6659 | ` * two elements.` |
|        - |  6660 | ` */` |
|        - |  6661 | `/* OP_BXOR * * *` |
|        - |  6662 | ` *` |
|        - |  6663 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6664 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  6665 | ` * two elements.` |
|        - |  6666 | ` */` |
|       43 |  6667 | `case PH7_OP_BAND:` |
|        - |  6668 | `case PH7_OP_BOR:` |
|        - |  6669 | `case PH7_OP_BXOR:{` |
|       88 |  6670 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6671 | `	sxi64 a,b,r;` |
|        - |  6672 | `#ifdef UNTRUST` |
|        - |  6673 | `	if( pNos < pStack ){` |
|        - |  6674 | `		goto Abort;` |
|        - |  6675 | `	}` |
|        - |  6676 | `#endif` |
|        - |  6677 | `	/* Force the operands to be integer */` |
|       88 |  6678 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6679 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6680 | `	}` |
|       88 |  6681 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6682 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6683 | `	}` |
|        - |  6684 | `	/* Perform the requested operation */` |
|       88 |  6685 | `	a = pNos->x.iVal;` |
|       88 |  6686 | `	b = pTos->x.iVal;` |
|       88 |  6687 | `	switch(pInstr->iOp){` |
|        7 |  6688 | `	case PH7_OP_BOR_STORE:` |
|       15 |  6689 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        7 |  6690 | `	case PH7_OP_BXOR_STORE:` |
|       15 |  6691 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       29 |  6692 | `	case PH7_OP_BAND_STORE:` |
|       29 |  6693 | `	case PH7_OP_BAND:` |
|       60 |  6694 | `	default:          r = a&b; break;` |
|        - |  6695 | `	}` |
|        - |  6696 | `	/* Push the result */` |
|       88 |  6697 | `	pNos->x.iVal = r;` |
|       88 |  6698 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       88 |  6699 | `	VmPopOperand(&pTos,1);` |
|       88 |  6700 | `	break;` |
|        - |  6701 | `				 }` |
|        - |  6702 | `/* OP_BAND_STORE * * *` |
|        - |  6703 | ` *` |
|        - |  6704 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6705 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  6706 | ` * two elements.` |
|        - |  6707 | `*/` |
|        - |  6708 | `/* OP_BOR_STORE * * *` |
|        - |  6709 | ` *` |
|        - |  6710 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6711 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  6712 | ` * two elements.` |
|        - |  6713 | ` */` |
|        - |  6714 | `/* OP_BXOR_STORE * * *` |
|        - |  6715 | ` *` |
|        - |  6716 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6717 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  6718 | ` * two elements.` |
|        - |  6719 | ` */` |
|       10 |  6720 | `case PH7_OP_BAND_STORE:` |
|        - |  6721 | `case PH7_OP_BOR_STORE:` |
|        - |  6722 | `case PH7_OP_BXOR_STORE:{` |
|       21 |  6723 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6724 | `	ph7_value *pObj;` |
|        - |  6725 | `	sxi64 a,b,r;` |
|        - |  6726 | `#ifdef UNTRUST` |
|        - |  6727 | `	if( pNos < pStack ){` |
|        - |  6728 | `		goto Abort;` |
|        - |  6729 | `	}` |
|        - |  6730 | `#endif` |
|        - |  6731 | `	/* Force the operands to be integer */` |
|       21 |  6732 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6733 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6734 | `	}` |
|       21 |  6735 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6736 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6737 | `	}` |
|        - |  6738 | `	/* Perform the requested operation */` |
|       21 |  6739 | `	a = pTos->x.iVal;` |
|       21 |  6740 | `	b = pNos->x.iVal;` |
|       21 |  6741 | `	switch(pInstr->iOp){` |
|        3 |  6742 | `	case PH7_OP_BOR_STORE:` |
|        7 |  6743 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        4 |  6744 | `	case PH7_OP_BXOR_STORE:` |
|        9 |  6745 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        3 |  6746 | `	case PH7_OP_BAND_STORE:` |
|        3 |  6747 | `	case PH7_OP_BAND:` |
|        7 |  6748 | `	default:          r = a&b; break;` |
|        - |  6749 | `	}` |
|        - |  6750 | `	/* Push the result */` |
|       21 |  6751 | `	pNos->x.iVal = r;` |
|       21 |  6752 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       21 |  6753 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6754 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       21 |  6755 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       21 |  6756 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       21 |  6757 | `		PH7_MemObjStore(pNos,pObj);` |
|       10 |  6758 | `	}` |
|       21 |  6759 | `	VmPopOperand(&pTos,1);` |
|       21 |  6760 | `	break;` |
|        - |  6761 | `				 }` |
|        - |  6762 | `/* OP_SHL * * *` |
|        - |  6763 | ` *` |
|        - |  6764 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6765 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6766 | ` * left by N bits where N is the top element on the stack.` |
|        - |  6767 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6768 | ` */` |
|        - |  6769 | `/* OP_SHR * * *` |
|        - |  6770 | ` *` |
|        - |  6771 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6772 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6773 | ` * right by N bits where N is the top element on the stack.` |
|        - |  6774 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6775 | ` */` |
|       12 |  6776 | `case PH7_OP_SHL:` |
|        - |  6777 | `case PH7_OP_SHR: {` |
|       25 |  6778 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6779 | `	sxi64 a,r;` |
|        - |  6780 | `	sxi32 b;` |
|        - |  6781 | `#ifdef UNTRUST` |
|        - |  6782 | `	if( pNos < pStack ){` |
|        - |  6783 | `		goto Abort;` |
|        - |  6784 | `	}` |
|        - |  6785 | `#endif` |
|        - |  6786 | `	/* Force the operands to be integer */` |
|       25 |  6787 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6788 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6789 | `	}` |
|       25 |  6790 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6791 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6792 | `	}` |
|        - |  6793 | `	/* Perform the requested operation */` |
|       25 |  6794 | `	a = pNos->x.iVal;` |
|       25 |  6795 | `	b = (sxi32)pTos->x.iVal;` |
|       25 |  6796 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       15 |  6797 | `		r = a << b;` |
|        8 |  6798 | `	}else{` |
|       11 |  6799 | `		r = a >> b;` |
|        - |  6800 | `	}` |
|        - |  6801 | `	/* Push the result */` |
|       25 |  6802 | `	pNos->x.iVal = r;` |
|       25 |  6803 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       25 |  6804 | `	VmPopOperand(&pTos,1);` |
|       25 |  6805 | `	break;` |
|        - |  6806 | `				 }` |
|        - |  6807 | `/*  OP_SHL_STORE * * *` |
|        - |  6808 | ` *` |
|        - |  6809 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6810 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6811 | ` * left by N bits where N is the top element on the stack.` |
|        - |  6812 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6813 | ` */` |
|        - |  6814 | `/* OP_SHR_STORE * * *` |
|        - |  6815 | ` *` |
|        - |  6816 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6817 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6818 | ` * right by N bits where N is the top element on the stack.` |
|        - |  6819 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6820 | ` */` |
|        9 |  6821 | `case PH7_OP_SHL_STORE:` |
|        - |  6822 | `case PH7_OP_SHR_STORE: {` |
|       19 |  6823 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6824 | `	ph7_value *pObj;` |
|        - |  6825 | `	sxi64 a,r;` |
|        - |  6826 | `	sxi32 b;` |
|        - |  6827 | `#ifdef UNTRUST` |
|        - |  6828 | `	if( pNos < pStack ){` |
|        - |  6829 | `		goto Abort;` |
|        - |  6830 | `	}` |
|        - |  6831 | `#endif` |
|        - |  6832 | `	/* Force the operands to be integer */` |
|       19 |  6833 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6834 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6835 | `	}` |
|       19 |  6836 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6837 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6838 | `	}` |
|        - |  6839 | `	/* Perform the requested operation */` |
|       19 |  6840 | `	a = pTos->x.iVal;` |
|       19 |  6841 | `	b = (sxi32)pNos->x.iVal;` |
|       19 |  6842 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        9 |  6843 | `		r = a << b;` |
|        5 |  6844 | `	}else{` |
|       11 |  6845 | `		r = a >> b;` |
|        - |  6846 | `	}` |
|        - |  6847 | `	/* Push the result */` |
|       19 |  6848 | `	pNos->x.iVal = r;` |
|       19 |  6849 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  6850 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6851 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  6852 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  6853 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       19 |  6854 | `		PH7_MemObjStore(pNos,pObj);` |
|        9 |  6855 | `	}` |
|       19 |  6856 | `	VmPopOperand(&pTos,1);` |
|       19 |  6857 | `	break;` |
|        - |  6858 | `				 }` |
|        - |  6859 | `/* CAT:  P1 * *` |
|        - |  6860 | ` *` |
|        - |  6861 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  6862 | ` * back.` |
|        - |  6863 | ` */` |
|    72080 |  6864 | `case PH7_OP_CAT:{` |
|        - |  6865 | `	ph7_value *pNos,*pCur;` |
|   144162 |  6866 | `	if( pInstr->iP1 < 1 ){` |
|   116676 |  6867 | `		pNos = &pTos[-1];` |
|    58339 |  6868 | `	}else{` |
|    27488 |  6869 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  6870 | `	}` |
|        - |  6871 | `#ifdef UNTRUST` |
|        - |  6872 | `	if( pNos < pStack ){` |
|        - |  6873 | `		goto Abort;` |
|        - |  6874 | `	}` |
|        - |  6875 | `#endif` |
|        - |  6876 | `	/* Force a string cast */` |
|   144162 |  6877 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1672 |  6878 | `		PH7_MemObjToString(pNos);` |
|      835 |  6879 | `	}` |
|   144162 |  6880 | `	pCur = &pNos[1];` |
|   291052 |  6881 | `	while( pCur <= pTos ){` |
|   146892 |  6882 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50962 |  6883 | `			PH7_MemObjToString(pCur);` |
|    25480 |  6884 | `		}` |
|        - |  6885 | `		/* Perform the concatenation */` |
|   146892 |  6886 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   146848 |  6887 | `			if( PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob)) != SXRET_OK ){` |
|        - |  6888 | `				/* Allocation failure: raise a fatal instead of a truncated concat */` |
|      ! 0 |  6889 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 |  6890 | `				goto Abort;` |
|        - |  6891 | `			}` |
|    73423 |  6892 | `		}` |
|   146892 |  6893 | `		SyBlobRelease(&pCur->sBlob);` |
|   146892 |  6894 | `		pCur++;` |
|        2 |  6895 | `	}` |
|   144162 |  6896 | `	pTos = pNos;` |
|   144162 |  6897 | `	break;` |
|        - |  6898 | `				}` |
|        - |  6899 | `/*  CAT_STORE: * * *` |
|        - |  6900 | ` *` |
|        - |  6901 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  6902 | ` * back.` |
|        - |  6903 | ` */` |
|     4149 |  6904 | `case PH7_OP_CAT_STORE:{` |
|     8300 |  6905 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6906 | `	ph7_value *pObj;` |
|        - |  6907 | `	sxu32 nIdx;` |
|        - |  6908 | `#ifdef UNTRUST` |
|        - |  6909 | `	if( pNos < pStack ){` |
|        - |  6910 | `		goto Abort;` |
|        - |  6911 | `	}` |
|        - |  6912 | `#endif` |
|        - |  6913 | `	/* The right operand must be a string to append it */` |
|     8300 |  6914 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  6915 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  6916 | `	}` |
|     8300 |  6917 | `	nIdx = pTos->nIdx;` |
|        - |  6918 | `	/* Fast path: append straight into the lvalue's own (geometrically grown) buffer` |
|        - |  6919 | `	 * instead of copy-on-write-dup'ing the read-only-aliased stack value and then` |
|        - |  6920 | ``	 * storing the whole buffer back twice. This turns `$s .= ...` (and the`` |
|        - |  6921 | `	 * $a[$i] .= / $obj->prop .= forms) from O(n^2) into amortized O(1).` |
|        - |  6922 | `	 * Guards: a real owned slot; the right operand must NOT alias that same slot` |
|        - |  6923 | ``	 * (`$s .= $s`, or a reference to it, would realloc the buffer out from under`` |
|        - |  6924 | `	 * the source we copy from — references share the slot index, so one check` |
|        - |  6925 | `	 * covers both); and not a typed property, whose store-time type check/coercion` |
|        - |  6926 | `	 * must run before any mutation (left to the slow path).` |
|        - |  6927 | ``	 * NOTE: the explicit `$s = $s . x` form (OP_CAT + OP_STORE) is not covered here`` |
|        - |  6928 | `	 * and remains O(n^2) by design. */` |
|     8301 |  6929 | `	if( nIdx != SXU32_HIGH` |
|     8298 |  6930 | `	 && nIdx != pNos->nIdx` |
|     8294 |  6931 | `	 && (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0` |
|     8292 |  6932 | `	 && (SyHashTotalEntry(&pVm->hTypedSlot) == 0` |
|     4148 |  6933 | `	     \|\| SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32)) == 0) ){` |
|     8286 |  6934 | `		if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6935 | `			/* e.g. $x = 5; $x .= "a";  ->  "5a" */` |
|        3 |  6936 | `			PH7_MemObjToString(pObj);` |
|        1 |  6937 | `		}` |
|     8286 |  6938 | `		if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     8284 |  6939 | `			if( PH7_MemObjStringAppend(pObj,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - |  6940 | `				/* Allocation failure: the grow happens before the copy, so pObj` |
|        - |  6941 | `				 * keeps its prior valid contents — raise the fatal uncorrupted. */` |
|      ! 0 |  6942 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 |  6943 | `				goto Abort;` |
|        - |  6944 | `			}` |
|     4141 |  6945 | `		}` |
|        - |  6946 | ``		/* Produce the expression result. A `.=` result is a temporary, never an`` |
|        - |  6947 | ``		 * addressable lvalue, so nIdx is SXU32_HIGH (otherwise `f($s .= "x")` with a`` |
|        - |  6948 | ``		 * by-ref param, or `&($s .= "x")`, would alias the live variable).`` |
|        - |  6949 | ``		 * In the dominant statement form `$s .= "x";` the result is discarded by the`` |
|        - |  6950 | `		 * very next opcode (OP_POP), so we skip building it and leave the (harmless)` |
|        - |  6951 | `		 * RHS operand for the POP to drop — keeping the hot path allocation-free.` |
|        - |  6952 | `		 * Otherwise the result is consumed, so materialize an INDEPENDENT owned copy` |
|        - |  6953 | `		 * of the updated value: a read-only alias into pObj's buffer would dangle if` |
|        - |  6954 | `		 * the same slot is appended to again later in the statement` |
|        - |  6955 | ``		 * (e.g. `($s .= "a") . ($s .= "b")` reallocs the buffer the first result`` |
|        - |  6956 | `		 * still points at). Peeking pInstr+1 is safe: the compiler always emits a` |
|        - |  6957 | `		 * terminating OP_DONE, so it is in-bounds inside any non-DONE opcode. */` |
|     8286 |  6958 | `		if( (pInstr+1)->iOp != PH7_OP_POP ){` |
|        9 |  6959 | `			PH7_MemObjStore(pObj,pNos);` |
|        4 |  6960 | `		}` |
|     8286 |  6961 | `		pNos->nIdx = SXU32_HIGH;` |
|     8286 |  6962 | `		VmPopOperand(&pTos,1);` |
|     8293 |  6963 | `		break;` |
|        - |  6964 | `	}` |
|        - |  6965 | `	/* Slow path: read-only/typed/constant-attribute/self-aliasing lvalues. */` |
|       16 |  6966 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6967 | `		/* Force a string cast */` |
|        6 |  6968 | `		PH7_MemObjToString(pTos);` |
|        2 |  6969 | `	}` |
|        - |  6970 | `	/* Perform the concatenation (Reverse order) */` |
|       16 |  6971 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|       16 |  6972 | `		if( PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - |  6973 | `			/* Allocation failure: raise a fatal before committing the store so` |
|        - |  6974 | `			 * no partially-concatenated value is written to the lvalue. */` |
|      ! 0 |  6975 | `			PH7_VmMemoryError(&(*pVm));` |
|      ! 0 |  6976 | `			goto Abort;` |
|        - |  6977 | `		}` |
|        7 |  6978 | `	}` |
|        - |  6979 | `	/* Perform the store operation */` |
|       16 |  6980 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6981 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       16 |  6982 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       16 |  6983 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);` |
|       11 |  6984 | `		PH7_MemObjStore(pTos,pObj);` |
|        5 |  6985 | `	}` |
|       11 |  6986 | `	PH7_MemObjStore(pTos,pNos);` |
|       11 |  6987 | `	VmPopOperand(&pTos,1);` |
|       11 |  6988 | `	break;` |
|        - |  6989 | `				}` |
|        - |  6990 | `/* OP_AND: * * *` |
|        - |  6991 | ` *` |
|        - |  6992 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  6993 | ` * two values and push the resulting boolean value back onto the` |
|        - |  6994 | ` * stack.` |
|        - |  6995 | ` */` |
|        - |  6996 | `/* OP_OR: * * *` |
|        - |  6997 | ` *` |
|        - |  6998 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  6999 | ` * two values and push the resulting boolean value back onto the` |
|        - |  7000 | ` * stack.` |
|        - |  7001 | ` */` |
|   108542 |  7002 | `case PH7_OP_LAND:` |
|        - |  7003 | `case PH7_OP_LOR: {` |
|   217130 |  7004 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7005 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  7006 | `#ifdef UNTRUST` |
|        - |  7007 | `	if( pNos < pStack ){` |
|        - |  7008 | `		goto Abort;` |
|        - |  7009 | `	}` |
|        - |  7010 | `#endif` |
|        - |  7011 | `	/* Force a boolean cast */` |
|   217130 |  7012 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  7013 | `		PH7_MemObjToBool(pTos);` |
|        1 |  7014 | `	}` |
|   217130 |  7015 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  7016 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  7017 | `	}` |
|   217130 |  7018 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   217130 |  7019 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   217130 |  7020 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  7021 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    99888 |  7022 | `		v1 = and_logic[v1*3+v2];` |
|    49967 |  7023 | `	}else{` |
|        - |  7024 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   117244 |  7025 | `		v1 = or_logic[v1*3+v2];` |
|        - |  7026 | `	}` |
|   217130 |  7027 | `	if( v1 == 2 ){` |
|      ! 0 |  7028 | `		v1 = 1;` |
|      ! 0 |  7029 | `	}` |
|   217130 |  7030 | `	VmPopOperand(&pTos,1);` |
|   217130 |  7031 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   217130 |  7032 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   217130 |  7033 | `	break;` |
|        - |  7034 | `				 }` |
|        - |  7035 | `/*` |
|        - |  7036 | ` * OP_NULLC: * * *` |
|        - |  7037 | ` * Null coalescing operator '??'.` |
|        - |  7038 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - |  7039 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - |  7040 | ` */` |
|        - |  7041 | `/*` |
|        - |  7042 | ` * OP_NULLC: * P2 *` |
|        - |  7043 | ` * Short-circuit null coalescing '??'.` |
|        - |  7044 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - |  7045 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - |  7046 | ` */` |
|       99 |  7047 | `case PH7_OP_NULLC: {` |
|        - |  7048 | `#ifdef UNTRUST` |
|        - |  7049 | `	if( pTos < pStack ){` |
|        - |  7050 | `		goto Abort;` |
|        - |  7051 | `	}` |
|        - |  7052 | `#endif` |
|      200 |  7053 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  7054 | `		/* Left is not null — keep it and skip the RHS */` |
|      120 |  7055 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       61 |  7056 | `	}else{` |
|        - |  7057 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|       82 |  7058 | `		VmPopOperand(&pTos, 1);` |
|        - |  7059 | `	}` |
|      200 |  7060 | `	break;` |
|        - |  7061 |  |
|        - |  7062 | `/*` |
|        - |  7063 | ` * OP_NULLC_JMP: * P2 *` |
|        - |  7064 | ` * Null coalescing assignment short-circuit.` |
|        - |  7065 | ` * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).` |
|        - |  7066 | ` * If TOS IS null, fall through with TOS retained — it carries the LHS's` |
|        - |  7067 | ` * nIdx so the upcoming NULLC_STORE can write back into the variable slot.` |
|        - |  7068 | ` */` |
|       28 |  7069 | `case PH7_OP_NULLC_JMP: {` |
|        - |  7070 | `#ifdef UNTRUST` |
|        - |  7071 | `	if( pTos < pStack ){` |
|        - |  7072 | `		goto Abort;` |
|        - |  7073 | `	}` |
|        - |  7074 | `#endif` |
|       58 |  7075 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       22 |  7076 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       10 |  7077 | `	}` |
|       58 |  7078 | `	break;` |
|        - |  7079 |  |
|        - |  7080 | `/*` |
|        - |  7081 | ` * OP_NULLC_STORE: * * *` |
|        - |  7082 | ` * Null coalescing assignment store.` |
|        - |  7083 | ` * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],` |
|        - |  7084 | ` * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the` |
|        - |  7085 | ` * expression result.` |
|        - |  7086 | ` */` |
|        - |  7087 | `/*` |
|        - |  7088 | ` * OP_NULLSAFE_JMP: * P2 *` |
|        - |  7089 | `` * Nullsafe object operator short-circuit (PHP 8.0 `?->`).`` |
|        - |  7090 | ` * Peek TOS (the object operand): if it is null, jump to P2 leaving NULL` |
|        - |  7091 | ` * on the stack as the result of the entire containing postfix chain. If` |
|        - |  7092 | ` * non-null, fall through without modifying the stack so the following` |
|        - |  7093 | ` * PH7_OP_MEMBER can consume the object as usual.` |
|        - |  7094 | ` */` |
|       51 |  7095 | `case PH7_OP_NULLSAFE_JMP: {` |
|        - |  7096 | `#ifdef UNTRUST` |
|        - |  7097 | `	if( pTos < pStack ){` |
|        - |  7098 | `		goto Abort;` |
|        - |  7099 | `	}` |
|        - |  7100 | `#endif` |
|      104 |  7101 | `	if( (pTos->iFlags & MEMOBJ_NULL) \|\| pTos->iFlags == 0 ){` |
|        - |  7102 | `		/* Object operand is NULL (or uninitialized) — short-circuit. The` |
|        - |  7103 | `		 * NULL slot already on TOS becomes the chain's final value. */` |
|       41 |  7104 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       20 |  7105 | `	}` |
|      104 |  7106 | `	break;` |
|        - |  7107 |  |
|       17 |  7108 | `case PH7_OP_NULLC_STORE: {` |
|       36 |  7109 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7110 | `	ph7_value *pObj;` |
|        - |  7111 | `	sxu32 nIdx;` |
|        - |  7112 | `#ifdef UNTRUST` |
|        - |  7113 | `	if( pNos < pStack ){` |
|        - |  7114 | `		goto Abort;` |
|        - |  7115 | `	}` |
|        - |  7116 | `#endif` |
|        - |  7117 | `	/* ArrayAccess null-coalesce-assign target: the preceding LOAD_IDX iP2=3` |
|        - |  7118 | `	 * armed pVm with the (object, key) on a missing key. Dispatch to` |
|        - |  7119 | `	 * offsetSet instead of writing through the synthetic pNos->nIdx. */` |
|       36 |  7120 | `	if( pVm->bCoalesceArmed && pVm->pCoalesceObj ){` |
|        5 |  7121 | `		ph7_class_instance *pInst = pVm->pCoalesceObj;` |
|        5 |  7122 | `		ph7_class_method *pSet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  7123 | `			"offsetSet",sizeof("offsetSet")-1);` |
|        - |  7124 | `		ph7_value *apArg[2];` |
|        5 |  7125 | `		apArg[0] = &pVm->sCoalesceKey;` |
|        5 |  7126 | `		apArg[1] = pTos;` |
|        5 |  7127 | `		if( pSet ){` |
|        5 |  7128 | `			PH7_VmCallClassMethod(&(*pVm),pInst,pSet,0,2,apArg);` |
|        2 |  7129 | `		}` |
|        - |  7130 | `		/* Leave RHS as the expression result (replace pNos with pTos). */` |
|        5 |  7131 | `		PH7_MemObjStore(pTos,pNos);` |
|        5 |  7132 | `		VmPopOperand(&pTos,1);` |
|        - |  7133 | `		/* Disarm and release the cached instance ref + key. */` |
|        5 |  7134 | `		VmCoalesceDisarm(pVm);` |
|        5 |  7135 | `		break;` |
|        - |  7136 | `	}` |
|       32 |  7137 | `	nIdx = pNos->nIdx;` |
|       32 |  7138 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  7139 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7140 | `			"Cannot perform assignment on a constant class attribute");` |
|       32 |  7141 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|       32 |  7142 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|       32 |  7143 | `		PH7_MemObjStore(pTos,pObj);` |
|       15 |  7144 | `	}` |
|       32 |  7145 | `	PH7_MemObjStore(pTos,pNos);` |
|       32 |  7146 | `	VmPopOperand(&pTos,1);` |
|       32 |  7147 | `	break;` |
|        - |  7148 |  |
|        - |  7149 | `/*` |
|        - |  7150 | ` * OP_SPREAD: * * *` |
|        - |  7151 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - |  7152 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - |  7153 | ` * Accumulates the net stack growth in pVm->iSpreadExtra so the next CALL` |
|        - |  7154 | ` * can adjust its argument count (the CALL may not be the next instruction).` |
|        - |  7155 | ` */` |
|        9 |  7156 | `case PH7_OP_SPREAD: {` |
|        - |  7157 | `#ifdef UNTRUST` |
|        - |  7158 | `	if( pTos < pStack ){` |
|        - |  7159 | `		goto Abort;` |
|        - |  7160 | `	}` |
|        - |  7161 | `#endif` |
|       20 |  7162 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       20 |  7163 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       20 |  7164 | `		sxu32 nEntry = pMap->nEntry;` |
|       20 |  7165 | `		if( nEntry == 0 ){` |
|        - |  7166 | `			/* Empty array — remove from stack */` |
|        3 |  7167 | `			VmPopOperand(&pTos, 1);` |
|        3 |  7168 | `			pVm->iSpreadExtra--; /* One expression produced zero args */` |
|       19 |  7169 | `		}else if( pVm->iSpreadExtra + (sxi32)(nEntry - 1) >= VM_STACK_GUARD ){` |
|        - |  7170 | `			/* Safety: refuse to expand beyond the stack guard margin */` |
|      ! 0 |  7171 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - |  7172 | `				"Argument unpacking: cumulative expansion exceeds stack guard (%d)",` |
|        - |  7173 | `				VM_STACK_GUARD);` |
|      ! 0 |  7174 | `		}else{` |
|        - |  7175 | `			ph7_hashmap_node *pNode2;` |
|        - |  7176 | `			ph7_value *pElem;` |
|        - |  7177 | `			sxu32 i;` |
|        - |  7178 | `			/* Overwrite TOS with first element */` |
|       18 |  7179 | `			pNode2 = pMap->pFirst;` |
|       18 |  7180 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       18 |  7181 | `			PH7_MemObjRelease(pTos);` |
|       18 |  7182 | `			if( pElem ){` |
|       18 |  7183 | `				PH7_MemObjLoad(pElem, pTos);` |
|        8 |  7184 | `			}` |
|       18 |  7185 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  7186 | `			/* Traverse in insertion order (pPrev is the forward link` |
|        - |  7187 | `			 * in PHL's circular doubly-linked hashmap node list). */` |
|       18 |  7188 | `			pNode2 = pNode2->pPrev;` |
|        - |  7189 | `			/* Push remaining elements */` |
|       44 |  7190 | `			for( i = 1; i < nEntry; i++ ){` |
|       28 |  7191 | `				pTos++;` |
|       28 |  7192 | `				PH7_MemObjInit(pVm, pTos);` |
|       28 |  7193 | `				pTos->nIdx = SXU32_HIGH;` |
|       28 |  7194 | `				pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       28 |  7195 | `				if( pElem ){` |
|       28 |  7196 | `					PH7_MemObjLoad(pElem, pTos);` |
|       13 |  7197 | `				}` |
|       28 |  7198 | `				pNode2 = pNode2->pPrev;` |
|       15 |  7199 | `			}` |
|       18 |  7200 | `			pVm->iSpreadExtra += (sxi32)(nEntry - 1);` |
|        - |  7201 | `		}` |
|        9 |  7202 | `	}` |
|        - |  7203 | `	/* else: not an array — leave as-is (single arg) */` |
|       20 |  7204 | `	break;` |
|        - |  7205 |  |
|        - |  7206 | `/*` |
|        - |  7207 | ` * OP_FLAG_SPREAD: * * *` |
|        - |  7208 | ` * Mark the value at TOS as a spread source for the next LOAD_MAP.` |
|        - |  7209 | ` * Used by array literal unpacking '[...$arr]'.` |
|        - |  7210 | ` */` |
|       34 |  7211 | `case PH7_OP_FLAG_SPREAD: {` |
|        - |  7212 | `#ifdef UNTRUST` |
|        - |  7213 | `	if( pTos < pStack ){` |
|        - |  7214 | `		goto Abort;` |
|        - |  7215 | `	}` |
|        - |  7216 | `#endif` |
|       70 |  7217 | `	pTos->iFlags \|= MEMOBJ_AUX_SPREAD;` |
|       70 |  7218 | `	break;` |
|        - |  7219 |  |
|        - |  7220 | `/* OP_LXOR: * * *` |
|        - |  7221 | ` *` |
|        - |  7222 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  7223 | ` * two values and push the resulting boolean value back onto the` |
|        - |  7224 | ` * stack.` |
|        - |  7225 | ` * According to the PHP language reference manual:` |
|        - |  7226 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  7227 | ` *  TRUE,but not both.` |
|        - |  7228 | ` */` |
|        5 |  7229 | `case PH7_OP_LXOR:{` |
|       11 |  7230 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  7231 | `	sxi32 v = 0;` |
|        - |  7232 | `#ifdef UNTRUST` |
|        - |  7233 | `	if( pNos < pStack ){` |
|        - |  7234 | `		goto Abort;` |
|        - |  7235 | `	}` |
|        - |  7236 | `#endif` |
|        - |  7237 | `	/* Force a boolean cast */` |
|       11 |  7238 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  7239 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  7240 | `	}` |
|       11 |  7241 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  7242 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  7243 | `	}` |
|       11 |  7244 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  7245 | `		v = 1;` |
|        3 |  7246 | `	}` |
|       11 |  7247 | `	VmPopOperand(&pTos,1);` |
|       11 |  7248 | `	pTos->x.iVal = v;` |
|       11 |  7249 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  7250 | `	break;` |
|        - |  7251 | `				 }` |
|        - |  7252 | `/* OP_EQ P1 P2 P3` |
|        - |  7253 | ` *` |
|        - |  7254 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  7255 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  7256 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7257 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7258 | ` */` |
|        - |  7259 | `/* OP_NEQ P1 P2 P3` |
|        - |  7260 | ` *` |
|        - |  7261 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  7262 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  7263 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7264 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7265 | ` */` |
|     4597 |  7266 | `case PH7_OP_EQ:` |
|        - |  7267 | `case PH7_OP_NEQ: {` |
|     9196 |  7268 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7269 | `	/* Perform the comparison and act accordingly */` |
|        - |  7270 | `#ifdef UNTRUST` |
|        - |  7271 | `	if( pNos < pStack ){` |
|        - |  7272 | `		goto Abort;` |
|        - |  7273 | `	}` |
|        - |  7274 | `#endif` |
|     9196 |  7275 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     9196 |  7276 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       19 |  7277 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     9187 |  7278 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     9152 |  7279 | `		rc = rc == 0;` |
|     4577 |  7280 | `	}else{` |
|       28 |  7281 | `		rc = rc != 0;` |
|        - |  7282 | `	}` |
|     9196 |  7283 | `	VmPopOperand(&pTos,1);` |
|     9196 |  7284 | `	if( !pInstr->iP2 ){` |
|        - |  7285 | `		/* Push comparison result without taking the jump */` |
|     9196 |  7286 | `		PH7_MemObjRelease(pTos);` |
|     9196 |  7287 | `		pTos->x.iVal = rc;` |
|        - |  7288 | `		/* Invalidate any prior representation */` |
|     9196 |  7289 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     4599 |  7290 | `	}else{` |
|      ! 0 |  7291 | `		if( rc ){` |
|        - |  7292 | `			/* Jump to the desired location */` |
|      ! 0 |  7293 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7294 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7295 | `		}` |
|        - |  7296 | `	}` |
|     9196 |  7297 | `	break;` |
|        - |  7298 | `				 }` |
|        - |  7299 | `/* OP_TEQ P1 P2 *` |
|        - |  7300 | ` *` |
|        - |  7301 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  7302 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  7303 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7304 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7305 | ` */` |
|   162165 |  7306 | `case PH7_OP_TEQ: {` |
|   324332 |  7307 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7308 | `	/* Perform the comparison and act accordingly */` |
|        - |  7309 | `#ifdef UNTRUST` |
|        - |  7310 | `	if( pNos < pStack ){` |
|        - |  7311 | `		goto Abort;` |
|        - |  7312 | `	}` |
|        - |  7313 | `#endif` |
|   324332 |  7314 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   324332 |  7315 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  7316 | `		rc = 0;` |
|        2 |  7317 | `	}else{` |
|   324330 |  7318 | `		rc = rc == 0;` |
|        - |  7319 | `	}` |
|   324332 |  7320 | `	VmPopOperand(&pTos,1);` |
|   324332 |  7321 | `	if( !pInstr->iP2 ){` |
|        - |  7322 | `		/* Push comparison result without taking the jump */` |
|   324332 |  7323 | `		PH7_MemObjRelease(pTos);` |
|   324332 |  7324 | `		pTos->x.iVal = rc;` |
|        - |  7325 | `		/* Invalidate any prior representation */` |
|   324332 |  7326 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   162167 |  7327 | `	}else{` |
|      ! 0 |  7328 | `		if( rc ){` |
|        - |  7329 | `			/* Jump to the desired location */` |
|      ! 0 |  7330 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7331 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7332 | `		}` |
|        - |  7333 | `	}` |
|   324332 |  7334 | `	break;` |
|        - |  7335 | `				 }` |
|        - |  7336 | `/* OP_TNE P1 P2 *` |
|        - |  7337 | ` *` |
|        - |  7338 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  7339 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  7340 | ` * instruction.` |
|        - |  7341 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7342 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7343 | ` *` |
|        - |  7344 | ` */` |
|   124723 |  7345 | `case PH7_OP_TNE: {` |
|   249448 |  7346 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7347 | `	/* Perform the comparison and act accordingly */` |
|        - |  7348 | `#ifdef UNTRUST` |
|        - |  7349 | `	if( pNos < pStack ){` |
|        - |  7350 | `		goto Abort;` |
|        - |  7351 | `	}` |
|        - |  7352 | `#endif` |
|   249448 |  7353 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   249448 |  7354 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  7355 | `		rc = 1;` |
|        2 |  7356 | `	}else{` |
|   249446 |  7357 | `		rc = rc != 0;` |
|        - |  7358 | `	}` |
|   249448 |  7359 | `	VmPopOperand(&pTos,1);` |
|   249448 |  7360 | `	if( !pInstr->iP2 ){` |
|        - |  7361 | `		/* Push comparison result without taking the jump */` |
|   249448 |  7362 | `		PH7_MemObjRelease(pTos);` |
|   249448 |  7363 | `		pTos->x.iVal = rc;` |
|        - |  7364 | `		/* Invalidate any prior representation */` |
|   249448 |  7365 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   124725 |  7366 | `	}else{` |
|      ! 0 |  7367 | `		if( rc ){` |
|        - |  7368 | `			/* Jump to the desired location */` |
|      ! 0 |  7369 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7370 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7371 | `		}` |
|        - |  7372 | `	}` |
|   249448 |  7373 | `	break;` |
|        - |  7374 | `				 }` |
|        - |  7375 | `/* OP_LT P1 P2 P3` |
|        - |  7376 | ` *` |
|        - |  7377 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  7378 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  7379 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  7380 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7381 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7382 | ` *` |
|        - |  7383 | ` */` |
|        - |  7384 | `/* OP_LE P1 P2 P3` |
|        - |  7385 | ` *` |
|        - |  7386 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  7387 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  7388 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  7389 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7390 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7391 | ` *` |
|        - |  7392 | ` */` |
|   112608 |  7393 | `case PH7_OP_LT:` |
|        - |  7394 | `case PH7_OP_LE: {` |
|   225262 |  7395 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7396 | `	/* Perform the comparison and act accordingly */` |
|        - |  7397 | `#ifdef UNTRUST` |
|        - |  7398 | `	if( pNos < pStack ){` |
|        - |  7399 | `		goto Abort;` |
|        - |  7400 | `	}` |
|        - |  7401 | `#endif` |
|   225262 |  7402 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   225262 |  7403 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  7404 | `		rc = 0;` |
|   225258 |  7405 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|     1608 |  7406 | `		rc = rc < 1;` |
|      805 |  7407 | `	}else{` |
|   223648 |  7408 | `		rc = rc < 0;` |
|        - |  7409 | `	}` |
|   225262 |  7410 | `	VmPopOperand(&pTos,1);` |
|   225262 |  7411 | `	if( !pInstr->iP2 ){` |
|        - |  7412 | `		/* Push comparison result without taking the jump */` |
|   225262 |  7413 | `		PH7_MemObjRelease(pTos);` |
|   225262 |  7414 | `		pTos->x.iVal = rc;` |
|        - |  7415 | `		/* Invalidate any prior representation */` |
|   225262 |  7416 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   112654 |  7417 | `	}else{` |
|      ! 0 |  7418 | `		if( rc ){` |
|        - |  7419 | `			/* Jump to the desired location */` |
|      ! 0 |  7420 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7421 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7422 | `		}` |
|        - |  7423 | `	}` |
|   225262 |  7424 | `	break;` |
|        - |  7425 | `				}` |
|        - |  7426 | `/* OP_GT P1 P2 P3` |
|        - |  7427 | ` *` |
|        - |  7428 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  7429 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  7430 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  7431 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7432 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7433 | ` *` |
|        - |  7434 | ` */` |
|        - |  7435 | `/* OP_GE P1 P2 P3` |
|        - |  7436 | ` *` |
|        - |  7437 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  7438 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  7439 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  7440 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7441 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7442 | ` *` |
|        - |  7443 | ` */` |
|    55701 |  7444 | `case PH7_OP_GT:` |
|        - |  7445 | `case PH7_OP_GE: {` |
|   111404 |  7446 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7447 | `	/* Perform the comparison and act accordingly */` |
|        - |  7448 | `#ifdef UNTRUST` |
|        - |  7449 | `	if( pNos < pStack ){` |
|        - |  7450 | `		goto Abort;` |
|        - |  7451 | `	}` |
|        - |  7452 | `#endif` |
|   111404 |  7453 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   111404 |  7454 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  7455 | `		rc = 0;` |
|   111400 |  7456 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|   110968 |  7457 | `		rc = rc >= 0;` |
|    55485 |  7458 | `	}else{` |
|      430 |  7459 | `		rc = rc > 0;` |
|        - |  7460 | `	}` |
|   111404 |  7461 | `	VmPopOperand(&pTos,1);` |
|   111404 |  7462 | `	if( !pInstr->iP2 ){` |
|        - |  7463 | `		/* Push comparison result without taking the jump */` |
|   111404 |  7464 | `		PH7_MemObjRelease(pTos);` |
|   111404 |  7465 | `		pTos->x.iVal = rc;` |
|        - |  7466 | `		/* Invalidate any prior representation */` |
|   111404 |  7467 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    55703 |  7468 | `	}else{` |
|      ! 0 |  7469 | `		if( rc ){` |
|        - |  7470 | `			/* Jump to the desired location */` |
|      ! 0 |  7471 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7472 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7473 | `		}` |
|        - |  7474 | `	}` |
|   111404 |  7475 | `	break;` |
|        - |  7476 | `				}` |
|        - |  7477 | `/* OP_SPACESHIP * * *` |
|        - |  7478 | ` *` |
|        - |  7479 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - |  7480 | ` *   -1 if left < right` |
|        - |  7481 | ` *    0 if left == right` |
|        - |  7482 | ` *    1 if left > right` |
|        - |  7483 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - |  7484 | ` */` |
|       25 |  7485 | `case PH7_OP_SPACESHIP: {` |
|       51 |  7486 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7487 | `#ifdef UNTRUST` |
|        - |  7488 | `	if( pNos < pStack ){` |
|        - |  7489 | `		goto Abort;` |
|        - |  7490 | `	}` |
|        - |  7491 | `#endif` |
|       51 |  7492 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|       51 |  7493 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        - |  7494 | `		/* NaN involved: PHP returns 1 for all NaN spaceship comparisons */` |
|        7 |  7495 | `		rc = 1;` |
|        4 |  7496 | `	}else{` |
|        - |  7497 | `		/* Normalize to exactly -1, 0, or 1 */` |
|       45 |  7498 | `		rc = (rc > 0) - (rc < 0);` |
|        - |  7499 | `	}` |
|       51 |  7500 | `	VmPopOperand(&pTos,1);` |
|       51 |  7501 | `	PH7_MemObjRelease(pTos);` |
|       51 |  7502 | `	pTos->x.iVal = rc;` |
|       51 |  7503 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       51 |  7504 | `	break;` |
|        - |  7505 | `				}` |
|        - |  7506 | `/* OP_SEQ P1 P2 *` |
|        - |  7507 | ` * Strict string comparison.` |
|        - |  7508 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  7509 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  7510 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  7511 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  7512 | ` * use PH7_OP_EQ.` |
|        - |  7513 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7514 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7515 | ` */` |
|        - |  7516 | `/* OP_SNE P1 P2 *` |
|        - |  7517 | ` * Strict string comparison.` |
|        - |  7518 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  7519 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  7520 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  7521 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  7522 | ` * use PH7_OP_EQ.` |
|        - |  7523 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7524 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7525 | ` */` |
|       18 |  7526 | `case PH7_OP_SEQ:` |
|        - |  7527 | `case PH7_OP_SNE: {` |
|       38 |  7528 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7529 | `	SyString s1,s2;` |
|        - |  7530 | `	/* Perform the comparison and act accordingly */` |
|        - |  7531 | `#ifdef UNTRUST` |
|        - |  7532 | `	if( pNos < pStack ){` |
|        - |  7533 | `		goto Abort;` |
|        - |  7534 | `	}` |
|        - |  7535 | `#endif` |
|        - |  7536 | `	/* Force a string cast */` |
|       38 |  7537 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  7538 | `		PH7_MemObjToString(pTos);` |
|        2 |  7539 | `	}` |
|       38 |  7540 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  7541 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  7542 | `	}` |
|       38 |  7543 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  7544 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  7545 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  7546 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  7547 | `		rc = rc != 0;` |
|      ! 0 |  7548 | `	}else{` |
|       38 |  7549 | `		rc = rc == 0;` |
|        - |  7550 | `	}` |
|       38 |  7551 | `	VmPopOperand(&pTos,1);` |
|       38 |  7552 | `	if( !pInstr->iP2 ){` |
|        - |  7553 | `		/* Push comparison result without taking the jump */` |
|       38 |  7554 | `		PH7_MemObjRelease(pTos);` |
|       38 |  7555 | `		pTos->x.iVal = rc;` |
|        - |  7556 | `		/* Invalidate any prior representation */` |
|       38 |  7557 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  7558 | `	}else{` |
|      ! 0 |  7559 | `		if( rc ){` |
|        - |  7560 | `			/* Jump to the desired location */` |
|      ! 0 |  7561 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7562 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7563 | `		}` |
|        - |  7564 | `	}` |
|       38 |  7565 | `	break;` |
|        - |  7566 | `				 }` |
|        - |  7567 | `/*` |
|        - |  7568 | ` * OP_LOAD_REF * * *` |
|        - |  7569 | ` * Push the index of a referenced object on the stack.` |
|        - |  7570 | ` */` |
|       60 |  7571 | `case PH7_OP_LOAD_REF: {` |
|        - |  7572 | `	sxu32 nIdx;` |
|        - |  7573 | `#ifdef UNTRUST` |
|        - |  7574 | `	if( pTos < pStack ){` |
|        - |  7575 | `		goto Abort;` |
|        - |  7576 | `	}` |
|        - |  7577 | `#endif` |
|        - |  7578 | `	/* Extract memory object index */` |
|      121 |  7579 | `	nIdx = pTos->nIdx;` |
|      121 |  7580 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  7581 | `		/* Nullify the object */` |
|      101 |  7582 | `		PH7_MemObjRelease(pTos);` |
|        - |  7583 | `		/* Mark as constant and store the index on the top of the stack */` |
|      101 |  7584 | `		pTos->x.iVal = (sxi64)nIdx;` |
|      101 |  7585 | `		pTos->nIdx = SXU32_HIGH;` |
|      101 |  7586 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       50 |  7587 | `	}` |
|      121 |  7588 | `	break;` |
|        - |  7589 | `					  }` |
|        - |  7590 | `/*` |
|        - |  7591 | ` * OP_STORE_REF * * P3` |
|        - |  7592 | ` * Perform an assignment operation by reference.` |
|        - |  7593 | ` */` |
|       18 |  7594 | ` case PH7_OP_STORE_REF: {` |
|       38 |  7595 | `	 SyString sName = { 0 , 0 };` |
|        - |  7596 | `	 VmFrame *pFrameLocal;` |
|        - |  7597 | `	SyHashEntry *pEntry;` |
|        - |  7598 | `	sxu32 nIdx;` |
|        - |  7599 | `#ifdef UNTRUST` |
|        - |  7600 | `	if( pTos < pStack ){` |
|        - |  7601 | `		goto Abort;` |
|        - |  7602 | `	}` |
|        - |  7603 | `#endif` |
|       38 |  7604 | `	if( pInstr->p3 == 0 ){` |
|        - |  7605 | `		char *zName;` |
|        - |  7606 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  7607 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7608 | `			/* Force a string cast */` |
|      ! 0 |  7609 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  7610 | `		}` |
|      ! 0 |  7611 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  7612 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  7613 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7614 | `			if( zName ){` |
|      ! 0 |  7615 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7616 | `			}` |
|      ! 0 |  7617 | `		}` |
|      ! 0 |  7618 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  7619 | `		pTos--;` |
|      ! 0 |  7620 | `	}else{` |
|       38 |  7621 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  7622 | `	}` |
|       38 |  7623 | `	nIdx = pTos->nIdx;` |
|       38 |  7624 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  7625 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  7626 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7627 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  7628 | `		}else{` |
|        - |  7629 | `			ph7_value *pObj;` |
|        - |  7630 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  7631 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  7632 | `			if( pObj == 0 ){` |
|      ! 0 |  7633 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7634 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  7635 | `				goto Abort;` |
|        - |  7636 | `			}` |
|        - |  7637 | `			/* Perform the store operation */` |
|      ! 0 |  7638 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  7639 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  7640 | `		}` |
|       38 |  7641 | `	}else if( sName.nByte > 0){` |
|       38 |  7642 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  7643 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  7644 | `		}else{` |
|       38 |  7645 | `			pFrameLocal = pVm->pFrame;` |
|       38 |  7646 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  7647 | `			/* Query the local frame */` |
|       38 |  7648 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       38 |  7649 | `			if( pEntry ){` |
|      ! 0 |  7650 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  7651 | `			}else{` |
|       38 |  7652 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       38 |  7653 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  7654 | `					/* Insert in the $GLOBALS array */` |
|       34 |  7655 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       16 |  7656 | `				}` |
|       38 |  7657 | `				if( rc == SXRET_OK ){` |
|       38 |  7658 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       18 |  7659 | `				}` |
|        - |  7660 | `			}` |
|        - |  7661 | `		}` |
|       18 |  7662 | `	}` |
|       38 |  7663 | `	break;` |
|        - |  7664 | `				 }` |
|        - |  7665 | `/*` |
|        - |  7666 | ` * OP_UPLINK P1 * *` |
|        - |  7667 | ` * Link a variable to the top active VM frame.` |
|        - |  7668 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  7669 | ` */` |
|       30 |  7670 | `case PH7_OP_UPLINK: {` |
|       62 |  7671 | `	if( pVm->pFrame->pParent ){` |
|       62 |  7672 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  7673 | `		SyString sName;` |
|        - |  7674 | `		/* Perform the link */` |
|      132 |  7675 | `		while( pLink <= pTos ){` |
|       72 |  7676 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7677 | `				/* Force a string cast */` |
|      ! 0 |  7678 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  7679 | `			}` |
|       72 |  7680 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       72 |  7681 | `			if( sName.nByte > 0 ){` |
|       72 |  7682 | `				VmFrameLink(&(*pVm),&sName);` |
|       35 |  7683 | `			}` |
|       72 |  7684 | `			pLink++;` |
|        2 |  7685 | `		}` |
|       30 |  7686 | `	}` |
|       62 |  7687 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       62 |  7688 | `	break;` |
|        - |  7689 | `					}` |
|        - |  7690 | `/*` |
|        - |  7691 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  7692 | ` * Push an exception in the corresponding container so that` |
|        - |  7693 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  7694 | ` */` |
|      191 |  7695 | `case PH7_OP_LOAD_EXCEPTION: {` |
|      384 |  7696 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  7697 | `	VmFrame *pFrameLocal;` |
|        - |  7698 | `	/* Reset per-entry state so finally runs on each iteration */` |
|      384 |  7699 | `	pException->iFinallyDone = 0;` |
|      384 |  7700 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  7701 | `	/* Create the exception frame */` |
|      384 |  7702 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|      384 |  7703 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7704 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  7705 | `		goto Abort;` |
|        - |  7706 | `	}` |
|        - |  7707 | `	/* Mark the special frame */` |
|      384 |  7708 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|      384 |  7709 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  7710 | `	/* Point to the frame that trigger the exception */` |
|      384 |  7711 | `	pFrameLocal = pFrameLocal->pParent;` |
|      384 |  7712 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      384 |  7713 | `	pException->pFrame = pFrameLocal;` |
|      384 |  7714 | `	break;` |
|        - |  7715 | `							}` |
|        - |  7716 | `/*` |
|        - |  7717 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  7718 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  7719 | ` */` |
|      190 |  7720 | `case PH7_OP_POP_EXCEPTION: {` |
|      382 |  7721 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|      382 |  7722 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  7723 | `		ph7_exception **apException;` |
|        - |  7724 | `		/* Pop the loaded exception */` |
|       32 |  7725 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       32 |  7726 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       30 |  7727 | `			(void)SySetPop(&pVm->aException);` |
|       14 |  7728 | `		}` |
|       15 |  7729 | `	}` |
|      382 |  7730 | `	pException->pFrame = 0;` |
|        - |  7731 | `	/* Leave the exception frame */` |
|      382 |  7732 | `	VmLeaveFrame(&(*pVm));` |
|        - |  7733 | `	/* Execute the finally block if present and not already executed by catch path */` |
|      382 |  7734 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  7735 | `		sxi32 rcFinally;` |
|       20 |  7736 | `		pException->iFinallyDone = 1;` |
|       20 |  7737 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       20 |  7738 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  7739 | `			goto Abort;` |
|        - |  7740 | `		}` |
|        9 |  7741 | `	}` |
|      382 |  7742 | `	break;` |
|        - |  7743 | `							}` |
|        - |  7744 |  |
|        - |  7745 | `/*` |
|        - |  7746 | ` * OP_THROW * P2 *` |
|        - |  7747 | ` * Throw an user exception.` |
|        - |  7748 | ` */` |
|       78 |  7749 | `case PH7_OP_THROW: {` |
|      158 |  7750 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|      158 |  7751 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  7752 | `#ifdef UNTRUST` |
|        - |  7753 | `	if( pTos < pStack ){` |
|        - |  7754 | `		goto Abort;` |
|        - |  7755 | `	}` |
|        - |  7756 | `#endif` |
|      158 |  7757 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  7758 | `	/* Tell the upper layer that an exception was thrown */` |
|      158 |  7759 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|      158 |  7760 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|      158 |  7761 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7762 | `		ph7_class *pThrowable;` |
|        - |  7763 | `		/* Thrown object must implement the Throwable interface (PHP 7+). */` |
|      158 |  7764 | `		pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|      159 |  7765 | `		if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|        - |  7766 | `			/* Not a Throwable: replace with Error(msg) matching PHP behavior.` |
|        - |  7767 | `			 * Error::__construct is defined in the built-in library and` |
|        - |  7768 | `			 * cannot realistically fail, so we do not check its return. */` |
|        3 |  7769 | `			ph7_class *pErrorClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        3 |  7770 | `			ph7_class_instance *pErrInst = 0;` |
|        3 |  7771 | `			if( pErrorClass ){` |
|        3 |  7772 | `				pErrInst = PH7_NewClassInstance(&(*pVm),pErrorClass);` |
|        1 |  7773 | `			}` |
|        3 |  7774 | `			if( pErrInst ){` |
|        - |  7775 | `				ph7_class_method *pCons;` |
|        3 |  7776 | `				pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|        3 |  7777 | `				if( pCons ){` |
|        - |  7778 | `					ph7_value sArg;` |
|        - |  7779 | `					ph7_value *apArg[1];` |
|        - |  7780 | `					SyString sMsgStr;` |
|        - |  7781 | `					static const char zErrMsg[] =` |
|        - |  7782 | `						"Cannot throw objects that do not implement Throwable";` |
|        3 |  7783 | `					SyStringInitFromBuf(&sMsgStr,zErrMsg,sizeof(zErrMsg)-1);` |
|        3 |  7784 | `					PH7_MemObjInit(pVm,&sArg);` |
|        3 |  7785 | `					PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|        3 |  7786 | `					apArg[0] = &sArg;` |
|        3 |  7787 | `					PH7_VmCallClassMethod(&(*pVm),pErrInst,pCons,0,1,apArg);` |
|        3 |  7788 | `					PH7_MemObjRelease(&sArg);` |
|        1 |  7789 | `				}` |
|        3 |  7790 | `				rc = VmThrowException(&(*pVm),pErrInst);` |
|        3 |  7791 | `				PH7_ClassInstanceUnref(pErrInst);` |
|        3 |  7792 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7793 | `					goto Abort;` |
|        - |  7794 | `				}` |
|        2 |  7795 | `			}else{` |
|        - |  7796 | `				/* Bootstrap failure — fall back to uncaught reporting */` |
|      ! 0 |  7797 | `				rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  7798 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7799 | `					goto Abort;` |
|        - |  7800 | `				}` |
|        - |  7801 | `			}` |
|        2 |  7802 | `		}else{` |
|        - |  7803 | `			/* Throw the exception */` |
|      156 |  7804 | `			rc = VmThrowException(&(*pVm),pThis);` |
|      156 |  7805 | `			if( rc == SXERR_ABORT ){` |
|        - |  7806 | `				/* Abort processing immediately */` |
|       11 |  7807 | `				goto Abort;` |
|        - |  7808 | `			}` |
|        - |  7809 | `		}` |
|       75 |  7810 | `	}else{` |
|        - |  7811 | `		/* Expecting a class instance */` |
|      ! 0 |  7812 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  7813 | `		if( rc == SXERR_ABORT ){` |
|        - |  7814 | `			/* Abort processing immediately */` |
|      ! 0 |  7815 | `			goto Abort;` |
|        - |  7816 | `		}` |
|        - |  7817 | `	}` |
|        - |  7818 | `	/* Pop the top entry */` |
|      148 |  7819 | `	VmPopOperand(&pTos,1);` |
|        - |  7820 | `	/* Perform an unconditional jump */` |
|      148 |  7821 | `	pc = nJump - 1;` |
|      148 |  7822 | `	break;` |
|        - |  7823 | `				   }` |
|        - |  7824 | `/*` |
|        - |  7825 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  7826 | ` * Prepare a foreach step.` |
|        - |  7827 | ` */` |
|     6207 |  7828 | `case PH7_OP_FOREACH_INIT: {` |
|    12416 |  7829 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  7830 | `	void *pName;` |
|        - |  7831 | `#ifdef UNTRUST` |
|        - |  7832 | `	if( pTos < pStack ){` |
|        - |  7833 | `		goto Abort;` |
|        - |  7834 | `	}` |
|        - |  7835 | `#endif` |
|    12416 |  7836 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  7837 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  7838 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7839 | `			/* Force a string cast */` |
|      ! 0 |  7840 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  7841 | `		}` |
|        - |  7842 | `		/* Duplicate name */` |
|      ! 0 |  7843 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  7844 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7845 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7846 | `		}` |
|      ! 0 |  7847 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  7848 | `	}` |
|    12416 |  7849 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  7850 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7851 | `			/* Force a string cast */` |
|      ! 0 |  7852 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  7853 | `		}` |
|        - |  7854 | `		/* Duplicate name */` |
|      ! 0 |  7855 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  7856 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7857 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7858 | `		}` |
|      ! 0 |  7859 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  7860 | `	}` |
|        - |  7861 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|    12416 |  7862 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  7863 | `		/* Jump out of the loop */` |
|      ! 0 |  7864 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  7865 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  7866 | `		}` |
|      ! 0 |  7867 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  7868 | `	}else{` |
|        - |  7869 | `		ph7_foreach_step *pStep;` |
|    12416 |  7870 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|    12416 |  7871 | `		if( pStep == 0 ){` |
|      ! 0 |  7872 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  7873 | `			/* Jump out of the loop */` |
|      ! 0 |  7874 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7875 | `		}else{` |
|        - |  7876 | `			/* Zero the structure */` |
|    12416 |  7877 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  7878 | `			/* Prepare the step */` |
|    12416 |  7879 | `			pStep->iFlags = pInfo->iFlags;` |
|    12416 |  7880 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  7881 | `				ph7_hashmap *pMap;` |
|        - |  7882 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  7883 | `				 * source array so mutations don't affect other sharers. */` |
|    12382 |  7884 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|        9 |  7885 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|        9 |  7886 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|        9 |  7887 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  7888 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  7889 | `						 * variable still points at the same hashmap as` |
|        - |  7890 | `						 * the stack value. */` |
|        9 |  7891 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|        9 |  7892 | `							pCur->iRef--;` |
|        - |  7893 | `							/* Use the returned map, not pBacking->x.pOther: PH7_HashmapDup` |
|        - |  7894 | `							 * inside CowSeparate can reallocate (move) pVm->aMemObj and leave` |
|        - |  7895 | `							 * pBacking dangling. The return value is the post-separation map. */` |
|        9 |  7896 | `							pTos->x.pOther = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|        9 |  7897 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  7898 | `						}` |
|        4 |  7899 | `					}` |
|        4 |  7900 | `				}` |
|    12382 |  7901 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  7902 | `				/* Reset the internal loop cursor */` |
|    12382 |  7903 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  7904 | `				/* Mark the step */` |
|    12382 |  7905 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|    12382 |  7906 | `				pStep->xIter.pMap = pMap;` |
|    12382 |  7907 | `				pMap->iRef++;` |
|     6192 |  7908 | `			}else{` |
|       36 |  7909 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7910 | `				ph7_class *pIteratorClass;` |
|        - |  7911 | `				/* Check if the object implements Iterator */` |
|       36 |  7912 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       47 |  7913 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  7914 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  7915 | `					ph7_class_method *pRewind;` |
|       24 |  7916 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       24 |  7917 | `					pStep->xIter.pThis = pThis;` |
|       24 |  7918 | `					pThis->iRef++;` |
|       24 |  7919 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       24 |  7920 | `					if( pRewind ){` |
|       24 |  7921 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|       11 |  7922 | `					}` |
|       13 |  7923 | `				}else{` |
|        - |  7924 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  7925 | `					ph7_class *pIterAggClass;` |
|       14 |  7926 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  7927 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       15 |  7928 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  7929 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  7930 | `						ph7_class_method *pGetIter;` |
|        3 |  7931 | `						int iterAggOk = 0;` |
|        3 |  7932 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  7933 | `						if( pGetIter ){` |
|        - |  7934 | `							ph7_value sResult;` |
|        3 |  7935 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  7936 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  7937 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  7938 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  7939 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  7940 | `									ph7_class_method *pRewind;` |
|        3 |  7941 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  7942 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  7943 | `									pIterObj->iRef++;` |
|        - |  7944 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  7945 | `									pStep->pOwner = pThis;` |
|        3 |  7946 | `									pThis->iRef++;` |
|        3 |  7947 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  7948 | `									if( pRewind ){` |
|        3 |  7949 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  7950 | `									}` |
|        3 |  7951 | `									iterAggOk = 1;` |
|        1 |  7952 | `								}` |
|        1 |  7953 | `							}` |
|        3 |  7954 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  7955 | `						}` |
|        3 |  7956 | `						if( !iterAggOk ){` |
|        - |  7957 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  7958 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7959 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  7960 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  7961 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  7962 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  7963 | `						}` |
|        2 |  7964 | `					}else{` |
|        - |  7965 | `						/* Plain object iteration via hAttr */` |
|       12 |  7966 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|       12 |  7967 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|       12 |  7968 | `						pStep->xIter.pThis = pThis;` |
|       12 |  7969 | `						pThis->iRef++;` |
|        - |  7970 | `					}` |
|        - |  7971 | `				}` |
|        - |  7972 | `			}` |
|        - |  7973 | `		}` |
|    12416 |  7974 | `		if( pStep ){` |
|    12416 |  7975 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  7976 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  7977 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  7978 | `				/* Jump out of the loop */` |
|      ! 0 |  7979 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  7980 | `			}` |
|     6207 |  7981 | `		}` |
|        - |  7982 | `	}` |
|    12416 |  7983 | `	VmPopOperand(&pTos,1);` |
|    12416 |  7984 | `	break;` |
|        - |  7985 | `						  }` |
|        - |  7986 | `/*` |
|        - |  7987 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  7988 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  7989 | ` */` |
|   102019 |  7990 | `case PH7_OP_FOREACH_STEP: {` |
|   204040 |  7991 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  7992 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  7993 | `	ph7_value *pValue;` |
|        - |  7994 | `	VmFrame *pFrameLocal;` |
|        - |  7995 | `	/* Peek the last step */` |
|   204040 |  7996 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   204040 |  7997 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   204040 |  7998 | `	pFrameLocal = pVm->pFrame;` |
|   204040 |  7999 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   204040 |  8000 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   203906 |  8001 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  8002 | `		ph7_hashmap_node *pNode;` |
|        - |  8003 | `		/* Extract the current node value */` |
|   203906 |  8004 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   203906 |  8005 | `		if( pNode == 0 ){` |
|        - |  8006 | `			/* No more entry to process */` |
|    12380 |  8007 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|    12380 |  8008 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  8009 | `				/* Break the reference with the last element */` |
|        7 |  8010 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  8011 | `			}` |
|        - |  8012 | `			/* Automatically reset the loop cursor */` |
|    12380 |  8013 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  8014 | `			/* Cleanup the mess left behind */` |
|    12380 |  8015 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    12380 |  8016 | `			SySetPop(&pInfo->aStep);` |
|    12380 |  8017 | `			PH7_HashmapUnref(pMap);` |
|     6191 |  8018 | `		}else{` |
|   191528 |  8019 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      528 |  8020 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      528 |  8021 | `				if( pKey ){` |
|      528 |  8022 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      263 |  8023 | `				}` |
|      263 |  8024 | `			}` |
|   191528 |  8025 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  8026 | `				SyHashEntry *pEntry;` |
|        - |  8027 | `				/* Pass by reference */` |
|       23 |  8028 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       23 |  8029 | `				if( pEntry ){` |
|       21 |  8030 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       11 |  8031 | `				}else{` |
|        4 |  8032 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|        2 |  8033 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  8034 | `				}` |
|       12 |  8035 | `			}else{` |
|        - |  8036 | `				/* Make a copy of the entry value */` |
|   191506 |  8037 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   191506 |  8038 | `				if( pValue ){` |
|   191506 |  8039 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    95752 |  8040 | `				}` |
|        - |  8041 | `			}` |
|        2 |  8042 | `		}` |
|   102088 |  8043 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  8044 | `		/* Iterator-based iteration.` |
|        - |  8045 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  8046 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  8047 | `		 */` |
|      106 |  8048 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  8049 | `		ph7_class_method *pMethod;` |
|        - |  8050 | `		ph7_value sResult;` |
|      106 |  8051 | `		int isValid = 0;` |
|        - |  8052 | `		/* Call next() to advance — but skip on the first iteration */` |
|      106 |  8053 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       26 |  8054 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       14 |  8055 | `		}else{` |
|       82 |  8056 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       82 |  8057 | `			if( pMethod ){` |
|       82 |  8058 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       40 |  8059 | `			}` |
|        - |  8060 | `		}` |
|        - |  8061 | `		/* Call valid() */` |
|      106 |  8062 | `		PH7_MemObjInit(pVm,&sResult);` |
|      106 |  8063 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|      106 |  8064 | `		if( pMethod ){` |
|      106 |  8065 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|      106 |  8066 | `			PH7_MemObjToBool(&sResult);` |
|      106 |  8067 | `			isValid = (sResult.x.iVal != 0);` |
|       52 |  8068 | `		}` |
|      106 |  8069 | `		PH7_MemObjRelease(&sResult);` |
|      106 |  8070 | `		if( !isValid ){` |
|        - |  8071 | `			/* Iterator exhausted */` |
|       24 |  8072 | `			pc = pInstr->iP2 - 1;` |
|        - |  8073 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       24 |  8074 | `			if( pStep->pOwner ){` |
|        3 |  8075 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  8076 | `			}` |
|       24 |  8077 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       24 |  8078 | `			SySetPop(&pInfo->aStep);` |
|       24 |  8079 | `			PH7_ClassInstanceUnref(pThis);` |
|       13 |  8080 | `		}else{` |
|        - |  8081 | `			/* Call current() to get value */` |
|       84 |  8082 | `			PH7_MemObjInit(pVm,&sResult);` |
|       84 |  8083 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       84 |  8084 | `			if( pMethod ){` |
|       84 |  8085 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       41 |  8086 | `			}` |
|       84 |  8087 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       84 |  8088 | `			if( pValue ){` |
|       84 |  8089 | `				PH7_MemObjStore(&sResult,pValue);` |
|       41 |  8090 | `			}` |
|       84 |  8091 | `			PH7_MemObjRelease(&sResult);` |
|        - |  8092 | `			/* Call key() if needed */` |
|       84 |  8093 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  8094 | `				ph7_value sKey;` |
|       35 |  8095 | `				PH7_MemObjInit(pVm,&sKey);` |
|       35 |  8096 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       35 |  8097 | `				if( pMethod ){` |
|       35 |  8098 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  8099 | `				}` |
|       35 |  8100 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       35 |  8101 | `				if( pValue ){` |
|       35 |  8102 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  8103 | `				}` |
|       35 |  8104 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  8105 | `			}` |
|        - |  8106 | `		}` |
|       54 |  8107 | `	}else{` |
|       32 |  8108 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       32 |  8109 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  8110 | `		SyHashEntry *pEntry;` |
|        - |  8111 | `		/* Point to the next attribute */` |
|       36 |  8112 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       26 |  8113 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  8114 | `			/* Check access permission */` |
|       38 |  8115 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       24 |  8116 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       22 |  8117 | `					break; /* Access is granted */` |
|        - |  8118 | `			}` |
|        1 |  8119 | `		}` |
|       32 |  8120 | `		if( pEntry == 0 ){` |
|        - |  8121 | `			/* Clean up the mess left behind */` |
|       12 |  8122 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|       12 |  8123 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  8124 | `				/* Break the reference with the last element */` |
|        3 |  8125 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  8126 | `			}` |
|       12 |  8127 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       12 |  8128 | `			SySetPop(&pInfo->aStep);` |
|       12 |  8129 | `			PH7_ClassInstanceUnref(pThis);` |
|        7 |  8130 | `		}else{` |
|       22 |  8131 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  8132 | `			ph7_value *pAttrValue;` |
|       22 |  8133 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  8134 | `				/* Fill with the current attribute name */` |
|       22 |  8135 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       22 |  8136 | `				if( pKey ){` |
|       22 |  8137 | `					SyBlobReset(&pKey->sBlob);` |
|       22 |  8138 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       22 |  8139 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|       10 |  8140 | `				}` |
|       10 |  8141 | `			}` |
|        - |  8142 | `			/* Extract attribute value */` |
|       22 |  8143 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       22 |  8144 | `			if( pAttrValue ){` |
|       22 |  8145 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  8146 | `					/* Pass by reference */` |
|        3 |  8147 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  8148 | `					if( pEntry ){` |
|        3 |  8149 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  8150 | `					}else{` |
|      ! 0 |  8151 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  8152 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  8153 | `					}` |
|        2 |  8154 | `				}else{` |
|        - |  8155 | `					/* Make a copy of the attribute value */` |
|       20 |  8156 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       20 |  8157 | `					if( pValue ){` |
|       20 |  8158 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        9 |  8159 | `					}` |
|        - |  8160 | `				}` |
|       10 |  8161 | `			}` |
|        - |  8162 | `		}` |
|        - |  8163 | `	}` |
|   204040 |  8164 | `	break;` |
|        - |  8165 | `						  }` |
|        - |  8166 | `/*` |
|        - |  8167 | ` * OP_MEMBER P1 P2` |
|        - |  8168 | ` * Load class attribute/method on the stack.` |
|        - |  8169 | ` */` |
|     4103 |  8170 | `case PH7_OP_MEMBER: {` |
|        - |  8171 | `	ph7_class_instance *pThis;` |
|        - |  8172 | `	ph7_value *pNos;` |
|        - |  8173 | `	SyString sName;` |
|     8208 |  8174 | `	if( !pInstr->iP1 ){` |
|     7968 |  8175 | `		pNos = &pTos[-1];` |
|        - |  8176 | `#ifdef UNTRUST` |
|        - |  8177 | `		if( pNos < pStack ){` |
|        - |  8178 | `			goto Abort;` |
|        - |  8179 | `		}` |
|        - |  8180 | `#endif` |
|     7968 |  8181 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  8182 | `			ph7_class *pClass;` |
|        - |  8183 | `			/* Class already instantiated */` |
|     7966 |  8184 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  8185 | `			/* Point to the instantiated class */` |
|     7966 |  8186 | `			pClass = pThis->pClass;` |
|        - |  8187 | `			/* Extract attribute name first */` |
|     7966 |  8188 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     7966 |  8189 | `			if( pInstr->iP2 ){` |
|        - |  8190 | `				/* Method call */` |
|      790 |  8191 | `				ph7_class_method *pMeth = 0;` |
|      790 |  8192 | `				if( sName.nByte > 0 ){` |
|        - |  8193 | `					/* Extract the target method */` |
|      790 |  8194 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      394 |  8195 | `				}` |
|      790 |  8196 | `				if( pMeth == 0 ){` |
|      ! 0 |  8197 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  8198 | `						&pClass->sName,&sName` |
|        - |  8199 | `						);` |
|        - |  8200 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  8201 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  8202 | `					/* Pop the method name from the stack */` |
|      ! 0 |  8203 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  8204 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  8205 | `				}else{` |
|        - |  8206 | `					/* Push method name on the stack */` |
|      790 |  8207 | `					PH7_MemObjRelease(pTos);` |
|      790 |  8208 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      790 |  8209 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  8210 | `				}` |
|      790 |  8211 | `				pTos->nIdx = SXU32_HIGH;` |
|      396 |  8212 | `			}else{` |
|        - |  8213 | `				/* Attribute access */` |
|     7178 |  8214 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  8215 | `				SyHashEntry *pEntry;` |
|        - |  8216 | `				/* Extract the target attribute */` |
|     7178 |  8217 | `				if( sName.nByte > 0 ){` |
|     7178 |  8218 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     7178 |  8219 | `					if( pEntry ){` |
|        - |  8220 | `						/* Point to the attribute value */` |
|     7176 |  8221 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     3587 |  8222 | `					}` |
|     3588 |  8223 | `				}` |
|     7178 |  8224 | `				if( pObjAttr == 0 ){` |
|        - |  8225 | `					/* No such attribute,load null */` |
|        4 |  8226 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  8227 | `						&pClass->sName,&sName);` |
|        - |  8228 | `					/* Call the __get magic method if available */` |
|        3 |  8229 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  8230 | `				}` |
|     7178 |  8231 | `				VmPopOperand(&pTos,1);` |
|        - |  8232 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  8233 | `				 * This is due to the following case:` |
|        - |  8234 | `				 *     (new TestClass())->foo;` |
|        - |  8235 | `				 */` |
|     7178 |  8236 | `				pThis->iRef++;` |
|     7178 |  8237 | `				PH7_MemObjRelease(pTos);` |
|     7178 |  8238 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     7178 |  8239 | `				if( pObjAttr ){` |
|     7176 |  8240 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  8241 | `					/* Check attribute access */` |
|     7176 |  8242 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){` |
|        - |  8243 | `						/* PHP 7.4+: reading an uninitialized typed property is an Error.` |
|        - |  8244 | `						 * We can only raise it on a real read, not when the slot is the` |
|        - |  8245 | `						 * LHS of an assignment — peek at the next instruction to decide.` |
|        - |  8246 | `						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so` |
|        - |  8247 | `						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */` |
|     7174 |  8248 | `						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|     3629 |  8249 | `						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|       82 |  8250 | `							VmInstr *pNext = pInstr + 1;` |
|       82 |  8251 | `							int bIsLhs = 0;` |
|       82 |  8252 | `							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|       80 |  8253 | `								bIsLhs = 1;` |
|       39 |  8254 | `							}` |
|       82 |  8255 | `							if( !bIsLhs ){` |
|        3 |  8256 | `								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);` |
|        3 |  8257 | `								PH7_ClassInstanceUnref(pThis);` |
|        3 |  8258 | `								if( rcU == PH7_ABORT ){` |
|      ! 0 |  8259 | `									goto Abort;` |
|        - |  8260 | `								}` |
|        - |  8261 | `								{` |
|        3 |  8262 | `									VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  8263 | `									if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  8264 | `										pc = pFrm2->iExceptionJump - 1;` |
|     4103 |  8265 | `										break;` |
|        - |  8266 | `									}` |
|        - |  8267 | `								}` |
|      ! 0 |  8268 | `								goto Exception;` |
|        - |  8269 | `							}` |
|       39 |  8270 | `						}` |
|        - |  8271 | `						/* Load attribute */` |
|     7174 |  8272 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     7174 |  8273 | `						if( pValue ){` |
|     7174 |  8274 | `							if( pThis->iRef < 2 ){` |
|        - |  8275 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  8276 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  8277 | `								 */` |
|        7 |  8278 | `								PH7_MemObjStore(pValue,pTos);` |
|        4 |  8279 | `							}else{` |
|        - |  8280 | `								/* Simple load */` |
|     7168 |  8281 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  8282 | `							}` |
|     7174 |  8283 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     7172 |  8284 | `								if( pThis->iRef > 1 ){` |
|        - |  8285 | `									/* Load attribute index */` |
|     7166 |  8286 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     3582 |  8287 | `								}` |
|     3585 |  8288 | `							}` |
|     3586 |  8289 | `						}` |
|     3588 |  8290 | `					}else{` |
|        - |  8291 | `						/* Throw Error exception (PHP-compatible).` |
|        - |  8292 | `						 * Build message before unref — pObjAttr belongs to pThis->hAttr. */` |
|        - |  8293 | `						char zMsg[256];` |
|      ! 0 |  8294 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  8295 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  8296 | `							zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  8297 | `							(int)pObjAttr->pAttr->sName.nByte,pObjAttr->pAttr->sName.zString);` |
|      ! 0 |  8298 | `						PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8299 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  8300 | `						goto Abort;` |
|        - |  8301 | `					}` |
|     3586 |  8302 | `				}` |
|        - |  8303 | `				/* Safely unreference the object */` |
|     7176 |  8304 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  8305 | `			}` |
|     3983 |  8306 | `		}else{` |
|        3 |  8307 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|        3 |  8308 | `			VmPopOperand(&pTos,1);` |
|        3 |  8309 | `			PH7_MemObjRelease(pTos);` |
|        3 |  8310 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  8311 | `		}` |
|     3984 |  8312 | `	}else{` |
|        - |  8313 | `		/* Static member access using class name */` |
|      242 |  8314 | `		pNos = pTos;` |
|      242 |  8315 | `		pThis = 0;` |
|      242 |  8316 | `		if( !pInstr->p3 ){` |
|      192 |  8317 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      192 |  8318 | `			pNos--;` |
|        - |  8319 | `#ifdef UNTRUST` |
|        - |  8320 | `			if( pNos < pStack ){` |
|        - |  8321 | `				goto Abort;` |
|        - |  8322 | `			}` |
|        - |  8323 | `#endif` |
|       97 |  8324 | `		}else{` |
|        - |  8325 | `			/* Attribute name already computed */` |
|       52 |  8326 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  8327 | `		}` |
|      242 |  8328 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      242 |  8329 | `			ph7_class *pClass = 0;` |
|      242 |  8330 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  8331 | `				/* Class already instantiated */` |
|        5 |  8332 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        5 |  8333 | `				pClass = pThis->pClass;` |
|        5 |  8334 | `				pThis->iRef++; /* Deffer garbage collection */` |
|        3 |  8335 | `			}else{` |
|        - |  8336 | `				/* Try to extract the target class */` |
|      238 |  8337 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      238 |  8338 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      238 |  8339 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  8340 | `					/* Handle self/static/parent keywords */` |
|      238 |  8341 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       62 |  8342 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       62 |  8343 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  8344 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  8345 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  8346 | `						}` |
|      208 |  8347 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       28 |  8348 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      178 |  8349 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       28 |  8350 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       28 |  8351 | `						if( pSelf && pSelf->pBase ){` |
|       28 |  8352 | `							pClass = pSelf->pBase;` |
|       13 |  8353 | `						}` |
|       15 |  8354 | `					}else{` |
|      126 |  8355 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  8356 | `					}` |
|      118 |  8357 | `				}` |
|        - |  8358 | `			}` |
|      242 |  8359 | `			if( pClass == 0 ){` |
|        - |  8360 | `				/* Undefined class */` |
|      ! 0 |  8361 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  8362 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  8363 | `					);` |
|      ! 0 |  8364 | `				if( !pInstr->p3 ){` |
|      ! 0 |  8365 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  8366 | `				}` |
|      ! 0 |  8367 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  8368 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  8369 | `			}else{` |
|      242 |  8370 | `				if( pInstr->iP2 ){` |
|        - |  8371 | `					/* Method call */` |
|       86 |  8372 | `					ph7_class_method *pMeth = 0;` |
|       86 |  8373 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  8374 | `						/* Extract the target method */` |
|       86 |  8375 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       42 |  8376 | `					}` |
|       86 |  8377 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  8378 | `						if( pMeth ){` |
|      ! 0 |  8379 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  8380 | `								&pClass->sName,&sName` |
|        - |  8381 | `								);` |
|      ! 0 |  8382 | `						}else{` |
|      ! 0 |  8383 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  8384 | `								&pClass->sName,&sName` |
|        - |  8385 | `								);` |
|        - |  8386 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  8387 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  8388 | `						}` |
|        - |  8389 | `						/* Pop the method name from the stack */` |
|      ! 0 |  8390 | `						if( !pInstr->p3 ){` |
|      ! 0 |  8391 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  8392 | `						}` |
|      ! 0 |  8393 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  8394 | `					}else{` |
|        - |  8395 | `						/* Push method name on the stack */` |
|       86 |  8396 | `						PH7_MemObjRelease(pTos);` |
|       86 |  8397 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       86 |  8398 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  8399 | `					}` |
|       86 |  8400 | `					pTos->nIdx = SXU32_HIGH;` |
|       44 |  8401 | `				}else{` |
|        - |  8402 | `					/* Attribute access */` |
|      158 |  8403 | `					ph7_class_attr *pAttr = 0;` |
|        - |  8404 | `					/* Check for special ::class pseudo-constant */` |
|      204 |  8405 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       92 |  8406 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  8407 | `						/* ::class returns the fully qualified class name */` |
|        - |  8408 | `						/* Pop the attribute name from the stack */` |
|       60 |  8409 | `						if( !pInstr->p3 ){` |
|       60 |  8410 | `							VmPopOperand(&pTos,1);` |
|       29 |  8411 | `						}` |
|       60 |  8412 | `						PH7_MemObjRelease(pTos);` |
|        - |  8413 | `						/* Load the class name */` |
|       60 |  8414 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       60 |  8415 | `						pTos->nIdx = SXU32_HIGH;` |
|       31 |  8416 | `					}else{` |
|        - |  8417 | `						/* Extract the target attribute */` |
|      100 |  8418 | `						if( sName.nByte > 0 ){` |
|      100 |  8419 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|       49 |  8420 | `						}` |
|      100 |  8421 | `						if( pAttr == 0 ){` |
|        - |  8422 | `							/* No such attribute,load null */` |
|      ! 0 |  8423 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  8424 | `								&pClass->sName,&sName);` |
|        - |  8425 | `							/* Call the __get magic method if available */` |
|      ! 0 |  8426 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  8427 | `						}` |
|        - |  8428 | `						/* Pop the attribute name from the stack */` |
|      100 |  8429 | `						if( !pInstr->p3 ){` |
|       50 |  8430 | `							VmPopOperand(&pTos,1);` |
|       24 |  8431 | `						}` |
|      100 |  8432 | `						PH7_MemObjRelease(pTos);` |
|      100 |  8433 | `						pTos->nIdx = SXU32_HIGH;` |
|      100 |  8434 | `						if( pAttr ){` |
|      100 |  8435 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  8436 | `								/* Access to a non static attribute */` |
|      ! 0 |  8437 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  8438 | `									&pClass->sName,&pAttr->sName` |
|        - |  8439 | `									);` |
|      ! 0 |  8440 | `							}else{` |
|        - |  8441 | `								ph7_value *pValue;` |
|        - |  8442 | `								/* Check if the access to the attribute is allowed */` |
|      100 |  8443 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        - |  8444 | `									/* PHP 7.4+: uninitialized typed static read.` |
|        - |  8445 | `									 * Same LHS-of-store peek as the instance path. */` |
|       94 |  8446 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|       68 |  8447 | `									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       59 |  8448 | `										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,` |
|       38 |  8449 | `											(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|       40 |  8450 | `										if( pS ){` |
|       40 |  8451 | `											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;` |
|       40 |  8452 | `											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){` |
|        8 |  8453 | `												VmInstr *pNext = pInstr + 1;` |
|        8 |  8454 | `												int bIsLhs = 0;` |
|        8 |  8455 | `												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|        6 |  8456 | `													bIsLhs = 1;` |
|        2 |  8457 | `												}` |
|        8 |  8458 | `												if( !bIsLhs ){` |
|        3 |  8459 | `													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);` |
|        3 |  8460 | `													if( pThis ){` |
|      ! 0 |  8461 | `														PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8462 | `													}` |
|        3 |  8463 | `													if( rcU == PH7_ABORT ){` |
|      ! 0 |  8464 | `														goto Abort;` |
|        - |  8465 | `													}` |
|        - |  8466 | `													{` |
|        3 |  8467 | `														VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  8468 | `														if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  8469 | `															pc = pFrm2->iExceptionJump - 1;` |
|        3 |  8470 | `															break;` |
|        - |  8471 | `														}` |
|        - |  8472 | `													}` |
|      ! 0 |  8473 | `													goto Exception;` |
|        - |  8474 | `												}` |
|        2 |  8475 | `											}` |
|       18 |  8476 | `										}` |
|       18 |  8477 | `									}` |
|        - |  8478 | `									/* Load the desired attribute */` |
|       94 |  8479 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       94 |  8480 | `									if( pValue ){` |
|       94 |  8481 | `										PH7_MemObjLoad(pValue,pTos);` |
|       94 |  8482 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  8483 | `											/* Load index number */` |
|       50 |  8484 | `											pTos->nIdx = pAttr->nIdx;` |
|       24 |  8485 | `										}` |
|       46 |  8486 | `									}` |
|       48 |  8487 | `								}else{` |
|        - |  8488 | `									/* Throw Error exception (PHP-compatible) */` |
|        - |  8489 | `									char zMsg[256];` |
|        5 |  8490 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|        5 |  8491 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|        7 |  8492 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",` |
|        4 |  8493 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|        4 |  8494 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        3 |  8495 | `									}else{` |
|      ! 0 |  8496 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  8497 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  8498 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        - |  8499 | `									}` |
|        5 |  8500 | `									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|        5 |  8501 | `									goto Abort;` |
|        - |  8502 | `								}` |
|        - |  8503 | `							}` |
|       46 |  8504 | `						}` |
|        - |  8505 | `					}` |
|        - |  8506 | `				}` |
|      236 |  8507 | `				if( pThis ){` |
|        - |  8508 | `					/* Safely unreference the object */` |
|        5 |  8509 | `					PH7_ClassInstanceUnref(pThis);` |
|        2 |  8510 | `				}` |
|        - |  8511 | `			}` |
|      119 |  8512 | `		}else{` |
|        - |  8513 | `			/* Pop operands */` |
|      ! 0 |  8514 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  8515 | `			if( !pInstr->p3 ){` |
|      ! 0 |  8516 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  8517 | `			}` |
|      ! 0 |  8518 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  8519 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  8520 | `		}` |
|        - |  8521 | `	}` |
|     8200 |  8522 | `	break;` |
|        - |  8523 | `					}` |
|        - |  8524 | `/*` |
|        - |  8525 | ` * OP_NEW P1 * * *` |
|        - |  8526 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  8527 | ` */` |
|      666 |  8528 | `case PH7_OP_NEW: {` |
|     1334 |  8529 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|     1334 |  8530 | `	ph7_class *pClass = 0;` |
|        - |  8531 | `	ph7_class_instance *pNew;` |
|     1334 |  8532 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  8533 | `		/* Try to extract the desired class */` |
|     2000 |  8534 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|     1332 |  8535 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      666 |  8536 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  8537 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  8538 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  8539 | `	}` |
|     1334 |  8540 | `	if( pClass == 0 ){` |
|        - |  8541 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  8542 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  8543 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  8544 | `			);` |
|        - |  8545 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  8546 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  8547 | `		if( pInstr->iP1 > 0 ){` |
|        - |  8548 | `			/* Pop given arguments */` |
|      ! 0 |  8549 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  8550 | `		}` |
|      ! 0 |  8551 | `		goto Abort;` |
|      ! 0 |  8552 | `	}else{` |
|        - |  8553 | `		ph7_class_method *pCons;` |
|        - |  8554 | `		/* Create a new class instance */` |
|     1334 |  8555 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|     1334 |  8556 | `		if( pNew == 0 ){` |
|      ! 0 |  8557 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8558 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  8559 | `				&pClass->sName` |
|        - |  8560 | `			);` |
|      ! 0 |  8561 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  8562 | `			if( pInstr->iP1 > 0 ){` |
|        - |  8563 | `				/* Pop given arguments */` |
|      ! 0 |  8564 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  8565 | `			}` |
|      ! 0 |  8566 | `			break;` |
|        - |  8567 | `		}` |
|        - |  8568 | `		/* Check if a constructor is available */` |
|     1334 |  8569 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|     1334 |  8570 | `		if( pCons == 0 ){` |
|      938 |  8571 | `			SyString *pName = &pClass->sName;` |
|        - |  8572 | `			/* Check for a constructor with the same base class name */` |
|      938 |  8573 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      468 |  8574 | `		}` |
|     1334 |  8575 | `		if( pCons ){` |
|        - |  8576 | `			/* Call the class constructor.  Collect args in stack order and` |
|        - |  8577 | `			 * forward any VmCallArgMap from the NEW instruction so the` |
|        - |  8578 | `			 * receiving OP_CALL path runs its named-argument matching` |
|        - |  8579 | `			 * (including variadic string-key packing). */` |
|      398 |  8580 | `			VmCallArgMap *pNewMap = (VmCallArgMap *)pInstr->p3;` |
|        - |  8581 | `			sxi32 rcCons;` |
|      398 |  8582 | `			SySetReset(&aArg);` |
|      778 |  8583 | `			while( pArg < pTos ){` |
|      382 |  8584 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      382 |  8585 | `				pArg++;` |
|        2 |  8586 | `			}` |
|      398 |  8587 | `			if( pVm->bErrReport && !(pNewMap && pNewMap->bHasNamed) ){` |
|        - |  8588 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  8589 | `				sxu32 n;` |
|      114 |  8590 | `				n = SySetUsed(&aArg);` |
|        - |  8591 | `				/* Emit a notice for missing arguments (positional-only:` |
|        - |  8592 | `				 * for named args the missing-arg check happens downstream` |
|        - |  8593 | `				 * after resolution). */` |
|      222 |  8594 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|      110 |  8595 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|      110 |  8596 | `					if( pFuncArg ){` |
|      110 |  8597 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  8598 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  8599 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  8600 | `						}` |
|       54 |  8601 | `					}` |
|      110 |  8602 | `					n++;` |
|        2 |  8603 | `				}` |
|       56 |  8604 | `			}` |
|      398 |  8605 | `			rcCons = VmCallClassMethodWithMap(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pNewMap);` |
|        - |  8606 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      398 |  8607 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  8608 | `				pNew->iRef = 1;` |
|      ! 0 |  8609 | `			}` |
|      398 |  8610 | `			if( rcCons == PH7_ABORT \|\| rcCons == PH7_EXCEPTION ){` |
|        - |  8611 | `				/* The constructor raised: the half-constructed object must not` |
|        - |  8612 | `				 * become the NEW result. Drop our reference so it is destroyed.` |
|        - |  8613 | `				 * The class-name operand (and any leftover args) are released by` |
|        - |  8614 | `				 * the Abort/Exception unwind, or explicitly on the resume path. */` |
|        - |  8615 | `				sxi32 iResumePc;` |
|        5 |  8616 | `				PH7_ClassInstanceUnref(pNew);` |
|        5 |  8617 | `				if( rcCons == PH7_ABORT ){` |
|      ! 0 |  8618 | `					goto Abort;` |
|        - |  8619 | `				}` |
|        5 |  8620 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        - |  8621 | `					/* This frame's own try caught it in-place: tidy the stack` |
|        - |  8622 | `					 * (pop ctor args + release the class-name slot) and resume. */` |
|        5 |  8623 | `					if( pInstr->iP1 > 0 ){` |
|        3 |  8624 | `						VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  8625 | `					}` |
|        5 |  8626 | `					PH7_MemObjRelease(pTos);` |
|        5 |  8627 | `					pc = iResumePc;` |
|        5 |  8628 | `					break;` |
|        - |  8629 | `				}` |
|      ! 0 |  8630 | `				goto Exception;` |
|        - |  8631 | `			}` |
|      196 |  8632 | `		}` |
|     1330 |  8633 | `		if( pInstr->iP1 > 0 ){` |
|        - |  8634 | `			/* Pop given arguments */` |
|      312 |  8635 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      155 |  8636 | `		}` |
|     1330 |  8637 | `		PH7_MemObjRelease(pTos);` |
|     1330 |  8638 | `		pTos->x.pOther = pNew;` |
|     1330 |  8639 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  8640 | `	}` |
|     1330 |  8641 | `	break;` |
|        - |  8642 | `				 }` |
|        - |  8643 | `/*` |
|        - |  8644 | ` * OP_CLONE * * *` |
|        - |  8645 | ` * Perfome a clone operation.` |
|        - |  8646 | ` */` |
|       24 |  8647 | `case PH7_OP_CLONE: {` |
|        - |  8648 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  8649 | `#ifdef UNTRUST` |
|        - |  8650 | `	if( pTos < pStack ){` |
|        - |  8651 | `		goto Abort;` |
|        - |  8652 | `	}` |
|        - |  8653 | `#endif` |
|        - |  8654 | `	/* Make sure we are dealing with a class instance */` |
|       50 |  8655 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  8656 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8657 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  8658 | `		PH7_MemObjRelease(pTos);` |
|        5 |  8659 | `		break;` |
|        - |  8660 | `	}` |
|        - |  8661 | `	/* Point to the source */` |
|       46 |  8662 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  8663 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       46 |  8664 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  8665 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8666 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  8667 | `			&pSrc->pClass->sName);` |
|      ! 0 |  8668 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  8669 | `		break;` |
|        - |  8670 | `	}` |
|        - |  8671 | `	/* Perform the clone operation */` |
|       46 |  8672 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       46 |  8673 | `	PH7_MemObjRelease(pTos);` |
|       46 |  8674 | `	if( pClone == 0 ){` |
|      ! 0 |  8675 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8676 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  8677 | `	}else{` |
|        - |  8678 | `		/* Load the cloned object */` |
|       46 |  8679 | `		pTos->x.pOther = pClone;` |
|       46 |  8680 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  8681 | `	}` |
|       46 |  8682 | `	break;` |
|        - |  8683 | `				   }` |
|        - |  8684 | `/*` |
|        - |  8685 | ` * OP_SWITCH * * P3` |
|        - |  8686 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  8687 | ` */` |
|       26 |  8688 | `case PH7_OP_SWITCH: {` |
|       54 |  8689 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  8690 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  8691 | `	ph7_value sValue,sCaseValue;` |
|        - |  8692 | `	sxu32 n,nEntry;` |
|        - |  8693 | `#ifdef UNTRUST` |
|        - |  8694 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  8695 | `		goto Abort;` |
|        - |  8696 | `	}` |
|        - |  8697 | `#endif` |
|        - |  8698 | `	/* Point to the case table  */` |
|       54 |  8699 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       54 |  8700 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  8701 | `	/* Select the appropriate case block to execute */` |
|       54 |  8702 | `	PH7_MemObjInit(pVm,&sValue);` |
|       54 |  8703 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|      132 |  8704 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|      130 |  8705 | `		pCase = &aCase[n];` |
|      130 |  8706 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  8707 | `		/* Execute the case expression first */` |
|      130 |  8708 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  8709 | `		/* Compare the two expression */` |
|      130 |  8710 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|      130 |  8711 | `		PH7_MemObjRelease(&sValue);` |
|      130 |  8712 | `		PH7_MemObjRelease(&sCaseValue);` |
|      130 |  8713 | `		if( rc == 0 ){` |
|        - |  8714 | `			/* Value match,jump to this block */` |
|       52 |  8715 | `			pc = pCase->nStart - 1;` |
|       52 |  8716 | `			break;` |
|        - |  8717 | `		}` |
|       41 |  8718 | `	}` |
|       54 |  8719 | `	VmPopOperand(&pTos,1);` |
|       54 |  8720 | `	if( n >= nEntry ){` |
|        - |  8721 | `		/* No approprite case to execute,jump to the default case */` |
|        3 |  8722 | `		if( pSwitch->nDefault > 0 ){` |
|        3 |  8723 | `			pc = pSwitch->nDefault - 1;` |
|        2 |  8724 | `		}else{` |
|        - |  8725 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  8726 | `			pc = pSwitch->nOut - 1;` |
|        - |  8727 | `		}` |
|        1 |  8728 | `	}` |
|       54 |  8729 | `	break;` |
|        - |  8730 | `					}` |
|        - |  8731 | `/*` |
|        - |  8732 | ` * OP_MATCH * * P3` |
|        - |  8733 | ` *  PHP 8.0 match expression. P3 points to a ph7_match struct holding` |
|        - |  8734 | ` *  the compiled arms. On entry, the subject is on top of the stack.` |
|        - |  8735 | ` *  On exit, the stack slot holds the matched arm's result value.` |
|        - |  8736 | ` *  Comparison is strict (===). No fallthrough. When no arm matches and` |
|        - |  8737 | ` *  no default is present, a fatal UnhandledMatchError is raised.` |
|        - |  8738 | ` */` |
|       54 |  8739 | `case PH7_OP_MATCH: {` |
|      110 |  8740 | `	ph7_match *pMatch = (ph7_match *)pInstr->p3;` |
|      110 |  8741 | `	ph7_match_arm *aArm,*pArm,*pDefault = 0;` |
|        - |  8742 | `	ph7_value sSubject,sCond,sResult;` |
|        - |  8743 | `	sxu32 i,j,nArm,nCond;` |
|      110 |  8744 | `	int matched = 0;` |
|        - |  8745 | `#ifdef UNTRUST` |
|        - |  8746 | `	if( pMatch == 0 \|\| pTos < pStack ){` |
|        - |  8747 | `		goto Abort;` |
|        - |  8748 | `	}` |
|        - |  8749 | `#endif` |
|      110 |  8750 | `	aArm = (ph7_match_arm *)SySetBasePtr(&pMatch->aArms);` |
|      110 |  8751 | `	nArm = SySetUsed(&pMatch->aArms);` |
|      110 |  8752 | `	PH7_MemObjInit(pVm,&sSubject);` |
|      110 |  8753 | `	PH7_MemObjInit(pVm,&sCond);` |
|      110 |  8754 | `	PH7_MemObjInit(pVm,&sResult);` |
|      110 |  8755 | `	PH7_MemObjLoad(pTos,&sSubject);` |
|      348 |  8756 | `	for( i = 0; i < nArm && !matched; ++i ){` |
|      240 |  8757 | `		pArm = &aArm[i];` |
|      240 |  8758 | `		if( pArm->bDefault ){` |
|       13 |  8759 | `			pDefault = pArm;` |
|       13 |  8760 | `			continue;` |
|        - |  8761 | `		}` |
|      228 |  8762 | `		nCond = SySetUsed(&pArm->aConds);` |
|      394 |  8763 | `		for( j = 0; j < nCond; ++j ){` |
|      260 |  8764 | `			SySet *pCondBc = (SySet *)SySetAt(&pArm->aConds,j);` |
|      260 |  8765 | `			if( pCondBc == 0 ){` |
|      ! 0 |  8766 | `				continue;` |
|        - |  8767 | `			}` |
|      260 |  8768 | `			VmLocalExec(pVm,pCondBc,&sCond);` |
|      260 |  8769 | `			rc = PH7_MemObjCmp(&sSubject,&sCond,TRUE /* strict */,0);` |
|      260 |  8770 | `			PH7_MemObjRelease(&sCond);` |
|      260 |  8771 | `			if( rc == 0 ){` |
|       93 |  8772 | `				VmLocalExec(pVm,&pArm->aResult,&sResult);` |
|       93 |  8773 | `				matched = 1;` |
|       93 |  8774 | `				break;` |
|        - |  8775 | `			}` |
|       85 |  8776 | `		}` |
|      115 |  8777 | `	}` |
|      110 |  8778 | `	if( !matched && pDefault ){` |
|       13 |  8779 | `		VmLocalExec(pVm,&pDefault->aResult,&sResult);` |
|       13 |  8780 | `		matched = 1;` |
|        6 |  8781 | `	}` |
|      110 |  8782 | `	if( !matched ){` |
|        5 |  8783 | `		const char *zType = "unknown";` |
|        - |  8784 | `		char zMsg[128];` |
|        - |  8785 | `		sxu32 nMsg;` |
|        5 |  8786 | `		switch(sSubject.iFlags & MEMOBJ_ALL){` |
|      ! 0 |  8787 | `		case MEMOBJ_NULL:   zType = "null";   break;` |
|      ! 0 |  8788 | `		case MEMOBJ_BOOL:   zType = "bool";   break;` |
|        5 |  8789 | `		case MEMOBJ_INT:    zType = "int";    break;` |
|      ! 0 |  8790 | `		case MEMOBJ_REAL:   zType = "float";  break;` |
|      ! 0 |  8791 | `		case MEMOBJ_STRING: zType = "string"; break;` |
|      ! 0 |  8792 | `		case MEMOBJ_HASHMAP:zType = "array";  break;` |
|      ! 0 |  8793 | `		case MEMOBJ_OBJ:    zType = "object"; break;` |
|      ! 0 |  8794 | `		case MEMOBJ_RES:    zType = "resource"; break;` |
|      ! 0 |  8795 | `		default: break;` |
|        - |  8796 | `		}` |
|        7 |  8797 | `		nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        2 |  8798 | `			"Unhandled match case of type %s",zType);` |
|        7 |  8799 | `		VmReportUncaughtException(&(*pVm),"UnhandledMatchError",` |
|        2 |  8800 | `			sizeof("UnhandledMatchError")-1,zMsg,nMsg,0,0);` |
|        5 |  8801 | `		PH7_MemObjRelease(&sSubject);` |
|        5 |  8802 | `		PH7_MemObjRelease(&sResult);` |
|        5 |  8803 | `		goto Abort;` |
|        - |  8804 | `	}` |
|      105 |  8805 | `	PH7_MemObjRelease(&sSubject);` |
|        - |  8806 | `	/* Replace subject on TOS with the arm result */` |
|      105 |  8807 | `	PH7_MemObjStore(&sResult,pTos);` |
|      105 |  8808 | `	PH7_MemObjRelease(&sResult);` |
|      105 |  8809 | `	break;` |
|        - |  8810 | `					}` |
|        - |  8811 | `/*` |
|        - |  8812 | ` * OP_YIELD P1 P2 *` |
|        - |  8813 | ` *  Yield a value from a generator function.` |
|        - |  8814 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  8815 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  8816 | ` */` |
|       34 |  8817 | `case PH7_OP_YIELD: {` |
|        - |  8818 | `	ph7_generator *pGen;` |
|       70 |  8819 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  8820 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  8821 | `		goto Abort;` |
|        - |  8822 | `	}` |
|       70 |  8823 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|       70 |  8824 | `	if( pInstr->iP2 ){` |
|        - |  8825 | `		/* yield $key => $value: value on top, key below */` |
|        - |  8826 | `#ifdef UNTRUST` |
|        - |  8827 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  8828 | `#endif` |
|        7 |  8829 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|        7 |  8830 | `		VmPopOperand(&pTos, 1);` |
|        7 |  8831 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|        7 |  8832 | `		VmPopOperand(&pTos, 1);` |
|        - |  8833 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|        7 |  8834 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  8835 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  8836 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  8837 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  8838 | `			}` |
|        1 |  8839 | `		}` |
|       67 |  8840 | `	}else if( pInstr->iP1 ){` |
|        - |  8841 | `		/* yield $value */` |
|        - |  8842 | `#ifdef UNTRUST` |
|        - |  8843 | `		if( pTos < pStack ) goto Abort;` |
|        - |  8844 | `#endif` |
|       64 |  8845 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       64 |  8846 | `		VmPopOperand(&pTos, 1);` |
|        - |  8847 | `		/* Auto-increment key */` |
|       64 |  8848 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|       64 |  8849 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|       64 |  8850 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       33 |  8851 | `	}else{` |
|        - |  8852 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  8853 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  8854 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  8855 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  8856 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  8857 | `	}` |
|        - |  8858 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|       70 |  8859 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|       70 |  8860 | `	goto Suspend;` |
|        - |  8861 |  |
|        - |  8862 | `/*` |
|        - |  8863 | ` * OP_CALL P1 * *` |
|        - |  8864 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  8865 | ` *  function on the stack.` |
|        - |  8866 | ` */` |
|   359395 |  8867 | `case PH7_OP_CALL: {` |
|   718836 |  8868 | `	sxi32 nCallArgs = pInstr->iP1 + pVm->iSpreadExtra;` |
|        - |  8869 | `	ph7_value *pArg;` |
|   718836 |  8870 | `	pVm->iSpreadExtra = 0; /* Always reset, even if zero */` |
|   718836 |  8871 | `	pArg = &pTos[-nCallArgs];` |
|        - |  8872 | `	SyHashEntry *pEntry;` |
|        - |  8873 | `	SyString sName;` |
|        - |  8874 | `	/* Extract function name */` |
|   718836 |  8875 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|       86 |  8876 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  8877 | `			ph7_value sResult;` |
|        - |  8878 | `			sxi32 rcArr;` |
|        3 |  8879 | `			SySetReset(&aArg);` |
|        3 |  8880 | `			while( pArg < pTos ){` |
|      ! 0 |  8881 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  8882 | `				pArg++;` |
|      ! 0 |  8883 | `			}` |
|        3 |  8884 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  8885 | `			/* May be a class instance and it's static method */` |
|        3 |  8886 | `			rcArr = PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|        3 |  8887 | `			SySetReset(&aArg);` |
|        - |  8888 | `			/* Pop given arguments */` |
|        3 |  8889 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8890 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8891 | `			}` |
|        3 |  8892 | `			if( rcArr == PH7_ABORT ){` |
|      ! 0 |  8893 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8894 | `				goto Abort;` |
|        - |  8895 | `			}` |
|        3 |  8896 | `			if( rcArr == PH7_EXCEPTION ){` |
|        - |  8897 | `				/* An array callable ([$obj,'m']()) raised: resume after this frame's` |
|        - |  8898 | `				 * try if it caught the exception in-place, otherwise propagate. */` |
|        - |  8899 | `				sxi32 iResumePc;` |
|        3 |  8900 | `				PH7_MemObjRelease(&sResult);` |
|        3 |  8901 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        3 |  8902 | `					PH7_MemObjRelease(pTos);` |
|        3 |  8903 | `					pc = iResumePc;` |
|        3 |  8904 | `					break;` |
|        - |  8905 | `				}` |
|      ! 0 |  8906 | `				goto Exception;` |
|        - |  8907 | `			}` |
|        - |  8908 | `			/* Copy result */` |
|      ! 0 |  8909 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  8910 | `			PH7_MemObjRelease(&sResult);` |
|       84 |  8911 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       84 |  8912 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  8913 | `			ph7_value sResult;` |
|        - |  8914 | `			sxi32 rcInv;` |
|       84 |  8915 | `			SySetReset(&aArg);` |
|      200 |  8916 | `			while( pArg < pTos ){` |
|      118 |  8917 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      118 |  8918 | `				pArg++;` |
|        2 |  8919 | `			}` |
|       84 |  8920 | `			PH7_MemObjInit(pVm,&sResult);` |
|      125 |  8921 | `			rcInv = VmCallObjectInvoke(&(*pVm),pThis,` |
|       82 |  8922 | `				(int)SySetUsed(&aArg),` |
|       82 |  8923 | `				(ph7_value **)SySetBasePtr(&aArg),` |
|        - |  8924 | `				&sResult,` |
|       82 |  8925 | `				(VmCallArgMap *)pInstr->p3);` |
|       84 |  8926 | `			SySetReset(&aArg);` |
|       84 |  8927 | `			if( nCallArgs > 0 ){` |
|       76 |  8928 | `				VmPopOperand(&pTos,nCallArgs);` |
|       37 |  8929 | `			}` |
|       84 |  8930 | `			if( rcInv == SXERR_INVALID ){` |
|        - |  8931 | `				/* No __invoke: raise a catchable Error and route through try/catch.` |
|        - |  8932 | `				 * sResult was already released by VmCallObjectInvoke.` |
|        - |  8933 | `				 * Pin pThis: the release below would otherwise drop the last ref` |
|        - |  8934 | `				 * to a temporary callable like (new Plain())(...), freeing the` |
|        - |  8935 | `				 * instance before VmRaiseNotCallable reads its class name. */` |
|       13 |  8936 | `				pThis->iRef++;` |
|       13 |  8937 | `				PH7_MemObjRelease(pTos);` |
|       13 |  8938 | `				rc = VmRaiseNotCallable(&(*pVm),pThis);` |
|       13 |  8939 | `				PH7_ClassInstanceUnref(pThis);` |
|       13 |  8940 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8941 | `					goto Abort;` |
|        - |  8942 | `				}` |
|        - |  8943 | `				{` |
|       13 |  8944 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       12 |  8945 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION)` |
|       13 |  8946 | `					 && pFrm2->iExceptionJump > 0 ){` |
|       13 |  8947 | `						pc = pFrm2->iExceptionJump - 1;` |
|       15 |  8948 | `						break;` |
|        - |  8949 | `					}` |
|        - |  8950 | `				}` |
|      ! 0 |  8951 | `				goto Exception;` |
|        - |  8952 | `			}` |
|       72 |  8953 | `			if( rcInv == PH7_ABORT ){` |
|      ! 0 |  8954 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8955 | `				goto Abort;` |
|        - |  8956 | `			}` |
|       72 |  8957 | `			if( rcInv == PH7_EXCEPTION ){` |
|        - |  8958 | `				/* __invoke raised. The catch body (if any) already ran in-place` |
|        - |  8959 | `				 * inside VmThrowException. If THIS frame's own try caught it,` |
|        - |  8960 | `				 * resume after the try/catch; otherwise propagate so the` |
|        - |  8961 | `				 * exception unwinds through intermediate frames with no handler. */` |
|        - |  8962 | `				sxi32 iResumePc;` |
|        7 |  8963 | `				PH7_MemObjRelease(&sResult);` |
|        7 |  8964 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        5 |  8965 | `					PH7_MemObjRelease(pTos);` |
|        5 |  8966 | `					pc = iResumePc;` |
|        5 |  8967 | `					break;` |
|        - |  8968 | `				}` |
|        3 |  8969 | `				goto Exception;` |
|        - |  8970 | `			}` |
|       66 |  8971 | `			PH7_MemObjStore(&sResult,pTos);` |
|       66 |  8972 | `			PH7_MemObjRelease(&sResult);` |
|       34 |  8973 | `		}else{` |
|        - |  8974 | `			/* Raise exception: Invalid function name */` |
|      ! 0 |  8975 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  8976 | `			/* Pop given arguments */` |
|      ! 0 |  8977 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8978 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8979 | `			}` |
|        - |  8980 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  8981 | `			PH7_MemObjRelease(pTos);` |
|        - |  8982 | `		}` |
|       66 |  8983 | `		break;` |
|        - |  8984 | `	}` |
|   718752 |  8985 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  8986 | `	/* Check for a compiled function first.` |
|        - |  8987 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  8988 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   718752 |  8989 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  8990 | `	/* If the compiler qualified this call with a namespace, and the namespaced` |
|        - |  8991 | `	 * function is not found, retry with the global name (strip the namespace` |
|        - |  8992 | `	 * prefix up to the last backslash) before falling back to host functions.` |
|        - |  8993 | `	 * This mirrors PHP's lookup order for unqualified function calls inside` |
|        - |  8994 | `	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */` |
|        - |  8995 | `	{` |
|   718752 |  8996 | `	VmCallArgMap *pCallMap = (VmCallArgMap *)pInstr->p3;` |
|   718752 |  8997 | `	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){` |
|        - |  8998 | `		const char *zFunc;` |
|        - |  8999 | `		const char *zEnd;` |
|        - |  9000 | `		const char *z;` |
|        - |  9001 | `		SyString sGlobal;` |
|       22 |  9002 | `		zFunc = sName.zString;` |
|       22 |  9003 | `		zEnd  = zFunc + sName.nByte;` |
|       22 |  9004 | `		z = zEnd;` |
|        - |  9005 | `		/* Find last namespace separator */` |
|      194 |  9006 | `		while( z > zFunc ){` |
|      194 |  9007 | `			if( z[-1] == '\\' ){` |
|       22 |  9008 | `				break;` |
|        - |  9009 | `			}` |
|      174 |  9010 | `			z--;` |
|        2 |  9011 | `		}` |
|       22 |  9012 | `		if( z > zFunc && z < zEnd ){` |
|        - |  9013 | `			/* Retry lookup using the unqualified/global function name */` |
|       22 |  9014 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       22 |  9015 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|       10 |  9016 | `		}` |
|       10 |  9017 | `	}` |
|        - |  9018 | `	} /* end VmCallArgMap namespace scope */` |
|   718752 |  9019 | `	if( pEntry ){` |
|        - |  9020 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  9021 | `		ph7_class_instance *pThis;` |
|        - |  9022 | `		ph7_value *pFrameStack;` |
|        - |  9023 | `		ph7_vm_func *pVmFunc;` |
|        - |  9024 | `		ph7_class *pSelf;` |
|        - |  9025 | `		VmFrame *pFrame;` |
|        - |  9026 | `		ph7_value *pObj;` |
|        - |  9027 | `		VmSlot sArg;` |
|        - |  9028 | `		sxu32 n;` |
|        - |  9029 | `		/* initialize fields */` |
|    18746 |  9030 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    18746 |  9031 | `		pThis = 0;` |
|    18746 |  9032 | `		pSelf = 0;` |
|    18746 |  9033 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  9034 | `			ph7_class_method *pMeth;` |
|        - |  9035 | `			/* Class method call */` |
|     3370 |  9036 | `			ph7_value *pTarget = &pTos[-1];` |
|     3370 |  9037 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  9038 | `				/* Extract the 'this' pointer */` |
|     3370 |  9039 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  9040 | `					/* Instance already loaded */` |
|     3280 |  9041 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     3280 |  9042 | `					pThis->iRef++;` |
|     3280 |  9043 | `					pSelf = pThis->pClass;` |
|     1639 |  9044 | `				}` |
|     3370 |  9045 | `				if( pSelf == 0 ){` |
|       92 |  9046 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  9047 | `						/* "Late Static Binding" class name */` |
|      128 |  9048 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       42 |  9049 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       42 |  9050 | `					}` |
|       92 |  9051 | `					if( pSelf == 0 ){` |
|       21 |  9052 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|       10 |  9053 | `					}` |
|       45 |  9054 | `				}` |
|     3370 |  9055 | `				if( pThis == 0  ){` |
|       92 |  9056 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       92 |  9057 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       92 |  9058 | `					if( pFrameLocal->pParent ){` |
|        - |  9059 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       66 |  9060 | `						pThis = pFrameLocal->pThis;` |
|       66 |  9061 | `						if( pThis ){` |
|       21 |  9062 | `							pThis->iRef++;` |
|       10 |  9063 | `						}` |
|       32 |  9064 | `					}` |
|       45 |  9065 | `				}` |
|     3370 |  9066 | `				VmPopOperand(&pTos,1);` |
|     3370 |  9067 | `				PH7_MemObjRelease(pTos);` |
|        - |  9068 | `				/* Synchronize pointers */` |
|     3370 |  9069 | `				pArg = &pTos[-nCallArgs];` |
|        - |  9070 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  9071 | `				 * user have already computed the random generated unique class method name` |
|        - |  9072 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  9073 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  9074 | `				 */` |
|     3370 |  9075 | `				while( pArg < pStack ){` |
|      ! 0 |  9076 | `					pArg++;` |
|      ! 0 |  9077 | `				}` |
|     3370 |  9078 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  9079 | `					/* Check if the call is allowed */` |
|     3370 |  9080 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     3370 |  9081 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       14 |  9082 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - |  9083 | `							/* Throw Error exception (PHP-compatible) */` |
|        - |  9084 | `							char zMsg[256];` |
|      ! 0 |  9085 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  9086 | `							SyBufferFormat(zMsg,sizeof(zMsg),"Call to %s method %.*s::%.*s() from global scope",` |
|      ! 0 |  9087 | `								zVis,(int)pSelf->sName.nByte,pSelf->sName.zString,` |
|      ! 0 |  9088 | `								(int)pVmFunc->sName.nByte,pVmFunc->sName.zString);` |
|        - |  9089 | `							/* Pop given arguments */` |
|      ! 0 |  9090 | `							if( nCallArgs > 0 ){` |
|      ! 0 |  9091 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  9092 | `							}` |
|      ! 0 |  9093 | `							VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  9094 | `							goto Abort;` |
|        - |  9095 | `						}` |
|        6 |  9096 | `					}` |
|     1684 |  9097 | `				}` |
|     1684 |  9098 | `			}` |
|     1684 |  9099 | `		}` |
|        - |  9100 | `		/* Check The recursion limit. Hitting it raises a clean, non-catchable` |
|        - |  9101 | `		 * fatal (was: silently set NULL and continue) and halts. The check is` |
|        - |  9102 | `		 * before VmEnterFrame/the recursive VmByteCodeExec below, so a` |
|        - |  9103 | `		 * correctly-set cap also keeps deep recursion off the native stack. */` |
|    18746 |  9104 | `		if( VmRecursionExceeded(pVm) ){` |
|        - |  9105 | `			/* Args and the function-name slot are released by the Abort label,` |
|        - |  9106 | `			 * which walks the whole operand stack — don't release them here. */` |
|        5 |  9107 | `			VmRecursionFatal(&(*pVm));` |
|        5 |  9108 | `			goto Abort;` |
|        - |  9109 | `		}` |
|    18742 |  9110 | `		if( pVmFunc->pNextName ){` |
|        - |  9111 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      140 |  9112 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       69 |  9113 | `		}` |
|    18742 |  9114 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  9115 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  9116 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  9117 | `			ph7_generator *pGenerator;` |
|        - |  9118 | `			ph7_class_instance *pGenObj;` |
|        - |  9119 | `			ph7_value *pCtxAttr;` |
|        - |  9120 | `			SyString sAttrName;` |
|        - |  9121 | `			ph7_value **apCallArgs;` |
|        - |  9122 | `			int nGenArgs, iArg;` |
|        - |  9123 | `			/* Collect arguments from the operand stack */` |
|       28 |  9124 | `			nGenArgs = (int)(pTos - pArg);` |
|       28 |  9125 | `			apCallArgs = 0;` |
|       28 |  9126 | `			if( nGenArgs > 0 ){` |
|       14 |  9127 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  9128 | `					nGenArgs * sizeof(ph7_value *));` |
|       10 |  9129 | `				if( apCallArgs == 0 ){` |
|        - |  9130 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  9131 | `					nGenArgs = 0;` |
|      ! 0 |  9132 | `				}else{` |
|       10 |  9133 | `					VmCallArgMap *pGenMap = (VmCallArgMap *)pInstr->p3;` |
|       10 |  9134 | `					int didReorder = 0;` |
|       10 |  9135 | `					if( pGenMap && pGenMap->bHasNamed ){` |
|        - |  9136 | `						/* Named-argument reordering for generator */` |
|        5 |  9137 | `						ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        5 |  9138 | `						sxu32 nF = SySetUsed(&pVmFunc->aArgs);` |
|        5 |  9139 | `						sxu32 nNV = nF;` |
|        5 |  9140 | `						sxi32 iVIdx = -1;` |
|        - |  9141 | `						sxi32 *aGSlot;` |
|        - |  9142 | `						sxu8 *aGUsed;` |
|        - |  9143 | `						sxu32 gi;` |
|       13 |  9144 | `						for( gi = 0; gi < nF; gi++ ){` |
|        9 |  9145 | `							if( aFA[gi].iFlags & VM_FUNC_ARG_VARIADIC ){ nNV = gi; iVIdx = (sxi32)gi; break; }` |
|        5 |  9146 | `						}` |
|        7 |  9147 | `						aGSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  9148 | `							(sxu32)nGenArgs * sizeof(sxi32) + nNV * sizeof(sxu8));` |
|        5 |  9149 | `						if( aGSlot ){` |
|        5 |  9150 | `							aGUsed = (sxu8 *)&aGSlot[nGenArgs];` |
|        7 |  9151 | `							rc = VmResolveNamedArgs(&(*pVm),pGenMap,aFA,nNV,iVIdx,` |
|        2 |  9152 | `								(sxu32)nGenArgs,aGSlot,aGUsed);` |
|        5 |  9153 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  9154 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 |  9155 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  9156 | `								goto Abort;` |
|        - |  9157 | `							}` |
|        - |  9158 | `							/* Build apCallArgs in formal-parameter order, then` |
|        - |  9159 | `							 * append overflow (variadic / positional beyond` |
|        - |  9160 | `							 * formals) so downstream sees every argument. */` |
|        - |  9161 | `							{` |
|        5 |  9162 | `								int nOut = 0;` |
|       13 |  9163 | `								for( gi = 0; gi < nNV; gi++ ){` |
|        - |  9164 | `									sxu32 gj;` |
|       13 |  9165 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       13 |  9166 | `										if( aGSlot[gj] == (sxi32)gi ){` |
|        9 |  9167 | `											apCallArgs[nOut++] = &pArg[gj];` |
|        9 |  9168 | `											break;` |
|        - |  9169 | `										}` |
|        3 |  9170 | `									}` |
|        5 |  9171 | `								}` |
|       13 |  9172 | `								for( gi = 0; gi < (sxu32)nGenArgs; gi++ ){` |
|        9 |  9173 | `									if( aGSlot[gi] == -1 \|\| aGSlot[gi] == -2 ){` |
|      ! 0 |  9174 | `										apCallArgs[nOut++] = &pArg[gi];` |
|      ! 0 |  9175 | `									}` |
|        5 |  9176 | `								}` |
|        5 |  9177 | `								nGenArgs = nOut;` |
|        - |  9178 | `							}` |
|        5 |  9179 | `							SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        5 |  9180 | `							didReorder = 1;` |
|        2 |  9181 | `						}` |
|        - |  9182 | `						/* If aGSlot allocation failed, fall through to` |
|        - |  9183 | `						 * positional fill below — preserves arg order rather` |
|        - |  9184 | `						 * than passing an uninitialized apCallArgs. */` |
|        2 |  9185 | `					}` |
|       10 |  9186 | `					if( !didReorder ){` |
|       12 |  9187 | `						for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|        8 |  9188 | `							apCallArgs[iArg] = &pArg[iArg];` |
|        5 |  9189 | `						}` |
|        2 |  9190 | `					}` |
|        - |  9191 | `				}` |
|        4 |  9192 | `			}` |
|        - |  9193 | `			/* Create execution context and generator wrapper */` |
|       28 |  9194 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       28 |  9195 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  9196 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  9197 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  9198 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  9199 | `				break;` |
|        - |  9200 | `			}` |
|       28 |  9201 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       28 |  9202 | `			if( pGenerator == 0 ){` |
|      ! 0 |  9203 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  9204 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  9205 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  9206 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  9207 | `				break;` |
|        - |  9208 | `			}` |
|        - |  9209 | `			/* Set up the frame with arguments, closure env, $this */` |
|       28 |  9210 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       28 |  9211 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       28 |  9212 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs);` |
|       28 |  9213 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       28 |  9214 | `			pExecCtx->pFrame->pParent = 0;` |
|       28 |  9215 | `			if( apCallArgs ){` |
|       10 |  9216 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        4 |  9217 | `			}` |
|       28 |  9218 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  9219 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  9220 | `				if( pThis ){` |
|      ! 0 |  9221 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  9222 | `				}` |
|      ! 0 |  9223 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9224 | `					goto Abort;` |
|        - |  9225 | `				}` |
|      ! 0 |  9226 | `				break;` |
|        - |  9227 | `			}` |
|        - |  9228 | `			/* Create Generator class instance */` |
|       28 |  9229 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       28 |  9230 | `			if( pGenObj == 0 ){` |
|      ! 0 |  9231 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  9232 | `				break;` |
|        - |  9233 | `			}` |
|        - |  9234 | `			/* Store generator in __ctx attribute */` |
|       28 |  9235 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       28 |  9236 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       28 |  9237 | `			if( pCtxAttr ){` |
|       28 |  9238 | `				pCtxAttr->x.pOther = pGenerator;` |
|       28 |  9239 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       13 |  9240 | `			}` |
|        - |  9241 | `			/* Pop args and function name, push Generator object */` |
|       28 |  9242 | `			PH7_MemObjRelease(pTos);` |
|       28 |  9243 | `			pTos = &pTos[-nCallArgs];` |
|       28 |  9244 | `			pTos->x.pOther = pGenObj;` |
|       28 |  9245 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       28 |  9246 | `			pGenObj->iRef++;` |
|       28 |  9247 | `			if( pThis ){` |
|      ! 0 |  9248 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  9249 | `			}` |
|       28 |  9250 | `			break;` |
|        - |  9251 | `		}` |
|        - |  9252 | `		/* Extract the formal argument set */` |
|    18716 |  9253 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  9254 | `		/* Create a new VM frame  */` |
|    18716 |  9255 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    18716 |  9256 | `		if( rc != SXRET_OK ){` |
|        - |  9257 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  9258 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  9259 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  9260 | `				&pVmFunc->sName);` |
|        - |  9261 | `			/* Pop given arguments */` |
|      ! 0 |  9262 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  9263 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  9264 | `			}` |
|        - |  9265 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  9266 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  9267 | `			break;` |
|        - |  9268 | `		}` |
|    18716 |  9269 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  9270 | `			/* Install the '$this' variable */` |
|        - |  9271 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     3298 |  9272 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     3298 |  9273 | `			if( pObj ){` |
|        - |  9274 | `				/* Reflect the change */` |
|     3298 |  9275 | `				pObj->x.pOther = pThis;` |
|     3298 |  9276 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|     1648 |  9277 | `			}` |
|     1648 |  9278 | `		}` |
|    18716 |  9279 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  9280 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  9281 | `			/* Install static variables */` |
|        6 |  9282 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|       12 |  9283 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|        6 |  9284 | `				pStatic = &aStatic[n];` |
|        6 |  9285 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  9286 | `					/* Initialize the static variables */` |
|        6 |  9287 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|        6 |  9288 | `					if( pObj ){` |
|        - |  9289 | `						/* Assume a NULL initialization value */` |
|        6 |  9290 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|        6 |  9291 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  9292 | `							/* Evaluate initialization expression (Any complex expression) */` |
|        6 |  9293 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|        3 |  9294 | `						}` |
|        6 |  9295 | `						pObj->nIdx = pStatic->nIdx;` |
|        3 |  9296 | `					}else{` |
|      ! 0 |  9297 | `						continue;` |
|        - |  9298 | `					}` |
|        3 |  9299 | `				}` |
|        - |  9300 | `				/* Install in the current frame */` |
|        9 |  9301 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|        6 |  9302 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|        3 |  9303 | `			}` |
|        3 |  9304 | `		}` |
|        - |  9305 | `		/* Push arguments in the local frame */` |
|        - |  9306 | `		{` |
|    18716 |  9307 | `		VmCallArgMap *pCallMap3 = (VmCallArgMap *)pInstr->p3;` |
|        - |  9308 | `		/* Caller file's strict_types mode — governs parameter coercion` |
|        - |  9309 | `		 * (but NOT return coercion, which uses the callee's file). */` |
|    18716 |  9310 | `		int bCallIsStrict = (pCallMap3 && pCallMap3->bStrict) ? 1 : 0;` |
|    18716 |  9311 | `		if( pCallMap3 && pCallMap3->bHasNamed ){` |
|        - |  9312 | `			/* ============================================================` |
|        - |  9313 | `			 * Named-argument matching path (PHP 8.0)` |
|        - |  9314 | `			 *` |
|        - |  9315 | `			 * Resolve each actual argument to its formal parameter by name` |
|        - |  9316 | `			 * or position, then install them in the frame.` |
|        - |  9317 | `			 * ============================================================ */` |
|       96 |  9318 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|       96 |  9319 | `			sxu32 nActual = (sxu32)(pTos - pArg);` |
|       96 |  9320 | `			sxi32 iVariadicIdx = -1;` |
|        - |  9321 | `			sxu32 nNonVariadic;` |
|        - |  9322 | `			sxi32 *aSlot;` |
|        - |  9323 | `			sxu8  *aUsed;` |
|        - |  9324 | `			sxu32 i;` |
|        - |  9325 | `			/* Find variadic parameter index */` |
|      292 |  9326 | `			for( i = 0; i < nFormal; i++ ){` |
|      206 |  9327 | `				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        9 |  9328 | `					iVariadicIdx = (sxi32)i;` |
|        9 |  9329 | `					break;` |
|        - |  9330 | `				}` |
|      100 |  9331 | `			}` |
|       96 |  9332 | `			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;` |
|        - |  9333 | `			/* Allocate mapping arrays */` |
|      143 |  9334 | `			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|       94 |  9335 | `				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));` |
|       96 |  9336 | `			if( aSlot == 0 ){` |
|      ! 0 |  9337 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");` |
|      ! 0 |  9338 | `				goto Abort;` |
|        - |  9339 | `			}` |
|       96 |  9340 | `			aUsed = (sxu8 *)&aSlot[nActual];` |
|        - |  9341 | `			/* Resolve named arguments to formal parameters */` |
|      143 |  9342 | `			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,` |
|       47 |  9343 | `				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);` |
|       96 |  9344 | `			if( rc == PH7_ABORT ){` |
|        7 |  9345 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        7 |  9346 | `				goto Abort;` |
|        - |  9347 | `			}` |
|        - |  9348 | `			/* Pass 2: install arguments into the frame by formal parameter order */` |
|      275 |  9349 | `			for( n = 0; n < nNonVariadic; n++ ){` |
|        - |  9350 | `				/* Find the stack arg mapped to formal n */` |
|      187 |  9351 | `				sxi32 iSrc = -1;` |
|      309 |  9352 | `				for( i = 0; i < nActual; i++ ){` |
|      291 |  9353 | `					if( aSlot[i] == (sxi32)n ){` |
|      169 |  9354 | `						iSrc = (sxi32)i;` |
|      169 |  9355 | `						break;` |
|        - |  9356 | `					}` |
|       62 |  9357 | `				}` |
|      187 |  9358 | `				if( iSrc >= 0 ){` |
|        - |  9359 | `					/* Argument was provided — install with type checking */` |
|      169 |  9360 | `					ph7_value *pVal = &pArg[iSrc];` |
|        - |  9361 | `					/* NULL-to-default redirect (existing behavior) */` |
|      168 |  9362 | `					if( (pVal->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|        5 |  9363 | `						&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|      ! 0 |  9364 | `						rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pVal);` |
|      ! 0 |  9365 | `						if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9366 | `					}` |
|        - |  9367 | `					/* Type checking: union types */` |
|      169 |  9368 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       19 |  9369 | `						sxi32 rcU = VmCoerceToUnion(pVm, pVal, &aFormalArg[n].aUnionAlts,` |
|       12 |  9370 | `							(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        6 |  9371 | `							bCallIsStrict);` |
|       13 |  9372 | `						if( rcU != SXRET_OK ){` |
|        - |  9373 | `							const char *zGiven;` |
|      ! 0 |  9374 | `							const char *zExpected = "union";` |
|        - |  9375 | `							char zBuf[128];` |
|        - |  9376 | `							char zTypeBuf[128];` |
|      ! 0 |  9377 | `							if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  9378 | `								zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|      ! 0 |  9379 | `							}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  9380 | `								zGiven = "null";` |
|      ! 0 |  9381 | `							}else{` |
|      ! 0 |  9382 | `								zGiven = ph7_type_name(pVal);` |
|        - |  9383 | `							}` |
|      ! 0 |  9384 | `							if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|      ! 0 |  9385 | `								zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  9386 | `							}` |
|      ! 0 |  9387 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9388 | `								&aFormalArg[n].sName, zExpected, zGiven);` |
|      ! 0 |  9389 | `							if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9390 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  9391 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  9392 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9393 | `							pFrameStack = 0;` |
|      ! 0 |  9394 | `							rc = PH7_EXCEPTION;` |
|      ! 0 |  9395 | `							goto SkipFuncBody;` |
|        - |  9396 | `						}` |
|      171 |  9397 | `					}else if( aFormalArg[n].nType > 0` |
|       91 |  9398 | `						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|        - |  9399 | `						/* Scalar/class type checking */` |
|       17 |  9400 | `						if( aFormalArg[n].nType == SXU32_HIGH ){` |
|      ! 0 |  9401 | `							SyString *pName = &aFormalArg[n].sClass;` |
|        - |  9402 | `							ph7_class *pClass;` |
|      ! 0 |  9403 | `							int rcPseudo = VmCheckPseudoType(&(*pVm),pVal,pName);` |
|      ! 0 |  9404 | `							if( rcPseudo == 0 ){` |
|        - |  9405 | `								/* Recognised pseudo-type (true/false/iterable); value mismatches */` |
|        - |  9406 | `								char zTypeBuf[128],zGivenBuf[128];` |
|      ! 0 |  9407 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9408 | `									&aFormalArg[n].sName,` |
|      ! 0 |  9409 | `									VmSyStringToCStr(pName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  9410 | `									VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|      ! 0 |  9411 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9412 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  9413 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  9414 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9415 | `								pFrameStack = 0;` |
|      ! 0 |  9416 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  9417 | `								goto SkipFuncBody;` |
|        - |  9418 | `							}` |
|        - |  9419 | `							/* rcPseudo==1 -> matched pseudo-type (accept); -1 -> real class */` |
|      ! 0 |  9420 | `							pClass = (rcPseudo == 1) ? 0 : PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  9421 | `							if( pClass ){` |
|      ! 0 |  9422 | `								if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9423 | `									if( (pVal->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  9424 | `										VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9425 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9426 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9427 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  9428 | `									}` |
|      ! 0 |  9429 | `								}else{` |
|      ! 0 |  9430 | `									ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  9431 | `									if( !PH7_VmInstanceOf(pInst->pClass,pClass) ){` |
|      ! 0 |  9432 | `										VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  9433 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9434 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9435 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  9436 | `									}` |
|        - |  9437 | `								}` |
|      ! 0 |  9438 | `							}` |
|       17 |  9439 | `						}else if( (pVal->iFlags & aFormalArg[n].nType) == 0 ){` |
|        7 |  9440 | `							if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|      ! 0 |  9441 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9442 | `									&aFormalArg[n].sName,"object",ph7_type_name(pVal));` |
|      ! 0 |  9443 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9444 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  9445 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  9446 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9447 | `								pFrameStack = 0;` |
|      ! 0 |  9448 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  9449 | `								goto SkipFuncBody;` |
|        7 |  9450 | `							}else if( VmEnforceScalarType(pVal, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  9451 | `								char zTypeBuf[128];` |
|      ! 0 |  9452 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9453 | `									&aFormalArg[n].sName,` |
|      ! 0 |  9454 | `									VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  9455 | `									ph7_type_name(pVal));` |
|      ! 0 |  9456 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9457 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  9458 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  9459 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9460 | `								pFrameStack = 0;` |
|      ! 0 |  9461 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  9462 | `								goto SkipFuncBody;` |
|        - |  9463 | `							}` |
|        3 |  9464 | `						}` |
|        8 |  9465 | `					}` |
|        - |  9466 | `					/* Install: by reference or by value */` |
|      169 |  9467 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        5 |  9468 | `						if( pVal->nIdx == SXU32_HIGH ){` |
|      ! 0 |  9469 | `							if( (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|      ! 0 |  9470 | `								VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9471 | `									"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  9472 | `									"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  9473 | `							}` |
|      ! 0 |  9474 | `							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  9475 | `						}else{` |
|        7 |  9476 | `							SyHashEntry *pRefEntry = SyHashGet(&pFrame->hVar,` |
|        4 |  9477 | `								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|        5 |  9478 | `							if( pRefEntry == 0 ){` |
|        7 |  9479 | `								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|        4 |  9480 | `									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));` |
|        5 |  9481 | `								sArg.nIdx = pVal->nIdx;` |
|        5 |  9482 | `								sArg.pUserData = 0;` |
|        5 |  9483 | `								SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        2 |  9484 | `							}` |
|        5 |  9485 | `							pObj = 0;` |
|        - |  9486 | `						}` |
|        3 |  9487 | `					}else{` |
|      165 |  9488 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  9489 | `					}` |
|      169 |  9490 | `					if( pObj ){` |
|      165 |  9491 | `						PH7_MemObjStore(pVal,pObj);` |
|      165 |  9492 | `						sArg.nIdx = pObj->nIdx;` |
|      165 |  9493 | `						sArg.pUserData = 0;` |
|      165 |  9494 | `						SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       82 |  9495 | `					}` |
|       85 |  9496 | `				}else{` |
|        - |  9497 | `					/* Argument was NOT provided — use default or leave unset */` |
|       19 |  9498 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  9499 | `						/* Should not reach here; variadic handled separately below */` |
|       19 |  9500 | `					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|       19 |  9501 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       19 |  9502 | `						if( pObj ){` |
|       19 |  9503 | `							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|       19 |  9504 | `							if( rc == PH7_ABORT ) goto Abort;` |
|       19 |  9505 | `							sArg.nIdx = pObj->nIdx;` |
|       19 |  9506 | `							sArg.pUserData = 0;` |
|       19 |  9507 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  9508 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|        1 |  9509 | `								&& (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  9510 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  9511 | `								if( xCast ) xCast(pObj);` |
|      ! 0 |  9512 | `							}` |
|        9 |  9513 | `						}` |
|        9 |  9514 | `					}` |
|        - |  9515 | `					/* else: required param missing — leave unset (matches existing behavior) */` |
|        - |  9516 | `				}` |
|       94 |  9517 | `			}` |
|        - |  9518 | `			/* Handle variadic parameter */` |
|       89 |  9519 | `			if( iVariadicIdx >= 0 ){` |
|        9 |  9520 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);` |
|        9 |  9521 | `				if( pObj ){` |
|        9 |  9522 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  9523 | `					{` |
|        9 |  9524 | `						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;` |
|       31 |  9525 | `						for( i = 0; i < nActual; i++ ){` |
|       23 |  9526 | `							if( aSlot[i] == -1 ){` |
|       16 |  9527 | `								if( i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0 ){` |
|        - |  9528 | `									/* Named variadic entry: insert with string key */` |
|        - |  9529 | `									ph7_value sKey;` |
|       11 |  9530 | `									PH7_MemObjInit(pVm, &sKey);` |
|       11 |  9531 | `									PH7_MemObjStringAppend(&sKey,` |
|       10 |  9532 | `										pCallMap3->aNames[i].zString,` |
|       10 |  9533 | `										(sxu32)pCallMap3->aNames[i].nByte);` |
|       11 |  9534 | `									PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);` |
|       11 |  9535 | `									PH7_MemObjRelease(&sKey);` |
|        6 |  9536 | `								}else{` |
|        - |  9537 | `									/* Positional variadic entry */` |
|      ! 0 |  9538 | `									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);` |
|        - |  9539 | `								}` |
|        5 |  9540 | `							}` |
|       12 |  9541 | `						}` |
|        - |  9542 | `					}` |
|        9 |  9543 | `					sArg.nIdx = pObj->nIdx;` |
|        9 |  9544 | `					sArg.pUserData = 0;` |
|        9 |  9545 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        4 |  9546 | `				}` |
|        5 |  9547 | `			}else{` |
|        - |  9548 | `				/* No variadic — preserve unresolved positional overflow` |
|        - |  9549 | `				 * (aSlot[i] == -2) as anonymous frame args so` |
|        - |  9550 | `				 * func_get_args() / func_num_args() still see them, matching` |
|        - |  9551 | `				 * the positional-only path's behavior. */` |
|       81 |  9552 | `				sxu32 nAnon = nNonVariadic;` |
|      237 |  9553 | `				for( i = 0; i < nActual; i++ ){` |
|      157 |  9554 | `					if( aSlot[i] == -2 ){` |
|        - |  9555 | `						char zAnonBuf[32];` |
|        - |  9556 | `						SyString sAnonName;` |
|      ! 0 |  9557 | `						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),` |
|      ! 0 |  9558 | `							"[%u]apArg",nAnon);` |
|      ! 0 |  9559 | `						sAnonName.zString = zAnonBuf;` |
|      ! 0 |  9560 | `						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);` |
|      ! 0 |  9561 | `						if( pObj ){` |
|      ! 0 |  9562 | `							PH7_MemObjStore(&pArg[i],pObj);` |
|      ! 0 |  9563 | `							sArg.nIdx = pObj->nIdx;` |
|      ! 0 |  9564 | `							sArg.pUserData = 0;` |
|      ! 0 |  9565 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      ! 0 |  9566 | `						}` |
|      ! 0 |  9567 | `						nAnon++;` |
|      ! 0 |  9568 | `					}` |
|       79 |  9569 | `				}` |
|        - |  9570 | `			}` |
|        - |  9571 | `			/* Release all stack arguments */` |
|      267 |  9572 | `			for( i = 0; i < nActual; i++ ){` |
|      179 |  9573 | `				PH7_MemObjRelease(&pArg[i]);` |
|       90 |  9574 | `			}` |
|       89 |  9575 | `			SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        - |  9576 | `			/* Set n to nFormal so the defaults loop below is skipped */` |
|       89 |  9577 | `			n = nFormal;` |
|       45 |  9578 | `		}else{` |
|        - |  9579 | `		/* ============================================================` |
|        - |  9580 | `		 * Positional-only matching path (original)` |
|        - |  9581 | `		 * ============================================================ */` |
|    18622 |  9582 | `		n = 0;` |
|    49358 |  9583 | `		while( pArg < pTos ){` |
|    30816 |  9584 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - |  9585 | `				/* Variadic parameter: collect all remaining args into an array */` |
|       40 |  9586 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       40 |  9587 | `				if( pObj ){` |
|        - |  9588 | `					/* Initialize as empty array */` |
|       40 |  9589 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  9590 | `					{` |
|       40 |  9591 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      150 |  9592 | `						while( pArg < pTos ){` |
|        - |  9593 | `							/* Variadic union type: per-element coercion via the shared helper.` |
|        - |  9594 | `							 *` |
|        - |  9595 | `							 * TODO: PHP reports the runtime element index here` |
|        - |  9596 | `							 * ("Argument #3 must be...") but we report the formal-arg` |
|        - |  9597 | `							 * index (always n+1, the position of the variadic). The` |
|        - |  9598 | `							 * non-union variadic path below has the same limitation;` |
|        - |  9599 | `							 * fixing both wants a separate counter for elements` |
|        - |  9600 | `							 * already packed into the variadic array. */` |
|      114 |  9601 | `							if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       23 |  9602 | `								sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       14 |  9603 | `									(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        7 |  9604 | `									bCallIsStrict);` |
|       16 |  9605 | `								if( rcU != SXRET_OK ){` |
|        - |  9606 | `									const char *zGiven;` |
|        3 |  9607 | `									const char *zExpected = "union";` |
|        - |  9608 | `									char zBuf[128];` |
|        - |  9609 | `									char zTypeBuf[128];` |
|        3 |  9610 | `									if( pArg->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  9611 | `										zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|        3 |  9612 | `									}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  9613 | `										zGiven = "null";` |
|      ! 0 |  9614 | `									}else{` |
|        3 |  9615 | `										zGiven = ph7_type_name(pArg);` |
|        - |  9616 | `									}` |
|        3 |  9617 | `									if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|        3 |  9618 | `										zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        1 |  9619 | `									}` |
|        4 |  9620 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        2 |  9621 | `										&aFormalArg[n].sName, zExpected, zGiven);` |
|        3 |  9622 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9623 | `										goto Abort;` |
|        - |  9624 | `									}` |
|        3 |  9625 | `									PH7_MemObjRelease(pTos);` |
|        3 |  9626 | `									pTos = &pTos[-nCallArgs];` |
|        3 |  9627 | `									pFrameStack = 0;` |
|        3 |  9628 | `									rc = PH7_EXCEPTION;` |
|        3 |  9629 | `									goto SkipFuncBody;` |
|        - |  9630 | `								}` |
|       14 |  9631 | `								PH7_HashmapInsert(pMap, 0, pArg);` |
|       14 |  9632 | `								pArg++;` |
|       14 |  9633 | `								continue;` |
|        - |  9634 | `							}` |
|        - |  9635 | `							/* Apply type coercion to each element if the variadic has a type hint.` |
|        - |  9636 | `							 * Nullable types (?type) allow null through without coercion. */` |
|      114 |  9637 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       42 |  9638 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL))` |
|       41 |  9639 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 |  9640 | `								if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  9641 | `									/* object type hint on variadic: reject non-objects with TypeError */` |
|      ! 0 |  9642 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9643 | `										&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|      ! 0 |  9644 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9645 | `										goto Abort;` |
|        - |  9646 | `									}` |
|        - |  9647 | `									/* Skip function body, route through normal cleanup */` |
|      ! 0 |  9648 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  9649 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9650 | `									pFrameStack = 0;` |
|      ! 0 |  9651 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  9652 | `									goto SkipFuncBody;` |
|       13 |  9653 | `								}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  9654 | `									char zTypeBuf[128];` |
|      ! 0 |  9655 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9656 | `										&aFormalArg[n].sName,` |
|      ! 0 |  9657 | `										VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  9658 | `										ph7_type_name(pArg));` |
|      ! 0 |  9659 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9660 | `										goto Abort;` |
|        - |  9661 | `									}` |
|      ! 0 |  9662 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  9663 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9664 | `									pFrameStack = 0;` |
|      ! 0 |  9665 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  9666 | `									goto SkipFuncBody;` |
|        - |  9667 | `								}` |
|        6 |  9668 | `							}` |
|      100 |  9669 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|      100 |  9670 | `							pArg++;` |
|        2 |  9671 | `						}` |
|        - |  9672 | `					}` |
|       38 |  9673 | `					sArg.nIdx = pObj->nIdx;` |
|       38 |  9674 | `					sArg.pUserData = 0;` |
|       38 |  9675 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  9676 | `				}` |
|       38 |  9677 | `				break; /* All remaining args consumed */` |
|        - |  9678 | `			}` |
|    30778 |  9679 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    30560 |  9680 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|       41 |  9681 | `					&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|        - |  9682 | `					/* NULL values are redirected to default arguments (but not for nullable types) */` |
|      ! 0 |  9683 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  9684 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  9685 | `						goto Abort;` |
|        - |  9686 | `					}` |
|      ! 0 |  9687 | `				}` |
|        - |  9688 | `				/* Union type: dispatch to the shared coercion helper. */` |
|    30562 |  9689 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       95 |  9690 | `					sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       62 |  9691 | `						(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|       31 |  9692 | `						bCallIsStrict);` |
|       64 |  9693 | `					if( rcU != SXRET_OK ){` |
|        - |  9694 | `						const char *zGiven;` |
|       19 |  9695 | `						const char *zExpected = "union";` |
|        - |  9696 | `						char zBuf[128];` |
|        - |  9697 | `						char zTypeBuf[128];` |
|       19 |  9698 | `						if( pArg->iFlags & MEMOBJ_OBJ ){` |
|        7 |  9699 | `							zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|       16 |  9700 | `						}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|        9 |  9701 | `							zGiven = "null";` |
|        5 |  9702 | `						}else{` |
|        5 |  9703 | `							zGiven = ph7_type_name(pArg);` |
|        - |  9704 | `						}` |
|       19 |  9705 | `						if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|       19 |  9706 | `							zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        9 |  9707 | `						}` |
|       28 |  9708 | `						rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       18 |  9709 | `							&aFormalArg[n].sName, zExpected, zGiven);` |
|       19 |  9710 | `						if( rc == PH7_ABORT ){` |
|      ! 0 |  9711 | `							goto Abort;` |
|        - |  9712 | `						}` |
|       19 |  9713 | `						PH7_MemObjRelease(pTos);` |
|       19 |  9714 | `						pTos = &pTos[-nCallArgs];` |
|       19 |  9715 | `						pFrameStack = 0;` |
|       19 |  9716 | `						rc = PH7_EXCEPTION;` |
|       19 |  9717 | `						goto SkipFuncBody;` |
|        - |  9718 | `					}` |
|       23 |  9719 | `				}else` |
|        - |  9720 | `				/* Make sure the given arguments are of the correct type.` |
|        - |  9721 | `				 * Nullable types (?type) allow null through without coercion. */` |
|    30528 |  9722 | `				if( aFormalArg[n].nType > 0` |
|    15974 |  9723 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|     1418 |  9724 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  9725 | `						/* Argument must be a class instance [i.e: object] */` |
|       36 |  9726 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  9727 | `						ph7_class *pClass;` |
|       36 |  9728 | `						int rcPseudo = VmCheckPseudoType(&(*pVm),pArg,pName);` |
|       36 |  9729 | `						if( rcPseudo == 0 ){` |
|        - |  9730 | `							/* Recognised pseudo-type (true/false/iterable); value mismatches */` |
|        - |  9731 | `							char zTypeBuf[128],zGivenBuf[128];` |
|        7 |  9732 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        4 |  9733 | `								&aFormalArg[n].sName,` |
|        2 |  9734 | `								VmSyStringToCStr(pName,zTypeBuf,sizeof(zTypeBuf)),` |
|        2 |  9735 | `								VmValueGivenName(pArg,zGivenBuf,sizeof(zGivenBuf)));` |
|        5 |  9736 | `							if( rc == PH7_ABORT ) goto Abort;` |
|        5 |  9737 | `							PH7_MemObjRelease(pTos);` |
|        5 |  9738 | `							pTos = &pTos[-nCallArgs];` |
|        5 |  9739 | `							pFrameStack = 0;` |
|        5 |  9740 | `							rc = PH7_EXCEPTION;` |
|        5 |  9741 | `							goto SkipFuncBody;` |
|        - |  9742 | `						}` |
|        - |  9743 | `						/* Try to extract the desired class (rcPseudo==1 accepts; -1 real class) */` |
|       32 |  9744 | `						pClass = (rcPseudo == 1) ? 0 : PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       32 |  9745 | `						if( pClass ){` |
|       22 |  9746 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9747 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  9748 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9749 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9750 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9751 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  9752 | `								}` |
|      ! 0 |  9753 | `							}else{` |
|        - |  9754 | `								/* reuse pThis declared in outer scope */` |
|       22 |  9755 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  9756 | `								/* Make sure the object is an instance of the given class */` |
|       22 |  9757 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  9758 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  9759 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9760 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9761 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  9762 | `								}` |
|        - |  9763 | `							}` |
|       12 |  9764 | `						}` |
|     1399 |  9765 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|       28 |  9766 | `						if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  9767 | `							/* object type hint: reject non-objects with TypeError */` |
|       16 |  9768 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       10 |  9769 | `								&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|       11 |  9770 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  9771 | `								goto Abort;` |
|        - |  9772 | `							}` |
|        - |  9773 | `							/* Skip function body, route through normal cleanup */` |
|       11 |  9774 | `							PH7_MemObjRelease(pTos);` |
|       11 |  9775 | `							pTos = &pTos[-nCallArgs];` |
|       11 |  9776 | `							pFrameStack = 0;` |
|       11 |  9777 | `							rc = PH7_EXCEPTION;` |
|       11 |  9778 | `							goto SkipFuncBody;` |
|       18 |  9779 | `						}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  9780 | `							char zTypeBuf[128];` |
|       14 |  9781 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        8 |  9782 | `								&aFormalArg[n].sName,` |
|        8 |  9783 | `								VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|        4 |  9784 | `								ph7_type_name(pArg));` |
|       10 |  9785 | `							if( rc == PH7_ABORT ){` |
|        5 |  9786 | `								goto Abort;` |
|        - |  9787 | `							}` |
|        5 |  9788 | `							PH7_MemObjRelease(pTos);` |
|        5 |  9789 | `							pTos = &pTos[-nCallArgs];` |
|        5 |  9790 | `							pFrameStack = 0;` |
|        5 |  9791 | `							rc = PH7_EXCEPTION;` |
|        5 |  9792 | `							goto SkipFuncBody;` |
|        - |  9793 | `						}` |
|        4 |  9794 | `					}` |
|      697 |  9795 | `				}` |
|    30522 |  9796 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  9797 | `					/* Pass by reference */` |
|       58 |  9798 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  9799 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  9800 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  9801 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9802 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  9803 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  9804 | `						}` |
|        - |  9805 | `						/* Switch to pass by value */` |
|      ! 0 |  9806 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  9807 | `					}else{` |
|        - |  9808 | `						SyHashEntry *pRefEntry;` |
|        - |  9809 | `						/* Install the referenced variable in the private function frame */` |
|       58 |  9810 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       58 |  9811 | `						if( pRefEntry == 0 ){` |
|       86 |  9812 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       56 |  9813 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       58 |  9814 | `							sArg.nIdx = pArg->nIdx;` |
|       58 |  9815 | `							sArg.pUserData = 0;` |
|       58 |  9816 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       28 |  9817 | `						}` |
|       58 |  9818 | `						pObj = 0;` |
|        - |  9819 | `					}` |
|       30 |  9820 | `				}else{` |
|        - |  9821 | `					/* Pass by value,make a copy of the given argument */` |
|    30466 |  9822 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  9823 | `				}` |
|    15262 |  9824 | `			}else{` |
|        - |  9825 | `				char zName[32];` |
|        - |  9826 | `				SyString sArgName;` |
|        - |  9827 | `				/* Set a dummy name */` |
|      218 |  9828 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      218 |  9829 | `				sArgName.zString = zName;` |
|        - |  9830 | `				/* Annonymous argument */` |
|      218 |  9831 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  9832 | `			}` |
|    30738 |  9833 | `			if( pObj ){` |
|    30682 |  9834 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  9835 | `				/* Insert argument index  */` |
|    30682 |  9836 | `				sArg.nIdx = pObj->nIdx;` |
|    30682 |  9837 | `				sArg.pUserData = 0;` |
|    30682 |  9838 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    15340 |  9839 | `			}` |
|    30738 |  9840 | `			PH7_MemObjRelease(pArg);` |
|    30738 |  9841 | `			pArg++;` |
|    30738 |  9842 | `			++n;` |
|        2 |  9843 | `		}` |
|        - |  9844 | `		} /* end named vs positional branch */` |
|        - |  9845 | `		/* Set up closure environment */` |
|    18668 |  9846 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  9847 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  9848 | `			ph7_value *pValue;` |
|        - |  9849 | `			sxu32 iEnv;` |
|      184 |  9850 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|      434 |  9851 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|      252 |  9852 | `				pEnv = &aEnv[iEnv];` |
|      252 |  9853 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  9854 | `					/* Do not install null value */` |
|      178 |  9855 | `					continue;` |
|        - |  9856 | `				}` |
|       76 |  9857 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       76 |  9858 | `				if( pValue == 0 ){` |
|      ! 0 |  9859 | `					continue;` |
|        - |  9860 | `				}` |
|        - |  9861 | `				/* Invalidate any prior representation */` |
|       76 |  9862 | `				PH7_MemObjRelease(pValue);` |
|        - |  9863 | `				/* Duplicate bound variable value */` |
|       76 |  9864 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|       39 |  9865 | `			}` |
|       91 |  9866 | `		}` |
|        - |  9867 | `		/* Process default values for remaining formal parameters */` |
|    21588 |  9868 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     2968 |  9869 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  9870 | `				/* Variadic parameter with no extra args — create empty array */` |
|       48 |  9871 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       48 |  9872 | `				if( pObj ){` |
|       48 |  9873 | `					PH7_MemObjToHashmap(pObj);` |
|       48 |  9874 | `					sArg.nIdx = pObj->nIdx;` |
|       48 |  9875 | `					sArg.pUserData = 0;` |
|       48 |  9876 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       23 |  9877 | `				}` |
|       48 |  9878 | `				n++;` |
|       48 |  9879 | `				break; /* Variadic is always last */` |
|        - |  9880 | `			}` |
|     2922 |  9881 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     2916 |  9882 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     2916 |  9883 | `				if( pObj ){` |
|        - |  9884 | `					/* Evaluate the default value and extract it's result */` |
|     2916 |  9885 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     2916 |  9886 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  9887 | `						goto Abort;` |
|        - |  9888 | `					}` |
|        - |  9889 | `					/* Insert argument index */` |
|     2916 |  9890 | `					sArg.nIdx = pObj->nIdx;` |
|     2916 |  9891 | `					sArg.pUserData = 0;` |
|     2916 |  9892 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  9893 | `					/* Make sure the default argument is of the correct type */` |
|     2914 |  9894 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|     1880 |  9895 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|        3 |  9896 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  9897 | `						/* Cast to the desired type */` |
|        3 |  9898 | `						xCast(pObj);` |
|        1 |  9899 | `					}` |
|     1457 |  9900 | `				}` |
|     1457 |  9901 | `			}` |
|     2922 |  9902 | `			++n;` |
|        2 |  9903 | `		}` |
|        - |  9904 | `		} /* end VmCallArgMap scope */` |
|        - |  9905 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  9906 | `		 * does not return anything.` |
|        - |  9907 | `		 */` |
|    18668 |  9908 | `		PH7_MemObjRelease(pTos);` |
|    18668 |  9909 | `		pTos = &pTos[-nCallArgs];` |
|        - |  9910 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    18668 |  9911 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    18668 |  9912 | `		if( pFrameStack == 0 ){` |
|        - |  9913 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  9914 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  9915 | `				&pVmFunc->sName);` |
|      ! 0 |  9916 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  9917 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  9918 | `			}` |
|      ! 0 |  9919 | `			break;` |
|        - |  9920 | `		}` |
|     9333 |  9921 | `SkipFuncBody:` |
|    18706 |  9922 | `		if( pSelf ){` |
|        - |  9923 | `			/* Push class name */` |
|     3368 |  9924 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|     1683 |  9925 | `		}` |
|        - |  9926 | `		/* Increment nesting level */` |
|    18706 |  9927 | `		pVm->nRecursionDepth++;` |
|    18706 |  9928 | `		if( rc != PH7_EXCEPTION ){` |
|        - |  9929 | `			/* Execute function body */` |
|    28001 |  9930 | `			rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0,` |
|    18666 |  9931 | `				pVmFunc->nReturnType > 0 ? pVmFunc : 0);` |
|     9333 |  9932 | `		}` |
|        - |  9933 | `		/* Decrement nesting level */` |
|    18706 |  9934 | `		pVm->nRecursionDepth--;` |
|    18706 |  9935 | `		if( pSelf ){` |
|        - |  9936 | `			/* Pop class name */` |
|     3368 |  9937 | `			(void)SySetPop(&pVm->aSelf);` |
|     1683 |  9938 | `		}` |
|        - |  9939 | `		/* Cleanup the mess left behind */` |
|    18706 |  9940 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  9941 | `			/* Return by reference,reflect that */` |
|        9 |  9942 | `			if( n != SXU32_HIGH ){` |
|        9 |  9943 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  9944 | `				sxu32 i;` |
|        - |  9945 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  9946 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  9947 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  9948 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  9949 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  9950 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  9951 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  9952 | `								&pVmFunc->sName);` |
|      ! 0 |  9953 | `						}` |
|      ! 0 |  9954 | `						n = SXU32_HIGH;` |
|      ! 0 |  9955 | `						break;` |
|        - |  9956 | `					}` |
|        3 |  9957 | `				}` |
|        5 |  9958 | `			}else{` |
|      ! 0 |  9959 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  9960 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  9961 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  9962 | `						&pVmFunc->sName);` |
|      ! 0 |  9963 | `				}` |
|        - |  9964 | `			}` |
|        9 |  9965 | `			pTos->nIdx = n;` |
|        4 |  9966 | `		}` |
|        - |  9967 | `		/* Cleanup the mess left behind */` |
|    18706 |  9968 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  9969 | `			/* An exception was throw in this frame */` |
|      112 |  9970 | `			pFrame = pFrame->pParent;` |
|      112 |  9971 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  9972 | `				/* Pop the resutlt */` |
|       74 |  9973 | `				VmPopOperand(&pTos,1);` |
|        - |  9974 | `				/* Jump to this destination */` |
|       74 |  9975 | `				pc = pFrame->iExceptionJump - 1;` |
|       74 |  9976 | `				rc = PH7_OK;` |
|       38 |  9977 | `			}else{` |
|       39 |  9978 | `				if( pFrame->pParent ){` |
|       39 |  9979 | `					rc = PH7_EXCEPTION;` |
|       20 |  9980 | `				}else{` |
|        - |  9981 | `					/* Continue normal execution */` |
|      ! 0 |  9982 | `					rc = PH7_OK;` |
|        - |  9983 | `				}` |
|        - |  9984 | `			}` |
|       55 |  9985 | `		}` |
|        - |  9986 | `		/* Free the operand stack (NULL when function body was skipped) */` |
|    18706 |  9987 | `		if( pFrameStack ){` |
|    18668 |  9988 | `			SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|     9333 |  9989 | `		}` |
|        - |  9990 | `		/* Leave the frame */` |
|    18706 |  9991 | `		VmLeaveFrame(&(*pVm));` |
|    18706 |  9992 | `		if( rc == PH7_ABORT ){` |
|        - |  9993 | `			/* Abort processing immeditaley */` |
|      117 |  9994 | `			goto Abort;` |
|    18590 |  9995 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  9996 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  9997 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  9998 | `			 * overwriting the state saved by the inner level.` |
|        - |  9999 | `			 * pTos points to the result slot (not yet written).` |
|        - | 10000 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       40 | 10001 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       40 | 10002 | `			goto Suspend;` |
|    18552 | 10003 | `		}else if( rc == PH7_EXCEPTION ){` |
|       39 | 10004 | `			goto Exception;` |
|        - | 10005 | `		}` |
|     9258 | 10006 | `	}else{` |
|        - | 10007 | `		ph7_user_func *pFunc;` |
|        - | 10008 | `		ph7_context sCtx;` |
|        - | 10009 | `		ph7_value sRet;` |
|        - | 10010 | `		/* Look for an installed foreign function.` |
|        - | 10011 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - | 10012 | `		 * If the compiler namespace-qualified the name, extract the short` |
|        - | 10013 | `		 * name (last component after \) and try that. This implements PHP's` |
|        - | 10014 | `		 * global fallback for unqualified function calls in namespaces. */` |
|   700008 | 10015 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|        - | 10016 | `		{` |
|   700008 | 10017 | `		VmCallArgMap *pCallMap2 = (VmCallArgMap *)pInstr->p3;` |
|   700008 | 10018 | `		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){` |
|        - | 10019 | `			/* Compiler-qualified: try short name as global fallback */` |
|       22 | 10020 | `			const char *zShort = sName.zString;` |
|        - | 10021 | `			sxu32 i;` |
|      334 | 10022 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      314 | 10023 | `				if( sName.zString[i] == '\\' ){` |
|       28 | 10024 | `					zShort = &sName.zString[i + 1];` |
|       13 | 10025 | `				}` |
|      158 | 10026 | `			}` |
|       22 | 10027 | `			if( zShort != sName.zString ){` |
|       22 | 10028 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       22 | 10029 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|       10 | 10030 | `			}` |
|       10 | 10031 | `		}` |
|        - | 10032 | `		} /* end VmCallArgMap namespace scope */` |
|   700008 | 10033 | `		if( pEntry == 0 ){` |
|        - | 10034 | `			/* Call to undefined function */` |
|        5 | 10035 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - | 10036 | `			/* Pop given arguments */` |
|        5 | 10037 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 | 10038 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 | 10039 | `			}` |
|        - | 10040 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 | 10041 | `			PH7_MemObjRelease(pTos);` |
|       58 | 10042 | `			break;` |
|        - | 10043 | `		}` |
|   700004 | 10044 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - | 10045 | `		/* Start collecting function arguments */` |
|   700004 | 10046 | `		SySetReset(&aArg);` |
|  1887406 | 10047 | `		while( pArg < pTos ){` |
|  1187404 | 10048 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  1187404 | 10049 | `			pArg++;` |
|        2 | 10050 | `		}` |
|        - | 10051 | `		/* Assume a null return value */` |
|   700004 | 10052 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - | 10053 | `		/* Init the call context */` |
|   700004 | 10054 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - | 10055 | `		/* Call the foreign function */` |
|   700004 | 10056 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - | 10057 | `		/* Release the call context */` |
|   700004 | 10058 | `		VmReleaseCallContext(&sCtx);` |
|   700004 | 10059 | `		if( rc == PH7_ABORT ){` |
|        - | 10060 | `			/* Release the (possibly partially-built) result slot before unwinding;` |
|        - | 10061 | `			 * the Abort: label only frees the operand stack, not this local` |
|        - | 10062 | `			 * (mirrors the PH7_EXCEPTION branch below). */` |
|      531 | 10063 | `			PH7_MemObjRelease(&sRet);` |
|      531 | 10064 | `			goto Abort;` |
|   699474 | 10065 | `		}else if( rc == PH7_EXCEPTION ){` |
|      112 | 10066 | `			VmFrame *pFrm = pVm->pFrame;` |
|      112 | 10067 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|      112 | 10068 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - | 10069 | `				/* Exception was NOT caught, propagate */` |
|        5 | 10070 | `				goto Exception;` |
|        - | 10071 | `			}` |
|        - | 10072 | `			/* Exception was caught: pop args and the result slot */` |
|      108 | 10073 | `			PH7_MemObjRelease(&sRet);` |
|      108 | 10074 | `			if( pInstr->iP1 > 0 ){` |
|       92 | 10075 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       45 | 10076 | `			}` |
|        - | 10077 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|      108 | 10078 | `			VmPopOperand(&pTos,1);` |
|        - | 10079 | `			/* Jump past the try/catch block via the exception frame */` |
|      108 | 10080 | `			pFrm = pVm->pFrame;` |
|      108 | 10081 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|      108 | 10082 | `				pc = pFrm->iExceptionJump - 1;` |
|       53 | 10083 | `			}` |
|      108 | 10084 | `			break;` |
|        - | 10085 | `		}` |
|   699364 | 10086 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - | 10087 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - | 10088 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - | 10089 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - | 10090 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - | 10091 | `			 * and we need to save state here. If it's a nested call (method` |
|        - | 10092 | `			 * body), the user-function path above will handle re-saving. */` |
|       40 | 10093 | `			PH7_MemObjRelease(&sRet);` |
|       40 | 10094 | `			if( pInstr->iP1 > 0 ){` |
|       40 | 10095 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 | 10096 | `			}` |
|        - | 10097 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - | 10098 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       40 | 10099 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       40 | 10100 | `			goto Suspend;` |
|        - | 10101 | `		}` |
|   699326 | 10102 | `		if( pInstr->iP1 > 0 ){` |
|        - | 10103 | `			/* Pop function name and arguments */` |
|   677220 | 10104 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   338631 | 10105 | `		}` |
|        - | 10106 | `		/* Save foreign function return value */` |
|   699326 | 10107 | `		PH7_MemObjStore(&sRet,pTos);` |
|   699326 | 10108 | `		PH7_MemObjRelease(&sRet);` |
|        - | 10109 | `	}` |
|   717838 | 10110 | `	break;` |
|        - | 10111 | `				  }` |
|        - | 10112 | `/*` |
|        - | 10113 | ` * OP_CONSUME: P1 * *` |
|        - | 10114 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - | 10115 | ` */` |
|    16073 | 10116 | `case PH7_OP_CONSUME: {` |
|    32148 | 10117 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    32148 | 10118 | `	ph7_value *pCur,*pOut = pTos;` |
|        - | 10119 |  |
|    32148 | 10120 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    32148 | 10121 | `	pCur = pOut;` |
|        - | 10122 | `	/* Start the consume process  */` |
|    64336 | 10123 | `	while( pOut <= pTos ){` |
|        - | 10124 | `		/* Force a string cast */` |
|    32190 | 10125 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1052 | 10126 | `			PH7_MemObjToString(pOut);` |
|      525 | 10127 | `		}` |
|    32190 | 10128 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - | 10129 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - | 10130 | `			/* Invoke the output consumer callback */` |
|    19718 | 10131 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    19718 | 10132 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    19718 | 10133 | `			SyBlobRelease(&pOut->sBlob);` |
|    19718 | 10134 | `			if( rc == SXERR_ABORT ){` |
|        - | 10135 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 | 10136 | `				goto Abort;` |
|        - | 10137 | `			}` |
|     9858 | 10138 | `		}` |
|    32190 | 10139 | `		pOut++;` |
|        2 | 10140 | `	}` |
|    32148 | 10141 | `	pTos = &pCur[-1];` |
|    32146 | 10142 | `	break;` |
|        - | 10143 | `					 }` |
|        - | 10144 |  |
|        - | 10145 | `		} /* Switch() */` |
| 11805700 | 10146 | `		pc++; /* Next instruction in the stream */` |
|        2 | 10147 | `	} /* For(;;) */` |
|    22310 | 10148 | `Done:` |
|    44622 | 10149 | `	SySetRelease(&aArg);` |
|    44622 | 10150 | `	return SXRET_OK;` |
|       72 | 10151 | `Suspend:` |
|      146 | 10152 | `	SySetRelease(&aArg);` |
|      146 | 10153 | `	return PH7_SUSPEND;` |
|      349 | 10154 | `Abort:` |
|      699 | 10155 | `	SySetRelease(&aArg);` |
|     2185 | 10156 | `	while( pTos >= pStack ){` |
|     1487 | 10157 | `		PH7_MemObjRelease(pTos);` |
|     1487 | 10158 | `		pTos--;` |
|        1 | 10159 | `	}` |
|      699 | 10160 | `	return PH7_ABORT;` |
|       32 | 10161 | `Exception:` |
|       66 | 10162 | `	SySetRelease(&aArg);` |
|      118 | 10163 | `	while( pTos >= pStack ){` |
|       54 | 10164 | `		PH7_MemObjRelease(pTos);` |
|       54 | 10165 | `		pTos--;` |
|        2 | 10166 | `	}` |
|       66 | 10167 | `	return PH7_EXCEPTION;` |
|    22765 | 10168 |  |
|        - | 10169 | `/*` |
|        - | 10170 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - | 10171 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - | 10172 | ` * See block-comment on that function for additional information.` |
|        - | 10173 | ` */` |
|    20782 | 10174 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 | 10175 |  |
|        - | 10176 | `	ph7_value *pStack;` |
|        - | 10177 | `	sxi32 rc;` |
|        - | 10178 | `	/* Allocate a new operand stack */` |
|    20784 | 10179 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    20784 | 10180 | `	if( pStack == 0 ){` |
|      ! 0 | 10181 | `		return SXERR_MEM;` |
|        - | 10182 | `	}` |
|        - | 10183 | `	/* Execute the program */` |
|    20784 | 10184 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0,0);` |
|        - | 10185 | `	/* Free the operand stack */` |
|    20784 | 10186 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - | 10187 | `	/* Execution result */` |
|    20784 | 10188 | `	return rc;` |
|    10393 | 10189 |  |
|        - | 10190 | `/*` |
|        - | 10191 | ` * Invoke any installed shutdown callbacks.` |
|        - | 10192 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - | 10193 | ` * or more calls to [register_shutdown_function()].` |
|        - | 10194 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - | 10195 | ` * execution ends.` |
|        - | 10196 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - | 10197 | ` * additional information.` |
|        - | 10198 | ` */` |
|     2840 | 10199 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 | 10200 |  |
|        - | 10201 | `	VmShutdownCB *pEntry;` |
|        - | 10202 | `	ph7_value *apArg[10];` |
|        - | 10203 | `	sxu32 n,nEntry;` |
|        - | 10204 | `	int i;` |
|        - | 10205 | `	/* Point to the stack of registered callbacks */` |
|     2842 | 10206 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    31242 | 10207 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    28402 | 10208 | `		apArg[i] = 0;` |
|    14202 | 10209 | `	}` |
|        - | 10210 | `	/* A halt that led us here is consumed; a fresh one set by a callback` |
|        - | 10211 | `	 * (i.e. exit() inside a shutdown function) skips the remaining` |
|        - | 10212 | `	 * callbacks, mirroring PHP.` |
|        - | 10213 | `	 */` |
|     2842 | 10214 | `	pVm->bHaltRequested = 0;` |
|     2854 | 10215 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       14 | 10216 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       14 | 10217 | `		if( pEntry ){` |
|        - | 10218 | `			/* Prepare callback arguments if any */` |
|       14 | 10219 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 | 10220 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 | 10221 | `					break;` |
|        - | 10222 | `				}` |
|      ! 0 | 10223 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 | 10224 | `			}` |
|        - | 10225 | `			/* Invoke the callback */` |
|       14 | 10226 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - | 10227 | `			/*` |
|        - | 10228 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - | 10229 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - | 10230 | `			 */` |
|       14 | 10231 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       14 | 10232 | `			if( pEntry ){` |
|       14 | 10233 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|       14 | 10234 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 | 10235 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 | 10236 | `				}` |
|        6 | 10237 | `			}` |
|       14 | 10238 | `			if( pVm->bHaltRequested ){` |
|        - | 10239 | `				/* exit() inside the callback: skip the remaining callbacks */` |
|      ! 0 | 10240 | `				break;` |
|        - | 10241 | `			}` |
|        6 | 10242 | `		}` |
|        8 | 10243 | `	}` |
|     2842 | 10244 | `	SySetReset(&pVm->aShutdown);` |
|     2842 | 10245 |  |
|        - | 10246 | `/*` |
|        - | 10247 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - | 10248 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - | 10249 | ` * See block-comment on that function for additional information.` |
|        - | 10250 | ` */` |
|     2840 | 10251 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 | 10252 |  |
|        - | 10253 | `	/* Make sure we are ready to execute this program */` |
|     2842 | 10254 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 | 10255 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - | 10256 | `	}` |
|        - | 10257 | `	/* Set the execution magic number  */` |
|     2842 | 10258 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - | 10259 | `	/* Execute the program */` |
|     2842 | 10260 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0,0);` |
|        - | 10261 | `	/* Invoke any shutdown callbacks */` |
|     2842 | 10262 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - | 10263 | `	/*` |
|        - | 10264 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - | 10265 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - | 10266 | `	 * [ph7_vm_reset()] first would fail.` |
|        - | 10267 | `	 */` |
|     2842 | 10268 | `	return SXRET_OK;` |
|     1422 | 10269 |  |
|        - | 10270 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - | 10271 | `/*` |
|        - | 10272 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - | 10273 | ` * The context is in CREATED state and ready to be started.` |
|        - | 10274 | ` */` |
|       50 | 10275 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        2 | 10276 |  |
|        - | 10277 | `	ph7_exec_ctx *pCtx;` |
|        - | 10278 | `	ph7_value *pStack;` |
|        - | 10279 | `	VmFrame *pFrame;` |
|       52 | 10280 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       52 | 10281 | `	if( pCtx == 0 ){` |
|      ! 0 | 10282 | `		return 0;` |
|        - | 10283 | `	}` |
|       52 | 10284 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       52 | 10285 | `	pCtx->pVm = pVm;` |
|       52 | 10286 | `	pCtx->pFunc = pFunc;` |
|       52 | 10287 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       52 | 10288 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       52 | 10289 | `	pCtx->pc = 0;` |
|       52 | 10290 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       52 | 10291 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - | 10292 | `	/* Allocate a private operand stack */` |
|       52 | 10293 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       52 | 10294 | `	if( pStack == 0 ){` |
|      ! 0 | 10295 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 | 10296 | `		return 0;` |
|        - | 10297 | `	}` |
|       52 | 10298 | `	pCtx->pStack = pStack;` |
|        - | 10299 | `	/* Create a detached frame for the fiber */` |
|       52 | 10300 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       52 | 10301 | `	if( pFrame == 0 ){` |
|      ! 0 | 10302 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 | 10303 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 | 10304 | `		return 0;` |
|        - | 10305 | `	}` |
|       52 | 10306 | `	pCtx->pFrame = pFrame;` |
|       52 | 10307 | `	return pCtx;` |
|       27 | 10308 |  |
|        - | 10309 | `/*` |
|        - | 10310 | ` * Start executing a fiber context for the first time.` |
|        - | 10311 | ` */` |
|       46 | 10312 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        2 | 10313 |  |
|        - | 10314 | `	ph7_exec_ctx *pOldCtx;` |
|        - | 10315 | `	sxi32 rc;` |
|       48 | 10316 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10317 | `		return SXERR_INVALID;` |
|        - | 10318 | `	}` |
|        - | 10319 | `	/* Bound fiber/generator nesting under the same cap (each start adds a C` |
|        - | 10320 | `	 * frame); reject before mutating VM state so the abort is clean. */` |
|       48 | 10321 | `	if( VmRecursionExceeded(pVm) ){` |
|      ! 0 | 10322 | `		return VmRecursionFatal(pVm);` |
|        - | 10323 | `	}` |
|        - | 10324 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       48 | 10325 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       48 | 10326 | `	pVm->pFrame = pCtx->pFrame;` |
|        - | 10327 | `	/* Save and set the active context */` |
|       48 | 10328 | `	pOldCtx = pVm->pActiveCtx;` |
|       48 | 10329 | `	pVm->pActiveCtx = pCtx;` |
|       48 | 10330 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       48 | 10331 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       48 | 10332 | `	pVm->nRecursionDepth++;` |
|        - | 10333 | `	/* Execute from the beginning */` |
|       48 | 10334 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       23 | 10335 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0,` |
|       46 | 10336 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0);` |
|       48 | 10337 | `	pVm->nRecursionDepth--;` |
|        - | 10338 | `	/* Restore the previous context */` |
|       48 | 10339 | `	pVm->pActiveCtx = pOldCtx;` |
|       48 | 10340 | `	if( rc == PH7_SUSPEND ){` |
|        - | 10341 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       46 | 10342 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       46 | 10343 | `		pCtx->pFrame->pParent = 0;` |
|       46 | 10344 | `		if( pResult ){` |
|       24 | 10345 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 | 10346 | `		}` |
|       46 | 10347 | `		return SXRET_OK;` |
|        - | 10348 | `	}` |
|        - | 10349 | `	/* Detach frame */` |
|        3 | 10350 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        3 | 10351 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        3 | 10352 | `		pCtx->pFrame->pParent = 0;` |
|        1 | 10353 | `	}` |
|        3 | 10354 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10355 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10356 | `		return PH7_ABORT;` |
|        - | 10357 | `	}` |
|        3 | 10358 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10359 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10360 | `		return PH7_EXCEPTION;` |
|        - | 10361 | `	}` |
|        - | 10362 | `	/* Normal completion */` |
|        3 | 10363 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        3 | 10364 | `	if( pResult ){` |
|        3 | 10365 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 | 10366 | `	}` |
|        3 | 10367 | `	return SXRET_OK;` |
|       25 | 10368 |  |
|        - | 10369 | `/*` |
|        - | 10370 | ` * Resume a suspended fiber context.` |
|        - | 10371 | ` */` |
|       98 | 10372 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        2 | 10373 |  |
|        - | 10374 | `	ph7_exec_ctx *pOldCtx;` |
|        - | 10375 | `	sxi32 rc;` |
|      100 | 10376 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 | 10377 | `		return SXERR_INVALID;` |
|        - | 10378 | `	}` |
|        - | 10379 | `	/* Bound fiber/generator nesting under the same cap; reject before mutating` |
|        - | 10380 | `	 * VM state so the abort is clean. */` |
|      100 | 10381 | `	if( VmRecursionExceeded(pVm) ){` |
|      ! 0 | 10382 | `		return VmRecursionFatal(pVm);` |
|        - | 10383 | `	}` |
|        - | 10384 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - | 10385 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - | 10386 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|      100 | 10387 | `	if( pResumeValue ){` |
|       40 | 10388 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       21 | 10389 | `	}else{` |
|       62 | 10390 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - | 10391 | `	}` |
|      100 | 10392 | `	pCtx->nTos++;` |
|        - | 10393 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|      100 | 10394 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      100 | 10395 | `	pVm->pFrame = pCtx->pFrame;` |
|        - | 10396 | `	/* Save and set the active context */` |
|      100 | 10397 | `	pOldCtx = pVm->pActiveCtx;` |
|      100 | 10398 | `	pVm->pActiveCtx = pCtx;` |
|      100 | 10399 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|      100 | 10400 | `	pVm->nRecursionDepth++;` |
|        - | 10401 | `	/* Resume execution from saved PC */` |
|      100 | 10402 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       49 | 10403 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc,` |
|       98 | 10404 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0);` |
|      100 | 10405 | `	pVm->nRecursionDepth--;` |
|        - | 10406 | `	/* Restore the previous context */` |
|      100 | 10407 | `	pVm->pActiveCtx = pOldCtx;` |
|      100 | 10408 | `	if( rc == PH7_SUSPEND ){` |
|        - | 10409 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|       64 | 10410 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       64 | 10411 | `		pCtx->pFrame->pParent = 0;` |
|       64 | 10412 | `		if( pResult ){` |
|       18 | 10413 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 | 10414 | `		}` |
|       64 | 10415 | `		return SXRET_OK;` |
|        - | 10416 | `	}` |
|        - | 10417 | `	/* Detach frame */` |
|       38 | 10418 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       38 | 10419 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       38 | 10420 | `		pCtx->pFrame->pParent = 0;` |
|       18 | 10421 | `	}` |
|       38 | 10422 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10423 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10424 | `		return PH7_ABORT;` |
|        - | 10425 | `	}` |
|       38 | 10426 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10427 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10428 | `		return PH7_EXCEPTION;` |
|        - | 10429 | `	}` |
|        - | 10430 | `	/* Normal completion */` |
|       38 | 10431 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       38 | 10432 | `	if( pResult ){` |
|       20 | 10433 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 | 10434 | `	}` |
|       38 | 10435 | `	return SXRET_OK;` |
|       51 | 10436 |  |
|        - | 10437 | `/*` |
|        - | 10438 | ` * Release an execution context and all its resources.` |
|        - | 10439 | ` */` |
|        4 | 10440 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 | 10441 |  |
|        5 | 10442 | `	if( pCtx == 0 ){` |
|      ! 0 | 10443 | `		return;` |
|        - | 10444 | `	}` |
|        5 | 10445 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - | 10446 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 | 10447 | `		return;` |
|        - | 10448 | `	}` |
|        5 | 10449 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - | 10450 | `	/* Release values */` |
|        5 | 10451 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|        5 | 10452 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - | 10453 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|        5 | 10454 | `	if( pCtx->pFrame ){` |
|        - | 10455 | `		VmSlot *aSlot;` |
|        - | 10456 | `		sxu32 n;` |
|        - | 10457 | `		/* Free local variables */` |
|        5 | 10458 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|       11 | 10459 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|        7 | 10460 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|        4 | 10461 | `		}` |
|        - | 10462 | `		/* Remove local references */` |
|        5 | 10463 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|       11 | 10464 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|        7 | 10465 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|        4 | 10466 | `		}` |
|        5 | 10467 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|        5 | 10468 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|        5 | 10469 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|        5 | 10470 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|        5 | 10471 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|        5 | 10472 | `		pCtx->pFrame = 0;` |
|        2 | 10473 | `	}` |
|        - | 10474 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - | 10475 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - | 10476 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|        5 | 10477 | `	if( pCtx->pStack ){` |
|        5 | 10478 | `		if( pCtx->nTos >= 0 ){` |
|        5 | 10479 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|        9 | 10480 | `			while( pTos >= pCtx->pStack ){` |
|        5 | 10481 | `				PH7_MemObjRelease(pTos);` |
|        5 | 10482 | `				pTos--;` |
|        1 | 10483 | `			}` |
|        2 | 10484 | `		}` |
|        5 | 10485 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|        5 | 10486 | `		pCtx->pStack = 0;` |
|        2 | 10487 | `	}` |
|        - | 10488 | `	/* Free the context itself */` |
|        5 | 10489 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|        3 | 10490 |  |
|        - | 10491 | `/*` |
|        - | 10492 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - | 10493 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - | 10494 | ` */` |
|       90 | 10495 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        2 | 10496 |  |
|        - | 10497 | `	ph7_class_instance *pThis;` |
|        - | 10498 | `	SyString sAttr;` |
|        - | 10499 | `	ph7_value *pAttr;` |
|       92 | 10500 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10501 | `		return 0;` |
|        - | 10502 | `	}` |
|       92 | 10503 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       92 | 10504 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 | 10505 | `		return 0;` |
|        - | 10506 | `	}` |
|       92 | 10507 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       92 | 10508 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       92 | 10509 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       32 | 10510 | `		return 0;` |
|        - | 10511 | `	}` |
|       62 | 10512 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       47 | 10513 |  |
|        - | 10514 | `/*` |
|        - | 10515 | ` * Fiber::suspend($value = null) — static method.` |
|        - | 10516 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - | 10517 | ` */` |
|       38 | 10518 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10519 |  |
|       40 | 10520 | `	ph7_vm *pVm = pCtx->pVm;` |
|       40 | 10521 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 | 10522 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10523 | `			"Cannot suspend outside of a fiber");` |
|        - | 10524 | `	}` |
|       40 | 10525 | `	if( nArg > 0 ){` |
|       40 | 10526 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       21 | 10527 | `	}else{` |
|      ! 0 | 10528 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - | 10529 | `	}` |
|       40 | 10530 | `	return PH7_SUSPEND;` |
|       21 | 10531 |  |
|        - | 10532 | `/*` |
|        - | 10533 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - | 10534 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - | 10535 | ` * and closure-environment binding happen with the correct argument context.` |
|        - | 10536 | ` */` |
|       24 | 10537 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10538 |  |
|        - | 10539 | `	ph7_class_instance *pThis;` |
|        - | 10540 | `	ph7_value *pAttr;` |
|        - | 10541 | `	SyString sAttrName;` |
|       26 | 10542 | `	if( nArg < 2 ){` |
|      ! 0 | 10543 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10544 | `			"Fiber::__construct() expects a callable argument");` |
|        - | 10545 | `	}` |
|       26 | 10546 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10547 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10548 | `			"Fiber::__construct(): invalid $this");` |
|        - | 10549 | `	}` |
|       26 | 10550 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       26 | 10551 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 | 10552 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10553 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - | 10554 | `	}` |
|        - | 10555 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       26 | 10556 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 | 10557 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10558 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - | 10559 | `	}` |
|        - | 10560 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       26 | 10561 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 | 10562 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 | 10563 | `	if( pAttr ){` |
|       26 | 10564 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 | 10565 | `	}` |
|       26 | 10566 | `	return PH7_OK;` |
|       14 | 10567 |  |
|        - | 10568 | `/*` |
|        - | 10569 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - | 10570 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - | 10571 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - | 10572 | ` * so that start() can bind it as $this for the closure environment.` |
|        - | 10573 | ` */` |
|       24 | 10574 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - | 10575 | `	ph7_class_instance **ppThis)` |
|        2 | 10576 |  |
|       26 | 10577 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10578 | `	ph7_value *pCallable;` |
|        - | 10579 | `	SyString sAttrName;` |
|       26 | 10580 | `	*ppThis = 0;` |
|       26 | 10581 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 | 10582 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       26 | 10583 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 | 10584 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 | 10585 | `		return 0;` |
|        - | 10586 | `	}` |
|       26 | 10587 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - | 10588 | `		/* String callable — look up in user functions with overload support */` |
|        - | 10589 | `		SyString sName;` |
|        - | 10590 | `		SyHashEntry *pEntry;` |
|        - | 10591 | `		ph7_vm_func *pFunc;` |
|       26 | 10592 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       26 | 10593 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       26 | 10594 | `		if( pEntry == 0 ){` |
|      ! 0 | 10595 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 | 10596 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 | 10597 | `			return 0;` |
|        - | 10598 | `		}` |
|       26 | 10599 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       26 | 10600 | `		return pFunc;` |
|      ! 0 | 10601 | `	}else{` |
|        - | 10602 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 | 10603 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 | 10604 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - | 10605 | `			sizeof("__invoke") - 1);` |
|      ! 0 | 10606 | `		if( pMethod == 0 ){` |
|      ! 0 | 10607 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10608 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 | 10609 | `			return 0;` |
|        - | 10610 | `		}` |
|      ! 0 | 10611 | `		*ppThis = pClosure;` |
|      ! 0 | 10612 | `		return &pMethod->sFunc;` |
|        - | 10613 | `	}` |
|       14 | 10614 |  |
|        - | 10615 | `/*` |
|        - | 10616 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - | 10617 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - | 10618 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - | 10619 | ` */` |
|       50 | 10620 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - | 10621 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        2 | 10622 |  |
|       52 | 10623 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - | 10624 | `	ph7_vm_func_arg *aFormalArg;` |
|        - | 10625 | `	sxu32 nFormal, n;` |
|        - | 10626 | `	VmSlot sSlot;` |
|        - | 10627 | `	sxi32 rc;` |
|        - | 10628 | `	/* Install $this for closure/method callables */` |
|       52 | 10629 | `	if( pClosureThis ){` |
|        - | 10630 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 | 10631 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 | 10632 | `		if( pObj ){` |
|      ! 0 | 10633 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 | 10634 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 | 10635 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 | 10636 | `		}` |
|      ! 0 | 10637 | `	}` |
|        - | 10638 | `	/* Install static variables */` |
|       52 | 10639 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - | 10640 | `		ph7_vm_func_static_var *aStatic;` |
|        - | 10641 | `		ph7_value *pVal;` |
|      ! 0 | 10642 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 | 10643 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 | 10644 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 | 10645 | `			if( pVal ){` |
|      ! 0 | 10646 | `				sSlot.pUserData = 0;` |
|      ! 0 | 10647 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 | 10648 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 | 10649 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 | 10650 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 | 10651 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal);` |
|      ! 0 | 10652 | `				}` |
|      ! 0 | 10653 | `			}` |
|      ! 0 | 10654 | `		}` |
|      ! 0 | 10655 | `	}` |
|        - | 10656 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       52 | 10657 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       52 | 10658 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       70 | 10659 | `	for( n = 0; n < nFormal; n++ ){` |
|        - | 10660 | `		ph7_value *pObj;` |
|       20 | 10661 | `		if( n < (sxu32)nArg ){` |
|        - | 10662 | `			/* Argument provided — install with type casting */` |
|       20 | 10663 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       20 | 10664 | `			if( pObj ){` |
|       20 | 10665 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - | 10666 | `				/* Type casting */` |
|       20 | 10667 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 | 10668 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 | 10669 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 10670 | `						if( xCast ){` |
|      ! 0 | 10671 | `							xCast(pObj);` |
|      ! 0 | 10672 | `						}` |
|      ! 0 | 10673 | `					}` |
|      ! 0 | 10674 | `				}` |
|       20 | 10675 | `				sSlot.nIdx = pObj->nIdx;` |
|       20 | 10676 | `				sSlot.pUserData = 0;` |
|       20 | 10677 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       11 | 10678 | `			}` |
|        9 | 10679 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - | 10680 | `			/* Default value */` |
|      ! 0 | 10681 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 | 10682 | `			if( pObj ){` |
|      ! 0 | 10683 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj);` |
|      ! 0 | 10684 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10685 | `					return rc;` |
|        - | 10686 | `				}` |
|      ! 0 | 10687 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 | 10688 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 | 10689 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 10690 | `						if( xCast ){` |
|      ! 0 | 10691 | `							xCast(pObj);` |
|      ! 0 | 10692 | `						}` |
|      ! 0 | 10693 | `					}` |
|      ! 0 | 10694 | `				}` |
|      ! 0 | 10695 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 | 10696 | `				sSlot.pUserData = 0;` |
|      ! 0 | 10697 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 | 10698 | `			}` |
|      ! 0 | 10699 | `		}` |
|       11 | 10700 | `	}` |
|        - | 10701 | `	/* Install closure environment (captured variables) */` |
|       52 | 10702 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - | 10703 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - | 10704 | `		ph7_value *pValue;` |
|        - | 10705 | `		sxu32 iEnv;` |
|        3 | 10706 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 | 10707 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 | 10708 | `			pEnv = &aEnv[iEnv];` |
|        7 | 10709 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 | 10710 | `				continue;` |
|        - | 10711 | `			}` |
|        5 | 10712 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 | 10713 | `			if( pValue == 0 ){` |
|      ! 0 | 10714 | `				continue;` |
|        - | 10715 | `			}` |
|        5 | 10716 | `			PH7_MemObjRelease(pValue);` |
|        5 | 10717 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 | 10718 | `		}` |
|        1 | 10719 | `	}` |
|       52 | 10720 | `	return SXRET_OK;` |
|       27 | 10721 |  |
|        - | 10722 | `/*` |
|        - | 10723 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - | 10724 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - | 10725 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - | 10726 | ` */` |
|       26 | 10727 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10728 |  |
|       28 | 10729 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10730 | `	ph7_class_instance *pThis;` |
|        - | 10731 | `	ph7_class_instance *pClosureThis;` |
|        - | 10732 | `	ph7_exec_ctx *pExecCtx;` |
|        - | 10733 | `	ph7_vm_func *pFunc;` |
|        - | 10734 | `	ph7_value sResult;` |
|        - | 10735 | `	ph7_value *pCtxAttr;` |
|        - | 10736 | `	SyString sAttrName;` |
|        - | 10737 | `	sxi32 rc;` |
|       28 | 10738 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10739 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - | 10740 | `	}` |
|       28 | 10741 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 10742 | `	/* Check if already started (has a __ctx) */` |
|       28 | 10743 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       28 | 10744 | `	if( pExecCtx != 0 ){` |
|        3 | 10745 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10746 | `			"Cannot start a fiber that has already been started");` |
|        - | 10747 | `	}` |
|        - | 10748 | `	/* Resolve callable */` |
|       26 | 10749 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       26 | 10750 | `	if( pFunc == 0 ){` |
|      ! 0 | 10751 | `		return PH7_EXCEPTION;` |
|        - | 10752 | `	}` |
|        - | 10753 | `	/* Create execution context now that we know the function */` |
|       26 | 10754 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       26 | 10755 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10756 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10757 | `			"Fiber::start(): out of memory");` |
|        - | 10758 | `	}` |
|        - | 10759 | `	/* Store context in $this->__ctx */` |
|       26 | 10760 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       26 | 10761 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 | 10762 | `	if( pCtxAttr ){` |
|       26 | 10763 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       26 | 10764 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 | 10765 | `	}` |
|        - | 10766 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - | 10767 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - | 10768 | `	 * into the fiber's frame, not the caller's. */` |
|       26 | 10769 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       26 | 10770 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - | 10771 | `	/* Unpack the args array and install into the frame */` |
|        - | 10772 | `	{` |
|       26 | 10773 | `		ph7_value **apValues = 0;` |
|       26 | 10774 | `		ph7_value *aStore = 0;` |
|       26 | 10775 | `		int nActual = 0;` |
|       26 | 10776 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       26 | 10777 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - | 10778 | `			ph7_hashmap_node *pNode;` |
|       26 | 10779 | `			sxu32 nCount = pMap->nEntry;` |
|       26 | 10780 | `			if( nCount > 0 ){` |
|        3 | 10781 | `				sxu32 idx = 0;` |
|        4 | 10782 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 | 10783 | `					nCount * sizeof(ph7_value *));` |
|        4 | 10784 | `				aStore = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 | 10785 | `					nCount * sizeof(ph7_value));` |
|        3 | 10786 | `				if( apValues && aStore ){` |
|        3 | 10787 | `					pNode = pMap->pFirst;` |
|        7 | 10788 | `					while( pNode && idx < nCount ){` |
|        - | 10789 | `						/* Snapshot each source into stable storage: VmFiberSetupFrame reserves` |
|        - | 10790 | `						 * memory objects (VmExtractMemObj) before reading the args, which can` |
|        - | 10791 | `						 * reallocate (move) pVm->aMemObj and dangle a raw pool pointer. A` |
|        - | 10792 | `						 * shallow copy is a safe source — the referent and the heap-resident` |
|        - | 10793 | `						 * blob data survive the move (same sSafeVal idiom the hashmap inserters` |
|        - | 10794 | `						 * use); it owns nothing independently, so it needs no release. */` |
|        5 | 10795 | `						ph7_value *pSrc = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 | 10796 | `						if( pSrc ){` |
|        5 | 10797 | `							aStore[idx] = *pSrc;` |
|        3 | 10798 | `						}else{` |
|      ! 0 | 10799 | `							PH7_MemObjInit(pVm, &aStore[idx]);` |
|        - | 10800 | `						}` |
|        5 | 10801 | `						apValues[idx] = &aStore[idx];` |
|        5 | 10802 | `						idx++;` |
|        5 | 10803 | `						pNode = pNode->pPrev;` |
|        1 | 10804 | `					}` |
|        3 | 10805 | `					nActual = (int)idx;` |
|        1 | 10806 | `				}` |
|        1 | 10807 | `			}` |
|       12 | 10808 | `		}` |
|       26 | 10809 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       26 | 10810 | `		if( aStore ){` |
|        3 | 10811 | `			SyMemBackendFree(&pVm->sAllocator, aStore);` |
|        1 | 10812 | `		}` |
|       26 | 10813 | `		if( apValues ){` |
|        3 | 10814 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 | 10815 | `		}` |
|        - | 10816 | `	}` |
|        - | 10817 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       26 | 10818 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       26 | 10819 | `	pExecCtx->pFrame->pParent = 0;` |
|       26 | 10820 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 10821 | `		return PH7_ABORT;` |
|        - | 10822 | `	}` |
|       26 | 10823 | `	PH7_MemObjInit(pVm, &sResult);` |
|       26 | 10824 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       26 | 10825 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10826 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10827 | `		return PH7_ABORT;` |
|        - | 10828 | `	}` |
|       26 | 10829 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10830 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10831 | `		return PH7_EXCEPTION;` |
|        - | 10832 | `	}` |
|       26 | 10833 | `	ph7_result_value(pCtx, &sResult);` |
|       26 | 10834 | `	PH7_MemObjRelease(&sResult);` |
|       26 | 10835 | `	return PH7_OK;` |
|       15 | 10836 |  |
|        - | 10837 | `/*` |
|        - | 10838 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - | 10839 | ` */` |
|       36 | 10840 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10841 |  |
|       38 | 10842 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10843 | `	ph7_exec_ctx *pExecCtx;` |
|        - | 10844 | `	ph7_value sResult;` |
|        - | 10845 | `	ph7_value *pResumeVal;` |
|        - | 10846 | `	sxi32 rc;` |
|       38 | 10847 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10848 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 | 10849 | `		return PH7_OK;` |
|        - | 10850 | `	}` |
|       38 | 10851 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       38 | 10852 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10853 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 | 10854 | `		return PH7_OK;` |
|        - | 10855 | `	}` |
|       38 | 10856 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 | 10857 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10858 | `			"Cannot resume a fiber that is not suspended");` |
|        - | 10859 | `	}` |
|       36 | 10860 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       36 | 10861 | `	PH7_MemObjInit(pVm, &sResult);` |
|       36 | 10862 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       36 | 10863 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10864 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10865 | `		return PH7_ABORT;` |
|        - | 10866 | `	}` |
|       36 | 10867 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10868 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10869 | `		return PH7_EXCEPTION;` |
|        - | 10870 | `	}` |
|       36 | 10871 | `	ph7_result_value(pCtx, &sResult);` |
|       36 | 10872 | `	PH7_MemObjRelease(&sResult);` |
|       36 | 10873 | `	return PH7_OK;` |
|       20 | 10874 |  |
|        - | 10875 | `/*` |
|        - | 10876 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - | 10877 | ` */` |
|        6 | 10878 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10879 |  |
|        8 | 10880 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10881 | `	ph7_exec_ctx *pExecCtx;` |
|        8 | 10882 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10883 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10884 | `		return PH7_OK;` |
|        - | 10885 | `	}` |
|        8 | 10886 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        8 | 10887 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10888 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10889 | `		return PH7_OK;` |
|        - | 10890 | `	}` |
|        8 | 10891 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 | 10892 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10893 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10894 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - | 10895 | `		}` |
|      ! 0 | 10896 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10897 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - | 10898 | `	}` |
|        8 | 10899 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        8 | 10900 | `	return PH7_OK;` |
|        5 | 10901 |  |
|        - | 10902 | `/*` |
|        - | 10903 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - | 10904 | ` */` |
|        6 | 10905 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10906 |  |
|        - | 10907 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 10908 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 10909 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 10910 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 | 10911 | `	return PH7_OK;` |
|        4 | 10912 |  |
|      ! 0 | 10913 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 10914 |  |
|        - | 10915 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 | 10916 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 | 10917 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 10918 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 | 10919 | `	return PH7_OK;` |
|      ! 0 | 10920 |  |
|        6 | 10921 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10922 |  |
|        - | 10923 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 10924 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 10925 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 10926 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 | 10927 | `	return PH7_OK;` |
|        4 | 10928 |  |
|        6 | 10929 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10930 |  |
|        - | 10931 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 10932 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 10933 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 10934 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 | 10935 | `	return PH7_OK;` |
|        4 | 10936 |  |
|        - | 10937 | `/*` |
|        - | 10938 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - | 10939 | ` */` |
|        4 | 10940 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10941 |  |
|        5 | 10942 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10943 | `	ph7_exec_ctx *pExecCtx;` |
|        5 | 10944 | `	if( nArg < 1 ){` |
|      ! 0 | 10945 | `		return PH7_OK;` |
|        - | 10946 | `	}` |
|        5 | 10947 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        5 | 10948 | `	if( pExecCtx ){` |
|        5 | 10949 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - | 10950 | `		/* Clear the attribute so double-free is prevented */` |
|        5 | 10951 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        5 | 10952 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 10953 | `			SyString sAttrName;` |
|        - | 10954 | `			ph7_value *pAttr;` |
|        5 | 10955 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|        5 | 10956 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|        5 | 10957 | `			if( pAttr ){` |
|        5 | 10958 | `				PH7_MemObjRelease(pAttr);` |
|        2 | 10959 | `			}` |
|        2 | 10960 | `		}` |
|        2 | 10961 | `	}` |
|        5 | 10962 | `	return PH7_OK;` |
|        3 | 10963 |  |
|        - | 10964 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 | 10965 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 | 10966 |  |
|        - | 10967 | `	ph7_class_instance *pThis;` |
|      ! 0 | 10968 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 | 10969 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 | 10970 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 | 10971 |  |
|      ! 0 | 10972 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 | 10973 |  |
|        - | 10974 | `	ph7_class_instance *pThis;` |
|      ! 0 | 10975 | `	ph7_class_instance *pClosureThis = 0;` |
|        - | 10976 | `	ph7_exec_ctx *pCtx;` |
|        - | 10977 | `	ph7_vm_func *pFunc;` |
|        - | 10978 | `	ph7_value *pCallable;` |
|        - | 10979 | `	ph7_value *pCtxAttr;` |
|        - | 10980 | `	SyString sAttrName;` |
|        - | 10981 | `	/* Must not already be started */` |
|      ! 0 | 10982 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10983 | `	if( pCtx != 0 ){` |
|      ! 0 | 10984 | `		return SXERR_INVALID;` |
|        - | 10985 | `	}` |
|      ! 0 | 10986 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10987 | `		return SXERR_INVALID;` |
|        - | 10988 | `	}` |
|      ! 0 | 10989 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - | 10990 | `	/* Get the callable */` |
|      ! 0 | 10991 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 | 10992 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 10993 | `	if( pCallable == 0 ){` |
|      ! 0 | 10994 | `		return SXERR_INVALID;` |
|        - | 10995 | `	}` |
|        - | 10996 | `	/* Resolve callable */` |
|      ! 0 | 10997 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - | 10998 | `		SyString sName;` |
|        - | 10999 | `		SyHashEntry *pEntry;` |
|      ! 0 | 11000 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 | 11001 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 | 11002 | `		if( pEntry == 0 ){` |
|      ! 0 | 11003 | `			return SXERR_NOTFOUND;` |
|        - | 11004 | `		}` |
|      ! 0 | 11005 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 | 11006 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 11007 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 | 11008 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - | 11009 | `			sizeof("__invoke") - 1);` |
|      ! 0 | 11010 | `		if( pMethod == 0 ){` |
|      ! 0 | 11011 | `			return SXERR_INVALID;` |
|        - | 11012 | `		}` |
|      ! 0 | 11013 | `		pClosureThis = pClosure;` |
|      ! 0 | 11014 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 | 11015 | `	}else{` |
|      ! 0 | 11016 | `		return SXERR_INVALID;` |
|        - | 11017 | `	}` |
|        - | 11018 | `	/* Create context */` |
|      ! 0 | 11019 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 | 11020 | `	if( pCtx == 0 ){` |
|      ! 0 | 11021 | `		return SXERR_MEM;` |
|        - | 11022 | `	}` |
|        - | 11023 | `	/* Store in __ctx */` |
|      ! 0 | 11024 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 | 11025 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 11026 | `	if( pCtxAttr ){` |
|      ! 0 | 11027 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 | 11028 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 | 11029 | `	}` |
|        - | 11030 | `	/* Set up frame with args */` |
|      ! 0 | 11031 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 | 11032 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 | 11033 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 | 11034 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 | 11035 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 | 11036 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 | 11037 |  |
|      ! 0 | 11038 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 | 11039 |  |
|      ! 0 | 11040 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 11041 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 | 11042 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 | 11043 |  |
|      ! 0 | 11044 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 11045 |  |
|      ! 0 | 11046 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 11047 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 | 11048 |  |
|      ! 0 | 11049 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 11050 |  |
|      ! 0 | 11051 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 11052 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 | 11053 |  |
|      ! 0 | 11054 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 11055 |  |
|      ! 0 | 11056 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 11057 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 | 11058 | `	return &pCtx->sRetValue;` |
|      ! 0 | 11059 |  |
|        - | 11060 | `/* ======================== Generator Infrastructure ======================== */` |
|        - | 11061 | `/*` |
|        - | 11062 | ` * Allocate a new generator wrapper around an execution context.` |
|        - | 11063 | ` */` |
|       26 | 11064 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        2 | 11065 |  |
|        - | 11066 | `	ph7_generator *pGen;` |
|       28 | 11067 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       28 | 11068 | `	if( pGen == 0 ){` |
|      ! 0 | 11069 | `		return 0;` |
|        - | 11070 | `	}` |
|       28 | 11071 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       28 | 11072 | `	pGen->pCtx = pCtx;` |
|       28 | 11073 | `	pGen->iImplicitKey = 0;` |
|       28 | 11074 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       28 | 11075 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - | 11076 | `	/* Link the generator back to the exec context */` |
|       28 | 11077 | `	pCtx->pPrivate = pGen;` |
|       28 | 11078 | `	return pGen;` |
|       15 | 11079 |  |
|        - | 11080 | `/*` |
|        - | 11081 | ` * Release a generator and its execution context.` |
|        - | 11082 | ` */` |
|      ! 0 | 11083 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 | 11084 |  |
|      ! 0 | 11085 | `	if( pGen == 0 ){` |
|      ! 0 | 11086 | `		return;` |
|        - | 11087 | `	}` |
|      ! 0 | 11088 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 | 11089 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 | 11090 | `	if( pGen->pCtx ){` |
|      ! 0 | 11091 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 | 11092 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 | 11093 | `		pGen->pCtx = 0;` |
|      ! 0 | 11094 | `	}` |
|      ! 0 | 11095 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 | 11096 |  |
|        - | 11097 | `/*` |
|        - | 11098 | ` * Extract ph7_generator from a Generator class instance.` |
|        - | 11099 | ` */` |
|      236 | 11100 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        2 | 11101 |  |
|        - | 11102 | `	ph7_class_instance *pThis;` |
|        - | 11103 | `	SyString sAttr;` |
|        - | 11104 | `	ph7_value *pAttr;` |
|      238 | 11105 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 11106 | `		return 0;` |
|        - | 11107 | `	}` |
|      238 | 11108 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      238 | 11109 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 | 11110 | `		return 0;` |
|        - | 11111 | `	}` |
|      238 | 11112 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      238 | 11113 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      238 | 11114 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 | 11115 | `		return 0;` |
|        - | 11116 | `	}` |
|      238 | 11117 | `	return (ph7_generator *)pAttr->x.pOther;` |
|      120 | 11118 |  |
|        - | 11119 | `/*` |
|        - | 11120 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - | 11121 | ` */` |
|       22 | 11122 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 11123 |  |
|        - | 11124 | `	ph7_generator *pGen;` |
|        - | 11125 | `	sxi32 rc;` |
|       24 | 11126 | `	if( nArg < 1 ) return PH7_OK;` |
|       24 | 11127 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       24 | 11128 | `	if( pGen == 0 ) return PH7_OK;` |
|       24 | 11129 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       24 | 11130 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       24 | 11131 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       24 | 11132 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       11 | 11133 | `	}` |
|       24 | 11134 | `	return PH7_OK;` |
|       13 | 11135 |  |
|        - | 11136 | `/*` |
|        - | 11137 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - | 11138 | ` */` |
|       68 | 11139 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 11140 |  |
|        - | 11141 | `	ph7_generator *pGen;` |
|       70 | 11142 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       70 | 11143 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 | 11144 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|       70 | 11145 | `	return PH7_OK;` |
|       36 | 11146 |  |
|        - | 11147 | `/*` |
|        - | 11148 | ` * Generator::current() — return the last yielded value.` |
|        - | 11149 | ` * Auto-starts the generator on first access (like PHP).` |
|        - | 11150 | ` */` |
|       68 | 11151 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 11152 |  |
|        - | 11153 | `	ph7_generator *pGen;` |
|        - | 11154 | `	sxi32 rc;` |
|       70 | 11155 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 | 11156 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 | 11157 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 | 11158 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 11159 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 | 11160 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 | 11161 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 | 11162 | `	}` |
|       70 | 11163 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       70 | 11164 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       36 | 11165 | `	}else{` |
|      ! 0 | 11166 | `		ph7_result_null(pCtx);` |
|        - | 11167 | `	}` |
|       70 | 11168 | `	return PH7_OK;` |
|       36 | 11169 |  |
|        - | 11170 | `/*` |
|        - | 11171 | ` * Generator::key() — return the last yielded key.` |
|        - | 11172 | ` * Auto-starts the generator on first access (like PHP).` |
|        - | 11173 | ` */` |
|       12 | 11174 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 11175 |  |
|        - | 11176 | `	ph7_generator *pGen;` |
|        - | 11177 | `	sxi32 rc;` |
|       13 | 11178 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 | 11179 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       13 | 11180 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 | 11181 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 11182 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 | 11183 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 | 11184 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 | 11185 | `	}` |
|       13 | 11186 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       13 | 11187 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|        7 | 11188 | `	}else{` |
|      ! 0 | 11189 | `		ph7_result_null(pCtx);` |
|        - | 11190 | `	}` |
|       13 | 11191 | `	return PH7_OK;` |
|        7 | 11192 |  |
|        - | 11193 | `/*` |
|        - | 11194 | ` * Generator::next() — advance to the next yield point.` |
|        - | 11195 | ` */` |
|       60 | 11196 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 11197 |  |
|        - | 11198 | `	ph7_generator *pGen;` |
|        - | 11199 | `	sxi32 rc;` |
|       62 | 11200 | `	if( nArg < 1 ) return PH7_OK;` |
|       62 | 11201 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       62 | 11202 | `	if( pGen == 0 ) return PH7_OK;` |
|       62 | 11203 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 11204 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       62 | 11205 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       62 | 11206 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       32 | 11207 | `	}else{` |
|      ! 0 | 11208 | `		return PH7_OK;` |
|        - | 11209 | `	}` |
|       62 | 11210 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       62 | 11211 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       62 | 11212 | `	return PH7_OK;` |
|       32 | 11213 |  |
|        - | 11214 | `/*` |
|        - | 11215 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - | 11216 | ` */` |
|        4 | 11217 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 11218 |  |
|        - | 11219 | `	ph7_generator *pGen;` |
|        - | 11220 | `	ph7_value *pSendVal;` |
|        - | 11221 | `	sxi32 rc;` |
|        5 | 11222 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 | 11223 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 | 11224 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 | 11225 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 | 11226 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - | 11227 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 | 11228 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 | 11229 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 | 11230 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 | 11231 | `	}else{` |
|      ! 0 | 11232 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11233 | `		return PH7_OK;` |
|        - | 11234 | `	}` |
|        5 | 11235 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 | 11236 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 | 11237 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 | 11238 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 | 11239 | `	}else{` |
|        3 | 11240 | `		ph7_result_null(pCtx);` |
|        - | 11241 | `	}` |
|        5 | 11242 | `	return PH7_OK;` |
|        3 | 11243 |  |
|        - | 11244 | `/*` |
|        - | 11245 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - | 11246 | ` *` |
|        - | 11247 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - | 11248 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - | 11249 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - | 11250 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - | 11251 | ` * the exception to the caller.` |
|        - | 11252 | ` */` |
|      ! 0 | 11253 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 11254 |  |
|        - | 11255 | `	ph7_generator *pGen;` |
|        - | 11256 | `	const char *zMsg;` |
|        - | 11257 | `	int nLen;` |
|      ! 0 | 11258 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 | 11259 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 11260 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 | 11261 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 | 11262 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 | 11263 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - | 11264 | `			"Cannot throw into a closed generator");` |
|        - | 11265 | `	}` |
|        - | 11266 | `	/* Close the generator. Re-throw the exception properly via` |
|        - | 11267 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - | 11268 | `	 * exception dispatch path works correctly. Extract the message` |
|        - | 11269 | `	 * from the passed exception object if possible. */` |
|      ! 0 | 11270 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 11271 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 | 11272 | `	nLen = 0;` |
|      ! 0 | 11273 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - | 11274 | `		/* Try to get the exception's message */` |
|        - | 11275 | `		SyString sAttr;` |
|        - | 11276 | `		ph7_value *pMsgAttr;` |
|      ! 0 | 11277 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 | 11278 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 | 11279 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 | 11280 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 | 11281 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 | 11282 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 | 11283 | `		}` |
|      ! 0 | 11284 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 11285 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 | 11286 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 | 11287 | `	}` |
|      ! 0 | 11288 | `	(void)nLen;` |
|      ! 0 | 11289 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 | 11290 |  |
|        - | 11291 | `/*` |
|        - | 11292 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - | 11293 | ` */` |
|        2 | 11294 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 11295 |  |
|        - | 11296 | `	ph7_generator *pGen;` |
|        3 | 11297 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 | 11298 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 | 11299 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 | 11300 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 | 11301 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - | 11302 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - | 11303 | `	}` |
|        3 | 11304 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 | 11305 | `	return PH7_OK;` |
|        2 | 11306 |  |
|        - | 11307 | `/*` |
|        - | 11308 | ` * Generator::__destruct() — clean up.` |
|        - | 11309 | ` */` |
|      ! 0 | 11310 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 11311 |  |
|        - | 11312 | `	ph7_generator *pGen;` |
|      ! 0 | 11313 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 | 11314 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 11315 | `	if( pGen ){` |
|      ! 0 | 11316 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 | 11317 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 11318 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 11319 | `			SyString sAttrName;` |
|        - | 11320 | `			ph7_value *pAttr;` |
|      ! 0 | 11321 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 | 11322 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 11323 | `			if( pAttr ){` |
|      ! 0 | 11324 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 | 11325 | `			}` |
|      ! 0 | 11326 | `		}` |
|      ! 0 | 11327 | `	}` |
|      ! 0 | 11328 | `	return PH7_OK;` |
|      ! 0 | 11329 |  |
|        - | 11330 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - | 11331 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - | 11332 | `/*` |
|        - | 11333 | ` * Invoke the installed VM output consumer callback to consume` |
|        - | 11334 | ` * the desired message.` |
|        - | 11335 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - | 11336 | ` * in 'api.c' for additional information.` |
|        - | 11337 | ` */` |
|      370 | 11338 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - | 11339 | `	ph7_vm *pVm,      /* Target VM */` |
|        - | 11340 | `	SyString *pString /* Message to output */` |
|        - | 11341 | `	)` |
|        2 | 11342 |  |
|      372 | 11343 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      372 | 11344 | `	sxi32 rc = SXRET_OK;` |
|        - | 11345 | `	/* Call the output consumer */` |
|      372 | 11346 | `	if( pString->nByte > 0 ){` |
|      372 | 11347 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      372 | 11348 | `		VmTrackOutput(pVm, pString->nByte);` |
|      185 | 11349 | `	}` |
|      372 | 11350 | `	return rc;` |
|        2 | 11351 |  |
|        - | 11352 | `/*` |
|        - | 11353 | ` * Format a message and invoke the installed VM output consumer` |
|        - | 11354 | ` * callback to consume the formatted message.` |
|        - | 11355 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - | 11356 | ` * in 'api.c' for additional information.` |
|        - | 11357 | ` */` |
|        2 | 11358 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - | 11359 | `	ph7_vm *pVm,         /* Target VM */` |
|        - | 11360 | `	const char *zFormat, /* Formatted message to output */` |
|        - | 11361 | `	va_list ap           /* Variable list of arguments */` |
|        - | 11362 | `	)` |
|        1 | 11363 |  |
|        3 | 11364 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 | 11365 | `	sxi32 rc = SXRET_OK;` |
|        - | 11366 | `	SyBlob sWorker;` |
|        - | 11367 | `	/* Format the message and call the output consumer */` |
|        3 | 11368 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 | 11369 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 | 11370 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - | 11371 | `		/* Consume the formatted message */` |
|        3 | 11372 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 | 11373 | `	}` |
|        3 | 11374 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - | 11375 | `	/* Release the working buffer */` |
|        3 | 11376 | `	SyBlobRelease(&sWorker);` |
|        3 | 11377 | `	return rc;` |
|        1 | 11378 |  |
|        - | 11379 | `/*` |
|        - | 11380 | ` * Return a string representation of the given PH7 OP code.` |
|        - | 11381 | ` * This function never fail and always return a pointer` |
|        - | 11382 | ` * to a null terminated string.` |
|        - | 11383 | ` */` |
|       12 | 11384 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 | 11385 |  |
|       13 | 11386 | `	const char *zOp = "Unknown     ";` |
|       13 | 11387 | `	switch(nOp){` |
|        3 | 11388 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 | 11389 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 | 11390 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 | 11391 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 | 11392 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 | 11393 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 | 11394 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 | 11395 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 | 11396 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 | 11397 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 | 11398 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 | 11399 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 | 11400 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 | 11401 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 | 11402 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 | 11403 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 | 11404 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 | 11405 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 | 11406 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 | 11407 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 | 11408 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 | 11409 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 | 11410 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 | 11411 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 | 11412 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 | 11413 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 | 11414 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 | 11415 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 | 11416 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 | 11417 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 | 11418 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 | 11419 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 | 11420 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 | 11421 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 | 11422 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 | 11423 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 | 11424 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 | 11425 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 | 11426 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 | 11427 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 | 11428 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 | 11429 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 | 11430 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 | 11431 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 | 11432 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 | 11433 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 | 11434 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 | 11435 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 | 11436 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 | 11437 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 | 11438 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 | 11439 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 | 11440 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|      ! 0 | 11441 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 | 11442 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 | 11443 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 | 11444 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|      ! 0 | 11445 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|      ! 0 | 11446 | `	case PH7_OP_NULLSAFE_JMP:zOp = "NULLSAFE_JMP"; break;` |
|      ! 0 | 11447 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 | 11448 | `	case PH7_OP_FLAG_SPREAD:zOp = "FLAG_SPREAD"; break;` |
|      ! 0 | 11449 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 | 11450 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 | 11451 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 | 11452 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 | 11453 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 | 11454 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 | 11455 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 | 11456 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 | 11457 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 | 11458 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 | 11459 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 | 11460 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 | 11461 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 | 11462 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 | 11463 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 | 11464 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 | 11465 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 | 11466 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 | 11467 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 | 11468 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 | 11469 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 | 11470 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 | 11471 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 | 11472 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 | 11473 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 | 11474 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 | 11475 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 | 11476 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 | 11477 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 | 11478 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 | 11479 | `	case PH7_OP_MATCH:      zOp = "MATCH      "; break;` |
|      ! 0 | 11480 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 | 11481 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 | 11482 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 | 11483 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 | 11484 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 | 11485 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 | 11486 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 | 11487 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 | 11488 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 | 11489 | `	default:` |
|      ! 0 | 11490 | `		break;` |
|        - | 11491 | `	}` |
|       13 | 11492 | `	return zOp;` |
|        1 | 11493 |  |
|        - | 11494 | `/*` |
|        - | 11495 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - | 11496 | ` * The xConsumer() callback which is an used defined function` |
|        - | 11497 | ` * is responsible of consuming the generated dump.` |
|        - | 11498 | ` */` |
|        2 | 11499 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - | 11500 | `	ph7_vm *pVm,            /* Target VM */` |
|        - | 11501 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - | 11502 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - | 11503 | `	)` |
|        1 | 11504 |  |
|        - | 11505 | `	sxi32 rc;` |
|        3 | 11506 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 | 11507 | `	return rc;` |
|        1 | 11508 |  |
|        - | 11509 | `/*` |
|        - | 11510 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - | 11511 | ` * outside a class body [i.e: global or function scope].` |
|        - | 11512 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - | 11513 | ` * in 'compile.c' for additional information.` |
|        - | 11514 | ` */` |
|       14 | 11515 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 | 11516 |  |
|       15 | 11517 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - | 11518 | `	/* Evaluate and expand constant value */` |
|       15 | 11519 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|       15 | 11520 |  |
|        - | 11521 | `/*` |
|        - | 11522 | ` * Section:` |
|        - | 11523 | ` *  Function handling functions.` |
|        - | 11524 | ` * Status:` |
|        - | 11525 | ` *    Stable.` |
|        - | 11526 | ` */` |
|        - | 11527 | `/*` |
|        - | 11528 | ` * int func_num_args(void)` |
|        - | 11529 | ` *   Returns the number of arguments passed to the function.` |
|        - | 11530 | ` * Parameters` |
|        - | 11531 | ` *   None.` |
|        - | 11532 | ` * Return` |
|        - | 11533 | ` *  Total number of arguments passed into the current user-defined function` |
|        - | 11534 | ` *  or -1 if called from the globe scope.` |
|        - | 11535 | ` */` |
|      980 | 11536 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11537 |  |
|        - | 11538 | `	VmFrame *pFrame;` |
|        - | 11539 | `	ph7_vm *pVm;` |
|        - | 11540 | `	/* Point to the target VM */` |
|      982 | 11541 | `	pVm = pCtx->pVm;` |
|        - | 11542 | `	/* Current frame */` |
|      982 | 11543 | `	pFrame = pVm->pFrame;` |
|      982 | 11544 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      982 | 11545 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 | 11546 | `		SXUNUSED(nArg);` |
|      ! 0 | 11547 | `		SXUNUSED(apArg);` |
|        - | 11548 | `		/* Global frame,return -1 */` |
|      ! 0 | 11549 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 | 11550 | `		return SXRET_OK;` |
|        - | 11551 | `	}` |
|        - | 11552 | `	/* Total number of arguments passed to the enclosing function */` |
|      982 | 11553 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      982 | 11554 | `	ph7_result_int(pCtx,nArg);` |
|      982 | 11555 | `	return SXRET_OK;` |
|      492 | 11556 |  |
|        - | 11557 | `/*` |
|        - | 11558 | ` * value func_get_arg(int $arg_num)` |
|        - | 11559 | ` *   Return an item from the argument list.` |
|        - | 11560 | ` * Parameters` |
|        - | 11561 | ` *  Argument number(index start from zero).` |
|        - | 11562 | ` * Return` |
|        - | 11563 | ` *  Returns the specified argument or FALSE on error.` |
|        - | 11564 | ` */` |
|       22 | 11565 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11566 |  |
|       24 | 11567 | `	ph7_value *pObj = 0;` |
|       24 | 11568 | `	VmSlot *pSlot = 0;` |
|        - | 11569 | `	VmFrame *pFrame;` |
|        - | 11570 | `	ph7_vm *pVm;` |
|        - | 11571 | `	/* Point to the target VM */` |
|       24 | 11572 | `	pVm = pCtx->pVm;` |
|        - | 11573 | `	/* Current frame */` |
|       24 | 11574 | `	pFrame = pVm->pFrame;` |
|       24 | 11575 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 | 11576 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - | 11577 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 | 11578 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 | 11579 | `		ph7_result_bool(pCtx,0);` |
|        3 | 11580 | `		return SXRET_OK;` |
|        - | 11581 | `	}` |
|        - | 11582 | `	/* Extract the desired index */` |
|       21 | 11583 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 | 11584 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - | 11585 | `		/* Invalid index,return FALSE */` |
|      ! 0 | 11586 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11587 | `		return SXRET_OK;` |
|        - | 11588 | `	}` |
|        - | 11589 | `	/* Extract the desired argument */` |
|       21 | 11590 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 | 11591 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - | 11592 | `			/* Return the desired argument */` |
|       21 | 11593 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 | 11594 | `		}else{` |
|        - | 11595 | `			/* No such argument,return false */` |
|      ! 0 | 11596 | `			ph7_result_bool(pCtx,0);` |
|        - | 11597 | `		}` |
|       11 | 11598 | `	}else{` |
|        - | 11599 | `		/* CAN'T HAPPEN */` |
|      ! 0 | 11600 | `		ph7_result_bool(pCtx,0);` |
|        - | 11601 | `	}` |
|       21 | 11602 | `	return SXRET_OK;` |
|       13 | 11603 |  |
|        - | 11604 | `/*` |
|        - | 11605 | ` * array func_get_args_byref(void)` |
|        - | 11606 | ` *   Returns an array comprising a function's argument list.` |
|        - | 11607 | ` * Parameters` |
|        - | 11608 | ` *  None.` |
|        - | 11609 | ` * Return` |
|        - | 11610 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - | 11611 | ` *  member of the current user-defined function's argument list.` |
|        - | 11612 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 11613 | ` * NOTE:` |
|        - | 11614 | ` *  Arguments are returned to the array by reference.` |
|        - | 11615 | ` */` |
|        2 | 11616 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11617 |  |
|        - | 11618 | `	ph7_value *pArray;` |
|        - | 11619 | `	VmFrame *pFrame;` |
|        - | 11620 | `	VmSlot *aSlot;` |
|        - | 11621 | `	sxu32 n;` |
|        - | 11622 | `	/* Point to the current frame */` |
|        3 | 11623 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 | 11624 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 | 11625 | `	if( pFrame->pParent == 0 ){` |
|        - | 11626 | `		/* Global frame,return FALSE */` |
|      ! 0 | 11627 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 11628 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11629 | `		return SXRET_OK;` |
|        - | 11630 | `	}` |
|        - | 11631 | `	/* Create a new array */` |
|        3 | 11632 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11633 | `	if( pArray == 0 ){` |
|      ! 0 | 11634 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11635 | `		SXUNUSED(apArg);` |
|      ! 0 | 11636 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11637 | `		return SXRET_OK;` |
|        - | 11638 | `	}` |
|        - | 11639 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 | 11640 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 | 11641 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 | 11642 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 | 11643 | `	}` |
|        - | 11644 | `	/* Return the freshly created array */` |
|        3 | 11645 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11646 | `	return SXRET_OK;` |
|        2 | 11647 |  |
|        - | 11648 | `/*` |
|        - | 11649 | ` * array func_get_args(void)` |
|        - | 11650 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - | 11651 | ` * Parameters` |
|        - | 11652 | ` *  None.` |
|        - | 11653 | ` * Return` |
|        - | 11654 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - | 11655 | ` *  member of the current user-defined function's argument list.` |
|        - | 11656 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 11657 | ` */` |
|       88 | 11658 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11659 |  |
|       90 | 11660 | `	ph7_value *pObj = 0;` |
|        - | 11661 | `	ph7_value *pArray;` |
|        - | 11662 | `	VmFrame *pFrame;` |
|        - | 11663 | `	VmSlot *aSlot;` |
|        - | 11664 | `	sxu32 n;` |
|        - | 11665 | `	/* Point to the current frame */` |
|       90 | 11666 | `	pFrame = pCtx->pVm->pFrame;` |
|       90 | 11667 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       90 | 11668 | `	if( pFrame->pParent == 0 ){` |
|        - | 11669 | `		/* Global frame,return FALSE */` |
|      ! 0 | 11670 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 11671 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11672 | `		return SXRET_OK;` |
|        - | 11673 | `	}` |
|        - | 11674 | `	/* Create a new array */` |
|       90 | 11675 | `	pArray = ph7_context_new_array(pCtx);` |
|       90 | 11676 | `	if( pArray == 0 ){` |
|      ! 0 | 11677 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11678 | `		SXUNUSED(apArg);` |
|      ! 0 | 11679 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11680 | `		return SXRET_OK;` |
|        - | 11681 | `	}` |
|        - | 11682 | `	/* Start filling the array with the given arguments */` |
|       90 | 11683 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      222 | 11684 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 | 11685 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 | 11686 | `		if( pObj ){` |
|      134 | 11687 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 | 11688 | `		}` |
|       68 | 11689 | `	}` |
|        - | 11690 | `	/* Return the freshly created array */` |
|       90 | 11691 | `	ph7_result_value(pCtx,pArray);` |
|       90 | 11692 | `	return SXRET_OK;` |
|       46 | 11693 |  |
|        - | 11694 | `/*` |
|        - | 11695 | ` * bool function_exists(string $name)` |
|        - | 11696 | ` *  Return TRUE if the given function has been defined.` |
|        - | 11697 | ` * Parameters` |
|        - | 11698 | ` *  The name of the desired function.` |
|        - | 11699 | ` * Return` |
|        - | 11700 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - | 11701 | ` */` |
|     1748 | 11702 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11703 |  |
|        - | 11704 | `	const char *zName;` |
|        - | 11705 | `	ph7_vm *pVm;` |
|        - | 11706 | `	int nLen;` |
|        - | 11707 | `	int res;` |
|     1750 | 11708 | `	if( nArg < 1 ){` |
|        - | 11709 | `		/* Missing argument,return FALSE */` |
|      ! 0 | 11710 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11711 | `		return SXRET_OK;` |
|        - | 11712 | `	}` |
|        - | 11713 | `	/* Point to the target VM */` |
|     1750 | 11714 | `	pVm = pCtx->pVm;` |
|        - | 11715 | `	/* Extract the function name */` |
|     1750 | 11716 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 11717 | `	/* Assume the function is not defined */` |
|     1750 | 11718 | `	res = 0;` |
|        - | 11719 | `	/* Perform the lookup */` |
|     2622 | 11720 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1744 | 11721 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 11722 | `			/* Function is defined */` |
|      268 | 11723 | `			res = 1;` |
|      133 | 11724 | `	}` |
|     1750 | 11725 | `	ph7_result_bool(pCtx,res);` |
|     1750 | 11726 | `	return SXRET_OK;` |
|      876 | 11727 |  |
|        - | 11728 | `/*` |
|        - | 11729 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 11730 | ` * [i.e: Whether it is callable or not].` |
|        - | 11731 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - | 11732 | ` */` |
|    23982 | 11733 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 | 11734 |  |
|    23984 | 11735 | `	int res = 0;` |
|    23984 | 11736 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 11737 | `		/* PHP semantics: an object is callable iff its class declares __invoke` |
|        - | 11738 | `		 * (inherited methods count). The CallInvoke flag is unused — it` |
|        - | 11739 | `		 * formerly invoked __invoke as a runtime predicate, which is not` |
|        - | 11740 | `		 * standard PHP behavior. */` |
|       20 | 11741 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|       20 | 11742 | `		if( PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1) ){` |
|       18 | 11743 | `			res = 1;` |
|       10 | 11744 | `		}` |
|        9 | 11745 | `		(void)CallInvoke;` |
|    23975 | 11746 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 | 11747 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 | 11748 | `		if( pMap->nEntry == 2 ){` |
|        - | 11749 | `			ph7_class *pClass;` |
|        - | 11750 | `			ph7_value *pV;` |
|        - | 11751 | `			/* Extract the target class */` |
|       12 | 11752 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 | 11753 | `			if( pV ){` |
|       12 | 11754 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 | 11755 | `				if( pClass ){` |
|        - | 11756 | `					ph7_class_method *pMethod;` |
|        - | 11757 | `					/* Extract the target method */` |
|       10 | 11758 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 | 11759 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - | 11760 | `						/* Perform the lookup */` |
|       10 | 11761 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 | 11762 | `						if( pMethod ){` |
|        - | 11763 | `							/* Method is callable */` |
|        5 | 11764 | `							res = 1;` |
|        2 | 11765 | `						}` |
|        4 | 11766 | `					}` |
|        4 | 11767 | `				}` |
|        5 | 11768 | `			}` |
|        7 | 11769 | `		}` |
|    23953 | 11770 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - | 11771 | `		const char *zName;` |
|        - | 11772 | `		int nLen;` |
|        - | 11773 | `		/* Extract the name */` |
|     5896 | 11774 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - | 11775 | `		/* Perform the lookup */` |
|     5911 | 11776 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 | 11777 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 11778 | `				/* Function is callable */` |
|     5878 | 11779 | `				res = 1;` |
|     2938 | 11780 | `		}` |
|     2947 | 11781 | `	}` |
|    23984 | 11782 | `	return res;` |
|        2 | 11783 |  |
|        - | 11784 | `/*` |
|        - | 11785 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - | 11786 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 11787 | ` * Parameters` |
|        - | 11788 | ` * $name` |
|        - | 11789 | ` *    The callback function to check` |
|        - | 11790 | ` * $syntax_only` |
|        - | 11791 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - | 11792 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - | 11793 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - | 11794 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - | 11795 | ` *    a string.` |
|        - | 11796 | ` * Return` |
|        - | 11797 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - | 11798 | ` */` |
|       20 | 11799 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11800 |  |
|        - | 11801 | `	ph7_vm *pVm;` |
|        - | 11802 | `	int res;` |
|       21 | 11803 | `	if( nArg < 1 ){` |
|        - | 11804 | `		/* Missing arguments,return FALSE */` |
|      ! 0 | 11805 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11806 | `		return SXRET_OK;` |
|        - | 11807 | `	}` |
|        - | 11808 | `	/* Point to the target VM */` |
|       21 | 11809 | `	pVm = pCtx->pVm;` |
|        - | 11810 | `	/* Perform the requested operation */` |
|       21 | 11811 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       21 | 11812 | `	ph7_result_bool(pCtx,res);` |
|       21 | 11813 | `	return SXRET_OK;` |
|       11 | 11814 |  |
|        - | 11815 | `/*` |
|        - | 11816 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - | 11817 | ` * defined below.` |
|        - | 11818 | ` */` |
|     1306 | 11819 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 11820 |  |
|     1307 | 11821 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 11822 | `	ph7_value sName;` |
|        - | 11823 | `	sxi32 rc;` |
|        - | 11824 | `	/* Prepare the function name for insertion */` |
|     1307 | 11825 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1307 | 11826 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 11827 | `	/* Perform the insertion */` |
|     1307 | 11828 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1307 | 11829 | `	PH7_MemObjRelease(&sName);` |
|     1307 | 11830 | `	return rc;` |
|        1 | 11831 |  |
|        - | 11832 | `/*` |
|        - | 11833 | ` * array get_defined_functions(void)` |
|        - | 11834 | ` *  Returns an array of all defined functions.` |
|        - | 11835 | ` * Parameter` |
|        - | 11836 | ` *  None.` |
|        - | 11837 | ` * Return` |
|        - | 11838 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - | 11839 | ` *  both built-in (internal) and user-defined.` |
|        - | 11840 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - | 11841 | ` *  defined ones using $arr["user"].` |
|        - | 11842 | ` * Note:` |
|        - | 11843 | ` *  NULL is returned on failure.` |
|        - | 11844 | ` */` |
|        2 | 11845 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11846 |  |
|        - | 11847 | `	ph7_value *pArray,*pEntry;` |
|        - | 11848 | `	/* NOTE:` |
|        - | 11849 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - | 11850 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - | 11851 | `	 */` |
|        3 | 11852 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11853 | ` 	if( pArray == 0 ){` |
|      ! 0 | 11854 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11855 | `		SXUNUSED(apArg);` |
|        - | 11856 | `		/* Return NULL */` |
|      ! 0 | 11857 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11858 | `		return SXRET_OK;` |
|        - | 11859 | `	}` |
|        3 | 11860 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 11861 | `	if( pEntry == 0 ){` |
|        - | 11862 | `		/* Return NULL */` |
|      ! 0 | 11863 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11864 | `		return SXRET_OK;` |
|        - | 11865 | `	}` |
|        - | 11866 | `	/* Fill with the appropriate information */` |
|        3 | 11867 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - | 11868 | `	/* Create the 'internal' index */` |
|        3 | 11869 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - | 11870 | `	/* Create the user-func array */` |
|        3 | 11871 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 11872 | `	if( pEntry == 0 ){` |
|        - | 11873 | `		/* Return NULL */` |
|      ! 0 | 11874 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11875 | `		return SXRET_OK;` |
|        - | 11876 | `	}` |
|        - | 11877 | `	/* Fill with the appropriate information */` |
|        3 | 11878 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - | 11879 | `	/* Create the 'user' index */` |
|        3 | 11880 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - | 11881 | `	/* Return the multi-dimensional array */` |
|        3 | 11882 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11883 | `	return SXRET_OK;` |
|        2 | 11884 |  |
|        - | 11885 | `/*` |
|        - | 11886 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - | 11887 | ` *  Register a function for execution on shutdown.` |
|        - | 11888 | ` * Note` |
|        - | 11889 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - | 11890 | ` *  be called in the same order as they were registered.` |
|        - | 11891 | ` * Parameters` |
|        - | 11892 | ` *  $callback` |
|        - | 11893 | ` *   The shutdown callback to register.` |
|        - | 11894 | ` * $param` |
|        - | 11895 | ` *  One or more Parameter to pass to the registered callback.` |
|        - | 11896 | ` * Return` |
|        - | 11897 | ` *  Nothing.` |
|        - | 11898 | ` */` |
|       12 | 11899 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11900 |  |
|        - | 11901 | `	VmShutdownCB sEntry;` |
|        - | 11902 | `	int i,j;` |
|       14 | 11903 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 11904 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 | 11905 | `		return PH7_OK;` |
|        - | 11906 | `	}` |
|        - | 11907 | `	/* Zero the Entry */` |
|       14 | 11908 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - | 11909 | `	/* Initialize fields */` |
|       14 | 11910 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - | 11911 | `	/* Save the callback name for later invocation name */` |
|       14 | 11912 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|      134 | 11913 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|      122 | 11914 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       62 | 11915 | `	}` |
|        - | 11916 | `	/* Copy arguments */` |
|       14 | 11917 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 | 11918 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - | 11919 | `			/* Limit reached */` |
|      ! 0 | 11920 | `			break;` |
|        - | 11921 | `		}` |
|      ! 0 | 11922 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 | 11923 | `	}` |
|       14 | 11924 | `	sEntry.nArg = j;` |
|        - | 11925 | `	/* Install the callback */` |
|       14 | 11926 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|       14 | 11927 | `	return PH7_OK;` |
|        8 | 11928 |  |
|        - | 11929 | `/*` |
|        - | 11930 | ` * Section:` |
|        - | 11931 | ` *  Class handling functions.` |
|        - | 11932 | ` * Status:` |
|        - | 11933 | ` *    Stable.` |
|        - | 11934 | ` */` |
|        - | 11935 | `/*` |
|        - | 11936 | ` * Extract the top active class. NULL is returned` |
|        - | 11937 | ` * if the class stack is empty.` |
|        - | 11938 | ` */` |
|     1000 | 11939 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 | 11940 |  |
|     1002 | 11941 | `	SySet *pSet = &pVm->aSelf;` |
|        - | 11942 | `	ph7_class **apClass;` |
|     1002 | 11943 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - | 11944 | `		/* Empty stack,return NULL */` |
|       15 | 11945 | `		return 0;` |
|        - | 11946 | `	}` |
|        - | 11947 | `	/* Peek the last entry */` |
|      988 | 11948 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      988 | 11949 | `	return apClass[pSet->nUsed - 1];` |
|      502 | 11950 |  |
|        - | 11951 | `/*` |
|        - | 11952 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - | 11953 | ` *   Get the class that declared the currently executing method.` |
|        - | 11954 | ` *   This is used for resolving the 'self::' constant.` |
|        - | 11955 | ` *` |
|        - | 11956 | ` * Parameters` |
|        - | 11957 | ` *   pVm: Target VM` |
|        - | 11958 | ` *` |
|        - | 11959 | ` * Return` |
|        - | 11960 | ` *   The declaring class of the current method, or NULL if:` |
|        - | 11961 | ` *   - Not executing within a class method` |
|        - | 11962 | ` *` |
|        - | 11963 | ` * Note` |
|        - | 11964 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - | 11965 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - | 11966 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - | 11967 | ` *   This is found by walking the call frames to locate the method's` |
|        - | 11968 | ` *   declaring class.` |
|        - | 11969 | ` */` |
|       98 | 11970 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 | 11971 |  |
|      100 | 11972 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 11973 | `	ph7_vm_func *pVmFunc;` |
|        - | 11974 |  |
|        - | 11975 | `	/* Skip exception frames to find the actual method frame */` |
|      100 | 11976 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - | 11977 |  |
|        - | 11978 | `	/* Check if we're in a method context */` |
|      100 | 11979 | `	if( pFrame->pParent ){` |
|       96 | 11980 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       96 | 11981 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - | 11982 | `			/* Return the declaring class */` |
|       96 | 11983 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - | 11984 | `		}` |
|      ! 0 | 11985 | `	}` |
|        - | 11986 |  |
|        5 | 11987 | `	return 0;` |
|       51 | 11988 |  |
|        - | 11989 |  |
|        - | 11990 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - | 11991 | `/*` |
|        - | 11992 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - | 11993 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - | 11994 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - | 11995 | ` * return value indicates failure.` |
|        - | 11996 | ` */` |
|        - | 11997 | `/*` |
|        - | 11998 | ` * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap` |
|        - | 11999 | ` * through to the synthetic CALL instruction.  Used by the NEW handler so` |
|        - | 12000 | ` * that constructor calls with named arguments reach the named-arg path` |
|        - | 12001 | ` * (with variadic string-key packing) rather than the positional path.` |
|        - | 12002 | ` */` |
|     2496 | 12003 | `static sxi32 VmCallClassMethodWithMap(` |
|        - | 12004 | `	ph7_vm *pVm,` |
|        - | 12005 | `	ph7_class_instance *pThis,` |
|        - | 12006 | `	ph7_class_method *pMethod,` |
|        - | 12007 | `	ph7_value *pResult,` |
|        - | 12008 | `	int nArg,` |
|        - | 12009 | `	ph7_value **apArg,` |
|        - | 12010 | `	VmCallArgMap *pMap` |
|        - | 12011 | `	)` |
|        2 | 12012 |  |
|        - | 12013 | `	ph7_value *aStack;` |
|        - | 12014 | `	VmInstr aInstr[2];` |
|        - | 12015 | `	int iCursor;` |
|        - | 12016 | `	int i;` |
|        - | 12017 | `	sxi32 rc;` |
|     2498 | 12018 | `	aStack = VmNewOperandStack(&(*pVm),2+nArg);` |
|     2498 | 12019 | `	if( aStack == 0 ){` |
|      ! 0 | 12020 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 12021 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 | 12022 | `		return SXERR_MEM;` |
|        - | 12023 | `	}` |
|     4056 | 12024 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1560 | 12025 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|     1560 | 12026 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      781 | 12027 | `	}` |
|     2498 | 12028 | `	iCursor = nArg + 1;` |
|     2498 | 12029 | `	if( pThis ){` |
|     2492 | 12030 | `		pThis->iRef++;` |
|     2492 | 12031 | `		aStack[i].x.pOther = pThis;` |
|     2492 | 12032 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|     1245 | 12033 | `	}` |
|     2498 | 12034 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     2498 | 12035 | `	i++;` |
|     2498 | 12036 | `	SyBlobReset(&aStack[i].sBlob);` |
|     2498 | 12037 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     2498 | 12038 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     2498 | 12039 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     2498 | 12040 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     2498 | 12041 | `	aInstr[0].iP1 = nArg;` |
|     2498 | 12042 | `	aInstr[0].iP2 = 0;` |
|     2498 | 12043 | `	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */` |
|     2498 | 12044 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     2498 | 12045 | `	aInstr[1].iP1 = 1;` |
|     2498 | 12046 | `	aInstr[1].iP2 = 0;` |
|     2498 | 12047 | `	aInstr[1].p3  = 0;` |
|     2498 | 12048 | `	rc = VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0,0);` |
|     2498 | 12049 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|        - | 12050 | `	/* Propagate the real exec status (PH7_EXCEPTION / PH7_ABORT) so callers` |
|        - | 12051 | `	 * can unwind instead of continuing past a method that raised. */` |
|     2498 | 12052 | `	return rc;` |
|     1250 | 12053 |  |
|     1938 | 12054 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - | 12055 | `	ph7_vm *pVm,               /* Target VM */` |
|        - | 12056 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - | 12057 | `	ph7_class_method *pMethod, /* Method name */` |
|        - | 12058 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - | 12059 | `	int nArg,                  /* Total number of given arguments */` |
|        - | 12060 | `	ph7_value **apArg          /* Method arguments */` |
|        - | 12061 | `	)` |
|        2 | 12062 |  |
|     1940 | 12063 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);` |
|        2 | 12064 |  |
|        - | 12065 | `/*` |
|        - | 12066 | ` * Dispatch a call to an object's __invoke magic method, forwarding arguments` |
|        - | 12067 | ` * and the return value. Used by the PH7_OP_CALL object-callable branch and by` |
|        - | 12068 | ` * PH7_VmCallUserFunction so that $obj(...), call_user_func($obj, ...) and` |
|        - | 12069 | ` * call_user_func_array($obj, [...]) all reach __invoke uniformly.` |
|        - | 12070 | ` *` |
|        - | 12071 | ` * Visibility is intentionally not checked: PHP allows private/protected` |
|        - | 12072 | ` * __invoke to be invoked via $obj() from any scope, and PHL's existing` |
|        - | 12073 | ` * is_callable / closure-invoke paths follow the same rule.` |
|        - | 12074 | ` *` |
|        - | 12075 | ` * pMap forwards the call-site VmCallArgMap so named-argument resolution and` |
|        - | 12076 | ` * strict_types coercion work for $obj(...) the same way they do for normal` |
|        - | 12077 | ` * function calls. Pass 0 from C-API call sites (call_user_func and friends),` |
|        - | 12078 | ` * which receive arguments positionally and don't carry a strict-types context.` |
|        - | 12079 | ` *` |
|        - | 12080 | ` * Returns SXRET_OK on success, SXERR_INVALID if __invoke is missing.` |
|        - | 12081 | ` */` |
|      174 | 12082 | `static sxi32 VmCallObjectInvoke(` |
|        - | 12083 | `	ph7_vm *pVm,` |
|        - | 12084 | `	ph7_class_instance *pThis,` |
|        - | 12085 | `	int nArg,` |
|        - | 12086 | `	ph7_value **apArg,` |
|        - | 12087 | `	ph7_value *pResult,` |
|        - | 12088 | `	VmCallArgMap *pMap` |
|        - | 12089 | `	)` |
|        2 | 12090 |  |
|        - | 12091 | `	ph7_class_method *pMethod;` |
|      176 | 12092 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      176 | 12093 | `	if( pMethod == 0 ){` |
|       13 | 12094 | `		if( pResult ){` |
|       13 | 12095 | `			PH7_MemObjRelease(pResult);` |
|        6 | 12096 | `		}` |
|       13 | 12097 | `		return SXERR_INVALID;` |
|        - | 12098 | `	}` |
|      164 | 12099 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|       89 | 12100 |  |
|        - | 12101 | `/*` |
|        - | 12102 | ` * Raise a catchable Error("Object of type X is not callable") when an object` |
|        - | 12103 | ` * is invoked as a function but lacks __invoke. Mirrors the OP_THROW pattern` |
|        - | 12104 | ` * (vm.c PH7_OP_THROW): build the Error instance, mark the current frame as` |
|        - | 12105 | ` * throwing, dispatch via VmThrowException so the nearest try/catch can handle` |
|        - | 12106 | ` * it. Caller is responsible for the post-throw control flow (iExceptionJump` |
|        - | 12107 | ` * lookup or 'goto Exception').` |
|        - | 12108 | ` *` |
|        - | 12109 | ` * Returns the result of VmThrowException (SXRET_OK on handled exception,` |
|        - | 12110 | ` * SXERR_ABORT on abort), or SXERR_ABORT if the Error class itself cannot` |
|        - | 12111 | ` * be bootstrapped — in which case an uncaught fatal has already been` |
|        - | 12112 | ` * reported.` |
|        - | 12113 | ` */` |
|       12 | 12114 | `static sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis)` |
|        1 | 12115 |  |
|        - | 12116 | `	ph7_class *pErrorClass;` |
|       13 | 12117 | `	ph7_class_instance *pErrInst = 0;` |
|        - | 12118 | `	ph7_class_method *pCons;` |
|        - | 12119 | `	VmFrame *pThrowFrame;` |
|        - | 12120 | `	char zMsg[256];` |
|        - | 12121 | `	int nMsg;` |
|        - | 12122 | `	sxi32 rc;` |
|       25 | 12123 | `	nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - | 12124 | `		"Object of type %.*s is not callable",` |
|       12 | 12125 | `		(int)pThis->pClass->sName.nByte,` |
|       12 | 12126 | `		pThis->pClass->sName.zString);` |
|       13 | 12127 | `	pErrorClass = PH7_VmExtractClass(pVm,"Error",sizeof("Error")-1,TRUE,0);` |
|       13 | 12128 | `	if( pErrorClass ){` |
|       13 | 12129 | `		pErrInst = PH7_NewClassInstance(pVm,pErrorClass);` |
|        6 | 12130 | `	}` |
|       13 | 12131 | `	if( pErrInst == 0 ){` |
|        - | 12132 | `		/* Bootstrap failure: Error class is part of the built-in library and` |
|        - | 12133 | `		 * should always be available, so this branch is effectively unreachable.` |
|        - | 12134 | `		 * Degrade to an uncaught fatal report so the failure is at least` |
|        - | 12135 | `		 * visible to the user. */` |
|      ! 0 | 12136 | `		VmReportUncaughtException(pVm,"Error",5,zMsg,(sxu32)nMsg,0,0);` |
|      ! 0 | 12137 | `		return SXERR_ABORT;` |
|        - | 12138 | `	}` |
|       13 | 12139 | `	pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|       13 | 12140 | `	if( pCons ){` |
|        - | 12141 | `		ph7_value sArg;` |
|        - | 12142 | `		ph7_value *apMsg[1];` |
|        - | 12143 | `		SyString sMsgStr;` |
|       13 | 12144 | `		SyStringInitFromBuf(&sMsgStr,zMsg,(sxu32)nMsg);` |
|       13 | 12145 | `		PH7_MemObjInit(pVm,&sArg);` |
|       13 | 12146 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|       13 | 12147 | `		apMsg[0] = &sArg;` |
|       13 | 12148 | `		PH7_VmCallClassMethod(pVm,pErrInst,pCons,0,1,apMsg);` |
|       13 | 12149 | `		PH7_MemObjRelease(&sArg);` |
|        6 | 12150 | `	}` |
|        - | 12151 | `	/* Else: Error::__construct is part of the built-in library and should` |
|        - | 12152 | `	 * always be present; if it isn't, the thrown exception still surfaces` |
|        - | 12153 | `	 * with an empty getMessage() rather than crashing. */` |
|       13 | 12154 | `	pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|       13 | 12155 | `	if( pThrowFrame ){` |
|       13 | 12156 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|        6 | 12157 | `	}` |
|       13 | 12158 | `	rc = VmThrowException(pVm,pErrInst);` |
|       13 | 12159 | `	PH7_ClassInstanceUnref(pErrInst);` |
|       13 | 12160 | `	return rc;` |
|        7 | 12161 |  |
|        - | 12162 | `/*` |
|        - | 12163 | ` * Call a user defined or foreign function where the name of the function` |
|        - | 12164 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - | 12165 | ` * in the apArg[] array.` |
|        - | 12166 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 12167 | ` * return value indicates failure.` |
|        - | 12168 | ` */` |
|     1214 | 12169 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - | 12170 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 12171 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 12172 | `	int nArg,          /* Total number of given arguments */` |
|        - | 12173 | `	ph7_value **apArg, /* Callback arguments */` |
|        - | 12174 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - | 12175 | `	)` |
|        2 | 12176 |  |
|        - | 12177 | `	ph7_value *aStack;` |
|        - | 12178 | `	VmInstr aInstr[2];` |
|        - | 12179 | `	int i;` |
|     1216 | 12180 | `	if( pFunc->iFlags & MEMOBJ_OBJ ){` |
|        - | 12181 | `		/* Object callable: dispatch through __invoke when available.` |
|        - | 12182 | `		 * No VmCallArgMap: call_user_func / array_map / usort and the C API` |
|        - | 12183 | `		 * pass arguments positionally and inherit no strict-types context. */` |
|      140 | 12184 | `		return VmCallObjectInvoke(&(*pVm),` |
|       92 | 12185 | `			(ph7_class_instance *)pFunc->x.pOther,` |
|       46 | 12186 | `			nArg,apArg,pResult,0);` |
|        - | 12187 | `	}` |
|     1124 | 12188 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 12189 | `		/* Don't bother processing,it's invalid anyway */` |
|      511 | 12190 | `		if( pResult ){` |
|        - | 12191 | `			/* Assume a null return value */` |
|      ! 0 | 12192 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 12193 | `		}` |
|      511 | 12194 | `		return SXERR_INVALID;` |
|        - | 12195 | `	}` |
|      614 | 12196 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 12197 | `		/* Class method */` |
|       15 | 12198 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       15 | 12199 | `		ph7_class_method *pMethod = 0;` |
|       15 | 12200 | `		ph7_class_instance *pThis = 0;` |
|       15 | 12201 | `		ph7_class *pClass = 0;` |
|        - | 12202 | `		ph7_value *pValue;` |
|        - | 12203 | `		sxi32 rc;` |
|       15 | 12204 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - | 12205 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 | 12206 | `			if( pResult ){` |
|        - | 12207 | `				/* Assume a null return value */` |
|      ! 0 | 12208 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 12209 | `			}` |
|      ! 0 | 12210 | `			return SXRET_OK;` |
|        - | 12211 | `		}` |
|        - | 12212 | `		/* Extract the class name or an instance of it */` |
|       15 | 12213 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       15 | 12214 | `		if( pValue ){` |
|       15 | 12215 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        7 | 12216 | `		}` |
|       15 | 12217 | `		if( pClass == 0 ){` |
|        - | 12218 | `			/* No such class,return NULL */` |
|      ! 0 | 12219 | `			if( pResult ){` |
|      ! 0 | 12220 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 12221 | `			}` |
|      ! 0 | 12222 | `			return SXRET_OK;` |
|        - | 12223 | `		}` |
|       15 | 12224 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 12225 | `			/* Point to the class instance */` |
|        9 | 12226 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        4 | 12227 | `		}` |
|        - | 12228 | `		/* Try to extract the method */` |
|       15 | 12229 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       15 | 12230 | `		if( pValue ){` |
|       15 | 12231 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       22 | 12232 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        7 | 12233 | `					SyBlobLength(&pValue->sBlob));` |
|        7 | 12234 | `			}` |
|        7 | 12235 | `		}` |
|       15 | 12236 | `		if( pMethod == 0 ){` |
|        - | 12237 | `			/* No such method,return NULL */` |
|      ! 0 | 12238 | `			if( pResult ){` |
|      ! 0 | 12239 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 12240 | `			}` |
|      ! 0 | 12241 | `			return SXRET_OK;` |
|        - | 12242 | `		}` |
|        - | 12243 | `		/* Call the class method */` |
|       15 | 12244 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       15 | 12245 | `		return rc;` |
|        - | 12246 | `	}` |
|        - | 12247 | `	/* Create a new operand stack */` |
|      600 | 12248 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      600 | 12249 | `	if( aStack == 0 ){` |
|      ! 0 | 12250 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 12251 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 | 12252 | `		if( pResult ){` |
|        - | 12253 | `			/* Assume a null return value */` |
|      ! 0 | 12254 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 12255 | `		}` |
|      ! 0 | 12256 | `		return SXERR_MEM;` |
|        - | 12257 | `	}` |
|        - | 12258 | `	/* Fill the operand stack with the given arguments */` |
|     1902 | 12259 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1304 | 12260 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - | 12261 | `		/*` |
|        - | 12262 | `		 * Symisc eXtension:` |
|        - | 12263 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - | 12264 | `		 */` |
|     1304 | 12265 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      653 | 12266 | `	}` |
|        - | 12267 | `	/* Push the function name */` |
|      600 | 12268 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      600 | 12269 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - | 12270 | `	/* Emit the CALL istruction */` |
|      600 | 12271 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      600 | 12272 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      600 | 12273 | `	aInstr[0].iP2 = 0;` |
|      600 | 12274 | `	aInstr[0].p3  = 0;` |
|        - | 12275 | `	/* Emit the DONE instruction */` |
|      600 | 12276 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      600 | 12277 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      600 | 12278 | `	aInstr[1].iP2 = 0;` |
|      600 | 12279 | `	aInstr[1].p3  = 0;` |
|        - | 12280 | `	/* Execute the function body (if available) */` |
|        - | 12281 | `	{` |
|        - | 12282 | `		sxi32 rcExec;` |
|      600 | 12283 | `		rcExec = VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0,0);` |
|        - | 12284 | `		/* Clean up the mess left behind */` |
|      600 | 12285 | `		SyMemBackendFree(&pVm->sAllocator,aStack);` |
|        - | 12286 | `		/* Propagate PH7_EXCEPTION/PH7_ABORT so a callback that raised unwinds. */` |
|      600 | 12287 | `		return rcExec;` |
|        - | 12288 | `	}` |
|      609 | 12289 |  |
|        - | 12290 | `/*` |
|        - | 12291 | ` * Call a user defined or foreign function whith a varibale number` |
|        - | 12292 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - | 12293 | ` * parameter.` |
|        - | 12294 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 12295 | ` * return value indicates failure.` |
|        - | 12296 | ` */` |
|      240 | 12297 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - | 12298 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 12299 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 12300 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - | 12301 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - | 12302 | `	)` |
|        1 | 12303 |  |
|        - | 12304 | `	ph7_value *pArg;` |
|        - | 12305 | `	SySet aArg;` |
|        - | 12306 | `	va_list ap;` |
|        - | 12307 | `	sxi32 rc;` |
|      241 | 12308 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - | 12309 | `	/* Copy arguments one after one */` |
|      241 | 12310 | `	va_start(ap,pResult);` |
|      399 | 12311 | `	for(;;){` |
|      799 | 12312 | `		pArg = va_arg(ap,ph7_value *);` |
|      799 | 12313 | `		if( pArg == 0 ){` |
|      241 | 12314 | `			break;` |
|        - | 12315 | `		}` |
|      559 | 12316 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 | 12317 | `	}` |
|        - | 12318 | `	/* Call the core routine */` |
|      241 | 12319 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - | 12320 | `	/* Cleanup */` |
|      241 | 12321 | `	SySetRelease(&aArg);` |
|      241 | 12322 | `	return rc;` |
|        1 | 12323 |  |
|        - | 12324 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - | 12325 | `/*` |
|        - | 12326 | ` * bool defined(string $name)` |
|        - | 12327 | ` *  Checks whether a given named constant exists.` |
|        - | 12328 | ` * Parameter:` |
|        - | 12329 | ` *  Name of the desired constant.` |
|        - | 12330 | ` * Return` |
|        - | 12331 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - | 12332 | ` */` |
|       26 | 12333 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12334 |  |
|        - | 12335 | `	const char *zName;` |
|       28 | 12336 | `	int nLen = 0;` |
|       28 | 12337 | `	int res = 0;` |
|       28 | 12338 | `	if( nArg < 1 ){` |
|        - | 12339 | `		/* Missing constant name,return FALSE */` |
|      ! 0 | 12340 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 | 12341 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12342 | `		return SXRET_OK;` |
|        - | 12343 | `	}` |
|        - | 12344 | `	/* Extract constant name */` |
|       28 | 12345 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 12346 | `	/* Perform the lookup */` |
|       28 | 12347 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 12348 | `		/* Already defined */` |
|       26 | 12349 | `		res = 1;` |
|       12 | 12350 | `	}` |
|       28 | 12351 | `	ph7_result_bool(pCtx,res);` |
|       28 | 12352 | `	return SXRET_OK;` |
|       15 | 12353 |  |
|        - | 12354 | `/*` |
|        - | 12355 | ` * Constant expansion callback used by the [define()] function defined` |
|        - | 12356 | ` * below.` |
|        - | 12357 | ` */` |
|       16 | 12358 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 | 12359 |  |
|       18 | 12360 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - | 12361 | `	/* Expand constant value */` |
|       18 | 12362 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       18 | 12363 |  |
|        - | 12364 | `/*` |
|        - | 12365 | ` * bool define(string $constant_name,expression value)` |
|        - | 12366 | ` *  Defines a named constant at runtime.` |
|        - | 12367 | ` * Parameter:` |
|        - | 12368 | ` *  $constant_name` |
|        - | 12369 | ` *   The name of the constant` |
|        - | 12370 | ` *  $value` |
|        - | 12371 | ` *   Constant value` |
|        - | 12372 | ` * Return:` |
|        - | 12373 | ` *   TRUE on success,FALSE on failure.` |
|        - | 12374 | ` */` |
|       14 | 12375 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12376 |  |
|        - | 12377 | `	const char *zName;  /* Constant name */` |
|        - | 12378 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       16 | 12379 | `	int nLen = 0;       /* Name length */` |
|        - | 12380 | `	sxi32 rc;` |
|       16 | 12381 | `	if( nArg < 2 ){` |
|        - | 12382 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 | 12383 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 | 12384 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12385 | `		return SXRET_OK;` |
|        - | 12386 | `	}` |
|       16 | 12387 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 | 12388 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 | 12389 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12390 | `		return SXRET_OK;` |
|        - | 12391 | `	}` |
|        - | 12392 | `	/* Extract constant name */` |
|       16 | 12393 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       16 | 12394 | `	if( nLen < 1 ){` |
|      ! 0 | 12395 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 | 12396 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12397 | `		return SXRET_OK;` |
|        - | 12398 | `	}` |
|        - | 12399 | `	/* Duplicate constant value */` |
|       16 | 12400 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       16 | 12401 | `	if( pValue == 0 ){` |
|      ! 0 | 12402 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 12403 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12404 | `		return SXRET_OK;` |
|        - | 12405 | `	}` |
|        - | 12406 | `	/* Initialize the memory object */` |
|       16 | 12407 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - | 12408 | `	/* Register the constant */` |
|       16 | 12409 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       16 | 12410 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 12411 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 | 12412 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 12413 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12414 | `		return SXRET_OK;` |
|        - | 12415 | `	}` |
|        - | 12416 | `	/* Duplicate constant value */` |
|       16 | 12417 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       16 | 12418 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - | 12419 | `		/* Lower case the constant name */` |
|      ! 0 | 12420 | `		char *zCur = (char *)zName;` |
|      ! 0 | 12421 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 | 12422 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - | 12423 | `				/* UTF-8 stream */` |
|      ! 0 | 12424 | `				zCur++;` |
|      ! 0 | 12425 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 | 12426 | `					zCur++;` |
|      ! 0 | 12427 | `				}` |
|      ! 0 | 12428 | `				continue;` |
|        - | 12429 | `			}` |
|      ! 0 | 12430 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 | 12431 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 | 12432 | `				zCur[0] = (char)c;` |
|      ! 0 | 12433 | `			}` |
|      ! 0 | 12434 | `			zCur++;` |
|      ! 0 | 12435 | `		}` |
|        - | 12436 | `		/* Register the lowercase alias with its OWN value copy (not the same` |
|        - | 12437 | `		 * pValue) so the two entries don't share one object — otherwise freeing` |
|        - | 12438 | `		 * one on a later overwrite would dangle the other. */` |
|        - | 12439 | `		{` |
|      ! 0 | 12440 | `			ph7_value *pAlias = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|      ! 0 | 12441 | `			if( pAlias ){` |
|      ! 0 | 12442 | `				PH7_MemObjInit(pCtx->pVm,pAlias);` |
|      ! 0 | 12443 | `				PH7_MemObjStore(apArg[1],pAlias);` |
|      ! 0 | 12444 | `				ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pAlias);` |
|      ! 0 | 12445 | `			}` |
|        - | 12446 | `		}` |
|      ! 0 | 12447 | `	}` |
|        - | 12448 | `	/* All done,return TRUE */` |
|       16 | 12449 | `	ph7_result_bool(pCtx,1);` |
|       16 | 12450 | `	return SXRET_OK;` |
|        9 | 12451 |  |
|        - | 12452 | `/*` |
|        - | 12453 | ` * value constant(string $name)` |
|        - | 12454 | ` *  Returns the value of a constant` |
|        - | 12455 | ` * Parameter` |
|        - | 12456 | ` *  $name` |
|        - | 12457 | ` *    Name of the constant.` |
|        - | 12458 | ` * Return` |
|        - | 12459 | ` *  Constant value or NULL if not defined.` |
|        - | 12460 | ` */` |
|        8 | 12461 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12462 |  |
|        - | 12463 | `	SyHashEntry *pEntry;` |
|        - | 12464 | `	ph7_constant *pCons;` |
|        - | 12465 | `	const char *zName; /* Constant name */` |
|        - | 12466 | `	ph7_value sVal;    /* Constant value */` |
|        - | 12467 | `	int nLen;` |
|       10 | 12468 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 12469 | `		/* Invallid argument,return NULL */` |
|      ! 0 | 12470 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 | 12471 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12472 | `		return SXRET_OK;` |
|        - | 12473 | `	}` |
|        - | 12474 | `	/* Extract the constant name */` |
|       10 | 12475 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 12476 | `	/* Perform the query */` |
|       10 | 12477 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 | 12478 | `	if( pEntry == 0 ){` |
|        3 | 12479 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 | 12480 | `		ph7_result_null(pCtx);` |
|        3 | 12481 | `		return SXRET_OK;` |
|        - | 12482 | `	}` |
|        8 | 12483 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - | 12484 | `	/* Point to the structure that describe the constant */` |
|        8 | 12485 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - | 12486 | `	/* Extract constant value by calling it's associated callback */` |
|        8 | 12487 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - | 12488 | `	/* Return that value */` |
|        8 | 12489 | `	ph7_result_value(pCtx,&sVal);` |
|        - | 12490 | `	/* Cleanup */` |
|        8 | 12491 | `	PH7_MemObjRelease(&sVal);` |
|        8 | 12492 | `	return SXRET_OK;` |
|        6 | 12493 |  |
|        - | 12494 | `/*` |
|        - | 12495 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - | 12496 | ` * defined below.` |
|        - | 12497 | ` */` |
|      466 | 12498 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 12499 |  |
|      467 | 12500 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 12501 | `	ph7_value sName;` |
|        - | 12502 | `	sxi32 rc;` |
|        - | 12503 | `	/* Prepare the constant name for insertion */` |
|      467 | 12504 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      467 | 12505 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 12506 | `	/* Perform the insertion */` |
|      467 | 12507 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      467 | 12508 | `	PH7_MemObjRelease(&sName);` |
|      467 | 12509 | `	return rc;` |
|        1 | 12510 |  |
|        - | 12511 | `/*` |
|        - | 12512 | ` * array get_defined_constants(void)` |
|        - | 12513 | ` *  Returns an associative array with the names of all defined` |
|        - | 12514 | ` *  constants.` |
|        - | 12515 | ` * Parameters` |
|        - | 12516 | ` *  NONE.` |
|        - | 12517 | ` * Returns` |
|        - | 12518 | ` *  Returns the names of all the constants currently defined.` |
|        - | 12519 | ` */` |
|        2 | 12520 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12521 |  |
|        - | 12522 | `	ph7_value *pArray;` |
|        - | 12523 | `	/* Create the array first*/` |
|        3 | 12524 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 12525 | `	if( pArray == 0 ){` |
|      ! 0 | 12526 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12527 | `		SXUNUSED(apArg);` |
|        - | 12528 | `		/* Return NULL */` |
|      ! 0 | 12529 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12530 | `		return SXRET_OK;` |
|        - | 12531 | `	}` |
|        - | 12532 | `	/* Fill the array with the defined constants */` |
|        3 | 12533 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - | 12534 | `	/* Return the created array */` |
|        3 | 12535 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 12536 | `	return SXRET_OK;` |
|        2 | 12537 |  |
|        - | 12538 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - | 12539 | `/*` |
|        - | 12540 | ` * Section:` |
|        - | 12541 | ` *  Random numbers/string generators.` |
|        - | 12542 | ` * Status:` |
|        - | 12543 | ` *    Stable.` |
|        - | 12544 | ` */` |
|        - | 12545 | `/*` |
|        - | 12546 | ` * Generate a random 32-bit unsigned integer.` |
|        - | 12547 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - | 12548 | ` * implemented in src/sx/sxrand.c).` |
|        - | 12549 | ` */` |
|     2912 | 12550 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 | 12551 |  |
|        - | 12552 | `	sxu32 iNum;` |
|     2914 | 12553 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2914 | 12554 | `	return iNum;` |
|        2 | 12555 |  |
|        - | 12556 | `/*` |
|        - | 12557 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - | 12558 | ` * Note that the generated string is NOT null terminated.` |
|        - | 12559 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - | 12560 | ` * implemented in src/sx/sxrand.c).` |
|        - | 12561 | ` */` |
|   237084 | 12562 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 | 12563 |  |
|        - | 12564 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - | 12565 | `	int i;` |
|        - | 12566 | `	/* Generate a binary string first */` |
|   237086 | 12567 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - | 12568 | `	/* Turn the binary string into english based alphabet */` |
|  2608094 | 12569 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  2371010 | 12570 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|  1185506 | 12571 | `	 }` |
|   237086 | 12572 |  |
|        - | 12573 | `/*` |
|        - | 12574 | ` * int rand()` |
|        - | 12575 | ` * int mt_rand()` |
|        - | 12576 | ` * int rand(int $min,int $max)` |
|        - | 12577 | ` * int mt_rand(int $min,int $max)` |
|        - | 12578 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - | 12579 | ` * Parameter` |
|        - | 12580 | ` *  $min` |
|        - | 12581 | ` *    The lowest value to return (default: 0)` |
|        - | 12582 | ` *  $max` |
|        - | 12583 | ` *   The highest value to return (default: getrandmax())` |
|        - | 12584 | ` * Return` |
|        - | 12585 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - | 12586 | ` * Note:` |
|        - | 12587 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 12588 | ` *  by te SQLite3 library.` |
|        - | 12589 | ` */` |
|       20 | 12590 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12591 |  |
|        - | 12592 | `	sxu32 iNum;` |
|        - | 12593 | `	/* Generate the random number */` |
|       21 | 12594 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 | 12595 | `	if( nArg > 1 ){` |
|        - | 12596 | `		sxu32 iMin,iMax;` |
|        3 | 12597 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 | 12598 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 | 12599 | `		if( iMin < iMax ){` |
|        3 | 12600 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 | 12601 | `			if( iDiv > 0 ){` |
|        3 | 12602 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 | 12603 | `			}` |
|        1 | 12604 | `		}else if(iMax > 0 ){` |
|      ! 0 | 12605 | `			iNum %= iMax;` |
|      ! 0 | 12606 | `		}` |
|        1 | 12607 | `	}` |
|        - | 12608 | `	/* Return the number */` |
|       21 | 12609 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 | 12610 | `	return SXRET_OK;` |
|        1 | 12611 |  |
|        - | 12612 | `/*` |
|        - | 12613 | ` * int getrandmax(void)` |
|        - | 12614 | ` * int mt_getrandmax(void)` |
|        - | 12615 | ` * int rc4_getrandmax(void)` |
|        - | 12616 | ` *   Show largest possible random value` |
|        - | 12617 | ` * Return` |
|        - | 12618 | ` *  The largest possible random value returned by rand() which is in` |
|        - | 12619 | ` *  this implementation 0xFFFFFFFF.` |
|        - | 12620 | ` * Note:` |
|        - | 12621 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 12622 | ` *  by te SQLite3 library.` |
|        - | 12623 | ` */` |
|        4 | 12624 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12625 |  |
|        2 | 12626 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 | 12627 | `	SXUNUSED(apArg);` |
|        5 | 12628 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 | 12629 | `	return SXRET_OK;` |
|        1 | 12630 |  |
|        - | 12631 | `/*` |
|        - | 12632 | ` * string rand_str()` |
|        - | 12633 | ` * string rand_str(int $len)` |
|        - | 12634 | ` *  Generate a random string (English alphabet).` |
|        - | 12635 | ` * Parameter` |
|        - | 12636 | ` *  $len` |
|        - | 12637 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - | 12638 | ` * Return` |
|        - | 12639 | ` *   A pseudo random string.` |
|        - | 12640 | ` * Note:` |
|        - | 12641 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 12642 | ` *  by te SQLite3 library.` |
|        - | 12643 | ` *  This function is a symisc extension.` |
|        - | 12644 | ` */` |
|      120 | 12645 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12646 |  |
|        - | 12647 | `	char zString[1024];` |
|      122 | 12648 | `	int iLen = 0x10;` |
|      122 | 12649 | `	if( nArg > 0 ){` |
|        - | 12650 | `		/* Get the desired length */` |
|      122 | 12651 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 | 12652 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - | 12653 | `			/* Default length */` |
|        3 | 12654 | `			iLen = 0x10;` |
|        1 | 12655 | `		}` |
|       60 | 12656 | `	}` |
|        - | 12657 | `	/* Generate the random string */` |
|      122 | 12658 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - | 12659 | `	/* Return the generated string */` |
|      122 | 12660 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 | 12661 | `	return SXRET_OK;` |
|        2 | 12662 |  |
|        - | 12663 | `/*` |
|        - | 12664 | ` * Reject non-numeric values (array/object/resource and non-numeric strings)` |
|        - | 12665 | ` * the same way intdiv() does. Returns SXRET_OK if the value is acceptable as` |
|        - | 12666 | ` * an int (PHP coerces float and numeric string silently).` |
|        - | 12667 | ` */` |
|      488 | 12668 | `static int VmRandomCheckIntArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgPos,const char *zParamName)` |
|        1 | 12669 |  |
|      488 | 12670 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg)` |
|      483 | 12671 | `		\|\| ph7_value_is_resource(pArg) ){` |
|       10 | 12672 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12673 | `			"TypeError",` |
|        - | 12674 | `			"%s(): Argument #%d (%s) must be of type int, %s given",` |
|        3 | 12675 | `			zFunc,iArgPos,zParamName,` |
|        3 | 12676 | `			ph7_type_name(pArg)` |
|        - | 12677 | `			);` |
|        - | 12678 | `	}` |
|      483 | 12679 | `	if( ph7_value_is_string(pArg) ){` |
|        - | 12680 | `		int len;` |
|        9 | 12681 | `		const char *zStr = ph7_value_to_string(pArg, &len);` |
|        9 | 12682 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|        7 | 12683 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12684 | `				"TypeError",` |
|        - | 12685 | `				"%s(): Argument #%d (%s) must be of type int, string given",` |
|        2 | 12686 | `				zFunc,iArgPos,zParamName` |
|        - | 12687 | `				);` |
|        - | 12688 | `		}` |
|        2 | 12689 | `	}` |
|      479 | 12690 | `	return SXRET_OK;` |
|      245 | 12691 |  |
|        - | 12692 | `/*` |
|        - | 12693 | ` * int random_int(int $min, int $max)` |
|        - | 12694 | ` *  Generate a cryptographically secure pseudo-random integer in [$min, $max].` |
|        - | 12695 | ` *  Mirrors PHP 7.0+ random_int(). Uses the OS CSPRNG via SyOSCSPRNG().` |
|        - | 12696 | ` *  Distribution is uniform via rejection sampling against the smallest` |
|        - | 12697 | ` *  power-of-two mask covering the range.` |
|        - | 12698 | ` */` |
|      242 | 12699 | `static int vm_builtin_random_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12700 |  |
|        - | 12701 | `	sxi64 iMin,iMax;` |
|        - | 12702 | `	sxu64 uRange,uMask,uResult;` |
|        - | 12703 | `	unsigned int nAttempt;` |
|        - | 12704 | `	int rc;` |
|      243 | 12705 | `	if( nArg != 2 ){` |
|       10 | 12706 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12707 | `			"ArgumentCountError",` |
|        - | 12708 | `			"random_int() expects exactly 2 arguments, %d given",` |
|        3 | 12709 | `			nArg` |
|        - | 12710 | `			);` |
|        - | 12711 | `	}` |
|      237 | 12712 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_int",1,"$min");` |
|      237 | 12713 | `	if( rc != SXRET_OK ){ return rc; }` |
|      233 | 12714 | `	rc = VmRandomCheckIntArg(pCtx,apArg[1],"random_int",2,"$max");` |
|      233 | 12715 | `	if( rc != SXRET_OK ){ return rc; }` |
|      231 | 12716 | `	iMin = ph7_value_to_int64(apArg[0]);` |
|      231 | 12717 | `	iMax = ph7_value_to_int64(apArg[1]);` |
|      231 | 12718 | `	if( iMin > iMax ){` |
|        3 | 12719 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12720 | `			"ValueError",` |
|        - | 12721 | `			"random_int(): Argument #1 ($min) must be less than or equal to argument #2 ($max)"` |
|        - | 12722 | `			);` |
|        - | 12723 | `	}` |
|      229 | 12724 | `	if( iMin == iMax ){` |
|        5 | 12725 | `		ph7_result_int64(pCtx,iMin);` |
|        5 | 12726 | `		return SXRET_OK;` |
|        - | 12727 | `	}` |
|      225 | 12728 | `	uRange = (sxu64)iMax - (sxu64)iMin;` |
|      225 | 12729 | `	uMask = uRange;` |
|      225 | 12730 | `	uMask \|= uMask >> 1;` |
|      225 | 12731 | `	uMask \|= uMask >> 2;` |
|      225 | 12732 | `	uMask \|= uMask >> 4;` |
|      225 | 12733 | `	uMask \|= uMask >> 8;` |
|      225 | 12734 | `	uMask \|= uMask >> 16;` |
|      225 | 12735 | `	uMask \|= uMask >> 32;` |
|      225 | 12736 | `	uResult = 0;` |
|      365 | 12737 | `	for( nAttempt = 0 ; nAttempt < 50 ; ++nAttempt ){` |
|        - | 12738 | `		/* Always draw a full 8 bytes so endianness of the cast doesn't matter` |
|        - | 12739 | `		 * (a 4-byte fill into a sxu64 would land in the high half on big-endian` |
|        - | 12740 | `		 * and the low-half mask would always read 0). */` |
|        - | 12741 | `		sxu64 uDraw;` |
|      365 | 12742 | `		if( SyOSCSPRNG(&uDraw,sizeof(uDraw)) != SXRET_OK ){` |
|        - | 12743 | `			/* PHP 8.2+ would throw Random\RandomException here; that class` |
|        - | 12744 | `			 * is not yet registered in PHL (see PLAN.md item 6.13). */` |
|      ! 0 | 12745 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12746 | `				"Exception",` |
|        - | 12747 | `				"Cannot gather sufficient random data"` |
|        - | 12748 | `				);` |
|        - | 12749 | `		}` |
|      365 | 12750 | `		uDraw &= uMask;` |
|      365 | 12751 | `		if( uDraw <= uRange ){` |
|      225 | 12752 | `			uResult = uDraw;` |
|      225 | 12753 | `			break;` |
|        - | 12754 | `		}` |
|       81 | 12755 | `	}` |
|      225 | 12756 | `	if( nAttempt >= 50 ){` |
|      ! 0 | 12757 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12758 | `			"Exception",` |
|        - | 12759 | `			"Cannot gather sufficient random data"` |
|        - | 12760 | `			);` |
|        - | 12761 | `	}` |
|      225 | 12762 | `	ph7_result_int64(pCtx,(sxi64)((sxu64)iMin + uResult));` |
|      225 | 12763 | `	return SXRET_OK;` |
|      122 | 12764 |  |
|        - | 12765 | `/*` |
|        - | 12766 | ` * string random_bytes(int $length)` |
|        - | 12767 | ` *  Generate $length cryptographically secure random bytes via SyOSCSPRNG().` |
|        - | 12768 | ` *  Mirrors PHP 7.0+ random_bytes().` |
|        - | 12769 | ` */` |
|       24 | 12770 | `static int vm_builtin_random_bytes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12771 |  |
|        - | 12772 | `	sxi64 iLen;` |
|        - | 12773 | `	unsigned char zStack[256];` |
|        - | 12774 | `	void *pBuf;` |
|        - | 12775 | `	int rc;` |
|       25 | 12776 | `	int bHeap = 0;` |
|       25 | 12777 | `	if( nArg != 1 ){` |
|        7 | 12778 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12779 | `			"ArgumentCountError",` |
|        - | 12780 | `			"random_bytes() expects exactly 1 argument, %d given",` |
|        2 | 12781 | `			nArg` |
|        - | 12782 | `			);` |
|        - | 12783 | `	}` |
|       21 | 12784 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_bytes",1,"$length");` |
|       21 | 12785 | `	if( rc != SXRET_OK ){ return rc; }` |
|       17 | 12786 | `	iLen = ph7_value_to_int64(apArg[0]);` |
|       17 | 12787 | `	if( iLen < 1 ){` |
|        5 | 12788 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12789 | `			"ValueError",` |
|        - | 12790 | `			"random_bytes(): Argument #1 ($length) must be greater than 0"` |
|        - | 12791 | `			);` |
|        - | 12792 | `	}` |
|        - | 12793 | `	/* The PH7 allocator and ph7_result_string both take sxu32/int sizes,` |
|        - | 12794 | `	 * so we can't honor a length above 2 GiB. Reject early rather than` |
|        - | 12795 | `	 * silently truncating via the (sxu32) cast below. */` |
|       13 | 12796 | `	if( iLen > 0x7FFFFFFF ){` |
|      ! 0 | 12797 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12798 | `			"ValueError",` |
|        - | 12799 | `			"random_bytes(): Argument #1 ($length) is too large"` |
|        - | 12800 | `			);` |
|        - | 12801 | `	}` |
|       13 | 12802 | `	if( iLen <= (sxi64)sizeof(zStack) ){` |
|       13 | 12803 | `		pBuf = zStack;` |
|        7 | 12804 | `	}else{` |
|      ! 0 | 12805 | `		pBuf = SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)iLen);` |
|      ! 0 | 12806 | `		if( pBuf == 0 ){` |
|      ! 0 | 12807 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12808 | `				"Exception",` |
|        - | 12809 | `				"random_bytes(): Failed to allocate %qd bytes",` |
|      ! 0 | 12810 | `				iLen` |
|        - | 12811 | `				);` |
|        - | 12812 | `		}` |
|      ! 0 | 12813 | `		bHeap = 1;` |
|        - | 12814 | `	}` |
|       13 | 12815 | `	if( SyOSCSPRNG(pBuf,(sxu32)iLen) != SXRET_OK ){` |
|      ! 0 | 12816 | `		if( bHeap ){` |
|      ! 0 | 12817 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 12818 | `		}` |
|      ! 0 | 12819 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12820 | `			"Exception",` |
|        - | 12821 | `			"Cannot gather sufficient random data"` |
|        - | 12822 | `			);` |
|        - | 12823 | `	}` |
|       13 | 12824 | `	ph7_result_string(pCtx,(const char *)pBuf,(int)iLen);` |
|       13 | 12825 | `	if( bHeap ){` |
|      ! 0 | 12826 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 12827 | `	}` |
|       13 | 12828 | `	return SXRET_OK;` |
|       13 | 12829 |  |
|        - | 12830 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12831 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 12832 | `/* Unique ID private data */` |
|        - | 12833 | `struct unique_id_data` |
|        - | 12834 |  |
|        - | 12835 | `	ph7_context *pCtx; /* Call context */` |
|        - | 12836 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - | 12837 | `};` |
|        - | 12838 | `/*` |
|        - | 12839 | ` * Binary to hex consumer callback.` |
|        - | 12840 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - | 12841 | ` * defined below.` |
|        - | 12842 | ` */` |
|      192 | 12843 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 | 12844 |  |
|      193 | 12845 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - | 12846 | `	sxu32 nBuflen;` |
|        - | 12847 | `	/* Extract result buffer length */` |
|      193 | 12848 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 | 12849 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - | 12850 | `			/*` |
|        - | 12851 | `			 * If the more_entropy flag is not set,then the returned` |
|        - | 12852 | `			 * string will be 13 characters long` |
|        - | 12853 | `			 */` |
|       25 | 12854 | `		return SXERR_ABORT;` |
|        - | 12855 | `	}` |
|      169 | 12856 | `	if( nBuflen > 22 ){` |
|      ! 0 | 12857 | `		return SXERR_ABORT;` |
|        - | 12858 | `	}` |
|        - | 12859 | `	/* Safely Consume the hex stream */` |
|      169 | 12860 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 | 12861 | `	return SXRET_OK;` |
|       97 | 12862 |  |
|        - | 12863 | `/*` |
|        - | 12864 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - | 12865 | ` *  Generate a unique ID` |
|        - | 12866 | ` * Parameter` |
|        - | 12867 | ` * $prefix` |
|        - | 12868 | ` *  Append this prefix to the generated unique ID.` |
|        - | 12869 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - | 12870 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - | 12871 | ` * $more_entropy` |
|        - | 12872 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - | 12873 | ` *  that the result will be unique.` |
|        - | 12874 | ` * Return` |
|        - | 12875 | ` *  Returns the unique identifier, as a string.` |
|        - | 12876 | ` */` |
|       24 | 12877 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12878 |  |
|        - | 12879 | `	struct unique_id_data sUniq;` |
|        - | 12880 | `	unsigned char zDigest[20];` |
|       25 | 12881 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12882 | `	const char *zPrefix;` |
|        - | 12883 | `	SHA1Context sCtx;` |
|        - | 12884 | `	char zRandom[7];` |
|        - | 12885 | `	int nPrefix;` |
|        - | 12886 | `	int entropy;` |
|        - | 12887 | `	/* Generate a random string first */` |
|       25 | 12888 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - | 12889 | `	/* Initialize fields */` |
|       25 | 12890 | `	zPrefix = 0;` |
|       25 | 12891 | `	nPrefix = 0;` |
|       25 | 12892 | `	entropy = 0;` |
|       25 | 12893 | `	if( nArg > 0 ){` |
|        - | 12894 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 | 12895 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 | 12896 | `		if( nArg > 1 ){` |
|      ! 0 | 12897 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 12898 | `		}` |
|      ! 0 | 12899 | `	}` |
|       25 | 12900 | `	SHA1Init(&sCtx);` |
|        - | 12901 | `	/* Generate the random ID */` |
|       25 | 12902 | `	if( nPrefix > 0 ){` |
|      ! 0 | 12903 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 | 12904 | `	}` |
|        - | 12905 | `	/* Append the random ID */` |
|       25 | 12906 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - | 12907 | `	/* Append the random string */` |
|       25 | 12908 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - | 12909 | `	/* Increment the number */` |
|       25 | 12910 | `	pVm->unique_id++;` |
|       25 | 12911 | `	SHA1Final(&sCtx,zDigest);` |
|        - | 12912 | `	/* Hexify the digest */` |
|       25 | 12913 | `	sUniq.pCtx = pCtx;` |
|       25 | 12914 | `	sUniq.entropy = entropy;` |
|       25 | 12915 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - | 12916 | `	/* All done */` |
|       25 | 12917 | `	return PH7_OK;` |
|        1 | 12918 |  |
|        - | 12919 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 12920 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 12921 | `/*` |
|        - | 12922 | ` * Section:` |
|        - | 12923 | ` *  Language construct implementation as foreign functions.` |
|        - | 12924 | ` * Status:` |
|        - | 12925 | ` *    Stable.` |
|        - | 12926 | ` */` |
|        - | 12927 | `/*` |
|        - | 12928 | ` * void echo($string...)` |
|        - | 12929 | ` *  Output one or more messages.` |
|        - | 12930 | ` * Parameters` |
|        - | 12931 | ` *  $string` |
|        - | 12932 | ` *   Message to output.` |
|        - | 12933 | ` * Return` |
|        - | 12934 | ` *  NULL.` |
|        - | 12935 | ` */` |
|      ! 0 | 12936 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 12937 |  |
|        - | 12938 | `	const char *zData;` |
|      ! 0 | 12939 | `	int nDataLen = 0;` |
|        - | 12940 | `	ph7_vm *pVm;` |
|        - | 12941 | `	int i,rc;` |
|        - | 12942 | `	/* Point to the target VM */` |
|      ! 0 | 12943 | `	pVm = pCtx->pVm;` |
|        - | 12944 | `	/* Output */` |
|      ! 0 | 12945 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 | 12946 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 | 12947 | `		if( nDataLen > 0 ){` |
|      ! 0 | 12948 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 | 12949 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 | 12950 | `			if( rc == SXERR_ABORT ){` |
|        - | 12951 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 12952 | `				return PH7_ABORT;` |
|        - | 12953 | `			}` |
|      ! 0 | 12954 | `		}` |
|      ! 0 | 12955 | `	}` |
|      ! 0 | 12956 | `	return SXRET_OK;` |
|      ! 0 | 12957 |  |
|        - | 12958 | `/*` |
|        - | 12959 | ` * int print($string...)` |
|        - | 12960 | ` *  Output one or more messages.` |
|        - | 12961 | ` * Parameters` |
|        - | 12962 | ` *  $string` |
|        - | 12963 | ` *   Message to output.` |
|        - | 12964 | ` * Return` |
|        - | 12965 | ` *  1 always.` |
|        - | 12966 | ` */` |
|        2 | 12967 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12968 |  |
|        - | 12969 | `	const char *zData;` |
|        3 | 12970 | `	int nDataLen = 0;` |
|        - | 12971 | `	ph7_vm *pVm;` |
|        - | 12972 | `	int i,rc;` |
|        - | 12973 | `	/* Point to the target VM */` |
|        3 | 12974 | `	pVm = pCtx->pVm;` |
|        - | 12975 | `	/* Output */` |
|        5 | 12976 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 | 12977 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 | 12978 | `		if( nDataLen > 0 ){` |
|        3 | 12979 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 | 12980 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 | 12981 | `			if( rc == SXERR_ABORT ){` |
|        - | 12982 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 12983 | `				return PH7_ABORT;` |
|        - | 12984 | `			}` |
|        1 | 12985 | `		}` |
|        2 | 12986 | `	}` |
|        - | 12987 | `	/* Return 1 */` |
|        3 | 12988 | `	ph7_result_int(pCtx,1);` |
|        3 | 12989 | `	return SXRET_OK;` |
|        2 | 12990 |  |
|        - | 12991 | `/*` |
|        - | 12992 | ` * void exit(string $msg)` |
|        - | 12993 | ` * void exit(int $status)` |
|        - | 12994 | ` * void die(string $ms)` |
|        - | 12995 | ` * void die(int $status)` |
|        - | 12996 | ` *   Output a message and terminate program execution.` |
|        - | 12997 | ` * Parameter` |
|        - | 12998 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - | 12999 | ` *  If status is an integer, that value will be used as the exit status` |
|        - | 13000 | ` *  and not printed` |
|        - | 13001 | ` * Return` |
|        - | 13002 | ` *  NULL` |
|        - | 13003 | ` */` |
|      ! 0 | 13004 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 13005 |  |
|      ! 0 | 13006 | `	if( nArg > 0 ){` |
|      ! 0 | 13007 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - | 13008 | `			const char *zData;` |
|      ! 0 | 13009 | `			int iLen = 0;` |
|        - | 13010 | `			/* Print exit message */` |
|      ! 0 | 13011 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 | 13012 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 | 13013 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - | 13014 | `			sxi32 iExitStatus;` |
|        - | 13015 | `			/* Record exit status code */` |
|      ! 0 | 13016 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 | 13017 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 | 13018 | `		}` |
|      ! 0 | 13019 | `	}` |
|        - | 13020 | `	/* Request a VM-wide halt (see PH7_OP_HALT) and abort processing` |
|        - | 13021 | `	 * immediately; the abort unwinds enclosing frames and execution units.` |
|        - | 13022 | `	 */` |
|      ! 0 | 13023 | `	pCtx->pVm->bHaltRequested = 1;` |
|      ! 0 | 13024 | `	return PH7_ABORT;` |
|      ! 0 | 13025 |  |
|        - | 13026 | `/*` |
|        - | 13027 | ` * bool isset($var,...)` |
|        - | 13028 | ` *  Finds out whether a variable is set.` |
|        - | 13029 | ` * Parameters` |
|        - | 13030 | ` *  One or more variable to check.` |
|        - | 13031 | ` * Return` |
|        - | 13032 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - | 13033 | ` */` |
|    93188 | 13034 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13035 |  |
|        - | 13036 | `	ph7_value *pObj;` |
|    93190 | 13037 | `	int res = 0;` |
|        - | 13038 | `	int i;` |
|    93190 | 13039 | `	if( nArg < 1 ){` |
|        - | 13040 | `		/* Missing arguments,return false */` |
|      ! 0 | 13041 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 | 13042 | `		return SXRET_OK;` |
|        - | 13043 | `	}` |
|        - | 13044 | `	/* Iterate over available arguments */` |
|   121792 | 13045 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    93200 | 13046 | `		pObj = apArg[i];` |
|    93200 | 13047 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|        - | 13048 | `			/* Skip the "expecting a variable" warning for MEMOBJ_BOOL —` |
|        - | 13049 | `			 * synthesized by LOAD_IDX iP2=4 (ArrayAccess::offsetExists) and` |
|        - | 13050 | `			 * by anyone passing a bool literal (rare, harmless). */` |
|    63608 | 13051 | `			if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - | 13052 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 | 13053 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 | 13054 | `			}` |
|    31803 | 13055 | `		}` |
|    93200 | 13056 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    93200 | 13057 | `		if( !res ){` |
|        - | 13058 | `			/* Variable not set,return FALSE */` |
|    64598 | 13059 | `			ph7_result_bool(pCtx,0);` |
|    64598 | 13060 | `			return SXRET_OK;` |
|        - | 13061 | `		}` |
|    14303 | 13062 | `	}` |
|        - | 13063 | `	/* All given variable are set,return TRUE */` |
|    28594 | 13064 | `	ph7_result_bool(pCtx,1);` |
|    28594 | 13065 | `	return SXRET_OK;` |
|    46596 | 13066 |  |
|        - | 13067 | `/*` |
|        - | 13068 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - | 13069 | ` * frame,the reference table and discard it's contents.` |
|        - | 13070 | ` * This function never fail and always return SXRET_OK.` |
|        - | 13071 | ` */` |
|  3164240 | 13072 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 | 13073 |  |
|        - | 13074 | `	ph7_value *pObj;` |
|        - | 13075 | `	VmRefObj *pRef;` |
|  3164242 | 13076 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  3164242 | 13077 | `	if( pObj ){` |
|        - | 13078 | `		/* Release the object */` |
|  3164242 | 13079 | `		PH7_MemObjRelease(pObj);` |
|  1582120 | 13080 | `	}` |
|        - | 13081 | `	/* Remove old reference links */` |
|  3164242 | 13082 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  3164242 | 13083 | `	if( pRef ){` |
|  3164236 | 13084 | `		sxi32 iFlags = pRef->iFlags;` |
|        - | 13085 | `		/* Unlink from the reference table */` |
|  3164236 | 13086 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  3164236 | 13087 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - | 13088 | `			VmSlot sFree;` |
|        - | 13089 | `			/* Restore to the free list */` |
|  3164228 | 13090 | `			sFree.nIdx = nObjIdx;` |
|  3164228 | 13091 | `			sFree.pUserData = 0;` |
|  3164228 | 13092 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1582113 | 13093 | `		}` |
|  1582117 | 13094 | `	}` |
|  3164242 | 13095 | `	return SXRET_OK;` |
|        2 | 13096 |  |
|        - | 13097 | `/*` |
|        - | 13098 | ` * void unset($var,...)` |
|        - | 13099 | ` *   Unset one or more given variable.` |
|        - | 13100 | ` * Parameters` |
|        - | 13101 | ` *  One or more variable to unset.` |
|        - | 13102 | ` * Return` |
|        - | 13103 | ` *  Nothing.` |
|        - | 13104 | ` */` |
|     7580 | 13105 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13106 |  |
|        - | 13107 | `	ph7_value *pObj;` |
|        - | 13108 | `	ph7_vm *pVm;` |
|        - | 13109 | `	int i;` |
|        - | 13110 | `	/* Point to the target VM */` |
|     7582 | 13111 | `	pVm = pCtx->pVm;` |
|        - | 13112 | `	/* Iterate and unset */` |
|    15162 | 13113 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     7582 | 13114 | `		pObj = apArg[i];` |
|     7582 | 13115 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      840 | 13116 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 13117 | `				/* Throw an error */` |
|      ! 0 | 13118 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 | 13119 | `			}` |
|      421 | 13120 | `		}else{` |
|     6744 | 13121 | `			sxu32 nIdx = pObj->nIdx;` |
|        - | 13122 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     6744 | 13123 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     6738 | 13124 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3368 | 13125 | `			}` |
|        - | 13126 | `		}` |
|     3792 | 13127 | `	}` |
|     7582 | 13128 | `	return SXRET_OK;` |
|        2 | 13129 |  |
|        - | 13130 | `/*` |
|        - | 13131 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - | 13132 | ` */` |
|      116 | 13133 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 13134 |  |
|      117 | 13135 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      117 | 13136 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 13137 | `	ph7_value *pObj;` |
|        - | 13138 | `	sxu32 nIdx;` |
|        - | 13139 | `	/* Extract the memory object */` |
|      117 | 13140 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      117 | 13141 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      117 | 13142 | `	if( pObj ){` |
|      117 | 13143 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      115 | 13144 | `			if( pEntry->nKeyLen > 0 ){` |
|        - | 13145 | `				SyString sName;` |
|        - | 13146 | `				ph7_value sKey;` |
|        - | 13147 | `				/* Perform the insertion (pObj may point into pVm->aMemObj; the` |
|        - | 13148 | `				 * inserter snapshots the source before reserving, so the pool may` |
|        - | 13149 | `				 * safely move underneath it — see HashmapInsertIntKey/BlobKey). */` |
|      115 | 13150 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      115 | 13151 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      115 | 13152 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      115 | 13153 | `				PH7_MemObjRelease(&sKey);` |
|       57 | 13154 | `			}` |
|       57 | 13155 | `		}` |
|       58 | 13156 | `	}` |
|      117 | 13157 | `	return SXRET_OK;` |
|        1 | 13158 |  |
|        - | 13159 | `/*` |
|        - | 13160 | ` * array get_defined_vars(void)` |
|        - | 13161 | ` *  Returns an array of all defined variables.` |
|        - | 13162 | ` * Parameter` |
|        - | 13163 | ` *  None` |
|        - | 13164 | ` * Return` |
|        - | 13165 | ` *  An array with all the variables defined in the current scope.` |
|        - | 13166 | ` */` |
|        2 | 13167 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13168 |  |
|        3 | 13169 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13170 | `	ph7_value *pArray;` |
|        - | 13171 | `	/* Create a new array */` |
|        3 | 13172 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 13173 | ` 	if( pArray == 0 ){` |
|      ! 0 | 13174 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 13175 | `		SXUNUSED(apArg);` |
|        - | 13176 | `		/* Return NULL */` |
|      ! 0 | 13177 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13178 | `		return SXRET_OK;` |
|        - | 13179 | `	}` |
|        - | 13180 | `	/* Superglobals first */` |
|        3 | 13181 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - | 13182 | `	/* Then variable defined in the current frame */` |
|        3 | 13183 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - | 13184 | `	/* Finally,return the created array */` |
|        3 | 13185 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 13186 | `	return SXRET_OK;` |
|        2 | 13187 |  |
|        - | 13188 | `/*` |
|        - | 13189 | ` * bool gettype($var)` |
|        - | 13190 | ` *  Get the type of a variable` |
|        - | 13191 | ` * Parameters` |
|        - | 13192 | ` *   $var` |
|        - | 13193 | ` *    The variable being type checked.` |
|        - | 13194 | ` * Return` |
|        - | 13195 | ` *   String representation of the given variable type.` |
|        - | 13196 | ` */` |
|       32 | 13197 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13198 |  |
|       34 | 13199 | `	const char *zType = "Empty";` |
|       34 | 13200 | `	if( nArg > 0 ){` |
|       34 | 13201 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 | 13202 | `	}` |
|        - | 13203 | `	/* Return the variable type */` |
|       34 | 13204 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 | 13205 | `	return SXRET_OK;` |
|        2 | 13206 |  |
|        - | 13207 | `/*` |
|        - | 13208 | ` * string get_resource_type(resource $handle)` |
|        - | 13209 | ` *  This function gets the type of the given resource.` |
|        - | 13210 | ` * Parameters` |
|        - | 13211 | ` *  $handle` |
|        - | 13212 | ` *  The evaluated resource handle.` |
|        - | 13213 | ` * Return` |
|        - | 13214 | ` *  If the given handle is a resource, this function will return a string` |
|        - | 13215 | ` *  representing its type. If the type is not identified by this function` |
|        - | 13216 | ` *  the return value will be the string Unknown.` |
|        - | 13217 | ` *  This function will return FALSE and generate an error if handle` |
|        - | 13218 | ` *  is not a resource.` |
|        - | 13219 | ` */` |
|        2 | 13220 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13221 |  |
|        3 | 13222 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13223 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 | 13224 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13225 | `		return PH7_OK;` |
|        - | 13226 | `	}` |
|        3 | 13227 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 | 13228 | `	return SXRET_OK;` |
|        2 | 13229 |  |
|        - | 13230 | `/*` |
|        - | 13231 | ` * void var_dump(expression,....)` |
|        - | 13232 | ` *   var_dump � Dumps information about a variable` |
|        - | 13233 | ` * Parameters` |
|        - | 13234 | ` *   One or more expression to dump.` |
|        - | 13235 | ` * Returns` |
|        - | 13236 | ` *  Nothing.` |
|        - | 13237 | ` */` |
|      218 | 13238 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13239 |  |
|        - | 13240 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - | 13241 | `	int i;` |
|      220 | 13242 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - | 13243 | `	/* Dump one or more expressions */` |
|      444 | 13244 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 | 13245 | `		ph7_value *pObj = apArg[i];` |
|        - | 13246 | `		/* Reset the working buffer */` |
|      226 | 13247 | `		SyBlobReset(&sDump);` |
|        - | 13248 | `		/* Dump the given expression */` |
|      226 | 13249 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - | 13250 | `		/* Output */` |
|      226 | 13251 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 | 13252 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 | 13253 | `		}` |
|      114 | 13254 | `	}` |
|        - | 13255 | `	/* Release the working buffer */` |
|      220 | 13256 | `	SyBlobRelease(&sDump);` |
|      220 | 13257 | `	return SXRET_OK;` |
|        2 | 13258 |  |
|        - | 13259 | `/*` |
|        - | 13260 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - | 13261 | ` *   print-r - Prints human-readable information about a variable` |
|        - | 13262 | ` * Parameters` |
|        - | 13263 | ` *   expression: Expression to dump` |
|        - | 13264 | ` *   return : If you would like to capture the output of print_r() use` |
|        - | 13265 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - | 13266 | ` *            print_r() will return the information rather than print it.` |
|        - | 13267 | ` * Return` |
|        - | 13268 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - | 13269 | ` *  Otherwise, the return value is TRUE.` |
|        - | 13270 | ` */` |
|       16 | 13271 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13272 |  |
|       17 | 13273 | `	int ret_string = 0;` |
|        - | 13274 | `	SyBlob sDump;` |
|       17 | 13275 | `	if( nArg < 1 ){` |
|        - | 13276 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 13277 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13278 | `		return SXRET_OK;` |
|        - | 13279 | `	}` |
|       17 | 13280 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 | 13281 | `	if ( nArg > 1 ){` |
|        - | 13282 | `		/* Where to redirect output */` |
|       11 | 13283 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 | 13284 | `	}` |
|        - | 13285 | `	/* Generate dump */` |
|       17 | 13286 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 | 13287 | `	if( !ret_string ){` |
|        - | 13288 | `		/* Output dump */` |
|        7 | 13289 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13290 | `		/* Return true */` |
|        7 | 13291 | `		ph7_result_bool(pCtx,1);` |
|        4 | 13292 | `	}else{` |
|        - | 13293 | `		/* Generated dump as return value */` |
|       11 | 13294 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13295 | `	}` |
|        - | 13296 | `	/* Release the working buffer */` |
|       17 | 13297 | `	SyBlobRelease(&sDump);` |
|       17 | 13298 | `	return SXRET_OK;` |
|        9 | 13299 |  |
|        - | 13300 | `/*` |
|        - | 13301 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - | 13302 | ` * Same job as print_r. (see coment above)` |
|        - | 13303 | ` */` |
|        2 | 13304 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13305 |  |
|        3 | 13306 | `	int ret_string = 0;` |
|        - | 13307 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 | 13308 | `	if( nArg < 1 ){` |
|        - | 13309 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 13310 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13311 | `		return SXRET_OK;` |
|        - | 13312 | `	}` |
|        3 | 13313 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 | 13314 | `	if ( nArg > 1 ){` |
|        - | 13315 | `		/* Where to redirect output */` |
|        3 | 13316 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 | 13317 | `	}` |
|        - | 13318 | `	/* Generate dump */` |
|        3 | 13319 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 | 13320 | `	if( !ret_string ){` |
|        - | 13321 | `		/* Output dump */` |
|      ! 0 | 13322 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13323 | `		/* Return NULL */` |
|      ! 0 | 13324 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13325 | `	}else{` |
|        - | 13326 | `		/* Generated dump as return value */` |
|        3 | 13327 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13328 | `	}` |
|        - | 13329 | `	/* Release the working buffer */` |
|        3 | 13330 | `	SyBlobRelease(&sDump);` |
|        3 | 13331 | `	return SXRET_OK;` |
|        2 | 13332 |  |
|        - | 13333 | `/*` |
|        - | 13334 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - | 13335 | ` *  Set/get the various assert flags.` |
|        - | 13336 | ` * Parameter` |
|        - | 13337 | ` * $what` |
|        - | 13338 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - | 13339 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - | 13340 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - | 13341 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - | 13342 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - | 13343 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - | 13344 | ` * $value` |
|        - | 13345 | ` *   An optional new value for the option.` |
|        - | 13346 | ` * Return` |
|        - | 13347 | ` *  Old setting on success or FALSE on failure.` |
|        - | 13348 | ` */` |
|       28 | 13349 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13350 |  |
|       30 | 13351 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13352 | `	int iOption;` |
|        - | 13353 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       30 | 13354 | `	if( nArg < 1 ){` |
|        3 | 13355 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13356 | `			"ArgumentCountError",` |
|        - | 13357 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - | 13358 | `			);` |
|        - | 13359 | `	}` |
|        - | 13360 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       26 | 13361 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       28 | 13362 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 | 13363 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13364 | `			"TypeError",` |
|        - | 13365 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 | 13366 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 | 13367 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - | 13368 | `			);` |
|        - | 13369 | `	}` |
|       28 | 13370 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - | 13371 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - | 13372 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - | 13373 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       28 | 13374 | `	switch( iOption ){` |
|        5 | 13375 | `	case 1: /* ASSERT_ACTIVE */` |
|        - | 13376 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       12 | 13377 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       12 | 13378 | `		if( nArg > 1 ){` |
|        5 | 13379 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 13380 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 | 13381 | `			}else{` |
|        3 | 13382 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - | 13383 | `			}` |
|        2 | 13384 | `		}` |
|       12 | 13385 | `		break;` |
|        1 | 13386 | `	case 2: /* ASSERT_CALLBACK */` |
|        - | 13387 | `		/* Return old callback or null */` |
|        3 | 13388 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 | 13389 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 | 13390 | `		}else{` |
|        3 | 13391 | `			ph7_result_null(pCtx);` |
|        - | 13392 | `		}` |
|        3 | 13393 | `		if( nArg > 1 ){` |
|      ! 0 | 13394 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 | 13395 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 | 13396 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 | 13397 | `			}else{` |
|      ! 0 | 13398 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - | 13399 | `			}` |
|      ! 0 | 13400 | `		}` |
|        3 | 13401 | `		break;` |
|        5 | 13402 | `	case 3: /* ASSERT_BAIL */` |
|       11 | 13403 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 | 13404 | `		if( nArg > 1 ){` |
|        5 | 13405 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 13406 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 | 13407 | `			}else{` |
|        3 | 13408 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - | 13409 | `			}` |
|        2 | 13410 | `		}` |
|       11 | 13411 | `		break;` |
|      ! 0 | 13412 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 | 13413 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 13414 | `		break;` |
|        1 | 13415 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 | 13416 | `		ph7_result_int(pCtx, 1);` |
|        3 | 13417 | `		break;` |
|      ! 0 | 13418 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 | 13419 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 13420 | `		break;` |
|        1 | 13421 | `	default:` |
|        - | 13422 | `		/* PHP 8: ValueError for invalid option */` |
|        3 | 13423 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13424 | `			"ValueError",` |
|        - | 13425 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - | 13426 | `			);` |
|        - | 13427 | `	}` |
|       26 | 13428 | `	return PH7_OK;` |
|       16 | 13429 |  |
|        - | 13430 | `/*` |
|        - | 13431 | ` * bool assert(mixed $assertion)` |
|        - | 13432 | ` *  Checks if assertion is FALSE.` |
|        - | 13433 | ` * Parameter` |
|        - | 13434 | ` *  $assertion` |
|        - | 13435 | ` *    The assertion to test.` |
|        - | 13436 | ` * Return` |
|        - | 13437 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - | 13438 | ` */` |
|       24 | 13439 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13440 |  |
|       26 | 13441 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13442 | `	int iFlags,iResult;` |
|        - | 13443 | `	const char *zDesc;` |
|        - | 13444 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       26 | 13445 | `	if( nArg < 1 ){` |
|        3 | 13446 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13447 | `			"ArgumentCountError",` |
|        - | 13448 | `			"assert() expects at least 1 argument, 0 given"` |
|        - | 13449 | `			);` |
|        - | 13450 | `	}` |
|       24 | 13451 | `	iFlags = pVm->iAssertFlags;` |
|       24 | 13452 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - | 13453 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 | 13454 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 | 13455 | `		return PH7_OK;` |
|        - | 13456 | `	}` |
|        - | 13457 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       24 | 13458 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       24 | 13459 | `	if( !iResult ){` |
|        - | 13460 | `		/* Assertion failed */` |
|        - | 13461 | `		/* Extract optional description */` |
|       13 | 13462 | `		zDesc = 0;` |
|       13 | 13463 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 13464 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 | 13465 | `		}` |
|       13 | 13466 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - | 13467 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - | 13468 | `			ph7_value sFile,sLine;` |
|        - | 13469 | `			ph7_value *apCbArg[3];` |
|        - | 13470 | `			SyString *pFile;` |
|        - | 13471 | `			/* Extract the processed script */` |
|      ! 0 | 13472 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 | 13473 | `			if( pFile == 0 ){` |
|      ! 0 | 13474 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 | 13475 | `			}` |
|        - | 13476 | `			/* Invoke the callback */` |
|      ! 0 | 13477 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 | 13478 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 | 13479 | `			apCbArg[0] = &sFile;` |
|      ! 0 | 13480 | `			apCbArg[1] = &sLine;` |
|      ! 0 | 13481 | `			apCbArg[2] = apArg[0];` |
|      ! 0 | 13482 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - | 13483 | `			/* Clean-up the mess left behind */` |
|      ! 0 | 13484 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 | 13485 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 | 13486 | `		}` |
|       13 | 13487 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - | 13488 | `			/* Abort VM execution immediately */` |
|      ! 0 | 13489 | `			return PH7_ABORT;` |
|        - | 13490 | `		}` |
|        - | 13491 | `		/* PHP 8: throw AssertionError by default */` |
|       13 | 13492 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 | 13493 | `			return PH7_VmThrowException(pCtx,` |
|        - | 13494 | `				"AssertionError",` |
|        - | 13495 | `				"%s",` |
|        1 | 13496 | `				zDesc` |
|        - | 13497 | `				);` |
|      ! 0 | 13498 | `		}else{` |
|       11 | 13499 | `			return PH7_VmThrowException(pCtx,` |
|        - | 13500 | `				"AssertionError",` |
|        - | 13501 | `				"assert(false)"` |
|        - | 13502 | `				);` |
|        - | 13503 | `		}` |
|        - | 13504 | `	}` |
|        - | 13505 | `	/* Assertion passed */` |
|       11 | 13506 | `	ph7_result_bool(pCtx,1);` |
|       11 | 13507 | `	return PH7_OK;` |
|       14 | 13508 |  |
|        - | 13509 | `/*` |
|        - | 13510 | ` * Section:` |
|        - | 13511 | ` *  Error reporting functions.` |
|        - | 13512 | ` * Status:` |
|        - | 13513 | ` *    Stable.` |
|        - | 13514 | ` */` |
|        - | 13515 | `/*` |
|        - | 13516 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - | 13517 | ` *  Generates a user-level error/warning/notice message.` |
|        - | 13518 | ` * Parameters` |
|        - | 13519 | ` *  $error_msg` |
|        - | 13520 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - | 13521 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - | 13522 | ` * $error_type` |
|        - | 13523 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - | 13524 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - | 13525 | ` * Return` |
|        - | 13526 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - | 13527 | ` */` |
|       12 | 13528 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13529 |  |
|       14 | 13530 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 | 13531 | `	int rc = PH7_OK;` |
|       14 | 13532 | `	if( nArg > 0 ){` |
|        - | 13533 | `		const char *zErr;` |
|        - | 13534 | `		int nLen;` |
|        - | 13535 | `		/* Extract the error message */` |
|       12 | 13536 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 | 13537 | `		if( nArg > 1 ){` |
|        - | 13538 | `			/* Extract the error type */` |
|       12 | 13539 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 | 13540 | `			switch( nErr ){` |
|        1 | 13541 | `			case 1:   /* E_ERROR */` |
|        - | 13542 | `			case 16:  /* E_CORE_ERROR */` |
|        - | 13543 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - | 13544 | `			case 256: /* E_USER_ERROR */` |
|        3 | 13545 | `				nErr = PH7_CTX_ERR;` |
|        3 | 13546 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 | 13547 | `				break;` |
|        1 | 13548 | `			case 2:   /* E_WARNING */` |
|        - | 13549 | `			case 32:  /* E_CORE_WARNING */` |
|        - | 13550 | `			case 123: /* E_COMPILE_WARNING */` |
|        - | 13551 | `			case 512: /* E_USER_WARNING */` |
|        3 | 13552 | `				nErr = PH7_CTX_WARNING;` |
|        3 | 13553 | `				break;` |
|        3 | 13554 | `			default:` |
|        8 | 13555 | `				nErr = PH7_CTX_NOTICE;` |
|        6 | 13556 | `				break;` |
|        - | 13557 | `			}` |
|        5 | 13558 | `		}` |
|        - | 13559 | `		/* Report error */` |
|       12 | 13560 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 | 13561 | `		if( rc == PH7_ABORT ){` |
|      ! 0 | 13562 | `			return rc;` |
|        - | 13563 | `		}` |
|        - | 13564 | `		/* Return true */` |
|       12 | 13565 | `		ph7_result_bool(pCtx,1);` |
|        7 | 13566 | `	}else{` |
|        - | 13567 | `		/* Missing arguments,return FALSE */` |
|        3 | 13568 | `		ph7_result_bool(pCtx,0);` |
|        - | 13569 | `	}` |
|       14 | 13570 | `	return rc;` |
|        8 | 13571 |  |
|        - | 13572 | `/*` |
|        - | 13573 | ` * int error_reporting([int $level])` |
|        - | 13574 | ` *  Sets which PHP errors are reported.` |
|        - | 13575 | ` * Parameters` |
|        - | 13576 | ` *  $level` |
|        - | 13577 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - | 13578 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - | 13579 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - | 13580 | ` *   levels will not always behave as expected.` |
|        - | 13581 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - | 13582 | ` *   in the predefined constants.` |
|        - | 13583 | ` * Return` |
|        - | 13584 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - | 13585 | ` *   parameter is given.` |
|        - | 13586 | ` */` |
|       32 | 13587 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13588 |  |
|       34 | 13589 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13590 | `	int nOld;` |
|        - | 13591 | `	/* Extract the old reporting level */` |
|       34 | 13592 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       34 | 13593 | `	if( nArg > 0 ){` |
|        - | 13594 | `		int nNew;` |
|        - | 13595 | `		/* Extract the desired error reporting level */` |
|       28 | 13596 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       28 | 13597 | `		if( !nNew ){` |
|        - | 13598 | `			/* Do not report errors at all */` |
|        5 | 13599 | `			pVm->bErrReport = 0;` |
|        3 | 13600 | `		}else{` |
|        - | 13601 | `			/* Report all errors */` |
|       24 | 13602 | `			pVm->bErrReport = 1;` |
|        - | 13603 | `		}` |
|       13 | 13604 | `	}` |
|        - | 13605 | `	/* Return the old level */` |
|       34 | 13606 | `	ph7_result_int(pCtx,nOld);` |
|       34 | 13607 | `	return PH7_OK;` |
|        2 | 13608 |  |
|        - | 13609 | `/*` |
|        - | 13610 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - | 13611 | ` *  Send an error message somewhere.` |
|        - | 13612 | ` * Parameter` |
|        - | 13613 | ` *  $message` |
|        - | 13614 | ` *   The error message that should be logged.` |
|        - | 13615 | ` *  $message_type` |
|        - | 13616 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - | 13617 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - | 13618 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - | 13619 | ` *       This is the default option.` |
|        - | 13620 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - | 13621 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - | 13622 | ` *    2  No longer an option.` |
|        - | 13623 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - | 13624 | ` *       to the end of the message string.` |
|        - | 13625 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - | 13626 | ` *  $destination` |
|        - | 13627 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - | 13628 | ` *  $extra_headers` |
|        - | 13629 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - | 13630 | ` * Return` |
|        - | 13631 | ` *  TRUE on success or FALSE on failure.` |
|        - | 13632 | ` * NOTE:` |
|        - | 13633 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - | 13634 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - | 13635 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - | 13636 | ` *  Otherwise this function is no-op.` |
|        - | 13637 | ` */` |
|        4 | 13638 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13639 |  |
|        - | 13640 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 | 13641 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 | 13642 | `	int iType = 0;` |
|        5 | 13643 | `	if( nArg < 1 ){` |
|        - | 13644 | `		/* Missing log message,return FALSE */` |
|      ! 0 | 13645 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13646 | `		return PH7_OK;` |
|        - | 13647 | `	}` |
|        5 | 13648 | `	if( pVm->xErrLog  ){` |
|        - | 13649 | `		/* Invoke the user callback */` |
|      ! 0 | 13650 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 | 13651 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 | 13652 | `		if( nArg > 1 ){` |
|      ! 0 | 13653 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 13654 | `			if( nArg > 2 ){` |
|      ! 0 | 13655 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 | 13656 | `				if( nArg > 3 ){` |
|      ! 0 | 13657 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 | 13658 | `				}` |
|      ! 0 | 13659 | `			}` |
|      ! 0 | 13660 | `		}` |
|      ! 0 | 13661 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 | 13662 | `	}` |
|        - | 13663 | `	/* Retun TRUE */` |
|        5 | 13664 | `	ph7_result_bool(pCtx,1);` |
|        5 | 13665 | `	return PH7_OK;` |
|        3 | 13666 |  |
|        - | 13667 | `/*` |
|        - | 13668 | ` * bool restore_exception_handler(void)` |
|        - | 13669 | ` *  Restores the previously defined exception handler function.` |
|        - | 13670 | ` * Parameter` |
|        - | 13671 | ` *  None` |
|        - | 13672 | ` * Return` |
|        - | 13673 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - | 13674 | ` */` |
|        4 | 13675 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13676 |  |
|        5 | 13677 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13678 | `	ph7_value *pOld,*pNew;` |
|        - | 13679 | `	/* Point to the old and the new handler */` |
|        5 | 13680 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 | 13681 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 | 13682 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 13683 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 13684 | `		SXUNUSED(apArg);` |
|        - | 13685 | `		/* No installed handler,return FALSE */` |
|        5 | 13686 | `		ph7_result_bool(pCtx,0);` |
|        5 | 13687 | `		return PH7_OK;` |
|        - | 13688 | `	}` |
|        - | 13689 | `	/* Copy the old handler */` |
|      ! 0 | 13690 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 13691 | `	PH7_MemObjRelease(pOld);` |
|        - | 13692 | `	/* Return TRUE */` |
|      ! 0 | 13693 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 13694 | `	return PH7_OK;` |
|        3 | 13695 |  |
|        - | 13696 | `/*` |
|        - | 13697 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - | 13698 | ` *  Sets a user-defined exception handler function.` |
|        - | 13699 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - | 13700 | ` * NOTE` |
|        - | 13701 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - | 13702 | ` *  the satndard PHP engine.` |
|        - | 13703 | ` * Parameters` |
|        - | 13704 | ` *  $exception_handler` |
|        - | 13705 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - | 13706 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - | 13707 | ` *   that was thrown.` |
|        - | 13708 | ` *  Note:` |
|        - | 13709 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 13710 | ` * Return` |
|        - | 13711 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - | 13712 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 13713 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 13714 | ` */` |
|        4 | 13715 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13716 |  |
|        6 | 13717 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13718 | `	ph7_value *pOld,*pNew;` |
|        - | 13719 | `	/* Point to the old and the new handler */` |
|        6 | 13720 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 | 13721 | `	pNew = &pVm->aExceptionCB[1];` |
|        - | 13722 | `	/* Return the old handler */` |
|        6 | 13723 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 | 13724 | `	if( nArg > 0 ){` |
|        6 | 13725 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 13726 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 | 13727 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 | 13728 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 13729 | `		}else{` |
|        6 | 13730 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 13731 | `			/* Install the new handler */` |
|        6 | 13732 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 13733 | `		}` |
|        2 | 13734 | `	}` |
|        6 | 13735 | `	return PH7_OK;` |
|        2 | 13736 |  |
|        - | 13737 | `/*` |
|        - | 13738 | ` * bool restore_error_handler(void)` |
|        - | 13739 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 13740 | ` * Parameters:` |
|        - | 13741 | ` *  None.` |
|        - | 13742 | ` * Return` |
|        - | 13743 | ` *  Always TRUE.` |
|        - | 13744 | ` */` |
|        6 | 13745 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13746 |  |
|        7 | 13747 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13748 | `	ph7_value *pOld,*pNew;` |
|        - | 13749 | `	/* Point to the old and the new handler */` |
|        7 | 13750 | `	pOld = &pVm->aErrCB[0];` |
|        7 | 13751 | `	pNew = &pVm->aErrCB[1];` |
|        7 | 13752 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        3 | 13753 | `		SXUNUSED(nArg); /* cc warning */` |
|        3 | 13754 | `		SXUNUSED(apArg);` |
|        - | 13755 | `		/* No installed callback,return FALSE */` |
|        7 | 13756 | `		ph7_result_bool(pCtx,0);` |
|        7 | 13757 | `		return PH7_OK;` |
|        - | 13758 | `	}` |
|        - | 13759 | `	/* Copy the old callback */` |
|      ! 0 | 13760 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 13761 | `	PH7_MemObjRelease(pOld);` |
|        - | 13762 | `	/* Return TRUE */` |
|      ! 0 | 13763 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 13764 | `	return PH7_OK;` |
|        4 | 13765 |  |
|        - | 13766 | `/*` |
|        - | 13767 | ` * value set_error_handler(callable $error_handler)` |
|        - | 13768 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 13769 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 13770 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 13771 | ` *  Sets a user-defined error handler function.` |
|        - | 13772 | ` *  This function can be used for defining your own way of handling errors during` |
|        - | 13773 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - | 13774 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - | 13775 | ` *  conditions (using trigger_error()).` |
|        - | 13776 | ` * Parameters` |
|        - | 13777 | ` *  $error_handler` |
|        - | 13778 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - | 13779 | ` *   describing the error.` |
|        - | 13780 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - | 13781 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - | 13782 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - | 13783 | ` *   The function can be shown as:` |
|        - | 13784 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - | 13785 | ` *     errno` |
|        - | 13786 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - | 13787 | ` *   errstr` |
|        - | 13788 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - | 13789 | ` *   errfile` |
|        - | 13790 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - | 13791 | ` *     was raised in, as a string.` |
|        - | 13792 | ` *  Note:` |
|        - | 13793 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 13794 | ` * Return` |
|        - | 13795 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - | 13796 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 13797 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 13798 | ` */` |
|    10908 | 13799 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13800 |  |
|    10910 | 13801 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13802 | `	ph7_value *pOld,*pNew;` |
|        - | 13803 | `	/* Point to the old and the new handler */` |
|    10910 | 13804 | `	pOld = &pVm->aErrCB[0];` |
|    10910 | 13805 | `	pNew = &pVm->aErrCB[1];` |
|        - | 13806 | `	/* Return the old handler */` |
|    10910 | 13807 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|    10910 | 13808 | `	if( nArg > 0 ){` |
|    10910 | 13809 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 13810 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     5449 | 13811 | `			PH7_MemObjRelease(pNew);` |
|     5449 | 13812 | `			ph7_result_bool(pCtx,1);` |
|     2725 | 13813 | `		}else{` |
|     5462 | 13814 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 13815 | `			/* Install the new handler */` |
|     5462 | 13816 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 13817 | `		}` |
|     5454 | 13818 | `	}` |
|    10910 | 13819 | `	return PH7_OK;` |
|        2 | 13820 |  |
|        - | 13821 | `/*` |
|        - | 13822 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - | 13823 | ` *  Generates a backtrace.` |
|        - | 13824 | ` * Paramaeter` |
|        - | 13825 | ` *  $options` |
|        - | 13826 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - | 13827 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - | 13828 | ` *   all the function/method arguments, to save memory.` |
|        - | 13829 | ` * $limit` |
|        - | 13830 | ` *   (Not Used)` |
|        - | 13831 | ` * Return` |
|        - | 13832 | ` *  An array.The possible returned elements are as follows:` |
|        - | 13833 | ` *          Possible returned elements from debug_backtrace()` |
|        - | 13834 | ` *          Name        Type      Description` |
|        - | 13835 | ` *          ------      ------     -----------` |
|        - | 13836 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - | 13837 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - | 13838 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - | 13839 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 13840 | ` *          object      object    The current object.` |
|        - | 13841 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 13842 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 13843 | ` */` |
|      942 | 13844 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13845 |  |
|      944 | 13846 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13847 | `	ph7_value *pArray;` |
|        - | 13848 | `	ph7_class *pClass;` |
|        - | 13849 | `	ph7_value *pValue;` |
|        - | 13850 | `	SyString *pFile;` |
|        - | 13851 | `	/* Create a new array */` |
|      944 | 13852 | `	pArray = ph7_context_new_array(pCtx);` |
|      944 | 13853 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      944 | 13854 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 13855 | `		/* Out of memory,return NULL */` |
|      ! 0 | 13856 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 13857 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13858 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 13859 | `		SXUNUSED(apArg);` |
|      ! 0 | 13860 | `		return PH7_OK;` |
|        - | 13861 | `	}` |
|        - | 13862 | `	/* Dump running function name and it's arguments  */` |
|      944 | 13863 | `	if( pVm->pFrame->pParent ){` |
|      944 | 13864 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 13865 | `		ph7_vm_func *pFunc;` |
|        - | 13866 | `		ph7_value *pArg;` |
|      944 | 13867 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      944 | 13868 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      944 | 13869 | `		if( pFrame->pParent && pFunc ){` |
|      944 | 13870 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      944 | 13871 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      944 | 13872 | `			ph7_value_reset_string_cursor(pValue);` |
|      471 | 13873 | `		}` |
|        - | 13874 | `		/* Function arguments */` |
|      944 | 13875 | `		pArg = ph7_context_new_array(pCtx);` |
|      944 | 13876 | `		if( pArg  ){` |
|        - | 13877 | `			ph7_value *pObj;` |
|        - | 13878 | `			VmSlot *aSlot;` |
|        - | 13879 | `			sxu32 n;` |
|        - | 13880 | `			/* Start filling the array with the given arguments */` |
|      944 | 13881 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     3774 | 13882 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     2832 | 13883 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     2832 | 13884 | `				if( pObj ){` |
|     2832 | 13885 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|     1415 | 13886 | `				}` |
|     1417 | 13887 | `			}` |
|        - | 13888 | `			/* Save the array */` |
|      944 | 13889 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      471 | 13890 | `		}` |
|      471 | 13891 | `	}` |
|      944 | 13892 | `	ph7_value_int(pValue,1);` |
|        - | 13893 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 13894 | `	 * line numbers at run-time. )` |
|        - | 13895 | `	 */` |
|      944 | 13896 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 13897 | `	/* Current processed script */` |
|      944 | 13898 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      944 | 13899 | `	if( pFile ){` |
|      944 | 13900 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      944 | 13901 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      944 | 13902 | `		ph7_value_reset_string_cursor(pValue);` |
|      471 | 13903 | `	}` |
|        - | 13904 | `	/* Top class */` |
|      944 | 13905 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      944 | 13906 | `	if( pClass ){` |
|      940 | 13907 | `		ph7_value_reset_string_cursor(pValue);` |
|      940 | 13908 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      940 | 13909 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      469 | 13910 | `	}` |
|        - | 13911 | `	/* Return the freshly created array */` |
|      944 | 13912 | `	ph7_result_value(pCtx,pArray);` |
|        - | 13913 | `	/*` |
|        - | 13914 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 13915 | `	 * as soon we return from this function.` |
|        - | 13916 | `	 */` |
|      944 | 13917 | `	return PH7_OK;` |
|      473 | 13918 |  |
|        - | 13919 | `/*` |
|        - | 13920 | ` * Generate a small backtrace.` |
|        - | 13921 | ` * Store the generated dump in the given BLOB` |
|        - | 13922 | ` */` |
|        4 | 13923 | `static int VmMiniBacktrace(` |
|        - | 13924 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 13925 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 13926 | `	)` |
|        1 | 13927 |  |
|        5 | 13928 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 13929 | `	ph7_vm_func *pFunc;` |
|        - | 13930 | `	ph7_class *pClass;` |
|        - | 13931 | `	SyString *pFile;` |
|        - | 13932 | `	/* Called function */` |
|        5 | 13933 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 13934 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 13935 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 13936 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 13937 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 13938 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 13939 | `	}else{` |
|      ! 0 | 13940 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 13941 | `	}` |
|        5 | 13942 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 13943 | `	/* Current processed script */` |
|        5 | 13944 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 13945 | `	if( pFile ){` |
|        5 | 13946 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 13947 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 13948 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 13949 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 13950 | `	}` |
|        - | 13951 | `	/* Top class */` |
|        5 | 13952 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 13953 | `	if( pClass ){` |
|      ! 0 | 13954 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 13955 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 13956 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 13957 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 13958 | `	}` |
|        5 | 13959 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 13960 | `	/* All done */` |
|        5 | 13961 | `	return SXRET_OK;` |
|        1 | 13962 |  |
|        - | 13963 | `/*` |
|        - | 13964 | ` * void debug_print_backtrace()` |
|        - | 13965 | ` *  Prints a backtrace` |
|        - | 13966 | ` * Parameters` |
|        - | 13967 | ` * None` |
|        - | 13968 | ` * Return` |
|        - | 13969 | ` * NULL` |
|        - | 13970 | ` */` |
|        2 | 13971 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13972 |  |
|        3 | 13973 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13974 | `	SyBlob sDump;` |
|        3 | 13975 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 13976 | `	/* Generate the backtrace */` |
|        3 | 13977 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 13978 | `	/* Output backtrace */` |
|        3 | 13979 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13980 | `	/* All done,cleanup */` |
|        3 | 13981 | `	SyBlobRelease(&sDump);` |
|        1 | 13982 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 13983 | `	SXUNUSED(apArg);` |
|        3 | 13984 | `	return PH7_OK;` |
|        1 | 13985 |  |
|        - | 13986 | `/*` |
|        - | 13987 | ` * string debug_string_backtrace()` |
|        - | 13988 | ` *  Generate a backtrace` |
|        - | 13989 | ` * Parameters` |
|        - | 13990 | ` * None` |
|        - | 13991 | ` * Return` |
|        - | 13992 | ` *  A mini backtrace().` |
|        - | 13993 | ` * Note that this is a symisc extension.` |
|        - | 13994 | ` */` |
|        2 | 13995 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13996 |  |
|        3 | 13997 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13998 | `	SyBlob sDump;` |
|        3 | 13999 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 14000 | `	/* Generate the backtrace */` |
|        3 | 14001 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 14002 | `	/* Return the backtrace */` |
|        3 | 14003 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 14004 | `	/* All done,cleanup */` |
|        3 | 14005 | `	SyBlobRelease(&sDump);` |
|        1 | 14006 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 14007 | `	SXUNUSED(apArg);` |
|        3 | 14008 | `	return PH7_OK;` |
|        1 | 14009 |  |
|        - | 14010 | `/*` |
|        - | 14011 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 14012 | ` * exception is triggered.` |
|        - | 14013 | ` */` |
|      512 | 14014 | `static sxi32 VmUncaughtException(` |
|        - | 14015 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 14016 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 14017 | `	)` |
|        1 | 14018 |  |
|        - | 14019 | `	ph7_value *apArg[2],sArg;` |
|      513 | 14020 | `	int nArg = 1;` |
|        - | 14021 | `	sxi32 rc;` |
|      513 | 14022 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 14023 | `		/* Nesting limit reached */` |
|      ! 0 | 14024 | `		return SXRET_OK;` |
|        - | 14025 | `	}` |
|        - | 14026 | `	/* Call any exception handler if available */` |
|      513 | 14027 | `	PH7_MemObjInit(pVm,&sArg);` |
|      513 | 14028 | `	if( pThis ){` |
|        - | 14029 | `		/* Load the exception instance */` |
|      513 | 14030 | `		sArg.x.pOther = pThis;` |
|      513 | 14031 | `		pThis->iRef++;` |
|      513 | 14032 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      257 | 14033 | `	}else{` |
|      ! 0 | 14034 | `		nArg = 0;` |
|        - | 14035 | `	}` |
|      513 | 14036 | `	apArg[0] = &sArg;` |
|        - | 14037 | `	/* Call the exception handler if available */` |
|      513 | 14038 | `	pVm->nExceptDepth++;` |
|      513 | 14039 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      513 | 14040 | `	pVm->nExceptDepth--;` |
|      513 | 14041 | `	if( rc != SXRET_OK ){` |
|        - | 14042 | `		SyBlob sMsgBuf;` |
|      511 | 14043 | `		const char *zClass = "Exception";` |
|      511 | 14044 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 14045 | `		const char *zMsg;` |
|        - | 14046 | `		sxu32 nMsg;` |
|        - | 14047 | `		const char *zFuncName;` |
|        - | 14048 | `		int nFuncLen;` |
|      511 | 14049 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      511 | 14050 | `		if( pThis ){` |
|        - | 14051 | `			ph7_class_method *pGetMessage;` |
|        - | 14052 | `			ph7_value sMsg;` |
|        - | 14053 | `			const char *zTmp;` |
|        - | 14054 | `			int nTmp;` |
|      511 | 14055 | `			zClass = pThis->pClass->sName.zString;` |
|      511 | 14056 | `			nClass = pThis->pClass->sName.nByte;` |
|      511 | 14057 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      511 | 14058 | `			if( pGetMessage ){` |
|      511 | 14059 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      511 | 14060 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      511 | 14061 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      511 | 14062 | `					if( zTmp && nTmp > 0 ){` |
|      511 | 14063 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      255 | 14064 | `					}` |
|      255 | 14065 | `				}` |
|      511 | 14066 | `				PH7_MemObjRelease(&sMsg);` |
|      255 | 14067 | `			}` |
|      255 | 14068 | `		}` |
|      511 | 14069 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      511 | 14070 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      511 | 14071 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      511 | 14072 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      511 | 14073 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 14074 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      511 | 14075 | `		rc = SXERR_ABORT;` |
|      255 | 14076 | `	}` |
|      513 | 14077 | `	PH7_MemObjRelease(&sArg);` |
|      513 | 14078 | `	return rc;` |
|      257 | 14079 |  |
|        - | 14080 | `/*` |
|        - | 14081 | ` * Throw a user exception.` |
|        - | 14082 | ` *` |
|        - | 14083 | ` * Exception dispatch follows this sequence:` |
|        - | 14084 | ` *` |
|        - | 14085 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 14086 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 14087 | ` *` |
|        - | 14088 | ` * 2. If NO catch matches:` |
|        - | 14089 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 14090 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 14091 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 14092 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 14093 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 14094 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 14095 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 14096 | ` *` |
|        - | 14097 | ` * 3. If a catch DOES match:` |
|        - | 14098 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 14099 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 14100 | ` *       inside the catch body from immediately propagating past our` |
|        - | 14101 | ` *       finally block.` |
|        - | 14102 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 14103 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 14104 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 14105 | ` *       in pPendingException (step 2c).` |
|        - | 14106 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 14107 | ` *    d. Run finally (if present).` |
|        - | 14108 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 14109 | ` *       that handlers are restored and finally has run.` |
|        - | 14110 | ` */` |
|      872 | 14111 | `static sxi32 VmThrowException(` |
|        - | 14112 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 14113 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 14114 | `	)` |
|        2 | 14115 |  |
|        - | 14116 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 14117 | `	ph7_exception **apException;` |
|        - | 14118 | `	ph7_exception *pException;` |
|        - | 14119 | `	/* An in-flight throw abandons any pending null-coalesce-assign store:` |
|        - | 14120 | `	 * disarm so the RHS-evaluation throw can't leave the slot live for a` |
|        - | 14121 | `	 * later unrelated NULLC_STORE (stale offsetSet) or leak the instance ref. */` |
|      874 | 14122 | `	VmCoalesceDisarm(pVm);` |
|        - | 14123 | `	/* Point to the stack of loaded exceptions */` |
|      874 | 14124 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      874 | 14125 | `	pException = 0;` |
|      874 | 14126 | `	pCatch = 0;` |
|      874 | 14127 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 14128 | `		ph7_exception_block *aCatch;` |
|        - | 14129 | `		ph7_class *pClass;` |
|        - | 14130 | `		SyString *aNames;` |
|        - | 14131 | `		sxu32 nNames;` |
|        - | 14132 | `		int matched;` |
|        - | 14133 | `		sxu32 j,k;` |
|        - | 14134 | `		/* Locate the appropriate block to execute */` |
|      354 | 14135 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|      354 | 14136 | `		(void)SySetPop(&pVm->aException);` |
|      354 | 14137 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|      362 | 14138 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|        - | 14139 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
|      360 | 14140 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
|      360 | 14141 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
|      360 | 14142 | `			matched = 0;` |
|      386 | 14143 | `			for( k = 0 ; k < nNames ; ++k ){` |
|        - | 14144 | `				/* Extract the target class or interface (iLoadable=FALSE so` |
|        - | 14145 | `				 * interfaces like Throwable are resolvable as catch targets).` |
|        - | 14146 | `				 * Traits are never instance-compatible, so skip them explicitly. */` |
|      378 | 14147 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,FALSE,0);` |
|      378 | 14148 | `				if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - | 14149 | `					/* No such class, or trait — cannot match */` |
|      ! 0 | 14150 | `					continue;` |
|        - | 14151 | `				}` |
|      378 | 14152 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      352 | 14153 | `					matched = 1;` |
|      352 | 14154 | `					break;` |
|        - | 14155 | `				}` |
|       14 | 14156 | `			}` |
|      360 | 14157 | `			if( matched ){` |
|        - | 14158 | `				/* Catch block found,break immediately */` |
|      352 | 14159 | `				pCatch = &aCatch[j];` |
|      352 | 14160 | `				break;` |
|        - | 14161 | `			}` |
|        5 | 14162 | `		}` |
|      176 | 14163 | `	}` |
|        - | 14164 | `	/* Execute the cached block if available */` |
|      874 | 14165 | `	if( pCatch == 0 ){` |
|        - | 14166 | `		sxi32 rc;` |
|        - | 14167 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      524 | 14168 | `		if( pException && pException->iHasFinally ){` |
|        3 | 14169 | `			pException->iFinallyDone = 1;` |
|        3 | 14170 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 | 14171 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 14172 | `				return SXERR_ABORT;` |
|        - | 14173 | `			}` |
|        1 | 14174 | `		}` |
|        - | 14175 | `		/* Check if there is an outer exception handler on the stack */` |
|      524 | 14176 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 14177 | `			/* Re-throw to the outer handler */` |
|        3 | 14178 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 14179 | `		}` |
|        - | 14180 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 14181 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 14182 | `		 * exception instead of reporting it uncaught.` |
|        - | 14183 | `		 */` |
|      522 | 14184 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 14185 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 14186 | `			 * by looking for a catch frame on the stack.` |
|        - | 14187 | `			 */` |
|      522 | 14188 | `			VmFrame *pF = pVm->pFrame;` |
|      522 | 14189 | `			int inCatch = 0;` |
|     1050 | 14190 | `			while( pF ){` |
|      538 | 14191 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        9 | 14192 | `					inCatch = 1;` |
|        9 | 14193 | `					break;` |
|        - | 14194 | `				}` |
|      529 | 14195 | `				pF = pF->pParent;` |
|        1 | 14196 | `			}` |
|      522 | 14197 | `			if( inCatch ){` |
|        - | 14198 | `				/* Defer — will be re-thrown after finally runs */` |
|        9 | 14199 | `				pThis->iRef++;` |
|        9 | 14200 | `				pVm->pPendingException = pThis;` |
|        9 | 14201 | `				return SXRET_OK;` |
|        - | 14202 | `			}` |
|      256 | 14203 | `		}` |
|        - | 14204 | `		/* Truly uncaught */` |
|      513 | 14205 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      513 | 14206 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 14207 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 14208 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 14209 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 14210 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 14211 | `			}` |
|      ! 0 | 14212 | `		}` |
|      513 | 14213 | `		return rc;` |
|      ! 0 | 14214 | `	}else{` |
|      352 | 14215 | `		VmFrame *pFrame = pVm->pFrame;` |
|      352 | 14216 | `		ph7_exception **apSaved = 0;` |
|        - | 14217 | `		sxu32 nSavedCount;` |
|        - | 14218 | `		sxi32 rc;` |
|      352 | 14219 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      352 | 14220 | `		if( pException->pFrame == pFrame ){` |
|      240 | 14221 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      119 | 14222 | `		}` |
|        - | 14223 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 14224 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 14225 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 14226 | `		 */` |
|      352 | 14227 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|      352 | 14228 | `		if( nSavedCount > 0 ){` |
|       16 | 14229 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        5 | 14230 | `				nSavedCount * sizeof(ph7_exception *));` |
|       11 | 14231 | `			if( apSaved ){` |
|       16 | 14232 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        5 | 14233 | `					nSavedCount * sizeof(ph7_exception *));` |
|       11 | 14234 | `				SySetReset(&pVm->aException);` |
|        5 | 14235 | `			}` |
|        5 | 14236 | `		}` |
|        - | 14237 | `		/* Create a private frame first */` |
|      352 | 14238 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|      352 | 14239 | `		if( rc == SXRET_OK ){` |
|      352 | 14240 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|      352 | 14241 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|      352 | 14242 | `			if( pObj ){` |
|      352 | 14243 | `				pThis->iRef++;` |
|      352 | 14244 | `				pObj->x.pOther = pThis;` |
|      352 | 14245 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      175 | 14246 | `			}` |
|        - | 14247 | `			/* Execute the catch block */` |
|      352 | 14248 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - | 14249 | `			/* Leave the frame */` |
|      352 | 14250 | `			VmLeaveFrame(&(*pVm));` |
|      175 | 14251 | `		}` |
|        - | 14252 | `		/* Restore the outer exception handlers */` |
|      352 | 14253 | `		if( apSaved ){` |
|        - | 14254 | `			sxu32 k;` |
|        - | 14255 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 14256 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 14257 | `			 * Restore the original outer entries.` |
|        - | 14258 | `			 */` |
|       11 | 14259 | `			SySetReset(&pVm->aException);` |
|       21 | 14260 | `			for(k = 0; k < nSavedCount; k++){` |
|       11 | 14261 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        6 | 14262 | `			}` |
|       11 | 14263 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        5 | 14264 | `		}` |
|        - | 14265 | `		/* Execute the finally block after catch */` |
|      352 | 14266 | `		if( pException->iHasFinally ){` |
|       16 | 14267 | `			pException->iFinallyDone = 1;` |
|        - | 14268 | `			{` |
|       16 | 14269 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       16 | 14270 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 14271 | `					return SXERR_ABORT;` |
|        - | 14272 | `				}` |
|        - | 14273 | `			}` |
|        7 | 14274 | `		}` |
|      352 | 14275 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 14276 | `			return SXERR_ABORT;` |
|        - | 14277 | `		}` |
|        - | 14278 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 14279 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 14280 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - | 14281 | `		 */` |
|      352 | 14282 | `		if( pVm->pPendingException ){` |
|        9 | 14283 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        9 | 14284 | `			pVm->pPendingException = 0;` |
|        9 | 14285 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - | 14286 | `		}` |
|        - | 14287 | `	}` |
|        - | 14288 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 14289 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 14290 | `	 */` |
|      344 | 14291 | `	return SXRET_OK;` |
|      438 | 14292 |  |
|        - | 14293 | `/*` |
|        - | 14294 | ` * Section:` |
|        - | 14295 | ` *  Version,Credits and Copyright related functions.` |
|        - | 14296 | ` * Status:` |
|        - | 14297 | ` *    Stable.` |
|        - | 14298 | ` */` |
|        - | 14299 | `/*` |
|        - | 14300 | ` * string ph7version(void)` |
|        - | 14301 | ` *  Returns the running version of the PH7 version.` |
|        - | 14302 | ` * Parameters` |
|        - | 14303 | ` *  None` |
|        - | 14304 | ` * Return` |
|        - | 14305 | ` * Current PH7 version.` |
|        - | 14306 | ` */` |
|        2 | 14307 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14308 |  |
|        1 | 14309 | `	SXUNUSED(nArg);` |
|        1 | 14310 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 14311 | `	/* Current engine version */` |
|        3 | 14312 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 14313 | `	return PH7_OK;` |
|        1 | 14314 |  |
|        - | 14315 | `/*` |
|        - | 14316 | ` * string phpversion([ string $extension ])` |
|        - | 14317 | ` *  Returns the PHP-compatibility version PHL advertises (see PHP_COMPAT_VERSION).` |
|        - | 14318 | ` * Parameters` |
|        - | 14319 | ` *  $extension (optional): an extension name. PHL has no extension registry, so any` |
|        - | 14320 | ` *  argument yields NULL (PHP returns FALSE for an unknown extension).` |
|        - | 14321 | ` * Return` |
|        - | 14322 | ` *  The PHP-compat version string, or NULL when called with an extension argument.` |
|        - | 14323 | ` */` |
|        4 | 14324 | `static int vm_builtin_phpversion(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14325 |  |
|        2 | 14326 | `	SXUNUSED(apArg); /* cc warning */` |
|        5 | 14327 | `	if( nArg > 0 ){` |
|      ! 0 | 14328 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14329 | `		return PH7_OK;` |
|        - | 14330 | `	}` |
|        5 | 14331 | `	ph7_result_string(pCtx,PHP_COMPAT_VERSION,(int)sizeof(PHP_COMPAT_VERSION) - 1);` |
|        5 | 14332 | `	return PH7_OK;` |
|        3 | 14333 |  |
|        - | 14334 | `/*` |
|        - | 14335 | ` * string php_sapi_name(void)` |
|        - | 14336 | ` *  Returns the type of interface (SAPI) PHL is running under.` |
|        - | 14337 | ` * Parameters` |
|        - | 14338 | ` *  None` |
|        - | 14339 | ` * Return` |
|        - | 14340 | ` *  "cli-server" while serving an HTTP request via the built-in -S server, "cli" otherwise.` |
|        - | 14341 | ` */` |
|        2 | 14342 | `static int vm_builtin_php_sapi_name(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14343 |  |
|        3 | 14344 | `	const char *zSapi = pCtx->pVm->bHttpContext ? "cli-server" : "cli";` |
|        1 | 14345 | `	SXUNUSED(nArg);` |
|        1 | 14346 | `	SXUNUSED(apArg); /* cc warning */` |
|        3 | 14347 | `	ph7_result_string(pCtx,zSapi,-1);` |
|        3 | 14348 | `	return PH7_OK;` |
|        1 | 14349 |  |
|        - | 14350 | `/*` |
|        - | 14351 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 14352 | ` */` |
|        - | 14353 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 14354 | ` "<html><head>"\` |
|        - | 14355 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 14356 | ` "<style type=\"text/css\">"\` |
|        - | 14357 | ` "div {"\` |
|        - | 14358 | `     "border: 1px solid #cccccc;"\` |
|        - | 14359 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 14360 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 14361 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 14362 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 14363 | `     "-webkit-border-radius: 10px;"\` |
|        - | 14364 | `     "-o-border-radius: 10px;"\` |
|        - | 14365 | `     "border-radius: 10px;"\` |
|        - | 14366 | `     "padding-left: 2em;"\` |
|        - | 14367 | `     "background-color: white;"\` |
|        - | 14368 | `     "margin-left: auto;"\` |
|        - | 14369 | `     "font-family: verdana;"\` |
|        - | 14370 | `     "padding-right: 2em;"\` |
|        - | 14371 | `     "margin-right: auto;"\` |
|        - | 14372 | `     "}"\` |
|        - | 14373 | `     "body {"\` |
|        - | 14374 | `     "padding: 0.2em;"\` |
|        - | 14375 | `     "font-style: normal;"\` |
|        - | 14376 | `     "font-size: medium;"\` |
|        - | 14377 | `     "background-color: #f2f2f2;"\` |
|        - | 14378 | `     "}"\` |
|        - | 14379 | `     "hr {"\` |
|        - | 14380 | `     "border-style: solid none none;"\` |
|        - | 14381 | `     "border-width: 1px medium medium;"\` |
|        - | 14382 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 14383 | `     "height: 1px;"\` |
|        - | 14384 | `     "}"\` |
|        - | 14385 | `     "a {"\` |
|        - | 14386 | `     "color: #3366cc;"\` |
|        - | 14387 | `     "text-decoration: none;"\` |
|        - | 14388 | `     "}"\` |
|        - | 14389 | `     "a:hover {"\` |
|        - | 14390 | `     "color: #999999;"\` |
|        - | 14391 | `     "}"\` |
|        - | 14392 | `     "a:active {"\` |
|        - | 14393 | `     "color: #663399;"\` |
|        - | 14394 | `     "}"\` |
|        - | 14395 | `     "h1 {"\` |
|        - | 14396 | `     "margin: 0;"\` |
|        - | 14397 | `     "padding: 0;"\` |
|        - | 14398 | `     "font-family: Verdana;"\` |
|        - | 14399 | `     "font-weight: bold;"\` |
|        - | 14400 | `     "font-style: normal;"\` |
|        - | 14401 | `     "font-size: medium;"\` |
|        - | 14402 | `     "text-transform: capitalize;"\` |
|        - | 14403 | `     "color: #0a328c;"\` |
|        - | 14404 | `     "}"\` |
|        - | 14405 | `     "p {"\` |
|        - | 14406 | `     "margin: 0 auto;"\` |
|        - | 14407 | `     "font-size: medium;"\` |
|        - | 14408 | `     "font-style: normal;"\` |
|        - | 14409 | `     "font-family: verdana;"\` |
|        - | 14410 | `     "}"\` |
|        - | 14411 | `"</style></head><body>"\` |
|        - | 14412 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 14413 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 14414 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 14415 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 14416 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 14417 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 14418 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 14419 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 14420 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 14421 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 14422 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 14423 |  |
|        - | 14424 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 14425 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 14426 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 14427 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 14428 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 14429 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 14430 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 14431 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 14432 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 14433 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 14434 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 14435 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 14436 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 14437 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 14438 |  |
|        - | 14439 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 14440 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 14441 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 14442 | `"&nbsp;*<br>"\` |
|        - | 14443 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 14444 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 14445 | `"&nbsp;* are met:<br>"\` |
|        - | 14446 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 14447 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 14448 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 14449 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 14450 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 14451 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 14452 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 14453 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 14454 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 14455 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 14456 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 14457 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 14458 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 14459 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 14460 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 14461 | `"&nbsp;*<br>"\` |
|        - | 14462 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 14463 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 14464 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 14465 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 14466 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 14467 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 14468 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 14469 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 14470 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 14471 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 14472 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 14473 | `"&nbsp;*/<br>"\` |
|        - | 14474 | `"</span></small></small></p>"\` |
|        - | 14475 | `"</div></body></html>"` |
|        - | 14476 | `/*` |
|        - | 14477 | ` * bool ph7credits(void)` |
|        - | 14478 | ` * bool ph7info(void)` |
|        - | 14479 | ` * bool ph7copyright(void)` |
|        - | 14480 | ` *  Prints out the credits for PH7 engine` |
|        - | 14481 | ` * Parameters` |
|        - | 14482 | ` *  None` |
|        - | 14483 | ` * Return` |
|        - | 14484 | ` *  Always TRUE` |
|        - | 14485 | ` */` |
|        2 | 14486 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14487 |  |
|        3 | 14488 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 14489 | `	/* Expand the HTML page above*/` |
|        3 | 14490 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 14491 | `	ph7_context_output_format(` |
|        1 | 14492 | `		pCtx,` |
|        - | 14493 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 14494 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 14495 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 14496 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 14497 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 14498 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 14499 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 14500 | `#ifdef __WINNT__` |
|        - | 14501 | `		"Windows NT"` |
|        - | 14502 | `#elif defined(__UNIXES__)` |
|        - | 14503 | `		"UNIX-Like"` |
|        - | 14504 | `#else` |
|        - | 14505 | `		"Other OS"` |
|        - | 14506 | `#endif` |
|        - | 14507 | `		);` |
|        3 | 14508 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 14509 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 14510 | `	SXUNUSED(apArg);` |
|        - | 14511 | `	/* Return TRUE */` |
|        - | 14512 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 14513 | `	return PH7_OK;` |
|        1 | 14514 |  |
|        - | 14515 | `/*` |
|        - | 14516 | ` * Section:` |
|        - | 14517 | ` *    URL related routines.` |
|        - | 14518 | ` * Status:` |
|        - | 14519 | ` *    Stable.` |
|        - | 14520 | ` */` |
|        - | 14521 | `/*` |
|        - | 14522 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 14523 | ` *  Parse a URL and return its fields.` |
|        - | 14524 | ` * Parameters` |
|        - | 14525 | ` *  $url` |
|        - | 14526 | ` *   The URL to parse.` |
|        - | 14527 | ` * $component` |
|        - | 14528 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 14529 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 14530 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 14531 | ` *  in which case the return value will be an integer).` |
|        - | 14532 | ` * Return` |
|        - | 14533 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 14534 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 14535 | ` *  this array are:` |
|        - | 14536 | ` *   scheme - e.g. http` |
|        - | 14537 | ` *   host` |
|        - | 14538 | ` *   port` |
|        - | 14539 | ` *   user` |
|        - | 14540 | ` *   pass` |
|        - | 14541 | ` *   path` |
|        - | 14542 | ` *   query - after the question mark ?` |
|        - | 14543 | ` *   fragment - after the hashmark #` |
|        - | 14544 | ` * Note:` |
|        - | 14545 | ` *  FALSE is returned on failure.` |
|        - | 14546 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 14547 | ` *  with the standard PHP engine.` |
|        - | 14548 | ` */` |
|       28 | 14549 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14550 |  |
|        - | 14551 | `	const char *zStr; /* Input string */` |
|        - | 14552 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 14553 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 14554 | `	int nLen;` |
|        - | 14555 | `	sxi32 rc;` |
|       29 | 14556 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 14557 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 14558 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14559 | `		return PH7_OK;` |
|        - | 14560 | `	}` |
|        - | 14561 | `	/* Extract the given URI */` |
|       29 | 14562 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 14563 | `	if( nLen < 1 ){` |
|        - | 14564 | `		/* Nothing to process,return FALSE */` |
|        3 | 14565 | `		ph7_result_bool(pCtx,0);` |
|        3 | 14566 | `		return PH7_OK;` |
|        - | 14567 | `	}` |
|        - | 14568 | `	/* Get a parse */` |
|       27 | 14569 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 14570 | `	if( rc != SXRET_OK ){` |
|        - | 14571 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 14572 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14573 | `		return PH7_OK;` |
|        - | 14574 | `	}` |
|       27 | 14575 | `	if( nArg > 1 ){` |
|      ! 0 | 14576 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 14577 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 14578 | `		switch(nComponent){` |
|      ! 0 | 14579 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 14580 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 14581 | `			if( pComp->nByte < 1 ){` |
|        - | 14582 | `				/* No available value,return NULL */` |
|      ! 0 | 14583 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14584 | `			}else{` |
|      ! 0 | 14585 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14586 | `			}` |
|      ! 0 | 14587 | `			break;` |
|      ! 0 | 14588 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 14589 | `			pComp = &sURI.sHost;` |
|      ! 0 | 14590 | `			if( pComp->nByte < 1 ){` |
|        - | 14591 | `				/* No available value,return NULL */` |
|      ! 0 | 14592 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14593 | `			}else{` |
|      ! 0 | 14594 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14595 | `			}` |
|      ! 0 | 14596 | `			break;` |
|      ! 0 | 14597 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 14598 | `			pComp = &sURI.sPort;` |
|      ! 0 | 14599 | `			if( pComp->nByte < 1 ){` |
|        - | 14600 | `				/* No available value,return NULL */` |
|      ! 0 | 14601 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14602 | `			}else{` |
|      ! 0 | 14603 | `				int iPort = 0;` |
|        - | 14604 | `				/* Cast the value to integer */` |
|      ! 0 | 14605 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 14606 | `				ph7_result_int(pCtx,iPort);` |
|        - | 14607 | `			}` |
|      ! 0 | 14608 | `			break;` |
|      ! 0 | 14609 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 14610 | `			pComp = &sURI.sUser;` |
|      ! 0 | 14611 | `			if( pComp->nByte < 1 ){` |
|        - | 14612 | `				/* No available value,return NULL */` |
|      ! 0 | 14613 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14614 | `			}else{` |
|      ! 0 | 14615 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14616 | `			}` |
|      ! 0 | 14617 | `			break;` |
|      ! 0 | 14618 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 14619 | `			pComp = &sURI.sPass;` |
|      ! 0 | 14620 | `			if( pComp->nByte < 1 ){` |
|        - | 14621 | `				/* No available value,return NULL */` |
|      ! 0 | 14622 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14623 | `			}else{` |
|      ! 0 | 14624 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14625 | `			}` |
|      ! 0 | 14626 | `			break;` |
|      ! 0 | 14627 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 14628 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 14629 | `			if( pComp->nByte < 1 ){` |
|        - | 14630 | `				/* No available value,return NULL */` |
|      ! 0 | 14631 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14632 | `			}else{` |
|      ! 0 | 14633 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14634 | `			}` |
|      ! 0 | 14635 | `			break;` |
|      ! 0 | 14636 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 14637 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 14638 | `			if( pComp->nByte < 1 ){` |
|        - | 14639 | `				/* No available value,return NULL */` |
|      ! 0 | 14640 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14641 | `			}else{` |
|      ! 0 | 14642 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14643 | `			}` |
|      ! 0 | 14644 | `			break;` |
|      ! 0 | 14645 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 14646 | `			pComp = &sURI.sPath;` |
|      ! 0 | 14647 | `			if( pComp->nByte < 1 ){` |
|        - | 14648 | `				/* No available value,return NULL */` |
|      ! 0 | 14649 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14650 | `			}else{` |
|      ! 0 | 14651 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14652 | `			}` |
|      ! 0 | 14653 | `			break;` |
|      ! 0 | 14654 | `		default:` |
|        - | 14655 | `			/* No such entry,return NULL */` |
|      ! 0 | 14656 | `			ph7_result_null(pCtx);` |
|      ! 0 | 14657 | `			break;` |
|        - | 14658 | `		}` |
|      ! 0 | 14659 | `	}else{` |
|        - | 14660 | `		ph7_value *pArray,*pValue;` |
|        - | 14661 | `		/* Return an associative array */` |
|       27 | 14662 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 14663 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 14664 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 14665 | `			/* Out of memory */` |
|      ! 0 | 14666 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 14667 | `			/* Return false */` |
|      ! 0 | 14668 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 14669 | `			return PH7_OK;` |
|        - | 14670 | `		}` |
|        - | 14671 | `		/* Fill the array */` |
|       27 | 14672 | `		pComp = &sURI.sScheme;` |
|       27 | 14673 | `		if( pComp->nByte > 0 ){` |
|       19 | 14674 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 14675 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 14676 | `		}` |
|        - | 14677 | `		/* Reset the string cursor */` |
|       27 | 14678 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14679 | `		pComp = &sURI.sHost;` |
|       27 | 14680 | `		if( pComp->nByte > 0 ){` |
|       25 | 14681 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 14682 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 14683 | `		}` |
|        - | 14684 | `		/* Reset the string cursor */` |
|       27 | 14685 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14686 | `		pComp = &sURI.sPort;` |
|       27 | 14687 | `		if( pComp->nByte > 0 ){` |
|       11 | 14688 | `			int iPort = 0;/* cc warning */` |
|        - | 14689 | `			/* Convert to integer */` |
|       11 | 14690 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 14691 | `			ph7_value_int(pValue,iPort);` |
|       11 | 14692 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 14693 | `		}` |
|        - | 14694 | `		/* Reset the string cursor */` |
|       27 | 14695 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14696 | `		pComp = &sURI.sUser;` |
|       27 | 14697 | `		if( pComp->nByte > 0 ){` |
|        7 | 14698 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 14699 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 14700 | `		}` |
|        - | 14701 | `		/* Reset the string cursor */` |
|       27 | 14702 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14703 | `		pComp = &sURI.sPass;` |
|       27 | 14704 | `		if( pComp->nByte > 0 ){` |
|        7 | 14705 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 14706 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 14707 | `		}` |
|        - | 14708 | `		/* Reset the string cursor */` |
|       27 | 14709 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14710 | `		pComp = &sURI.sPath;` |
|       27 | 14711 | `		if( pComp->nByte > 0 ){` |
|       17 | 14712 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 14713 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 14714 | `		}` |
|        - | 14715 | `		/* Reset the string cursor */` |
|       27 | 14716 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14717 | `		pComp = &sURI.sQuery;` |
|       27 | 14718 | `		if( pComp->nByte > 0 ){` |
|        5 | 14719 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 14720 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 14721 | `		}` |
|        - | 14722 | `		/* Reset the string cursor */` |
|       27 | 14723 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14724 | `		pComp = &sURI.sFragment;` |
|       27 | 14725 | `		if( pComp->nByte > 0 ){` |
|        5 | 14726 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 14727 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 14728 | `		}` |
|        - | 14729 | `		/* Return the created array */` |
|       27 | 14730 | `		ph7_result_value(pCtx,pArray);` |
|        - | 14731 | `		/* NOTE:` |
|        - | 14732 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 14733 | `		 * automatically as soon we return from this function.` |
|        - | 14734 | `		 */` |
|        - | 14735 | `	}` |
|        - | 14736 | `	/* All done */` |
|       27 | 14737 | `	return PH7_OK;` |
|       15 | 14738 |  |
|        - | 14739 | `/*` |
|        - | 14740 | ` * Section:` |
|        - | 14741 | ` *   Array related routines.` |
|        - | 14742 | ` * Status:` |
|        - | 14743 | ` *    Stable.` |
|        - | 14744 | ` * Note 2012-5-21 01:04:15:` |
|        - | 14745 | ` *  Array related functions that need access to the underlying` |
|        - | 14746 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 14747 | ` */` |
|        - | 14748 | `/*` |
|        - | 14749 | ` * The [compact()] function store it's state information in an instance` |
|        - | 14750 | ` * of the following structure.` |
|        - | 14751 | ` */` |
|        - | 14752 | `struct compact_data` |
|        - | 14753 |  |
|        - | 14754 | `	ph7_value *pArray;  /* Target array */` |
|        - | 14755 | `	int nRecCount;      /* Recursion count */` |
|        - | 14756 | `};` |
|        - | 14757 | `/*` |
|        - | 14758 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 14759 | ` */` |
|      ! 0 | 14760 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 14761 |  |
|      ! 0 | 14762 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 14763 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 14764 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 14765 | `	/* Act according to the hashmap value */` |
|      ! 0 | 14766 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 14767 | `		SyString sVar;` |
|      ! 0 | 14768 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 14769 | `		if( sVar.nByte > 0 ){` |
|        - | 14770 | `			/* Query the current frame */` |
|      ! 0 | 14771 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 14772 | `			/* ^` |
|        - | 14773 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 14774 | `			 */` |
|      ! 0 | 14775 | `			if( pKey ){` |
|        - | 14776 | `				/* Perform the insertion */` |
|      ! 0 | 14777 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 14778 | `			}` |
|      ! 0 | 14779 | `		}` |
|      ! 0 | 14780 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 14781 | `		int rc;` |
|        - | 14782 | `		/* Recursively traverse this array */` |
|      ! 0 | 14783 | `		pData->nRecCount++;` |
|      ! 0 | 14784 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 14785 | `		pData->nRecCount--;` |
|      ! 0 | 14786 | `		return rc;` |
|        - | 14787 | `	}` |
|      ! 0 | 14788 | `	return SXRET_OK;` |
|      ! 0 | 14789 |  |
|        - | 14790 | `/*` |
|        - | 14791 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 14792 | ` *  Create array containing variables and their values.` |
|        - | 14793 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 14794 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 14795 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 14796 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 14797 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 14798 | ` * Parameters` |
|        - | 14799 | ` *  $varname` |
|        - | 14800 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 14801 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 14802 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 14803 | ` *   it recursively.` |
|        - | 14804 | ` * Return` |
|        - | 14805 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 14806 | ` */` |
|        2 | 14807 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14808 |  |
|        - | 14809 | `	ph7_value *pArray,*pObj;` |
|        3 | 14810 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14811 | `	const char *zName;` |
|        - | 14812 | `	SyString sVar;` |
|        - | 14813 | `	int i,nLen;` |
|        3 | 14814 | `	if( nArg < 1 ){` |
|        - | 14815 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 14816 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14817 | `		return PH7_OK;` |
|        - | 14818 | `	}` |
|        - | 14819 | `	/* Create the array */` |
|        3 | 14820 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 14821 | `	if( pArray == 0 ){` |
|        - | 14822 | `		/* Out of memory */` |
|      ! 0 | 14823 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 14824 | `		/* Return NULL */` |
|      ! 0 | 14825 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14826 | `		return PH7_OK;` |
|        - | 14827 | `	}` |
|        - | 14828 | `	/* Perform the requested operation */` |
|        7 | 14829 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 14830 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 14831 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 14832 | `				struct compact_data sData;` |
|      ! 0 | 14833 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 14834 | `				/* Recursively walk the array */` |
|      ! 0 | 14835 | `				sData.nRecCount = 0;` |
|      ! 0 | 14836 | `				sData.pArray = pArray;` |
|      ! 0 | 14837 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 14838 | `			}` |
|      ! 0 | 14839 | `		}else{` |
|        - | 14840 | `			/* Extract variable name */` |
|        5 | 14841 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 14842 | `			if( nLen > 0 ){` |
|        5 | 14843 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 14844 | `				/* Check if the variable is available in the current frame */` |
|        5 | 14845 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 14846 | `				if( pObj ){` |
|        5 | 14847 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 14848 | `				}` |
|        2 | 14849 | `			}` |
|        - | 14850 | `		}` |
|        3 | 14851 | `	}` |
|        - | 14852 | `	/* Return the array */` |
|        3 | 14853 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 14854 | `	return PH7_OK;` |
|        2 | 14855 |  |
|        - | 14856 | `/*` |
|        - | 14857 | ` * The [extract()] function store it's state information in an instance` |
|        - | 14858 | ` * of the following structure.` |
|        - | 14859 | ` */` |
|        - | 14860 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 14861 | `struct extract_aux_data` |
|        - | 14862 |  |
|        - | 14863 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 14864 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 14865 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 14866 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 14867 | `	int iFlags;           /* Control flags */` |
|        - | 14868 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 14869 | `};` |
|        - | 14870 | `/* Forward declaration */` |
|        - | 14871 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 14872 | `/*` |
|        - | 14873 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 14874 | ` *   Import variables into the current symbol table from an array.` |
|        - | 14875 | ` * Parameters` |
|        - | 14876 | ` * $var_array` |
|        - | 14877 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 14878 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 14879 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 14880 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 14881 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 14882 | ` * $extract_type` |
|        - | 14883 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 14884 | ` *  It can be one of the following values:` |
|        - | 14885 | ` *   EXTR_OVERWRITE` |
|        - | 14886 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 14887 | ` *   EXTR_SKIP` |
|        - | 14888 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 14889 | ` *   EXTR_PREFIX_SAME` |
|        - | 14890 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 14891 | ` *   EXTR_PREFIX_ALL` |
|        - | 14892 | ` *       Prefix all variable names with prefix.` |
|        - | 14893 | ` *   EXTR_PREFIX_INVALID` |
|        - | 14894 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 14895 | ` *   EXTR_IF_EXISTS` |
|        - | 14896 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 14897 | ` *       otherwise do nothing.` |
|        - | 14898 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 14899 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 14900 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 14901 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 14902 | ` *      the current symbol table.` |
|        - | 14903 | ` * $prefix` |
|        - | 14904 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 14905 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 14906 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 14907 | ` *  underscore character.` |
|        - | 14908 | ` * Return` |
|        - | 14909 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 14910 | ` */` |
|        4 | 14911 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14912 |  |
|        - | 14913 | `	extract_aux_data sAux;` |
|        - | 14914 | `	ph7_hashmap *pMap;` |
|        5 | 14915 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 14916 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 14917 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 14918 | `		return PH7_OK;` |
|        - | 14919 | `	}` |
|        - | 14920 | `	/* Point to the target hashmap */` |
|        5 | 14921 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 14922 | `	if( pMap->nEntry < 1 ){` |
|        - | 14923 | `		/* Empty map,return  0 */` |
|      ! 0 | 14924 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 14925 | `		return PH7_OK;` |
|        - | 14926 | `	}` |
|        - | 14927 | `	/* Prepare the aux data */` |
|        5 | 14928 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 14929 | `	if( nArg > 1 ){` |
|        3 | 14930 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 14931 | `		if( nArg > 2 ){` |
|      ! 0 | 14932 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 14933 | `		}` |
|        1 | 14934 | `	}` |
|        5 | 14935 | `	sAux.pVm = pCtx->pVm;` |
|        - | 14936 | `	/* Invoke the worker callback */` |
|        5 | 14937 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 14938 | `	/* Number of variables successfully imported */` |
|        5 | 14939 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 14940 | `	return PH7_OK;` |
|        3 | 14941 |  |
|        - | 14942 | `/*` |
|        - | 14943 | ` * Worker callback for the [extract()] function defined` |
|        - | 14944 | ` * below.` |
|        - | 14945 | ` */` |
|        8 | 14946 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 14947 |  |
|        9 | 14948 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 14949 | `	int iFlags = pAux->iFlags;` |
|        9 | 14950 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 14951 | `	ph7_value *pObj;` |
|        - | 14952 | `	SyString sVar;` |
|        9 | 14953 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 14954 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 14955 | `	}` |
|        - | 14956 | `	/* Perform a string cast */` |
|        9 | 14957 | `	PH7_MemObjToString(pKey);` |
|        9 | 14958 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 14959 | `		/* Unavailable variable name */` |
|      ! 0 | 14960 | `		return SXRET_OK;` |
|        - | 14961 | `	}` |
|        9 | 14962 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 14963 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 14964 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 14965 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 14966 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 14967 | `			);` |
|      ! 0 | 14968 | `	}else{` |
|       13 | 14969 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 14970 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 14971 | `	}` |
|        9 | 14972 | `	sVar.zString = pAux->zWorker;` |
|        - | 14973 | `	/* Try to extract the variable */` |
|        9 | 14974 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 14975 | `	if( pObj ){` |
|        - | 14976 | `		/* Collision */` |
|        5 | 14977 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 14978 | `			return SXRET_OK;` |
|        - | 14979 | `		}` |
|        5 | 14980 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 14981 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 14982 | `				/* Already prefixed */` |
|      ! 0 | 14983 | `				return SXRET_OK;` |
|        - | 14984 | `			}` |
|      ! 0 | 14985 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 14986 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 14987 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 14988 | `				);` |
|      ! 0 | 14989 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 14990 | `		}` |
|        3 | 14991 | `	}else{` |
|        - | 14992 | `		/* Create the variable */` |
|        5 | 14993 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 14994 | `	}` |
|        9 | 14995 | `	if( pObj ){` |
|        - | 14996 | `		/* Overwrite the old value */` |
|        9 | 14997 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 14998 | `		/* Increment counter */` |
|        9 | 14999 | `		pAux->iCount++;` |
|        4 | 15000 | `	}` |
|        9 | 15001 | `	return SXRET_OK;` |
|        5 | 15002 |  |
|        - | 15003 | `/*` |
|        - | 15004 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 15005 | ` * defined below.` |
|        - | 15006 | ` */` |
|        2 | 15007 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 15008 |  |
|        3 | 15009 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 15010 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 15011 | `	ph7_value *pObj;` |
|        - | 15012 | `	SyString sVar;` |
|        - | 15013 | `	/* Perform a string cast */` |
|        3 | 15014 | `	PH7_MemObjToString(pKey);` |
|        3 | 15015 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 15016 | `		/* Unavailable variable name */` |
|      ! 0 | 15017 | `		return SXRET_OK;` |
|        - | 15018 | `	}` |
|        3 | 15019 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 15020 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 15021 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 15022 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 15023 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 15024 | `			);` |
|        2 | 15025 | `	}else{` |
|      ! 0 | 15026 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 15027 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 15028 | `	}` |
|        3 | 15029 | `	sVar.zString = pAux->zWorker;` |
|        - | 15030 | `	/* Extract the variable */` |
|        3 | 15031 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 15032 | `	if( pObj ){` |
|        3 | 15033 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 15034 | `	}` |
|        3 | 15035 | `	return SXRET_OK;` |
|        2 | 15036 |  |
|        - | 15037 | `/*` |
|        - | 15038 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 15039 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 15040 | ` * Parameters` |
|        - | 15041 | ` * $types` |
|        - | 15042 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 15043 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 15044 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 15045 | ` *  POST includes the POST uploaded file information.` |
|        - | 15046 | ` *  Note:` |
|        - | 15047 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 15048 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 15049 | ` * $prefix` |
|        - | 15050 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 15051 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 15052 | ` *  variable named $pref_userid.` |
|        - | 15053 | ` * Return` |
|        - | 15054 | ` *  TRUE on success or FALSE on failure.` |
|        - | 15055 | ` */` |
|        2 | 15056 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15057 |  |
|        - | 15058 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 15059 | `	extract_aux_data sAux;` |
|        - | 15060 | `	int nLen,nPrefixLen;` |
|        - | 15061 | `	ph7_value *pSuper;` |
|        - | 15062 | `	ph7_vm *pVm;` |
|        - | 15063 | `	/* By default import only $_GET variables  */` |
|        3 | 15064 | `	zImport = "G";` |
|        3 | 15065 | `	nLen = (int)sizeof(char);` |
|        3 | 15066 | `	zPrefix = 0;` |
|        3 | 15067 | `	nPrefixLen = 0;` |
|        3 | 15068 | `	if( nArg > 0 ){` |
|        3 | 15069 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 15070 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 15071 | `		}` |
|        3 | 15072 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 15073 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 15074 | `		}` |
|        1 | 15075 | `	}` |
|        - | 15076 | `	/* Point to the underlying VM */` |
|        3 | 15077 | `	pVm = pCtx->pVm;` |
|        - | 15078 | `	/* Initialize the aux data */` |
|        3 | 15079 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 15080 | `	sAux.zPrefix = zPrefix;` |
|        3 | 15081 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 15082 | `	sAux.pVm = pVm;` |
|        - | 15083 | `	/* Extract */` |
|        3 | 15084 | `	zEnd = &zImport[nLen];` |
|        5 | 15085 | `	while( zImport < zEnd ){` |
|        3 | 15086 | `		int c = zImport[0];` |
|        3 | 15087 | `		pSuper = 0;` |
|        3 | 15088 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 15089 | `			/* Import $_GET variables */` |
|        3 | 15090 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 15091 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 15092 | `			/* Import $_POST variables */` |
|      ! 0 | 15093 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 15094 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 15095 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 15096 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 15097 | `		}` |
|        3 | 15098 | `		if( pSuper ){` |
|        - | 15099 | `			/* Iterate throw array entries */` |
|        3 | 15100 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 15101 | `		}` |
|        - | 15102 | `		/* Advance the cursor */` |
|        3 | 15103 | `		zImport++;` |
|        1 | 15104 | `	}` |
|        - | 15105 | `	/* All done,return TRUE*/` |
|        3 | 15106 | `	ph7_result_bool(pCtx,0);` |
|        3 | 15107 | `	return PH7_OK;` |
|        1 | 15108 |  |
|        - | 15109 | `/*` |
|        - | 15110 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 15111 | ` * Refer to the eval() language construct implementation for more` |
|        - | 15112 | ` * information.` |
|        - | 15113 | ` */` |
|    12818 | 15114 | `static sxi32 VmEvalChunk(` |
|        - | 15115 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 15116 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 15117 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 15118 | `	int iFlags,         /* Compile flag */` |
|        - | 15119 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 15120 | `	)` |
|        2 | 15121 |  |
|        - | 15122 | `	SySet *pByteCode,aByteCode;` |
|        - | 15123 | `	SyBlob sSavedNs;` |
|    12820 | 15124 | `	ProcConsumer xErr = 0;` |
|    12820 | 15125 | `	void *pErrData = 0;` |
|        - | 15126 | `	/* Initialize bytecode container */` |
|    12820 | 15127 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    12820 | 15128 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 15129 | `	/* Reset the code generator */` |
|    12820 | 15130 | `	if( bTrueReturn ){` |
|        - | 15131 | `		/* Included file,log compile-time errors */` |
|     9616 | 15132 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     9616 | 15133 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4807 | 15134 | `	}` |
|    12820 | 15135 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 15136 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 15137 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 15138 | `	 * the caller's namespace is restored. */` |
|    12820 | 15139 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    12820 | 15140 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    12820 | 15141 | `	if( bTrueReturn ){` |
|        - | 15142 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     9616 | 15143 | `		SyBlobReset(&pVm->sNamespace);` |
|     4807 | 15144 | `	}` |
|        - | 15145 | `	/* Swap bytecode container */` |
|    12820 | 15146 | `	pByteCode = pVm->pByteContainer;` |
|    12820 | 15147 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 15148 | `	/* Compile the chunk */` |
|    12820 | 15149 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    19228 | 15150 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 15151 | `		/* Compilation error,return false */` |
|        3 | 15152 | `		if( pCtx ){` |
|        3 | 15153 | `			ph7_result_bool(pCtx,0);` |
|        1 | 15154 | `		}` |
|        2 | 15155 | `	}else{` |
|        - | 15156 | `		/* Mount any newly defined classes */` |
|        - | 15157 | `		SyHashEntry *pEntry;` |
|        - | 15158 | `		ph7_class *pClass;` |
|        - | 15159 | `		ph7_value sResult; /* Return value */` |
|        - | 15160 | `		sxi32 rc;` |
|    12818 | 15161 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   977140 | 15162 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   957916 | 15163 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 15164 | `			/* Only mount classes that haven't been mounted yet */` |
|   957916 | 15165 | `			if( !pClass->bMounted ){` |
|   247364 | 15166 | `				rc = VmMountUserClass(pVm,pClass);` |
|   247364 | 15167 | `				if( rc != SXRET_OK ){` |
|        - | 15168 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 15169 | `					if( pCtx ){` |
|      ! 0 | 15170 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 15171 | `					}` |
|      ! 0 | 15172 | `					goto Cleanup;` |
|        - | 15173 | `				}` |
|   123681 | 15174 | `			}` |
|        2 | 15175 | `		}` |
|    12818 | 15176 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 15177 | `			/* Out of memory */` |
|      ! 0 | 15178 | `			if( pCtx ){` |
|      ! 0 | 15179 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 15180 | `			}` |
|      ! 0 | 15181 | `			goto Cleanup;` |
|        - | 15182 | `		}` |
|    12818 | 15183 | `		if( bTrueReturn ){` |
|        - | 15184 | `			/* Assume a boolean true return value */` |
|     9616 | 15185 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4809 | 15186 | `		}else{` |
|        - | 15187 | `			/* Assume a null return value */` |
|     3204 | 15188 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 15189 | `		}` |
|        - | 15190 | `		/* Execute the compiled chunk. eval()/include/require recurse in C here,` |
|        - | 15191 | `		 * a path the OP_CALL cap check can't see; bound it under the same limit` |
|        - | 15192 | `		 * so a recursive include/eval can't overflow the native stack. */` |
|    12818 | 15193 | `		if( VmRecursionExceeded(pVm) ){` |
|        3 | 15194 | `			PH7_MemObjRelease(&sResult);` |
|        3 | 15195 | `			VmRecursionFatal(pVm);` |
|        3 | 15196 | `			goto Cleanup;` |
|        - | 15197 | `		}` |
|    12816 | 15198 | `		pVm->nRecursionDepth++;` |
|    12816 | 15199 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    12816 | 15200 | `		pVm->nRecursionDepth--;` |
|    12816 | 15201 | `		if( pCtx ){` |
|        - | 15202 | `			/* Set the execution result */` |
|     9668 | 15203 | `			ph7_result_value(pCtx,&sResult);` |
|     4833 | 15204 | `		}` |
|    12816 | 15205 | `		PH7_MemObjRelease(&sResult);` |
|        - | 15206 | `	}` |
|     6409 | 15207 | `Cleanup:` |
|        - | 15208 | `	/* Cleanup the mess left behind */` |
|    12820 | 15209 | `	pVm->pByteContainer = pByteCode;` |
|    12820 | 15210 | `	SySetRelease(&aByteCode);` |
|        - | 15211 | `	/* Restore caller's namespace state */` |
|    12820 | 15212 | `	SyBlobReset(&pVm->sNamespace);` |
|    12820 | 15213 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    12820 | 15214 | `	SyBlobRelease(&sSavedNs);` |
|    12820 | 15215 | `	return SXRET_OK;` |
|        2 | 15216 |  |
|        - | 15217 | `/*` |
|        - | 15218 | ` * value eval(string $code)` |
|        - | 15219 | ` *   Evaluate a string as PHP code.` |
|        - | 15220 | ` * Parameter` |
|        - | 15221 | ` *  code: PHP code to evaluate.` |
|        - | 15222 | ` * Return` |
|        - | 15223 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 15224 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 15225 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 15226 | ` */` |
|       58 | 15227 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15228 |  |
|        - | 15229 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       60 | 15230 | `	if( nArg < 1 ){` |
|        - | 15231 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15232 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15233 | `		return SXRET_OK;` |
|        - | 15234 | `	}` |
|        - | 15235 | `	/* Chunk to evaluate */` |
|       60 | 15236 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       60 | 15237 | `	if( sChunk.nByte < 1 ){` |
|        - | 15238 | `		/* Empty string,return NULL */` |
|        3 | 15239 | `		ph7_result_null(pCtx);` |
|        3 | 15240 | `		return SXRET_OK;` |
|        - | 15241 | `	}` |
|        - | 15242 | `	/* Eval the chunk */` |
|       58 | 15243 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       58 | 15244 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15245 | `		/* exit/die inside the evaluated chunk: cascade the halt */` |
|       37 | 15246 | `		return PH7_ABORT;` |
|        - | 15247 | `	}` |
|       22 | 15248 | `	return SXRET_OK;` |
|       31 | 15249 |  |
|        - | 15250 | `/*` |
|        - | 15251 | ` * Check if a file path is already included.` |
|        - | 15252 | ` */` |
|    19226 | 15253 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        2 | 15254 |  |
|        - | 15255 | `	SyString *aEntries;` |
|        - | 15256 | `	sxu32 n;` |
|    19228 | 15257 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 15258 | `	/* Perform a linear search */` |
| 92218428 | 15259 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 92199212 | 15260 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 15261 | `			/* Already included */` |
|       11 | 15262 | `			return TRUE;` |
|        - | 15263 | `		}` |
| 46099602 | 15264 | `	}` |
|    19218 | 15265 | `	return FALSE;` |
|     9615 | 15266 |  |
|        - | 15267 | `/*` |
|        - | 15268 | ` * Push a file path in the appropriate VM container.` |
|        - | 15269 | ` */` |
|    22366 | 15270 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 15271 |  |
|        - | 15272 | `	SyString sPath;` |
|        - | 15273 | `	char *zDup;` |
|        - | 15274 | `#ifdef __WINNT__` |
|        - | 15275 | `	char *zCur;` |
|        - | 15276 | `#endif` |
|        - | 15277 | `	sxi32 rc;` |
|    22368 | 15278 | `	if( nLen < 0 ){` |
|     3142 | 15279 | `		nLen = SyStrlen(zPath);` |
|     1570 | 15280 | `	}` |
|        - | 15281 | `	/* Duplicate the file path first */` |
|    22368 | 15282 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    22368 | 15283 | `	if( zDup == 0 ){` |
|      ! 0 | 15284 | `		return SXERR_MEM;` |
|        - | 15285 | `	}` |
|        - | 15286 | `#ifdef __WINNT__` |
|        - | 15287 | `	/* Normalize path on windows` |
|        - | 15288 | `	 * Example:` |
|        - | 15289 | `	 *    Path/To/File.php` |
|        - | 15290 | `	 * becomes` |
|        - | 15291 | `	 *   path\to\file.php` |
|        - | 15292 | `	 */` |
|        2 | 15293 | `	zCur = zDup;` |
|        2 | 15294 | `	while( zCur[0] != 0 ){` |
|        2 | 15295 | `		if( zCur[0] == '/' ){` |
|        2 | 15296 | `			zCur[0] = '\\';` |
|        2 | 15297 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 15298 | `			int c = SyToLower(zCur[0]);` |
|        1 | 15299 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 15300 | `		}` |
|        2 | 15301 | `		zCur++;` |
|        2 | 15302 | `	}` |
|        - | 15303 | `#endif` |
|        - | 15304 | `	/* Install the file path */` |
|    22368 | 15305 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    22368 | 15306 | `	if( !bMain ){` |
|    19228 | 15307 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 15308 | `			/* Already included */` |
|       11 | 15309 | `			*pNew = 0;` |
|        6 | 15310 | `		}else{` |
|        - | 15311 | `			/* Insert in the corresponding container */` |
|    19218 | 15312 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    19218 | 15313 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 15314 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 15315 | `				return rc;` |
|        - | 15316 | `			}` |
|    19218 | 15317 | `			*pNew = 1;` |
|        - | 15318 | `		}` |
|     9613 | 15319 | `	}` |
|    22368 | 15320 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    22368 | 15321 | `	return SXRET_OK;` |
|    11185 | 15322 |  |
|        - | 15323 | `/*` |
|        - | 15324 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 15325 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 15326 | ` * indicates failure.` |
|        - | 15327 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 15328 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 15329 | ` * operations.` |
|        - | 15330 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 15331 | ` * this function is a no-op.` |
|        - | 15332 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 15333 | ` * constructs for more information.` |
|        - | 15334 | ` */` |
|     9628 | 15335 | `static sxi32 VmExecIncludedFile(` |
|        - | 15336 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 15337 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 15338 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 15339 | `	 )` |
|        2 | 15340 |  |
|        - | 15341 | `	sxi32 rc;` |
|        - | 15342 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 15343 | `	const ph7_io_stream *pStream;` |
|        - | 15344 | `	SyBlob sContents;` |
|        - | 15345 | `	void *pHandle;` |
|        - | 15346 | `	ph7_vm *pVm;` |
|        - | 15347 | `	int isNew;` |
|        - | 15348 | `	/* Initialize fields */` |
|     9630 | 15349 | `	pVm = pCtx->pVm;` |
|     9630 | 15350 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     9630 | 15351 | `	isNew = 0;` |
|        - | 15352 | `	/* Extract the associated stream */` |
|     9630 | 15353 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 15354 | `	/*` |
|        - | 15355 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 15356 | `	 * in a read-only mode.` |
|        - | 15357 | `	 */` |
|     9630 | 15358 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     9630 | 15359 | `	if( pHandle == 0 ){` |
|        8 | 15360 | `		return SXERR_IO;` |
|        - | 15361 | `	}` |
|     9624 | 15362 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     9624 | 15363 | `	if( IncludeOnce && !isNew ){` |
|        - | 15364 | `		/* Already included */` |
|        9 | 15365 | `		rc = SXERR_EXISTS;` |
|        5 | 15366 | `	}else{` |
|        - | 15367 | `		/* Read the whole file contents */` |
|     9616 | 15368 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     9616 | 15369 | `		if( rc == SXRET_OK ){` |
|        - | 15370 | `			SyString sScript;` |
|        - | 15371 | `			/* Compile and execute the script */` |
|     9616 | 15372 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     9616 | 15373 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4807 | 15374 | `		}` |
|        - | 15375 | `	}` |
|        - | 15376 | `	/* Pop from the set of included file */` |
|     9624 | 15377 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 15378 | `	/* Close the handle */` |
|     9624 | 15379 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 15380 | `	/* Release the working buffer */` |
|     9624 | 15381 | `	SyBlobRelease(&sContents);` |
|        - | 15382 | `#else` |
|        - | 15383 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 15384 | `	SXUNUSED(pPath);` |
|        - | 15385 | `	SXUNUSED(IncludeOnce);` |
|        - | 15386 | `	rc = SXERR_IO;` |
|        - | 15387 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     9624 | 15388 | `	return rc;` |
|     4816 | 15389 |  |
|        - | 15390 | `/*` |
|        - | 15391 | ` * string get_include_path(void)` |
|        - | 15392 | ` *  Gets the current include_path configuration option.` |
|        - | 15393 | ` * Parameter` |
|        - | 15394 | ` *  None` |
|        - | 15395 | ` * Return` |
|        - | 15396 | ` *  Included paths as a string` |
|        - | 15397 | ` */` |
|        2 | 15398 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15399 |  |
|        3 | 15400 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 15401 | `	SyString *aEntry;` |
|        - | 15402 | `	int dir_sep;` |
|        - | 15403 | `	sxu32 n;` |
|        - | 15404 | `#ifdef __WINNT__` |
|        1 | 15405 | `	dir_sep = ';';` |
|        - | 15406 | `#else` |
|        - | 15407 | `	/* Assume UNIX path separator */` |
|        2 | 15408 | `	dir_sep = ':';` |
|        - | 15409 | `#endif` |
|        1 | 15410 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 15411 | `	SXUNUSED(apArg);` |
|        - | 15412 | `	/* Point to the list of import paths */` |
|        3 | 15413 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 15414 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 15415 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 15416 | `		if( n > 0 ){` |
|        - | 15417 | `			/* Append dir seprator */` |
|      ! 0 | 15418 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 15419 | `		}` |
|        - | 15420 | `		/* Append path */` |
|        3 | 15421 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 15422 | `	}` |
|        3 | 15423 | `	return PH7_OK;` |
|        1 | 15424 |  |
|        - | 15425 | `/*` |
|        - | 15426 | ` * string get_get_included_files(void)` |
|        - | 15427 | ` *  Gets the current include_path configuration option.` |
|        - | 15428 | ` * Parameter` |
|        - | 15429 | ` *  None` |
|        - | 15430 | ` * Return` |
|        - | 15431 | ` *  Included paths as a string` |
|        - | 15432 | ` */` |
|        2 | 15433 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15434 |  |
|        3 | 15435 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 15436 | `	ph7_value *pArray,*pWorker;` |
|        - | 15437 | `	SyString *pEntry;` |
|        - | 15438 | `	int c,d;` |
|        - | 15439 | `	/* Create an array and a working value */` |
|        3 | 15440 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 15441 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 15442 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 15443 | `		/* Out of memory,return null */` |
|      ! 0 | 15444 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15445 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 15446 | `		SXUNUSED(apArg);` |
|      ! 0 | 15447 | `		return PH7_OK;` |
|        - | 15448 | `	}` |
|        3 | 15449 | `	c = d = '/';` |
|        - | 15450 | `#ifdef __WINNT__` |
|        1 | 15451 | `	d = '\\';` |
|        - | 15452 | `#endif` |
|        - | 15453 | `	/* Iterate throw entries */` |
|        3 | 15454 | `	SySetResetCursor(pFiles);` |
|     3917 | 15455 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 15456 | `		const char *zBase,*zEnd;` |
|        - | 15457 | `		int iLen;` |
|        - | 15458 | `		/* reset the string cursor */` |
|     3915 | 15459 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 15460 | `		/* Extract base name */` |
|     3915 | 15461 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 15462 | `		/* Ignore trailing '/' */` |
|     5872 | 15463 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 15464 | `			zEnd--;` |
|      ! 0 | 15465 | `		}` |
|     3915 | 15466 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   120668 | 15467 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   114797 | 15468 | `			zEnd--;` |
|        1 | 15469 | `		}` |
|     3915 | 15470 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3915 | 15471 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 15472 | `		/* Copy entry name */` |
|     3915 | 15473 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 15474 | `		/* Perform the insertion */` |
|     3915 | 15475 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 15476 | `	}` |
|        - | 15477 | `	/* All done,return the created array */` |
|        3 | 15478 | `	ph7_result_value(pCtx,pArray);` |
|        - | 15479 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 15480 | `	 * by the engine as soon we return from this foreign` |
|        - | 15481 | `	 * function.` |
|        - | 15482 | `	 */` |
|        3 | 15483 | `	return PH7_OK;` |
|        2 | 15484 |  |
|        - | 15485 | `/*` |
|        - | 15486 | ` * include:` |
|        - | 15487 | ` * According to the PHP reference manual.` |
|        - | 15488 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 15489 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 15490 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 15491 | ` *  include() will finally check in the calling script's own directory` |
|        - | 15492 | ` *  and the current working directory before failing. The include()` |
|        - | 15493 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 15494 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 15495 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 15496 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 15497 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 15498 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 15499 | ` *  directory to find the requested file.` |
|        - | 15500 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 15501 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 15502 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 15503 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 15504 | ` */` |
|     9604 | 15505 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15506 |  |
|        - | 15507 | `	SyString sFile;` |
|        - | 15508 | `	sxi32 rc;` |
|     9606 | 15509 | `	if( nArg < 1 ){` |
|        - | 15510 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15511 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15512 | `		return SXRET_OK;` |
|        - | 15513 | `	}` |
|        - | 15514 | `	/* File to include */` |
|     9606 | 15515 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     9606 | 15516 | `	if( sFile.nByte < 1 ){` |
|        - | 15517 | `		/* Empty string,return NULL */` |
|      ! 0 | 15518 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15519 | `		return SXRET_OK;` |
|        - | 15520 | `	}` |
|        - | 15521 | `	/* Open,compile and execute the desired script */` |
|     9606 | 15522 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     9606 | 15523 | `	if( rc != SXRET_OK ){` |
|        - | 15524 | `		/* Emit a warning and return false */` |
|        3 | 15525 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 15526 | `		ph7_result_bool(pCtx,0);` |
|        1 | 15527 | `	}` |
|     9606 | 15528 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15529 | `		/* exit/die inside the included file: cascade the halt */` |
|        5 | 15530 | `		return PH7_ABORT;` |
|        - | 15531 | `	}` |
|     9602 | 15532 | `	return SXRET_OK;` |
|     4804 | 15533 |  |
|        - | 15534 | `/*` |
|        - | 15535 | ` * include_once:` |
|        - | 15536 | ` *  According to the PHP reference manual.` |
|        - | 15537 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 15538 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 15539 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 15540 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 15541 | ` *   just once.` |
|        - | 15542 | ` */` |
|       10 | 15543 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15544 |  |
|        - | 15545 | `	SyString sFile;` |
|        - | 15546 | `	sxi32 rc;` |
|       11 | 15547 | `	if( nArg < 1 ){` |
|        - | 15548 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15549 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15550 | `		return SXRET_OK;` |
|        - | 15551 | `	}` |
|        - | 15552 | `	/* File to include */` |
|       11 | 15553 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|       11 | 15554 | `	if( sFile.nByte < 1 ){` |
|        - | 15555 | `		/* Empty string,return NULL */` |
|      ! 0 | 15556 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15557 | `		return SXRET_OK;` |
|        - | 15558 | `	}` |
|        - | 15559 | `	/* Open,compile and execute the desired script */` |
|       11 | 15560 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|       11 | 15561 | `	if( rc == SXERR_EXISTS ){` |
|        - | 15562 | `		/* File already included,return TRUE */` |
|        7 | 15563 | `		ph7_result_bool(pCtx,1);` |
|        7 | 15564 | `		return SXRET_OK;` |
|        - | 15565 | `	}` |
|        5 | 15566 | `	if( rc != SXRET_OK ){` |
|        - | 15567 | `		/* Emit a warning and return false */` |
|      ! 0 | 15568 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 15569 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15570 | ` 	}` |
|        5 | 15571 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15572 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 15573 | `		return PH7_ABORT;` |
|        - | 15574 | `	}` |
|        5 | 15575 | `	return SXRET_OK;` |
|        6 | 15576 |  |
|        - | 15577 | `/*` |
|        - | 15578 | ` * require.` |
|        - | 15579 | ` *  According to the PHP reference manual.` |
|        - | 15580 | ` *   require() is identical to include() except upon failure it will` |
|        - | 15581 | ` *   also produce a fatal level error.` |
|        - | 15582 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 15583 | ` *   emits a warning  which allows the script to continue.` |
|        - | 15584 | ` */` |
|        6 | 15585 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15586 |  |
|        - | 15587 | `	SyString sFile;` |
|        - | 15588 | `	sxi32 rc;` |
|        8 | 15589 | `	if( nArg < 1 ){` |
|        - | 15590 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15591 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15592 | `		return SXRET_OK;` |
|        - | 15593 | `	}` |
|        - | 15594 | `	/* File to include */` |
|        8 | 15595 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 | 15596 | `	if( sFile.nByte < 1 ){` |
|        - | 15597 | `		/* Empty string,return NULL */` |
|      ! 0 | 15598 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15599 | `		return SXRET_OK;` |
|        - | 15600 | `	}` |
|        - | 15601 | `	/* Open,compile and execute the desired script */` |
|        8 | 15602 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        8 | 15603 | `	if( rc != SXRET_OK ){` |
|        - | 15604 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 15605 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 15606 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15607 | `		return PH7_ABORT;` |
|        - | 15608 | `	}` |
|        8 | 15609 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15610 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 15611 | `		return PH7_ABORT;` |
|        - | 15612 | `	}` |
|        8 | 15613 | `	return SXRET_OK;` |
|        5 | 15614 |  |
|        - | 15615 | `/*` |
|        - | 15616 | ` * require_once:` |
|        - | 15617 | ` *  According to the PHP reference manual.` |
|        - | 15618 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 15619 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 15620 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 15621 | ` *   and how it differs from its non _once siblings.` |
|        - | 15622 | ` */` |
|        4 | 15623 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15624 |  |
|        - | 15625 | `	SyString sFile;` |
|        - | 15626 | `	sxi32 rc;` |
|        5 | 15627 | `	if( nArg < 1 ){` |
|        - | 15628 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15629 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15630 | `		return SXRET_OK;` |
|        - | 15631 | `	}` |
|        - | 15632 | `	/* File to include */` |
|        5 | 15633 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 15634 | `	if( sFile.nByte < 1 ){` |
|        - | 15635 | `		/* Empty string,return NULL */` |
|      ! 0 | 15636 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15637 | `		return SXRET_OK;` |
|        - | 15638 | `	}` |
|        - | 15639 | `	/* Open,compile and execute the desired script */` |
|        5 | 15640 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 15641 | `	if( rc == SXERR_EXISTS ){` |
|        - | 15642 | `		/* File already included,return TRUE */` |
|        3 | 15643 | `		ph7_result_bool(pCtx,1);` |
|        3 | 15644 | `		return SXRET_OK;` |
|        - | 15645 | `	}` |
|        3 | 15646 | `	if( rc != SXRET_OK ){` |
|        - | 15647 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 15648 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 15649 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15650 | `		return PH7_ABORT;` |
|        - | 15651 | `	}` |
|        3 | 15652 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15653 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 15654 | `		return PH7_ABORT;` |
|        - | 15655 | `	}` |
|        3 | 15656 | `	return SXRET_OK;` |
|        3 | 15657 |  |
|        - | 15658 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 15659 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 15660 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 15661 | `/*` |
|        - | 15662 | ` * Section:` |
|        - | 15663 | ` *  SPL Autoloading functions.` |
|        - | 15664 | ` * Status:` |
|        - | 15665 | ` *  Stable.` |
|        - | 15666 | ` */` |
|        - | 15667 | `/*` |
|        - | 15668 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 15669 | ` *  Register given function as __autoload() implementation.` |
|        - | 15670 | ` * Parameters` |
|        - | 15671 | ` *  callback` |
|        - | 15672 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 15673 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 15674 | ` *  throw` |
|        - | 15675 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 15676 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - | 15677 | ` *  prepend` |
|        - | 15678 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 15679 | ` *   autoload stack instead of appending it.` |
|        - | 15680 | ` * Return` |
|        - | 15681 | ` *  TRUE on success, FALSE on failure.` |
|        - | 15682 | ` */` |
|       34 | 15683 | `static int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15684 |  |
|        - | 15685 | `	VmAutoloadCB sEntry;` |
|       36 | 15686 | `	ph7_vm *pVm = pCtx->pVm;` |
|       36 | 15687 | `	int iPrepend = 0;` |
|        - | 15688 | `	sxu32 n;` |
|       36 | 15689 | `	if( nArg < 1 ){` |
|        - | 15690 | `		/* No callback provided — register default spl_autoload.` |
|        - | 15691 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - | 15692 | `		/* Check for duplicates first */` |
|        9 | 15693 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 | 15694 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        6 | 15695 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 | 15696 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 | 15697 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 | 15698 | `				ph7_result_bool(pCtx,1);` |
|        5 | 15699 | `				return SXRET_OK;` |
|        - | 15700 | `			}` |
|      ! 0 | 15701 | `		}` |
|        5 | 15702 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 | 15703 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 | 15704 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 | 15705 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 | 15706 | `		ph7_result_bool(pCtx,1);` |
|        5 | 15707 | `		return SXRET_OK;` |
|        - | 15708 | `	}` |
|        - | 15709 | `	/* Validate that the callback is callable */` |
|       28 | 15710 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 | 15711 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 | 15712 | `		if( nArg >= 2 ){` |
|      ! 0 | 15713 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 15714 | `		}` |
|      ! 0 | 15715 | `		if( iThrow ){` |
|      ! 0 | 15716 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 15717 | `				"Argument is not callable");` |
|      ! 0 | 15718 | `		}` |
|      ! 0 | 15719 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15720 | `		return SXRET_OK;` |
|        - | 15721 | `	}` |
|        - | 15722 | `	/* Check for duplicates */` |
|       46 | 15723 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 | 15724 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 | 15725 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 15726 | `			/* Already registered */` |
|      ! 0 | 15727 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 15728 | `			return SXRET_OK;` |
|        - | 15729 | `		}` |
|       11 | 15730 | `	}` |
|        - | 15731 | `	/* Check prepend flag */` |
|       28 | 15732 | `	if( nArg >= 3 ){` |
|        3 | 15733 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 | 15734 | `	}` |
|        - | 15735 | `	/* Store the callback */` |
|       28 | 15736 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       28 | 15737 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       28 | 15738 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       29 | 15739 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 15740 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 15741 | `		 * We do this by appending first, then rotating the array. */` |
|        3 | 15742 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 15743 | `		VmAutoloadCB *aBase;` |
|        3 | 15744 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 15745 | `		/* Rotate: move last entry to front */` |
|        3 | 15746 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 | 15747 | `		if( aBase ){` |
|        - | 15748 | `			VmAutoloadCB sTemp;` |
|        - | 15749 | `			sxu32 i;` |
|        3 | 15750 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 | 15751 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 | 15752 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 | 15753 | `			}` |
|        3 | 15754 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 | 15755 | `		}` |
|        2 | 15756 | `	}else{` |
|       26 | 15757 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 15758 | `	}` |
|       28 | 15759 | `	ph7_result_bool(pCtx,1);` |
|       28 | 15760 | `	return SXRET_OK;` |
|       19 | 15761 |  |
|        - | 15762 | `/*` |
|        - | 15763 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 15764 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 15765 | ` * Parameters` |
|        - | 15766 | ` *  callback` |
|        - | 15767 | ` *   The autoload function being unregistered.` |
|        - | 15768 | ` * Return` |
|        - | 15769 | ` *  TRUE on success, FALSE on failure.` |
|        - | 15770 | ` */` |
|       32 | 15771 | `static int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15772 |  |
|       34 | 15773 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 15774 | `	sxu32 n,nEntry;` |
|       34 | 15775 | `	if( nArg < 1 ){` |
|      ! 0 | 15776 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15777 | `		return SXRET_OK;` |
|        - | 15778 | `	}` |
|       34 | 15779 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 15780 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       36 | 15781 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       36 | 15782 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 15783 | `			/* Found — remove by shifting remaining entries down */` |
|       32 | 15784 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 15785 | `			sxu32 i;` |
|       32 | 15786 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       46 | 15787 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 | 15788 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 | 15789 | `			}` |
|        - | 15790 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       32 | 15791 | `			SySetPop(&pVm->aAutoload);` |
|       32 | 15792 | `			ph7_result_bool(pCtx,1);` |
|       32 | 15793 | `			return SXRET_OK;` |
|        - | 15794 | `		}` |
|        3 | 15795 | `	}` |
|        3 | 15796 | `	ph7_result_bool(pCtx,0);` |
|        3 | 15797 | `	return SXRET_OK;` |
|       18 | 15798 |  |
|        - | 15799 | `/*` |
|        - | 15800 | ` * array spl_autoload_functions(void)` |
|        - | 15801 | ` *  Return all registered __autoload() functions.` |
|        - | 15802 | ` * Return` |
|        - | 15803 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 15804 | ` *  an empty array is returned.` |
|        - | 15805 | ` */` |
|       20 | 15806 | `static int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15807 |  |
|       21 | 15808 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 15809 | `	ph7_value *pArray;` |
|        - | 15810 | `	sxu32 n,nEntry;` |
|       10 | 15811 | `	SXUNUSED(nArg);` |
|       10 | 15812 | `	SXUNUSED(apArg);` |
|       21 | 15813 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 15814 | `	if( pArray == 0 ){` |
|      ! 0 | 15815 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15816 | `		return SXRET_OK;` |
|        - | 15817 | `	}` |
|       21 | 15818 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 15819 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 15820 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 15821 | `		if( pEntry ){` |
|       15 | 15822 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 15823 | `		}` |
|        8 | 15824 | `	}` |
|       21 | 15825 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 15826 | `	return SXRET_OK;` |
|       11 | 15827 |  |
|        - | 15828 | `/*` |
|        - | 15829 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - | 15830 | ` *  Default implementation of __autoload().` |
|        - | 15831 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 15832 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 15833 | ` * Parameters` |
|        - | 15834 | ` *  class` |
|        - | 15835 | ` *   The class name being searched.` |
|        - | 15836 | ` *  file_extensions` |
|        - | 15837 | ` *   Comma-separated list of file extensions to try.` |
|        - | 15838 | ` */` |
|        2 | 15839 | `static int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15840 |  |
|        - | 15841 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 15842 | `	SyBlob sPath;` |
|        - | 15843 | `	int nClass;` |
|        - | 15844 | `	sxi32 rc;` |
|        3 | 15845 | `	if( nArg < 1 ){` |
|      ! 0 | 15846 | `		return SXRET_OK;` |
|        - | 15847 | `	}` |
|        3 | 15848 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 15849 | `	if( nClass < 1 ){` |
|      ! 0 | 15850 | `		return SXRET_OK;` |
|        - | 15851 | `	}` |
|        - | 15852 | `	/* Default extensions */` |
|        3 | 15853 | `	zExt = ".php,.inc";` |
|        3 | 15854 | `	if( nArg >= 2 ){` |
|        - | 15855 | `		int nExt;` |
|      ! 0 | 15856 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 15857 | `		if( nExt < 1 ){` |
|      ! 0 | 15858 | `			zExt = ".php,.inc";` |
|      ! 0 | 15859 | `		}` |
|      ! 0 | 15860 | `	}` |
|        3 | 15861 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 15862 | `	/* Iterate over comma-separated extensions */` |
|        3 | 15863 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 | 15864 | `	zCur = zExt;` |
|        7 | 15865 | `	while( zCur < zEnd ){` |
|        - | 15866 | `		const char *zComma;` |
|        - | 15867 | `		SyString sFile;` |
|        - | 15868 | `		int i;` |
|        - | 15869 | `		/* Find next comma or end */` |
|        5 | 15870 | `		zComma = zCur;` |
|       21 | 15871 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 15872 | `			zComma++;` |
|        1 | 15873 | `		}` |
|        - | 15874 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 15875 | `		SyBlobReset(&sPath);` |
|       69 | 15876 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 15877 | `			char c = zClass[i];` |
|       65 | 15878 | `			if( c == '\\' ){` |
|      ! 0 | 15879 | `				c = '/';` |
|       65 | 15880 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 15881 | `				c = c + ('a' - 'A');` |
|        6 | 15882 | `			}` |
|       65 | 15883 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 15884 | `		}` |
|        - | 15885 | `		/* Append extension */` |
|        5 | 15886 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 15887 | `		/* Try to include the file */` |
|        5 | 15888 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 15889 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 15890 | `		if( rc == SXRET_OK ){` |
|        - | 15891 | `			/* File included successfully */` |
|      ! 0 | 15892 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 15893 | `			return SXRET_OK;` |
|        - | 15894 | `		}` |
|        - | 15895 | `		/* Move past the comma */` |
|        5 | 15896 | `		zCur = zComma;` |
|        5 | 15897 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 15898 | `			zCur++;` |
|        1 | 15899 | `		}` |
|        1 | 15900 | `	}` |
|        3 | 15901 | `	SyBlobRelease(&sPath);` |
|        3 | 15902 | `	return SXRET_OK;` |
|        2 | 15903 |  |
|        - | 15904 | `/* Table of built-in VM functions. */` |
|        - | 15905 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 15906 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 15907 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 15908 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 15909 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 15910 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 15911 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 15912 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 15913 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 15914 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 15915 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 15916 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 15917 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 15918 | `	    /* Constants management */` |
|        - | 15919 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 15920 | `	{ "define",   vm_builtin_define               },` |
|        - | 15921 | `	{ "constant", vm_builtin_constant             },` |
|        - | 15922 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 15923 | `	   /* Class/Object functions */` |
|        - | 15924 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 15925 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 15926 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 15927 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 15928 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 15929 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 15930 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 15931 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 15932 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 15933 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 15934 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 15935 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 15936 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 15937 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 15938 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 15939 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 15940 | `	   /* SPL Autoloading */` |
|        - | 15941 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 15942 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 15943 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 15944 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 15945 | `	   /* Random numbers/strings generators */` |
|        - | 15946 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 15947 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 15948 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 15949 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 15950 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 15951 | `	{ "random_int",    vm_builtin_random_int      },` |
|        - | 15952 | `	{ "random_bytes",  vm_builtin_random_bytes    },` |
|        - | 15953 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 15954 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 15955 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 15956 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 15957 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 15958 | `	   /* Language constructs functions */` |
|        - | 15959 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 15960 | `	{ "print", vm_builtin_print                   },` |
|        - | 15961 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 15962 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 15963 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 15964 | `	  /* Variable handling functions */` |
|        - | 15965 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 15966 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 15967 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 15968 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 15969 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 15970 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 15971 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 15972 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 15973 | `	  /* Ouput control functions */` |
|        - | 15974 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 15975 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 15976 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 15977 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 15978 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 15979 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 15980 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 15981 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 15982 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 15983 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 15984 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 15985 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 15986 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 15987 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 15988 | `	  /* Assertion functions */` |
|        - | 15989 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 15990 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 15991 | `	  /* Error reporting functions */` |
|        - | 15992 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 15993 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 15994 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 15995 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 15996 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 15997 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 15998 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 15999 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 16000 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 16001 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 16002 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 16003 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 16004 | `	  /* Release info */` |
|        - | 16005 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 16006 | `	{"phpversion",       vm_builtin_phpversion    },` |
|        - | 16007 | `	{"php_sapi_name",    vm_builtin_php_sapi_name },` |
|        - | 16008 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 16009 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 16010 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 16011 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 16012 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 16013 | `	  /* hashmap */` |
|        - | 16014 | `	{"compact",          vm_builtin_compact       },` |
|        - | 16015 | `	{"extract",          vm_builtin_extract       },` |
|        - | 16016 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 16017 | `	  /* URL related function */` |
|        - | 16018 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 16019 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 16020 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 16021 | `	   /* XML processing functions */` |
|        - | 16022 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 16023 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 16024 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 16025 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 16026 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 16027 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 16028 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 16029 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 16030 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 16031 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 16032 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 16033 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 16034 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 16035 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 16036 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 16037 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 16038 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 16039 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 16040 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 16041 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 16042 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 16043 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 16044 | `	   /* UTF-8 encoding/decoding */` |
|        - | 16045 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 16046 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 16047 | `	   /* Command line processing */` |
|        - | 16048 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 16049 | `	   /* JSON encoding/decoding */` |
|        - | 16050 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 16051 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 16052 | `	{"json_last_error_msg",vm_builtin_json_last_error_msg},` |
|        - | 16053 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 16054 | `	{"json_validate",  vm_builtin_json_validate },` |
|        - | 16055 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 16056 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 16057 | `	   /* Files/URI inclusion facility */` |
|        - | 16058 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 16059 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 16060 | `	{ "include",      vm_builtin_include          },` |
|        - | 16061 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 16062 | `	{ "require",      vm_builtin_require          },` |
|        - | 16063 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 16064 | `};` |
|        - | 16065 | `/*` |
|        - | 16066 | ` * Register the built-in VM functions defined above.` |
|        - | 16067 | ` */` |
|     2834 | 16068 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 16069 |  |
|        - | 16070 | `	sxi32 rc;` |
|        - | 16071 | `	sxu32 n;` |
|   382592 | 16072 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 16073 | `		/* Note that these special functions have access` |
|        - | 16074 | `		 * to the underlying virtual machine as their` |
|        - | 16075 | `		 * private data.` |
|        - | 16076 | `		 */` |
|   379758 | 16077 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   379758 | 16078 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 16079 | `			return rc;` |
|        - | 16080 | `		}` |
|   189880 | 16081 | `	}` |
|     2836 | 16082 | `	return SXRET_OK;` |
|     1419 | 16083 |  |
|        - | 16084 | `/*` |
|        - | 16085 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 16086 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 16087 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 16088 | ` */` |
|   186022 | 16089 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        2 | 16090 |  |
|   186024 | 16091 | `	if( !iLoadable ){` |
|   183918 | 16092 | `		return pClass;` |
|        - | 16093 | `	}` |
|     2112 | 16094 | `	while(pClass){` |
|     2108 | 16095 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     2104 | 16096 | `			return pClass;` |
|        - | 16097 | `		}` |
|        5 | 16098 | `		pClass = pClass->pNextName;` |
|        1 | 16099 | `	}` |
|        5 | 16100 | `	return 0;` |
|    93013 | 16101 |  |
|        - | 16102 | `/*` |
|        - | 16103 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 16104 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 16105 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 16106 | ` * registered in the VM's class table.` |
|        - | 16107 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 16108 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 16109 | ` */` |
|       38 | 16110 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 16111 |  |
|        - | 16112 | `	VmAutoloadCB *pEntry;` |
|        - | 16113 | `	ph7_value sArg,sResult;` |
|        - | 16114 | `	SyHashEntry *pHashEntry;` |
|        - | 16115 | `	ph7_class *pClass;` |
|        - | 16116 | `	sxu32 n,nEntry;` |
|       40 | 16117 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       40 | 16118 | `	if( nEntry < 1 ){` |
|       26 | 16119 | `		return 0;` |
|        - | 16120 | `	}` |
|        - | 16121 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       16 | 16122 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 16123 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 16124 | `	}` |
|        - | 16125 | `	/* Mark this class as being autoloaded */` |
|       14 | 16126 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 16127 | `	/* Prepare the class name argument */` |
|       14 | 16128 | `	PH7_MemObjInit(pVm,&sArg);` |
|       14 | 16129 | `	PH7_MemObjInit(pVm,&sResult);` |
|       14 | 16130 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       14 | 16131 | `	pClass = 0;` |
|       28 | 16132 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 16133 | `		ph7_value *apArg[1];` |
|       24 | 16134 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       24 | 16135 | `		if( pEntry == 0 ){` |
|      ! 0 | 16136 | `			continue;` |
|        - | 16137 | `		}` |
|       24 | 16138 | `		apArg[0] = &sArg;` |
|       24 | 16139 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 16140 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 16141 | `			continue;` |
|        - | 16142 | `		}` |
|        - | 16143 | `		/* Check if the class is now available */` |
|       24 | 16144 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       24 | 16145 | `		if( pHashEntry ){` |
|       10 | 16146 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       10 | 16147 | `			if( pClass ){` |
|       10 | 16148 | `				break;` |
|        - | 16149 | `			}` |
|      ! 0 | 16150 | `		}` |
|        9 | 16151 | `	}` |
|       14 | 16152 | `	PH7_MemObjRelease(&sArg);` |
|       14 | 16153 | `	PH7_MemObjRelease(&sResult);` |
|        - | 16154 | `	/* Remove reentrancy guard */` |
|       14 | 16155 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       14 | 16156 | `	return pClass;` |
|       21 | 16157 |  |
|        - | 16158 | `/*` |
|        - | 16159 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 16160 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 16161 | ` */` |
|       18 | 16162 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 16163 |  |
|       20 | 16164 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        2 | 16165 |  |
|        - | 16166 | `/*` |
|        - | 16167 | ` * Check if the given name refer to an installed class.` |
|        - | 16168 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 16169 | ` */` |
|   186034 | 16170 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 16171 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 16172 | `	const char *zName,  /* Name of the target class */` |
|        - | 16173 | `	sxu32 nByte,        /* zName length */` |
|        - | 16174 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 16175 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 16176 | `						 */` |
|        - | 16177 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 16178 | `	)` |
|        2 | 16179 |  |
|        - | 16180 | `	SyHashEntry *pEntry;` |
|        - | 16181 | `	ph7_class *pClass;` |
|    93017 | 16182 | `	SXUNUSED(iNest);` |
|        - | 16183 | `	/* Exact class lookup.` |
|        - | 16184 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 16185 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   186036 | 16186 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|   186036 | 16187 | `	if( pEntry == 0 ){` |
|        - | 16188 | `		/* Class not found in hash table — try autoload before giving up */` |
|       22 | 16189 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 16190 | `	}` |
|   186016 | 16191 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|   186016 | 16192 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|    93019 | 16193 |  |
|        - | 16194 | `/*` |
|        - | 16195 | ` * Reference Table Implementation` |
|        - | 16196 | ` * Status: stable <chm@symisc.net>` |
|        - | 16197 | ` * Intro` |
|        - | 16198 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 16199 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 16200 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 16201 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 16202 | ` *  Refer to the official for more information on this powerful` |
|        - | 16203 | ` *  extension.` |
|        - | 16204 | ` */` |
|        - | 16205 | `/*` |
|        - | 16206 | ` * Allocate a new reference entry.` |
|        - | 16207 | ` */` |
|  3205822 | 16208 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 16209 |  |
|        - | 16210 | `	VmRefObj *pRef;` |
|        - | 16211 | `	/* Allocate a new instance */` |
|  3205824 | 16212 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3205824 | 16213 | `	if( pRef == 0 ){` |
|      ! 0 | 16214 | `		return 0;` |
|        - | 16215 | `	}` |
|        - | 16216 | `	/* Zero the structure */` |
|  3205824 | 16217 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 16218 | `	/* Initialize fields */` |
|  3205824 | 16219 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3205824 | 16220 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3205824 | 16221 | `	pRef->nIdx = nIdx;` |
|  3205824 | 16222 | `	return pRef;` |
|  1602913 | 16223 |  |
|        - | 16224 | `/*` |
|        - | 16225 | ` * Default hash function used by the reference table` |
|        - | 16226 | ` * for lookup/insertion operations.` |
|        - | 16227 | ` */` |
| 17555571 | 16228 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 16229 |  |
|        - | 16230 | `	/* Calculate the hash based on the memory object index */` |
| 17555573 | 16231 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 16232 |  |
|        - | 16233 | `/*` |
|        - | 16234 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 16235 | ` * in the reference table.` |
|        - | 16236 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 16237 | ` * otherwise.` |
|        - | 16238 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16239 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16240 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16241 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16242 | ` * Refer to the official for more information on this powerful` |
|        - | 16243 | ` * extension.` |
|        - | 16244 | ` */` |
|  9557230 | 16245 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 16246 |  |
|        - | 16247 | `	VmRefObj *pRef;` |
|        - | 16248 | `	sxu32 nBucket;` |
|        - | 16249 | `	/* Point to the appropriate bucket */` |
|  9557232 | 16250 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 16251 | `	/* Perform the lookup */` |
|  9557232 | 16252 | `	pRef = pVm->apRefObj[nBucket];` |
| 21011750 | 16253 | `	for(;;){` |
| 42012849 | 16254 | `		if( pRef == 0 ){` |
|  3311598 | 16255 | `			break;` |
|        - | 16256 | `		}` |
| 38701253 | 16257 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 16258 | `			/* Entry found */` |
|  6245636 | 16259 | `			return pRef;` |
|        - | 16260 | `		}` |
|        - | 16261 | `		/* Point to the next entry */` |
| 32455619 | 16262 | `		pRef = pRef->pNextCollide;` |
|        2 | 16263 | `	}` |
|        - | 16264 | `	/* No such entry,return NULL */` |
|  3311598 | 16265 | `	return 0;` |
|  4778617 | 16266 |  |
|        - | 16267 | `/*` |
|        - | 16268 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 16269 | ` *` |
|        - | 16270 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16271 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16272 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16273 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16274 | ` * Refer to the official for more information on this powerful` |
|        - | 16275 | ` * extension.` |
|        - | 16276 | ` */` |
|  3205822 | 16277 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 16278 |  |
|        - | 16279 | `	sxu32 nBucket;` |
|  3205824 | 16280 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 16281 | `		VmRefObj **apNew;` |
|        - | 16282 | `		sxu32 nNew;` |
|        - | 16283 | `		/* Allocate a larger table */` |
|     4492 | 16284 | `		nNew = pVm->nRefSize << 1;` |
|     4492 | 16285 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     4492 | 16286 | `		if( apNew ){` |
|     4492 | 16287 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 16288 | `			sxu32 n;` |
|        - | 16289 | `			/* Zero the structure */` |
|     4492 | 16290 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 16291 | `			/* Rehash all referenced entries */` |
|  2848166 | 16292 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 16293 | `				/* Remove old collision links */` |
|  2843676 | 16294 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 16295 | `				/* Point to the appropriate bucket */` |
|  2843676 | 16296 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 16297 | `				/* Insert the entry  */` |
|  2843676 | 16298 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2843676 | 16299 | `				if( apNew[nBucket] ){` |
|  2301116 | 16300 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1150557 | 16301 | `				}` |
|  2843676 | 16302 | `				apNew[nBucket] = pEntry;` |
|        - | 16303 | `				/* Point to the next entry */` |
|  2843676 | 16304 | `				pEntry = pEntry->pNext;` |
|  1421839 | 16305 | `			}` |
|        - | 16306 | `			/* Release the old table */` |
|     4492 | 16307 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 16308 | `			/* Install the new one */` |
|     4492 | 16309 | `			pVm->apRefObj = apNew;` |
|     4492 | 16310 | `			pVm->nRefSize = nNew;` |
|     2245 | 16311 | `		}` |
|     2245 | 16312 | `	}` |
|        - | 16313 | `	/* Point to the appropriate bucket */` |
|  3205824 | 16314 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 16315 | `	/* Insert the entry */` |
|  3205824 | 16316 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3205824 | 16317 | `	if( pVm->apRefObj[nBucket] ){` |
|  2615737 | 16318 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1307876 | 16319 | `	}` |
|  3205824 | 16320 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3205824 | 16321 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3205824 | 16322 | `	pVm->nRefUsed++;` |
|  3205824 | 16323 | `	return SXRET_OK;` |
|        2 | 16324 |  |
|        - | 16325 | `/*` |
|        - | 16326 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 16327 | ` * the reference table.` |
|        - | 16328 | ` * This function is invoked when the user perform an unset` |
|        - | 16329 | ` * call [i.e: unset($var); ].` |
|        - | 16330 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16331 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16332 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16333 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16334 | ` * Refer to the official for more information on this powerful` |
|        - | 16335 | ` * extension.` |
|        - | 16336 | ` */` |
|  3164432 | 16337 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 16338 |  |
|        - | 16339 | `	ph7_hashmap_node **apNode;` |
|        - | 16340 | `	SyHashEntry **apEntry;` |
|        - | 16341 | `	sxu32 n;` |
|        - | 16342 | `	/* Point to the reference table */` |
|  3164434 | 16343 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  3164434 | 16344 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 16345 | `	/* Unlink the entry from the reference table */` |
|  3276202 | 16346 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|   111770 | 16347 | `		if( apEntry[n] ){` |
|   111720 | 16348 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    55859 | 16349 | `		}` |
|    55886 | 16350 | `	}` |
|  6217110 | 16351 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  3052678 | 16352 | `		if( apNode[n] ){` |
|     7068 | 16353 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3533 | 16354 | `		}` |
|  1526340 | 16355 | `	}` |
|  3164434 | 16356 | `	if( pRef->pPrevCollide ){` |
|  1215589 | 16357 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   607483 | 16358 | `	}else{` |
|  1948847 | 16359 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 16360 | `	}` |
|  3164434 | 16361 | `	if( pRef->pNextCollide ){` |
|  1802704 | 16362 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   901347 | 16363 | `	}` |
|  3164434 | 16364 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 16365 | `	/* Release the node */` |
|  3164434 | 16366 | `	SySetRelease(&pRef->aReference);` |
|  3164434 | 16367 | `	SySetRelease(&pRef->aArrEntries);` |
|  3164434 | 16368 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  3164434 | 16369 | `	pVm->nRefUsed--;` |
|  3164434 | 16370 | `	return SXRET_OK;` |
|        2 | 16371 |  |
|        - | 16372 | `/*` |
|        - | 16373 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 16374 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16375 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16376 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16377 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16378 | ` * Refer to the official for more information on this powerful` |
|        - | 16379 | ` * extension.` |
|        - | 16380 | ` */` |
|  3241534 | 16381 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 16382 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 16383 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 16384 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 16385 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 16386 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 16387 | `	)` |
|        2 | 16388 |  |
|  3241536 | 16389 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 16390 | `	VmRefObj *pRef;` |
|        - | 16391 | `	/* Check if the referenced object already exists */` |
|  3241536 | 16392 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3241536 | 16393 | `	if( pRef == 0 ){` |
|        - | 16394 | `		/* Create a new entry */` |
|  3205824 | 16395 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3205824 | 16396 | `		if( pRef == 0 ){` |
|      ! 0 | 16397 | `			return SXERR_MEM;` |
|        - | 16398 | `		}` |
|  3205824 | 16399 | `		pRef->iFlags = iFlags;` |
|        - | 16400 | `		/* Install the entry */` |
|  3205824 | 16401 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1602911 | 16402 | `	}` |
|  3241536 | 16403 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3241536 | 16404 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 16405 | `		VmSlot sRef;` |
|        - | 16406 | `		/* Local frame,record referenced entry so that it can` |
|        - | 16407 | `		 * be deleted when we leave this frame.` |
|        - | 16408 | `		 */` |
|   105884 | 16409 | `		sRef.nIdx = nIdx;` |
|   105884 | 16410 | `		sRef.pUserData = pEntry;` |
|   105884 | 16411 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 16412 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 16413 | `		}` |
|    52941 | 16414 | `	}` |
|  3241536 | 16415 | `	if( pEntry ){` |
|        - | 16416 | `		/* Address of the hash-entry */` |
|   141368 | 16417 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    70683 | 16418 | `	}` |
|  3241536 | 16419 | `	if( pMapEntry ){` |
|        - | 16420 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  3091644 | 16421 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1545821 | 16422 | `	}` |
|  3241536 | 16423 | `	return SXRET_OK;` |
|  1620769 | 16424 |  |
|        - | 16425 | `/*` |
|        - | 16426 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 16427 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16428 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16429 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16430 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16431 | ` * Refer to the official for more information on this powerful` |
|        - | 16432 | ` * extension.` |
|        - | 16433 | ` */` |
|  3151456 | 16434 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 16435 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 16436 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 16437 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 16438 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 16439 | `	)` |
|        2 | 16440 |  |
|        - | 16441 | `	VmRefObj *pRef;` |
|        - | 16442 | `	sxu32 n;` |
|        - | 16443 | `	/* Check if the referenced object already exists */` |
|  3151458 | 16444 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3151458 | 16445 | `	if( pRef == 0 ){` |
|        - | 16446 | `		/* Not such entry */` |
|   105770 | 16447 | `		return SXERR_NOTFOUND;` |
|        - | 16448 | `	}` |
|        - | 16449 | `	/* Remove the desired entry */` |
|  3045690 | 16450 | `	if( pEntry ){` |
|        - | 16451 | `		SyHashEntry **apEntry;` |
|       74 | 16452 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      264 | 16453 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      192 | 16454 | `			if( apEntry[n] == pEntry ){` |
|        - | 16455 | `				/* Nullify the entry */` |
|       74 | 16456 | `				apEntry[n] = 0;` |
|        - | 16457 | `				/*` |
|        - | 16458 | `				 * NOTE:` |
|        - | 16459 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 16460 | `				 * we avoid wasting spaces.` |
|        - | 16461 | `				 */` |
|       36 | 16462 | `			}` |
|       97 | 16463 | `		}` |
|       36 | 16464 | `	}` |
|  3045690 | 16465 | `	if( pMapEntry ){` |
|        - | 16466 | `		ph7_hashmap_node **apNode;` |
|  3045618 | 16467 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  6091328 | 16468 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  3045712 | 16469 | `			if( apNode[n] == pMapEntry ){` |
|        - | 16470 | `				/* nullify the entry */` |
|  3045618 | 16471 | `				apNode[n] = 0;` |
|  1522808 | 16472 | `			}` |
|  1522857 | 16473 | `		}` |
|  1522808 | 16474 | `	}` |
|  3045690 | 16475 | `	return SXRET_OK;` |
|  1575730 | 16476 |  |
|        - | 16477 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 16478 | `/*` |
|        - | 16479 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 16480 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 16481 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 16482 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 16483 | ` * For more information on how to register IO stream devices,please` |
|        - | 16484 | ` * refer to the official documentation.` |
|        - | 16485 | ` */` |
|    29242 | 16486 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 16487 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 16488 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 16489 | `	int nByte              /* *pzDevice length*/` |
|        - | 16490 | `	)` |
|        2 | 16491 |  |
|        - | 16492 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 16493 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 16494 | `	SyString sDev,sCur;` |
|        - | 16495 | `	sxu32 n,nEntry;` |
|        - | 16496 | `	int rc;` |
|        - | 16497 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    29244 | 16498 | `	zNext = zCur = zIn = *pzDevice;` |
|    29244 | 16499 | `	zEnd = &zIn[nByte];` |
|  1867416 | 16500 | `	while( zIn < zEnd ){` |
|  1838176 | 16501 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 16502 | `			/* Got one */` |
|        3 | 16503 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 16504 | `			break;` |
|        - | 16505 | `		}` |
|        - | 16506 | `		/* Advance the cursor */` |
|  1838174 | 16507 | `		zIn++;` |
|        2 | 16508 | `	}` |
|    29244 | 16509 | `	if( zIn >= zEnd ){` |
|        - | 16510 | `		/* No such scheme,return the default stream */` |
|    29242 | 16511 | `		return pVm->pDefStream;` |
|        - | 16512 | `	}` |
|        3 | 16513 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 16514 | `	/* Remove leading and trailing white spaces */` |
|        3 | 16515 | `	SyStringFullTrim(&sDev);` |
|        - | 16516 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 16517 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 16518 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 16519 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 16520 | `		pStream = apStream[n];` |
|        3 | 16521 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 16522 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 16523 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 16524 | `		if( rc == 0 ){` |
|        - | 16525 | `			/* Stream device found */` |
|        3 | 16526 | `			*pzDevice = zNext;` |
|        3 | 16527 | `			return pStream;` |
|        - | 16528 | `		}` |
|      ! 0 | 16529 | `	}` |
|        - | 16530 | `	/* No such stream,return NULL */` |
|      ! 0 | 16531 | `	return 0;` |
|    14623 | 16532 |  |
|        - | 16533 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 16534 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 16535 |  |
