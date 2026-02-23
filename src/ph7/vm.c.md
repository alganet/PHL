# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5225/7390 lines (70.70%)

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
|        - |     9 | `/*` |
|        - |    10 | ` * The code in this file implements execution method of the PH7 Virtual Machine.` |
|        - |    11 | ` * The PH7 compiler (implemented in 'compiler.c' and 'parse.c') generates a bytecode program` |
|        - |    12 | ` * which is then executed by the virtual machine implemented here to do the work of the PHP` |
|        - |    13 | ` * statements.` |
|        - |    14 | ` * PH7 bytecode programs are similar in form to assembly language. The program consists` |
|        - |    15 | ` * of a linear sequence of operations .Each operation has an opcode and 3 operands.` |
|        - |    16 | ` * Operands P1 and P2 are integers where the first is signed while the second is unsigned.` |
|        - |    17 | ` * Operand P3 is an arbitrary pointer specific to each instruction. The P2 operand is usually` |
|        - |    18 | ` * the jump destination used by the OP_JMP,OP_JZ,OP_JNZ,... instructions.` |
|        - |    19 | ` * Opcodes will typically ignore one or more operands. Many opcodes ignore all three operands.` |
|        - |    20 | ` * Computation results are stored on a stack. Each entry on the stack is of type ph7_value.` |
|        - |    21 | ` * PH7 uses the ph7_value object to represent all values that can be stored in a PHP variable.` |
|        - |    22 | ` * Since PHP uses dynamic typing for the values it stores. Values stored in ph7_value objects` |
|        - |    23 | ` * can be integers,floating point values,strings,arrays,class instances (object in the PHP jargon)` |
|        - |    24 | ` * and so on.` |
|        - |    25 | ` * Internally,the PH7 virtual machine manipulates nearly all PHP values as ph7_values structures.` |
|        - |    26 | ` * Each ph7_value may cache multiple representations(string,integer etc.) of the same value.` |
|        - |    27 | ` * An implicit conversion from one type to the other occurs as necessary.` |
|        - |    28 | ` * Most of the code in this file is taken up by the [VmByteCodeExec()] function which does` |
|        - |    29 | ` * the work of interpreting a PH7 bytecode program. But other routines are also provided` |
|        - |    30 | ` * to help in building up a program instruction by instruction. Also note that sepcial` |
|        - |    31 | ` * functions that need access to the underlying virtual machine details such as [die()],` |
|        - |    32 | ` * [func_get_args()],[call_user_func()],[ob_start()] and many more are implemented here.` |
|        - |    33 | ` */` |
|        - |    34 | `/*` |
|        - |    35 | ` * Each active virtual machine frame is represented by an instance` |
|        - |    36 | ` * of the following structure.` |
|        - |    37 | ` * VM Frame hold local variables and other stuff related to function call.` |
|        - |    38 | ` */` |
|        - |    39 | `struct VmFrame` |
|        - |    40 |  |
|        - |    41 | `	VmFrame *pParent; /* Parent frame or NULL if global scope */` |
|        - |    42 | `	void *pUserData;  /* Upper layer private data associated with this frame */` |
|        - |    43 | `	ph7_class_instance *pThis; /* Current class instance [i.e: the '$this' variable].NULL otherwise */` |
|        - |    44 | `	SySet sLocal;     /* Local variables container (VmSlot instance) */` |
|        - |    45 | `	ph7_vm *pVm;      /* VM that own this frame */` |
|        - |    46 | `	SyHash hVar;      /* Variable hashtable for fast lookup */` |
|        - |    47 | `	SySet sArg;       /* Function arguments container */` |
|        - |    48 | `	SySet sRef;       /* Local reference table (VmSlot instance) */` |
|        - |    49 | `	sxi32 iFlags;     /* Frame configuration flags (See below)*/` |
|        - |    50 | `	sxu32 iExceptionJump; /* Exception jump destination */` |
|        - |    51 | `};` |
|        - |    52 | `#define VM_FRAME_EXCEPTION  0x01 /* Special Exception frame */` |
|        - |    53 | `#define VM_FRAME_THROW      0x02 /* An exception was thrown */` |
|        - |    54 | `#define VM_FRAME_CATCH      0x04 /* Catch frame */` |
|        - |    55 | `/*` |
|        - |    56 | ` * When a user defined variable is released (via manual unset($x) or garbage collected)` |
|        - |    57 | ` * memory object index is stored in an instance of the following structure and put` |
|        - |    58 | ` * in the free object table so that it can be reused again without allocating` |
|        - |    59 | ` * a new memory object.` |
|        - |    60 | ` */` |
|        - |    61 | `typedef struct VmSlot VmSlot;` |
|        - |    62 | `struct VmSlot` |
|        - |    63 |  |
|        - |    64 | `	sxu32 nIdx;      /* Index in pVm->aMemObj[] */` |
|        - |    65 | `	void *pUserData; /* Upper-layer private data */` |
|        - |    66 | `};` |
|        - |    67 | `/*` |
|        - |    68 | ` * An entry in the reference table is represented by an instance of the` |
|        - |    69 | ` * follwoing table.` |
|        - |    70 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - |    71 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - |    72 | ` * the reference implementation is consistent,solid and it's` |
|        - |    73 | ` * behavior resemble the C++ reference mechanism.` |
|        - |    74 | ` * Refer to the official for more information on this powerful` |
|        - |    75 | ` * extension.` |
|        - |    76 | ` */` |
|        - |    77 | `struct VmRefObj` |
|        - |    78 |  |
|        - |    79 | `	SySet aReference;  /* Table of references to this memory object */` |
|        - |    80 | `	SySet aArrEntries; /* Foreign hashmap entries [i.e: array(&$a) ] */` |
|        - |    81 | `	sxu32 nIdx;        /* Referenced object index */` |
|        - |    82 | `	sxi32 iFlags;      /* Configuration flags */` |
|        - |    83 | `	VmRefObj *pNextCollide,*pPrevCollide; /* Collision link */` |
|        - |    84 | `	VmRefObj *pNext,*pPrev;               /* List of all referenced objects */` |
|        - |    85 | `};` |
|        - |    86 | `#define VM_REF_IDX_KEEP  0x001 /* Do not restore the memory object to the free list */` |
|        - |    87 | `/*` |
|        - |    88 | ` * Output control buffer entry.` |
|        - |    89 | ` * Refer to the implementation of [ob_start()] for more information.` |
|        - |    90 | ` */` |
|        - |    91 | `typedef struct VmObEntry VmObEntry;` |
|        - |    92 | `struct VmObEntry` |
|        - |    93 |  |
|        - |    94 | `	ph7_value sCallback; /* User defined callback */` |
|        - |    95 | `	SyBlob sOB;          /* Output buffer consumer */` |
|        - |    96 | `};` |
|        - |    97 | `/*` |
|        - |    98 | ` * Each installed shutdown callback (registered using [register_shutdown_function()] )` |
|        - |    99 | ` * is stored in an instance of the following structure.` |
|        - |   100 | ` * Refer to the implementation of [register_shutdown_function(()] for more information.` |
|        - |   101 | ` */` |
|        - |   102 | `typedef struct VmShutdownCB VmShutdownCB;` |
|        - |   103 | `struct VmShutdownCB` |
|        - |   104 |  |
|        - |   105 | `	ph7_value sCallback; /* Shutdown callback */` |
|        - |   106 | `	ph7_value aArg[10];   /* Callback arguments (10 maximum arguments) */` |
|        - |   107 | `	int nArg;             /* Total number of given arguments */` |
|        - |   108 | `};` |
|        - |   109 | `/* Uncaught exception code value */` |
|        - |   110 | `#define PH7_EXCEPTION -255` |
|        - |   111 |  |
|        - |   112 | `/*` |
|        - |   113 | ` * Return TRUE if either operand is a NaN real value.` |
|        - |   114 | ` */` |
|   491542 |   115 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |   116 |  |
|   491544 |   117 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       23 |   118 | `		return TRUE;` |
|        - |   119 | `	}` |
|   491522 |   120 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|        9 |   121 | `		return TRUE;` |
|        - |   122 | `	}` |
|   491514 |   123 | `	return FALSE;` |
|   245795 |   124 |  |
|        - |   125 | `/*` |
|        - |   126 | ` * Each parsed URI is recorded and stored in an instance of the following structure.` |
|        - |   127 | ` * This structure and it's related routines are taken verbatim from the xHT project` |
|        - |   128 | ` * [A modern embeddable HTTP engine implementing all the RFC2616 methods]` |
|        - |   129 | ` * the xHT project is developed internally by Symisc Systems.` |
|        - |   130 | ` */` |
|        - |   131 | `typedef struct SyhttpUri SyhttpUri;` |
|        - |   132 | `struct SyhttpUri` |
|        - |   133 |  |
|        - |   134 | `	SyString sHost;     /* Hostname or IP address */` |
|        - |   135 | `	SyString sPort;     /* Port number */` |
|        - |   136 | `	SyString sPath;     /* Mandatory resource path passed verbatim (Not decoded) */` |
|        - |   137 | `	SyString sQuery;    /* Query part */` |
|        - |   138 | `	SyString sFragment; /* Fragment part */` |
|        - |   139 | `	SyString sScheme;   /* Scheme */` |
|        - |   140 | `	SyString sUser;     /* Username */` |
|        - |   141 | `	SyString sPass;     /* Password */` |
|        - |   142 | `	SyString sRaw;      /* Raw URI */` |
|        - |   143 | `};` |
|        - |   144 | `/*` |
|        - |   145 | ` * An instance of the following structure is used to record all MIME headers seen` |
|        - |   146 | ` * during a HTTP interaction.` |
|        - |   147 | ` * This structure and it's related routines are taken verbatim from the xHT project` |
|        - |   148 | ` * [A modern embeddable HTTP engine implementing all the RFC2616 methods]` |
|        - |   149 | ` * the xHT project is developed internally by Symisc Systems.` |
|        - |   150 | ` */` |
|        - |   151 | `typedef struct SyhttpHeader SyhttpHeader;` |
|        - |   152 | `struct SyhttpHeader` |
|        - |   153 |  |
|        - |   154 | `	SyString sName;    /* Header name [i.e:"Content-Type","Host","User-Agent"]. NOT NUL TERMINATED */` |
|        - |   155 | `	SyString sValue;   /* Header values [i.e: "text/html"]. NOT NUL TERMINATED */` |
|        - |   156 | `};` |
|        - |   157 | `/*` |
|        - |   158 | ` * Supported HTTP methods.` |
|        - |   159 | ` */` |
|        - |   160 | `#define HTTP_METHOD_GET  1 /* GET */` |
|        - |   161 | `#define HTTP_METHOD_HEAD 2 /* HEAD */` |
|        - |   162 | `#define HTTP_METHOD_POST 3 /* POST */` |
|        - |   163 | `#define HTTP_METHOD_PUT  4 /* PUT */` |
|        - |   164 | `#define HTTP_METHOD_OTHR 5 /* Other HTTP methods [i.e: DELETE,TRACE,OPTIONS...]*/` |
|        - |   165 | `/*` |
|        - |   166 | ` * Supported HTTP protocol version.` |
|        - |   167 | ` */` |
|        - |   168 | `#define HTTP_PROTO_10 1 /* HTTP/1.0 */` |
|        - |   169 | `#define HTTP_PROTO_11 2 /* HTTP/1.1 */` |
|        - |   170 | `/*` |
|        - |   171 | ` * Register a constant and it's associated expansion callback so that` |
|        - |   172 | ` * it can be expanded from the target PHP program.` |
|        - |   173 | ` * The constant expansion mechanism under PH7 is extremely powerful yet` |
|        - |   174 | ` * simple and work as follows:` |
|        - |   175 | ` * Each registered constant have a C procedure associated with it.` |
|        - |   176 | ` * This procedure known as the constant expansion callback is responsible` |
|        - |   177 | ` * of expanding the invoked constant to the desired value,for example:` |
|        - |   178 | ` * The C procedure associated with the "__PI__" constant expands to 3.14 (the value of PI).` |
|        - |   179 | ` * The "__OS__" constant procedure expands to the name of the host Operating Systems` |
|        - |   180 | ` * (Windows,Linux,...) and so on.` |
|        - |   181 | ` * Please refer to the official documentation for additional information.` |
|        - |   182 | ` */` |
|   193130 |   183 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
|        - |   184 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |   185 | `	const SyString *pName,  /* Constant name */` |
|        - |   186 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|        - |   187 | `	void *pUserData         /* Last argument to xExpand() */` |
|        - |   188 | `	)` |
|        2 |   189 |  |
|        - |   190 | `	ph7_constant *pCons;` |
|        - |   191 | `	SyHashEntry *pEntry;` |
|        - |   192 | `	char *zDupName;` |
|        - |   193 | `	sxi32 rc;` |
|   193132 |   194 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   193132 |   195 | `	if( pEntry ){` |
|        - |   196 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   197 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   198 | `		pCons->xExpand = xExpand;` |
|        6 |   199 | `		pCons->pUserData = pUserData;` |
|        6 |   200 | `		return SXRET_OK;` |
|        - |   201 | `	}` |
|        - |   202 | `	/* Allocate a new constant instance */` |
|   193128 |   203 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   193128 |   204 | `	if( pCons == 0 ){` |
|      ! 0 |   205 | `		return 0;` |
|        - |   206 | `	}` |
|        - |   207 | `	/* Duplicate constant name */` |
|   193128 |   208 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   193128 |   209 | `	if( zDupName == 0 ){` |
|      ! 0 |   210 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   211 | `		return 0;` |
|        - |   212 | `	}` |
|        - |   213 | `	/* Install the constant */` |
|   193128 |   214 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   193128 |   215 | `	pCons->xExpand = xExpand;` |
|   193128 |   216 | `	pCons->pUserData = pUserData;` |
|   193128 |   217 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   193128 |   218 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   219 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   220 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   221 | `		return rc;` |
|        - |   222 | `	}` |
|        - |   223 | `	/* All done,constant can be invoked from PHP code */` |
|   193128 |   224 | `	return SXRET_OK;` |
|    96567 |   225 |  |
|        - |   226 | `/*` |
|        - |   227 | ` * Allocate a new foreign function instance.` |
|        - |   228 | ` * This function return SXRET_OK on success. Any other` |
|        - |   229 | ` * return value indicates failure.` |
|        - |   230 | ` * Please refer to the official documentation for an introduction to` |
|        - |   231 | ` * the foreign function mechanism.` |
|        - |   232 | ` */` |
|   415860 |   233 | `static sxi32 PH7_NewForeignFunction(` |
|        - |   234 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   235 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   236 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   237 | `	void *pUserData,          /* Foreign function private data */` |
|        - |   238 | `	ph7_user_func **ppOut     /* OUT: VM image of the foreign function */` |
|        - |   239 | `	)` |
|        2 |   240 |  |
|        - |   241 | `	ph7_user_func *pFunc;` |
|        - |   242 | `	char *zDup;` |
|        - |   243 | `	/* Allocate a new user function */` |
|   415862 |   244 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|   415862 |   245 | `	if( pFunc == 0 ){` |
|      ! 0 |   246 | `		return SXERR_MEM;` |
|        - |   247 | `	}` |
|        - |   248 | `	/* Duplicate function name */` |
|   415862 |   249 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   415862 |   250 | `	if( zDup == 0 ){` |
|      ! 0 |   251 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   252 | `		return SXERR_MEM;` |
|        - |   253 | `	}` |
|        - |   254 | `	/* Zero the structure */` |
|   415862 |   255 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   256 | `	/* Initialize structure fields */` |
|   415862 |   257 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|   415862 |   258 | `	pFunc->pVm   = pVm;` |
|   415862 |   259 | `	pFunc->xFunc = xFunc;` |
|   415862 |   260 | `	pFunc->pUserData = pUserData;` |
|   415862 |   261 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   262 | `	/* Write a pointer to the new function */` |
|   415862 |   263 | `	*ppOut = pFunc;` |
|   415862 |   264 | `	return SXRET_OK;` |
|   207932 |   265 |  |
|        - |   266 | `/*` |
|        - |   267 | ` * Install a foreign function and it's associated callback so that` |
|        - |   268 | ` * it can be invoked from the target PHP code.` |
|        - |   269 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   270 | ` * return value indicates failure.` |
|        - |   271 | ` * Please refer to the official documentation for an introduction to` |
|        - |   272 | ` * the foreign function mechanism.` |
|        - |   273 | ` */` |
|   416816 |   274 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
|        - |   275 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   276 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   277 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   278 | `	void *pUserData           /* Foreign function private data */` |
|        - |   279 | `	)` |
|        2 |   280 |  |
|        - |   281 | `	ph7_user_func *pFunc;` |
|        - |   282 | `	SyHashEntry *pEntry;` |
|        - |   283 | `	sxi32 rc;` |
|        - |   284 | `	/* Overwrite any previously registered function with the same name */` |
|   416818 |   285 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|   416818 |   286 | `	if( pEntry ){` |
|      958 |   287 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|      958 |   288 | `		pFunc->pUserData = pUserData;` |
|      958 |   289 | `		pFunc->xFunc = xFunc;` |
|      958 |   290 | `		SySetReset(&pFunc->aAux);` |
|      958 |   291 | `		return SXRET_OK;` |
|        - |   292 | `	}` |
|        - |   293 | `	/* Create a new user function */` |
|   415862 |   294 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|   415862 |   295 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   296 | `		return rc;` |
|        - |   297 | `	}` |
|        - |   298 | `	/* Install the function in the corresponding hashtable */` |
|   415862 |   299 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|   415862 |   300 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   301 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   302 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   303 | `		return rc;` |
|        - |   304 | `	}` |
|        - |   305 | `	/* User function successfully installed */` |
|   415862 |   306 | `	return SXRET_OK;` |
|   208410 |   307 |  |
|        - |   308 | `/*` |
|        - |   309 | ` * Initialize a VM function.` |
|        - |   310 | ` */` |
|    51680 |   311 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   312 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   313 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   314 | `	const char *zName,  /* Function name */` |
|        - |   315 | `	sxu32 nByte,        /* zName length */` |
|        - |   316 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   317 | `	void *pUserData     /* Function private data */` |
|        - |   318 | `	)` |
|        2 |   319 |  |
|        - |   320 | `	/* Zero the structure */` |
|    51682 |   321 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   322 | `	/* Initialize structure fields */` |
|        - |   323 | `	/* Arguments container */` |
|    51682 |   324 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   325 | `	/* Static variable container */` |
|    51682 |   326 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   327 | `	/* Bytecode container */` |
|    51682 |   328 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   329 | `    /* Preallocate some instruction slots */` |
|    51682 |   330 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   331 | `	/* Closure environment */` |
|    51682 |   332 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|    51682 |   333 | `	pFunc->iFlags = iFlags;` |
|    51682 |   334 | `	pFunc->pUserData = pUserData;` |
|    51682 |   335 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|    51682 |   336 | `	return SXRET_OK;` |
|        2 |   337 |  |
|        - |   338 | `/*` |
|        - |   339 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   340 | ` */` |
|   138070 |   341 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   342 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   343 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   344 | `	SyString *pName     /* Function name */` |
|        - |   345 | `	)` |
|        2 |   346 |  |
|        - |   347 | `	SyHashEntry *pEntry;` |
|        - |   348 | `	sxi32 rc;` |
|   138072 |   349 | `	if( pName == 0 ){` |
|        - |   350 | `		/* Use the built-in name */` |
|    16210 |   351 | `		pName = &pFunc->sName;` |
|     8104 |   352 | `	}` |
|        - |   353 | `	/* Check for duplicates (functions with the same name) first */` |
|   138072 |   354 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   138072 |   355 | `	if( pEntry ){` |
|    96328 |   356 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|    96328 |   357 | `		if( pLink != pFunc ){` |
|        - |   358 | `			/* Link */` |
|      185 |   359 | `			pFunc->pNextName = pLink;` |
|      185 |   360 | `			pEntry->pUserData = pFunc;` |
|       92 |   361 | `		}` |
|    96328 |   362 | `		return SXRET_OK;` |
|        - |   363 | `	}` |
|        - |   364 | `	/* First time seen */` |
|    41746 |   365 | `	pFunc->pNextName = 0;` |
|    41746 |   366 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|    41746 |   367 | `	return rc;` |
|    69037 |   368 |  |
|        - |   369 | `/*` |
|        - |   370 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   371 | ` */` |
|    12412 |   372 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   373 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   374 | `	ph7_class *pClass /* Target Class */` |
|        - |   375 | `	)` |
|        2 |   376 |  |
|    12414 |   377 | `	SyString *pName = &pClass->sName;` |
|        - |   378 | `	SyHashEntry *pEntry;` |
|        - |   379 | `	sxi32 rc;` |
|        - |   380 | `	/* Check for duplicates */` |
|    12414 |   381 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    12414 |   382 | `	if( pEntry ){` |
|       63 |   383 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   384 | `		/* Link entry with the same name */` |
|       63 |   385 | `		pClass->pNextName = pLink;` |
|       63 |   386 | `		pEntry->pUserData = pClass;` |
|       63 |   387 | `		return SXRET_OK;` |
|        - |   388 | `	}` |
|    12352 |   389 | `	pClass->pNextName = 0;` |
|        - |   390 | `	/* Perform a simple hashtable insertion */` |
|    12352 |   391 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    12352 |   392 | `	return rc;` |
|     6208 |   393 |  |
|        - |   394 | `/*` |
|        - |   395 | ` * Instruction builder interface.` |
|        - |   396 | ` */` |
|  1306516 |   397 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
|        - |   398 | `	ph7_vm *pVm,  /* Target VM */` |
|        - |   399 | `	sxi32 iOp,    /* Operation to perform */` |
|        - |   400 | `	sxi32 iP1,    /* First operand */` |
|        - |   401 | `	sxu32 iP2,    /* Second operand */` |
|        - |   402 | `	void *p3,     /* Third operand */` |
|        - |   403 | `	sxu32 *pIndex /* Instruction index. NULL otherwise */` |
|        - |   404 | `	)` |
|        2 |   405 |  |
|        - |   406 | `	VmInstr sInstr;` |
|        - |   407 | `	sxi32 rc;` |
|        - |   408 | `	/* Fill the VM instruction */` |
|  1306518 |   409 | `	sInstr.iOp = (sxu8)iOp;` |
|  1306518 |   410 | `	sInstr.iP1 = iP1;` |
|  1306518 |   411 | `	sInstr.iP2 = iP2;` |
|  1306518 |   412 | `	sInstr.p3  = p3;` |
|  1306518 |   413 | `	if( pIndex ){` |
|        - |   414 | `		/* Instruction index in the bytecode array */` |
|    78604 |   415 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    39301 |   416 | `	}` |
|        - |   417 | `	/* Finally,record the instruction */` |
|  1306518 |   418 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  1306518 |   419 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   420 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   421 | `		/* Fall throw */` |
|      ! 0 |   422 | `	}` |
|  1306518 |   423 | `	return rc;` |
|        2 |   424 |  |
|        - |   425 | `/*` |
|        - |   426 | ` * Swap the current bytecode container with the given one.` |
|        - |   427 | ` */` |
|   125748 |   428 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   429 |  |
|   125750 |   430 | `	if( pContainer == 0 ){` |
|        - |   431 | `		/* Point to the default container */` |
|      ! 0 |   432 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   433 | `	}else{` |
|        - |   434 | `		/* Change container */` |
|   125750 |   435 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   436 | `	}` |
|   125750 |   437 | `	return SXRET_OK;` |
|        2 |   438 |  |
|        - |   439 | `/*` |
|        - |   440 | ` * Return the current bytecode container.` |
|        - |   441 | ` */` |
|    62874 |   442 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   443 |  |
|    62876 |   444 | `	return pVm->pByteContainer;` |
|        2 |   445 |  |
|        - |   446 | `/*` |
|        - |   447 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   448 | ` */` |
|    77314 |   449 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   450 |  |
|        - |   451 | `	VmInstr *pInstr;` |
|    77316 |   452 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|    77316 |   453 | `	return pInstr;` |
|        2 |   454 |  |
|        - |   455 | `/*` |
|        - |   456 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   457 | ` */` |
|   377784 |   458 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   459 |  |
|   377786 |   460 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   461 |  |
|        - |   462 | `/*` |
|        - |   463 | ` * Pop the last VM instruction.` |
|        - |   464 | ` */` |
|    73964 |   465 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   466 |  |
|    73966 |   467 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   468 |  |
|        - |   469 | `/*` |
|        - |   470 | ` * Peek the last VM instruction.` |
|        - |   471 | ` */` |
|   197858 |   472 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   473 |  |
|   197860 |   474 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   475 |  |
|     2652 |   476 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   477 |  |
|        - |   478 | `	VmInstr *aInstr;` |
|        - |   479 | `	sxu32 n;` |
|     2654 |   480 | `	n = SySetUsed(pVm->pByteContainer);` |
|     2654 |   481 | `	if( n < 2 ){` |
|      ! 0 |   482 | `		return 0;` |
|        - |   483 | `	}` |
|     2654 |   484 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|     2654 |   485 | `	return &aInstr[n - 2];` |
|     1328 |   486 |  |
|        - |   487 | `/*` |
|        - |   488 | ` * Allocate a new virtual machine frame.` |
|        - |   489 | ` */` |
|     8972 |   490 | `static VmFrame * VmNewFrame(` |
|        - |   491 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   492 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   493 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   494 | `	)` |
|        2 |   495 |  |
|        - |   496 | `	VmFrame *pFrame;` |
|        - |   497 | `	/* Allocate a new vm frame */` |
|     8974 |   498 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|     8974 |   499 | `	if( pFrame == 0 ){` |
|      ! 0 |   500 | `		return 0;` |
|        - |   501 | `	}` |
|        - |   502 | `	/* Zero the structure */` |
|     8974 |   503 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   504 | `	/* Initialize frame fields */` |
|     8974 |   505 | `	pFrame->pUserData = pUserData;` |
|     8974 |   506 | `	pFrame->pThis = pThis;` |
|     8974 |   507 | `	pFrame->pVm = pVm;` |
|     8974 |   508 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|     8974 |   509 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|     8974 |   510 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|     8974 |   511 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|     8974 |   512 | `	return pFrame;` |
|     4488 |   513 |  |
|        - |   514 | `/*` |
|        - |   515 | ` * Enter a VM frame.` |
|        - |   516 | ` */` |
|     8972 |   517 | `static sxi32 VmEnterFrame(` |
|        - |   518 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   519 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   520 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   521 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   522 | `	)` |
|        2 |   523 |  |
|        - |   524 | `	VmFrame *pFrame;` |
|        - |   525 | `	/* Allocate a new frame */` |
|     8974 |   526 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|     8974 |   527 | `	if( pFrame == 0 ){` |
|      ! 0 |   528 | `		return SXERR_MEM;` |
|        - |   529 | `	}` |
|        - |   530 | `	/* Link to the list of active VM frame */` |
|     8974 |   531 | `	pFrame->pParent = pVm->pFrame;` |
|     8974 |   532 | `	pVm->pFrame = pFrame;` |
|     8974 |   533 | `	if( ppFrame ){` |
|        - |   534 | `		/* Write a pointer to the new VM frame */` |
|     7758 |   535 | `		*ppFrame = pFrame;` |
|     3878 |   536 | `	}` |
|     8974 |   537 | `	return SXRET_OK;` |
|     4488 |   538 |  |
|        - |   539 | `/*` |
|        - |   540 | ` * Link a foreign variable with the TOP most active frame.` |
|        - |   541 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|        - |   542 | ` * information.` |
|        - |   543 | ` */` |
|       30 |   544 | `static sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|        1 |   545 |  |
|        - |   546 | `	VmFrame *pTarget,*pFrame;` |
|       31 |   547 | `	SyHashEntry *pEntry = 0;` |
|        - |   548 | `	sxi32 rc;` |
|        - |   549 | `	/* Point to the upper frame */` |
|       31 |   550 | `	pFrame = pVm->pFrame;` |
|       31 |   551 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |   552 | `		/* Safely ignore the exception frame */` |
|      ! 0 |   553 | `		pFrame = pFrame->pParent;` |
|      ! 0 |   554 | `	}` |
|       31 |   555 | `	pTarget = pFrame;` |
|       31 |   556 | `	pFrame = pTarget->pParent;` |
|       45 |   557 | `	while( pFrame ){` |
|       45 |   558 | `		if( (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   559 | `			/* Query the current frame */` |
|       31 |   560 | `			pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|       31 |   561 | `			if( pEntry ){` |
|        - |   562 | `				/* Variable found */` |
|       31 |   563 | `				break;` |
|        - |   564 | `			}` |
|      ! 0 |   565 | `		}` |
|        - |   566 | `		/* Point to the upper frame */` |
|       15 |   567 | `		pFrame = pFrame->pParent;` |
|        1 |   568 | `	}` |
|       31 |   569 | `	if( pEntry == 0 ){` |
|        - |   570 | `		/* Inexistant variable */` |
|      ! 0 |   571 | `		return SXERR_NOTFOUND;` |
|        - |   572 | `	}` |
|        - |   573 | `	/* Link to the current frame */` |
|       31 |   574 | `	rc = SyHashInsert(&pTarget->hVar,pEntry->pKey,pEntry->nKeyLen,pEntry->pUserData);` |
|       31 |   575 | `	if( rc == SXRET_OK ){` |
|        - |   576 | `		sxu32 nIdx;` |
|       31 |   577 | `		nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|       31 |   578 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pTarget->hVar),0,0);` |
|       15 |   579 | `	}` |
|       31 |   580 | `	return rc;` |
|       16 |   581 |  |
|        - |   582 | `/*` |
|        - |   583 | ` * Leave the top-most active frame.` |
|        - |   584 | ` */` |
|     7754 |   585 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   586 |  |
|     7756 |   587 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|     7756 |   588 | `	if( pCurFrame ){` |
|        - |   589 | `		/* Unlink from the list of active VM frame */` |
|     7756 |   590 | `		pVm->pFrame = pCurFrame->pParent;` |
|     7756 |   591 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   592 | `			VmSlot  *aSlot;` |
|        - |   593 | `			sxu32 n;` |
|        - |   594 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|     7738 |   595 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    57650 |   596 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   597 | `				/* Unset the local variable */` |
|    49914 |   598 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    24958 |   599 | `			}` |
|        - |   600 | `			/* Remove local reference */` |
|     7738 |   601 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    57684 |   602 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    49948 |   603 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    24975 |   604 | `			}` |
|     3868 |   605 | `		}` |
|        - |   606 | `		/* Release internal containers */` |
|     7756 |   607 | `		SyHashRelease(&pCurFrame->hVar);` |
|     7756 |   608 | `		SySetRelease(&pCurFrame->sArg);` |
|     7756 |   609 | `		SySetRelease(&pCurFrame->sLocal);` |
|     7756 |   610 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   611 | `		/* Release the whole structure */` |
|     7756 |   612 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     3877 |   613 | `	}` |
|     7756 |   614 |  |
|        - |   615 | `/*` |
|        - |   616 | ` * Compare two functions signature and return the comparison result.` |
|        - |   617 | ` */` |
|      818 |   618 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|        1 |   619 |  |
|      819 |   620 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|      819 |   621 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|      819 |   622 | `	const char *zSin = pSecond->zString;` |
|      819 |   623 | `	const char *zFin = pFirst->zString;` |
|      819 |   624 | `	const char *zPtr = zFin;` |
|      409 |   625 | `	for(;;){` |
|      819 |   626 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|      410 |   627 | `			break;` |
|        - |   628 | `		}` |
|      ! 0 |   629 | `		if( zFin[0] != zSin[0] ){` |
|        - |   630 | `			/* mismatch */` |
|      ! 0 |   631 | `			break;` |
|        - |   632 | `		}` |
|      ! 0 |   633 | `		zFin++;` |
|      ! 0 |   634 | `		zSin++;` |
|      ! 0 |   635 | `	}` |
|      819 |   636 | `	return (int)(zFin-zPtr);` |
|        1 |   637 |  |
|        - |   638 | `/*` |
|        - |   639 | ` * Select the appropriate VM function for the current call context.` |
|        - |   640 | ` * This is the implementation of the powerful 'function overloading' feature` |
|        - |   641 | ` * introduced by the version 2 of the PH7 engine.` |
|        - |   642 | ` * Refer to the official documentation for more information.` |
|        - |   643 | ` */` |
|      128 |   644 | `static ph7_vm_func * VmOverload(` |
|        - |   645 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |   646 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|        - |   647 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|        - |   648 | `	int nArg             /* Total number of passed arguments  */` |
|        - |   649 | `	)` |
|        1 |   650 |  |
|        - |   651 | `	int iTarget,i,j,iCur,iMax;` |
|        - |   652 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|        - |   653 | `	ph7_vm_func *pLink;` |
|        - |   654 | `	SyString sArgSig;` |
|        - |   655 | `	SyBlob sSig;` |
|        - |   656 |  |
|      129 |   657 | `	pLink = pList;` |
|      129 |   658 | `	i = 0;` |
|        - |   659 | `	/* Put functions expecting the same number of passed arguments */` |
|     1073 |   660 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|     1011 |   661 | `		if( pLink == 0 ){` |
|       67 |   662 | `			break;` |
|        - |   663 | `		}` |
|      945 |   664 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|        - |   665 | `			/* Candidate for overloading */` |
|      883 |   666 | `			apSet[i++] = pLink;` |
|      441 |   667 | `		}` |
|        - |   668 | `		/* Point to the next entry */` |
|      945 |   669 | `		pLink = pLink->pNextName;` |
|        1 |   670 | `	}` |
|      129 |   671 | `	if( i < 1 ){` |
|        - |   672 | `		/* No candidates,return the head of the list */` |
|      ! 0 |   673 | `		return pList;` |
|        - |   674 | `	}` |
|      129 |   675 | `	if( nArg < 1 \|\| i < 2 ){` |
|        - |   676 | `		/* Return the only candidate */` |
|       27 |   677 | `		return apSet[0];` |
|        - |   678 | `	}` |
|        - |   679 | `	/* Calculate function signature */` |
|      103 |   680 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|      355 |   681 | `	for( j = 0 ; j < nArg ; j++ ){` |
|      253 |   682 | `		int c = 'n'; /* null */` |
|      253 |   683 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|        - |   684 | `			/* Hashmap */` |
|       45 |   685 | `			c = 'h';` |
|      231 |   686 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|        - |   687 | `			/* bool */` |
|      ! 0 |   688 | `			c = 'b';` |
|      209 |   689 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|        - |   690 | `			/* int */` |
|        5 |   691 | `			c = 'i';` |
|      207 |   692 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|        - |   693 | `			/* String */` |
|      105 |   694 | `			c = 's';` |
|      153 |   695 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|        - |   696 | `			/* Float */` |
|      ! 0 |   697 | `			c = 'f';` |
|      101 |   698 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|        - |   699 | `			/* Class instance */` |
|      ! 0 |   700 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|      ! 0 |   701 | `			SyString *pName = &pClass->sName;` |
|      ! 0 |   702 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|      ! 0 |   703 | `			c = -1;` |
|      ! 0 |   704 | `		}` |
|      253 |   705 | `		if( c > 0 ){` |
|      253 |   706 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|      126 |   707 | `		}` |
|      127 |   708 | `	}` |
|      103 |   709 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|      103 |   710 | `	iTarget = 0;` |
|      103 |   711 | `	iMax = -1;` |
|        - |   712 | `	/* Select the appropriate function */` |
|      921 |   713 | `	for( j = 0 ; j < i ; j++ ){` |
|        - |   714 | `		/* Compare the two signatures */` |
|      819 |   715 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|      819 |   716 | `		if( iCur > iMax ){` |
|      103 |   717 | `			iMax = iCur;` |
|      103 |   718 | `			iTarget = j;` |
|       51 |   719 | `		}` |
|      410 |   720 | `	}` |
|      103 |   721 | `	SyBlobRelease(&sSig);` |
|        - |   722 | `	/* Appropriate function for the current call context */` |
|      103 |   723 | `	return apSet[iTarget];` |
|       65 |   724 |  |
|        - |   725 | `/* Forward declaration */` |
|        - |   726 | `static sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult);` |
|        - |   727 | `static sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...);` |
|        - |   728 | `/*` |
|        - |   729 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|        - |   730 | ` * it can be instanciated from the executed PHP script.` |
|        - |   731 | ` */` |
|    50686 |   732 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   733 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   734 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   735 | `	)` |
|        2 |   736 |  |
|        - |   737 | `	ph7_class_method *pMeth;` |
|        - |   738 | `	ph7_class_attr *pAttr;` |
|        - |   739 | `	SyHashEntry *pEntry;` |
|        - |   740 | `	sxi32 rc;` |
|        - |   741 | `	/* Reset the loop cursor */` |
|    50688 |   742 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   743 | `	/* Process only static and constant attribute */` |
|   151357 |   744 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   745 | `		/* Extract the current attribute */` |
|    75328 |   746 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|    75328 |   747 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   748 | `			ph7_value *pMemObj;` |
|        - |   749 | `			/* Reserve a memory object for this constant/static attribute */` |
|     1290 |   750 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1290 |   751 | `			if( pMemObj == 0 ){` |
|      ! 0 |   752 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   753 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   754 | `					&pClass->sName,&pAttr->sName` |
|        - |   755 | `					);` |
|      ! 0 |   756 | `				return SXERR_MEM;` |
|        - |   757 | `			}` |
|     1290 |   758 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   759 | `				/* Initialize attribute default value (any complex expression) */` |
|     1290 |   760 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      644 |   761 | `			}` |
|        - |   762 | `			/* Record attribute index */` |
|     1290 |   763 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   764 | `			/* Install static attribute in the reference table */` |
|     1290 |   765 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      644 |   766 | `		}` |
|        2 |   767 | `	}` |
|        - |   768 | `	/* Install class methods */` |
|    50688 |   769 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|        - |   770 | `		/* Do not mount interface methods since they are signatures only.` |
|        - |   771 | `		 */` |
|    35256 |   772 | `		return SXRET_OK;` |
|        - |   773 | `	}` |
|        - |   774 | `	/* Create constructor alias if not yet done */` |
|    15434 |   775 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   776 | `		/* User constructor with the same base class name */` |
|      200 |   777 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|      200 |   778 | `		if( pEntry ){` |
|      ! 0 |   779 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   780 | `			/* Create the alias */` |
|      ! 0 |   781 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   782 | `		}` |
|       99 |   783 | `	}` |
|        - |   784 | `	/* Install the methods now */` |
|    15434 |   785 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   145018 |   786 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   121870 |   787 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   121870 |   788 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   121864 |   789 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   121864 |   790 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   791 | `				return rc;` |
|        - |   792 | `			}` |
|    60931 |   793 | `		}` |
|        2 |   794 | `	}` |
|        - |   795 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    15434 |   796 | `	pClass->bMounted = TRUE;` |
|    15434 |   797 | `	return SXRET_OK;` |
|    25345 |   798 |  |
|        - |   799 | `/*` |
|        - |   800 | ` * Allocate a private frame for attributes of the given` |
|        - |   801 | ` * class instance (Object in the PHP jargon).` |
|        - |   802 | ` */` |
|      566 |   803 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   804 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   805 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   806 | `	)` |
|        2 |   807 |  |
|      568 |   808 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   809 | `	ph7_class_attr *pAttr;` |
|        - |   810 | `	SyHashEntry *pEntry;` |
|        - |   811 | `	sxi32 rc;` |
|        - |   812 | `	/* Install class attribute in the private frame associated with this instance */` |
|      568 |   813 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     1292 |   814 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   815 | `		VmClassAttr *pVmAttr;` |
|        - |   816 | `		/* Extract the current attribute */` |
|      726 |   817 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      726 |   818 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|      726 |   819 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   820 | `			return SXERR_MEM;` |
|        - |   821 | `		}` |
|      726 |   822 | `		pVmAttr->pAttr = pAttr;` |
|      726 |   823 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   824 | `			ph7_value *pMemObj;` |
|        - |   825 | `			/* Reserve a memory object for this attribute */` |
|      720 |   826 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|      720 |   827 | `			if( pMemObj == 0 ){` |
|      ! 0 |   828 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   829 | `				return SXERR_MEM;` |
|        - |   830 | `			}` |
|      720 |   831 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|      720 |   832 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   833 | `				/* Initialize attribute default value (any complex expression) */` |
|      218 |   834 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      108 |   835 | `			}` |
|      720 |   836 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|      720 |   837 | `			if( rc != SXRET_OK ){` |
|        - |   838 | `				VmSlot sSlot;` |
|        - |   839 | `				/* Restore memory object */` |
|      ! 0 |   840 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   841 | `				sSlot.pUserData = 0;` |
|      ! 0 |   842 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   843 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   844 | `				return SXERR_MEM;` |
|        - |   845 | `			}` |
|        - |   846 | `			/* Install attribute in the reference table */` |
|      720 |   847 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      361 |   848 | `		}else{` |
|        - |   849 | `			/* Install static/constant attribute */` |
|        8 |   850 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|        8 |   851 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|        8 |   852 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   853 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   854 | `				return SXERR_MEM;` |
|        - |   855 | `			}` |
|        - |   856 | `		}` |
|        2 |   857 | `	}` |
|      568 |   858 | `	return SXRET_OK;` |
|      285 |   859 |  |
|        - |   860 | `/* Forward declaration */` |
|        - |   861 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx);` |
|        - |   862 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef);` |
|        - |   863 | `/*` |
|        - |   864 | ` * Dummy read-only buffer used for slot reservation.` |
|        - |   865 | ` */` |
|        - |   866 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|        - |   867 | `/*` |
|        - |   868 | ` * Reserve a constant memory object.` |
|        - |   869 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   870 | ` */` |
|   149492 |   871 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   872 |  |
|        - |   873 | `	ph7_value *pObj;` |
|        - |   874 | `	sxi32 rc;` |
|   149494 |   875 | `	if( pIndex ){` |
|        - |   876 | `		/* Object index in the object table */` |
|   145846 |   877 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|    72922 |   878 | `	}` |
|        - |   879 | `	/* Reserve a slot for the new object */` |
|   149494 |   880 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   149494 |   881 | `	if( rc != SXRET_OK ){` |
|        - |   882 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   883 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   884 | `		 */` |
|      ! 0 |   885 | `		return 0;` |
|        - |   886 | `	}` |
|   149494 |   887 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   149494 |   888 | `	return pObj;` |
|    74748 |   889 |  |
|        - |   890 | `/*` |
|        - |   891 | ` * Reserve a memory object.` |
|        - |   892 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   893 | ` */` |
|    76182 |   894 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   895 |  |
|        - |   896 | `	ph7_value *pObj;` |
|        - |   897 | `	sxi32 rc;` |
|    76184 |   898 | `	if( pIndex ){` |
|        - |   899 | `		/* Object index in the object table */` |
|    76184 |   900 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|    38091 |   901 | `	}` |
|        - |   902 | `	/* Reserve a slot for the new object */` |
|    76184 |   903 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|    76184 |   904 | `	if( rc != SXRET_OK ){` |
|        - |   905 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   906 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   907 | `		 */` |
|      ! 0 |   908 | `		return 0;` |
|        - |   909 | `	}` |
|    76184 |   910 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|    76184 |   911 | `	return pObj;` |
|    38093 |   912 |  |
|        - |   913 | `/* Forward declaration */` |
|        - |   914 | `static sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);` |
|        - |   915 | `/*` |
|        - |   916 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |   917 | ` * directly as foreign functions.` |
|        - |   918 | ` */` |
|        - |   919 | `#define PH7_BUILTIN_LIB \` |
|        - |   920 | `	"class Exception { "\` |
|        - |   921 | `    "protected $message = 'Unknown exception';"\` |
|        - |   922 | `    "protected $code = 0;"\` |
|        - |   923 | `    "protected $file;"\` |
|        - |   924 | `    "protected $line;"\` |
|        - |   925 | `    "protected $trace;"\` |
|        - |   926 | `    "protected $previous;"\` |
|        - |   927 | `	"public function __construct($message = null, $code = 0, Exception $previous = null){"\` |
|        - |   928 | `	"   if( isset($message) ){"\` |
|        - |   929 | `	"	  $this->message = $message;"\` |
|        - |   930 | `	"   }"\` |
|        - |   931 | `	"   $this->code = $code;"\` |
|        - |   932 | `	"   $this->file = __FILE__;"\` |
|        - |   933 | `	"   $this->line = __LINE__;"\` |
|        - |   934 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   935 | `	"   if( isset($previous) ){"\` |
|        - |   936 | `	"     $this->previous = $previous;"\` |
|        - |   937 | `	"   }"\` |
|        - |   938 | `	"}"\` |
|        - |   939 | `	"public function getMessage(){"\` |
|        - |   940 | `	"   return $this->message;"\` |
|        - |   941 | `	"}"\` |
|        - |   942 | `	" public function getCode(){"\` |
|        - |   943 | `	"  return $this->code;"\` |
|        - |   944 | `	"}"\` |
|        - |   945 | `	"public function getFile(){"\` |
|        - |   946 | `	"  return $this->file;"\` |
|        - |   947 | `	"}"\` |
|        - |   948 | `	"public function getLine(){"\` |
|        - |   949 | `	"  return $this->line;"\` |
|        - |   950 | `	"}"\` |
|        - |   951 | `	"public function getTrace(){"\` |
|        - |   952 | `	"   return $this->trace;"\` |
|        - |   953 | `	"}"\` |
|        - |   954 | `	"public function getTraceAsString(){"\` |
|        - |   955 | `	"  return debug_string_backtrace();"\` |
|        - |   956 | `	"}"\` |
|        - |   957 | `	"public function getPrevious(){"\` |
|        - |   958 | `	"    return $this->previous;"\` |
|        - |   959 | `	"}"\` |
|        - |   960 | `	"public function __toString(){"\` |
|        - |   961 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |   962 | `    "}"\` |
|        - |   963 | `	"}"\` |
|        - |   964 | `	"class Error extends Exception { }"\` |
|        - |   965 | `	"class TypeError extends Error { }"\` |
|        - |   966 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |   967 | `	"class ErrorException extends Exception { "\` |
|        - |   968 | `	"protected $severity;"\` |
|        - |   969 | `	"public function __construct(string $message = null,"\` |
|        - |   970 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Exception $previous = null){"\` |
|        - |   971 | `	"   if( isset($message) ){"\` |
|        - |   972 | `	"	  $this->message = $message;"\` |
|        - |   973 | `	"   }"\` |
|        - |   974 | `	"   $this->severity = $severity;"\` |
|        - |   975 | `	"   $this->code = $code;"\` |
|        - |   976 | `	"   $this->file = $filename;"\` |
|        - |   977 | `	"   $this->line = $lineno;"\` |
|        - |   978 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   979 | `	"   if( isset($previous) ){"\` |
|        - |   980 | `	"     $this->previous = $previous;"\` |
|        - |   981 | `	"   }"\` |
|        - |   982 | `	"}"\` |
|        - |   983 | `	"public function getSeverity(){"\` |
|        - |   984 | `	"   return $this->severity;"\` |
|        - |   985 | `    "}"\` |
|        - |   986 | `	"}"\` |
|        - |   987 | `	"interface Iterator {"\` |
|        - |   988 | `	"public function current();"\` |
|        - |   989 | `	"public function key();"\` |
|        - |   990 | `	"public function next();"\` |
|        - |   991 | `	"public function rewind();"\` |
|        - |   992 | `	"public function valid();"\` |
|        - |   993 | `	"}"\` |
|        - |   994 | `	"interface IteratorAggregate {"\` |
|        - |   995 | `	"public function getIterator();"\` |
|        - |   996 | `	"}"\` |
|        - |   997 | `	"interface Serializable {"\` |
|        - |   998 | `	"public function serialize();"\` |
|        - |   999 | `	"public function unserialize(string $serialized);"\` |
|        - |  1000 | `	"}"\` |
|        - |  1001 | `	"/* Directory releated IO */"\` |
|        - |  1002 | `	"class Directory {"\` |
|        - |  1003 | `	"public $handle = null;"\` |
|        - |  1004 | `	"public $path  = null;"\` |
|        - |  1005 | `	"public function __construct(string $path)"\` |
|        - |  1006 | `	"{"\` |
|        - |  1007 | `	"   $this->handle = opendir($path);"\` |
|        - |  1008 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |  1009 | `	"      $this->path = $path;"\` |
|        - |  1010 | `	"   }"\` |
|        - |  1011 | `	"}"\` |
|        - |  1012 | `	"public function __destruct()"\` |
|        - |  1013 | `	"{"\` |
|        - |  1014 | `	"  if( $this->handle != null ){"\` |
|        - |  1015 | `	"       closedir($this->handle);"\` |
|        - |  1016 | `	"  }"\` |
|        - |  1017 | `	"}"\` |
|        - |  1018 | `	"public function read()"\` |
|        - |  1019 | `	"{"\` |
|        - |  1020 | `	"    return readdir($this->handle);"\` |
|        - |  1021 | `	"}"\` |
|        - |  1022 | `	"public function rewind()"\` |
|        - |  1023 | `	"{"\` |
|        - |  1024 | `	"    rewinddir($this->handle);"\` |
|        - |  1025 | `	"}"\` |
|        - |  1026 | `	"public function close()"\` |
|        - |  1027 | `	"{"\` |
|        - |  1028 | `	"    closedir($this->handle);"\` |
|        - |  1029 | `	"    $this->handle = null;"\` |
|        - |  1030 | `	"}"\` |
|        - |  1031 | `	"}"\` |
|        - |  1032 | `	"class stdClass{"\` |
|        - |  1033 | `	"  public $value;"\` |
|        - |  1034 | `	" /* Magic methods */"\` |
|        - |  1035 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1036 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1037 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1038 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1039 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1040 | `	"}"\` |
|        - |  1041 | `	"function dir(string $path){"\` |
|        - |  1042 | `	"   return new Directory($path);"\` |
|        - |  1043 | `	"}"\` |
|        - |  1044 | `	"function Dir(string $path){"\` |
|        - |  1045 | `	"   return new Directory($path);"\` |
|        - |  1046 | `	"}"\` |
|        - |  1047 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1048 | `    "{"\` |
|        - |  1049 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1050 | `	"  $aDir = array();"\` |
|        - |  1051 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1052 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1053 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1054 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1055 | `	"   }"\` |
|        - |  1056 | `	"  closedir($pHandle);"\` |
|        - |  1057 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1058 | `	"      rsort($aDir);"\` |
|        - |  1059 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1060 | `	"      sort($aDir);"\` |
|        - |  1061 | `	"  }"\` |
|        - |  1062 | `	"  return $aDir;"\` |
|        - |  1063 | `	"}"\` |
|        - |  1064 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1065 | `	"/* Open the target directory */"\` |
|        - |  1066 | `	"$zDir = dirname($pattern);"\` |
|        - |  1067 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1068 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1069 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1070 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1071 | `	"	return FALSE;"\` |
|        - |  1072 | `	"}"\` |
|        - |  1073 | `	"$pattern = basename($pattern);"\` |
|        - |  1074 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1075 | `	"/* Loop throw available entries */"\` |
|        - |  1076 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1077 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1078 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1079 | `	"	if( $rc ){"\` |
|        - |  1080 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1081 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1082 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1083 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1084 | `	"		  }"\` |
|        - |  1085 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1086 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1087 | `	"		 continue;"\` |
|        - |  1088 | `	"	   }"\` |
|        - |  1089 | `	"	   /* Add the entry */"\` |
|        - |  1090 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1091 | `	"	}"\` |
|        - |  1092 | `	" }"\` |
|        - |  1093 | `	"/* Close the handle */"\` |
|        - |  1094 | `	"closedir($pHandle);"\` |
|        - |  1095 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1096 | `	"  /* Sort the array */"\` |
|        - |  1097 | `	"  sort($pArray);"\` |
|        - |  1098 | `	"}"\` |
|        - |  1099 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1100 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1101 | `	"  $pArray[] = $pattern;"\` |
|        - |  1102 | `	"}"\` |
|        - |  1103 | `	"/* Return the created array */"\` |
|        - |  1104 | `	"return $pArray;"\` |
|        - |  1105 | `   "}"\` |
|        - |  1106 | `   "/* Creates a temporary file */"\` |
|        - |  1107 | `   "function tmpfile(){"\` |
|        - |  1108 | `   "  /* Extract the temp directory */"\` |
|        - |  1109 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1110 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1111 | `   "    /* Use the current dir */"\` |
|        - |  1112 | `   "    $zTempDir = '.';"\` |
|        - |  1113 | `   "  }"\` |
|        - |  1114 | `   "  /* Create the file */"\` |
|        - |  1115 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1116 | `   "  return $pHandle;"\` |
|        - |  1117 | `   "}"\` |
|        - |  1118 | `   "/* Creates a temporary filename */"\` |
|        - |  1119 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1120 | `   "{"\` |
|        - |  1121 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1122 | `   "}"\` |
|        - |  1123 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1124 | `   " if( func_num_args() < 1 \|\| !is_array($pArray) ){  return 0; }"\` |
|        - |  1125 | `   "/* Copy arguments */"\` |
|        - |  1126 | `   "$nArgs = func_num_args();"\` |
|        - |  1127 | `   "$pNew = array();"\` |
|        - |  1128 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1129 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1130 | `    "}"\` |
|        - |  1131 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1132 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1133 | `	"/* Erase */"\` |
|        - |  1134 | `	"array_erase($pArray);"\` |
|        - |  1135 | `	"/* Unshift */"\` |
|        - |  1136 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1137 | `	"return sizeof($pArray);"\` |
|        - |  1138 | `    "}"\` |
|        - |  1139 | `	"function array_merge_recursive($array1, $array2){"\` |
|        - |  1140 | `	"if( func_num_args() < 1 ){ return NULL; }"\` |
|        - |  1141 | `    "$arrays = func_get_args();"\` |
|        - |  1142 | `    "$narrays = count($arrays);"\` |
|        - |  1143 | `    "$ret = $arrays[0];"\` |
|        - |  1144 | `    "for ($i = 1; $i < $narrays; $i++) {"\` |
|        - |  1145 | `	 " if( array_same($ret,$arrays[$i]) ){ /* Same instance */continue;}"\` |
|        - |  1146 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1147 | `     "  if (((string) $key) === ((string) intval($key))) {"\` |
|        - |  1148 | `     "   $ret[] = $value;"\` |
|        - |  1149 | `     "  }else{"\` |
|        - |  1150 | `     "  if (is_array($value) && isset($ret[$key]) ) {"\` |
|        - |  1151 | `     "   $ret[$key] = array_merge_recursive($ret[$key], $value);"\` |
|        - |  1152 | `     " }else {"\` |
|        - |  1153 | `     "   $ret[$key] = $value;"\` |
|        - |  1154 | `     "  }"\` |
|        - |  1155 | `     " }"\` |
|        - |  1156 | `     " }"\` |
|        - |  1157 | `	 "}"\` |
|        - |  1158 | `	 " return $ret;"\` |
|        - |  1159 | `    "}"\` |
|        - |  1160 | `	"function max(){"\` |
|        - |  1161 | `    "  $pArgs = func_get_args();"\` |
|        - |  1162 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1163 | `	"  return null;"\` |
|        - |  1164 | `    " }"\` |
|        - |  1165 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1166 | `    " $pArg = $pArgs[0];"\` |
|        - |  1167 | `	" if( !is_array($pArg) ){"\` |
|        - |  1168 | `	"   return $pArg; "\` |
|        - |  1169 | `	" }"\` |
|        - |  1170 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1171 | `	"   return null;"\` |
|        - |  1172 | `	" }"\` |
|        - |  1173 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1174 | `	" reset($pArg);"\` |
|        - |  1175 | `	" $max = current($pArg);"\` |
|        - |  1176 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1177 | `	"   if( $val > $max ){"\` |
|        - |  1178 | `	"     $max = $val;"\` |
|        - |  1179 | `    " }"\` |
|        - |  1180 | `	" }"\` |
|        - |  1181 | `	" return $max;"\` |
|        - |  1182 | `    " }"\` |
|        - |  1183 | `    " $max = $pArgs[0];"\` |
|        - |  1184 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1185 | `    " $val = $pArgs[$i];"\` |
|        - |  1186 | `	"if( $val > $max ){"\` |
|        - |  1187 | `	" $max = $val;"\` |
|        - |  1188 | `	"}"\` |
|        - |  1189 | `    " }"\` |
|        - |  1190 | `	" return $max;"\` |
|        - |  1191 | `    "}"\` |
|        - |  1192 | `	"function min(){"\` |
|        - |  1193 | `    "  $pArgs = func_get_args();"\` |
|        - |  1194 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1195 | `	"  return null;"\` |
|        - |  1196 | `    " }"\` |
|        - |  1197 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1198 | `    " $pArg = $pArgs[0];"\` |
|        - |  1199 | `	" if( !is_array($pArg) ){"\` |
|        - |  1200 | `	"   return $pArg; "\` |
|        - |  1201 | `	" }"\` |
|        - |  1202 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1203 | `	"   return null;"\` |
|        - |  1204 | `	" }"\` |
|        - |  1205 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1206 | `	" reset($pArg);"\` |
|        - |  1207 | `	" $min = current($pArg);"\` |
|        - |  1208 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1209 | `	"   if( $val < $min ){"\` |
|        - |  1210 | `	"     $min = $val;"\` |
|        - |  1211 | `    " }"\` |
|        - |  1212 | `	" }"\` |
|        - |  1213 | `	" return $min;"\` |
|        - |  1214 | `    " }"\` |
|        - |  1215 | `    " $min = $pArgs[0];"\` |
|        - |  1216 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1217 | `    " $val = $pArgs[$i];"\` |
|        - |  1218 | `	"if( $val < $min ){"\` |
|        - |  1219 | `	" $min = $val;"\` |
|        - |  1220 | `	" }"\` |
|        - |  1221 | `    " }"\` |
|        - |  1222 | `	" return $min;"\` |
|        - |  1223 | `	"}"\` |
|        - |  1224 | `	"function fileowner(string $file){"\` |
|        - |  1225 | `    " $a = stat($file);"\` |
|        - |  1226 | `	" if( !is_array($a) ){"\` |
|        - |  1227 | `	"	return false;"\` |
|        - |  1228 | `	" }"\` |
|        - |  1229 | `	" return $a['uid'];"\` |
|        - |  1230 | `    "}"\` |
|        - |  1231 | `    "function filegroup(string $file){"\` |
|        - |  1232 | `	" $a = stat($file);"\` |
|        - |  1233 | `	" if( !is_array($a) ){"\` |
|        - |  1234 | `	"	return false;"\` |
|        - |  1235 | `	" }"\` |
|        - |  1236 | `	" return $a['gid'];"\` |
|        - |  1237 | `    "}"\` |
|        - |  1238 | `	 "function fileinode(string $file){"\` |
|        - |  1239 | `	" $a = stat($file);"\` |
|        - |  1240 | `	" if( !is_array($a) ){"\` |
|        - |  1241 | `	"	return false;"\` |
|        - |  1242 | `	" }"\` |
|        - |  1243 | `	" return $a['ino'];"\` |
|        - |  1244 | `    "}"` |
|        - |  1245 |  |
|        - |  1246 | `/*` |
|        - |  1247 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1248 | ` * start compiling the target PHP program.` |
|        - |  1249 | ` */` |
|     1216 |  1250 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1251 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1252 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1253 | `	 )` |
|        2 |  1254 |  |
|        - |  1255 | `	SyString sBuiltin;` |
|        - |  1256 | `	ph7_value *pObj;` |
|        - |  1257 | `	sxi32 rc;` |
|        - |  1258 | `	/* Zero the structure */` |
|     1218 |  1259 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1260 | `	/* Initialize VM fields */` |
|     1218 |  1261 | `	pVm->pEngine = &(*pEngine);` |
|     1218 |  1262 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1263 | `	/* Instructions containers */` |
|     1218 |  1264 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     1218 |  1265 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     1218 |  1266 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1267 | `	/* Object containers */` |
|     1218 |  1268 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     1218 |  1269 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1270 | `	/* Virtual machine internal containers */` |
|     1218 |  1271 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     1218 |  1272 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     1218 |  1273 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     1218 |  1274 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     1218 |  1275 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     1218 |  1276 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     1218 |  1277 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     1218 |  1278 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     1218 |  1279 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     1218 |  1280 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     1218 |  1281 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     1218 |  1282 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     1218 |  1283 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     1218 |  1284 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     1218 |  1285 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|        - |  1286 | `	/* Configuration containers */` |
|     1218 |  1287 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     1218 |  1288 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     1218 |  1289 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     1218 |  1290 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     1218 |  1291 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1292 | `	/* Error callbacks containers */` |
|     1218 |  1293 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     1218 |  1294 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     1218 |  1295 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     1218 |  1296 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     1218 |  1297 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1298 | `	/* Set a default recursion limit */` |
|        - |  1299 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     1218 |  1300 | `	pVm->nMaxDepth = 32;` |
|        - |  1301 | `#else` |
|        - |  1302 | `	pVm->nMaxDepth = 16;` |
|        - |  1303 | `#endif` |
|        - |  1304 | `	/* Default assertion flags */` |
|     1218 |  1305 | `	pVm->iAssertFlags = PH7_ASSERT_WARNING; /* Issue a warning for each failed assertion */` |
|        - |  1306 | `	/* JSON return status */` |
|     1218 |  1307 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1308 | `	/* PRNG context */` |
|     1218 |  1309 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1310 | `	/* Install the null constant */` |
|     1218 |  1311 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     1218 |  1312 | `	if( pObj == 0 ){` |
|      ! 0 |  1313 | `		rc = SXERR_MEM;` |
|      ! 0 |  1314 | `		goto Err;` |
|        - |  1315 | `	}` |
|     1218 |  1316 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1317 | `	/* Install the boolean TRUE constant */` |
|     1218 |  1318 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     1218 |  1319 | `	if( pObj == 0 ){` |
|      ! 0 |  1320 | `		rc = SXERR_MEM;` |
|      ! 0 |  1321 | `		goto Err;` |
|        - |  1322 | `	}` |
|     1218 |  1323 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1324 | `	/* Install the boolean FALSE constant */` |
|     1218 |  1325 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     1218 |  1326 | `	if( pObj == 0 ){` |
|      ! 0 |  1327 | `		rc = SXERR_MEM;` |
|      ! 0 |  1328 | `		goto Err;` |
|        - |  1329 | `	}` |
|     1218 |  1330 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1331 | `	/* Create the global frame */` |
|     1218 |  1332 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     1218 |  1333 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1334 | `		goto Err;` |
|        - |  1335 | `	}` |
|        - |  1336 | `	/* Initialize the code generator */` |
|     1218 |  1337 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     1218 |  1338 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1339 | `		goto Err;` |
|        - |  1340 | `	}` |
|        - |  1341 | `	/* VM correctly initialized,set the magic number */` |
|     1218 |  1342 | `	pVm->nMagic = PH7_VM_INIT;` |
|     1218 |  1343 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1344 | `	/* Compile the built-in library */` |
|     1218 |  1345 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1346 | `	/* Reset the code generator */` |
|     1218 |  1347 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     1218 |  1348 | `	return SXRET_OK;` |
|      ! 0 |  1349 | `Err:` |
|      ! 0 |  1350 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1351 | `	return rc;` |
|      610 |  1352 |  |
|        - |  1353 | `/*` |
|        - |  1354 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1355 | ` * routine which store the output in an internal blob.` |
|        - |  1356 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1357 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1358 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1359 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1360 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1361 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1362 | ` * to finish executing and extracting the output.` |
|        - |  1363 | ` */` |
|      ! 0 |  1364 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1365 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1366 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1367 | `	void *pUserData     /* User private data */` |
|        - |  1368 | `	)` |
|      ! 0 |  1369 |  |
|        - |  1370 | `	 sxi32 rc;` |
|        - |  1371 | `	 /* Store the output in an internal BLOB */` |
|      ! 0 |  1372 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|      ! 0 |  1373 | `	 return rc;` |
|      ! 0 |  1374 |  |
|        - |  1375 | `#define VM_STACK_GUARD 16` |
|        - |  1376 | `/*` |
|        - |  1377 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1378 | ` * our compiled PHP program.` |
|        - |  1379 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1380 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1381 | ` */` |
|    20090 |  1382 | `static ph7_value * VmNewOperandStack(` |
|        - |  1383 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1384 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1385 | `	)` |
|        2 |  1386 |  |
|        - |  1387 | `	ph7_value *pStack;` |
|        - |  1388 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1389 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1390 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1391 | `  ** on the maximum stack depth required.` |
|        - |  1392 | `  **` |
|        - |  1393 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1394 | `  */` |
|    20092 |  1395 | `	nInstr += VM_STACK_GUARD;` |
|    20092 |  1396 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    20092 |  1397 | `	if( pStack == 0 ){` |
|      ! 0 |  1398 | `		return 0;` |
|        - |  1399 | `	}` |
|        - |  1400 | `	/* Initialize the operand stack */` |
|  1255384 |  1401 | `	while( nInstr > 0 ){` |
|  1235294 |  1402 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  1235294 |  1403 | `		--nInstr;` |
|        2 |  1404 | `	}` |
|        - |  1405 | `	/* Ready for bytecode execution */` |
|    20092 |  1406 | `	return pStack;` |
|    10047 |  1407 |  |
|        - |  1408 | `/* Forward declaration */` |
|        - |  1409 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1410 | `static int VmInstanceOf(ph7_class *pThis,ph7_class *pClass);` |
|        - |  1411 | `static int VmClassMemberAccess(ph7_vm *pVm,ph7_class *pClass,const SyString *pAttrName,sxi32 iProtection,int bLog);` |
|        - |  1412 | `/*` |
|        - |  1413 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1414 | ` * This routine gets called by the PH7 engine after` |
|        - |  1415 | ` * successful compilation of the target PHP program.` |
|        - |  1416 | ` */` |
|      956 |  1417 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1418 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1419 | `	)` |
|        2 |  1420 |  |
|        - |  1421 | `	SyHashEntry *pEntry;` |
|        - |  1422 | `	sxi32 rc;` |
|      958 |  1423 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1424 | `		/* Initialize your VM first */` |
|      ! 0 |  1425 | `		return SXERR_CORRUPT;` |
|        - |  1426 | `	}` |
|        - |  1427 | `	/* Mark the VM ready for byte-code execution */` |
|      958 |  1428 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1429 | `	/* Release the code generator now we have compiled our program */` |
|      958 |  1430 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1431 | `	/* Emit the DONE instruction */` |
|      958 |  1432 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|      958 |  1433 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1434 | `		return SXERR_MEM;` |
|        - |  1435 | `	}` |
|        - |  1436 | `	/* Script return value */` |
|      958 |  1437 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1438 | `	/* Allocate a new operand stack */` |
|      958 |  1439 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|      958 |  1440 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1441 | `		return SXERR_MEM;` |
|        - |  1442 | `	}` |
|        - |  1443 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1444 | `	 * private data. */` |
|      958 |  1445 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|      958 |  1446 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1447 | `	/* Allocate the reference table */` |
|      958 |  1448 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|      958 |  1449 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|      958 |  1450 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1451 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1452 | `		return SXERR_MEM;` |
|        - |  1453 | `	}` |
|        - |  1454 | `	/* Zero the reference table */` |
|      958 |  1455 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1456 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|      958 |  1457 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|      958 |  1458 | `	if( rc != SXRET_OK ){` |
|        - |  1459 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1460 | `		return rc;` |
|        - |  1461 | `	}` |
|        - |  1462 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|      958 |  1463 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|      958 |  1464 | `	if( rc != SXRET_OK ){` |
|        - |  1465 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1466 | `		return rc;` |
|        - |  1467 | `	}` |
|        - |  1468 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|      958 |  1469 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1470 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|      958 |  1471 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1472 | `	/* Initialize and install static and constants class attributes */` |
|      958 |  1473 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    10540 |  1474 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|     9584 |  1475 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|     9584 |  1476 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1477 | `			return rc;` |
|        - |  1478 | `		}` |
|        2 |  1479 | `	}` |
|        - |  1480 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|      958 |  1481 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1482 | `	/* VM is ready for bytecode execution */` |
|      958 |  1483 | `	return SXRET_OK;` |
|      480 |  1484 |  |
|        - |  1485 | `/*` |
|        - |  1486 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1487 | ` */` |
|      ! 0 |  1488 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1489 |  |
|      ! 0 |  1490 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1491 | `		return SXERR_CORRUPT;` |
|        - |  1492 | `	}` |
|        - |  1493 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1494 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1495 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1496 | `	/* Set the ready flag */` |
|      ! 0 |  1497 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1498 | `	return SXRET_OK;` |
|      ! 0 |  1499 |  |
|        - |  1500 | `/*` |
|        - |  1501 | ` * Release a Virtual Machine.` |
|        - |  1502 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1503 | ` */` |
|      948 |  1504 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1505 |  |
|        - |  1506 | `	/* Set the stale magic number */` |
|      950 |  1507 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1508 | `	/* Release the private memory subsystem */` |
|      950 |  1509 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      950 |  1510 | `	return SXRET_OK;` |
|        2 |  1511 |  |
|        - |  1512 | `/*` |
|        - |  1513 | ` * Initialize a foreign function call context.` |
|        - |  1514 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1515 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1516 | ` * functions.` |
|        - |  1517 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1518 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1519 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1520 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1521 | ` */` |
|   406100 |  1522 | `static sxi32 VmInitCallContext(` |
|        - |  1523 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1524 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1525 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1526 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1527 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1528 | `	)` |
|        2 |  1529 |  |
|   406102 |  1530 | `	pOut->pFunc = pFunc;` |
|   406102 |  1531 | `	pOut->pVm   = pVm;` |
|   406102 |  1532 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   406102 |  1533 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1534 | `	/* Assume a null return value */` |
|   406102 |  1535 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   406102 |  1536 | `	pOut->pRet = pRet;` |
|   406102 |  1537 | `	pOut->iFlags = iFlags;` |
|   406102 |  1538 | `	return SXRET_OK;` |
|        2 |  1539 |  |
|        - |  1540 | `/*` |
|        - |  1541 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1542 | ` * left behind.` |
|        - |  1543 | ` */` |
|   406100 |  1544 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1545 |  |
|        - |  1546 | `	sxu32 n;` |
|   406102 |  1547 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     4448 |  1548 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    12358 |  1549 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     7912 |  1550 | `			if( apObj[n] == 0 ){` |
|        - |  1551 | `				/* Already released */` |
|      250 |  1552 | `				continue;` |
|        - |  1553 | `			}` |
|     7664 |  1554 | `			PH7_MemObjRelease(apObj[n]);` |
|     7664 |  1555 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     3833 |  1556 | `		}` |
|     4448 |  1557 | `		SySetRelease(&pCtx->sVar);` |
|     2223 |  1558 | `	}` |
|   406102 |  1559 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1560 | `		ph7_aux_data *aAux;` |
|        - |  1561 | `		void *pChunk;` |
|        - |  1562 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1563 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1564 | `		 */` |
|        9 |  1565 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1566 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1567 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1568 | `			/* Release the chunk */` |
|       25 |  1569 | `			if( pChunk ){` |
|       25 |  1570 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1571 | `			}` |
|       13 |  1572 | `		}` |
|        9 |  1573 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1574 | `	}` |
|   406102 |  1575 |  |
|        - |  1576 | `/*` |
|        - |  1577 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1578 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1579 | ` */` |
|      248 |  1580 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1581 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1582 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1583 | `	)` |
|        2 |  1584 |  |
|      250 |  1585 | `	if( pValue == 0 ){` |
|        - |  1586 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1587 | `		return;` |
|        - |  1588 | `	}` |
|      250 |  1589 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      250 |  1590 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1591 | `		sxu32 n;` |
|      936 |  1592 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|      936 |  1593 | `			if( apObj[n] == pValue ){` |
|      250 |  1594 | `				PH7_MemObjRelease(pValue);` |
|      250 |  1595 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1596 | `				/* Mark as released */` |
|      250 |  1597 | `				apObj[n] = 0;` |
|      250 |  1598 | `				break;` |
|        - |  1599 | `			}` |
|      345 |  1600 | `		}` |
|      124 |  1601 | `	}` |
|      126 |  1602 |  |
|        - |  1603 | `/*` |
|        - |  1604 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1605 | ` */` |
|  2218174 |  1606 | `static void VmPopOperand(` |
|        - |  1607 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1608 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1609 | `	)` |
|        2 |  1610 |  |
|  2218176 |  1611 | `	ph7_value *pTos = *ppTos;` |
|  4745162 |  1612 | `	while( nPop > 0 ){` |
|  2526988 |  1613 | `		PH7_MemObjRelease(pTos);` |
|  2526988 |  1614 | `		pTos--;` |
|  2526988 |  1615 | `		nPop--;` |
|        2 |  1616 | `	}` |
|        - |  1617 | `	/* Top of the stack */` |
|  2218176 |  1618 | `	*ppTos = pTos;` |
|  2218176 |  1619 |  |
|        - |  1620 | `/*` |
|        - |  1621 | ` * Reserve a memory object.` |
|        - |  1622 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1623 | ` */` |
|   611956 |  1624 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1625 |  |
|   611958 |  1626 | `	ph7_value *pObj = 0;` |
|        - |  1627 | `	VmSlot *pSlot;` |
|        - |  1628 | `	sxu32 nIdx;` |
|        - |  1629 | `	/* Check for a free slot */` |
|   611958 |  1630 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|   611958 |  1631 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|   611958 |  1632 | `	if( pSlot ){` |
|   535776 |  1633 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   535776 |  1634 | `		nIdx = pSlot->nIdx;` |
|   267887 |  1635 | `	}` |
|   611958 |  1636 | `	if( pObj == 0 ){` |
|        - |  1637 | `		/* Reserve a new memory object */` |
|    76184 |  1638 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|    76184 |  1639 | `		if( pObj == 0 ){` |
|      ! 0 |  1640 | `			return 0;` |
|        - |  1641 | `		}` |
|    38091 |  1642 | `	}` |
|        - |  1643 | `	/* Set a null default value */` |
|   611958 |  1644 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|   611958 |  1645 | `	pObj->nIdx = nIdx;` |
|   611958 |  1646 | `	return pObj;` |
|   305980 |  1647 |  |
|        - |  1648 | `/*` |
|        - |  1649 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1650 | ` */` |
|    14174 |  1651 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1652 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1653 | `	const char *zKey,  /* Entry key */` |
|        - |  1654 | `	sxu32 nByte,       /* Key length */` |
|        - |  1655 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1656 | `	)` |
|        2 |  1657 |  |
|        - |  1658 | `	ph7_value sKey;` |
|        - |  1659 | `	sxi32 rc;` |
|    14176 |  1660 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    14176 |  1661 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1662 | `	/* Perform the insertion */` |
|    14176 |  1663 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    14176 |  1664 | `	PH7_MemObjRelease(&sKey);` |
|    14176 |  1665 | `	return rc;` |
|        2 |  1666 |  |
|        - |  1667 | `/*` |
|        - |  1668 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1669 | ` * Return a pointer to the variable value on success.` |
|        - |  1670 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1671 | ` */` |
|  2009750 |  1672 | `static ph7_value * VmExtractMemObj(` |
|        - |  1673 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1674 | `	const SyString *pName, /* Variable name */` |
|        - |  1675 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1676 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1677 | `	)` |
|        2 |  1678 |  |
|  2009752 |  1679 | `	int bNullify = FALSE;` |
|        - |  1680 | `	SyHashEntry *pEntry;` |
|        - |  1681 | `	VmFrame *pFrame;` |
|        - |  1682 | `	ph7_value *pObj;` |
|        - |  1683 | `	sxu32 nIdx;` |
|        - |  1684 | `	sxi32 rc;` |
|        - |  1685 | `	/* Point to the top active frame */` |
|  2009752 |  1686 | `	pFrame = pVm->pFrame;` |
|  2060064 |  1687 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  1688 | `		/* Safely ignore the exception frame */` |
|    50313 |  1689 | `		pFrame = pFrame->pParent; /* Parent frame */` |
|        1 |  1690 | `	}` |
|        - |  1691 | `	/* Perform the lookup */` |
|  2009752 |  1692 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1693 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1694 | `		pName = &sAnnon;` |
|        - |  1695 | `		/* Always nullify the object */` |
|      ! 0 |  1696 | `		bNullify = TRUE;` |
|      ! 0 |  1697 | `		bDup = FALSE;` |
|      ! 0 |  1698 | `	}` |
|        - |  1699 | `	/* Check the superglobals table first */` |
|  2009752 |  1700 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  2009752 |  1701 | `	if( pEntry == 0 ){` |
|        - |  1702 | `		/* Query the top active frame */` |
|  2009716 |  1703 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  2009716 |  1704 | `		if( pEntry == 0 ){` |
|    54968 |  1705 | `			char *zName = (char *)pName->zString;` |
|        - |  1706 | `			VmSlot sLocal;` |
|    54968 |  1707 | `			if( !bCreate ){` |
|        - |  1708 | `				/* Do not create the variable,return NULL instead */` |
|      466 |  1709 | `				return 0;` |
|        - |  1710 | `			}` |
|        - |  1711 | `			/* No such variable,automatically create a new one and install` |
|        - |  1712 | `			 * it in the current frame.` |
|        - |  1713 | `			 */` |
|    54504 |  1714 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    54504 |  1715 | `			if( pObj == 0 ){` |
|      ! 0 |  1716 | `				return 0;` |
|        - |  1717 | `			}` |
|    54504 |  1718 | `			nIdx = pObj->nIdx;` |
|    54504 |  1719 | `			if( bDup ){` |
|        - |  1720 | `				/* Duplicate name */` |
|      115 |  1721 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      115 |  1722 | `				if( zName == 0 ){` |
|      ! 0 |  1723 | `					return 0;` |
|        - |  1724 | `				}` |
|       57 |  1725 | `			}` |
|        - |  1726 | `			/* Link to the top active VM frame */` |
|    54504 |  1727 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    54504 |  1728 | `			if( rc != SXRET_OK ){` |
|        - |  1729 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1730 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1731 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1732 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1733 | `				return 0;` |
|        - |  1734 | `			}` |
|    54504 |  1735 | `			if( pFrame->pParent != 0 ){` |
|        - |  1736 | `				/* Local variable */` |
|    49914 |  1737 | `				sLocal.nIdx = nIdx;` |
|    49914 |  1738 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    24958 |  1739 | `			}else{` |
|        - |  1740 | `				/* Register in the $GLOBALS array */` |
|     4592 |  1741 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1742 | `			}` |
|        - |  1743 | `			/* Install in the reference table */` |
|    54504 |  1744 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1745 | `			/* Save object index */` |
|    54504 |  1746 | `			pObj->nIdx = nIdx;` |
|    27253 |  1747 | `		}else{` |
|        - |  1748 | `			/* Extract variable contents */` |
|  1954750 |  1749 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  1954750 |  1750 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  1954750 |  1751 | `			if( bNullify && pObj ){` |
|      ! 0 |  1752 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1753 | `			}` |
|        - |  1754 | `		}` |
|  1004737 |  1755 | `	}else{` |
|        - |  1756 | `		/* Superglobal */` |
|       38 |  1757 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       38 |  1758 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1759 | `	}` |
|  2009288 |  1760 | `	return pObj;` |
|  1004987 |  1761 |  |
|        - |  1762 | `/*` |
|        - |  1763 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1764 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1765 | ` */` |
|      974 |  1766 | `static ph7_value * VmExtractSuper(` |
|        - |  1767 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1768 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1769 | `	sxu32 nByte        /* zName length */` |
|        - |  1770 | `	)` |
|        2 |  1771 |  |
|        - |  1772 | `	SyHashEntry *pEntry;` |
|        - |  1773 | `	ph7_value *pValue;` |
|        - |  1774 | `	sxu32 nIdx;` |
|        - |  1775 | `	/* Query the superglobal table */` |
|      976 |  1776 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|      976 |  1777 | `	if( pEntry == 0 ){` |
|        - |  1778 | `		/* No such entry */` |
|      ! 0 |  1779 | `		return 0;` |
|        - |  1780 | `	}` |
|        - |  1781 | `	/* Extract the superglobal index in the global object pool */` |
|      976 |  1782 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1783 | `	/* Extract the variable value  */` |
|      976 |  1784 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      976 |  1785 | `	return pValue;` |
|      489 |  1786 |  |
|        - |  1787 | `/*` |
|        - |  1788 | ` * Perform a raw hashmap insertion.` |
|        - |  1789 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1790 | ` */` |
|      972 |  1791 | `static sxi32 VmHashmapInsert(` |
|        - |  1792 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1793 | `	const char *zKey,   /* Entry key */` |
|        - |  1794 | `	int nKeylen,        /* zKey length*/` |
|        - |  1795 | `	const char *zData,  /* Entry data */` |
|        - |  1796 | `	int nLen            /* zData length */` |
|        - |  1797 | `	)` |
|        2 |  1798 |  |
|        - |  1799 | `	ph7_value sKey,sValue;` |
|        - |  1800 | `	sxi32 rc;` |
|      974 |  1801 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|      974 |  1802 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|      974 |  1803 | `	if( zKey ){` |
|      960 |  1804 | `		if( nKeylen < 0 ){` |
|      960 |  1805 | `			nKeylen = (int)SyStrlen(zKey);` |
|      479 |  1806 | `		}` |
|      960 |  1807 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|      479 |  1808 | `	}` |
|      974 |  1809 | `	if( zData ){` |
|      974 |  1810 | `		if( nLen < 0 ){` |
|        - |  1811 | `			/* Compute length automatically */` |
|      ! 0 |  1812 | `			nLen = (int)SyStrlen(zData);` |
|      ! 0 |  1813 | `		}` |
|      974 |  1814 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|      486 |  1815 | `	}` |
|        - |  1816 | `	/* Perform the insertion */` |
|      974 |  1817 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|      974 |  1818 | `	PH7_MemObjRelease(&sKey);` |
|      974 |  1819 | `	PH7_MemObjRelease(&sValue);` |
|      974 |  1820 | `	return rc;` |
|        2 |  1821 |  |
|        - |  1822 | `/* Forward declaration */` |
|        - |  1823 | `static sxi32 VmHttpProcessRequest(ph7_vm *pVm,const char *zRequest,int nByte);` |
|        - |  1824 | `/*` |
|        - |  1825 | ` * Configure a working virtual machine instance.` |
|        - |  1826 | ` *` |
|        - |  1827 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  1828 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  1829 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  1830 | ` * The second argument to this function is an integer configuration option` |
|        - |  1831 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  1832 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  1833 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  1834 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  1835 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  1836 | ` */` |
|    15312 |  1837 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1838 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1839 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1840 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1841 | `	)` |
|        2 |  1842 |  |
|    15314 |  1843 | `	sxi32 rc = SXRET_OK;` |
|    15314 |  1844 | `	switch(nOp){` |
|      478 |  1845 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|      958 |  1846 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|      958 |  1847 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1848 | `		/* VM output consumer callback */` |
|        - |  1849 | `#ifdef UNTRUST` |
|        - |  1850 | `		if( xConsumer == 0 ){` |
|        - |  1851 | `			rc = SXERR_CORRUPT;` |
|        - |  1852 | `			break;` |
|        - |  1853 | `		}` |
|        - |  1854 | `#endif` |
|        - |  1855 | `		/* Install the output consumer */` |
|      958 |  1856 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|      958 |  1857 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|      958 |  1858 | `		break;` |
|        - |  1859 | `							   }` |
|      478 |  1860 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1861 | `		/* Import path */` |
|        - |  1862 | `		  const char *zPath;` |
|        - |  1863 | `		  SyString sPath;` |
|      958 |  1864 | `		  zPath = va_arg(ap,const char *);` |
|        - |  1865 | `#if defined(UNTRUST)` |
|        - |  1866 | `		  if( zPath == 0 ){` |
|        - |  1867 | `			  rc = SXERR_EMPTY;` |
|        - |  1868 | `			  break;` |
|        - |  1869 | `		  }` |
|        - |  1870 | `#endif` |
|      958 |  1871 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  1872 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  1873 | `#ifdef __WINNT__` |
|        2 |  1874 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  1875 | `#endif` |
|     1914 |  1876 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  1877 | `		  /* Remove leading and trailing white spaces */` |
|      958 |  1878 | `		  SyStringFullTrim(&sPath);` |
|      958 |  1879 | `		  if( sPath.nByte > 0 ){` |
|        - |  1880 | `			  /* Store the path in the corresponding conatiner */` |
|      958 |  1881 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|      478 |  1882 | `		  }` |
|      958 |  1883 | `		  break;` |
|        - |  1884 | `									 }` |
|      478 |  1885 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  1886 | `		/* Run-Time Error report */` |
|      958 |  1887 | `		pVm->bErrReport = 1;` |
|      958 |  1888 | `		break;` |
|      ! 0 |  1889 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  1890 | `		/* Recursion depth */` |
|      ! 0 |  1891 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  1892 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  1893 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  1894 | `		}` |
|      ! 0 |  1895 | `		break;` |
|        - |  1896 | `									   }` |
|      ! 0 |  1897 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  1898 | `		/* VM output length in bytes */` |
|      ! 0 |  1899 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  1900 | `#ifdef UNTRUST` |
|        - |  1901 | `		if( pOut == 0 ){` |
|        - |  1902 | `			rc = SXERR_CORRUPT;` |
|        - |  1903 | `			break;` |
|        - |  1904 | `		}` |
|        - |  1905 | `#endif` |
|      ! 0 |  1906 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  1907 | `		break;` |
|        - |  1908 | `							   }` |
|        - |  1909 |  |
|     4780 |  1910 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  1911 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  1912 | `		/* Create a new superglobal/global variable */` |
|     9562 |  1913 | `		const char *zName = va_arg(ap,const char *);` |
|     9562 |  1914 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  1915 | `		SyHashEntry *pEntry;` |
|        - |  1916 | `		ph7_value *pObj;` |
|        - |  1917 | `		sxu32 nByte;` |
|        - |  1918 | `		sxu32 nIdx;` |
|        - |  1919 | `#ifdef UNTRUST` |
|        - |  1920 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  1921 | `			rc = SXERR_CORRUPT;` |
|        - |  1922 | `			break;` |
|        - |  1923 | `		}` |
|        - |  1924 | `#endif` |
|     9562 |  1925 | `		nByte = SyStrlen(zName);` |
|     9562 |  1926 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1927 | `			/* Check if the superglobal is already installed */` |
|     9562 |  1928 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     4782 |  1929 | `		}else{` |
|        - |  1930 | `			/* Query the top active VM frame */` |
|      ! 0 |  1931 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  1932 | `		}` |
|     9562 |  1933 | `		if( pEntry ){` |
|        - |  1934 | `			/* Variable already installed */` |
|      ! 0 |  1935 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1936 | `			/* Extract contents */` |
|      ! 0 |  1937 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  1938 | `			if( pObj ){` |
|        - |  1939 | `				/* Overwrite old contents */` |
|      ! 0 |  1940 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  1941 | `			}` |
|      ! 0 |  1942 | `		}else{` |
|        - |  1943 | `			/* Install a new variable */` |
|     9562 |  1944 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|     9562 |  1945 | `			if( pObj == 0 ){` |
|      ! 0 |  1946 | `				rc = SXERR_MEM;` |
|      ! 0 |  1947 | `				break;` |
|        - |  1948 | `			}` |
|     9562 |  1949 | `			nIdx = pObj->nIdx;` |
|        - |  1950 | `			/* Copy value */` |
|     9562 |  1951 | `			PH7_MemObjStore(pValue,pObj);` |
|     9562 |  1952 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1953 | `				/* Install the superglobal */` |
|     9562 |  1954 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|     4782 |  1955 | `			}else{` |
|        - |  1956 | `				/* Install in the current frame */` |
|      ! 0 |  1957 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  1958 | `			}` |
|     9562 |  1959 | `			if( rc == SXRET_OK ){` |
|        - |  1960 | `				SyHashEntry *pRef;` |
|     9562 |  1961 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|     9562 |  1962 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|     4782 |  1963 | `				}else{` |
|      ! 0 |  1964 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  1965 | `				}` |
|        - |  1966 | `				/* Install in the reference table */` |
|     9562 |  1967 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|     9562 |  1968 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  1969 | `					/* Register in the $GLOBALS array */` |
|     9562 |  1970 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|     4780 |  1971 | `				}` |
|     4780 |  1972 | `			}` |
|        - |  1973 | `		}` |
|     9562 |  1974 | `		break;` |
|        - |  1975 | `									}` |
|      479 |  1976 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  1977 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  1978 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  1979 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  1980 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  1981 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  1982 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|      960 |  1983 | `		const char *zKey   = va_arg(ap,const char *);` |
|      960 |  1984 | `		const char *zValue = va_arg(ap,const char *);` |
|      960 |  1985 | `		int nLen = va_arg(ap,int);` |
|        - |  1986 | `		ph7_hashmap *pMap;` |
|        - |  1987 | `		ph7_value *pValue;` |
|      960 |  1988 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  1989 | `			/* Extract the $_ENV superglobal */` |
|        3 |  1990 | `			pValue = VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|      959 |  1991 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  1992 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  1993 | `			pValue = VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|      958 |  1994 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  1995 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  1996 | `			pValue = VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|      958 |  1997 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  1998 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  1999 | `			pValue = VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|      958 |  2000 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2001 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2002 | `			pValue = VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|      958 |  2003 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2004 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2005 | `			pValue = VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2006 | `		}else{` |
|        - |  2007 | `			/* Extract the $_SERVER superglobal */` |
|      958 |  2008 | `			pValue = VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2009 | `		}` |
|      960 |  2010 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2011 | `			/* No such entry */` |
|      ! 0 |  2012 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2013 | `			break;` |
|        - |  2014 | `		}` |
|        - |  2015 | `		/* Point to the hashmap */` |
|      960 |  2016 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2017 | `		/* Perform the insertion */` |
|      960 |  2018 | `		rc = VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|      960 |  2019 | `		break;` |
|        - |  2020 | `								   }` |
|        7 |  2021 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2022 | `		/* Script arguments */` |
|       16 |  2023 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2024 | `		ph7_hashmap *pMap;` |
|        - |  2025 | `		ph7_value *pValue;` |
|        - |  2026 | `		sxu32 n;` |
|       16 |  2027 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2028 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2029 | `			break;` |
|        - |  2030 | `		}` |
|        - |  2031 | `		/* Extract the $argv array */` |
|       16 |  2032 | `		pValue = VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       16 |  2033 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2034 | `			/* No such entry */` |
|      ! 0 |  2035 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2036 | `			break;` |
|        - |  2037 | `		}` |
|        - |  2038 | `		/* Point to the hashmap */` |
|       16 |  2039 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2040 | `		/* Perform the insertion */` |
|       16 |  2041 | `		n = (sxu32)SyStrlen(zValue);` |
|       16 |  2042 | `		rc = VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       16 |  2043 | `		if( rc == SXRET_OK ){` |
|       16 |  2044 | `			if( pMap->nEntry > 1 ){` |
|        - |  2045 | `				/* Append space separator first */` |
|       10 |  2046 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        4 |  2047 | `			}` |
|       16 |  2048 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|        7 |  2049 | `		}` |
|       16 |  2050 | `		break;` |
|        - |  2051 | `								  }` |
|      ! 0 |  2052 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2053 | `		/* error_log() consumer */` |
|      ! 0 |  2054 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2055 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2056 | `		break;` |
|        - |  2057 | `										}` |
|      ! 0 |  2058 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2059 | `		/* Script return value */` |
|      ! 0 |  2060 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2061 | `#ifdef UNTRUST` |
|        - |  2062 | `		if( ppValue == 0 ){` |
|        - |  2063 | `			rc = SXERR_CORRUPT;` |
|        - |  2064 | `			break;` |
|        - |  2065 | `		}` |
|        - |  2066 | `#endif` |
|      ! 0 |  2067 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2068 | `		break;` |
|        - |  2069 | `								   }` |
|      956 |  2070 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2071 | `		/* Register an IO stream device */` |
|     1914 |  2072 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2073 | `		/* Make sure we are dealing with a valid IO stream */` |
|     2868 |  2074 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     1914 |  2075 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2076 | `				/* Invalid stream */` |
|      ! 0 |  2077 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2078 | `				break;` |
|        - |  2079 | `		}` |
|     1914 |  2080 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2081 | `			/* Make the 'file://' stream the defaut stream device */` |
|      958 |  2082 | `			pVm->pDefStream = pStream;` |
|      478 |  2083 | `		}` |
|        - |  2084 | `		/* Insert in the appropriate container */` |
|     1914 |  2085 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     1914 |  2086 | `		break;` |
|        - |  2087 | `								  }` |
|      ! 0 |  2088 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2089 | `		/* Point to the VM internal output consumer buffer */` |
|      ! 0 |  2090 | `		const void **ppOut = va_arg(ap,const void **);` |
|      ! 0 |  2091 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2092 | `#ifdef UNTRUST` |
|        - |  2093 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2094 | `			rc = SXERR_CORRUPT;` |
|        - |  2095 | `			break;` |
|        - |  2096 | `		}` |
|        - |  2097 | `#endif` |
|      ! 0 |  2098 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|      ! 0 |  2099 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|      ! 0 |  2100 | `		break;` |
|        - |  2101 | `									   }` |
|      ! 0 |  2102 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2103 | `		/* Raw HTTP request*/` |
|      ! 0 |  2104 | `		const char *zRequest = va_arg(ap,const char *);` |
|      ! 0 |  2105 | `		int nByte = va_arg(ap,int);` |
|      ! 0 |  2106 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2107 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2108 | `			break;` |
|        - |  2109 | `		}` |
|      ! 0 |  2110 | `		if( nByte < 0 ){` |
|        - |  2111 | `			/* Compute length automatically */` |
|      ! 0 |  2112 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2113 | `		}` |
|        - |  2114 | `		/* Process the request */` |
|      ! 0 |  2115 | `		rc = VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|      ! 0 |  2116 | `		break;` |
|        - |  2117 | `									}` |
|      ! 0 |  2118 | `	default:` |
|        - |  2119 | `		/* Unknown configuration option */` |
|      ! 0 |  2120 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2121 | `		break;` |
|        - |  2122 | `	}` |
|    15314 |  2123 | `	return rc;` |
|        2 |  2124 |  |
|        - |  2125 | `/* Forward declaration */` |
|        - |  2126 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2127 | `/*` |
|        - |  2128 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2129 | ` * format.` |
|        - |  2130 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2131 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2132 | ` * (STDOUT).` |
|        - |  2133 | ` */` |
|        2 |  2134 | `static sxi32 VmByteCodeDump(` |
|        - |  2135 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2136 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2137 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2138 | `	)` |
|        1 |  2139 |  |
|        - |  2140 | `	static const char zDump[] = {` |
|        - |  2141 | `		"====================================================\n"` |
|        - |  2142 | `		"PH7 VM Dump\n"` |
|        - |  2143 | `		"====================================================\n"` |
|        - |  2144 | `	};` |
|        - |  2145 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2146 | `	sxi32 rc = SXRET_OK;` |
|        - |  2147 | `	sxu32 n;` |
|        - |  2148 | `	/* Point to the PH7 instructions */` |
|        3 |  2149 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2150 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2151 | `	n = 0;` |
|        3 |  2152 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2153 | `	/* Dump instructions */` |
|        6 |  2154 | `	for(;;){` |
|       13 |  2155 | `		if( pInstr >= pEnd ){` |
|        - |  2156 | `			/* No more instructions */` |
|        3 |  2157 | `			break;` |
|        - |  2158 | `		}` |
|        - |  2159 | `		/* Format and call the consumer callback */` |
|       16 |  2160 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       10 |  2161 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       10 |  2162 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       11 |  2163 | `		if( rc != SXRET_OK ){` |
|        - |  2164 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2165 | `			return rc;` |
|        - |  2166 | `		}` |
|       11 |  2167 | `		++n;` |
|       11 |  2168 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2169 | `	}` |
|        3 |  2170 | `	return rc;` |
|        2 |  2171 |  |
|        - |  2172 | `/* Forward declaration */` |
|        - |  2173 | `static int VmObConsumer(const void *pData,unsigned int nDataLen,void *pUserData);` |
|        - |  2174 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2175 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2176 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2177 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2178 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2179 | `/*` |
|        - |  2180 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2181 | ` * consumer callback.` |
|        - |  2182 | ` */` |
|       92 |  2183 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        2 |  2184 |  |
|       94 |  2185 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|       94 |  2186 | `	sxi32 rc = SXRET_OK;` |
|        - |  2187 | `	/* Append a new line */` |
|        - |  2188 | `#ifdef __WINNT__` |
|        2 |  2189 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2190 | `#else` |
|       92 |  2191 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2192 | `#endif` |
|        - |  2193 | `	/* Invoke the output consumer callback */` |
|       94 |  2194 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|       94 |  2195 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  2196 | `		/* Increment output length */` |
|       93 |  2197 | `		pVm->nOutputLen += SyBlobLength(pMsg);` |
|       46 |  2198 | `	}` |
|       94 |  2199 | `	return rc;` |
|        2 |  2200 |  |
|        - |  2201 | `/*` |
|        - |  2202 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2203 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2204 | ` * information.` |
|        - |  2205 | ` */` |
|       86 |  2206 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, SyString *pFile, sxi32 iLine)` |
|        2 |  2207 |  |
|       88 |  2208 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2209 | `		ph7_value apArg[4];` |
|        - |  2210 | `		ph7_value *apArgPtr[4];` |
|        - |  2211 | `		ph7_value sResult;` |
|        - |  2212 | `		SyString sErr;` |
|        - |  2213 | `		/* Prepare arguments */` |
|        9 |  2214 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        9 |  2215 | `		SyStringInitFromBuf(&sErr,zMessage,SyStrlen(zMessage));` |
|        9 |  2216 | `		PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|        9 |  2217 | `		if( pFile ){` |
|        9 |  2218 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|        9 |  2219 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|        5 |  2220 | `		}else{` |
|      ! 0 |  2221 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2222 | `		}` |
|        9 |  2223 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|        9 |  2224 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2225 | `		/* Set up pointer array */` |
|        9 |  2226 | `		apArgPtr[0] = &apArg[0];` |
|        9 |  2227 | `		apArgPtr[1] = &apArg[1];` |
|        9 |  2228 | `		apArgPtr[2] = &apArg[2];` |
|        9 |  2229 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2230 | `		/* Call the handler */` |
|        9 |  2231 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2232 | `		/* Check return value */` |
|        9 |  2233 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2234 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2235 | `		}` |
|        - |  2236 | `		/* Release */` |
|        9 |  2237 | `		PH7_MemObjRelease(&apArg[0]);` |
|        9 |  2238 | `		PH7_MemObjRelease(&apArg[1]);` |
|        9 |  2239 | `		PH7_MemObjRelease(&apArg[2]);` |
|        9 |  2240 | `		PH7_MemObjRelease(&apArg[3]);` |
|        9 |  2241 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2242 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2243 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|        9 |  2244 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2245 | `	}` |
|        - |  2246 | `	/* No handler, always call error handler */` |
|       80 |  2247 | `	return TRUE;` |
|       45 |  2248 |  |
|       62 |  2249 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2250 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2251 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2252 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2253 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2254 | `	)` |
|        2 |  2255 |  |
|       64 |  2256 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2257 | `	SyString *pFile;` |
|        - |  2258 | `	char *zErr;` |
|       64 |  2259 | `	sxi32 rc = SXRET_OK;` |
|       64 |  2260 | `	if( !pVm->bErrReport ){` |
|        - |  2261 | `		/* Don't bother reporting errors */` |
|        3 |  2262 | `		return SXRET_OK;` |
|        - |  2263 | `	}` |
|        - |  2264 | `	/* Reset the working buffer */` |
|       62 |  2265 | `	SyBlobReset(pWorker);` |
|        - |  2266 | `	/* Peek the processed file if available */` |
|       62 |  2267 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       62 |  2268 | `	if( pFile ){` |
|        - |  2269 | `		/* Append file name */` |
|       62 |  2270 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       62 |  2271 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       30 |  2272 | `	}` |
|       62 |  2273 | `	zErr = "Error: ";` |
|       62 |  2274 | `	switch(iErr){` |
|       27 |  2275 | `	case PH7_CTX_WARNING: zErr = "Warning: "; break;` |
|       14 |  2276 | `	case PH7_CTX_NOTICE:  zErr = "Notice: ";  break;` |
|       11 |  2277 | `	default:` |
|       23 |  2278 | `		iErr = PH7_CTX_ERR;` |
|       22 |  2279 | `		break;` |
|        - |  2280 | `	}` |
|       62 |  2281 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       62 |  2282 | `	if( pFuncName ){` |
|        - |  2283 | `		/* Append function name first */` |
|       29 |  2284 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       29 |  2285 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       14 |  2286 | `	}` |
|       62 |  2287 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2288 | `	/* Check for user error handler */` |
|       62 |  2289 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, pFile, 0) ){` |
|       53 |  2290 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       26 |  2291 | `	}` |
|       62 |  2292 | `	return rc;` |
|       33 |  2293 |  |
|        - |  2294 | `/*` |
|        - |  2295 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2296 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2297 | ` * information.` |
|        - |  2298 | ` */` |
|       26 |  2299 | `static sxi32 VmThrowErrorAp(` |
|        - |  2300 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2301 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2302 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2303 | `	const char *zFormat, /* Format message */` |
|        - |  2304 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2305 | `	)` |
|        2 |  2306 |  |
|       28 |  2307 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2308 | `	SyBlob sMsg;` |
|        - |  2309 | `	SyString *pFile;` |
|        - |  2310 | `	char *zErr;` |
|       28 |  2311 | `	sxi32 rc = SXRET_OK;` |
|       28 |  2312 | `	if( !pVm->bErrReport ){` |
|        - |  2313 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2314 | `		return SXRET_OK;` |
|        - |  2315 | `	}` |
|        - |  2316 | `	/* Reset the working buffer */` |
|       28 |  2317 | `	SyBlobReset(pWorker);` |
|        - |  2318 | `	/* Peek the processed file if available */` |
|       28 |  2319 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       28 |  2320 | `	if( pFile ){` |
|        - |  2321 | `		/* Append file name */` |
|       28 |  2322 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       28 |  2323 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       13 |  2324 | `	}` |
|       28 |  2325 | `	zErr = "Error: ";` |
|       28 |  2326 | `	switch(iErr){` |
|       10 |  2327 | `	case PH7_CTX_WARNING: zErr = "Warning: "; break;` |
|        7 |  2328 | `	case PH7_CTX_NOTICE:  zErr = "Notice: ";  break;` |
|        6 |  2329 | `	default:` |
|       13 |  2330 | `		iErr = PH7_CTX_ERR;` |
|       12 |  2331 | `		break;` |
|        - |  2332 | `	}` |
|       28 |  2333 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       28 |  2334 | `	if( pFuncName ){` |
|        - |  2335 | `		/* Append function name first */` |
|       14 |  2336 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       14 |  2337 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|        6 |  2338 | `	}` |
|        - |  2339 | `	/* Format the raw message */` |
|       28 |  2340 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       28 |  2341 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2342 | `	/* Check if a user error handler is installed */` |
|       28 |  2343 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), pFile, 0) ){` |
|        - |  2344 | `		/* No handler or handler returned TRUE, normal processing */` |
|       28 |  2345 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       28 |  2346 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2347 | `	}` |
|       28 |  2348 | `	SyBlobRelease(&sMsg);` |
|       28 |  2349 | `	return rc;` |
|       15 |  2350 |  |
|        - |  2351 | `/*` |
|        - |  2352 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2353 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2354 | ` * information.` |
|        - |  2355 | ` * ------------------------------------` |
|        - |  2356 | ` * Simple boring wrapper function.` |
|        - |  2357 | ` * ------------------------------------` |
|        - |  2358 | ` */` |
|       14 |  2359 | `static sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  2360 |  |
|        - |  2361 | `	va_list ap;` |
|        - |  2362 | `	sxi32 rc;` |
|       15 |  2363 | `	va_start(ap,zFormat);` |
|       15 |  2364 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       15 |  2365 | `	va_end(ap);` |
|       15 |  2366 | `	return rc;` |
|        1 |  2367 |  |
|        - |  2368 | `/*` |
|        - |  2369 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2370 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2371 | ` * information.` |
|        - |  2372 | ` * ------------------------------------` |
|        - |  2373 | ` * Simple boring wrapper function.` |
|        - |  2374 | ` * ------------------------------------` |
|        - |  2375 | ` */` |
|       12 |  2376 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  2377 |  |
|        - |  2378 | `	sxi32 rc;` |
|       14 |  2379 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       14 |  2380 | `	return rc;` |
|        2 |  2381 |  |
|        - |  2382 | `/*` |
|        - |  2383 | ` * Resolve function context from the current frame.` |
|        - |  2384 | ` */` |
|       28 |  2385 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  2386 |  |
|        - |  2387 | `	VmFrame *pFrame;` |
|        - |  2388 | `	ph7_vm_func *pFunc;` |
|       29 |  2389 | `	*pzFuncName = 0;` |
|       29 |  2390 | `	*pnFuncLen = 0;` |
|       29 |  2391 | `	pFrame = pVm->pFrame;` |
|       29 |  2392 | `	if( pFrame == 0 ){` |
|      ! 0 |  2393 | `		return;` |
|        - |  2394 | `	}` |
|       29 |  2395 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  2396 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  2397 | `	}` |
|       29 |  2398 | `	if( pFrame->pParent == 0 ){` |
|       29 |  2399 | `		return;` |
|        - |  2400 | `	}` |
|      ! 0 |  2401 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      ! 0 |  2402 | `	if( pFunc == 0 ){` |
|      ! 0 |  2403 | `		return;` |
|        - |  2404 | `	}` |
|      ! 0 |  2405 | `	*pzFuncName = pFunc->sName.zString;` |
|      ! 0 |  2406 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|       15 |  2407 |  |
|        - |  2408 | `/*` |
|        - |  2409 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  2410 | ` */` |
|       14 |  2411 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  2412 |  |
|        - |  2413 | `	SyBlob sOut;` |
|        - |  2414 | `	SyString *pFile;` |
|       15 |  2415 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  2416 | `		return PH7_OK;` |
|        - |  2417 | `	}` |
|       15 |  2418 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  2419 | `		zClass = "Exception";` |
|      ! 0 |  2420 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  2421 | `	}` |
|       15 |  2422 | `	if( zMsg == 0 ){` |
|      ! 0 |  2423 | `		zMsg = "Unknown exception";` |
|      ! 0 |  2424 | `		nMsg = (sxu32)sizeof("Unknown exception") - 1;` |
|      ! 0 |  2425 | `	}` |
|       15 |  2426 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|       15 |  2427 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|        7 |  2428 | `	}` |
|       15 |  2429 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       15 |  2430 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|       15 |  2431 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|       15 |  2432 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|       15 |  2433 | `	SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|       15 |  2434 | `	SyBlobAppend(&sOut,zMsg,nMsg);` |
|       15 |  2435 | `	if( pFile ){` |
|       15 |  2436 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|       15 |  2437 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|       15 |  2438 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|        7 |  2439 | `	}` |
|       15 |  2440 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|       15 |  2441 | `	if( pFile ){` |
|       15 |  2442 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|       15 |  2443 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|       15 |  2444 | `		if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  2445 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  2446 | `		}else{` |
|       15 |  2447 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  2448 | `		}` |
|        7 |  2449 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  2450 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  2451 | `	}else{` |
|      ! 0 |  2452 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  2453 | `	}` |
|       15 |  2454 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|       15 |  2455 | `	if( pFile ){` |
|       15 |  2456 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|       15 |  2457 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|       15 |  2458 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|       15 |  2459 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|        7 |  2460 | `	}` |
|       15 |  2461 | `	VmCallErrorHandler(pVm,&sOut);` |
|       15 |  2462 | `	SyBlobRelease(&sOut);` |
|       15 |  2463 | `	return PH7_ABORT;` |
|        8 |  2464 |  |
|        - |  2465 | `/*` |
|        - |  2466 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  2467 | ` */` |
|       14 |  2468 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  2469 |  |
|        - |  2470 | `	ph7_vm *pVm;` |
|        - |  2471 | `	ph7_class *pClass;` |
|        - |  2472 | `	ph7_class_instance *pThis;` |
|        - |  2473 | `	ph7_class_method *pCons;` |
|        - |  2474 | `	ph7_value sArg;` |
|        - |  2475 | `	ph7_value *apArg[1];` |
|        - |  2476 | `	SyBlob sMsg;` |
|        - |  2477 | `	SyString sMsgStr;` |
|        - |  2478 | `	VmFrame *pFrame;` |
|        - |  2479 | `	va_list ap;` |
|        - |  2480 | `	sxi32 rc;` |
|        - |  2481 |  |
|       16 |  2482 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2483 | `		return PH7_ABORT;` |
|        - |  2484 | `	}` |
|       16 |  2485 | `	pVm = pCtx->pVm;` |
|       16 |  2486 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2487 | `		zClass = "Error";` |
|      ! 0 |  2488 | `	}` |
|       16 |  2489 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|       16 |  2490 | `	if( pClass == 0 ){` |
|      ! 0 |  2491 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2492 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  2493 | `			zClass` |
|        - |  2494 | `			);` |
|        - |  2495 | `	}` |
|       16 |  2496 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       16 |  2497 | `	if( pThis == 0 ){` |
|      ! 0 |  2498 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2499 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  2500 | `			);` |
|        - |  2501 | `	}` |
|        - |  2502 |  |
|       16 |  2503 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       16 |  2504 | `	va_start(ap,zFormat);` |
|       16 |  2505 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|       16 |  2506 | `	va_end(ap);` |
|        - |  2507 |  |
|       16 |  2508 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       16 |  2509 | `	if( pCons ){` |
|       16 |  2510 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       16 |  2511 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       16 |  2512 | `		apArg[0] = &sArg;` |
|       16 |  2513 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       16 |  2514 | `		PH7_MemObjRelease(&sArg);` |
|        7 |  2515 | `	}` |
|       16 |  2516 | `	SyBlobRelease(&sMsg);` |
|        - |  2517 |  |
|       16 |  2518 | `	pFrame = pVm->pFrame;` |
|       16 |  2519 | `	if( pFrame ){` |
|       18 |  2520 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        3 |  2521 | `			pFrame = pFrame->pParent;` |
|        1 |  2522 | `		}` |
|       16 |  2523 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        7 |  2524 | `	}` |
|       16 |  2525 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       16 |  2526 | `	PH7_ClassInstanceUnref(pThis);` |
|       16 |  2527 | `	if( rc == SXERR_ABORT ){` |
|       13 |  2528 | `		return PH7_ABORT;` |
|        - |  2529 | `	}` |
|        3 |  2530 | `	return PH7_EXCEPTION;` |
|        9 |  2531 |  |
|        - |  2532 | `/*` |
|        - |  2533 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  2534 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  2535 | ` */` |
|      ! 0 |  2536 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  2537 |  |
|        - |  2538 | `	ph7_vm *pVm;` |
|        - |  2539 | `	SyBlob sMsg;` |
|      ! 0 |  2540 | `	const char *zFuncName = 0;` |
|      ! 0 |  2541 | `	int nFuncLen = 0;` |
|        - |  2542 | `	va_list ap;` |
|        - |  2543 | `	sxi32 rc;` |
|        - |  2544 |  |
|      ! 0 |  2545 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2546 | `		return PH7_OK;` |
|        - |  2547 | `	}` |
|      ! 0 |  2548 | `	pVm = pCtx->pVm;` |
|      ! 0 |  2549 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2550 | `		zClass = "Error";` |
|      ! 0 |  2551 | `	}` |
|        - |  2552 |  |
|      ! 0 |  2553 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2554 |  |
|      ! 0 |  2555 | `	va_start(ap,zFormat);` |
|      ! 0 |  2556 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  2557 | `	va_end(ap);` |
|        - |  2558 |  |
|      ! 0 |  2559 | `	if( pCtx->pFunc ){` |
|      ! 0 |  2560 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  2561 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  2562 | `	}` |
|      ! 0 |  2563 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  2564 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  2565 | `	}` |
|      ! 0 |  2566 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  2567 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  2568 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  2569 | `	return rc;` |
|      ! 0 |  2570 |  |
|        - |  2571 | `/*` |
|        - |  2572 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  2573 | ` *` |
|        - |  2574 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  2575 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  2576 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  2577 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  2578 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  2579 | ` * then the program execution is halted.` |
|        - |  2580 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  2581 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  2582 | ` * or to reset the VM to it's initial state.` |
|        - |  2583 | ` */` |
|    20090 |  2584 | `static sxi32 VmByteCodeExec(` |
|        - |  2585 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2586 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  2587 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  2588 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  2589 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  2590 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  2591 | `	int is_callback      /* TRUE if we are executing a callback */` |
|        - |  2592 | `	)` |
|        2 |  2593 |  |
|        - |  2594 | `	VmInstr *pInstr;` |
|        - |  2595 | `	ph7_value *pTos;` |
|        - |  2596 | `	SySet aArg;` |
|        - |  2597 | `	sxi32 pc;` |
|        - |  2598 | `	sxi32 rc;` |
|        - |  2599 | `	/* Argument container */` |
|    20092 |  2600 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    20092 |  2601 | `	if( nTos < 0 ){` |
|    19564 |  2602 | `		pTos = &pStack[-1];` |
|     9783 |  2603 | `	}else{` |
|      530 |  2604 | `		pTos = &pStack[nTos];` |
|        - |  2605 | `	}` |
|    20092 |  2606 | `	pc = 0;` |
|        - |  2607 | `	/* Execute as much as we can */` |
|  3321810 |  2608 | `	for(;;){` |
|        - |  2609 | `		/* Fetch the instruction to execute */` |
|  6642918 |  2610 | `		pInstr = &aInstr[pc];` |
|  6642918 |  2611 | `		rc = SXRET_OK;` |
|        - |  2612 | `/*` |
|        - |  2613 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2614 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2615 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2616 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2617 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2618 | ` */` |
|  6642918 |  2619 | `		switch(pInstr->iOp){` |
|        - |  2620 | `/*` |
|        - |  2621 | ` * DONE: P1 * *` |
|        - |  2622 | ` *` |
|        - |  2623 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2624 | ` * and return immediately.` |
|        - |  2625 | ` */` |
|    10031 |  2626 | `case PH7_OP_DONE:` |
|    20064 |  2627 | `	if( pInstr->iP1 ){` |
|        - |  2628 | `#ifdef UNTRUST` |
|        - |  2629 | `		if( pTos < pStack ){` |
|        - |  2630 | `			goto Abort;` |
|        - |  2631 | `		}` |
|        - |  2632 | `#endif` |
|    11008 |  2633 | `		if( pLastRef ){` |
|     7340 |  2634 | `			*pLastRef = pTos->nIdx;` |
|     3669 |  2635 | `		}` |
|    11008 |  2636 | `		if( pResult ){` |
|        - |  2637 | `			/* Execution result */` |
|    10708 |  2638 | `			PH7_MemObjStore(pTos,pResult);` |
|     5353 |  2639 | `		}` |
|    11008 |  2640 | `		VmPopOperand(&pTos,1);` |
|    14561 |  2641 | `	}else if( pLastRef ){` |
|        - |  2642 | `		/* Nothing referenced */` |
|      386 |  2643 | `		*pLastRef = SXU32_HIGH;` |
|      192 |  2644 | `	}` |
|    20064 |  2645 | `	goto Done;` |
|        - |  2646 | `/*` |
|        - |  2647 | ` * HALT: P1 * *` |
|        - |  2648 | ` *` |
|        - |  2649 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  2650 | ` * and abort immediately.` |
|        - |  2651 | ` */` |
|        4 |  2652 | `case PH7_OP_HALT:` |
|        9 |  2653 | `	if( pInstr->iP1 ){` |
|        - |  2654 | `#ifdef UNTRUST` |
|        - |  2655 | `		if( pTos < pStack ){` |
|        - |  2656 | `			goto Abort;` |
|        - |  2657 | `		}` |
|        - |  2658 | `#endif` |
|        9 |  2659 | `		if( pLastRef ){` |
|      ! 0 |  2660 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  2661 | `		}` |
|        9 |  2662 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  2663 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2664 | `				/* Output the exit message */` |
|        7 |  2665 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  2666 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  2667 | `				if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  2668 | `					/* Increment output length */` |
|        5 |  2669 | `					pVm->nOutputLen += SyBlobLength(&pTos->sBlob);` |
|        2 |  2670 | `				}` |
|        3 |  2671 | `			}` |
|        7 |  2672 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  2673 | `			/* Record exit status */` |
|        5 |  2674 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  2675 | `		}` |
|        9 |  2676 | `		VmPopOperand(&pTos,1);` |
|        4 |  2677 | `	}else if( pLastRef ){` |
|        - |  2678 | `		/* Nothing referenced */` |
|      ! 0 |  2679 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  2680 | `	}` |
|        - |  2681 | `	/* Check if we're in an included file context */` |
|        9 |  2682 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  2683 | `		/* Terminate the entire process */` |
|        9 |  2684 | `		exit(pVm->iExitStatus);` |
|        - |  2685 | `	}` |
|      ! 0 |  2686 | `	goto Abort;` |
|        - |  2687 | `/*` |
|        - |  2688 | ` * JMP: * P2 *` |
|        - |  2689 | ` *` |
|        - |  2690 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  2691 | ` * the one at index P2 from the beginning of the program.` |
|        - |  2692 | ` */` |
|   150149 |  2693 | `case PH7_OP_JMP:` |
|   300344 |  2694 | `	pc = pInstr->iP2 - 1;` |
|   300344 |  2695 | `	break;` |
|        - |  2696 | `/*` |
|        - |  2697 | ` * JZ: P1 P2 *` |
|        - |  2698 | ` *` |
|        - |  2699 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2700 | ` * entry in the stack if P1 is zero.` |
|        - |  2701 | ` */` |
|   332146 |  2702 | `case PH7_OP_JZ:` |
|        - |  2703 | `#ifdef UNTRUST` |
|        - |  2704 | `	if( pTos < pStack ){` |
|        - |  2705 | `		goto Abort;` |
|        - |  2706 | `	}` |
|        - |  2707 | `#endif` |
|        - |  2708 | `	/* Get a boolean value */` |
|   664382 |  2709 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       77 |  2710 | `		PH7_MemObjToBool(pTos);` |
|       38 |  2711 | `	}` |
|   664382 |  2712 | `	if( !pTos->x.iVal ){` |
|        - |  2713 | `		/* Take the jump */` |
|   316638 |  2714 | `		pc = pInstr->iP2 - 1;` |
|   158318 |  2715 | `	}` |
|   664382 |  2716 | `	if( !pInstr->iP1 ){` |
|   518408 |  2717 | `		VmPopOperand(&pTos,1);` |
|   259225 |  2718 | `	}` |
|   664382 |  2719 | `	break;` |
|        - |  2720 | `/*` |
|        - |  2721 | ` * JNZ: P1 P2 *` |
|        - |  2722 | ` *` |
|        - |  2723 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2724 | ` * entry in the stack if P1 is zero.` |
|        - |  2725 | ` */` |
|    28033 |  2726 | `case PH7_OP_JNZ:` |
|        - |  2727 | `#ifdef UNTRUST` |
|        - |  2728 | `	if( pTos < pStack ){` |
|        - |  2729 | `		goto Abort;` |
|        - |  2730 | `	}` |
|        - |  2731 | `#endif` |
|        - |  2732 | `	/* Get a boolean value */` |
|    56068 |  2733 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2734 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2735 | `	}` |
|    56068 |  2736 | `	if( pTos->x.iVal ){` |
|        - |  2737 | `		/* Take the jump */` |
|     3088 |  2738 | `		pc = pInstr->iP2 - 1;` |
|     1543 |  2739 | `	}` |
|    56068 |  2740 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2741 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2742 | `	}` |
|    56068 |  2743 | `	break;` |
|        - |  2744 | `/*` |
|        - |  2745 | ` * NOOP: * * *` |
|        - |  2746 | ` *` |
|        - |  2747 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  2748 | ` * destination.` |
|        - |  2749 | ` */` |
|      ! 0 |  2750 | `case PH7_OP_NOOP:` |
|      ! 0 |  2751 | `	break;` |
|        - |  2752 | `/*` |
|        - |  2753 | ` * POP: P1 * *` |
|        - |  2754 | ` *` |
|        - |  2755 | ` * Pop P1 elements from the operand stack.` |
|        - |  2756 | ` */` |
|   269158 |  2757 | `case PH7_OP_POP: {` |
|   538362 |  2758 | `	sxi32 n = pInstr->iP1;` |
|   538362 |  2759 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2760 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 |  2761 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 |  2762 | `	}` |
|   538362 |  2763 | `	VmPopOperand(&pTos,n);` |
|   538362 |  2764 | `	break;` |
|        - |  2765 | `				 }` |
|        - |  2766 | `/*` |
|        - |  2767 | ` * CVT_INT: * * *` |
|        - |  2768 | ` *` |
|        - |  2769 | ` * Force the top of the stack to be an integer.` |
|        - |  2770 | ` */` |
|       29 |  2771 | `case PH7_OP_CVT_INT:` |
|        - |  2772 | `#ifdef UNTRUST` |
|        - |  2773 | `	if( pTos < pStack ){` |
|        - |  2774 | `		goto Abort;` |
|        - |  2775 | `	}` |
|        - |  2776 | `#endif` |
|       60 |  2777 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|       27 |  2778 | `		PH7_MemObjToInteger(pTos);` |
|       13 |  2779 | `	}` |
|        - |  2780 | `	/* Invalidate any prior representation */` |
|       60 |  2781 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       60 |  2782 | `	break;` |
|        - |  2783 | `/*` |
|        - |  2784 | ` * CVT_REAL: * * *` |
|        - |  2785 | ` *` |
|        - |  2786 | ` * Force the top of the stack to be a real.` |
|        - |  2787 | ` */` |
|        4 |  2788 | `case PH7_OP_CVT_REAL:` |
|        - |  2789 | `#ifdef UNTRUST` |
|        - |  2790 | `	if( pTos < pStack ){` |
|        - |  2791 | `		goto Abort;` |
|        - |  2792 | `	}` |
|        - |  2793 | `#endif` |
|        9 |  2794 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  2795 | `		PH7_MemObjToReal(pTos);` |
|        2 |  2796 | `	}` |
|        - |  2797 | `	/* Invalidate any prior representation */` |
|        9 |  2798 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|        9 |  2799 | `	break;` |
|        - |  2800 | `/*` |
|        - |  2801 | ` * CVT_STR: * * *` |
|        - |  2802 | ` *` |
|        - |  2803 | ` * Force the top of the stack to be a string.` |
|        - |  2804 | ` */` |
|      136 |  2805 | `case PH7_OP_CVT_STR:` |
|        - |  2806 | `#ifdef UNTRUST` |
|        - |  2807 | `	if( pTos < pStack ){` |
|        - |  2808 | `		goto Abort;` |
|        - |  2809 | `	}` |
|        - |  2810 | `#endif` |
|      274 |  2811 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      274 |  2812 | `		PH7_MemObjToString(pTos);` |
|      136 |  2813 | `	}` |
|      274 |  2814 | `	break;` |
|        - |  2815 | `/*` |
|        - |  2816 | ` * CVT_BOOL: * * *` |
|        - |  2817 | ` *` |
|        - |  2818 | ` * Force the top of the stack to be a boolean.` |
|        - |  2819 | ` */` |
|        5 |  2820 | `case PH7_OP_CVT_BOOL:` |
|        - |  2821 | `#ifdef UNTRUST` |
|        - |  2822 | `	if( pTos < pStack ){` |
|        - |  2823 | `		goto Abort;` |
|        - |  2824 | `	}` |
|        - |  2825 | `#endif` |
|       11 |  2826 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  2827 | `		PH7_MemObjToBool(pTos);` |
|        3 |  2828 | `	}` |
|       11 |  2829 | `	break;` |
|        - |  2830 | `/*` |
|        - |  2831 | ` * CVT_NULL: * * *` |
|        - |  2832 | ` *` |
|        - |  2833 | ` * Nullify the top of the stack.` |
|        - |  2834 | ` */` |
|        3 |  2835 | `case PH7_OP_CVT_NULL:` |
|        - |  2836 | `#ifdef UNTRUST` |
|        - |  2837 | `	if( pTos < pStack ){` |
|        - |  2838 | `		goto Abort;` |
|        - |  2839 | `	}` |
|        - |  2840 | `#endif` |
|        7 |  2841 | `	PH7_MemObjRelease(pTos);` |
|        7 |  2842 | `	break;` |
|        - |  2843 | `/*` |
|        - |  2844 | ` * CVT_NUMC: * * *` |
|        - |  2845 | ` *` |
|        - |  2846 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  2847 | ` */` |
|      ! 0 |  2848 | `case PH7_OP_CVT_NUMC:` |
|        - |  2849 | `#ifdef UNTRUST` |
|        - |  2850 | `	if( pTos < pStack ){` |
|        - |  2851 | `		goto Abort;` |
|        - |  2852 | `	}` |
|        - |  2853 | `#endif` |
|        - |  2854 | `	/* Force a numeric cast */` |
|      ! 0 |  2855 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  2856 | `	break;` |
|        - |  2857 | `/*` |
|        - |  2858 | ` * CVT_ARRAY: * * *` |
|        - |  2859 | ` *` |
|        - |  2860 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  2861 | ` */` |
|       10 |  2862 | `case PH7_OP_CVT_ARRAY:` |
|        - |  2863 | `#ifdef UNTRUST` |
|        - |  2864 | `	if( pTos < pStack ){` |
|        - |  2865 | `		goto Abort;` |
|        - |  2866 | `	}` |
|        - |  2867 | `#endif` |
|        - |  2868 | `	/* Force a hashmap cast */` |
|       21 |  2869 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  2870 | `	if( rc != SXRET_OK ){` |
|        - |  2871 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  2872 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  2873 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  2874 | `	}` |
|       21 |  2875 | `	break;` |
|        - |  2876 | `/*` |
|        - |  2877 | ` * CVT_OBJ: * * *` |
|        - |  2878 | ` *` |
|        - |  2879 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  2880 | ` */` |
|        8 |  2881 | `case PH7_OP_CVT_OBJ:` |
|        - |  2882 | `#ifdef UNTRUST` |
|        - |  2883 | `	if( pTos < pStack ){` |
|        - |  2884 | `		goto Abort;` |
|        - |  2885 | `	}` |
|        - |  2886 | `#endif` |
|       17 |  2887 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  2888 | `		/* Force a 'stdClass()' cast */` |
|       17 |  2889 | `		PH7_MemObjToObject(pTos);` |
|        8 |  2890 | `	}` |
|       17 |  2891 | `	break;` |
|        - |  2892 | `/*` |
|        - |  2893 | ` * ERR_CTRL * * *` |
|        - |  2894 | ` *` |
|        - |  2895 | ` * Error control operator.` |
|        - |  2896 | ` */` |
|     7723 |  2897 | `case PH7_OP_ERR_CTRL:` |
|        - |  2898 | `	/*` |
|        - |  2899 | `	 * TICKET 1433-038:` |
|        - |  2900 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  2901 | `	 * use the public API,to control error output.` |
|        - |  2902 | `	 */` |
|    15446 |  2903 | `	break;` |
|        - |  2904 | `/*` |
|        - |  2905 | ` * IS_A * * *` |
|        - |  2906 | ` *` |
|        - |  2907 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  2908 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  2909 | ` * holding a class name or an object).` |
|        - |  2910 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  2911 | ` */` |
|       11 |  2912 | `case PH7_OP_IS_A:{` |
|       23 |  2913 | `	ph7_value *pNos = &pTos[-1];` |
|       23 |  2914 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  2915 | `#ifdef UNTRUST` |
|        - |  2916 | `	if( pNos < pStack ){` |
|        - |  2917 | `		goto Abort;` |
|        - |  2918 | `	}` |
|        - |  2919 | `#endif` |
|       23 |  2920 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       21 |  2921 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       21 |  2922 | `		ph7_class *pClass = 0;` |
|        - |  2923 | `		/* Extract the target class */` |
|       21 |  2924 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  2925 | `			/* Instance already loaded */` |
|      ! 0 |  2926 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       21 |  2927 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2928 | `			/* Perform the query */` |
|       31 |  2929 | `			pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|       20 |  2930 | `				SyBlobLength(&pTos->sBlob),FALSE,0);` |
|       10 |  2931 | `		}` |
|       21 |  2932 | `		if( pClass ){` |
|        - |  2933 | `			/* Perform the query */` |
|       21 |  2934 | `			iRes = VmInstanceOf(pThis->pClass,pClass);` |
|       10 |  2935 | `		}` |
|       10 |  2936 | `	}` |
|        - |  2937 | `	/* Push result */` |
|       23 |  2938 | `	VmPopOperand(&pTos,1);` |
|       23 |  2939 | `	PH7_MemObjRelease(pTos);` |
|       23 |  2940 | `	pTos->x.iVal = iRes;` |
|       23 |  2941 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       23 |  2942 | `	break;` |
|        - |  2943 | `				 }` |
|        - |  2944 |  |
|        - |  2945 | `/*` |
|        - |  2946 | ` * LOADC P1 P2 *` |
|        - |  2947 | ` *` |
|        - |  2948 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  2949 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  2950 | ` */` |
|   605066 |  2951 | `case PH7_OP_LOADC: {` |
|        - |  2952 | `	ph7_value *pObj;` |
|        - |  2953 | `	/* Reserve a room */` |
|  1210178 |  2954 | `	pTos++;` |
|  1210178 |  2955 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1210178 |  2956 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  2957 | `			SyHashEntry *pEntry;` |
|        - |  2958 | `			/* Candidate for expansion via user defined callbacks */` |
|    11882 |  2959 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    11882 |  2960 | `			if( pEntry ){` |
|    10882 |  2961 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  2962 | `				/* Set a NULL default value */` |
|    10882 |  2963 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    10882 |  2964 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  2965 | `				/* Invoke the callback and deal with the expanded value */` |
|    10882 |  2966 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  2967 | `				/* Mark as constant */` |
|    10882 |  2968 | `				pTos->nIdx = SXU32_HIGH;` |
|    10882 |  2969 | `				break;` |
|        - |  2970 | `			}` |
|      500 |  2971 | `		}` |
|  1199298 |  2972 | `		PH7_MemObjLoad(pObj,pTos);` |
|   599672 |  2973 | `	}else{` |
|        - |  2974 | `		/* Set a NULL value */` |
|      ! 0 |  2975 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  2976 | `	}` |
|        - |  2977 | `	/* Mark as constant */` |
|  1199298 |  2978 | `	pTos->nIdx = SXU32_HIGH;` |
|  1199298 |  2979 | `	break;` |
|        - |  2980 | `				  }` |
|        - |  2981 | `/*` |
|        - |  2982 | ` * LOAD: P1 * P3` |
|        - |  2983 | ` *` |
|        - |  2984 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  2985 | ` * from the P3 operand.` |
|        - |  2986 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  2987 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  2988 | ` */` |
|   863755 |  2989 | `case PH7_OP_LOAD:{` |
|        - |  2990 | `	ph7_value *pObj;` |
|        - |  2991 | `	SyString sName;` |
|  1727732 |  2992 | `	if( pInstr->p3 == 0 ){` |
|        - |  2993 | `		/* Take the variable name from the top of the stack */` |
|        - |  2994 | `#ifdef UNTRUST` |
|        - |  2995 | `		if( pTos < pStack ){` |
|        - |  2996 | `			goto Abort;` |
|        - |  2997 | `		}` |
|        - |  2998 | `#endif` |
|        - |  2999 | `		/* Force a string cast */` |
|       25 |  3000 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3001 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3002 | `		}` |
|       25 |  3003 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       13 |  3004 | `	}else{` |
|  1727708 |  3005 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3006 | `		/* Reserve a room for the target object */` |
|  1727708 |  3007 | `		pTos++;` |
|        - |  3008 | `	}` |
|        - |  3009 | `	/* Extract the requested memory object */` |
|  1727732 |  3010 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  1727732 |  3011 | `	if( pObj == 0 ){` |
|      456 |  3012 | `		if( pInstr->iP1 ){` |
|        - |  3013 | `			/* Variable not found,load NULL */` |
|      456 |  3014 | `			if( !pInstr->p3 ){` |
|      ! 0 |  3015 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3016 | `			}else{` |
|      456 |  3017 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3018 | `			}` |
|      456 |  3019 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|   863984 |  3020 | `			break;` |
|      ! 0 |  3021 | `		}else{` |
|        - |  3022 | `			/* Fatal error */` |
|      ! 0 |  3023 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3024 | `			goto Abort;` |
|        - |  3025 | `		}` |
|        - |  3026 | `	}` |
|        - |  3027 | `	/* Load variable contents */` |
|  1727278 |  3028 | `	PH7_MemObjLoad(pObj,pTos);` |
|  1727278 |  3029 | `	pTos->nIdx = pObj->nIdx;` |
|  1727278 |  3030 | `	break;` |
|        - |  3031 | `				   }` |
|        - |  3032 | `/*` |
|        - |  3033 | ` * LOAD_MAP P1 * *` |
|        - |  3034 | ` *` |
|        - |  3035 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  3036 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  3037 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  3038 | ` */` |
|    13057 |  3039 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3040 | `	ph7_hashmap *pMap;` |
|        - |  3041 | `	/* Allocate a new hashmap instance */` |
|    26116 |  3042 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    26116 |  3043 | `	if( pMap == 0 ){` |
|      ! 0 |  3044 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3045 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3046 | `		goto Abort;` |
|        - |  3047 | `	}` |
|    26116 |  3048 | `	if( pInstr->iP1 > 0 ){` |
|     1414 |  3049 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3050 | `		/* Perform the insertion */` |
|     3994 |  3051 | `		while( pEntry < pTos ){` |
|     2582 |  3052 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3053 | `				/* Insertion by reference */` |
|      142 |  3054 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3055 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3056 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3057 | `					);` |
|       48 |  3058 | `			}else{` |
|        - |  3059 | `				/* Standard insertion */` |
|     3731 |  3060 | `				PH7_HashmapInsert(pMap,` |
|     2486 |  3061 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     1243 |  3062 | `					&pEntry[1]` |
|        - |  3063 | `				);` |
|        - |  3064 | `			}` |
|        - |  3065 | `			/* Next pair on the stack */` |
|     2582 |  3066 | `			pEntry += 2;` |
|        2 |  3067 | `		}` |
|        - |  3068 | `		/* Pop P1 elements */` |
|     1414 |  3069 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|      706 |  3070 | `	}` |
|        - |  3071 | `	/* Push the hashmap */` |
|    26116 |  3072 | `	pTos++;` |
|    26116 |  3073 | `	pTos->nIdx = SXU32_HIGH;` |
|    26116 |  3074 | `	pTos->x.pOther = pMap;` |
|    26116 |  3075 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    26116 |  3076 | `	break;` |
|        - |  3077 | `					  }` |
|        - |  3078 | `/*` |
|        - |  3079 | ` * LOAD_LIST: P1 * *` |
|        - |  3080 | ` *` |
|        - |  3081 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  3082 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  3083 | ` * Caveats:` |
|        - |  3084 | ` *  This implementation support only a single nesting level.` |
|        - |  3085 | ` */` |
|       17 |  3086 | `case PH7_OP_LOAD_LIST: {` |
|        - |  3087 | `	ph7_value *pEntry;` |
|       35 |  3088 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  3089 | `		/* Empty list,break immediately */` |
|      ! 0 |  3090 | `		break;` |
|        - |  3091 | `	}` |
|       35 |  3092 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  3093 | `#ifdef UNTRUST` |
|        - |  3094 | `	if( &pEntry[-1] < pStack ){` |
|        - |  3095 | `		goto Abort;` |
|        - |  3096 | `	}` |
|        - |  3097 | `#endif` |
|       35 |  3098 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       31 |  3099 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  3100 | `		ph7_hashmap_node *pNode;` |
|        - |  3101 | `		ph7_value sKey,*pObj;` |
|        - |  3102 | `		/* Start Copying */` |
|       31 |  3103 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|       99 |  3104 | `		while( pEntry <= pTos ){` |
|       69 |  3105 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|       65 |  3106 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|       65 |  3107 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       65 |  3108 | `					if( rc == SXRET_OK ){` |
|        - |  3109 | `						/* Store node value */` |
|       65 |  3110 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       33 |  3111 | `					}else{` |
|        - |  3112 | `						/* Nullify the variable */` |
|      ! 0 |  3113 | `						PH7_MemObjRelease(pObj);` |
|        - |  3114 | `					}` |
|       32 |  3115 | `				}` |
|       32 |  3116 | `			}` |
|       69 |  3117 | `			sKey.x.iVal++; /* Next numeric index */` |
|       69 |  3118 | `			pEntry++;` |
|        1 |  3119 | `		}` |
|       15 |  3120 | `	}` |
|       35 |  3121 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       35 |  3122 | `	break;` |
|        - |  3123 | `					   }` |
|        - |  3124 | `/*` |
|        - |  3125 | ` * LOAD_IDX: P1 P2 *` |
|        - |  3126 | ` *` |
|        - |  3127 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  3128 | ` * from the stack.` |
|        - |  3129 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  3130 | ` * instead.` |
|        - |  3131 | ` */` |
|   121552 |  3132 | `case PH7_OP_LOAD_IDX: {` |
|   243150 |  3133 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   243150 |  3134 | `	ph7_hashmap *pMap = 0;` |
|        - |  3135 | `	ph7_value *pIdx;` |
|   243150 |  3136 | `	pIdx = 0;` |
|   243150 |  3137 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  3138 | `		if( !pInstr->iP2){` |
|        - |  3139 | `			/* No available index,load NULL */` |
|      ! 0 |  3140 | `			if( pTos >= pStack ){` |
|      ! 0 |  3141 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3142 | `			}else{` |
|        - |  3143 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  3144 | `				pTos++;` |
|      ! 0 |  3145 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3146 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  3147 | `			}` |
|        - |  3148 | `			/* Emit a notice */` |
|      ! 0 |  3149 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  3150 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  3151 | `			break;` |
|        - |  3152 | `		}` |
|      ! 0 |  3153 | `	}else{` |
|   243150 |  3154 | `		pIdx = pTos;` |
|   243150 |  3155 | `		pTos--;` |
|        - |  3156 | `	}` |
|   243150 |  3157 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  3158 | `		/* String access */` |
|   178230 |  3159 | `		if( pIdx ){` |
|        - |  3160 | `			sxu32 nOfft;` |
|   178230 |  3161 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  3162 | `				/* Force an int cast */` |
|      ! 0 |  3163 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3164 | `			}` |
|   178230 |  3165 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   178230 |  3166 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  3167 | `				/* Invalid offset,load null */` |
|      ! 0 |  3168 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3169 | `			}else{` |
|   178230 |  3170 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   178230 |  3171 | `				int c = zData[nOfft];` |
|   178230 |  3172 | `				PH7_MemObjRelease(pTos);` |
|   178230 |  3173 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   178230 |  3174 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  3175 | `			}` |
|    89138 |  3176 | `		}else{` |
|        - |  3177 | `			/* No available index,load NULL */` |
|      ! 0 |  3178 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3179 | `		}` |
|   178230 |  3180 | `		break;` |
|        - |  3181 | `	}` |
|    64922 |  3182 | `	if( pInstr->iP2 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  3183 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3184 | `			ph7_value *pObj;` |
|      ! 0 |  3185 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      ! 0 |  3186 | `				PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3187 | `				PH7_MemObjLoad(pObj,pTos);` |
|      ! 0 |  3188 | `			}` |
|      ! 0 |  3189 | `		}` |
|      ! 0 |  3190 | `	}` |
|    64922 |  3191 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    64922 |  3192 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3193 | `		/* Point to the hashmap */` |
|    64922 |  3194 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    64922 |  3195 | `		if( pIdx ){` |
|        - |  3196 | `			/* Load the desired entry */` |
|    64922 |  3197 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    32460 |  3198 | `		}` |
|    64922 |  3199 | `		if( rc != SXRET_OK && pInstr->iP2 ){` |
|        - |  3200 | `			/* Create a new empty entry */` |
|      ! 0 |  3201 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      ! 0 |  3202 | `			if( rc == SXRET_OK ){` |
|        - |  3203 | `				/* Point to the last inserted entry */` |
|      ! 0 |  3204 | `				pNode = pMap->pLast;` |
|      ! 0 |  3205 | `			}` |
|      ! 0 |  3206 | `		}` |
|    32460 |  3207 | `	}` |
|    64922 |  3208 | `	if( pIdx ){` |
|    64922 |  3209 | `		PH7_MemObjRelease(pIdx);` |
|    32460 |  3210 | `	}` |
|    64922 |  3211 | `	if( rc == SXRET_OK ){` |
|        - |  3212 | `		/* Load entry contents */` |
|    31504 |  3213 | `		if( pMap->iRef < 2 ){` |
|        - |  3214 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3215 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3216 | `			 */` |
|      ! 0 |  3217 | `			pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  3218 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|      ! 0 |  3219 | `		}else{` |
|    31504 |  3220 | `			pTos->nIdx = pNode->nValIdx;` |
|    31504 |  3221 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    31504 |  3222 | `			PH7_HashmapUnref(pMap);` |
|        - |  3223 | `		}` |
|    15753 |  3224 | `	}else{` |
|        - |  3225 | `		/* No such entry,load NULL */` |
|    33420 |  3226 | `		PH7_MemObjRelease(pTos);` |
|    33420 |  3227 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3228 | `	}` |
|    64922 |  3229 | `	break;` |
|        - |  3230 | `					  }` |
|        - |  3231 | `/*` |
|        - |  3232 | ` * LOAD_CLOSURE * * P3` |
|        - |  3233 | ` *` |
|        - |  3234 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  3235 | ` * name in the stack.` |
|        - |  3236 | ` */` |
|        2 |  3237 | `case PH7_OP_LOAD_CLOSURE:{` |
|        5 |  3238 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|        5 |  3239 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  3240 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  3241 | `		ph7_vm_func *pClosure;` |
|        - |  3242 | `		char *zName;` |
|        - |  3243 | `		sxu32 mLen;` |
|        - |  3244 | `		sxu32 n;` |
|        - |  3245 | `		/* Create a new VM function */` |
|        5 |  3246 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  3247 | `		/* Generate an unique closure name */` |
|        5 |  3248 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|        5 |  3249 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  3250 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  3251 | `			goto Abort;` |
|        - |  3252 | `		}` |
|        5 |  3253 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|        5 |  3254 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  3255 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  3256 | `		}` |
|        - |  3257 | `		/* Zero the stucture */` |
|        5 |  3258 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  3259 | `		/* Perform a structure assignment on read-only items */` |
|        5 |  3260 | `		pClosure->aArgs = pFunc->aArgs;` |
|        5 |  3261 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|        5 |  3262 | `		pClosure->aStatic = pFunc->aStatic;` |
|        5 |  3263 | `		pClosure->iFlags = pFunc->iFlags;` |
|        5 |  3264 | `		pClosure->pUserData = pFunc->pUserData;` |
|        5 |  3265 | `		pClosure->sSignature = pFunc->sSignature;` |
|        5 |  3266 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  3267 | `		/* Register the closure */` |
|        5 |  3268 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  3269 | `		/* Set up closure environment */` |
|        5 |  3270 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        5 |  3271 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       13 |  3272 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  3273 | `			ph7_value *pValue;` |
|        9 |  3274 | `			pEnv = &aEnv[n];` |
|        9 |  3275 | `			sEnv.sName  = pEnv->sName;` |
|        9 |  3276 | `			sEnv.iFlags = pEnv->iFlags;` |
|        9 |  3277 | `			sEnv.nIdx = SXU32_HIGH;` |
|        9 |  3278 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|        9 |  3279 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  3280 | `				/* Pass by reference */` |
|      ! 0 |  3281 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  3282 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  3283 | `					);` |
|      ! 0 |  3284 | `			}` |
|        - |  3285 | `			/* Standard pass by value */` |
|        9 |  3286 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|        9 |  3287 | `			if( pValue ){` |
|        - |  3288 | `				/* Copy imported value */` |
|        5 |  3289 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|        2 |  3290 | `			}` |
|        - |  3291 | `			/* Insert the imported variable */` |
|        9 |  3292 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|        5 |  3293 | `		}` |
|        - |  3294 | `		/* Finally,load the closure name on the stack */` |
|        5 |  3295 | `		pTos++;` |
|        5 |  3296 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|        2 |  3297 | `	}` |
|        5 |  3298 | `	break;` |
|        - |  3299 | `						 }` |
|        - |  3300 | `/*` |
|        - |  3301 | ` * STORE * P2 P3` |
|        - |  3302 | ` *` |
|        - |  3303 | ` * Perform a store (Assignment) operation.` |
|        - |  3304 | ` */` |
|    78436 |  3305 | `case PH7_OP_STORE: {` |
|        - |  3306 | `	ph7_value *pObj;` |
|        - |  3307 | `	SyString sName;` |
|        - |  3308 | `#ifdef UNTRUST` |
|        - |  3309 | `	if( pTos < pStack ){` |
|        - |  3310 | `		goto Abort;` |
|        - |  3311 | `	}` |
|        - |  3312 | `#endif` |
|   156874 |  3313 | `	if( pInstr->iP2 ){` |
|        - |  3314 | `		sxu32 nIdx;` |
|        - |  3315 | `		/* Member store operation */` |
|      538 |  3316 | `		nIdx = pTos->nIdx;` |
|      538 |  3317 | `		VmPopOperand(&pTos,1);` |
|      538 |  3318 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  3319 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  3320 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  3321 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  3322 | `		}else{` |
|        - |  3323 | `			/* Point to the desired memory object */` |
|      534 |  3324 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      534 |  3325 | `			if( pObj ){` |
|        - |  3326 | `				/* Perform the store operation */` |
|      534 |  3327 | `				PH7_MemObjStore(pTos,pObj);` |
|      266 |  3328 | `			}` |
|        - |  3329 | `		}` |
|    78706 |  3330 | `		break;` |
|   156338 |  3331 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  3332 | `		/* Take the variable name from the next on the stack */` |
|        7 |  3333 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  3334 | `			/* Force a string cast */` |
|      ! 0 |  3335 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3336 | `		}` |
|        7 |  3337 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  3338 | `		pTos--;` |
|        - |  3339 | `#ifdef UNTRUST` |
|        - |  3340 | `		if( pTos < pStack  ){` |
|        - |  3341 | `			goto Abort;` |
|        - |  3342 | `		}` |
|        - |  3343 | `#endif` |
|        4 |  3344 | `	}else{` |
|   156332 |  3345 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3346 | `	}` |
|        - |  3347 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   156338 |  3348 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   156338 |  3349 | `	if( pObj == 0 ){` |
|      ! 0 |  3350 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3351 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3352 | `		goto Abort;` |
|        - |  3353 | `	}` |
|   156338 |  3354 | `	if( !pInstr->p3 ){` |
|        7 |  3355 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3356 | `	}` |
|        - |  3357 | `	/* Perform the store operation */` |
|   156338 |  3358 | `	PH7_MemObjStore(pTos,pObj);` |
|   156338 |  3359 | `	break;` |
|        - |  3360 | `				   }` |
|        - |  3361 | `/*` |
|        - |  3362 | ` * STORE_IDX:   P1 * P3` |
|        - |  3363 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3364 | ` *` |
|        - |  3365 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3366 | ` */` |
|    68266 |  3367 | `case PH7_OP_STORE_IDX:` |
|        - |  3368 | `case PH7_OP_STORE_IDX_REF: {` |
|   136534 |  3369 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3370 | `	ph7_value *pKey;` |
|        - |  3371 | `	sxu32 nIdx;` |
|   136534 |  3372 | `	if( pInstr->iP1 ){` |
|        - |  3373 | `		/* Key is next on stack */` |
|    50728 |  3374 | `		pKey = pTos;` |
|    50728 |  3375 | `		pTos--;` |
|    25365 |  3376 | `	}else{` |
|    85808 |  3377 | `		pKey = 0;` |
|        - |  3378 | `	}` |
|   136534 |  3379 | `	nIdx = pTos->nIdx;` |
|   136534 |  3380 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3381 | `		/* Hashmap already loaded */` |
|   136482 |  3382 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   136482 |  3383 | `		if( pMap->iRef < 2 ){` |
|        - |  3384 | `			/* TICKET 1433-48: Prevent garbage collection */` |
|      ! 0 |  3385 | `			pMap->iRef = 2;` |
|      ! 0 |  3386 | `		}` |
|    68242 |  3387 | `	}else{` |
|        - |  3388 | `		ph7_value *pObj;` |
|       53 |  3389 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  3390 | `		if( pObj == 0 ){` |
|      ! 0 |  3391 | `			if( pKey ){` |
|      ! 0 |  3392 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  3393 | `			}` |
|      ! 0 |  3394 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  3395 | `			break;` |
|        - |  3396 | `		}` |
|        - |  3397 | `		/* Phase#1: Load the array */` |
|       53 |  3398 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  3399 | `			VmPopOperand(&pTos,1);` |
|       53 |  3400 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  3401 | `				/* Force a string cast */` |
|      ! 0 |  3402 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  3403 | `			}` |
|       53 |  3404 | `			if( pKey == 0 ){` |
|        - |  3405 | `				/* Append string */` |
|        3 |  3406 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  3407 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  3408 | `				}` |
|        2 |  3409 | `			}else{` |
|        - |  3410 | `				sxu32 nOfft;` |
|       51 |  3411 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  3412 | `					/* Force an int cast */` |
|       51 |  3413 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  3414 | `				}` |
|       51 |  3415 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  3416 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  3417 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  3418 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  3419 | `					zData[nOfft] = zBlob[0];` |
|       26 |  3420 | `				}else{` |
|      ! 0 |  3421 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  3422 | `						/* Perform an append operation */` |
|      ! 0 |  3423 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  3424 | `					}` |
|        - |  3425 | `				}` |
|        - |  3426 | `			}` |
|       53 |  3427 | `			if( pKey ){` |
|       51 |  3428 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  3429 | `			}` |
|       53 |  3430 | `			break;` |
|      ! 0 |  3431 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  3432 | `			/* Force a hashmap cast  */` |
|      ! 0 |  3433 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3434 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3435 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  3436 | `				goto Abort;` |
|        - |  3437 | `			}` |
|      ! 0 |  3438 | `		}` |
|      ! 0 |  3439 | `		pMap = (ph7_hashmap *)pObj->x.pOther;` |
|        - |  3440 | `	}` |
|   136482 |  3441 | `	VmPopOperand(&pTos,1);` |
|        - |  3442 | `	/* Phase#2: Perform the insertion */` |
|   136482 |  3443 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3444 | `		/* Insertion by reference */` |
|       13 |  3445 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        7 |  3446 | `	}else{` |
|   136470 |  3447 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3448 | `	}` |
|   136482 |  3449 | `	if( pKey ){` |
|    50678 |  3450 | `		PH7_MemObjRelease(pKey);` |
|    25338 |  3451 | `	}` |
|   136482 |  3452 | `	break;` |
|        - |  3453 | `					   }` |
|        - |  3454 | `/*` |
|        - |  3455 | ` * INCR: P1 * *` |
|        - |  3456 | ` *` |
|        - |  3457 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3458 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3459 | ` * the stack and increment after that.` |
|        - |  3460 | ` */` |
|    93888 |  3461 | `case PH7_OP_INCR:` |
|        - |  3462 | `#ifdef UNTRUST` |
|        - |  3463 | `	if( pTos < pStack ){` |
|        - |  3464 | `		goto Abort;` |
|        - |  3465 | `	}` |
|        - |  3466 | `#endif` |
|   187822 |  3467 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   187822 |  3468 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3469 | `			ph7_value *pObj;` |
|   187822 |  3470 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3471 | `				/* Force a numeric cast */` |
|   187822 |  3472 | `				PH7_MemObjToNumeric(pObj);` |
|   187822 |  3473 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3474 | `					pObj->rVal++;` |
|        - |  3475 | `					/* Try to get an integer representation */` |
|      ! 0 |  3476 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3477 | `				}else{` |
|   187822 |  3478 | `					pObj->x.iVal++;` |
|   187822 |  3479 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3480 | `				}` |
|   187822 |  3481 | `				if( pInstr->iP1 ){` |
|        - |  3482 | `					/* Pre-icrement */` |
|       55 |  3483 | `					PH7_MemObjStore(pObj,pTos);` |
|       27 |  3484 | `				}` |
|    93932 |  3485 | `			}` |
|    93934 |  3486 | `		}else{` |
|      ! 0 |  3487 | `			if( pInstr->iP1 ){` |
|        - |  3488 | `				/* Force a numeric cast */` |
|      ! 0 |  3489 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  3490 | `				/* Pre-increment */` |
|      ! 0 |  3491 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3492 | `					pTos->rVal++;` |
|        - |  3493 | `					/* Try to get an integer representation */` |
|      ! 0 |  3494 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3495 | `				}else{` |
|      ! 0 |  3496 | `					pTos->x.iVal++;` |
|      ! 0 |  3497 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3498 | `				}` |
|      ! 0 |  3499 | `			}` |
|        - |  3500 | `		}` |
|    93932 |  3501 | `	}` |
|   187822 |  3502 | `	break;` |
|        - |  3503 | `/*` |
|        - |  3504 | ` * DECR: P1 * *` |
|        - |  3505 | ` *` |
|        - |  3506 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  3507 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  3508 | ` * and decrement after that.` |
|        - |  3509 | ` */` |
|        2 |  3510 | `case PH7_OP_DECR:` |
|        - |  3511 | `#ifdef UNTRUST` |
|        - |  3512 | `	if( pTos < pStack ){` |
|        - |  3513 | `		goto Abort;` |
|        - |  3514 | `	}` |
|        - |  3515 | `#endif` |
|        5 |  3516 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  3517 | `		/* Force a numeric cast */` |
|        5 |  3518 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  3519 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3520 | `			ph7_value *pObj;` |
|        5 |  3521 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3522 | `				/* Force a numeric cast */` |
|        5 |  3523 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  3524 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3525 | `					pObj->rVal--;` |
|        - |  3526 | `					/* Try to get an integer representation */` |
|      ! 0 |  3527 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3528 | `				}else{` |
|        5 |  3529 | `					pObj->x.iVal--;` |
|        5 |  3530 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3531 | `				}` |
|        5 |  3532 | `				if( pInstr->iP1 ){` |
|        - |  3533 | `					/* Pre-icrement */` |
|      ! 0 |  3534 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  3535 | `				}` |
|        2 |  3536 | `			}` |
|        3 |  3537 | `		}else{` |
|      ! 0 |  3538 | `			if( pInstr->iP1 ){` |
|        - |  3539 | `				/* Pre-increment */` |
|      ! 0 |  3540 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3541 | `					pTos->rVal--;` |
|        - |  3542 | `					/* Try to get an integer representation */` |
|      ! 0 |  3543 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3544 | `				}else{` |
|      ! 0 |  3545 | `					pTos->x.iVal--;` |
|      ! 0 |  3546 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3547 | `				}` |
|      ! 0 |  3548 | `			}` |
|        - |  3549 | `		}` |
|        2 |  3550 | `	}` |
|        5 |  3551 | `	break;` |
|        - |  3552 | `/*` |
|        - |  3553 | ` * UMINUS: * * *` |
|        - |  3554 | ` *` |
|        - |  3555 | ` * Perform a unary minus operation.` |
|        - |  3556 | ` */` |
|    17047 |  3557 | `case PH7_OP_UMINUS:` |
|        - |  3558 | `#ifdef UNTRUST` |
|        - |  3559 | `	if( pTos < pStack ){` |
|        - |  3560 | `		goto Abort;` |
|        - |  3561 | `	}` |
|        - |  3562 | `#endif` |
|        - |  3563 | `	/* Force a numeric (integer,real or both) cast */` |
|    34096 |  3564 | `	PH7_MemObjToNumeric(pTos);` |
|    34096 |  3565 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       19 |  3566 | `		pTos->rVal = -pTos->rVal;` |
|        9 |  3567 | `	}` |
|    34096 |  3568 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    34078 |  3569 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    17038 |  3570 | `	}` |
|    34096 |  3571 | `	break;` |
|        - |  3572 | `/*` |
|        - |  3573 | ` * UPLUS: * * *` |
|        - |  3574 | ` *` |
|        - |  3575 | ` * Perform a unary plus operation.` |
|        - |  3576 | ` */` |
|       16 |  3577 | `case PH7_OP_UPLUS:` |
|        - |  3578 | `#ifdef UNTRUST` |
|        - |  3579 | `	if( pTos < pStack ){` |
|        - |  3580 | `		goto Abort;` |
|        - |  3581 | `	}` |
|        - |  3582 | `#endif` |
|        - |  3583 | `	/* Force a numeric (integer,real or both) cast */` |
|       33 |  3584 | `	PH7_MemObjToNumeric(pTos);` |
|       33 |  3585 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3586 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  3587 | `	}` |
|       33 |  3588 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       33 |  3589 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       16 |  3590 | `	}` |
|       33 |  3591 | `	break;` |
|        - |  3592 | `/*` |
|        - |  3593 | ` * OP_LNOT: * * *` |
|        - |  3594 | ` *` |
|        - |  3595 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  3596 | ` * with its complement.` |
|        - |  3597 | ` */` |
|    26859 |  3598 | `case PH7_OP_LNOT:` |
|        - |  3599 | `#ifdef UNTRUST` |
|        - |  3600 | `	if( pTos < pStack ){` |
|        - |  3601 | `		goto Abort;` |
|        - |  3602 | `	}` |
|        - |  3603 | `#endif` |
|        - |  3604 | `	/* Force a boolean cast */` |
|    53764 |  3605 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  3606 | `		PH7_MemObjToBool(pTos);` |
|       10 |  3607 | `	}` |
|    53764 |  3608 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    53764 |  3609 | `	break;` |
|        - |  3610 | `/*` |
|        - |  3611 | ` * OP_BITNOT: * * *` |
|        - |  3612 | ` *` |
|        - |  3613 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  3614 | ` * with its ones-complement.` |
|        - |  3615 | ` */` |
|        3 |  3616 | `case PH7_OP_BITNOT:` |
|        - |  3617 | `#ifdef UNTRUST` |
|        - |  3618 | `	if( pTos < pStack ){` |
|        - |  3619 | `		goto Abort;` |
|        - |  3620 | `	}` |
|        - |  3621 | `#endif` |
|        - |  3622 | `	/* Force an integer cast */` |
|        7 |  3623 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3624 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3625 | `	}` |
|        7 |  3626 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|        7 |  3627 | `	break;` |
|        - |  3628 | `/* OP_MUL * * *` |
|        - |  3629 | ` * OP_MUL_STORE * * *` |
|        - |  3630 | ` *` |
|        - |  3631 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  3632 | ` * and push the result back onto the stack.` |
|        - |  3633 | ` */` |
|     1231 |  3634 | `case PH7_OP_MUL:` |
|        - |  3635 | `case PH7_OP_MUL_STORE: {` |
|     2464 |  3636 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3637 | `	/* Force the operand to be numeric */` |
|        - |  3638 | `#ifdef UNTRUST` |
|        - |  3639 | `	if( pNos < pStack ){` |
|        - |  3640 | `		goto Abort;` |
|        - |  3641 | `	}` |
|        - |  3642 | `#endif` |
|     2464 |  3643 | `	PH7_MemObjToNumeric(pTos);` |
|     2464 |  3644 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  3645 | `	/* Perform the requested operation */` |
|     2464 |  3646 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3647 | `		/* Floating point arithemic */` |
|        - |  3648 | `		ph7_real a,b,r;` |
|       17 |  3649 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3650 | `			PH7_MemObjToReal(pTos);` |
|        3 |  3651 | `		}` |
|       17 |  3652 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3653 | `			PH7_MemObjToReal(pNos);` |
|        3 |  3654 | `		}` |
|       17 |  3655 | `		a = pNos->rVal;` |
|       17 |  3656 | `		b = pTos->rVal;` |
|       17 |  3657 | `		r = a * b;` |
|        - |  3658 | `		/* Push the result */` |
|       17 |  3659 | `		pNos->rVal = r;` |
|       17 |  3660 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3661 | `		/* Try to get an integer representation */` |
|       17 |  3662 | `		PH7_MemObjTryInteger(pNos);` |
|        9 |  3663 | `	}else{` |
|        - |  3664 | `		/* Integer arithmetic */` |
|        - |  3665 | `		sxi64 a,b,r;` |
|     2448 |  3666 | `		a = pNos->x.iVal;` |
|     2448 |  3667 | `		b = pTos->x.iVal;` |
|     2448 |  3668 | `		r = a * b;` |
|        - |  3669 | `		/* Push the result */` |
|     2448 |  3670 | `		pNos->x.iVal = r;` |
|     2448 |  3671 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3672 | `	}` |
|     2464 |  3673 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  3674 | `		ph7_value *pObj;` |
|       19 |  3675 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3676 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  3677 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  3678 | `			PH7_MemObjStore(pNos,pObj);` |
|        9 |  3679 | `		}` |
|        9 |  3680 | `	}` |
|     2464 |  3681 | `	VmPopOperand(&pTos,1);` |
|     2464 |  3682 | `	break;` |
|        - |  3683 | `				 }` |
|        - |  3684 | `/* OP_ADD * * *` |
|        - |  3685 | ` *` |
|        - |  3686 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3687 | ` * and push the result back onto the stack.` |
|        - |  3688 | ` */` |
|      424 |  3689 | `case PH7_OP_ADD:{` |
|      850 |  3690 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3691 | `#ifdef UNTRUST` |
|        - |  3692 | `	if( pNos < pStack ){` |
|        - |  3693 | `		goto Abort;` |
|        - |  3694 | `	}` |
|        - |  3695 | `#endif` |
|        - |  3696 | `	/* Perform the addition */` |
|      850 |  3697 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      850 |  3698 | `	VmPopOperand(&pTos,1);` |
|      850 |  3699 | `	break;` |
|        - |  3700 | `				}` |
|        - |  3701 | `/*` |
|        - |  3702 | ` * OP_ADD_STORE * * *` |
|        - |  3703 | ` *` |
|        - |  3704 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3705 | ` * and push the result back onto the stack.` |
|        - |  3706 | ` */` |
|      481 |  3707 | `case PH7_OP_ADD_STORE:{` |
|      963 |  3708 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3709 | `	ph7_value *pObj;` |
|        - |  3710 | `	sxu32 nIdx;` |
|        - |  3711 | `#ifdef UNTRUST` |
|        - |  3712 | `	if( pNos < pStack ){` |
|        - |  3713 | `		goto Abort;` |
|        - |  3714 | `	}` |
|        - |  3715 | `#endif` |
|        - |  3716 | `	/* Perform the addition */` |
|      963 |  3717 | `	nIdx = pTos->nIdx;` |
|      963 |  3718 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  3719 | `	/* Peform the store operation */` |
|      963 |  3720 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  3721 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      963 |  3722 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      963 |  3723 | `		PH7_MemObjStore(pTos,pObj);` |
|      481 |  3724 | `	}` |
|        - |  3725 | `	/* Ticket 1433-35: Perform a stack dup */` |
|      963 |  3726 | `	PH7_MemObjStore(pTos,pNos);` |
|      963 |  3727 | `	VmPopOperand(&pTos,1);` |
|      963 |  3728 | `	break;` |
|        - |  3729 | `				}` |
|        - |  3730 | `/* OP_SUB * * *` |
|        - |  3731 | ` *` |
|        - |  3732 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3733 | ` * first (what was next on the stack) from the second (the` |
|        - |  3734 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3735 | ` */` |
|      280 |  3736 | `case PH7_OP_SUB: {` |
|      561 |  3737 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3738 | `#ifdef UNTRUST` |
|        - |  3739 | `	if( pNos < pStack ){` |
|        - |  3740 | `		goto Abort;` |
|        - |  3741 | `	}` |
|        - |  3742 | `#endif` |
|      561 |  3743 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3744 | `		/* Floating point arithemic */` |
|        - |  3745 | `		ph7_real a,b,r;` |
|       73 |  3746 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3747 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3748 | `		}` |
|       73 |  3749 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  3750 | `			PH7_MemObjToReal(pNos);` |
|        2 |  3751 | `		}` |
|       73 |  3752 | `		a = pNos->rVal;` |
|       73 |  3753 | `		b = pTos->rVal;` |
|       73 |  3754 | `		r = a - b;` |
|        - |  3755 | `		/* Push the result */` |
|       73 |  3756 | `		pNos->rVal = r;` |
|       73 |  3757 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3758 | `		/* Try to get an integer representation */` |
|       73 |  3759 | `		PH7_MemObjTryInteger(pNos);` |
|       37 |  3760 | `	}else{` |
|        - |  3761 | `		/* Integer arithmetic */` |
|        - |  3762 | `		sxi64 a,b,r;` |
|      489 |  3763 | `		a = pNos->x.iVal;` |
|      489 |  3764 | `		b = pTos->x.iVal;` |
|      489 |  3765 | `		r = a - b;` |
|        - |  3766 | `		/* Push the result */` |
|      489 |  3767 | `		pNos->x.iVal = r;` |
|      489 |  3768 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3769 | `	}` |
|      561 |  3770 | `	VmPopOperand(&pTos,1);` |
|      561 |  3771 | `	break;` |
|        - |  3772 | `				 }` |
|        - |  3773 | `/* OP_SUB_STORE * * *` |
|        - |  3774 | ` *` |
|        - |  3775 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3776 | ` * first (what was next on the stack) from the second (the` |
|        - |  3777 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3778 | ` */` |
|        1 |  3779 | `case PH7_OP_SUB_STORE: {` |
|        3 |  3780 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3781 | `	ph7_value *pObj;` |
|        - |  3782 | `#ifdef UNTRUST` |
|        - |  3783 | `	if( pNos < pStack ){` |
|        - |  3784 | `		goto Abort;` |
|        - |  3785 | `	}` |
|        - |  3786 | `#endif` |
|        3 |  3787 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3788 | `		/* Floating point arithemic */` |
|        - |  3789 | `		ph7_real a,b,r;` |
|      ! 0 |  3790 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3791 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3792 | `		}` |
|      ! 0 |  3793 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3794 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  3795 | `		}` |
|      ! 0 |  3796 | `		a = pTos->rVal;` |
|      ! 0 |  3797 | `		b = pNos->rVal;` |
|      ! 0 |  3798 | `		r = a - b;` |
|        - |  3799 | `		/* Push the result */` |
|      ! 0 |  3800 | `		pNos->rVal = r;` |
|      ! 0 |  3801 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3802 | `		/* Try to get an integer representation */` |
|      ! 0 |  3803 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  3804 | `	}else{` |
|        - |  3805 | `		/* Integer arithmetic */` |
|        - |  3806 | `		sxi64 a,b,r;` |
|        3 |  3807 | `		a = pTos->x.iVal;` |
|        3 |  3808 | `		b = pNos->x.iVal;` |
|        3 |  3809 | `		r = a - b;` |
|        - |  3810 | `		/* Push the result */` |
|        3 |  3811 | `		pNos->x.iVal = r;` |
|        3 |  3812 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3813 | `	}` |
|        3 |  3814 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3815 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3816 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3817 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3818 | `	}` |
|        3 |  3819 | `	VmPopOperand(&pTos,1);` |
|        3 |  3820 | `	break;` |
|        - |  3821 | `				 }` |
|        - |  3822 |  |
|        - |  3823 | `/*` |
|        - |  3824 | ` * OP_MOD * * *` |
|        - |  3825 | ` *` |
|        - |  3826 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3827 | ` * first (what was next on the stack) from the second (the` |
|        - |  3828 | ` * top of the stack) and push the remainder after division` |
|        - |  3829 | ` * onto the stack.` |
|        - |  3830 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3831 | ` */` |
|      296 |  3832 | `case PH7_OP_MOD:{` |
|      594 |  3833 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3834 | `	sxi64 a,b,r;` |
|        - |  3835 | `#ifdef UNTRUST` |
|        - |  3836 | `	if( pNos < pStack ){` |
|        - |  3837 | `		goto Abort;` |
|        - |  3838 | `	}` |
|        - |  3839 | `#endif` |
|        - |  3840 | `	/* Force the operands to be integer */` |
|      594 |  3841 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3842 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3843 | `	}` |
|      594 |  3844 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  3845 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  3846 | `	}` |
|        - |  3847 | `	/* Perform the requested operation */` |
|      594 |  3848 | `	a = pNos->x.iVal;` |
|      594 |  3849 | `	b = pTos->x.iVal;` |
|      594 |  3850 | `	if( b == 0 ){` |
|        3 |  3851 | `		r = 0;` |
|        3 |  3852 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3853 | `		/* goto Abort; */` |
|        2 |  3854 | `	}else{` |
|      591 |  3855 | `		r = a%b;` |
|        - |  3856 | `	}` |
|        - |  3857 | `	/* Push the result */` |
|      594 |  3858 | `	pNos->x.iVal = r;` |
|      594 |  3859 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      594 |  3860 | `	VmPopOperand(&pTos,1);` |
|      594 |  3861 | `	break;` |
|        - |  3862 | `				}` |
|        - |  3863 | `/*` |
|        - |  3864 | ` * OP_MOD_STORE * * *` |
|        - |  3865 | ` *` |
|        - |  3866 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3867 | ` * first (what was next on the stack) from the second (the` |
|        - |  3868 | ` * top of the stack) and push the remainder after division` |
|        - |  3869 | ` * onto the stack.` |
|        - |  3870 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3871 | ` */` |
|        1 |  3872 | `case PH7_OP_MOD_STORE: {` |
|        3 |  3873 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3874 | `	ph7_value *pObj;` |
|        - |  3875 | `	sxi64 a,b,r;` |
|        - |  3876 | `#ifdef UNTRUST` |
|        - |  3877 | `	if( pNos < pStack ){` |
|        - |  3878 | `		goto Abort;` |
|        - |  3879 | `	}` |
|        - |  3880 | `#endif` |
|        - |  3881 | `	/* Force the operands to be integer */` |
|        3 |  3882 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3883 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3884 | `	}` |
|        3 |  3885 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3886 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  3887 | `	}` |
|        - |  3888 | `	/* Perform the requested operation */` |
|        3 |  3889 | `	a = pTos->x.iVal;` |
|        3 |  3890 | `	b = pNos->x.iVal;` |
|        3 |  3891 | `	if( b == 0 ){` |
|      ! 0 |  3892 | `		r = 0;` |
|      ! 0 |  3893 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3894 | `		/* goto Abort; */` |
|      ! 0 |  3895 | `	}else{` |
|        3 |  3896 | `		r = a%b;` |
|        - |  3897 | `	}` |
|        - |  3898 | `	/* Push the result */` |
|        3 |  3899 | `	pNos->x.iVal = r;` |
|        3 |  3900 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  3901 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3902 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3903 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3904 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3905 | `	}` |
|        3 |  3906 | `	VmPopOperand(&pTos,1);` |
|        3 |  3907 | `	break;` |
|        - |  3908 | `				}` |
|        - |  3909 | `/*` |
|        - |  3910 | ` * OP_DIV * * *` |
|        - |  3911 | ` *` |
|        - |  3912 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3913 | ` * first (what was next on the stack) from the second (the` |
|        - |  3914 | ` * top of the stack) and push the result onto the stack.` |
|        - |  3915 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  3916 | ` */` |
|       28 |  3917 | `case PH7_OP_DIV:{` |
|       58 |  3918 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3919 | `	ph7_real a,b,r;` |
|        - |  3920 | `#ifdef UNTRUST` |
|        - |  3921 | `	if( pNos < pStack ){` |
|        - |  3922 | `		goto Abort;` |
|        - |  3923 | `	}` |
|        - |  3924 | `#endif` |
|        - |  3925 | `	/* Force the operands to be real */` |
|       58 |  3926 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       54 |  3927 | `		PH7_MemObjToReal(pTos);` |
|       26 |  3928 | `	}` |
|       58 |  3929 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       20 |  3930 | `		PH7_MemObjToReal(pNos);` |
|        9 |  3931 | `	}` |
|        - |  3932 | `	/* Perform the requested operation */` |
|       58 |  3933 | `	a = pNos->rVal;` |
|       58 |  3934 | `	b = pTos->rVal;` |
|       58 |  3935 | `	if( b == 0 ){` |
|        - |  3936 | `		/* Division by zero */` |
|        3 |  3937 | `		pNos->rVal = 0;` |
|        3 |  3938 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  3939 | `		/* goto Abort; */` |
|        2 |  3940 | `	}else{` |
|       55 |  3941 | `		r = a/b;` |
|        - |  3942 | `		/* Push the result */` |
|       55 |  3943 | `		pNos->rVal = r;` |
|       55 |  3944 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3945 | `		/* Try to get an integer representation */` |
|       55 |  3946 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  3947 | `	}` |
|       58 |  3948 | `	VmPopOperand(&pTos,1);` |
|       58 |  3949 | `	break;` |
|        - |  3950 | `				}` |
|        - |  3951 | `/*` |
|        - |  3952 | ` * OP_DIV_STORE * * *` |
|        - |  3953 | ` *` |
|        - |  3954 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3955 | ` * first (what was next on the stack) from the second (the` |
|        - |  3956 | ` * top of the stack) and push the result onto the stack.` |
|        - |  3957 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  3958 | ` */` |
|        1 |  3959 | `case PH7_OP_DIV_STORE:{` |
|        3 |  3960 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3961 | `	ph7_value *pObj;` |
|        - |  3962 | `	ph7_real a,b,r;` |
|        - |  3963 | `#ifdef UNTRUST` |
|        - |  3964 | `	if( pNos < pStack ){` |
|        - |  3965 | `		goto Abort;` |
|        - |  3966 | `	}` |
|        - |  3967 | `#endif` |
|        - |  3968 | `	/* Force the operands to be real */` |
|        3 |  3969 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  3970 | `		PH7_MemObjToReal(pTos);` |
|        1 |  3971 | `	}` |
|        3 |  3972 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  3973 | `		PH7_MemObjToReal(pNos);` |
|        1 |  3974 | `	}` |
|        - |  3975 | `	/* Perform the requested operation */` |
|        3 |  3976 | `	a = pTos->rVal;` |
|        3 |  3977 | `	b = pNos->rVal;` |
|        3 |  3978 | `	if( b == 0 ){` |
|        - |  3979 | `		/* Division by zero */` |
|      ! 0 |  3980 | `		r = 0;` |
|      ! 0 |  3981 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  3982 | `		/* goto Abort; */` |
|      ! 0 |  3983 | `	}else{` |
|        3 |  3984 | `		r = a/b;` |
|        - |  3985 | `		/* Push the result */` |
|        3 |  3986 | `		pNos->rVal = r;` |
|        3 |  3987 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3988 | `		/* Try to get an integer representation */` |
|        3 |  3989 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  3990 | `	}` |
|        3 |  3991 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3992 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3993 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3994 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3995 | `	}` |
|        3 |  3996 | `	VmPopOperand(&pTos,1);` |
|        3 |  3997 | `	break;` |
|        - |  3998 | `				}` |
|        - |  3999 | `/* OP_BAND * * *` |
|        - |  4000 | ` *` |
|        - |  4001 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4002 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4003 | ` * two elements.` |
|        - |  4004 | `*/` |
|        - |  4005 | `/* OP_BOR * * *` |
|        - |  4006 | ` *` |
|        - |  4007 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4008 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4009 | ` * two elements.` |
|        - |  4010 | ` */` |
|        - |  4011 | `/* OP_BXOR * * *` |
|        - |  4012 | ` *` |
|        - |  4013 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4014 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4015 | ` * two elements.` |
|        - |  4016 | ` */` |
|       19 |  4017 | `case PH7_OP_BAND:` |
|        - |  4018 | `case PH7_OP_BOR:` |
|        - |  4019 | `case PH7_OP_BXOR:{` |
|       39 |  4020 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4021 | `	sxi64 a,b,r;` |
|        - |  4022 | `#ifdef UNTRUST` |
|        - |  4023 | `	if( pNos < pStack ){` |
|        - |  4024 | `		goto Abort;` |
|        - |  4025 | `	}` |
|        - |  4026 | `#endif` |
|        - |  4027 | `	/* Force the operands to be integer */` |
|       39 |  4028 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4029 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4030 | `	}` |
|       39 |  4031 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4032 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4033 | `	}` |
|        - |  4034 | `	/* Perform the requested operation */` |
|       39 |  4035 | `	a = pNos->x.iVal;` |
|       39 |  4036 | `	b = pTos->x.iVal;` |
|       39 |  4037 | `	switch(pInstr->iOp){` |
|        6 |  4038 | `	case PH7_OP_BOR_STORE:` |
|       13 |  4039 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        6 |  4040 | `	case PH7_OP_BXOR_STORE:` |
|       13 |  4041 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        7 |  4042 | `	case PH7_OP_BAND_STORE:` |
|        7 |  4043 | `	case PH7_OP_BAND:` |
|       15 |  4044 | `	default:          r = a&b; break;` |
|        - |  4045 | `	}` |
|        - |  4046 | `	/* Push the result */` |
|       39 |  4047 | `	pNos->x.iVal = r;` |
|       39 |  4048 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       39 |  4049 | `	VmPopOperand(&pTos,1);` |
|       39 |  4050 | `	break;` |
|        - |  4051 | `				 }` |
|        - |  4052 | `/* OP_BAND_STORE * * *` |
|        - |  4053 | ` *` |
|        - |  4054 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4055 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4056 | ` * two elements.` |
|        - |  4057 | `*/` |
|        - |  4058 | `/* OP_BOR_STORE * * *` |
|        - |  4059 | ` *` |
|        - |  4060 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4061 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4062 | ` * two elements.` |
|        - |  4063 | ` */` |
|        - |  4064 | `/* OP_BXOR_STORE * * *` |
|        - |  4065 | ` *` |
|        - |  4066 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4067 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4068 | ` * two elements.` |
|        - |  4069 | ` */` |
|        7 |  4070 | `case PH7_OP_BAND_STORE:` |
|        - |  4071 | `case PH7_OP_BOR_STORE:` |
|        - |  4072 | `case PH7_OP_BXOR_STORE:{` |
|       15 |  4073 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4074 | `	ph7_value *pObj;` |
|        - |  4075 | `	sxi64 a,b,r;` |
|        - |  4076 | `#ifdef UNTRUST` |
|        - |  4077 | `	if( pNos < pStack ){` |
|        - |  4078 | `		goto Abort;` |
|        - |  4079 | `	}` |
|        - |  4080 | `#endif` |
|        - |  4081 | `	/* Force the operands to be integer */` |
|       15 |  4082 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4083 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4084 | `	}` |
|       15 |  4085 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4086 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4087 | `	}` |
|        - |  4088 | `	/* Perform the requested operation */` |
|       15 |  4089 | `	a = pTos->x.iVal;` |
|       15 |  4090 | `	b = pNos->x.iVal;` |
|       15 |  4091 | `	switch(pInstr->iOp){` |
|        2 |  4092 | `	case PH7_OP_BOR_STORE:` |
|        5 |  4093 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        3 |  4094 | `	case PH7_OP_BXOR_STORE:` |
|        7 |  4095 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        2 |  4096 | `	case PH7_OP_BAND_STORE:` |
|        2 |  4097 | `	case PH7_OP_BAND:` |
|        5 |  4098 | `	default:          r = a&b; break;` |
|        - |  4099 | `	}` |
|        - |  4100 | `	/* Push the result */` |
|       15 |  4101 | `	pNos->x.iVal = r;` |
|       15 |  4102 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4103 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4104 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4105 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4106 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4107 | `	}` |
|       15 |  4108 | `	VmPopOperand(&pTos,1);` |
|       15 |  4109 | `	break;` |
|        - |  4110 | `				 }` |
|        - |  4111 | `/* OP_SHL * * *` |
|        - |  4112 | ` *` |
|        - |  4113 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4114 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4115 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4116 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4117 | ` */` |
|        - |  4118 | `/* OP_SHR * * *` |
|        - |  4119 | ` *` |
|        - |  4120 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4121 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4122 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4123 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4124 | ` */` |
|        9 |  4125 | `case PH7_OP_SHL:` |
|        - |  4126 | `case PH7_OP_SHR: {` |
|       19 |  4127 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4128 | `	sxi64 a,r;` |
|        - |  4129 | `	sxi32 b;` |
|        - |  4130 | `#ifdef UNTRUST` |
|        - |  4131 | `	if( pNos < pStack ){` |
|        - |  4132 | `		goto Abort;` |
|        - |  4133 | `	}` |
|        - |  4134 | `#endif` |
|        - |  4135 | `	/* Force the operands to be integer */` |
|       19 |  4136 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4137 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4138 | `	}` |
|       19 |  4139 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4140 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4141 | `	}` |
|        - |  4142 | `	/* Perform the requested operation */` |
|       19 |  4143 | `	a = pNos->x.iVal;` |
|       19 |  4144 | `	b = (sxi32)pTos->x.iVal;` |
|       19 |  4145 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       11 |  4146 | `		r = a << b;` |
|        6 |  4147 | `	}else{` |
|        9 |  4148 | `		r = a >> b;` |
|        - |  4149 | `	}` |
|        - |  4150 | `	/* Push the result */` |
|       19 |  4151 | `	pNos->x.iVal = r;` |
|       19 |  4152 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  4153 | `	VmPopOperand(&pTos,1);` |
|       19 |  4154 | `	break;` |
|        - |  4155 | `				 }` |
|        - |  4156 | `/*  OP_SHL_STORE * * *` |
|        - |  4157 | ` *` |
|        - |  4158 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4159 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4160 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4161 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4162 | ` */` |
|        - |  4163 | `/* OP_SHR_STORE * * *` |
|        - |  4164 | ` *` |
|        - |  4165 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4166 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4167 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4168 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4169 | ` */` |
|        7 |  4170 | `case PH7_OP_SHL_STORE:` |
|        - |  4171 | `case PH7_OP_SHR_STORE: {` |
|       15 |  4172 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4173 | `	ph7_value *pObj;` |
|        - |  4174 | `	sxi64 a,r;` |
|        - |  4175 | `	sxi32 b;` |
|        - |  4176 | `#ifdef UNTRUST` |
|        - |  4177 | `	if( pNos < pStack ){` |
|        - |  4178 | `		goto Abort;` |
|        - |  4179 | `	}` |
|        - |  4180 | `#endif` |
|        - |  4181 | `	/* Force the operands to be integer */` |
|       15 |  4182 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4183 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4184 | `	}` |
|       15 |  4185 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4186 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4187 | `	}` |
|        - |  4188 | `	/* Perform the requested operation */` |
|       15 |  4189 | `	a = pTos->x.iVal;` |
|       15 |  4190 | `	b = (sxi32)pNos->x.iVal;` |
|       15 |  4191 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        7 |  4192 | `		r = a << b;` |
|        4 |  4193 | `	}else{` |
|        9 |  4194 | `		r = a >> b;` |
|        - |  4195 | `	}` |
|        - |  4196 | `	/* Push the result */` |
|       15 |  4197 | `	pNos->x.iVal = r;` |
|       15 |  4198 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4199 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4200 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4201 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4202 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4203 | `	}` |
|       15 |  4204 | `	VmPopOperand(&pTos,1);` |
|       15 |  4205 | `	break;` |
|        - |  4206 | `				 }` |
|        - |  4207 | `/* CAT:  P1 * *` |
|        - |  4208 | ` *` |
|        - |  4209 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  4210 | ` * back.` |
|        - |  4211 | ` */` |
|    51807 |  4212 | `case PH7_OP_CAT:{` |
|        - |  4213 | `	ph7_value *pNos,*pCur;` |
|   103616 |  4214 | `	if( pInstr->iP1 < 1 ){` |
|    73918 |  4215 | `		pNos = &pTos[-1];` |
|    36960 |  4216 | `	}else{` |
|    29700 |  4217 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4218 | `	}` |
|        - |  4219 | `#ifdef UNTRUST` |
|        - |  4220 | `	if( pNos < pStack ){` |
|        - |  4221 | `		goto Abort;` |
|        - |  4222 | `	}` |
|        - |  4223 | `#endif` |
|        - |  4224 | `	/* Force a string cast */` |
|   103616 |  4225 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      538 |  4226 | `		PH7_MemObjToString(pNos);` |
|      268 |  4227 | `	}` |
|   103616 |  4228 | `	pCur = &pNos[1];` |
|   217496 |  4229 | `	while( pCur <= pTos ){` |
|   113882 |  4230 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    53218 |  4231 | `			PH7_MemObjToString(pCur);` |
|    26608 |  4232 | `		}` |
|        - |  4233 | `		/* Perform the concatenation */` |
|   113882 |  4234 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   113842 |  4235 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    56920 |  4236 | `		}` |
|   113882 |  4237 | `		SyBlobRelease(&pCur->sBlob);` |
|   113882 |  4238 | `		pCur++;` |
|        2 |  4239 | `	}` |
|   103616 |  4240 | `	pTos = pNos;` |
|   103616 |  4241 | `	break;` |
|        - |  4242 | `				}` |
|        - |  4243 | `/*  CAT_STORE: * * *` |
|        - |  4244 | ` *` |
|        - |  4245 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  4246 | ` * back.` |
|        - |  4247 | ` */` |
|     1215 |  4248 | `case PH7_OP_CAT_STORE:{` |
|     2431 |  4249 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4250 | `	ph7_value *pObj;` |
|        - |  4251 | `#ifdef UNTRUST` |
|        - |  4252 | `	if( pNos < pStack ){` |
|        - |  4253 | `		goto Abort;` |
|        - |  4254 | `	}` |
|        - |  4255 | `#endif` |
|     2431 |  4256 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4257 | `		/* Force a string cast */` |
|      467 |  4258 | `		PH7_MemObjToString(pTos);` |
|      233 |  4259 | `	}` |
|     2431 |  4260 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4261 | `		/* Force a string cast */` |
|      ! 0 |  4262 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4263 | `	}` |
|        - |  4264 | `	/* Perform the concatenation (Reverse order) */` |
|     2431 |  4265 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     2431 |  4266 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     1215 |  4267 | `	}` |
|        - |  4268 | `	/* Perform the store operation */` |
|     2431 |  4269 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4270 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     2431 |  4271 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     2431 |  4272 | `		PH7_MemObjStore(pTos,pObj);` |
|     1215 |  4273 | `	}` |
|     2431 |  4274 | `	PH7_MemObjStore(pTos,pNos);` |
|     2431 |  4275 | `	VmPopOperand(&pTos,1);` |
|     2431 |  4276 | `	break;` |
|        - |  4277 | `				}` |
|        - |  4278 | `/* OP_AND: * * *` |
|        - |  4279 | ` *` |
|        - |  4280 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  4281 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4282 | ` * stack.` |
|        - |  4283 | ` */` |
|        - |  4284 | `/* OP_OR: * * *` |
|        - |  4285 | ` *` |
|        - |  4286 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  4287 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4288 | ` * stack.` |
|        - |  4289 | ` */` |
|    56311 |  4290 | `case PH7_OP_LAND:` |
|        - |  4291 | `case PH7_OP_LOR: {` |
|   112668 |  4292 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4293 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4294 | `#ifdef UNTRUST` |
|        - |  4295 | `	if( pNos < pStack ){` |
|        - |  4296 | `		goto Abort;` |
|        - |  4297 | `	}` |
|        - |  4298 | `#endif` |
|        - |  4299 | `	/* Force a boolean cast */` |
|   112668 |  4300 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4301 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4302 | `	}` |
|   112668 |  4303 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4304 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4305 | `	}` |
|   112668 |  4306 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   112668 |  4307 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   112668 |  4308 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4309 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    59688 |  4310 | `		v1 = and_logic[v1*3+v2];` |
|    29867 |  4311 | `	}else{` |
|        - |  4312 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|    52982 |  4313 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4314 | `	}` |
|   112668 |  4315 | `	if( v1 == 2 ){` |
|      ! 0 |  4316 | `		v1 = 1;` |
|      ! 0 |  4317 | `	}` |
|   112668 |  4318 | `	VmPopOperand(&pTos,1);` |
|   112668 |  4319 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   112668 |  4320 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   112668 |  4321 | `	break;` |
|        - |  4322 | `				 }` |
|        - |  4323 | `/* OP_LXOR: * * *` |
|        - |  4324 | ` *` |
|        - |  4325 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  4326 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4327 | ` * stack.` |
|        - |  4328 | ` * According to the PHP language reference manual:` |
|        - |  4329 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  4330 | ` *  TRUE,but not both.` |
|        - |  4331 | ` */` |
|        5 |  4332 | `case PH7_OP_LXOR:{` |
|       11 |  4333 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  4334 | `	sxi32 v = 0;` |
|        - |  4335 | `#ifdef UNTRUST` |
|        - |  4336 | `	if( pNos < pStack ){` |
|        - |  4337 | `		goto Abort;` |
|        - |  4338 | `	}` |
|        - |  4339 | `#endif` |
|        - |  4340 | `	/* Force a boolean cast */` |
|       11 |  4341 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4342 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4343 | `	}` |
|       11 |  4344 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4345 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4346 | `	}` |
|       11 |  4347 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  4348 | `		v = 1;` |
|        3 |  4349 | `	}` |
|       11 |  4350 | `	VmPopOperand(&pTos,1);` |
|       11 |  4351 | `	pTos->x.iVal = v;` |
|       11 |  4352 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  4353 | `	break;` |
|        - |  4354 | `				 }` |
|        - |  4355 | `/* OP_EQ P1 P2 P3` |
|        - |  4356 | ` *` |
|        - |  4357 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  4358 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4359 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4360 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4361 | ` */` |
|        - |  4362 | `/* OP_NEQ P1 P2 P3` |
|        - |  4363 | ` *` |
|        - |  4364 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  4365 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4366 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4367 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4368 | ` */` |
|     3041 |  4369 | `case PH7_OP_EQ:` |
|        - |  4370 | `case PH7_OP_NEQ: {` |
|     6084 |  4371 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4372 | `	/* Perform the comparison and act accordingly */` |
|        - |  4373 | `#ifdef UNTRUST` |
|        - |  4374 | `	if( pNos < pStack ){` |
|        - |  4375 | `		goto Abort;` |
|        - |  4376 | `	}` |
|        - |  4377 | `#endif` |
|     6084 |  4378 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     6084 |  4379 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       11 |  4380 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     6079 |  4381 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     6052 |  4382 | `		rc = rc == 0;` |
|     3027 |  4383 | `	}else{` |
|       24 |  4384 | `		rc = rc != 0;` |
|        - |  4385 | `	}` |
|     6084 |  4386 | `	VmPopOperand(&pTos,1);` |
|     6084 |  4387 | `	if( !pInstr->iP2 ){` |
|        - |  4388 | `		/* Push comparison result without taking the jump */` |
|     6084 |  4389 | `		PH7_MemObjRelease(pTos);` |
|     6084 |  4390 | `		pTos->x.iVal = rc;` |
|        - |  4391 | `		/* Invalidate any prior representation */` |
|     6084 |  4392 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     3043 |  4393 | `	}else{` |
|      ! 0 |  4394 | `		if( rc ){` |
|        - |  4395 | `			/* Jump to the desired location */` |
|      ! 0 |  4396 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4397 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4398 | `		}` |
|        - |  4399 | `	}` |
|     6084 |  4400 | `	break;` |
|        - |  4401 | `				 }` |
|        - |  4402 | `/* OP_TEQ P1 P2 *` |
|        - |  4403 | ` *` |
|        - |  4404 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4405 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4406 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4407 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4408 | ` */` |
|    83454 |  4409 | `case PH7_OP_TEQ: {` |
|   166910 |  4410 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4411 | `	/* Perform the comparison and act accordingly */` |
|        - |  4412 | `#ifdef UNTRUST` |
|        - |  4413 | `	if( pNos < pStack ){` |
|        - |  4414 | `		goto Abort;` |
|        - |  4415 | `	}` |
|        - |  4416 | `#endif` |
|   166910 |  4417 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   166910 |  4418 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4419 | `		rc = 0;` |
|        2 |  4420 | `	}else{` |
|   166908 |  4421 | `		rc = rc == 0;` |
|        - |  4422 | `	}` |
|   166910 |  4423 | `	VmPopOperand(&pTos,1);` |
|   166910 |  4424 | `	if( !pInstr->iP2 ){` |
|        - |  4425 | `		/* Push comparison result without taking the jump */` |
|   166910 |  4426 | `		PH7_MemObjRelease(pTos);` |
|   166910 |  4427 | `		pTos->x.iVal = rc;` |
|        - |  4428 | `		/* Invalidate any prior representation */` |
|   166910 |  4429 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    83456 |  4430 | `	}else{` |
|      ! 0 |  4431 | `		if( rc ){` |
|        - |  4432 | `			/* Jump to the desired location */` |
|      ! 0 |  4433 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4434 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4435 | `		}` |
|        - |  4436 | `	}` |
|   166910 |  4437 | `	break;` |
|        - |  4438 | `				 }` |
|        - |  4439 | `/* OP_TNE P1 P2 *` |
|        - |  4440 | ` *` |
|        - |  4441 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  4442 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  4443 | ` * instruction.` |
|        - |  4444 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4445 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4446 | ` *` |
|        - |  4447 | ` */` |
|    65700 |  4448 | `case PH7_OP_TNE: {` |
|   131402 |  4449 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4450 | `	/* Perform the comparison and act accordingly */` |
|        - |  4451 | `#ifdef UNTRUST` |
|        - |  4452 | `	if( pNos < pStack ){` |
|        - |  4453 | `		goto Abort;` |
|        - |  4454 | `	}` |
|        - |  4455 | `#endif` |
|   131402 |  4456 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   131402 |  4457 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4458 | `		rc = 1;` |
|        2 |  4459 | `	}else{` |
|   131400 |  4460 | `		rc = rc != 0;` |
|        - |  4461 | `	}` |
|   131402 |  4462 | `	VmPopOperand(&pTos,1);` |
|   131402 |  4463 | `	if( !pInstr->iP2 ){` |
|        - |  4464 | `		/* Push comparison result without taking the jump */` |
|   131402 |  4465 | `		PH7_MemObjRelease(pTos);` |
|   131402 |  4466 | `		pTos->x.iVal = rc;` |
|        - |  4467 | `		/* Invalidate any prior representation */` |
|   131402 |  4468 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    65702 |  4469 | `	}else{` |
|      ! 0 |  4470 | `		if( rc ){` |
|        - |  4471 | `			/* Jump to the desired location */` |
|      ! 0 |  4472 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4473 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4474 | `		}` |
|        - |  4475 | `	}` |
|   131402 |  4476 | `	break;` |
|        - |  4477 | `				 }` |
|        - |  4478 | `/* OP_LT P1 P2 P3` |
|        - |  4479 | ` *` |
|        - |  4480 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4481 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4482 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4483 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4484 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4485 | ` *` |
|        - |  4486 | ` */` |
|        - |  4487 | `/* OP_LE P1 P2 P3` |
|        - |  4488 | ` *` |
|        - |  4489 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4490 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4491 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4492 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4493 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4494 | ` *` |
|        - |  4495 | ` */` |
|    69352 |  4496 | `case PH7_OP_LT:` |
|        - |  4497 | `case PH7_OP_LE: {` |
|   138750 |  4498 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4499 | `	/* Perform the comparison and act accordingly */` |
|        - |  4500 | `#ifdef UNTRUST` |
|        - |  4501 | `	if( pNos < pStack ){` |
|        - |  4502 | `		goto Abort;` |
|        - |  4503 | `	}` |
|        - |  4504 | `#endif` |
|   138750 |  4505 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   138750 |  4506 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4507 | `		rc = 0;` |
|   138746 |  4508 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      395 |  4509 | `		rc = rc < 1;` |
|      198 |  4510 | `	}else{` |
|   138348 |  4511 | `		rc = rc < 0;` |
|        - |  4512 | `	}` |
|   138750 |  4513 | `	VmPopOperand(&pTos,1);` |
|   138750 |  4514 | `	if( !pInstr->iP2 ){` |
|        - |  4515 | `		/* Push comparison result without taking the jump */` |
|   138750 |  4516 | `		PH7_MemObjRelease(pTos);` |
|   138750 |  4517 | `		pTos->x.iVal = rc;` |
|        - |  4518 | `		/* Invalidate any prior representation */` |
|   138750 |  4519 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    69398 |  4520 | `	}else{` |
|      ! 0 |  4521 | `		if( rc ){` |
|        - |  4522 | `			/* Jump to the desired location */` |
|      ! 0 |  4523 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4524 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4525 | `		}` |
|        - |  4526 | `	}` |
|   138750 |  4527 | `	break;` |
|        - |  4528 | `				}` |
|        - |  4529 | `/* OP_GT P1 P2 P3` |
|        - |  4530 | ` *` |
|        - |  4531 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4532 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4533 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4534 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4535 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4536 | ` *` |
|        - |  4537 | ` */` |
|        - |  4538 | `/* OP_GE P1 P2 P3` |
|        - |  4539 | ` *` |
|        - |  4540 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4541 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4542 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4543 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4544 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4545 | ` *` |
|        - |  4546 | ` */` |
|    24202 |  4547 | `case PH7_OP_GT:` |
|        - |  4548 | `case PH7_OP_GE: {` |
|    48406 |  4549 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4550 | `	/* Perform the comparison and act accordingly */` |
|        - |  4551 | `#ifdef UNTRUST` |
|        - |  4552 | `	if( pNos < pStack ){` |
|        - |  4553 | `		goto Abort;` |
|        - |  4554 | `	}` |
|        - |  4555 | `#endif` |
|    48406 |  4556 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|    48406 |  4557 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4558 | `		rc = 0;` |
|    48402 |  4559 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|    48250 |  4560 | `		rc = rc >= 0;` |
|    24126 |  4561 | `	}else{` |
|      150 |  4562 | `		rc = rc > 0;` |
|        - |  4563 | `	}` |
|    48406 |  4564 | `	VmPopOperand(&pTos,1);` |
|    48406 |  4565 | `	if( !pInstr->iP2 ){` |
|        - |  4566 | `		/* Push comparison result without taking the jump */` |
|    48406 |  4567 | `		PH7_MemObjRelease(pTos);` |
|    48406 |  4568 | `		pTos->x.iVal = rc;` |
|        - |  4569 | `		/* Invalidate any prior representation */` |
|    48406 |  4570 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    24204 |  4571 | `	}else{` |
|      ! 0 |  4572 | `		if( rc ){` |
|        - |  4573 | `			/* Jump to the desired location */` |
|      ! 0 |  4574 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4575 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4576 | `		}` |
|        - |  4577 | `	}` |
|    48406 |  4578 | `	break;` |
|        - |  4579 | `				}` |
|        - |  4580 | `/* OP_SEQ P1 P2 *` |
|        - |  4581 | ` * Strict string comparison.` |
|        - |  4582 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  4583 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4584 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4585 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4586 | ` * use PH7_OP_EQ.` |
|        - |  4587 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4588 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4589 | ` */` |
|        - |  4590 | `/* OP_SNE P1 P2 *` |
|        - |  4591 | ` * Strict string comparison.` |
|        - |  4592 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  4593 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4594 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4595 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4596 | ` * use PH7_OP_EQ.` |
|        - |  4597 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4598 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4599 | ` */` |
|       18 |  4600 | `case PH7_OP_SEQ:` |
|        - |  4601 | `case PH7_OP_SNE: {` |
|       38 |  4602 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4603 | `	SyString s1,s2;` |
|        - |  4604 | `	/* Perform the comparison and act accordingly */` |
|        - |  4605 | `#ifdef UNTRUST` |
|        - |  4606 | `	if( pNos < pStack ){` |
|        - |  4607 | `		goto Abort;` |
|        - |  4608 | `	}` |
|        - |  4609 | `#endif` |
|        - |  4610 | `	/* Force a string cast */` |
|       38 |  4611 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        9 |  4612 | `		PH7_MemObjToString(pTos);` |
|        4 |  4613 | `	}` |
|       38 |  4614 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  4615 | `		PH7_MemObjToString(pNos);` |
|        2 |  4616 | `	}` |
|       38 |  4617 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  4618 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  4619 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  4620 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  4621 | `		rc = rc != 0;` |
|      ! 0 |  4622 | `	}else{` |
|       38 |  4623 | `		rc = rc == 0;` |
|        - |  4624 | `	}` |
|       38 |  4625 | `	VmPopOperand(&pTos,1);` |
|       38 |  4626 | `	if( !pInstr->iP2 ){` |
|        - |  4627 | `		/* Push comparison result without taking the jump */` |
|       38 |  4628 | `		PH7_MemObjRelease(pTos);` |
|       38 |  4629 | `		pTos->x.iVal = rc;` |
|        - |  4630 | `		/* Invalidate any prior representation */` |
|       38 |  4631 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  4632 | `	}else{` |
|      ! 0 |  4633 | `		if( rc ){` |
|        - |  4634 | `			/* Jump to the desired location */` |
|      ! 0 |  4635 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4636 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4637 | `		}` |
|        - |  4638 | `	}` |
|       38 |  4639 | `	break;` |
|        - |  4640 | `				 }` |
|        - |  4641 | `/*` |
|        - |  4642 | ` * OP_LOAD_REF * * *` |
|        - |  4643 | ` * Push the index of a referenced object on the stack.` |
|        - |  4644 | ` */` |
|       57 |  4645 | `case PH7_OP_LOAD_REF: {` |
|        - |  4646 | `	sxu32 nIdx;` |
|        - |  4647 | `#ifdef UNTRUST` |
|        - |  4648 | `	if( pTos < pStack ){` |
|        - |  4649 | `		goto Abort;` |
|        - |  4650 | `	}` |
|        - |  4651 | `#endif` |
|        - |  4652 | `	/* Extract memory object index */` |
|      115 |  4653 | `	nIdx = pTos->nIdx;` |
|      115 |  4654 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  4655 | `		/* Nullify the object */` |
|       95 |  4656 | `		PH7_MemObjRelease(pTos);` |
|        - |  4657 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  4658 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  4659 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  4660 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  4661 | `	}` |
|      115 |  4662 | `	break;` |
|        - |  4663 | `					  }` |
|        - |  4664 | `/*` |
|        - |  4665 | ` * OP_STORE_REF * * P3` |
|        - |  4666 | ` * Perform an assignment operation by reference.` |
|        - |  4667 | ` */` |
|       14 |  4668 | ` case PH7_OP_STORE_REF: {` |
|       30 |  4669 | `	 SyString sName = { 0 , 0 };` |
|        - |  4670 | `	 VmFrame *pFrameLocal;` |
|        - |  4671 | `	SyHashEntry *pEntry;` |
|        - |  4672 | `	sxu32 nIdx;` |
|        - |  4673 | `#ifdef UNTRUST` |
|        - |  4674 | `	if( pTos < pStack ){` |
|        - |  4675 | `		goto Abort;` |
|        - |  4676 | `	}` |
|        - |  4677 | `#endif` |
|       30 |  4678 | `	if( pInstr->p3 == 0 ){` |
|        - |  4679 | `		char *zName;` |
|        - |  4680 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  4681 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4682 | `			/* Force a string cast */` |
|      ! 0 |  4683 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4684 | `		}` |
|      ! 0 |  4685 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4686 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  4687 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4688 | `			if( zName ){` |
|      ! 0 |  4689 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4690 | `			}` |
|      ! 0 |  4691 | `		}` |
|      ! 0 |  4692 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  4693 | `		pTos--;` |
|      ! 0 |  4694 | `	}else{` |
|       30 |  4695 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4696 | `	}` |
|       30 |  4697 | `	nIdx = pTos->nIdx;` |
|       30 |  4698 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  4699 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  4700 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  4701 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  4702 | `		}else{` |
|        - |  4703 | `			ph7_value *pObj;` |
|        - |  4704 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  4705 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  4706 | `			if( pObj == 0 ){` |
|      ! 0 |  4707 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  4708 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4709 | `				goto Abort;` |
|        - |  4710 | `			}` |
|        - |  4711 | `			/* Perform the store operation */` |
|      ! 0 |  4712 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  4713 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  4714 | `		}` |
|       30 |  4715 | `	}else if( sName.nByte > 0){` |
|       30 |  4716 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  4717 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  4718 | `		}else{` |
|       30 |  4719 | `			pFrameLocal = pVm->pFrame;` |
|       50 |  4720 | `			while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4721 | `				/* Safely ignore the exception frame */` |
|       21 |  4722 | `				pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4723 | `			}` |
|        - |  4724 | `			/* Query the local frame */` |
|       30 |  4725 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       30 |  4726 | `			if( pEntry ){` |
|      ! 0 |  4727 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  4728 | `			}else{` |
|       30 |  4729 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       30 |  4730 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  4731 | `					/* Insert in the $GLOBALS array */` |
|       26 |  4732 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       12 |  4733 | `				}` |
|       30 |  4734 | `				if( rc == SXRET_OK ){` |
|       30 |  4735 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       14 |  4736 | `				}` |
|        - |  4737 | `			}` |
|        - |  4738 | `		}` |
|       14 |  4739 | `	}` |
|       30 |  4740 | `	break;` |
|        - |  4741 | `				 }` |
|        - |  4742 | `/*` |
|        - |  4743 | ` * OP_UPLINK P1 * *` |
|        - |  4744 | ` * Link a variable to the top active VM frame.` |
|        - |  4745 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  4746 | ` */` |
|       14 |  4747 | `case PH7_OP_UPLINK: {` |
|       29 |  4748 | `	if( pVm->pFrame->pParent ){` |
|       29 |  4749 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  4750 | `		SyString sName;` |
|        - |  4751 | `		/* Perform the link */` |
|       59 |  4752 | `		while( pLink <= pTos ){` |
|       31 |  4753 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4754 | `				/* Force a string cast */` |
|      ! 0 |  4755 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  4756 | `			}` |
|       31 |  4757 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       31 |  4758 | `			if( sName.nByte > 0 ){` |
|       31 |  4759 | `				VmFrameLink(&(*pVm),&sName);` |
|       15 |  4760 | `			}` |
|       31 |  4761 | `			pLink++;` |
|        1 |  4762 | `		}` |
|       14 |  4763 | `	}` |
|       29 |  4764 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       29 |  4765 | `	break;` |
|        - |  4766 | `					}` |
|        - |  4767 | `/*` |
|        - |  4768 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  4769 | ` * Push an exception in the corresponding container so that` |
|        - |  4770 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  4771 | ` */` |
|       10 |  4772 | `case PH7_OP_LOAD_EXCEPTION: {` |
|       22 |  4773 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  4774 | `	VmFrame *pFrameLocal;` |
|       22 |  4775 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  4776 | `	/* Create the exception frame */` |
|       22 |  4777 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|       22 |  4778 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4779 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  4780 | `		goto Abort;` |
|        - |  4781 | `	}` |
|        - |  4782 | `	/* Mark the special frame */` |
|       22 |  4783 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|       22 |  4784 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  4785 | `	/* Point to the frame that trigger the exception */` |
|       22 |  4786 | `	pFrameLocal = pFrameLocal->pParent;` |
|       34 |  4787 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|       13 |  4788 | `		pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4789 | `	}` |
|       22 |  4790 | `	pException->pFrame = pFrameLocal;` |
|       22 |  4791 | `	break;` |
|        - |  4792 | `							}` |
|        - |  4793 | `/*` |
|        - |  4794 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  4795 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  4796 | ` */` |
|        9 |  4797 | `case PH7_OP_POP_EXCEPTION: {` |
|       20 |  4798 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|       20 |  4799 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  4800 | `		ph7_exception **apException;` |
|        - |  4801 | `		/* Pop the loaded exception */` |
|        7 |  4802 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        7 |  4803 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|        7 |  4804 | `			(void)SySetPop(&pVm->aException);` |
|        3 |  4805 | `		}` |
|        3 |  4806 | `	}` |
|       20 |  4807 | `	pException->pFrame = 0;` |
|        - |  4808 | `	/* Leave the exception frame */` |
|       20 |  4809 | `	VmLeaveFrame(&(*pVm));` |
|       20 |  4810 | `	break;` |
|        - |  4811 | `							}` |
|        - |  4812 |  |
|        - |  4813 | `/*` |
|        - |  4814 | ` * OP_THROW * P2 *` |
|        - |  4815 | ` * Throw an user exception.` |
|        - |  4816 | ` */` |
|        8 |  4817 | `case PH7_OP_THROW: {` |
|       18 |  4818 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       18 |  4819 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  4820 | `#ifdef UNTRUST` |
|        - |  4821 | `	if( pTos < pStack ){` |
|        - |  4822 | `		goto Abort;` |
|        - |  4823 | `	}` |
|        - |  4824 | `#endif` |
|       24 |  4825 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4826 | `		/* Safely ignore the exception frame */` |
|        8 |  4827 | `		pFrameLocal = pFrameLocal->pParent;` |
|        2 |  4828 | `	}` |
|        - |  4829 | `	/* Tell the upper layer that an exception was thrown */` |
|       18 |  4830 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       18 |  4831 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       18 |  4832 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4833 | `		ph7_class *pException;` |
|        - |  4834 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  4835 | `		 */` |
|       18 |  4836 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       18 |  4837 | `		if( pException == 0 \|\| !VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  4838 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  4839 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  4840 | `			if( rc == SXERR_ABORT ){` |
|        - |  4841 | `				/* Abort processing immediately */` |
|      ! 0 |  4842 | `				goto Abort;` |
|        - |  4843 | `			}` |
|      ! 0 |  4844 | `		}else{` |
|        - |  4845 | `			/* Throw the exception */` |
|       18 |  4846 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       18 |  4847 | `			if( rc == SXERR_ABORT ){` |
|        - |  4848 | `				/* Abort processing immediately */` |
|        3 |  4849 | `				goto Abort;` |
|        - |  4850 | `			}` |
|        - |  4851 | `		}` |
|        9 |  4852 | `	}else{` |
|        - |  4853 | `		/* Expecting a class instance */` |
|      ! 0 |  4854 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  4855 | `		if( rc == SXERR_ABORT ){` |
|        - |  4856 | `			/* Abort processing immediately */` |
|      ! 0 |  4857 | `			goto Abort;` |
|        - |  4858 | `		}` |
|        - |  4859 | `	}` |
|        - |  4860 | `	/* Pop the top entry */` |
|       16 |  4861 | `	VmPopOperand(&pTos,1);` |
|        - |  4862 | `	/* Perform an unconditional jump */` |
|       16 |  4863 | `	pc = nJump - 1;` |
|       16 |  4864 | `	break;` |
|        - |  4865 | `				   }` |
|        - |  4866 | `/*` |
|        - |  4867 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  4868 | ` * Prepare a foreach step.` |
|        - |  4869 | ` */` |
|     3456 |  4870 | `case PH7_OP_FOREACH_INIT: {` |
|     6914 |  4871 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4872 | `	void *pName;` |
|        - |  4873 | `#ifdef UNTRUST` |
|        - |  4874 | `	if( pTos < pStack ){` |
|        - |  4875 | `		goto Abort;` |
|        - |  4876 | `	}` |
|        - |  4877 | `#endif` |
|     6914 |  4878 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4879 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  4880 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4881 | `			/* Force a string cast */` |
|      ! 0 |  4882 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4883 | `		}` |
|        - |  4884 | `		/* Duplicate name */` |
|      ! 0 |  4885 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4886 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4887 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4888 | `		}` |
|      ! 0 |  4889 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4890 | `	}` |
|     6914 |  4891 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  4892 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4893 | `			/* Force a string cast */` |
|      ! 0 |  4894 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4895 | `		}` |
|        - |  4896 | `		/* Duplicate name */` |
|      ! 0 |  4897 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4898 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4899 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4900 | `		}` |
|      ! 0 |  4901 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4902 | `	}` |
|        - |  4903 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|     6914 |  4904 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4905 | `		/* Jump out of the loop */` |
|      ! 0 |  4906 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  4907 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  4908 | `		}` |
|      ! 0 |  4909 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  4910 | `	}else{` |
|        - |  4911 | `		ph7_foreach_step *pStep;` |
|     6914 |  4912 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|     6914 |  4913 | `		if( pStep == 0 ){` |
|      ! 0 |  4914 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  4915 | `			/* Jump out of the loop */` |
|      ! 0 |  4916 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4917 | `		}else{` |
|        - |  4918 | `			/* Zero the structure */` |
|     6914 |  4919 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  4920 | `			/* Prepare the step */` |
|     6914 |  4921 | `			pStep->iFlags = pInfo->iFlags;` |
|     6914 |  4922 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|     6906 |  4923 | `				ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4924 | `				/* Reset the internal loop cursor */` |
|     6906 |  4925 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  4926 | `				/* Mark the step */` |
|     6906 |  4927 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|     6906 |  4928 | `				pStep->xIter.pMap = pMap;` |
|     6906 |  4929 | `				pMap->iRef++;` |
|     3454 |  4930 | `			}else{` |
|        9 |  4931 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4932 | `				/* Reset the loop cursor */` |
|        9 |  4933 | `				SyHashResetLoopCursor(&pThis->hAttr);` |
|        - |  4934 | `				/* Mark the step */` |
|        9 |  4935 | `				pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  4936 | `				pStep->xIter.pThis = pThis;` |
|        9 |  4937 | `				pThis->iRef++;` |
|        - |  4938 | `			}` |
|        - |  4939 | `		}` |
|     6914 |  4940 | `		if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  4941 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  4942 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  4943 | `			/* Jump out of the loop */` |
|      ! 0 |  4944 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4945 | `		}` |
|        - |  4946 | `	}` |
|     6914 |  4947 | `	VmPopOperand(&pTos,1);` |
|     6914 |  4948 | `	break;` |
|        - |  4949 | `						  }` |
|        - |  4950 | `/*` |
|        - |  4951 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  4952 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  4953 | ` */` |
|    58452 |  4954 | `case PH7_OP_FOREACH_STEP: {` |
|   116906 |  4955 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4956 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  4957 | `	ph7_value *pValue;` |
|        - |  4958 | `	VmFrame *pFrameLocal;` |
|        - |  4959 | `	/* Peek the last step */` |
|   116906 |  4960 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   116906 |  4961 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   116906 |  4962 | `	pFrameLocal = pVm->pFrame;` |
|   121938 |  4963 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4964 | `		/* Safely ignore the exception frame */` |
|     5033 |  4965 | `		pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4966 | `	}` |
|   116906 |  4967 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   116882 |  4968 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  4969 | `		ph7_hashmap_node *pNode;` |
|        - |  4970 | `		/* Extract the current node value */` |
|   116882 |  4971 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   116882 |  4972 | `		if( pNode == 0 ){` |
|        - |  4973 | `			/* No more entry to process */` |
|     6906 |  4974 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|     6906 |  4975 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  4976 | `				/* Break the reference with the last element */` |
|        5 |  4977 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        2 |  4978 | `			}` |
|        - |  4979 | `			/* Automatically reset the loop cursor */` |
|     6906 |  4980 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  4981 | `			/* Cleanup the mess left behind */` |
|     6906 |  4982 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     6906 |  4983 | `			SySetPop(&pInfo->aStep);` |
|     6906 |  4984 | `			PH7_HashmapUnref(pMap);` |
|     3454 |  4985 | `		}else{` |
|   109978 |  4986 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      135 |  4987 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      135 |  4988 | `				if( pKey ){` |
|      135 |  4989 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|       67 |  4990 | `				}` |
|       67 |  4991 | `			}` |
|   109978 |  4992 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  4993 | `				SyHashEntry *pEntry;` |
|        - |  4994 | `				/* Pass by reference */` |
|       13 |  4995 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       13 |  4996 | `				if( pEntry ){` |
|       13 |  4997 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|        7 |  4998 | `				}else{` |
|      ! 0 |  4999 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5000 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  5001 | `				}` |
|        7 |  5002 | `			}else{` |
|        - |  5003 | `				/* Make a copy of the entry value */` |
|   109966 |  5004 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   109966 |  5005 | `				if( pValue ){` |
|   109966 |  5006 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    54982 |  5007 | `				}` |
|        - |  5008 | `			}` |
|        - |  5009 | `		}` |
|    58442 |  5010 | `	}else{` |
|       25 |  5011 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  5012 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  5013 | `		SyHashEntry *pEntry;` |
|        - |  5014 | `		/* Point to the next attribute */` |
|       29 |  5015 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  5016 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  5017 | `			/* Check access permission */` |
|       31 |  5018 | `			if( VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  5019 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  5020 | `					break; /* Access is granted */` |
|        - |  5021 | `			}` |
|        1 |  5022 | `		}` |
|       25 |  5023 | `		if( pEntry == 0 ){` |
|        - |  5024 | `			/* Clean up the mess left behind */` |
|        9 |  5025 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  5026 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5027 | `				/* Break the reference with the last element */` |
|        3 |  5028 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  5029 | `			}` |
|        9 |  5030 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  5031 | `			SySetPop(&pInfo->aStep);` |
|        9 |  5032 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  5033 | `		}else{` |
|       17 |  5034 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  5035 | `			ph7_value *pAttrValue;` |
|       17 |  5036 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  5037 | `				/* Fill with the current attribute name */` |
|       17 |  5038 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  5039 | `				if( pKey ){` |
|       17 |  5040 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  5041 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  5042 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  5043 | `				}` |
|        8 |  5044 | `			}` |
|        - |  5045 | `			/* Extract attribute value */` |
|       17 |  5046 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  5047 | `			if( pAttrValue ){` |
|       17 |  5048 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5049 | `					/* Pass by reference */` |
|        3 |  5050 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  5051 | `					if( pEntry ){` |
|        3 |  5052 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  5053 | `					}else{` |
|      ! 0 |  5054 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5055 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  5056 | `					}` |
|        2 |  5057 | `				}else{` |
|        - |  5058 | `					/* Make a copy of the attribute value */` |
|       15 |  5059 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  5060 | `					if( pValue ){` |
|       15 |  5061 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  5062 | `					}` |
|        - |  5063 | `				}` |
|        8 |  5064 | `			}` |
|        - |  5065 | `		}` |
|        - |  5066 | `	}` |
|   116906 |  5067 | `	break;` |
|        - |  5068 | `						  }` |
|        - |  5069 | `/*` |
|        - |  5070 | ` * OP_MEMBER P1 P2` |
|        - |  5071 | ` * Load class attribute/method on the stack.` |
|        - |  5072 | ` */` |
|      456 |  5073 | `case PH7_OP_MEMBER: {` |
|        - |  5074 | `	ph7_class_instance *pThis;` |
|        - |  5075 | `	ph7_value *pNos;` |
|        - |  5076 | `	SyString sName;` |
|      914 |  5077 | `	if( !pInstr->iP1 ){` |
|      856 |  5078 | `		pNos = &pTos[-1];` |
|        - |  5079 | `#ifdef UNTRUST` |
|        - |  5080 | `		if( pNos < pStack ){` |
|        - |  5081 | `			goto Abort;` |
|        - |  5082 | `		}` |
|        - |  5083 | `#endif` |
|      856 |  5084 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5085 | `			ph7_class *pClass;` |
|        - |  5086 | `			/* Class already instantiated */` |
|      856 |  5087 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  5088 | `			/* Point to the instantiated class */` |
|      856 |  5089 | `			pClass = pThis->pClass;` |
|        - |  5090 | `			/* Extract attribute name first */` |
|      856 |  5091 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      856 |  5092 | `			if( pInstr->iP2 ){` |
|        - |  5093 | `				/* Method call */` |
|      120 |  5094 | `				ph7_class_method *pMeth = 0;` |
|      120 |  5095 | `				if( sName.nByte > 0 ){` |
|        - |  5096 | `					/* Extract the target method */` |
|      120 |  5097 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       59 |  5098 | `				}` |
|      120 |  5099 | `				if( pMeth == 0 ){` |
|      ! 0 |  5100 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  5101 | `						&pClass->sName,&sName` |
|        - |  5102 | `						);` |
|        - |  5103 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  5104 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  5105 | `					/* Pop the method name from the stack */` |
|      ! 0 |  5106 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5107 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  5108 | `				}else{` |
|        - |  5109 | `					/* Push method name on the stack */` |
|      120 |  5110 | `					PH7_MemObjRelease(pTos);` |
|      120 |  5111 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      120 |  5112 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5113 | `				}` |
|      120 |  5114 | `				pTos->nIdx = SXU32_HIGH;` |
|       61 |  5115 | `			}else{` |
|        - |  5116 | `				/* Attribute access */` |
|      738 |  5117 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  5118 | `				SyHashEntry *pEntry;` |
|        - |  5119 | `				/* Extract the target attribute */` |
|      738 |  5120 | `				if( sName.nByte > 0 ){` |
|      738 |  5121 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|      738 |  5122 | `					if( pEntry ){` |
|        - |  5123 | `						/* Point to the attribute value */` |
|      736 |  5124 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|      367 |  5125 | `					}` |
|      368 |  5126 | `				}` |
|      738 |  5127 | `				if( pObjAttr == 0 ){` |
|        - |  5128 | `					/* No such attribute,load null */` |
|        4 |  5129 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  5130 | `						&pClass->sName,&sName);` |
|        - |  5131 | `					/* Call the __get magic method if available */` |
|        3 |  5132 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  5133 | `				}` |
|      738 |  5134 | `				VmPopOperand(&pTos,1);` |
|        - |  5135 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  5136 | `				 * This is due to the following case:` |
|        - |  5137 | `				 *     (new TestClass())->foo;` |
|        - |  5138 | `				 */` |
|      738 |  5139 | `				pThis->iRef++;` |
|      738 |  5140 | `				PH7_MemObjRelease(pTos);` |
|      738 |  5141 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|      738 |  5142 | `				if( pObjAttr ){` |
|      736 |  5143 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  5144 | `					/* Check attribute access */` |
|      736 |  5145 | `					if( VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,TRUE) ){` |
|        - |  5146 | `						/* Load attribute */` |
|      736 |  5147 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|      736 |  5148 | `						if( pValue ){` |
|      736 |  5149 | `							if( pThis->iRef < 2 ){` |
|        - |  5150 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  5151 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  5152 | `								 */` |
|        3 |  5153 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  5154 | `							}else{` |
|        - |  5155 | `								/* Simple load */` |
|      734 |  5156 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  5157 | `							}` |
|      736 |  5158 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|      734 |  5159 | `								if( pThis->iRef > 1 ){` |
|        - |  5160 | `									/* Load attribute index */` |
|      732 |  5161 | `									pTos->nIdx = pObjAttr->nIdx;` |
|      365 |  5162 | `								}` |
|      366 |  5163 | `							}` |
|      367 |  5164 | `						}` |
|      367 |  5165 | `					}` |
|      367 |  5166 | `				}` |
|        - |  5167 | `				/* Safely unreference the object */` |
|      738 |  5168 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  5169 | `			}` |
|      429 |  5170 | `		}else{` |
|      ! 0 |  5171 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  5172 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5173 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5174 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  5175 | `		}` |
|      429 |  5176 | `	}else{` |
|        - |  5177 | `		/* Static member access using class name */` |
|       59 |  5178 | `		pNos = pTos;` |
|       59 |  5179 | `		pThis = 0;` |
|       59 |  5180 | `		if( !pInstr->p3 ){` |
|       57 |  5181 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       57 |  5182 | `			pNos--;` |
|        - |  5183 | `#ifdef UNTRUST` |
|        - |  5184 | `			if( pNos < pStack ){` |
|        - |  5185 | `				goto Abort;` |
|        - |  5186 | `			}` |
|        - |  5187 | `#endif` |
|       29 |  5188 | `		}else{` |
|        - |  5189 | `			/* Attribute name already computed */` |
|        3 |  5190 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5191 | `		}` |
|       59 |  5192 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|       59 |  5193 | `			ph7_class *pClass = 0;` |
|       59 |  5194 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5195 | `				/* Class already instantiated */` |
|      ! 0 |  5196 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      ! 0 |  5197 | `				pClass = pThis->pClass;` |
|      ! 0 |  5198 | `				pThis->iRef++; /* Deffer garbage collection */` |
|      ! 0 |  5199 | `			}else{` |
|        - |  5200 | `				/* Try to extract the target class */` |
|       59 |  5201 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|       88 |  5202 | `					pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pNos->sBlob),` |
|       29 |  5203 | `						SyBlobLength(&pNos->sBlob),FALSE,0);` |
|       29 |  5204 | `				}` |
|        - |  5205 | `			}` |
|       59 |  5206 | `			if( pClass == 0 ){` |
|        - |  5207 | `				/* Undefined class */` |
|      ! 0 |  5208 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  5209 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  5210 | `					);` |
|      ! 0 |  5211 | `				if( !pInstr->p3 ){` |
|      ! 0 |  5212 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5213 | `				}` |
|      ! 0 |  5214 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5215 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5216 | `			}else{` |
|       59 |  5217 | `				if( pInstr->iP2 ){` |
|        - |  5218 | `					/* Method call */` |
|       25 |  5219 | `					ph7_class_method *pMeth = 0;` |
|       25 |  5220 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  5221 | `						/* Extract the target method */` |
|       25 |  5222 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       12 |  5223 | `					}` |
|       25 |  5224 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  5225 | `						if( pMeth ){` |
|      ! 0 |  5226 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  5227 | `								&pClass->sName,&sName` |
|        - |  5228 | `								);` |
|      ! 0 |  5229 | `						}else{` |
|      ! 0 |  5230 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5231 | `								&pClass->sName,&sName` |
|        - |  5232 | `								);` |
|        - |  5233 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  5234 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  5235 | `						}` |
|        - |  5236 | `						/* Pop the method name from the stack */` |
|      ! 0 |  5237 | `						if( !pInstr->p3 ){` |
|      ! 0 |  5238 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  5239 | `						}` |
|      ! 0 |  5240 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  5241 | `					}else{` |
|        - |  5242 | `						/* Push method name on the stack */` |
|       25 |  5243 | `						PH7_MemObjRelease(pTos);` |
|       25 |  5244 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       25 |  5245 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5246 | `					}` |
|       25 |  5247 | `					pTos->nIdx = SXU32_HIGH;` |
|       13 |  5248 | `				}else{` |
|        - |  5249 | `					/* Attribute access */` |
|       35 |  5250 | `					ph7_class_attr *pAttr = 0;` |
|        - |  5251 | `					/* Check for special ::class pseudo-constant */` |
|       49 |  5252 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       28 |  5253 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  5254 | `						/* ::class returns the fully qualified class name */` |
|        - |  5255 | `						/* Pop the attribute name from the stack */` |
|       27 |  5256 | `						if( !pInstr->p3 ){` |
|       27 |  5257 | `							VmPopOperand(&pTos,1);` |
|       13 |  5258 | `						}` |
|       27 |  5259 | `						PH7_MemObjRelease(pTos);` |
|        - |  5260 | `						/* Load the class name */` |
|       27 |  5261 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       27 |  5262 | `						pTos->nIdx = SXU32_HIGH;` |
|       14 |  5263 | `					}else{` |
|        - |  5264 | `						/* Extract the target attribute */` |
|        9 |  5265 | `						if( sName.nByte > 0 ){` |
|        9 |  5266 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|        4 |  5267 | `						}` |
|        9 |  5268 | `						if( pAttr == 0 ){` |
|        - |  5269 | `							/* No such attribute,load null */` |
|      ! 0 |  5270 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5271 | `								&pClass->sName,&sName);` |
|        - |  5272 | `							/* Call the __get magic method if available */` |
|      ! 0 |  5273 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  5274 | `						}` |
|        - |  5275 | `						/* Pop the attribute name from the stack */` |
|        9 |  5276 | `						if( !pInstr->p3 ){` |
|        7 |  5277 | `							VmPopOperand(&pTos,1);` |
|        3 |  5278 | `						}` |
|        9 |  5279 | `						PH7_MemObjRelease(pTos);` |
|        9 |  5280 | `						pTos->nIdx = SXU32_HIGH;` |
|        9 |  5281 | `						if( pAttr ){` |
|        9 |  5282 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  5283 | `								/* Access to a non static attribute */` |
|      ! 0 |  5284 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5285 | `									&pClass->sName,&pAttr->sName` |
|        - |  5286 | `									);` |
|      ! 0 |  5287 | `							}else{` |
|        - |  5288 | `								ph7_value *pValue;` |
|        - |  5289 | `								/* Check if the access to the attribute is allowed */` |
|        9 |  5290 | `								if( VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,TRUE) ){` |
|        - |  5291 | `									/* Load the desired attribute */` |
|        9 |  5292 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|        9 |  5293 | `									if( pValue ){` |
|        9 |  5294 | `										PH7_MemObjLoad(pValue,pTos);` |
|        9 |  5295 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  5296 | `											/* Load index number */` |
|        3 |  5297 | `											pTos->nIdx = pAttr->nIdx;` |
|        1 |  5298 | `										}` |
|        4 |  5299 | `									}` |
|        4 |  5300 | `								}` |
|        - |  5301 | `							}` |
|        4 |  5302 | `						}` |
|        - |  5303 | `					}` |
|        - |  5304 | `				}` |
|       59 |  5305 | `				if( pThis ){` |
|        - |  5306 | `					/* Safely unreference the object */` |
|      ! 0 |  5307 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  5308 | `				}` |
|        - |  5309 | `			}` |
|       30 |  5310 | `		}else{` |
|        - |  5311 | `			/* Pop operands */` |
|      ! 0 |  5312 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  5313 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5314 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  5315 | `			}` |
|      ! 0 |  5316 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5317 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5318 | `		}` |
|        - |  5319 | `	}` |
|      914 |  5320 | `	break;` |
|        - |  5321 | `					}` |
|        - |  5322 | `/*` |
|        - |  5323 | ` * OP_NEW P1 * * *` |
|        - |  5324 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  5325 | ` */` |
|      247 |  5326 | `case PH7_OP_NEW: {` |
|      496 |  5327 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      496 |  5328 | `	ph7_class *pClass = 0;` |
|        - |  5329 | `	ph7_class_instance *pNew;` |
|      496 |  5330 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  5331 | `		/* Try to extract the desired class */` |
|      743 |  5332 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      494 |  5333 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      247 |  5334 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5335 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  5336 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  5337 | `	}` |
|      496 |  5338 | `	if( pClass == 0 ){` |
|        - |  5339 | `		/* No such class */` |
|      ! 0 |  5340 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined,PH7 is loading NULL",` |
|      ! 0 |  5341 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  5342 | `			);` |
|      ! 0 |  5343 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5344 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5345 | `			/* Pop given arguments */` |
|      ! 0 |  5346 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5347 | `		}` |
|      ! 0 |  5348 | `	}else{` |
|        - |  5349 | `		ph7_class_method *pCons;` |
|        - |  5350 | `		/* Create a new class instance */` |
|      496 |  5351 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      496 |  5352 | `		if( pNew == 0 ){` |
|      ! 0 |  5353 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5354 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  5355 | `				&pClass->sName` |
|        - |  5356 | `			);` |
|      ! 0 |  5357 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5358 | `			if( pInstr->iP1 > 0 ){` |
|        - |  5359 | `				/* Pop given arguments */` |
|      ! 0 |  5360 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5361 | `			}` |
|      ! 0 |  5362 | `			break;` |
|        - |  5363 | `		}` |
|        - |  5364 | `		/* Check if a constructor is available */` |
|      496 |  5365 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      496 |  5366 | `		if( pCons == 0 ){` |
|      444 |  5367 | `			SyString *pName = &pClass->sName;` |
|        - |  5368 | `			/* Check for a constructor with the same base class name */` |
|      444 |  5369 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      221 |  5370 | `		}` |
|      496 |  5371 | `		if( pCons ){` |
|        - |  5372 | `			/* Call the class constructor */` |
|       54 |  5373 | `			SySetReset(&aArg);` |
|       96 |  5374 | `			while( pArg < pTos ){` |
|       44 |  5375 | `				SySetPut(&aArg,(const void *)&pArg);` |
|       44 |  5376 | `				pArg++;` |
|        2 |  5377 | `			}` |
|       54 |  5378 | `			if( pVm->bErrReport ){` |
|        - |  5379 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  5380 | `				sxu32 n;` |
|       12 |  5381 | `				n = SySetUsed(&aArg);` |
|        - |  5382 | `				/* Emit a notice for missing arguments */` |
|       28 |  5383 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       18 |  5384 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       18 |  5385 | `					if( pFuncArg ){` |
|       18 |  5386 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  5387 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  5388 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  5389 | `						}` |
|        8 |  5390 | `					}` |
|       18 |  5391 | `					n++;` |
|        2 |  5392 | `				}` |
|        5 |  5393 | `			}` |
|       54 |  5394 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5395 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|       54 |  5396 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  5397 | `				pNew->iRef = 1;` |
|      ! 0 |  5398 | `			}` |
|       26 |  5399 | `		}` |
|      496 |  5400 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5401 | `			/* Pop given arguments */` |
|       38 |  5402 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       18 |  5403 | `		}` |
|      496 |  5404 | `		PH7_MemObjRelease(pTos);` |
|      496 |  5405 | `		pTos->x.pOther = pNew;` |
|      496 |  5406 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5407 | `	}` |
|      496 |  5408 | `	break;` |
|        - |  5409 | `				 }` |
|        - |  5410 | `/*` |
|        - |  5411 | ` * OP_CLONE * * *` |
|        - |  5412 | ` * Perfome a clone operation.` |
|        - |  5413 | ` */` |
|       23 |  5414 | `case PH7_OP_CLONE: {` |
|        - |  5415 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  5416 | `#ifdef UNTRUST` |
|        - |  5417 | `	if( pTos < pStack ){` |
|        - |  5418 | `		goto Abort;` |
|        - |  5419 | `	}` |
|        - |  5420 | `#endif` |
|        - |  5421 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  5422 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  5423 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5424 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  5425 | `		PH7_MemObjRelease(pTos);` |
|        5 |  5426 | `		break;` |
|        - |  5427 | `	}` |
|        - |  5428 | `	/* Point to the source */` |
|       44 |  5429 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5430 | `	/* Perform the clone operation */` |
|       44 |  5431 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  5432 | `	PH7_MemObjRelease(pTos);` |
|       44 |  5433 | `	if( pClone == 0 ){` |
|      ! 0 |  5434 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5435 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  5436 | `	}else{` |
|        - |  5437 | `		/* Load the cloned object */` |
|       44 |  5438 | `		pTos->x.pOther = pClone;` |
|       44 |  5439 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5440 | `	}` |
|       44 |  5441 | `	break;` |
|        - |  5442 | `				   }` |
|        - |  5443 | `/*` |
|        - |  5444 | ` * OP_SWITCH * * P3` |
|        - |  5445 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  5446 | ` */` |
|       16 |  5447 | `case PH7_OP_SWITCH: {` |
|       34 |  5448 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  5449 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  5450 | `	ph7_value sValue,sCaseValue;` |
|        - |  5451 | `	sxu32 n,nEntry;` |
|        - |  5452 | `#ifdef UNTRUST` |
|        - |  5453 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  5454 | `		goto Abort;` |
|        - |  5455 | `	}` |
|        - |  5456 | `#endif` |
|        - |  5457 | `	/* Point to the case table  */` |
|       34 |  5458 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       34 |  5459 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  5460 | `	/* Select the appropriate case block to execute */` |
|       34 |  5461 | `	PH7_MemObjInit(pVm,&sValue);` |
|       34 |  5462 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|       68 |  5463 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       68 |  5464 | `		pCase = &aCase[n];` |
|       68 |  5465 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  5466 | `		/* Execute the case expression first */` |
|       68 |  5467 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  5468 | `		/* Compare the two expression */` |
|       68 |  5469 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|       68 |  5470 | `		PH7_MemObjRelease(&sValue);` |
|       68 |  5471 | `		PH7_MemObjRelease(&sCaseValue);` |
|       68 |  5472 | `		if( rc == 0 ){` |
|        - |  5473 | `			/* Value match,jump to this block */` |
|       34 |  5474 | `			pc = pCase->nStart - 1;` |
|       34 |  5475 | `			break;` |
|        - |  5476 | `		}` |
|       19 |  5477 | `	}` |
|       34 |  5478 | `	VmPopOperand(&pTos,1);` |
|       34 |  5479 | `	if( n >= nEntry ){` |
|        - |  5480 | `		/* No approprite case to execute,jump to the default case */` |
|      ! 0 |  5481 | `		if( pSwitch->nDefault > 0 ){` |
|      ! 0 |  5482 | `			pc = pSwitch->nDefault - 1;` |
|      ! 0 |  5483 | `		}else{` |
|        - |  5484 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  5485 | `			pc = pSwitch->nOut - 1;` |
|        - |  5486 | `		}` |
|      ! 0 |  5487 | `	}` |
|       34 |  5488 | `	break;` |
|        - |  5489 | `					}` |
|        - |  5490 | `/*` |
|        - |  5491 | ` * OP_CALL P1 * *` |
|        - |  5492 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  5493 | ` *  function on the stack.` |
|        - |  5494 | ` */` |
|   206893 |  5495 | `case PH7_OP_CALL: {` |
|   413832 |  5496 | `	ph7_value *pArg = &pTos[-pInstr->iP1];` |
|        - |  5497 | `	SyHashEntry *pEntry;` |
|        - |  5498 | `	SyString sName;` |
|        - |  5499 | `	/* Extract function name */` |
|   413832 |  5500 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  5501 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5502 | `			ph7_value sResult;` |
|      ! 0 |  5503 | `			SySetReset(&aArg);` |
|      ! 0 |  5504 | `			while( pArg < pTos ){` |
|      ! 0 |  5505 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  5506 | `				pArg++;` |
|      ! 0 |  5507 | `			}` |
|      ! 0 |  5508 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  5509 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  5510 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  5511 | `			SySetReset(&aArg);` |
|        - |  5512 | `			/* Pop given arguments */` |
|      ! 0 |  5513 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5514 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5515 | `			}` |
|        - |  5516 | `			/* Copy result */` |
|      ! 0 |  5517 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  5518 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  5519 | `		}else{` |
|        3 |  5520 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  5521 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5522 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  5523 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  5524 | `			}else{` |
|        - |  5525 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  5526 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  5527 | `			}` |
|        - |  5528 | `			/* Pop given arguments */` |
|        3 |  5529 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5530 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5531 | `			}` |
|        - |  5532 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5533 | `			PH7_MemObjRelease(pTos);` |
|        - |  5534 | `		}` |
|   206886 |  5535 | `		break;` |
|        - |  5536 | `	}` |
|   413830 |  5537 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  5538 | `	/* Check for a compiled function first */` |
|   413830 |  5539 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|   413830 |  5540 | `	if( pEntry ){` |
|        - |  5541 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  5542 | `		ph7_class_instance *pThis;` |
|        - |  5543 | `		ph7_value *pFrameStack;` |
|        - |  5544 | `		ph7_vm_func *pVmFunc;` |
|        - |  5545 | `		ph7_class *pSelf;` |
|        - |  5546 | `		VmFrame *pFrame;` |
|        - |  5547 | `		ph7_value *pObj;` |
|        - |  5548 | `		VmSlot sArg;` |
|        - |  5549 | `		sxu32 n;` |
|        - |  5550 | `		/* initialize fields */` |
|     7726 |  5551 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|     7726 |  5552 | `		pThis = 0;` |
|     7726 |  5553 | `		pSelf = 0;` |
|     7726 |  5554 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  5555 | `			ph7_class_method *pMeth;` |
|        - |  5556 | `			/* Class method call */` |
|      374 |  5557 | `			ph7_value *pTarget = &pTos[-1];` |
|      374 |  5558 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  5559 | `				/* Extract the 'this' pointer */` |
|      374 |  5560 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  5561 | `					/* Instance already loaded */` |
|      344 |  5562 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|      344 |  5563 | `					pThis->iRef++;` |
|      344 |  5564 | `					pSelf = pThis->pClass;` |
|      171 |  5565 | `				}` |
|      374 |  5566 | `				if( pSelf == 0 ){` |
|       31 |  5567 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  5568 | `						/* "Late Static Binding" class name */` |
|       37 |  5569 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       12 |  5570 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       12 |  5571 | `					}` |
|       31 |  5572 | `					if( pSelf == 0 ){` |
|        7 |  5573 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        3 |  5574 | `					}` |
|       15 |  5575 | `				}` |
|      374 |  5576 | `				if( pThis == 0  ){` |
|       31 |  5577 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       33 |  5578 | `					while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  5579 | `						/* Safely ignore the exception frame */` |
|        3 |  5580 | `						pFrameLocal = pFrameLocal->pParent;` |
|        1 |  5581 | `					}` |
|       31 |  5582 | `					if( pFrameLocal->pParent ){` |
|        - |  5583 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       19 |  5584 | `						pThis = pFrameLocal->pThis;` |
|       19 |  5585 | `						if( pThis ){` |
|       13 |  5586 | `							pThis->iRef++;` |
|        6 |  5587 | `						}` |
|        9 |  5588 | `					}` |
|       15 |  5589 | `				}` |
|      374 |  5590 | `				VmPopOperand(&pTos,1);` |
|      374 |  5591 | `				PH7_MemObjRelease(pTos);` |
|        - |  5592 | `				/* Synchronize pointers */` |
|      374 |  5593 | `				pArg = &pTos[-pInstr->iP1];` |
|        - |  5594 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  5595 | `				 * user have already computed the random generated unique class method name` |
|        - |  5596 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  5597 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  5598 | `				 */` |
|      374 |  5599 | `				while( pArg < pStack ){` |
|      ! 0 |  5600 | `					pArg++;` |
|      ! 0 |  5601 | `				}` |
|      374 |  5602 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  5603 | `					/* Check if the call is allowed */` |
|      374 |  5604 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|      374 |  5605 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        5 |  5606 | `						if( !VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,TRUE) ){` |
|        - |  5607 | `							/* Pop given arguments */` |
|      ! 0 |  5608 | `							if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5609 | `								VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5610 | `							}` |
|        - |  5611 | `							/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5612 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  5613 | `							break;` |
|        - |  5614 | `						}` |
|        2 |  5615 | `					}` |
|      186 |  5616 | `				}` |
|      186 |  5617 | `			}` |
|      186 |  5618 | `		}` |
|        - |  5619 | `		/* Check The recursion limit */` |
|     7726 |  5620 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  5621 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5622 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  5623 | `				&pVmFunc->sName);` |
|        - |  5624 | `			/* Pop given arguments */` |
|        3 |  5625 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5626 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5627 | `			}` |
|        - |  5628 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5629 | `			PH7_MemObjRelease(pTos);` |
|        3 |  5630 | `			break;` |
|        - |  5631 | `		}` |
|     7724 |  5632 | `		if( pVmFunc->pNextName ){` |
|        - |  5633 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      129 |  5634 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       64 |  5635 | `		}` |
|        - |  5636 | `		/* Extract the formal argument set */` |
|     7724 |  5637 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  5638 | `		/* Create a new VM frame  */` |
|     7724 |  5639 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|     7724 |  5640 | `		if( rc != SXRET_OK ){` |
|        - |  5641 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5642 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5643 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5644 | `				&pVmFunc->sName);` |
|        - |  5645 | `			/* Pop given arguments */` |
|      ! 0 |  5646 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5647 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5648 | `			}` |
|        - |  5649 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5650 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5651 | `			break;` |
|        - |  5652 | `		}` |
|     7724 |  5653 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  5654 | `			/* Install the '$this' variable */` |
|        - |  5655 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|      354 |  5656 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|      354 |  5657 | `			if( pObj ){` |
|        - |  5658 | `				/* Reflect the change */` |
|      354 |  5659 | `				pObj->x.pOther = pThis;` |
|      354 |  5660 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      176 |  5661 | `			}` |
|      176 |  5662 | `		}` |
|     7724 |  5663 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  5664 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  5665 | `			/* Install static variables */` |
|      ! 0 |  5666 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  5667 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  5668 | `				pStatic = &aStatic[n];` |
|      ! 0 |  5669 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  5670 | `					/* Initialize the static variables */` |
|      ! 0 |  5671 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  5672 | `					if( pObj ){` |
|        - |  5673 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  5674 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  5675 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  5676 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  5677 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  5678 | `						}` |
|      ! 0 |  5679 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  5680 | `					}else{` |
|      ! 0 |  5681 | `						continue;` |
|        - |  5682 | `					}` |
|      ! 0 |  5683 | `				}` |
|        - |  5684 | `				/* Install in the current frame */` |
|      ! 0 |  5685 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  5686 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  5687 | `			}` |
|      ! 0 |  5688 | `		}` |
|        - |  5689 | `		/* Push arguments in the local frame */` |
|     7724 |  5690 | `		n = 0;` |
|    22098 |  5691 | `		while( pArg < pTos ){` |
|    14376 |  5692 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    14276 |  5693 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  5694 | `					/* NULL values are redirected to default arguments */` |
|      748 |  5695 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      748 |  5696 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5697 | `						goto Abort;` |
|        - |  5698 | `					}` |
|      373 |  5699 | `				}` |
|        - |  5700 | `				/* Make sure the given arguments are of the correct type */` |
|    14276 |  5701 | `				if( aFormalArg[n].nType > 0 ){` |
|      984 |  5702 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  5703 | `						/* Argument must be a class instance [i.e: object] */` |
|      ! 0 |  5704 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  5705 | `						ph7_class *pClass;` |
|        - |  5706 | `						/* Try to extract the desired class */` |
|      ! 0 |  5707 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  5708 | `						if( pClass ){` |
|      ! 0 |  5709 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  5710 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5711 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5712 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5713 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5714 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5715 | `								}` |
|      ! 0 |  5716 | `							}else{` |
|        - |  5717 | `								/* reuse pThis declared in outer scope */` |
|      ! 0 |  5718 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  5719 | `								/* Make sure the object is an instance of the given class */` |
|      ! 0 |  5720 | `								if( ! VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  5721 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5722 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5723 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5724 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5725 | `								}` |
|        - |  5726 | `							}` |
|      ! 0 |  5727 | `						}` |
|      984 |  5728 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5729 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5730 | `						/* Cast to the desired type */` |
|      ! 0 |  5731 | `						xCast(pArg);` |
|      ! 0 |  5732 | `					}` |
|      491 |  5733 | `				}` |
|    14276 |  5734 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  5735 | `					/* Pass by reference */` |
|       25 |  5736 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  5737 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  5738 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  5739 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5740 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  5741 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  5742 | `						}` |
|        - |  5743 | `						/* Switch to pass by value */` |
|      ! 0 |  5744 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  5745 | `					}else{` |
|        - |  5746 | `						SyHashEntry *pRefEntry;` |
|        - |  5747 | `						/* Install the referenced variable in the private function frame */` |
|       25 |  5748 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       25 |  5749 | `						if( pRefEntry == 0 ){` |
|       37 |  5750 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       24 |  5751 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       25 |  5752 | `							sArg.nIdx = pArg->nIdx;` |
|       25 |  5753 | `							sArg.pUserData = 0;` |
|       25 |  5754 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       12 |  5755 | `						}` |
|       25 |  5756 | `						pObj = 0;` |
|        - |  5757 | `					}` |
|       13 |  5758 | `				}else{` |
|        - |  5759 | `					/* Pass by value,make a copy of the given argument */` |
|    14252 |  5760 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  5761 | `				}` |
|     7139 |  5762 | `			}else{` |
|        - |  5763 | `				char zName[32];` |
|        - |  5764 | `				SyString sArgName;` |
|        - |  5765 | `				/* Set a dummy name */` |
|      101 |  5766 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      101 |  5767 | `				sArgName.zString = zName;` |
|        - |  5768 | `				/* Annonymous argument */` |
|      101 |  5769 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  5770 | `			}` |
|    14376 |  5771 | `			if( pObj ){` |
|    14352 |  5772 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  5773 | `				/* Insert argument index  */` |
|    14352 |  5774 | `				sArg.nIdx = pObj->nIdx;` |
|    14352 |  5775 | `				sArg.pUserData = 0;` |
|    14352 |  5776 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|     7175 |  5777 | `			}` |
|    14376 |  5778 | `			PH7_MemObjRelease(pArg);` |
|    14376 |  5779 | `			pArg++;` |
|    14376 |  5780 | `			++n;` |
|        2 |  5781 | `		}` |
|        - |  5782 | `		/* Set up closure environment */` |
|     7724 |  5783 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  5784 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  5785 | `			ph7_value *pValue;` |
|        - |  5786 | `			sxu32 iEnv;` |
|        9 |  5787 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|       25 |  5788 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|       17 |  5789 | `				pEnv = &aEnv[iEnv];` |
|       17 |  5790 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  5791 | `					/* Do not install null value */` |
|        9 |  5792 | `					continue;` |
|        - |  5793 | `				}` |
|        9 |  5794 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|        9 |  5795 | `				if( pValue == 0 ){` |
|      ! 0 |  5796 | `					continue;` |
|        - |  5797 | `				}` |
|        - |  5798 | `				/* Invalidate any prior representation */` |
|        9 |  5799 | `				PH7_MemObjRelease(pValue);` |
|        - |  5800 | `				/* Duplicate bound variable value */` |
|        9 |  5801 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|        5 |  5802 | `			}` |
|        4 |  5803 | `		}` |
|        - |  5804 | `		/* Process default values */` |
|     8538 |  5805 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|      816 |  5806 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|      806 |  5807 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      806 |  5808 | `				if( pObj ){` |
|        - |  5809 | `					/* Evaluate the default value and extract it's result */` |
|      806 |  5810 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|      806 |  5811 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5812 | `						goto Abort;` |
|        - |  5813 | `					}` |
|        - |  5814 | `					/* Insert argument index */` |
|      806 |  5815 | `					sArg.nIdx = pObj->nIdx;` |
|      806 |  5816 | `					sArg.pUserData = 0;` |
|      806 |  5817 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  5818 | `					/* Make sure the default argument is of the correct type */` |
|      806 |  5819 | `					if( aFormalArg[n].nType > 0 && ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5820 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5821 | `						/* Cast to the desired type */` |
|      ! 0 |  5822 | `						xCast(pObj);` |
|      ! 0 |  5823 | `					}` |
|      402 |  5824 | `				}` |
|      402 |  5825 | `			}` |
|      816 |  5826 | `			++n;` |
|        2 |  5827 | `		}` |
|        - |  5828 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  5829 | `		 * does not return anything.` |
|        - |  5830 | `		 */` |
|     7724 |  5831 | `		PH7_MemObjRelease(pTos);` |
|     7724 |  5832 | `		pTos = &pTos[-pInstr->iP1];` |
|        - |  5833 | `		/* Allocate a new operand stack and evaluate the function body */` |
|     7724 |  5834 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|     7724 |  5835 | `		if( pFrameStack == 0 ){` |
|        - |  5836 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5837 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5838 | `				&pVmFunc->sName);` |
|      ! 0 |  5839 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5840 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5841 | `			}` |
|      ! 0 |  5842 | `			break;` |
|        - |  5843 | `		}` |
|     7724 |  5844 | `		if( pSelf ){` |
|        - |  5845 | `			/* Push class name */` |
|      372 |  5846 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|      185 |  5847 | `		}` |
|        - |  5848 | `		/* Increment nesting level */` |
|     7724 |  5849 | `		pVm->nRecursionDepth++;` |
|        - |  5850 | `		/* Execute function body */` |
|     7724 |  5851 | `		rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE);` |
|        - |  5852 | `		/* Decrement nesting level */` |
|     7724 |  5853 | `		pVm->nRecursionDepth--;` |
|     7724 |  5854 | `		if( pSelf ){` |
|        - |  5855 | `			/* Pop class name */` |
|      372 |  5856 | `			(void)SySetPop(&pVm->aSelf);` |
|      185 |  5857 | `		}` |
|        - |  5858 | `		/* Cleanup the mess left behind */` |
|     7724 |  5859 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  5860 | `			/* Return by reference,reflect that */` |
|        9 |  5861 | `			if( n != SXU32_HIGH ){` |
|        9 |  5862 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  5863 | `				sxu32 i;` |
|        - |  5864 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  5865 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  5866 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  5867 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  5868 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5869 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  5870 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  5871 | `								&pVmFunc->sName);` |
|      ! 0 |  5872 | `						}` |
|      ! 0 |  5873 | `						n = SXU32_HIGH;` |
|      ! 0 |  5874 | `						break;` |
|        - |  5875 | `					}` |
|        3 |  5876 | `				}` |
|        5 |  5877 | `			}else{` |
|      ! 0 |  5878 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5879 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  5880 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  5881 | `						&pVmFunc->sName);` |
|      ! 0 |  5882 | `				}` |
|        - |  5883 | `			}` |
|        9 |  5884 | `			pTos->nIdx = n;` |
|        4 |  5885 | `		}` |
|        - |  5886 | `		/* Cleanup the mess left behind */` |
|     7724 |  5887 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  5888 | `			/* An exception was throw in this frame */` |
|        7 |  5889 | `			pFrame = pFrame->pParent;` |
|        7 |  5890 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  5891 | `				/* Pop the resutlt */` |
|        5 |  5892 | `				VmPopOperand(&pTos,1);` |
|        - |  5893 | `				/* Jump to this destination */` |
|        5 |  5894 | `				pc = pFrame->iExceptionJump - 1;` |
|        5 |  5895 | `				rc = PH7_OK;` |
|        3 |  5896 | `			}else{` |
|        3 |  5897 | `				if( pFrame->pParent ){` |
|        3 |  5898 | `					rc = PH7_EXCEPTION;` |
|        2 |  5899 | `				}else{` |
|        - |  5900 | `					/* Continue normal execution */` |
|      ! 0 |  5901 | `					rc = PH7_OK;` |
|        - |  5902 | `				}` |
|        - |  5903 | `			}` |
|        3 |  5904 | `		}` |
|        - |  5905 | `		/* Free the operand stack */` |
|     7724 |  5906 | `		SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|        - |  5907 | `		/* Leave the frame */` |
|     7724 |  5908 | `		VmLeaveFrame(&(*pVm));` |
|     7724 |  5909 | `		if( rc == PH7_ABORT ){` |
|        - |  5910 | `			/* Abort processing immeditaley */` |
|      ! 0 |  5911 | `			goto Abort;` |
|     7724 |  5912 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  5913 | `			goto Exception;` |
|        - |  5914 | `		}` |
|     3862 |  5915 | `	}else{` |
|        - |  5916 | `		ph7_user_func *pFunc;` |
|        - |  5917 | `		ph7_context sCtx;` |
|        - |  5918 | `		ph7_value sRet;` |
|        - |  5919 | `		/* Look for an installed foreign function */` |
|   406106 |  5920 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   406106 |  5921 | `		if( pEntry == 0 ){` |
|        - |  5922 | `			/* Call to undefined function */` |
|        5 |  5923 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  5924 | `			/* Pop given arguments */` |
|        5 |  5925 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5926 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5927 | `			}` |
|        - |  5928 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  5929 | `			PH7_MemObjRelease(pTos);` |
|        5 |  5930 | `			break;` |
|        - |  5931 | `		}` |
|   406102 |  5932 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  5933 | `		/* Start collecting function arguments */` |
|   406102 |  5934 | `		SySetReset(&aArg);` |
|  1102566 |  5935 | `		while( pArg < pTos ){` |
|   696466 |  5936 | `			SySetPut(&aArg,(const void *)&pArg);` |
|   696466 |  5937 | `			pArg++;` |
|        2 |  5938 | `		}` |
|        - |  5939 | `		/* Assume a null return value */` |
|   406102 |  5940 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  5941 | `		/* Init the call context */` |
|   406102 |  5942 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  5943 | `		/* Call the foreign function */` |
|   406102 |  5944 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5945 | `		/* Release the call context */` |
|   406102 |  5946 | `		VmReleaseCallContext(&sCtx);` |
|   406102 |  5947 | `		if( rc == PH7_ABORT ){` |
|       15 |  5948 | `			goto Abort;` |
|   406088 |  5949 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  5950 | `			goto Exception;` |
|        - |  5951 | `		}` |
|   406086 |  5952 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5953 | `			/* Pop function name and arguments */` |
|   391440 |  5954 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   195741 |  5955 | `		}` |
|        - |  5956 | `		/* Save foreign function return value */` |
|   406086 |  5957 | `		PH7_MemObjStore(&sRet,pTos);` |
|   406086 |  5958 | `		PH7_MemObjRelease(&sRet);` |
|        - |  5959 | `	}` |
|   413806 |  5960 | `	break;` |
|        - |  5961 | `				  }` |
|        - |  5962 | `/*` |
|        - |  5963 | ` * OP_CONSUME: P1 * *` |
|        - |  5964 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  5965 | ` */` |
|     8145 |  5966 | `case PH7_OP_CONSUME: {` |
|    16292 |  5967 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    16292 |  5968 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  5969 |  |
|    16292 |  5970 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    16292 |  5971 | `	pCur = pOut;` |
|        - |  5972 | `	/* Start the consume process  */` |
|    32612 |  5973 | `	while( pOut <= pTos ){` |
|        - |  5974 | `		/* Force a string cast */` |
|    16322 |  5975 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|       56 |  5976 | `			PH7_MemObjToString(pOut);` |
|       27 |  5977 | `		}` |
|    16322 |  5978 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  5979 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  5980 | `			/* Invoke the output consumer callback */` |
|     8850 |  5981 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|     8850 |  5982 | `			if( pCons->xConsumer != VmObConsumer ){` |
|        - |  5983 | `				/* Increment output length */` |
|     3350 |  5984 | `				pVm->nOutputLen += SyBlobLength(&pOut->sBlob);` |
|     1674 |  5985 | `			}` |
|     8850 |  5986 | `			SyBlobRelease(&pOut->sBlob);` |
|     8850 |  5987 | `			if( rc == SXERR_ABORT ){` |
|        - |  5988 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  5989 | `				goto Abort;` |
|        - |  5990 | `			}` |
|     4424 |  5991 | `		}` |
|    16322 |  5992 | `		pOut++;` |
|        2 |  5993 | `	}` |
|    16292 |  5994 | `	pTos = &pCur[-1];` |
|    16290 |  5995 | `	break;` |
|        - |  5996 | `					 }` |
|        - |  5997 |  |
|        - |  5998 | `		} /* Switch() */` |
|  6622828 |  5999 | `		pc++; /* Next instruction in the stream */` |
|        2 |  6000 | `	} /* For(;;) */` |
|    10031 |  6001 | `Done:` |
|    20064 |  6002 | `	SySetRelease(&aArg);` |
|    20064 |  6003 | `	return SXRET_OK;` |
|        8 |  6004 | `Abort:` |
|       17 |  6005 | `	SySetRelease(&aArg);` |
|       45 |  6006 | `	while( pTos >= pStack ){` |
|       29 |  6007 | `		PH7_MemObjRelease(pTos);` |
|       29 |  6008 | `		pTos--;` |
|        1 |  6009 | `	}` |
|       17 |  6010 | `	return PH7_ABORT;` |
|        2 |  6011 | `Exception:` |
|        5 |  6012 | `	SySetRelease(&aArg);` |
|        9 |  6013 | `	while( pTos >= pStack ){` |
|        5 |  6014 | `		PH7_MemObjRelease(pTos);` |
|        5 |  6015 | `		pTos--;` |
|        1 |  6016 | `	}` |
|        5 |  6017 | `	return PH7_EXCEPTION;` |
|    10043 |  6018 |  |
|        - |  6019 | `/*` |
|        - |  6020 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  6021 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6022 | ` * See block-comment on that function for additional information.` |
|        - |  6023 | ` */` |
|    10884 |  6024 | `static sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  6025 |  |
|        - |  6026 | `	ph7_value *pStack;` |
|        - |  6027 | `	sxi32 rc;` |
|        - |  6028 | `	/* Allocate a new operand stack */` |
|    10886 |  6029 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    10886 |  6030 | `	if( pStack == 0 ){` |
|      ! 0 |  6031 | `		return SXERR_MEM;` |
|        - |  6032 | `	}` |
|        - |  6033 | `	/* Execute the program */` |
|    10886 |  6034 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE);` |
|        - |  6035 | `	/* Free the operand stack */` |
|    10886 |  6036 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  6037 | `	/* Execution result */` |
|    10886 |  6038 | `	return rc;` |
|     5444 |  6039 |  |
|        - |  6040 | `/*` |
|        - |  6041 | ` * Invoke any installed shutdown callbacks.` |
|        - |  6042 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  6043 | ` * or more calls to [register_shutdown_function()].` |
|        - |  6044 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  6045 | ` * execution ends.` |
|        - |  6046 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  6047 | ` * additional information.` |
|        - |  6048 | ` */` |
|      948 |  6049 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  6050 |  |
|        - |  6051 | `	VmShutdownCB *pEntry;` |
|        - |  6052 | `	ph7_value *apArg[10];` |
|        - |  6053 | `	sxu32 n,nEntry;` |
|        - |  6054 | `	int i;` |
|        - |  6055 | `	/* Point to the stack of registered callbacks */` |
|      950 |  6056 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    10430 |  6057 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|     9482 |  6058 | `		apArg[i] = 0;` |
|     4742 |  6059 | `	}` |
|      952 |  6060 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  6061 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6062 | `		if( pEntry ){` |
|        - |  6063 | `			/* Prepare callback arguments if any */` |
|        3 |  6064 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  6065 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  6066 | `					break;` |
|        - |  6067 | `				}` |
|      ! 0 |  6068 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  6069 | `			}` |
|        - |  6070 | `			/* Invoke the callback */` |
|        3 |  6071 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  6072 | `			/*` |
|        - |  6073 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  6074 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  6075 | `			 */` |
|        3 |  6076 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6077 | `			if( pEntry ){` |
|        3 |  6078 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  6079 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  6080 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  6081 | `				}` |
|        1 |  6082 | `			}` |
|        1 |  6083 | `		}` |
|        2 |  6084 | `	}` |
|      950 |  6085 | `	SySetReset(&pVm->aShutdown);` |
|      950 |  6086 |  |
|        - |  6087 | `/*` |
|        - |  6088 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  6089 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6090 | ` * See block-comment on that function for additional information.` |
|        - |  6091 | ` */` |
|      956 |  6092 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  6093 |  |
|        - |  6094 | `	/* Make sure we are ready to execute this program */` |
|      958 |  6095 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  6096 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  6097 | `	}` |
|        - |  6098 | `	/* Set the execution magic number  */` |
|      958 |  6099 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  6100 | `	/* Execute the program */` |
|      958 |  6101 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE);` |
|        - |  6102 | `	/* Invoke any shutdown callbacks */` |
|      954 |  6103 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  6104 | `	/*` |
|        - |  6105 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  6106 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  6107 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  6108 | `	 */` |
|      954 |  6109 | `	return SXRET_OK;` |
|      480 |  6110 |  |
|        - |  6111 | `/*` |
|        - |  6112 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  6113 | ` * the desired message.` |
|        - |  6114 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  6115 | ` * in 'api.c' for additional information.` |
|        - |  6116 | ` */` |
|      380 |  6117 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  6118 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  6119 | `	SyString *pString /* Message to output */` |
|        - |  6120 | `	)` |
|        2 |  6121 |  |
|      382 |  6122 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      382 |  6123 | `	sxi32 rc = SXRET_OK;` |
|        - |  6124 | `	/* Call the output consumer */` |
|      382 |  6125 | `	if( pString->nByte > 0 ){` |
|      382 |  6126 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      382 |  6127 | `		if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6128 | `			/* Increment output length */` |
|       17 |  6129 | `			pVm->nOutputLen += pString->nByte;` |
|        8 |  6130 | `		}` |
|      190 |  6131 | `	}` |
|      382 |  6132 | `	return rc;` |
|        2 |  6133 |  |
|        - |  6134 | `/*` |
|        - |  6135 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  6136 | ` * callback to consume the formatted message.` |
|        - |  6137 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  6138 | ` * in 'api.c' for additional information.` |
|        - |  6139 | ` */` |
|        2 |  6140 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  6141 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  6142 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  6143 | `	va_list ap           /* Variable list of arguments */` |
|        - |  6144 | `	)` |
|        1 |  6145 |  |
|        3 |  6146 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  6147 | `	sxi32 rc = SXRET_OK;` |
|        - |  6148 | `	SyBlob sWorker;` |
|        - |  6149 | `	/* Format the message and call the output consumer */` |
|        3 |  6150 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  6151 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  6152 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  6153 | `		/* Consume the formatted message */` |
|        3 |  6154 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  6155 | `	}` |
|        3 |  6156 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6157 | `		/* Increment output length */` |
|      ! 0 |  6158 | `		pVm->nOutputLen += SyBlobLength(&sWorker);` |
|      ! 0 |  6159 | `	}` |
|        - |  6160 | `	/* Release the working buffer */` |
|        3 |  6161 | `	SyBlobRelease(&sWorker);` |
|        3 |  6162 | `	return rc;` |
|        1 |  6163 |  |
|        - |  6164 | `/*` |
|        - |  6165 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  6166 | ` * This function never fail and always return a pointer` |
|        - |  6167 | ` * to a null terminated string.` |
|        - |  6168 | ` */` |
|       10 |  6169 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  6170 |  |
|       11 |  6171 | `	const char *zOp = "Unknown     ";` |
|       11 |  6172 | `	switch(nOp){` |
|        3 |  6173 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  6174 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  6175 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  6176 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  6177 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  6178 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  6179 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  6180 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  6181 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  6182 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  6183 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  6184 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  6185 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  6186 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  6187 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  6188 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  6189 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  6190 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  6191 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  6192 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  6193 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  6194 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  6195 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  6196 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  6197 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  6198 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  6199 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  6200 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  6201 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  6202 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  6203 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  6204 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  6205 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  6206 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  6207 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  6208 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  6209 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  6210 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  6211 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  6212 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  6213 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  6214 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  6215 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  6216 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  6217 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  6218 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  6219 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  6220 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  6221 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  6222 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  6223 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  6224 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  6225 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  6226 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  6227 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  6228 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  6229 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  6230 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  6231 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  6232 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  6233 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  6234 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  6235 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  6236 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  6237 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  6238 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  6239 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  6240 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  6241 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  6242 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  6243 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  6244 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  6245 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  6246 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  6247 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  6248 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  6249 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  6250 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  6251 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  6252 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  6253 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  6254 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  6255 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  6256 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  6257 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  6258 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  6259 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  6260 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  6261 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  6262 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  6263 | `	default:` |
|      ! 0 |  6264 | `		break;` |
|        - |  6265 | `	}` |
|       11 |  6266 | `	return zOp;` |
|        1 |  6267 |  |
|        - |  6268 | `/*` |
|        - |  6269 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  6270 | ` * The xConsumer() callback which is an used defined function` |
|        - |  6271 | ` * is responsible of consuming the generated dump.` |
|        - |  6272 | ` */` |
|        2 |  6273 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  6274 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  6275 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  6276 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  6277 | `	)` |
|        1 |  6278 |  |
|        - |  6279 | `	sxi32 rc;` |
|        3 |  6280 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  6281 | `	return rc;` |
|        1 |  6282 |  |
|        - |  6283 | `/*` |
|        - |  6284 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  6285 | ` * outside a class body [i.e: global or function scope].` |
|        - |  6286 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  6287 | ` * in 'compile.c' for additional information.` |
|        - |  6288 | ` */` |
|        8 |  6289 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  6290 |  |
|        9 |  6291 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  6292 | `	/* Evaluate and expand constant value */` |
|        9 |  6293 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|        9 |  6294 |  |
|        - |  6295 | `/*` |
|        - |  6296 | ` * Section:` |
|        - |  6297 | ` *  Function handling functions.` |
|        - |  6298 | ` * Status:` |
|        - |  6299 | ` *    Stable.` |
|        - |  6300 | ` */` |
|        - |  6301 | `/*` |
|        - |  6302 | ` * int func_num_args(void)` |
|        - |  6303 | ` *   Returns the number of arguments passed to the function.` |
|        - |  6304 | ` * Parameters` |
|        - |  6305 | ` *   None.` |
|        - |  6306 | ` * Return` |
|        - |  6307 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  6308 | ` *  or -1 if called from the globe scope.` |
|        - |  6309 | ` */` |
|      752 |  6310 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6311 |  |
|        - |  6312 | `	VmFrame *pFrame;` |
|        - |  6313 | `	ph7_vm *pVm;` |
|        - |  6314 | `	/* Point to the target VM */` |
|      754 |  6315 | `	pVm = pCtx->pVm;` |
|        - |  6316 | `	/* Current frame */` |
|      754 |  6317 | `	pFrame = pVm->pFrame;` |
|      754 |  6318 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6319 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6320 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6321 | `	}` |
|      754 |  6322 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  6323 | `		SXUNUSED(nArg);` |
|      ! 0 |  6324 | `		SXUNUSED(apArg);` |
|        - |  6325 | `		/* Global frame,return -1 */` |
|      ! 0 |  6326 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  6327 | `		return SXRET_OK;` |
|        - |  6328 | `	}` |
|        - |  6329 | `	/* Total number of arguments passed to the enclosing function */` |
|      754 |  6330 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      754 |  6331 | `	ph7_result_int(pCtx,nArg);` |
|      754 |  6332 | `	return SXRET_OK;` |
|      378 |  6333 |  |
|        - |  6334 | `/*` |
|        - |  6335 | ` * value func_get_arg(int $arg_num)` |
|        - |  6336 | ` *   Return an item from the argument list.` |
|        - |  6337 | ` * Parameters` |
|        - |  6338 | ` *  Argument number(index start from zero).` |
|        - |  6339 | ` * Return` |
|        - |  6340 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  6341 | ` */` |
|        6 |  6342 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6343 |  |
|        8 |  6344 | `	ph7_value *pObj = 0;` |
|        8 |  6345 | `	VmSlot *pSlot = 0;` |
|        - |  6346 | `	VmFrame *pFrame;` |
|        - |  6347 | `	ph7_vm *pVm;` |
|        - |  6348 | `	/* Point to the target VM */` |
|        8 |  6349 | `	pVm = pCtx->pVm;` |
|        - |  6350 | `	/* Current frame */` |
|        8 |  6351 | `	pFrame = pVm->pFrame;` |
|        8 |  6352 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6353 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6354 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6355 | `	}` |
|        8 |  6356 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  6357 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  6358 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  6359 | `		ph7_result_bool(pCtx,0);` |
|        3 |  6360 | `		return SXRET_OK;` |
|        - |  6361 | `	}` |
|        - |  6362 | `	/* Extract the desired index */` |
|        5 |  6363 | `	nArg = ph7_value_to_int(apArg[0]);` |
|        5 |  6364 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  6365 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  6366 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6367 | `		return SXRET_OK;` |
|        - |  6368 | `	}` |
|        - |  6369 | `	/* Extract the desired argument */` |
|        5 |  6370 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|        5 |  6371 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  6372 | `			/* Return the desired argument */` |
|        5 |  6373 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|        3 |  6374 | `		}else{` |
|        - |  6375 | `			/* No such argument,return false */` |
|      ! 0 |  6376 | `			ph7_result_bool(pCtx,0);` |
|        - |  6377 | `		}` |
|        3 |  6378 | `	}else{` |
|        - |  6379 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  6380 | `		ph7_result_bool(pCtx,0);` |
|        - |  6381 | `	}` |
|        5 |  6382 | `	return SXRET_OK;` |
|        5 |  6383 |  |
|        - |  6384 | `/*` |
|        - |  6385 | ` * array func_get_args_byref(void)` |
|        - |  6386 | ` *   Returns an array comprising a function's argument list.` |
|        - |  6387 | ` * Parameters` |
|        - |  6388 | ` *  None.` |
|        - |  6389 | ` * Return` |
|        - |  6390 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  6391 | ` *  member of the current user-defined function's argument list.` |
|        - |  6392 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6393 | ` * NOTE:` |
|        - |  6394 | ` *  Arguments are returned to the array by reference.` |
|        - |  6395 | ` */` |
|        2 |  6396 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6397 |  |
|        - |  6398 | `	ph7_value *pArray;` |
|        - |  6399 | `	VmFrame *pFrame;` |
|        - |  6400 | `	VmSlot *aSlot;` |
|        - |  6401 | `	sxu32 n;` |
|        - |  6402 | `	/* Point to the current frame */` |
|        3 |  6403 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  6404 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6405 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6406 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6407 | `	}` |
|        3 |  6408 | `	if( pFrame->pParent == 0 ){` |
|        - |  6409 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6410 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6411 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6412 | `		return SXRET_OK;` |
|        - |  6413 | `	}` |
|        - |  6414 | `	/* Create a new array */` |
|        3 |  6415 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6416 | `	if( pArray == 0 ){` |
|      ! 0 |  6417 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6418 | `		SXUNUSED(apArg);` |
|      ! 0 |  6419 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6420 | `		return SXRET_OK;` |
|        - |  6421 | `	}` |
|        - |  6422 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  6423 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  6424 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  6425 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  6426 | `	}` |
|        - |  6427 | `	/* Return the freshly created array */` |
|        3 |  6428 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6429 | `	return SXRET_OK;` |
|        2 |  6430 |  |
|        - |  6431 | `/*` |
|        - |  6432 | ` * array func_get_args(void)` |
|        - |  6433 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  6434 | ` * Parameters` |
|        - |  6435 | ` *  None.` |
|        - |  6436 | ` * Return` |
|        - |  6437 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  6438 | ` *  member of the current user-defined function's argument list.` |
|        - |  6439 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6440 | ` */` |
|       46 |  6441 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6442 |  |
|       47 |  6443 | `	ph7_value *pObj = 0;` |
|        - |  6444 | `	ph7_value *pArray;` |
|        - |  6445 | `	VmFrame *pFrame;` |
|        - |  6446 | `	VmSlot *aSlot;` |
|        - |  6447 | `	sxu32 n;` |
|        - |  6448 | `	/* Point to the current frame */` |
|       47 |  6449 | `	pFrame = pCtx->pVm->pFrame;` |
|       47 |  6450 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6451 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6452 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6453 | `	}` |
|       47 |  6454 | `	if( pFrame->pParent == 0 ){` |
|        - |  6455 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6456 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6457 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6458 | `		return SXRET_OK;` |
|        - |  6459 | `	}` |
|        - |  6460 | `	/* Create a new array */` |
|       47 |  6461 | `	pArray = ph7_context_new_array(pCtx);` |
|       47 |  6462 | `	if( pArray == 0 ){` |
|      ! 0 |  6463 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6464 | `		SXUNUSED(apArg);` |
|      ! 0 |  6465 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6466 | `		return SXRET_OK;` |
|        - |  6467 | `	}` |
|        - |  6468 | `	/* Start filling the array with the given arguments */` |
|       47 |  6469 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      143 |  6470 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|       97 |  6471 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|       97 |  6472 | `		if( pObj ){` |
|       97 |  6473 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       48 |  6474 | `		}` |
|       49 |  6475 | `	}` |
|        - |  6476 | `	/* Return the freshly created array */` |
|       47 |  6477 | `	ph7_result_value(pCtx,pArray);` |
|       47 |  6478 | `	return SXRET_OK;` |
|       24 |  6479 |  |
|        - |  6480 | `/*` |
|        - |  6481 | ` * bool function_exists(string $name)` |
|        - |  6482 | ` *  Return TRUE if the given function has been defined.` |
|        - |  6483 | ` * Parameters` |
|        - |  6484 | ` *  The name of the desired function.` |
|        - |  6485 | ` * Return` |
|        - |  6486 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  6487 | ` */` |
|     1722 |  6488 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6489 |  |
|        - |  6490 | `	const char *zName;` |
|        - |  6491 | `	ph7_vm *pVm;` |
|        - |  6492 | `	int nLen;` |
|        - |  6493 | `	int res;` |
|     1724 |  6494 | `	if( nArg < 1 ){` |
|        - |  6495 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  6496 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6497 | `		return SXRET_OK;` |
|        - |  6498 | `	}` |
|        - |  6499 | `	/* Point to the target VM */` |
|     1724 |  6500 | `	pVm = pCtx->pVm;` |
|        - |  6501 | `	/* Extract the function name */` |
|     1724 |  6502 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  6503 | `	/* Assume the function is not defined */` |
|     1724 |  6504 | `	res = 0;` |
|        - |  6505 | `	/* Perform the lookup */` |
|     2583 |  6506 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1718 |  6507 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6508 | `			/* Function is defined */` |
|      212 |  6509 | `			res = 1;` |
|      105 |  6510 | `	}` |
|     1724 |  6511 | `	ph7_result_bool(pCtx,res);` |
|     1724 |  6512 | `	return SXRET_OK;` |
|      863 |  6513 |  |
|        - |  6514 | `/* Forward declaration */` |
|        - |  6515 | `static ph7_class * VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg);` |
|        - |  6516 | `/*` |
|        - |  6517 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6518 | ` * [i.e: Whether it is callable or not].` |
|        - |  6519 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  6520 | ` */` |
|    11416 |  6521 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  6522 |  |
|    11418 |  6523 | `	int res = 0;` |
|    11418 |  6524 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  6525 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  6526 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  6527 | `		ph7_class_method *pMethod;` |
|      ! 0 |  6528 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  6529 | `		if( pMethod && CallInvoke ){` |
|        - |  6530 | `			ph7_value sResult;` |
|        - |  6531 | `			sxi32 rc;` |
|        - |  6532 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  6533 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  6534 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  6535 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  6536 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  6537 | `			}` |
|      ! 0 |  6538 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  6539 | `		}` |
|    11418 |  6540 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        7 |  6541 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        7 |  6542 | `		if( pMap->nEntry > 1 ){` |
|        - |  6543 | `			ph7_class *pClass;` |
|        - |  6544 | `			ph7_value *pV;` |
|        - |  6545 | `			/* Extract the target class */` |
|        7 |  6546 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|        7 |  6547 | `			if( pV ){` |
|        7 |  6548 | `				pClass = VmExtractClassFromValue(pVm,pV);` |
|        7 |  6549 | `				if( pClass ){` |
|        - |  6550 | `					ph7_class_method *pMethod;` |
|        - |  6551 | `					/* Extract the target method */` |
|        7 |  6552 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|        7 |  6553 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  6554 | `						/* Perform the lookup */` |
|        7 |  6555 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|        7 |  6556 | `						if( pMethod ){` |
|        - |  6557 | `							/* Method is callable */` |
|        5 |  6558 | `							res = 1;` |
|        2 |  6559 | `						}` |
|        3 |  6560 | `					}` |
|        3 |  6561 | `				}` |
|        3 |  6562 | `			}` |
|        4 |  6563 | `		}` |
|    11415 |  6564 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  6565 | `		const char *zName;` |
|        - |  6566 | `		int nLen;` |
|        - |  6567 | `		/* Extract the name */` |
|     2798 |  6568 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  6569 | `		/* Perform the lookup */` |
|     2801 |  6570 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|        6 |  6571 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6572 | `				/* Function is callable */` |
|     2794 |  6573 | `				res = 1;` |
|     1396 |  6574 | `		}` |
|     1398 |  6575 | `	}` |
|    11418 |  6576 | `	return res;` |
|        2 |  6577 |  |
|        - |  6578 | `/*` |
|        - |  6579 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  6580 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6581 | ` * Parameters` |
|        - |  6582 | ` * $name` |
|        - |  6583 | ` *    The callback function to check` |
|        - |  6584 | ` * $syntax_only` |
|        - |  6585 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  6586 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  6587 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  6588 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  6589 | ` *    a string.` |
|        - |  6590 | ` * Return` |
|        - |  6591 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  6592 | ` */` |
|       14 |  6593 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6594 |  |
|        - |  6595 | `	ph7_vm *pVm;` |
|        - |  6596 | `	int res;` |
|       15 |  6597 | `	if( nArg < 1 ){` |
|        - |  6598 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  6599 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6600 | `		return SXRET_OK;` |
|        - |  6601 | `	}` |
|        - |  6602 | `	/* Point to the target VM */` |
|       15 |  6603 | `	pVm = pCtx->pVm;` |
|        - |  6604 | `	/* Perform the requested operation */` |
|       15 |  6605 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  6606 | `	ph7_result_bool(pCtx,res);` |
|       15 |  6607 | `	return SXRET_OK;` |
|        8 |  6608 |  |
|        - |  6609 | `/*` |
|        - |  6610 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  6611 | ` * defined below.` |
|        - |  6612 | ` */` |
|     1040 |  6613 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  6614 |  |
|     1041 |  6615 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  6616 | `	ph7_value sName;` |
|        - |  6617 | `	sxi32 rc;` |
|        - |  6618 | `	/* Prepare the function name for insertion */` |
|     1041 |  6619 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1041 |  6620 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  6621 | `	/* Perform the insertion */` |
|     1041 |  6622 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1041 |  6623 | `	PH7_MemObjRelease(&sName);` |
|     1041 |  6624 | `	return rc;` |
|        1 |  6625 |  |
|        - |  6626 | `/*` |
|        - |  6627 | ` * array get_defined_functions(void)` |
|        - |  6628 | ` *  Returns an array of all defined functions.` |
|        - |  6629 | ` * Parameter` |
|        - |  6630 | ` *  None.` |
|        - |  6631 | ` * Return` |
|        - |  6632 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  6633 | ` *  both built-in (internal) and user-defined.` |
|        - |  6634 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  6635 | ` *  defined ones using $arr["user"].` |
|        - |  6636 | ` * Note:` |
|        - |  6637 | ` *  NULL is returned on failure.` |
|        - |  6638 | ` */` |
|        2 |  6639 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6640 |  |
|        - |  6641 | `	ph7_value *pArray,*pEntry;` |
|        - |  6642 | `	/* NOTE:` |
|        - |  6643 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  6644 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  6645 | `	 */` |
|        3 |  6646 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6647 | ` 	if( pArray == 0 ){` |
|      ! 0 |  6648 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6649 | `		SXUNUSED(apArg);` |
|        - |  6650 | `		/* Return NULL */` |
|      ! 0 |  6651 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6652 | `		return SXRET_OK;` |
|        - |  6653 | `	}` |
|        3 |  6654 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6655 | `	if( pEntry == 0 ){` |
|        - |  6656 | `		/* Return NULL */` |
|      ! 0 |  6657 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6658 | `		return SXRET_OK;` |
|        - |  6659 | `	}` |
|        - |  6660 | `	/* Fill with the appropriate information */` |
|        3 |  6661 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  6662 | `	/* Create the 'internal' index */` |
|        3 |  6663 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  6664 | `	/* Create the user-func array */` |
|        3 |  6665 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6666 | `	if( pEntry == 0 ){` |
|        - |  6667 | `		/* Return NULL */` |
|      ! 0 |  6668 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6669 | `		return SXRET_OK;` |
|        - |  6670 | `	}` |
|        - |  6671 | `	/* Fill with the appropriate information */` |
|        3 |  6672 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  6673 | `	/* Create the 'user' index */` |
|        3 |  6674 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  6675 | `	/* Return the multi-dimensional array */` |
|        3 |  6676 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6677 | `	return SXRET_OK;` |
|        2 |  6678 |  |
|        - |  6679 | `/*` |
|        - |  6680 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  6681 | ` *  Register a function for execution on shutdown.` |
|        - |  6682 | ` * Note` |
|        - |  6683 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  6684 | ` *  be called in the same order as they were registered.` |
|        - |  6685 | ` * Parameters` |
|        - |  6686 | ` *  $callback` |
|        - |  6687 | ` *   The shutdown callback to register.` |
|        - |  6688 | ` * $param` |
|        - |  6689 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  6690 | ` * Return` |
|        - |  6691 | ` *  Nothing.` |
|        - |  6692 | ` */` |
|        2 |  6693 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6694 |  |
|        - |  6695 | `	VmShutdownCB sEntry;` |
|        - |  6696 | `	int i,j;` |
|        3 |  6697 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  6698 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  6699 | `		return PH7_OK;` |
|        - |  6700 | `	}` |
|        - |  6701 | `	/* Zero the Entry */` |
|        3 |  6702 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  6703 | `	/* Initialize fields */` |
|        3 |  6704 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  6705 | `	/* Save the callback name for later invocation name */` |
|        3 |  6706 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  6707 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  6708 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  6709 | `	}` |
|        - |  6710 | `	/* Copy arguments */` |
|        3 |  6711 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  6712 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  6713 | `			/* Limit reached */` |
|      ! 0 |  6714 | `			break;` |
|        - |  6715 | `		}` |
|      ! 0 |  6716 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  6717 | `	}` |
|        3 |  6718 | `	sEntry.nArg = j;` |
|        - |  6719 | `	/* Install the callback */` |
|        3 |  6720 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  6721 | `	return PH7_OK;` |
|        2 |  6722 |  |
|        - |  6723 | `/*` |
|        - |  6724 | ` * Section:` |
|        - |  6725 | ` *  Class handling functions.` |
|        - |  6726 | ` * Status:` |
|        - |  6727 | ` *    Stable.` |
|        - |  6728 | ` */` |
|        - |  6729 | `/*` |
|        - |  6730 | ` * Extract the top active class. NULL is returned` |
|        - |  6731 | ` * if the class stack is empty.` |
|        - |  6732 | ` */` |
|       56 |  6733 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  6734 |  |
|       58 |  6735 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  6736 | `	ph7_class **apClass;` |
|       58 |  6737 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  6738 | `		/* Empty stack,return NULL */` |
|       15 |  6739 | `		return 0;` |
|        - |  6740 | `	}` |
|        - |  6741 | `	/* Peek the last entry */` |
|       44 |  6742 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|       44 |  6743 | `	return apClass[pSet->nUsed - 1];` |
|       30 |  6744 |  |
|        - |  6745 | `/*` |
|        - |  6746 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  6747 | ` *   Get the class that declared the currently executing method.` |
|        - |  6748 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  6749 | ` *` |
|        - |  6750 | ` * Parameters` |
|        - |  6751 | ` *   pVm: Target VM` |
|        - |  6752 | ` *` |
|        - |  6753 | ` * Return` |
|        - |  6754 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  6755 | ` *   - Not executing within a class method` |
|        - |  6756 | ` *` |
|        - |  6757 | ` * Note` |
|        - |  6758 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  6759 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  6760 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  6761 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  6762 | ` *   declaring class.` |
|        - |  6763 | ` */` |
|       18 |  6764 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        1 |  6765 |  |
|       19 |  6766 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  6767 | `	ph7_vm_func *pVmFunc;` |
|        - |  6768 |  |
|        - |  6769 | `	/* Skip exception frames to find the actual method frame */` |
|       19 |  6770 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  6771 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6772 | `	}` |
|        - |  6773 |  |
|        - |  6774 | `	/* Check if we're in a method context */` |
|       19 |  6775 | `	if( pFrame->pParent ){` |
|       15 |  6776 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       15 |  6777 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  6778 | `			/* Return the declaring class */` |
|       15 |  6779 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  6780 | `		}` |
|      ! 0 |  6781 | `	}` |
|        - |  6782 |  |
|        5 |  6783 | `	return 0;` |
|       10 |  6784 |  |
|        - |  6785 |  |
|        - |  6786 | `/*` |
|        - |  6787 | ` * string get_class ([ object $object = NULL ] )` |
|        - |  6788 | ` *   Returns the name of the class of an object` |
|        - |  6789 | ` * Parameters` |
|        - |  6790 | ` *  object` |
|        - |  6791 | ` *   The tested object. This parameter may be omitted when inside a class.` |
|        - |  6792 | ` * Return` |
|        - |  6793 | ` *  The name of the class of which object is an instance.` |
|        - |  6794 | ` *  Returns FALSE if object is not an object.` |
|        - |  6795 | ` *  If object is omitted when inside a class, the name of that class is returned.` |
|        - |  6796 | ` */` |
|       18 |  6797 | `static int vm_builtin_get_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6798 |  |
|        - |  6799 | `	ph7_class *pClass;` |
|        - |  6800 | `	SyString *pName;` |
|       20 |  6801 | `	if( nArg < 1 ){` |
|        - |  6802 | `		/* Check if we are inside a class */` |
|      ! 0 |  6803 | `		pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|      ! 0 |  6804 | `		if( pClass ){` |
|        - |  6805 | `			/* Point to the class name */` |
|      ! 0 |  6806 | `			pName = &pClass->sName;` |
|      ! 0 |  6807 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|      ! 0 |  6808 | `		}else{` |
|        - |  6809 | `			/* Not inside class,return FALSE */` |
|      ! 0 |  6810 | `			ph7_result_bool(pCtx,0);` |
|        - |  6811 | `		}` |
|      ! 0 |  6812 | `	}else{` |
|        - |  6813 | `		/* Extract the target class */` |
|       20 |  6814 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|       20 |  6815 | `		if( pClass ){` |
|       18 |  6816 | `			pName = &pClass->sName;` |
|        - |  6817 | `			/* Return the class name */` |
|       18 |  6818 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|       10 |  6819 | `		}else{` |
|        - |  6820 | `			/* Not a class instance,return FALSE */` |
|        3 |  6821 | `			ph7_result_bool(pCtx,0);` |
|        - |  6822 | `		}` |
|        - |  6823 | `	}` |
|       20 |  6824 | `	return PH7_OK;` |
|        2 |  6825 |  |
|        - |  6826 | `/*` |
|        - |  6827 | ` * string get_parent_class([object $object = NULL ] )` |
|        - |  6828 | ` *   Returns the name of the parent class of an object` |
|        - |  6829 | ` * Parameters` |
|        - |  6830 | ` *  object` |
|        - |  6831 | ` *   The tested object. This parameter may be omitted when inside a class.` |
|        - |  6832 | ` * Return` |
|        - |  6833 | ` *  The name of the parent class of which object is an instance.` |
|        - |  6834 | ` *  Returns FALSE if object is not an object or if the object does` |
|        - |  6835 | ` *  not have a parent.` |
|        - |  6836 | ` *  If object is omitted when inside a class, the name of that class is returned.` |
|        - |  6837 | ` */` |
|        8 |  6838 | `static int vm_builtin_get_parent_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6839 |  |
|        - |  6840 | `	ph7_class *pClass;` |
|        - |  6841 | `	SyString *pName;` |
|        9 |  6842 | `	if( nArg < 1 ){` |
|        - |  6843 | `		/* Check if we are inside a class [i.e: a method call]*/` |
|        3 |  6844 | `		pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|        3 |  6845 | `		if( pClass && pClass->pBase ){` |
|        - |  6846 | `			/* Point to the class name */` |
|        3 |  6847 | `			pName = &pClass->pBase->sName;` |
|        3 |  6848 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|        2 |  6849 | `		}else{` |
|        - |  6850 | `			/* Not inside class,return FALSE */` |
|      ! 0 |  6851 | `			ph7_result_bool(pCtx,0);` |
|        - |  6852 | `		}` |
|        2 |  6853 | `	}else{` |
|        - |  6854 | `		/* Extract the target class */` |
|        7 |  6855 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        7 |  6856 | `		if( pClass ){` |
|        7 |  6857 | `			if( pClass->pBase ){` |
|        5 |  6858 | `				pName = &pClass->pBase->sName;` |
|        - |  6859 | `				/* Return the parent class name */` |
|        5 |  6860 | `				ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|        3 |  6861 | `			}else{` |
|        - |  6862 | `				/* Object does not have a parent class */` |
|        3 |  6863 | `				ph7_result_bool(pCtx,0);` |
|        - |  6864 | `			}` |
|        4 |  6865 | `		}else{` |
|        - |  6866 | `			/* Not a class instance,return FALSE */` |
|      ! 0 |  6867 | `			ph7_result_bool(pCtx,0);` |
|        - |  6868 | `		}` |
|        - |  6869 | `	}` |
|        9 |  6870 | `	return PH7_OK;` |
|        1 |  6871 |  |
|        - |  6872 | `/*` |
|        - |  6873 | ` * string get_called_class(void)` |
|        - |  6874 | ` *   Gets the name of the class the static method is called in.` |
|        - |  6875 | ` * Parameters` |
|        - |  6876 | ` *  None.` |
|        - |  6877 | ` * Return` |
|        - |  6878 | ` *  Returns the class name. Returns FALSE if called from outside a class.` |
|        - |  6879 | ` */` |
|        4 |  6880 | `static int vm_builtin_get_called_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6881 |  |
|        - |  6882 | `	ph7_class *pClass;` |
|        - |  6883 | `	/* Check if we are inside a class [i.e: a method call] */` |
|        5 |  6884 | `	pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|        5 |  6885 | `	if( pClass ){` |
|        - |  6886 | `		SyString *pName;` |
|        - |  6887 | `		/* Point to the class name */` |
|        5 |  6888 | `		pName = &pClass->sName;` |
|        5 |  6889 | `		ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|        3 |  6890 | `	}else{` |
|      ! 0 |  6891 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6892 | `		SXUNUSED(apArg);` |
|        - |  6893 | `		/* Not inside class,return FALSE */` |
|      ! 0 |  6894 | `		ph7_result_bool(pCtx,0);` |
|        - |  6895 | `	}` |
|        5 |  6896 | `	return PH7_OK;` |
|        1 |  6897 |  |
|        - |  6898 | `/*` |
|        - |  6899 | ` * Extract a ph7_class from the given ph7_value.` |
|        - |  6900 | ` * The given value must be of type object [i.e: class instance] or` |
|        - |  6901 | ` * string which hold the class name.` |
|        - |  6902 | ` */` |
|       78 |  6903 | `static ph7_class * VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg)` |
|        2 |  6904 |  |
|       80 |  6905 | `	ph7_class *pClass = 0;` |
|       80 |  6906 | `	if( ph7_value_is_object(pArg) ){` |
|        - |  6907 | `		/* Class instance already loaded,no need to perform a lookup */` |
|       44 |  6908 | `		pClass = ((ph7_class_instance *)pArg->x.pOther)->pClass;` |
|       59 |  6909 | `	}else if( ph7_value_is_string(pArg) ){` |
|        - |  6910 | `		const char *zClass;` |
|        - |  6911 | `		int nLen;` |
|        - |  6912 | `		/* Extract class name */` |
|       35 |  6913 | `		zClass = ph7_value_to_string(pArg,&nLen);` |
|       35 |  6914 | `		if( nLen > 0 ){` |
|        - |  6915 | `			SyHashEntry *pEntry;` |
|        - |  6916 | `			/* Perform a lookup */` |
|       35 |  6917 | `			pEntry = SyHashGet(&pVm->hClass,(const void *)zClass,(sxu32)nLen);` |
|       35 |  6918 | `			if( pEntry ){` |
|        - |  6919 | `				/* Point to the desired class */` |
|       31 |  6920 | `				pClass = (ph7_class *)pEntry->pUserData;` |
|       15 |  6921 | `			}` |
|       17 |  6922 | `		}` |
|       17 |  6923 | `	}` |
|       80 |  6924 | `	return pClass;` |
|        2 |  6925 |  |
|        - |  6926 | `/*` |
|        - |  6927 | ` * bool property_exists(mixed $class,string $property)` |
|        - |  6928 | ` *   Checks if the object or class has a property.` |
|        - |  6929 | ` * Parameters` |
|        - |  6930 | ` *  class` |
|        - |  6931 | ` *   The class name or an object of the class to test for` |
|        - |  6932 | ` * property` |
|        - |  6933 | ` *  The name of the property` |
|        - |  6934 | ` * Return` |
|        - |  6935 | ` *   Returns TRUE if the property exists,FALSE otherwise.` |
|        - |  6936 | ` */` |
|       12 |  6937 | `static int vm_builtin_property_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6938 |  |
|       13 |  6939 | `	int res = 0; /* Assume attribute does not exists */` |
|       13 |  6940 | `	if( nArg > 1 ){` |
|        - |  6941 | `		ph7_class *pClass;` |
|       13 |  6942 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|       13 |  6943 | `		if( pClass ){` |
|        - |  6944 | `			const char *zName;` |
|        - |  6945 | `			int nLen;` |
|        - |  6946 | `			/* Extract attribute name */` |
|       13 |  6947 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|       13 |  6948 | `			if( nLen > 0 ){` |
|        - |  6949 | `				/* Perform the lookup in the attribute and method table */` |
|       12 |  6950 | `				if( SyHashGet(&pClass->hAttr,(const void *)zName,(sxu32)nLen) != 0` |
|        8 |  6951 | `					\|\| SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6952 | `						/* property exists,flag that */` |
|       11 |  6953 | `						res = 1;` |
|        5 |  6954 | `				}` |
|        6 |  6955 | `			}` |
|        6 |  6956 | `		}` |
|        6 |  6957 | `	}` |
|       13 |  6958 | `	ph7_result_bool(pCtx,res);` |
|       13 |  6959 | `	return PH7_OK;` |
|        1 |  6960 |  |
|        - |  6961 | `/*` |
|        - |  6962 | ` * bool method_exists(mixed $class,string $method)` |
|        - |  6963 | ` *   Checks if the given method is a class member.` |
|        - |  6964 | ` * Parameters` |
|        - |  6965 | ` *  class` |
|        - |  6966 | ` *   The class name or an object of the class to test for` |
|        - |  6967 | ` * property` |
|        - |  6968 | ` *  The name of the method` |
|        - |  6969 | ` * Return` |
|        - |  6970 | ` *   Returns TRUE if the method exists,FALSE otherwise.` |
|        - |  6971 | ` */` |
|        4 |  6972 | `static int vm_builtin_method_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6973 |  |
|        5 |  6974 | `	int res = 0; /* Assume method does not exists */` |
|        5 |  6975 | `	if( nArg > 1 ){` |
|        - |  6976 | `		ph7_class *pClass;` |
|        5 |  6977 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        5 |  6978 | `		if( pClass ){` |
|        - |  6979 | `			const char *zName;` |
|        - |  6980 | `			int nLen;` |
|        - |  6981 | `			/* Extract method name */` |
|        5 |  6982 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|        5 |  6983 | `			if( nLen > 0 ){` |
|        - |  6984 | `				/* Perform the lookup in the method table */` |
|        5 |  6985 | `				if( SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6986 | `					/* method exists,flag that */` |
|        3 |  6987 | `					res = 1;` |
|        1 |  6988 | `				}` |
|        2 |  6989 | `			}` |
|        2 |  6990 | `		}` |
|        2 |  6991 | `	}` |
|        5 |  6992 | `	ph7_result_bool(pCtx,res);` |
|        5 |  6993 | `	return PH7_OK;` |
|        1 |  6994 |  |
|        - |  6995 | `/*` |
|        - |  6996 | ` * bool class_exists(string $class_name [, bool $autoload = true ] )` |
|        - |  6997 | ` *   Checks if the class has been defined.` |
|        - |  6998 | ` * Parameters` |
|        - |  6999 | ` *  class_name` |
|        - |  7000 | ` *   The class name. The name is matched in a case-sensitive manner` |
|        - |  7001 | ` *   unlinke the standard PHP engine.` |
|        - |  7002 | ` *  autoload` |
|        - |  7003 | ` *   Whether or not to call __autoload by default.` |
|        - |  7004 | ` * Return` |
|        - |  7005 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|        - |  7006 | ` */` |
|       12 |  7007 | `static int vm_builtin_class_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7008 |  |
|       14 |  7009 | `	int res = 0; /* Assume class does not exists */` |
|       14 |  7010 | `	if( nArg > 0 ){` |
|        - |  7011 | `		const char *zName;` |
|        - |  7012 | `		int nLen;` |
|        - |  7013 | `		/* Extract given name */` |
|       14 |  7014 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7015 | `		/* Perform a hashlookup */` |
|       14 |  7016 | `		if( nLen > 0 && SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  7017 | `			/* class is available */` |
|       10 |  7018 | `			res = 1;` |
|        4 |  7019 | `		}` |
|        6 |  7020 | `	}` |
|       14 |  7021 | `	ph7_result_bool(pCtx,res);` |
|       14 |  7022 | `	return PH7_OK;` |
|        2 |  7023 |  |
|        - |  7024 | `/*` |
|        - |  7025 | ` * bool interface_exists(string $class_name [, bool $autoload = true ] )` |
|        - |  7026 | ` *   Checks if the interface has been defined.` |
|        - |  7027 | ` * Parameters` |
|        - |  7028 | ` *  class_name` |
|        - |  7029 | ` *   The class name. The name is matched in a case-sensitive manner` |
|        - |  7030 | ` *   unlinke the standard PHP engine.` |
|        - |  7031 | ` *  autoload` |
|        - |  7032 | ` *   Whether or not to call __autoload by default.` |
|        - |  7033 | ` * Return` |
|        - |  7034 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|        - |  7035 | ` */` |
|        6 |  7036 | `static int vm_builtin_interface_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7037 |  |
|        7 |  7038 | `	int res = 0; /* Assume class does not exists */` |
|        7 |  7039 | `	if( nArg > 0 ){` |
|        7 |  7040 | `		SyHashEntry *pEntry = 0;` |
|        - |  7041 | `		const char *zName;` |
|        - |  7042 | `		int nLen;` |
|        - |  7043 | `		/* Extract given name */` |
|        7 |  7044 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7045 | `		/* Perform a hashlookup */` |
|        7 |  7046 | `		if( nLen > 0 ){` |
|        7 |  7047 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|        3 |  7048 | `		}` |
|        7 |  7049 | `		if( pEntry ){` |
|        5 |  7050 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|        5 |  7051 | `			while( pClass ){` |
|        5 |  7052 | `				if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|        - |  7053 | `					/* interface is available */` |
|        5 |  7054 | `					res = 1;` |
|        5 |  7055 | `					break;` |
|        - |  7056 | `				}` |
|        - |  7057 | `				/* Next with the same name */` |
|      ! 0 |  7058 | `				pClass = pClass->pNextName;` |
|      ! 0 |  7059 | `			}` |
|        2 |  7060 | `		}` |
|        3 |  7061 | `	}` |
|        7 |  7062 | `	ph7_result_bool(pCtx,res);` |
|        7 |  7063 | `	return PH7_OK;` |
|        1 |  7064 |  |
|        - |  7065 | `/*` |
|        - |  7066 | ` * bool class_alias([string $original[,string $alias ]])` |
|        - |  7067 | ` *   Creates an alias for a class.` |
|        - |  7068 | ` * Parameters` |
|        - |  7069 | ` *  original` |
|        - |  7070 | ` *    The original class.` |
|        - |  7071 | ` *  alias` |
|        - |  7072 | ` *   The alias name for the class.` |
|        - |  7073 | ` * Return` |
|        - |  7074 | ` *   Returns TRUE on success or FALSE on failure.` |
|        - |  7075 | ` */` |
|        2 |  7076 | `static int vm_builtin_class_alias(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7077 |  |
|        - |  7078 | `	const char *zOld,*zNew;` |
|        - |  7079 | `	int nOldLen,nNewLen;` |
|        - |  7080 | `	SyHashEntry *pEntry;` |
|        - |  7081 | `	ph7_class *pClass;` |
|        - |  7082 | `	char *zDup;` |
|        - |  7083 | `	sxi32 rc;` |
|        3 |  7084 | `	if( nArg < 2 ){` |
|        - |  7085 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  7086 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7087 | `		return PH7_OK;` |
|        - |  7088 | `	}` |
|        - |  7089 | `	/* Extract old class name */` |
|        3 |  7090 | `	zOld = ph7_value_to_string(apArg[0],&nOldLen);` |
|        - |  7091 | `	/* Extract alias name */` |
|        3 |  7092 | `	zNew = ph7_value_to_string(apArg[1],&nNewLen);` |
|        3 |  7093 | `	if( nNewLen < 1 ){` |
|        - |  7094 | `		/* Invalid alias name,return FALSE */` |
|      ! 0 |  7095 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7096 | `		return PH7_OK;` |
|        - |  7097 | `	}` |
|        - |  7098 | `	/* Perform a hash lookup */` |
|        3 |  7099 | `	pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zOld,(sxu32)nOldLen);` |
|        3 |  7100 | `	if( pEntry ==  0 ){` |
|        - |  7101 | `		/* No such class,return FALSE */` |
|      ! 0 |  7102 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7103 | `		return PH7_OK;` |
|        - |  7104 | `	}` |
|        - |  7105 | `	/* Point to the class */` |
|        3 |  7106 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  7107 | `	/* Duplicate alias name */` |
|        3 |  7108 | `	zDup = SyMemBackendStrDup(&pCtx->pVm->sAllocator,zNew,(sxu32)nNewLen);` |
|        3 |  7109 | `	if( zDup == 0 ){` |
|        - |  7110 | `		/* Out of memory,return FALSE */` |
|      ! 0 |  7111 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7112 | `		return PH7_OK;` |
|        - |  7113 | `	}` |
|        - |  7114 | `	/* Create the alias */` |
|        3 |  7115 | `	rc = SyHashInsert(&pCtx->pVm->hClass,(const void *)zDup,(sxu32)nNewLen,pClass);` |
|        3 |  7116 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7117 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,zDup);` |
|      ! 0 |  7118 | `	}` |
|        3 |  7119 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|        3 |  7120 | `	return PH7_OK;` |
|        2 |  7121 |  |
|        - |  7122 | `/*` |
|        - |  7123 | ` * array get_declared_classes(void)` |
|        - |  7124 | ` *   Returns an array with the name of the defined classes` |
|        - |  7125 | ` * Parameters` |
|        - |  7126 | ` *  None` |
|        - |  7127 | ` * Return` |
|        - |  7128 | ` *   Returns an array of the names of the declared classes` |
|        - |  7129 | ` *   in the current script.` |
|        - |  7130 | ` * Note:` |
|        - |  7131 | ` *   NULL is returned on failure.` |
|        - |  7132 | ` */` |
|        2 |  7133 | `static int vm_builtin_get_declared_classes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7134 |  |
|        - |  7135 | `	ph7_value *pName,*pArray;` |
|        - |  7136 | `	SyHashEntry *pEntry;` |
|        - |  7137 | `	/* Create a new array first */` |
|        3 |  7138 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7139 | `	pName = ph7_context_new_scalar(pCtx);` |
|        3 |  7140 | `	if( pArray == 0 \|\| pName == 0){` |
|      ! 0 |  7141 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7142 | `		SXUNUSED(apArg);` |
|        - |  7143 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7144 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7145 | `		return PH7_OK;` |
|        - |  7146 | `	}` |
|        - |  7147 | `	/* Fill the array with the defined classes */` |
|        3 |  7148 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|       50 |  7149 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|       47 |  7150 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  7151 | `		/* Do not register classes defined as interfaces */` |
|       47 |  7152 | `		if( (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       41 |  7153 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|        - |  7154 | `			/* insert class name */` |
|       41 |  7155 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|        - |  7156 | `			/* Reset the cursor */` |
|       41 |  7157 | `			ph7_value_reset_string_cursor(pName);` |
|       20 |  7158 | `		}` |
|        1 |  7159 | `	}` |
|        - |  7160 | `	/* Return the created array */` |
|        3 |  7161 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7162 | `	return PH7_OK;` |
|        2 |  7163 |  |
|        - |  7164 | `/*` |
|        - |  7165 | ` * array get_declared_interfaces(void)` |
|        - |  7166 | ` *   Returns an array with the name of the defined interfaces` |
|        - |  7167 | ` * Parameters` |
|        - |  7168 | ` *  None` |
|        - |  7169 | ` * Return` |
|        - |  7170 | ` *   Returns an array of the names of the declared interfaces` |
|        - |  7171 | ` *   in the current script.` |
|        - |  7172 | ` * Note:` |
|        - |  7173 | ` *   NULL is returned on failure.` |
|        - |  7174 | ` */` |
|        2 |  7175 | `static int vm_builtin_get_declared_interfaces(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7176 |  |
|        - |  7177 | `	ph7_value *pName,*pArray;` |
|        - |  7178 | `	SyHashEntry *pEntry;` |
|        - |  7179 | `	/* Create a new array first */` |
|        3 |  7180 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7181 | `	pName = ph7_context_new_scalar(pCtx);` |
|        3 |  7182 | `	if( pArray == 0 \|\| pName == 0 ){` |
|      ! 0 |  7183 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7184 | `		SXUNUSED(apArg);` |
|        - |  7185 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7186 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7187 | `		return PH7_OK;` |
|        - |  7188 | `	}` |
|        - |  7189 | `	/* Fill the array with the defined classes */` |
|        3 |  7190 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|       52 |  7191 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|       49 |  7192 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  7193 | `		/* Register classes defined as interfaces only */` |
|       49 |  7194 | `		if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|        9 |  7195 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|        - |  7196 | `			/* insert interface name */` |
|        9 |  7197 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|        - |  7198 | `			/* Reset the cursor */` |
|        9 |  7199 | `			ph7_value_reset_string_cursor(pName);` |
|        4 |  7200 | `		}` |
|        1 |  7201 | `	}` |
|        - |  7202 | `	/* Return the created array */` |
|        3 |  7203 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7204 | `	return PH7_OK;` |
|        2 |  7205 |  |
|        - |  7206 | `/*` |
|        - |  7207 | ` * array get_class_methods(string/object $class_name)` |
|        - |  7208 | ` *   Returns an array with the name of the class methods` |
|        - |  7209 | ` * Parameters` |
|        - |  7210 | ` *  class_name` |
|        - |  7211 | ` *  The class name or class instance` |
|        - |  7212 | ` * Return` |
|        - |  7213 | ` *  Returns an array of method names defined for the class specified by class_name.` |
|        - |  7214 | ` *  In case of an error, it returns NULL.` |
|        - |  7215 | ` * Note:` |
|        - |  7216 | ` *   NULL is returned on failure.` |
|        - |  7217 | ` */` |
|        6 |  7218 | `static int vm_builtin_get_class_methods(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7219 |  |
|        - |  7220 | `	ph7_value *pName,*pArray;` |
|        - |  7221 | `	SyHashEntry *pEntry;` |
|        - |  7222 | `	ph7_class *pClass;` |
|        - |  7223 | `	/* Extract the target class first */` |
|        7 |  7224 | `	pClass = 0;` |
|        7 |  7225 | `	if( nArg > 0 ){` |
|        7 |  7226 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        3 |  7227 | `	}` |
|        7 |  7228 | `	if( pClass == 0 ){` |
|        - |  7229 | `		/* No such class,return NULL */` |
|        3 |  7230 | `		ph7_result_null(pCtx);` |
|        3 |  7231 | `		return PH7_OK;` |
|        - |  7232 | `	}` |
|        - |  7233 | `	/* Create a new array  */` |
|        5 |  7234 | `	pArray = ph7_context_new_array(pCtx);` |
|        5 |  7235 | `	pName = ph7_context_new_scalar(pCtx);` |
|        5 |  7236 | `	if( pArray == 0 \|\| pName == 0){` |
|        - |  7237 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7238 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7239 | `		return PH7_OK;` |
|        - |  7240 | `	}` |
|        - |  7241 | `	/* Fill the array with the defined methods */` |
|        5 |  7242 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|       17 |  7243 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|       13 |  7244 | `		ph7_class_method *pMethod = (ph7_class_method *)pEntry->pUserData;` |
|        - |  7245 | `		/* Insert method name */` |
|       13 |  7246 | `		ph7_value_string(pName,SyStringData(&pMethod->sFunc.sName),(int)SyStringLength(&pMethod->sFunc.sName));` |
|       13 |  7247 | `		ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|        - |  7248 | `		/* Reset the cursor */` |
|       13 |  7249 | `		ph7_value_reset_string_cursor(pName);` |
|        1 |  7250 | `	}` |
|        - |  7251 | `	/* Return the created array */` |
|        5 |  7252 | `	ph7_result_value(pCtx,pArray);` |
|        - |  7253 | `	/*` |
|        - |  7254 | `	 * Don't worry about freeing memory here,everything will be relased` |
|        - |  7255 | `	 * automatically as soon we return from this foreign function.` |
|        - |  7256 | `	 */` |
|        5 |  7257 | `	return PH7_OK;` |
|        4 |  7258 |  |
|        - |  7259 | `/*` |
|        - |  7260 | ` * This function return TRUE(1) if the given class attribute stored` |
|        - |  7261 | ` * in the pAttrName parameter is visible and thus can be extracted` |
|        - |  7262 | ` * from the current scope.Otherwise FALSE is returned.` |
|        - |  7263 | ` */` |
|      776 |  7264 | `static int VmClassMemberAccess(` |
|        - |  7265 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  7266 | `	ph7_class *pClass,         /* Target Class */` |
|        - |  7267 | `	const SyString *pAttrName, /* Attribute name */` |
|        - |  7268 | `	sxi32 iProtection,         /* Attribute protection level [i.e: public,protected or private] */` |
|        - |  7269 | `	int bLog                   /* TRUE to log forbidden access. */` |
|        - |  7270 | `	)` |
|        2 |  7271 |  |
|      778 |  7272 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|      214 |  7273 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  7274 | `		ph7_vm_func *pVmFunc;` |
|      218 |  7275 | `		while( pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH) ) ){` |
|        - |  7276 | `			/* Safely ignore the exception frame */` |
|        5 |  7277 | `			pFrame = pFrame->pParent;` |
|        1 |  7278 | `		}` |
|      214 |  7279 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      214 |  7280 | `		if( pVmFunc == 0 \|\| (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|        9 |  7281 | `			goto dis; /* Access is forbidden */` |
|        - |  7282 | `		}` |
|      206 |  7283 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|        - |  7284 | `			/* Must be the same instance */` |
|        7 |  7285 | `			if( (ph7_class *)pVmFunc->pUserData != pClass ){` |
|      ! 0 |  7286 | `				goto dis; /* Access is forbidden */` |
|        - |  7287 | `			}` |
|        4 |  7288 | `		}else{` |
|        - |  7289 | `			/* Protected */` |
|      200 |  7290 | `			ph7_class *pBase = (ph7_class *)pVmFunc->pUserData;` |
|        - |  7291 | `			/* Must be a derived class */` |
|      200 |  7292 | `			if( !VmInstanceOf(pClass,pBase) ){` |
|      ! 0 |  7293 | `				goto dis; /* Access is forbidden */` |
|        - |  7294 | `			}` |
|        - |  7295 | `		}` |
|      102 |  7296 | `	}` |
|      770 |  7297 | `	return 1; /* Access is granted */` |
|        4 |  7298 | `dis:` |
|        9 |  7299 | `	if( bLog ){` |
|      ! 0 |  7300 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7301 | `			"Access to the class attribute '%z->%z' is forbidden",` |
|      ! 0 |  7302 | `			&pClass->sName,pAttrName);` |
|      ! 0 |  7303 | `	}` |
|        9 |  7304 | `	return 0; /* Access is forbidden */` |
|      390 |  7305 |  |
|        - |  7306 | `/*` |
|        - |  7307 | ` * array get_class_vars(string/object $class_name)` |
|        - |  7308 | ` *   Get the default properties of the class` |
|        - |  7309 | ` * Parameters` |
|        - |  7310 | ` *  class_name` |
|        - |  7311 | ` *   The class name or class instance` |
|        - |  7312 | ` * Return` |
|        - |  7313 | ` *  Returns an associative array of declared properties visible from the current scope` |
|        - |  7314 | ` *  with their default value. The resulting array elements are in the form` |
|        - |  7315 | ` *  of varname => value.` |
|        - |  7316 | ` * Note:` |
|        - |  7317 | ` *   NULL is returned on failure.` |
|        - |  7318 | ` */` |
|        2 |  7319 | `static int vm_builtin_get_class_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7320 |  |
|        - |  7321 | `	ph7_value *pName,*pArray,sValue;` |
|        - |  7322 | `	SyHashEntry *pEntry;` |
|        - |  7323 | `	ph7_class *pClass;` |
|        - |  7324 | `	/* Extract the target class first */` |
|        3 |  7325 | `	pClass = 0;` |
|        3 |  7326 | `	if( nArg > 0 ){` |
|        3 |  7327 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        1 |  7328 | `	}` |
|        3 |  7329 | `	if( pClass == 0 ){` |
|        - |  7330 | `		/* No such class,return NULL */` |
|      ! 0 |  7331 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7332 | `		return PH7_OK;` |
|        - |  7333 | `	}` |
|        - |  7334 | `	/* Create a new array  */` |
|        3 |  7335 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7336 | `	pName = ph7_context_new_scalar(pCtx);` |
|        3 |  7337 | `	PH7_MemObjInit(pCtx->pVm,&sValue);` |
|        3 |  7338 | `	if( pArray == 0 \|\| pName == 0){` |
|        - |  7339 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7340 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7341 | `		return PH7_OK;` |
|        - |  7342 | `	}` |
|        - |  7343 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|        3 |  7344 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        8 |  7345 | `	while((pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        5 |  7346 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|        - |  7347 | `		/* Check if the access is allowed */` |
|        5 |  7348 | `		if( VmClassMemberAccess(pCtx->pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        5 |  7349 | `			SyString *pAttrName = &pAttr->sName;` |
|        5 |  7350 | `			ph7_value *pValue = 0;` |
|        5 |  7351 | `			if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |  7352 | `				/* Extract static attribute value which is always computed */` |
|        5 |  7353 | `				pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pAttr->nIdx);` |
|        3 |  7354 | `			}else{` |
|      ! 0 |  7355 | `				if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|      ! 0 |  7356 | `					PH7_MemObjRelease(&sValue);` |
|        - |  7357 | `					/* Compute default value (any complex expression) associated with this attribute */` |
|      ! 0 |  7358 | `					VmLocalExec(pCtx->pVm,&pAttr->aByteCode,&sValue);` |
|      ! 0 |  7359 | `					pValue = &sValue;` |
|      ! 0 |  7360 | `				}` |
|        - |  7361 | `			}` |
|        - |  7362 | `			/* Fill in the array */` |
|        5 |  7363 | `			ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|        5 |  7364 | `			ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|        - |  7365 | `			/* Reset the cursor */` |
|        5 |  7366 | `			ph7_value_reset_string_cursor(pName);` |
|        2 |  7367 | `		}` |
|        1 |  7368 | `	}` |
|        3 |  7369 | `	PH7_MemObjRelease(&sValue);` |
|        - |  7370 | `	/* Return the created array */` |
|        3 |  7371 | `	ph7_result_value(pCtx,pArray);` |
|        - |  7372 | `	/*` |
|        - |  7373 | `	 * Don't worry about freeing memory here,everything will be relased` |
|        - |  7374 | `	 * automatically as soon we return from this foreign function.` |
|        - |  7375 | `	 */` |
|        3 |  7376 | `	return PH7_OK;` |
|        2 |  7377 |  |
|        - |  7378 | `/*` |
|        - |  7379 | ` * array get_object_vars(object $this)` |
|        - |  7380 | ` *   Gets the properties of the given object` |
|        - |  7381 | ` * Parameters` |
|        - |  7382 | ` *  this` |
|        - |  7383 | ` *   A class instance` |
|        - |  7384 | ` * Return` |
|        - |  7385 | ` *  Returns an associative array of defined object accessible non-static properties` |
|        - |  7386 | ` *  for the specified object in scope. If a property have not been assigned a value` |
|        - |  7387 | ` *  it will be returned with a NULL value.` |
|        - |  7388 | ` * Note:` |
|        - |  7389 | ` *   NULL is returned on failure.` |
|        - |  7390 | ` */` |
|        2 |  7391 | `static int vm_builtin_get_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7392 |  |
|        3 |  7393 | `	ph7_class_instance *pThis = 0;` |
|        - |  7394 | `	ph7_value *pName,*pArray;` |
|        - |  7395 | `	SyHashEntry *pEntry;` |
|        3 |  7396 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|        - |  7397 | `		/* Extract the target instance */` |
|        3 |  7398 | `		pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        1 |  7399 | `	}` |
|        3 |  7400 | `	if( pThis == 0 ){` |
|        - |  7401 | `		/* No such instance,return NULL */` |
|      ! 0 |  7402 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7403 | `		return PH7_OK;` |
|        - |  7404 | `	}` |
|        - |  7405 | `	/* Create a new array  */` |
|        3 |  7406 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7407 | `	pName = ph7_context_new_scalar(pCtx);` |
|        3 |  7408 | `	if( pArray == 0 \|\| pName == 0){` |
|        - |  7409 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7410 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7411 | `		return PH7_OK;` |
|        - |  7412 | `	}` |
|        - |  7413 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|        3 |  7414 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  7415 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|        7 |  7416 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  7417 | `		SyString *pAttrName;` |
|        7 |  7418 | `		if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|        - |  7419 | `			/* Only non-static/constant attributes are extracted */` |
|      ! 0 |  7420 | `			continue;` |
|        - |  7421 | `		}` |
|        7 |  7422 | `		pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  7423 | `		/* Check if the access is allowed */` |
|        7 |  7424 | `		if( VmClassMemberAccess(pCtx->pVm,pThis->pClass,pAttrName,pVmAttr->pAttr->iProtection,FALSE) ){` |
|        3 |  7425 | `			ph7_value *pValue = 0;` |
|        - |  7426 | `			/* Extract attribute */` |
|        3 |  7427 | `			pValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|        3 |  7428 | `			if( pValue ){` |
|        - |  7429 | `				/* Insert attribute name in the array */` |
|        3 |  7430 | `				ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|        3 |  7431 | `				ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|        1 |  7432 | `			}` |
|        - |  7433 | `			/* Reset the cursor */` |
|        3 |  7434 | `			ph7_value_reset_string_cursor(pName);` |
|        1 |  7435 | `		}` |
|        1 |  7436 | `	}` |
|        - |  7437 | `	/* Return the created array */` |
|        3 |  7438 | `	ph7_result_value(pCtx,pArray);` |
|        - |  7439 | `	/*` |
|        - |  7440 | `	 * Don't worry about freeing memory here,everything will be relased` |
|        - |  7441 | `	 * automatically as soon we return from this foreign function.` |
|        - |  7442 | `	 */` |
|        3 |  7443 | `	return PH7_OK;` |
|        2 |  7444 |  |
|        - |  7445 | `/*` |
|        - |  7446 | ` * This function returns TRUE if the given class is an implemented` |
|        - |  7447 | ` * interface.Otherwise FALSE is returned.` |
|        - |  7448 | ` */` |
|      242 |  7449 | `static int VmQueryInterfaceSet(ph7_class *pClass,SySet *pSet)` |
|        2 |  7450 |  |
|        - |  7451 | `	ph7_class **apInterface;` |
|        - |  7452 | `	sxu32 n;` |
|      244 |  7453 | `	if( SySetUsed(pSet) < 1 ){` |
|        - |  7454 | `		/* Empty interface container */` |
|      242 |  7455 | `		return FALSE;` |
|        - |  7456 | `	}` |
|        - |  7457 | `	/* Point to the set of implemented interfaces */` |
|        3 |  7458 | `	apInterface = (ph7_class **)SySetBasePtr(pSet);` |
|        - |  7459 | `	/* Perform the lookup */` |
|        3 |  7460 | `	for( n = 0 ; n < SySetUsed(pSet) ; n++ ){` |
|        3 |  7461 | `		if( apInterface[n] == pClass ){` |
|        3 |  7462 | `			return TRUE;` |
|        - |  7463 | `		}` |
|      ! 0 |  7464 | `	}` |
|      ! 0 |  7465 | `	return FALSE;` |
|      123 |  7466 |  |
|        - |  7467 | `/*` |
|        - |  7468 | ` * This function returns TRUE if the given class (first argument)` |
|        - |  7469 | ` * is an instance of the main class (second argument).` |
|        - |  7470 | ` * Otherwise FALSE is returned.` |
|        - |  7471 | ` */` |
|      250 |  7472 | `static int VmInstanceOf(ph7_class *pThis,ph7_class *pClass)` |
|        2 |  7473 |  |
|        - |  7474 | `	ph7_class *pParent;` |
|        - |  7475 | `	sxi32 rc;` |
|      252 |  7476 | `	if( pThis == pClass ){` |
|        - |  7477 | `		/* Instance of the same class */` |
|      140 |  7478 | `		return TRUE;` |
|        - |  7479 | `	}` |
|        - |  7480 | `	/* Check implemented interfaces */` |
|      114 |  7481 | `	rc = VmQueryInterfaceSet(pClass,&pThis->aInterface);` |
|      114 |  7482 | `	if( rc ){` |
|        3 |  7483 | `		return TRUE;` |
|        - |  7484 | `	}` |
|        - |  7485 | `	/* Check parent classes */` |
|      112 |  7486 | `	pParent = pThis->pBase;` |
|      242 |  7487 | `	while( pParent ){` |
|      240 |  7488 | `		if( pParent == pClass ){` |
|        - |  7489 | `			/* Same instance */` |
|      110 |  7490 | `			return TRUE;` |
|        - |  7491 | `		}` |
|        - |  7492 | `		/* Check the implemented interfaces */` |
|      132 |  7493 | `		rc = VmQueryInterfaceSet(pClass,&pParent->aInterface);` |
|      132 |  7494 | `		if( rc ){` |
|      ! 0 |  7495 | `			return TRUE;` |
|        - |  7496 | `		}` |
|        - |  7497 | `		/* Point to the parent class */` |
|      132 |  7498 | `		pParent = pParent->pBase;` |
|        2 |  7499 | `	}` |
|        - |  7500 | `	/* Not an instance of the the given class */` |
|        3 |  7501 | `	return FALSE;` |
|      127 |  7502 |  |
|        - |  7503 | `/*` |
|        - |  7504 | ` * This function returns TRUE if the given class (first argument)` |
|        - |  7505 | ` * is a subclass of the main class (second argument).` |
|        - |  7506 | ` * Otherwise FALSE is returned.` |
|        - |  7507 | ` */` |
|        4 |  7508 | `static int VmSubclassOf(ph7_class *pClass,ph7_class *pBase)` |
|        1 |  7509 |  |
|        5 |  7510 | `	SySet *pInterface = &pClass->aInterface;` |
|        - |  7511 | `	SyHashEntry *pEntry;` |
|        - |  7512 | `	SyString *pName;` |
|        - |  7513 | `	sxi32 rc;` |
|        5 |  7514 | `	while( pClass ){` |
|        5 |  7515 | `		pName = &pClass->sName;` |
|        - |  7516 | `		/* Query the derived hashtable */` |
|        5 |  7517 | `		pEntry = SyHashGet(&pBase->hDerived,(const void *)pName->zString,pName->nByte);` |
|        5 |  7518 | `		if( pEntry ){` |
|        5 |  7519 | `			return TRUE;` |
|        - |  7520 | `		}` |
|      ! 0 |  7521 | `		pClass = pClass->pBase;` |
|      ! 0 |  7522 | `	}` |
|      ! 0 |  7523 | `	rc = VmQueryInterfaceSet(pBase,pInterface);` |
|      ! 0 |  7524 | `	if( rc ){` |
|      ! 0 |  7525 | `		return TRUE;` |
|        - |  7526 | `	}` |
|        - |  7527 | `	/* Not a subclass */` |
|      ! 0 |  7528 | `	return FALSE;` |
|        3 |  7529 |  |
|        - |  7530 | `/*` |
|        - |  7531 | ` * bool is_a(object $object,string $class_name)` |
|        - |  7532 | ` *   Checks if the object is of this class or has this class as one of its parents.` |
|        - |  7533 | ` * Parameters` |
|        - |  7534 | ` *  object` |
|        - |  7535 | ` *   The tested object` |
|        - |  7536 | ` * class_name` |
|        - |  7537 | ` *  The class name` |
|        - |  7538 | ` * Return` |
|        - |  7539 | ` *   Returns TRUE if the object is of this class or has this class as one of its` |
|        - |  7540 | ` *   parents, FALSE otherwise.` |
|        - |  7541 | ` */` |
|        2 |  7542 | `static int vm_builtin_is_a(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7543 |  |
|        3 |  7544 | `	int res = 0; /* Assume FALSE by default */` |
|        3 |  7545 | `	if( nArg > 1 && ph7_value_is_object(apArg[0])  ){` |
|        3 |  7546 | `		ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7547 | `		ph7_class *pClass;` |
|        - |  7548 | `		/* Extract the given class */` |
|        3 |  7549 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|        3 |  7550 | `		if( pClass ){` |
|        - |  7551 | `			/* Perform the query */` |
|        3 |  7552 | `			res = VmInstanceOf(pThis->pClass,pClass);` |
|        1 |  7553 | `		}` |
|        1 |  7554 | `	}` |
|        - |  7555 | `	/* Query result */` |
|        3 |  7556 | `	ph7_result_bool(pCtx,res);` |
|        3 |  7557 | `	return PH7_OK;` |
|        1 |  7558 |  |
|        - |  7559 | `/*` |
|        - |  7560 | ` * bool is_subclass_of(object/string $object,object/string $class_name)` |
|        - |  7561 | ` *   Checks if the object has this class as one of its parents.` |
|        - |  7562 | ` * Parameters` |
|        - |  7563 | ` *  object` |
|        - |  7564 | ` *   The tested object` |
|        - |  7565 | ` * class_name` |
|        - |  7566 | ` *  The class name` |
|        - |  7567 | ` * Return` |
|        - |  7568 | ` *  This function returns TRUE if the object , belongs to a class` |
|        - |  7569 | ` *  which is a subclass of class_name, FALSE otherwise.` |
|        - |  7570 | ` */` |
|        6 |  7571 | `static int vm_builtin_is_subclass_of(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7572 |  |
|        7 |  7573 | `	int res = 0; /* Assume FALSE by default */` |
|        7 |  7574 | `	if( nArg > 1 ){` |
|        - |  7575 | `		ph7_class *pClass,*pMain;` |
|        - |  7576 | `		/* Extract the given classes */` |
|        7 |  7577 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        7 |  7578 | `		pMain = VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|        7 |  7579 | `		if( pClass && pMain ){` |
|        - |  7580 | `			/* Perform the query */` |
|        5 |  7581 | `			res = VmSubclassOf(pClass,pMain);` |
|        2 |  7582 | `		}` |
|        3 |  7583 | `	}` |
|        - |  7584 | `	/* Query result */` |
|        7 |  7585 | `	ph7_result_bool(pCtx,res);` |
|        7 |  7586 | `	return PH7_OK;` |
|        1 |  7587 |  |
|        - |  7588 | `/*` |
|        - |  7589 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  7590 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  7591 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  7592 | ` * return value indicates failure.` |
|        - |  7593 | ` */` |
|      230 |  7594 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  7595 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  7596 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  7597 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  7598 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  7599 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  7600 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  7601 | `	)` |
|        2 |  7602 |  |
|        - |  7603 | `	ph7_value *aStack;` |
|        - |  7604 | `	VmInstr aInstr[2];` |
|        - |  7605 | `	int iCursor;` |
|        - |  7606 | `	int i;` |
|        - |  7607 | `	/* Create a new operand stack */` |
|      232 |  7608 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|      232 |  7609 | `	if( aStack == 0 ){` |
|      ! 0 |  7610 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7611 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  7612 | `		return SXERR_MEM;` |
|        - |  7613 | `	}` |
|        - |  7614 | `	/* Fill the operand stack with the given arguments */` |
|      318 |  7615 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       88 |  7616 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  7617 | `		/*` |
|        - |  7618 | `		 * Symisc eXtension:` |
|        - |  7619 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  7620 | `		 */` |
|       88 |  7621 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|       45 |  7622 | `	}` |
|      232 |  7623 | `	iCursor = nArg + 1;` |
|      232 |  7624 | `	if( pThis ){` |
|        - |  7625 | `		/*` |
|        - |  7626 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  7627 | `		 */` |
|      226 |  7628 | `		pThis->iRef++; /* Increment reference count */` |
|      226 |  7629 | `		aStack[i].x.pOther = pThis;` |
|      226 |  7630 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      112 |  7631 | `	}` |
|      232 |  7632 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|      232 |  7633 | `	i++;` |
|        - |  7634 | `	/* Push method name */` |
|      232 |  7635 | `	SyBlobReset(&aStack[i].sBlob);` |
|      232 |  7636 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|      232 |  7637 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|      232 |  7638 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  7639 | `	/* Emit the CALL istruction */` |
|      232 |  7640 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      232 |  7641 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      232 |  7642 | `	aInstr[0].iP2 = 0;` |
|      232 |  7643 | `	aInstr[0].p3  = 0;` |
|        - |  7644 | `	/* Emit the DONE instruction */` |
|      232 |  7645 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      232 |  7646 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|      232 |  7647 | `	aInstr[1].iP2 = 0;` |
|      232 |  7648 | `	aInstr[1].p3  = 0;` |
|        - |  7649 | `	/* Execute the method body (if available) */` |
|      232 |  7650 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE);` |
|        - |  7651 | `	/* Clean up the mess left behind */` |
|      232 |  7652 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      232 |  7653 | `	return PH7_OK;` |
|      117 |  7654 |  |
|        - |  7655 | `/*` |
|        - |  7656 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  7657 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  7658 | ` * in the apArg[] array.` |
|        - |  7659 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  7660 | ` * return value indicates failure.` |
|        - |  7661 | ` */` |
|      322 |  7662 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  7663 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  7664 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  7665 | `	int nArg,          /* Total number of given arguments */` |
|        - |  7666 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  7667 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  7668 | `	)` |
|        2 |  7669 |  |
|        - |  7670 | `	ph7_value *aStack;` |
|        - |  7671 | `	VmInstr aInstr[2];` |
|        - |  7672 | `	int i;` |
|      324 |  7673 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  7674 | `		/* Don't bother processing,it's invalid anyway */` |
|       15 |  7675 | `		if( pResult ){` |
|        - |  7676 | `			/* Assume a null return value */` |
|      ! 0 |  7677 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  7678 | `		}` |
|       15 |  7679 | `		return SXERR_INVALID;` |
|        - |  7680 | `	}` |
|      310 |  7681 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  7682 | `		/* Class method */` |
|       11 |  7683 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  7684 | `		ph7_class_method *pMethod = 0;` |
|       11 |  7685 | `		ph7_class_instance *pThis = 0;` |
|       11 |  7686 | `		ph7_class *pClass = 0;` |
|        - |  7687 | `		ph7_value *pValue;` |
|        - |  7688 | `		sxi32 rc;` |
|       11 |  7689 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  7690 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  7691 | `			if( pResult ){` |
|        - |  7692 | `				/* Assume a null return value */` |
|      ! 0 |  7693 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7694 | `			}` |
|      ! 0 |  7695 | `			return SXRET_OK;` |
|        - |  7696 | `		}` |
|        - |  7697 | `		/* Extract the class name or an instance of it */` |
|       11 |  7698 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  7699 | `		if( pValue ){` |
|       11 |  7700 | `			pClass = VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  7701 | `		}` |
|       11 |  7702 | `		if( pClass == 0 ){` |
|        - |  7703 | `			/* No such class,return NULL */` |
|      ! 0 |  7704 | `			if( pResult ){` |
|      ! 0 |  7705 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7706 | `			}` |
|      ! 0 |  7707 | `			return SXRET_OK;` |
|        - |  7708 | `		}` |
|       11 |  7709 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  7710 | `			/* Point to the class instance */` |
|        5 |  7711 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  7712 | `		}` |
|        - |  7713 | `		/* Try to extract the method */` |
|       11 |  7714 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  7715 | `		if( pValue ){` |
|       11 |  7716 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  7717 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  7718 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  7719 | `			}` |
|        5 |  7720 | `		}` |
|       11 |  7721 | `		if( pMethod == 0 ){` |
|        - |  7722 | `			/* No such method,return NULL */` |
|      ! 0 |  7723 | `			if( pResult ){` |
|      ! 0 |  7724 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7725 | `			}` |
|      ! 0 |  7726 | `			return SXRET_OK;` |
|        - |  7727 | `		}` |
|        - |  7728 | `		/* Call the class method */` |
|       11 |  7729 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  7730 | `		return rc;` |
|        - |  7731 | `	}` |
|        - |  7732 | `	/* Create a new operand stack */` |
|      300 |  7733 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      300 |  7734 | `	if( aStack == 0 ){` |
|      ! 0 |  7735 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7736 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  7737 | `		if( pResult ){` |
|        - |  7738 | `			/* Assume a null return value */` |
|      ! 0 |  7739 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  7740 | `		}` |
|      ! 0 |  7741 | `		return SXERR_MEM;` |
|        - |  7742 | `	}` |
|        - |  7743 | `	/* Fill the operand stack with the given arguments */` |
|      928 |  7744 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      629 |  7745 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  7746 | `		/*` |
|        - |  7747 | `		 * Symisc eXtension:` |
|        - |  7748 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  7749 | `		 */` |
|      629 |  7750 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      315 |  7751 | `	}` |
|        - |  7752 | `	/* Push the function name */` |
|      300 |  7753 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      300 |  7754 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  7755 | `	/* Emit the CALL istruction */` |
|      300 |  7756 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      300 |  7757 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      300 |  7758 | `	aInstr[0].iP2 = 0;` |
|      300 |  7759 | `	aInstr[0].p3  = 0;` |
|        - |  7760 | `	/* Emit the DONE instruction */` |
|      300 |  7761 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      300 |  7762 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      300 |  7763 | `	aInstr[1].iP2 = 0;` |
|      300 |  7764 | `	aInstr[1].p3  = 0;` |
|        - |  7765 | `	/* Execute the function body (if available) */` |
|      300 |  7766 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE);` |
|        - |  7767 | `	/* Clean up the mess left behind */` |
|      300 |  7768 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      300 |  7769 | `	return PH7_OK;` |
|      163 |  7770 |  |
|        - |  7771 | `/*` |
|        - |  7772 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  7773 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  7774 | ` * parameter.` |
|        - |  7775 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  7776 | ` * return value indicates failure.` |
|        - |  7777 | ` */` |
|      190 |  7778 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  7779 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  7780 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  7781 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  7782 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  7783 | `	)` |
|        1 |  7784 |  |
|        - |  7785 | `	ph7_value *pArg;` |
|        - |  7786 | `	SySet aArg;` |
|        - |  7787 | `	va_list ap;` |
|        - |  7788 | `	sxi32 rc;` |
|      191 |  7789 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  7790 | `	/* Copy arguments one after one */` |
|      191 |  7791 | `	va_start(ap,pResult);` |
|      319 |  7792 | `	for(;;){` |
|      639 |  7793 | `		pArg = va_arg(ap,ph7_value *);` |
|      639 |  7794 | `		if( pArg == 0 ){` |
|      191 |  7795 | `			break;` |
|        - |  7796 | `		}` |
|      449 |  7797 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  7798 | `	}` |
|        - |  7799 | `	/* Call the core routine */` |
|      191 |  7800 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  7801 | `	/* Cleanup */` |
|      191 |  7802 | `	SySetRelease(&aArg);` |
|      191 |  7803 | `	return rc;` |
|        1 |  7804 |  |
|        - |  7805 | `/*` |
|        - |  7806 | ` * value call_user_func(callable $callback[,value $parameter[, value $... ]])` |
|        - |  7807 | ` *  Call the callback given by the first parameter.` |
|        - |  7808 | ` * Parameter` |
|        - |  7809 | ` *  $callback` |
|        - |  7810 | ` *   The callable to be called.` |
|        - |  7811 | ` *  ...` |
|        - |  7812 | ` *    Zero or more parameters to be passed to the callback.` |
|        - |  7813 | ` * Return` |
|        - |  7814 | ` *  Th return value of the callback, or FALSE on error.` |
|        - |  7815 | ` */` |
|       14 |  7816 | `static int vm_builtin_call_user_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7817 |  |
|        - |  7818 | `	ph7_value sResult; /* Store callback return value here */` |
|        - |  7819 | `	sxi32 rc;` |
|       15 |  7820 | `	if( nArg < 1 ){` |
|        - |  7821 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  7822 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7823 | `		return PH7_OK;` |
|        - |  7824 | `	}` |
|       15 |  7825 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|       15 |  7826 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  7827 | `	/* Try to invoke the callback */` |
|       15 |  7828 | `	rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult);` |
|       15 |  7829 | `	if( rc != SXRET_OK ){` |
|        - |  7830 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|      ! 0 |  7831 | `		ph7_result_bool(pCtx,0); /* return false */` |
|      ! 0 |  7832 | `	}else{` |
|        - |  7833 | `		/* Callback result */` |
|       15 |  7834 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|        - |  7835 | `	}` |
|       15 |  7836 | `	PH7_MemObjRelease(&sResult);` |
|       15 |  7837 | `	return PH7_OK;` |
|        8 |  7838 |  |
|        - |  7839 | `/*` |
|        - |  7840 | ` * value call_user_func_array(callable $callback,array $param_arr)` |
|        - |  7841 | ` *  Call a callback with an array of parameters.` |
|        - |  7842 | ` * Parameter` |
|        - |  7843 | ` *  $callback` |
|        - |  7844 | ` *   The callable to be called.` |
|        - |  7845 | ` * $param_arr` |
|        - |  7846 | ` *  The parameters to be passed to the callback, as an indexed array.` |
|        - |  7847 | ` * Return` |
|        - |  7848 | ` *  Returns the return value of the callback, or FALSE on error.` |
|        - |  7849 | ` */` |
|       10 |  7850 | `static int vm_builtin_call_user_func_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7851 |  |
|        - |  7852 | `	ph7_hashmap_node *pEntry; /* Current hashmap entry */` |
|        - |  7853 | `	ph7_value *pValue,sResult;/* Store callback return value here */` |
|        - |  7854 | `	ph7_hashmap *pMap;        /* Target hashmap */` |
|        - |  7855 | `	SySet aArg;               /* Arguments containers */` |
|        - |  7856 | `	sxi32 rc;` |
|        - |  7857 | `	sxu32 n;` |
|       11 |  7858 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|        - |  7859 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 |  7860 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7861 | `		return PH7_OK;` |
|        - |  7862 | `	}` |
|       11 |  7863 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|       11 |  7864 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  7865 | `	/* Initialize the arguments container */` |
|       11 |  7866 | `	SySetInit(&aArg,&pCtx->pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  7867 | `	/* Turn hashmap entries into callback arguments */` |
|       11 |  7868 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       11 |  7869 | `	pEntry = pMap->pFirst; /* First inserted entry */` |
|       23 |  7870 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|        - |  7871 | `		/* Extract node value */` |
|       13 |  7872 | `		if( (pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx)) != 0 ){` |
|       13 |  7873 | `			SySetPut(&aArg,(const void *)&pValue);` |
|        6 |  7874 | `		}` |
|        - |  7875 | `		/* Point to the next entry */` |
|       13 |  7876 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        7 |  7877 | `	}` |
|        - |  7878 | `	/* Try to invoke the callback */` |
|       11 |  7879 | `	rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|       11 |  7880 | `	if( rc != SXRET_OK ){` |
|        - |  7881 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|      ! 0 |  7882 | `		ph7_result_bool(pCtx,0); /* return false */` |
|      ! 0 |  7883 | `	}else{` |
|        - |  7884 | `		/* Callback result */` |
|       11 |  7885 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|        - |  7886 | `	}` |
|        - |  7887 | `	/* Cleanup the mess left behind */` |
|       11 |  7888 | `	PH7_MemObjRelease(&sResult);` |
|       11 |  7889 | `	SySetRelease(&aArg);` |
|       11 |  7890 | `	return PH7_OK;` |
|        6 |  7891 |  |
|        - |  7892 | `/*` |
|        - |  7893 | ` * bool defined(string $name)` |
|        - |  7894 | ` *  Checks whether a given named constant exists.` |
|        - |  7895 | ` * Parameter:` |
|        - |  7896 | ` *  Name of the desired constant.` |
|        - |  7897 | ` * Return` |
|        - |  7898 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  7899 | ` */` |
|       12 |  7900 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7901 |  |
|        - |  7902 | `	const char *zName;` |
|       13 |  7903 | `	int nLen = 0;` |
|       13 |  7904 | `	int res = 0;` |
|       13 |  7905 | `	if( nArg < 1 ){` |
|        - |  7906 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  7907 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  7908 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7909 | `		return SXRET_OK;` |
|        - |  7910 | `	}` |
|        - |  7911 | `	/* Extract constant name */` |
|       13 |  7912 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7913 | `	/* Perform the lookup */` |
|       13 |  7914 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  7915 | `		/* Already defined */` |
|        7 |  7916 | `		res = 1;` |
|        3 |  7917 | `	}` |
|       13 |  7918 | `	ph7_result_bool(pCtx,res);` |
|       13 |  7919 | `	return SXRET_OK;` |
|        7 |  7920 |  |
|        - |  7921 | `/*` |
|        - |  7922 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  7923 | ` * below.` |
|        - |  7924 | ` */` |
|        8 |  7925 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  7926 |  |
|       10 |  7927 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  7928 | `	/* Expand constant value */` |
|       10 |  7929 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       10 |  7930 |  |
|        - |  7931 | `/*` |
|        - |  7932 | ` * bool define(string $constant_name,expression value)` |
|        - |  7933 | ` *  Defines a named constant at runtime.` |
|        - |  7934 | ` * Parameter:` |
|        - |  7935 | ` *  $constant_name` |
|        - |  7936 | ` *   The name of the constant` |
|        - |  7937 | ` *  $value` |
|        - |  7938 | ` *   Constant value` |
|        - |  7939 | ` * Return:` |
|        - |  7940 | ` *   TRUE on success,FALSE on failure.` |
|        - |  7941 | ` */` |
|       10 |  7942 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7943 |  |
|        - |  7944 | `	const char *zName;  /* Constant name */` |
|        - |  7945 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       12 |  7946 | `	int nLen = 0;       /* Name length */` |
|        - |  7947 | `	sxi32 rc;` |
|       12 |  7948 | `	if( nArg < 2 ){` |
|        - |  7949 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  7950 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  7951 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7952 | `		return SXRET_OK;` |
|        - |  7953 | `	}` |
|       12 |  7954 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  7955 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  7956 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7957 | `		return SXRET_OK;` |
|        - |  7958 | `	}` |
|        - |  7959 | `	/* Extract constant name */` |
|       12 |  7960 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  7961 | `	if( nLen < 1 ){` |
|      ! 0 |  7962 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  7963 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7964 | `		return SXRET_OK;` |
|        - |  7965 | `	}` |
|        - |  7966 | `	/* Duplicate constant value */` |
|       12 |  7967 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       12 |  7968 | `	if( pValue == 0 ){` |
|      ! 0 |  7969 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7970 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7971 | `		return SXRET_OK;` |
|        - |  7972 | `	}` |
|        - |  7973 | `	/* Initialize the memory object */` |
|       12 |  7974 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  7975 | `	/* Register the constant */` |
|       12 |  7976 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       12 |  7977 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7978 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  7979 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7980 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7981 | `		return SXRET_OK;` |
|        - |  7982 | `	}` |
|        - |  7983 | `	/* Duplicate constant value */` |
|       12 |  7984 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       12 |  7985 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  7986 | `		/* Lower case the constant name */` |
|      ! 0 |  7987 | `		char *zCur = (char *)zName;` |
|      ! 0 |  7988 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  7989 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  7990 | `				/* UTF-8 stream */` |
|      ! 0 |  7991 | `				zCur++;` |
|      ! 0 |  7992 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  7993 | `					zCur++;` |
|      ! 0 |  7994 | `				}` |
|      ! 0 |  7995 | `				continue;` |
|        - |  7996 | `			}` |
|      ! 0 |  7997 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  7998 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  7999 | `				zCur[0] = (char)c;` |
|      ! 0 |  8000 | `			}` |
|      ! 0 |  8001 | `			zCur++;` |
|      ! 0 |  8002 | `		}` |
|        - |  8003 | `		/* Finally,register the constant */` |
|      ! 0 |  8004 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  8005 | `	}` |
|        - |  8006 | `	/* All done,return TRUE */` |
|       12 |  8007 | `	ph7_result_bool(pCtx,1);` |
|       12 |  8008 | `	return SXRET_OK;` |
|        7 |  8009 |  |
|        - |  8010 | `/*` |
|        - |  8011 | ` * value constant(string $name)` |
|        - |  8012 | ` *  Returns the value of a constant` |
|        - |  8013 | ` * Parameter` |
|        - |  8014 | ` *  $name` |
|        - |  8015 | ` *    Name of the constant.` |
|        - |  8016 | ` * Return` |
|        - |  8017 | ` *  Constant value or NULL if not defined.` |
|        - |  8018 | ` */` |
|        8 |  8019 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8020 |  |
|        - |  8021 | `	SyHashEntry *pEntry;` |
|        - |  8022 | `	ph7_constant *pCons;` |
|        - |  8023 | `	const char *zName; /* Constant name */` |
|        - |  8024 | `	ph7_value sVal;    /* Constant value */` |
|        - |  8025 | `	int nLen;` |
|       10 |  8026 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  8027 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  8028 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  8029 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8030 | `		return SXRET_OK;` |
|        - |  8031 | `	}` |
|        - |  8032 | `	/* Extract the constant name */` |
|       10 |  8033 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8034 | `	/* Perform the query */` |
|       10 |  8035 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  8036 | `	if( pEntry == 0 ){` |
|        3 |  8037 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  8038 | `		ph7_result_null(pCtx);` |
|        3 |  8039 | `		return SXRET_OK;` |
|        - |  8040 | `	}` |
|        8 |  8041 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  8042 | `	/* Point to the structure that describe the constant */` |
|        8 |  8043 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  8044 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  8045 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  8046 | `	/* Return that value */` |
|        8 |  8047 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  8048 | `	/* Cleanup */` |
|        8 |  8049 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  8050 | `	return SXRET_OK;` |
|        6 |  8051 |  |
|        - |  8052 | `/*` |
|        - |  8053 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  8054 | ` * defined below.` |
|        - |  8055 | ` */` |
|      414 |  8056 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  8057 |  |
|      415 |  8058 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  8059 | `	ph7_value sName;` |
|        - |  8060 | `	sxi32 rc;` |
|        - |  8061 | `	/* Prepare the constant name for insertion */` |
|      415 |  8062 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      415 |  8063 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  8064 | `	/* Perform the insertion */` |
|      415 |  8065 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      415 |  8066 | `	PH7_MemObjRelease(&sName);` |
|      415 |  8067 | `	return rc;` |
|        1 |  8068 |  |
|        - |  8069 | `/*` |
|        - |  8070 | ` * array get_defined_constants(void)` |
|        - |  8071 | ` *  Returns an associative array with the names of all defined` |
|        - |  8072 | ` *  constants.` |
|        - |  8073 | ` * Parameters` |
|        - |  8074 | ` *  NONE.` |
|        - |  8075 | ` * Returns` |
|        - |  8076 | ` *  Returns the names of all the constants currently defined.` |
|        - |  8077 | ` */` |
|        2 |  8078 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8079 |  |
|        - |  8080 | `	ph7_value *pArray;` |
|        - |  8081 | `	/* Create the array first*/` |
|        3 |  8082 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8083 | `	if( pArray == 0 ){` |
|      ! 0 |  8084 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8085 | `		SXUNUSED(apArg);` |
|        - |  8086 | `		/* Return NULL */` |
|      ! 0 |  8087 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8088 | `		return SXRET_OK;` |
|        - |  8089 | `	}` |
|        - |  8090 | `	/* Fill the array with the defined constants */` |
|        3 |  8091 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  8092 | `	/* Return the created array */` |
|        3 |  8093 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8094 | `	return SXRET_OK;` |
|        2 |  8095 |  |
|        - |  8096 | `/*` |
|        - |  8097 | ` * Section:` |
|        - |  8098 | ` *  Output Control (OB) functions.` |
|        - |  8099 | ` * Status:` |
|        - |  8100 | ` *    Stable.` |
|        - |  8101 | ` */` |
|        - |  8102 | `/* Forward declaration */` |
|        - |  8103 | `static void VmObRestore(ph7_vm *pVm,VmObEntry *pEntry);` |
|        - |  8104 | `/*` |
|        - |  8105 | ` * void ob_clean(void)` |
|        - |  8106 | ` *  This function discards the contents of the output buffer.` |
|        - |  8107 | ` *  This function does not destroy the output buffer like ob_end_clean() does.` |
|        - |  8108 | ` * Parameter` |
|        - |  8109 | ` *  None` |
|        - |  8110 | ` * Return` |
|        - |  8111 | ` *  No value is returned.` |
|        - |  8112 | ` */` |
|        2 |  8113 | `static int vm_builtin_ob_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8114 |  |
|        3 |  8115 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8116 | `	VmObEntry *pOb;` |
|        1 |  8117 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8118 | `	SXUNUSED(apArg);` |
|        - |  8119 | `	/* Peek the top most OB */` |
|        3 |  8120 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|        3 |  8121 | `	if( pOb ){` |
|        3 |  8122 | `		SyBlobRelease(&pOb->sOB);` |
|        1 |  8123 | `	}` |
|        3 |  8124 | `	return PH7_OK;` |
|        1 |  8125 |  |
|        - |  8126 | `/*` |
|        - |  8127 | ` * bool ob_end_clean(void)` |
|        - |  8128 | ` *  Clean (erase) the output buffer and turn off output buffering` |
|        - |  8129 | ` *  This function discards the contents of the topmost output buffer and turns` |
|        - |  8130 | ` *  off this output buffering. If you want to further process the buffer's contents` |
|        - |  8131 | ` *  you have to call ob_get_contents() before ob_end_clean() as the buffer contents` |
|        - |  8132 | ` *  are discarded when ob_end_clean() is called.` |
|        - |  8133 | ` * Parameter` |
|        - |  8134 | ` *  None` |
|        - |  8135 | ` * Return` |
|        - |  8136 | ` *  Returns TRUE on success or FALSE on failure. Reasons for failure are first that you called` |
|        - |  8137 | ` *  the function without an active buffer or that for some reason a buffer could not be deleted` |
|        - |  8138 | ` * (possible for special buffer)` |
|        - |  8139 | ` */` |
|     2628 |  8140 | `static int vm_builtin_ob_end_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8141 |  |
|     2630 |  8142 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8143 | `	VmObEntry *pOb;` |
|        - |  8144 | `	/* Pop the top most OB */` |
|     2630 |  8145 | `	pOb = (VmObEntry *)SySetPop(&pVm->aOB);` |
|     2630 |  8146 | `	if( pOb == 0){` |
|        - |  8147 | `		/* No such OB,return FALSE */` |
|      ! 0 |  8148 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8149 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8150 | `		SXUNUSED(apArg);` |
|      ! 0 |  8151 | `	}else{` |
|        - |  8152 | `		/* Release */` |
|     2630 |  8153 | `		VmObRestore(pVm,pOb);` |
|        - |  8154 | `		/* Return true */` |
|     2630 |  8155 | `		ph7_result_bool(pCtx,1);` |
|        - |  8156 | `	}` |
|     2630 |  8157 | `	return PH7_OK;` |
|        2 |  8158 |  |
|        - |  8159 | `/*` |
|        - |  8160 | ` * string ob_get_contents(void)` |
|        - |  8161 | ` *  Gets the contents of the output buffer without clearing it.` |
|        - |  8162 | ` * Parameter` |
|        - |  8163 | ` *  None` |
|        - |  8164 | ` * Return` |
|        - |  8165 | ` *  This will return the contents of the output buffer or FALSE, if output buffering isn't active.` |
|        - |  8166 | ` */` |
|        6 |  8167 | `static int vm_builtin_ob_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8168 |  |
|        7 |  8169 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8170 | `	VmObEntry *pOb;` |
|        - |  8171 | `	/* Peek the top most OB */` |
|        7 |  8172 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|        7 |  8173 | `	if( pOb == 0 ){` |
|        - |  8174 | `		/* No active OB,return FALSE */` |
|      ! 0 |  8175 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8176 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8177 | `		SXUNUSED(apArg);` |
|      ! 0 |  8178 | `	}else{` |
|        - |  8179 | `		/* Return contents */` |
|        7 |  8180 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&pOb->sOB),(int)SyBlobLength(&pOb->sOB));` |
|        - |  8181 | `	}` |
|        7 |  8182 | `	return PH7_OK;` |
|        1 |  8183 |  |
|        - |  8184 | `/*` |
|        - |  8185 | ` * string ob_get_clean(void)` |
|        - |  8186 | ` * string ob_get_flush(void)` |
|        - |  8187 | ` *  Get current buffer contents and delete current output buffer.` |
|        - |  8188 | ` * Parameter` |
|        - |  8189 | ` *  None` |
|        - |  8190 | ` * Return` |
|        - |  8191 | ` *  This will return the contents of the output buffer or FALSE, if output buffering isn't active.` |
|        - |  8192 | ` */` |
|     3900 |  8193 | `static int vm_builtin_ob_get_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8194 |  |
|     3902 |  8195 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8196 | `	VmObEntry *pOb;` |
|        - |  8197 | `	/* Pop the top most OB */` |
|     3902 |  8198 | `	pOb = (VmObEntry *)SySetPop(&pVm->aOB);` |
|     3902 |  8199 | `	if( pOb == 0 ){` |
|        - |  8200 | `		/* No active OB,return FALSE */` |
|      ! 0 |  8201 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8202 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8203 | `		SXUNUSED(apArg);` |
|      ! 0 |  8204 | `	}else{` |
|        - |  8205 | `		/* Return contents */` |
|     3902 |  8206 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&pOb->sOB),(int)SyBlobLength(&pOb->sOB)); /* Will make it's own copy */` |
|        - |  8207 | `		/* Release */` |
|     3902 |  8208 | `		VmObRestore(pVm,pOb);` |
|        - |  8209 | `	}` |
|     3902 |  8210 | `	return PH7_OK;` |
|        2 |  8211 |  |
|        - |  8212 | `/*` |
|        - |  8213 | ` * int ob_get_length(void)` |
|        - |  8214 | ` *  Return the length of the output buffer.` |
|        - |  8215 | ` * Parameter` |
|        - |  8216 | ` *  None` |
|        - |  8217 | ` * Return` |
|        - |  8218 | ` *  Returns the length of the output buffer contents or FALSE if no buffering is active.` |
|        - |  8219 | ` */` |
|        2 |  8220 | `static int vm_builtin_ob_get_length(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8221 |  |
|        3 |  8222 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8223 | `	VmObEntry *pOb;` |
|        - |  8224 | `	/* Peek the top most OB */` |
|        3 |  8225 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|        3 |  8226 | `	if( pOb == 0 ){` |
|        - |  8227 | `		/* No active OB,return FALSE */` |
|      ! 0 |  8228 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8229 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8230 | `		SXUNUSED(apArg);` |
|      ! 0 |  8231 | `	}else{` |
|        - |  8232 | `		/* Return OB length */` |
|        3 |  8233 | `		ph7_result_int64(pCtx,(ph7_int64)SyBlobLength(&pOb->sOB));` |
|        - |  8234 | `	}` |
|        3 |  8235 | `	return PH7_OK;` |
|        1 |  8236 |  |
|        - |  8237 | `/*` |
|        - |  8238 | ` * int ob_get_level(void)` |
|        - |  8239 | ` *  Returns the nesting level of the output buffering mechanism.` |
|        - |  8240 | ` * Parameter` |
|        - |  8241 | ` *  None` |
|        - |  8242 | ` * Return` |
|        - |  8243 | ` *  Returns the level of nested output buffering handlers or zero if output buffering is not active.` |
|        - |  8244 | ` */` |
|        6 |  8245 | `static int vm_builtin_ob_get_level(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8246 |  |
|        7 |  8247 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8248 | `	int iNest;` |
|        3 |  8249 | `	SXUNUSED(nArg); /* cc warning */` |
|        3 |  8250 | `	SXUNUSED(apArg);` |
|        - |  8251 | `	/* Nesting level */` |
|        7 |  8252 | `	iNest = (int)SySetUsed(&pVm->aOB);` |
|        - |  8253 | `	/* Return the nesting value */` |
|        7 |  8254 | `	ph7_result_int(pCtx,iNest);` |
|        7 |  8255 | `	return PH7_OK;` |
|        1 |  8256 |  |
|        - |  8257 | `/*` |
|        - |  8258 | ` * Output Buffer(OB) default VM consumer routine.All VM output is now redirected` |
|        - |  8259 | ` * to a stackable internal buffer,until the user call [ob_get_clean(),ob_end_clean(),...].` |
|        - |  8260 | ` * Refer to the implementation of [ob_start()] for more information.` |
|        - |  8261 | ` */` |
|     5868 |  8262 | `static int VmObConsumer(const void *pData,unsigned int nDataLen,void *pUserData)` |
|        2 |  8263 |  |
|     5870 |  8264 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|        - |  8265 | `	VmObEntry *pEntry;` |
|        - |  8266 | `	ph7_value sResult;` |
|        - |  8267 | `	/* Peek the top most entry */` |
|     5870 |  8268 | `	pEntry = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|     5870 |  8269 | `	if( pEntry == 0 ){` |
|        - |  8270 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  8271 | `		return PH7_OK;` |
|        - |  8272 | `	}` |
|     5870 |  8273 | `	PH7_MemObjInit(pVm,&sResult);` |
|     5870 |  8274 | `	if( ph7_value_is_callable(&pEntry->sCallback) && pVm->nObDepth < 15 ){` |
|        - |  8275 | `		ph7_value sArg,*apArg[2];` |
|        - |  8276 | `		/* Fill the first argument */` |
|      ! 0 |  8277 | `		PH7_MemObjInitFromString(pVm,&sArg,0);` |
|      ! 0 |  8278 | `		PH7_MemObjStringAppend(&sArg,(const char *)pData,nDataLen);` |
|      ! 0 |  8279 | `		apArg[0] = &sArg;` |
|        - |  8280 | `		/* Call the 'filter' callback */` |
|      ! 0 |  8281 | `		pVm->nObDepth++;` |
|      ! 0 |  8282 | `		PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult);` |
|      ! 0 |  8283 | `		pVm->nObDepth--;` |
|      ! 0 |  8284 | `		if( sResult.iFlags & MEMOBJ_STRING ){` |
|        - |  8285 | `			/* Extract the function result */` |
|      ! 0 |  8286 | `			pData = SyBlobData(&sResult.sBlob);` |
|      ! 0 |  8287 | `			nDataLen = SyBlobLength(&sResult.sBlob);` |
|      ! 0 |  8288 | `		}` |
|      ! 0 |  8289 | `		PH7_MemObjRelease(&sArg);` |
|      ! 0 |  8290 | `	}` |
|     5870 |  8291 | `	if( nDataLen > 0 ){` |
|        - |  8292 | `		/* Redirect the VM output to the internal buffer */` |
|     5870 |  8293 | `		SyBlobAppend(&pEntry->sOB,pData,nDataLen);` |
|     2934 |  8294 | `	}` |
|        - |  8295 | `	/* Release */` |
|     5870 |  8296 | `	PH7_MemObjRelease(&sResult);` |
|     5870 |  8297 | `	return PH7_OK;` |
|     2936 |  8298 |  |
|        - |  8299 | `/*` |
|        - |  8300 | ` * Restore the default consumer.` |
|        - |  8301 | ` * Refer to the implementation of [ob_end_clean()] for more` |
|        - |  8302 | ` * information.` |
|        - |  8303 | ` */` |
|     6530 |  8304 | `static void VmObRestore(ph7_vm *pVm,VmObEntry *pEntry)` |
|        2 |  8305 |  |
|     6532 |  8306 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|     6532 |  8307 | `	if( SySetUsed(&pVm->aOB) < 1 ){` |
|        - |  8308 | `		/* No more stackable OB */` |
|     6514 |  8309 | `		pCons->xConsumer = pCons->xDef;` |
|     6514 |  8310 | `		pCons->pUserData = pCons->pDefData;` |
|     3256 |  8311 | `	}` |
|        - |  8312 | `	/* Release OB data */` |
|     6532 |  8313 | `	PH7_MemObjRelease(&pEntry->sCallback);` |
|     6532 |  8314 | `	SyBlobRelease(&pEntry->sOB);` |
|     6532 |  8315 |  |
|        - |  8316 | `/*` |
|        - |  8317 | ` * bool ob_start([ callback $output_callback] )` |
|        - |  8318 | ` * This function will turn output buffering on. While output buffering is active no output` |
|        - |  8319 | ` *  is sent from the script (other than headers), instead the output is stored in an internal` |
|        - |  8320 | ` *  buffer.` |
|        - |  8321 | ` * Parameter` |
|        - |  8322 | ` *  $output_callback` |
|        - |  8323 | ` *   An optional output_callback function may be specified. This function takes a string` |
|        - |  8324 | ` *   as a parameter and should return a string. The function will be called when the output` |
|        - |  8325 | ` *   buffer is flushed (sent) or cleaned (with ob_flush(), ob_clean() or similar function)` |
|        - |  8326 | ` *   or when the output buffer is flushed to the browser at the end of the request.` |
|        - |  8327 | ` *   When output_callback is called, it will receive the contents of the output buffer` |
|        - |  8328 | ` *   as its parameter and is expected to return a new output buffer as a result, which will` |
|        - |  8329 | ` *   be sent to the browser. If the output_callback is not a callable function, this function` |
|        - |  8330 | ` *   will return FALSE.` |
|        - |  8331 | ` *   If the callback function has two parameters, the second parameter is filled with` |
|        - |  8332 | ` *   a bit-field consisting of PHP_OUTPUT_HANDLER_START, PHP_OUTPUT_HANDLER_CONT` |
|        - |  8333 | ` *   and PHP_OUTPUT_HANDLER_END.` |
|        - |  8334 | ` *   If output_callback returns FALSE original input is sent to the browser.` |
|        - |  8335 | ` *   The output_callback parameter may be bypassed by passing a NULL value.` |
|        - |  8336 | ` * Return` |
|        - |  8337 | ` *   Returns TRUE on success or FALSE on failure.` |
|        - |  8338 | ` */` |
|     6530 |  8339 | `static int vm_builtin_ob_start(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8340 |  |
|     6532 |  8341 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8342 | `	VmObEntry sOb;` |
|        - |  8343 | `	sxi32 rc;` |
|        - |  8344 | `	/* Initialize the OB entry */` |
|     6532 |  8345 | `	PH7_MemObjInit(pCtx->pVm,&sOb.sCallback);` |
|     6532 |  8346 | `	SyBlobInit(&sOb.sOB,&pVm->sAllocator);` |
|     6532 |  8347 | `	if( nArg > 0 && (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) ){` |
|        - |  8348 | `		/* Save the callback name for later invocation */` |
|      ! 0 |  8349 | `		PH7_MemObjStore(apArg[0],&sOb.sCallback);` |
|      ! 0 |  8350 | `	}` |
|        - |  8351 | `	/* Push in the stack */` |
|     6532 |  8352 | `	rc = SySetPut(&pVm->aOB,(const void *)&sOb);` |
|     6532 |  8353 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8354 | `		PH7_MemObjRelease(&sOb.sCallback);` |
|      ! 0 |  8355 | `	}else{` |
|     6532 |  8356 | `		ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        - |  8357 | `		/* Substitute the default VM consumer */` |
|     6532 |  8358 | `		if( pCons->xConsumer != VmObConsumer ){` |
|     6514 |  8359 | `			pCons->xDef = pCons->xConsumer;` |
|     6514 |  8360 | `			pCons->pDefData = pCons->pUserData;` |
|        - |  8361 | `			/* Install the new consumer */` |
|     6514 |  8362 | `			pCons->xConsumer = VmObConsumer;` |
|     6514 |  8363 | `			pCons->pUserData = pVm;` |
|     3256 |  8364 | `		}` |
|        - |  8365 | `	}` |
|     6532 |  8366 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|     6532 |  8367 | `	return PH7_OK;` |
|        2 |  8368 |  |
|        - |  8369 | `/*` |
|        - |  8370 | ` * Flush Output buffer to the default VM output consumer.` |
|        - |  8371 | ` * Refer to the implementation of [ob_flush()] for more` |
|        - |  8372 | ` * information.` |
|        - |  8373 | ` */` |
|        4 |  8374 | `static sxi32 VmObFlush(ph7_vm *pVm,VmObEntry *pEntry,int bRelease)` |
|        1 |  8375 |  |
|        5 |  8376 | `	SyBlob *pBlob = &pEntry->sOB;` |
|        - |  8377 | `	sxi32 rc;` |
|        - |  8378 | `	/* Flush contents */` |
|        5 |  8379 | `	rc = PH7_OK;` |
|        5 |  8380 | `	if( SyBlobLength(pBlob) > 0 ){` |
|        - |  8381 | `		/* Call the VM output consumer */` |
|        5 |  8382 | `		rc = pVm->sVmConsumer.xDef(SyBlobData(pBlob),SyBlobLength(pBlob),pVm->sVmConsumer.pDefData);` |
|        - |  8383 | `		/* Increment VM output counter */` |
|        5 |  8384 | `		pVm->nOutputLen += SyBlobLength(pBlob);` |
|        5 |  8385 | `		if( rc != PH7_ABORT ){` |
|        5 |  8386 | `			rc = PH7_OK;` |
|        2 |  8387 | `		}` |
|        2 |  8388 | `	}` |
|        5 |  8389 | `	if( bRelease ){` |
|        3 |  8390 | `		VmObRestore(&(*pVm),pEntry);` |
|        2 |  8391 | `	}else{` |
|        - |  8392 | `		/* Reset the blob */` |
|        3 |  8393 | `		SyBlobReset(pBlob);` |
|        - |  8394 | `	}` |
|        5 |  8395 | `	return rc;` |
|        1 |  8396 |  |
|        - |  8397 | `/*` |
|        - |  8398 | ` * void ob_flush(void)` |
|        - |  8399 | ` * void flush(void)` |
|        - |  8400 | ` *  Flush (send) the output buffer.` |
|        - |  8401 | ` * Parameter` |
|        - |  8402 | ` *  None` |
|        - |  8403 | ` * Return` |
|        - |  8404 | ` *  No return value.` |
|        - |  8405 | ` */` |
|        2 |  8406 | `static int vm_builtin_ob_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8407 |  |
|        3 |  8408 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8409 | `	VmObEntry *pOb;` |
|        - |  8410 | `	sxi32 rc;` |
|        - |  8411 | `	/* Peek the top most OB entry */` |
|        3 |  8412 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|        3 |  8413 | `	if( pOb == 0 ){` |
|        - |  8414 | `		/* Empty stack,return immediately */` |
|      ! 0 |  8415 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8416 | `		SXUNUSED(apArg);` |
|      ! 0 |  8417 | `		return PH7_OK;` |
|        - |  8418 | `	}` |
|        - |  8419 | `	/* Flush contents */` |
|        3 |  8420 | `	rc = VmObFlush(pVm,pOb,FALSE);` |
|        3 |  8421 | `	return rc;` |
|        2 |  8422 |  |
|        - |  8423 | `/*` |
|        - |  8424 | ` * bool ob_end_flush(void)` |
|        - |  8425 | ` *  Flush (send) the output buffer and turn off output buffering.` |
|        - |  8426 | ` * Parameter` |
|        - |  8427 | ` *  None` |
|        - |  8428 | ` * Return` |
|        - |  8429 | ` *  Returns TRUE on success or FALSE on failure. Reasons for failure are first` |
|        - |  8430 | ` *  that you called the function without an active buffer or that for some reason` |
|        - |  8431 | ` *  a buffer could not be deleted (possible for special buffer).` |
|        - |  8432 | ` */` |
|        2 |  8433 | `static int vm_builtin_ob_end_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8434 |  |
|        3 |  8435 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8436 | `	VmObEntry *pOb;` |
|        - |  8437 | `	sxi32 rc;` |
|        - |  8438 | `	/* Pop the top most OB entry */` |
|        3 |  8439 | `	pOb = (VmObEntry *)SySetPop(&pVm->aOB);` |
|        3 |  8440 | `	if( pOb == 0 ){` |
|        - |  8441 | `		/* Empty stack,return FALSE */` |
|      ! 0 |  8442 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8443 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8444 | `		SXUNUSED(apArg);` |
|      ! 0 |  8445 | `		return PH7_OK;` |
|        - |  8446 | `	}` |
|        - |  8447 | `	/* Flush contents */` |
|        3 |  8448 | `	rc = VmObFlush(pVm,pOb,TRUE);` |
|        - |  8449 | `	/* Return true */` |
|        3 |  8450 | `	ph7_result_bool(pCtx,1);` |
|        3 |  8451 | `	return rc;` |
|        2 |  8452 |  |
|        - |  8453 | `/*` |
|        - |  8454 | ` * void ob_implicit_flush([int $flag = true ])` |
|        - |  8455 | ` *  ob_implicit_flush() will turn implicit flushing on or off.` |
|        - |  8456 | ` *  Implicit flushing will result in a flush operation after every` |
|        - |  8457 | ` *  output call, so that explicit calls to flush() will no longer be needed.` |
|        - |  8458 | ` * Parameter` |
|        - |  8459 | ` *  $flag` |
|        - |  8460 | ` *   TRUE to turn implicit flushing on, FALSE otherwise.` |
|        - |  8461 | ` * Return` |
|        - |  8462 | ` *   Nothing` |
|        - |  8463 | ` */` |
|        4 |  8464 | `static int vm_builtin_ob_implicit_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8465 |  |
|        - |  8466 | `	/* NOTE: As of this version,this function is a no-op.` |
|        - |  8467 | `	 * PH7 is smart enough to flush it's internal buffer when appropriate.` |
|        - |  8468 | `	 */` |
|        2 |  8469 | `	SXUNUSED(pCtx);` |
|        2 |  8470 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  8471 | `	SXUNUSED(apArg);` |
|        5 |  8472 | `	return PH7_OK;` |
|        1 |  8473 |  |
|        - |  8474 | `/*` |
|        - |  8475 | ` * array ob_list_handlers(void)` |
|        - |  8476 | ` *  Lists all output handlers in use.` |
|        - |  8477 | ` * Parameter` |
|        - |  8478 | ` *  None` |
|        - |  8479 | ` * Return` |
|        - |  8480 | ` *  This will return an array with the output handlers in use (if any).` |
|        - |  8481 | ` */` |
|        2 |  8482 | `static int vm_builtin_ob_list_handlers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8483 |  |
|        3 |  8484 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8485 | `	ph7_value *pArray;` |
|        - |  8486 | `	VmObEntry *aEntry;` |
|        - |  8487 | `	ph7_value sVal;` |
|        - |  8488 | `	sxu32 n;` |
|        3 |  8489 | `	if( SySetUsed(&pVm->aOB) < 1 ){` |
|        - |  8490 | `		/* Empty stack,return null */` |
|      ! 0 |  8491 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8492 | `		return PH7_OK;` |
|        - |  8493 | `	}` |
|        - |  8494 | `	/* Create a new array */` |
|        3 |  8495 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8496 | `	if( pArray == 0 ){` |
|        - |  8497 | `		/* Out of memory,return NULL */` |
|      ! 0 |  8498 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8499 | `		SXUNUSED(apArg);` |
|      ! 0 |  8500 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8501 | `		return PH7_OK;` |
|        - |  8502 | `	}` |
|        3 |  8503 | `	PH7_MemObjInit(pVm,&sVal);` |
|        - |  8504 | `	/* Point to the installed OB entries */` |
|        3 |  8505 | `	aEntry = (VmObEntry *)SySetBasePtr(&pVm->aOB);` |
|        - |  8506 | `	/* Perform the requested operation */` |
|        5 |  8507 | `	for( n = 0 ; n < SySetUsed(&pVm->aOB) ; n++ ){` |
|        3 |  8508 | `		VmObEntry *pEntry = &aEntry[n];` |
|        - |  8509 | `		/* Extract handler name */` |
|        3 |  8510 | `		SyBlobReset(&sVal.sBlob);` |
|        3 |  8511 | `		if( pEntry->sCallback.iFlags & MEMOBJ_STRING ){` |
|        - |  8512 | `			/* Callback,dup it's name */` |
|      ! 0 |  8513 | `			SyBlobDup(&pEntry->sCallback.sBlob,&sVal.sBlob);` |
|        3 |  8514 | `		}else if( pEntry->sCallback.iFlags & MEMOBJ_HASHMAP ){` |
|      ! 0 |  8515 | `			SyBlobAppend(&sVal.sBlob,"Class Method",sizeof("Class Method")-1);` |
|      ! 0 |  8516 | `		}else{` |
|        3 |  8517 | `			SyBlobAppend(&sVal.sBlob,"default output handler",sizeof("default output handler")-1);` |
|        - |  8518 | `		}` |
|        3 |  8519 | `		sVal.iFlags = MEMOBJ_STRING;` |
|        - |  8520 | `		/* Perform the insertion */` |
|        3 |  8521 | `		ph7_array_add_elem(pArray,0/* Automatic index assign */,&sVal /* Will make it's own copy */);` |
|        2 |  8522 | `	}` |
|        3 |  8523 | `	PH7_MemObjRelease(&sVal);` |
|        - |  8524 | `	/* Return the freshly created array */` |
|        3 |  8525 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8526 | `	return PH7_OK;` |
|        2 |  8527 |  |
|        - |  8528 | `/*` |
|        - |  8529 | ` * Section:` |
|        - |  8530 | ` *  Random numbers/string generators.` |
|        - |  8531 | ` * Status:` |
|        - |  8532 | ` *    Stable.` |
|        - |  8533 | ` */` |
|        - |  8534 | `/*` |
|        - |  8535 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  8536 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  8537 | ` * used by te SQLite3 library.` |
|        - |  8538 | ` */` |
|     1026 |  8539 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  8540 |  |
|        - |  8541 | `	sxu32 iNum;` |
|     1028 |  8542 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     1028 |  8543 | `	return iNum;` |
|        2 |  8544 |  |
|        - |  8545 | `/*` |
|        - |  8546 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  8547 | ` * Note that the generated string is NOT null terminated.` |
|        - |  8548 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  8549 | ` * by te SQLite3 library.` |
|        - |  8550 | ` */` |
|    35616 |  8551 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  8552 |  |
|        - |  8553 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  8554 | `	int i;` |
|        - |  8555 | `	/* Generate a binary string first */` |
|    35618 |  8556 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  8557 | `	/* Turn the binary string into english based alphabet */` |
|   391950 |  8558 | `	for( i = 0 ; i < nLen ; ++i ){` |
|   356334 |  8559 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   178168 |  8560 | `	 }` |
|    35618 |  8561 |  |
|        - |  8562 | `/*` |
|        - |  8563 | ` * int rand()` |
|        - |  8564 | ` * int mt_rand()` |
|        - |  8565 | ` * int rand(int $min,int $max)` |
|        - |  8566 | ` * int mt_rand(int $min,int $max)` |
|        - |  8567 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  8568 | ` * Parameter` |
|        - |  8569 | ` *  $min` |
|        - |  8570 | ` *    The lowest value to return (default: 0)` |
|        - |  8571 | ` *  $max` |
|        - |  8572 | ` *   The highest value to return (default: getrandmax())` |
|        - |  8573 | ` * Return` |
|        - |  8574 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  8575 | ` * Note:` |
|        - |  8576 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  8577 | ` *  by te SQLite3 library.` |
|        - |  8578 | ` */` |
|       20 |  8579 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8580 |  |
|        - |  8581 | `	sxu32 iNum;` |
|        - |  8582 | `	/* Generate the random number */` |
|       21 |  8583 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  8584 | `	if( nArg > 1 ){` |
|        - |  8585 | `		sxu32 iMin,iMax;` |
|        3 |  8586 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  8587 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  8588 | `		if( iMin < iMax ){` |
|        3 |  8589 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  8590 | `			if( iDiv > 0 ){` |
|        3 |  8591 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  8592 | `			}` |
|        1 |  8593 | `		}else if(iMax > 0 ){` |
|      ! 0 |  8594 | `			iNum %= iMax;` |
|      ! 0 |  8595 | `		}` |
|        1 |  8596 | `	}` |
|        - |  8597 | `	/* Return the number */` |
|       21 |  8598 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  8599 | `	return SXRET_OK;` |
|        1 |  8600 |  |
|        - |  8601 | `/*` |
|        - |  8602 | ` * int getrandmax(void)` |
|        - |  8603 | ` * int mt_getrandmax(void)` |
|        - |  8604 | ` * int rc4_getrandmax(void)` |
|        - |  8605 | ` *   Show largest possible random value` |
|        - |  8606 | ` * Return` |
|        - |  8607 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  8608 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  8609 | ` * Note:` |
|        - |  8610 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  8611 | ` *  by te SQLite3 library.` |
|        - |  8612 | ` */` |
|        4 |  8613 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8614 |  |
|        2 |  8615 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  8616 | `	SXUNUSED(apArg);` |
|        5 |  8617 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  8618 | `	return SXRET_OK;` |
|        1 |  8619 |  |
|        - |  8620 | `/*` |
|        - |  8621 | ` * string rand_str()` |
|        - |  8622 | ` * string rand_str(int $len)` |
|        - |  8623 | ` *  Generate a random string (English alphabet).` |
|        - |  8624 | ` * Parameter` |
|        - |  8625 | ` *  $len` |
|        - |  8626 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  8627 | ` * Return` |
|        - |  8628 | ` *   A pseudo random string.` |
|        - |  8629 | ` * Note:` |
|        - |  8630 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  8631 | ` *  by te SQLite3 library.` |
|        - |  8632 | ` *  This function is a symisc extension.` |
|        - |  8633 | ` */` |
|      122 |  8634 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8635 |  |
|        - |  8636 | `	char zString[1024];` |
|      124 |  8637 | `	int iLen = 0x10;` |
|      124 |  8638 | `	if( nArg > 0 ){` |
|        - |  8639 | `		/* Get the desired length */` |
|      124 |  8640 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      124 |  8641 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  8642 | `			/* Default length */` |
|        3 |  8643 | `			iLen = 0x10;` |
|        1 |  8644 | `		}` |
|       61 |  8645 | `	}` |
|        - |  8646 | `	/* Generate the random string */` |
|      124 |  8647 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  8648 | `	/* Return the generated string */` |
|      124 |  8649 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      124 |  8650 | `	return SXRET_OK;` |
|        2 |  8651 |  |
|        - |  8652 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  8653 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  8654 | `/* Unique ID private data */` |
|        - |  8655 | `struct unique_id_data` |
|        - |  8656 |  |
|        - |  8657 | `	ph7_context *pCtx; /* Call context */` |
|        - |  8658 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  8659 | `};` |
|        - |  8660 | `/*` |
|        - |  8661 | ` * Binary to hex consumer callback.` |
|        - |  8662 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  8663 | ` * defined below.` |
|        - |  8664 | ` */` |
|      192 |  8665 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  8666 |  |
|      193 |  8667 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  8668 | `	sxu32 nBuflen;` |
|        - |  8669 | `	/* Extract result buffer length */` |
|      193 |  8670 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  8671 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  8672 | `			/*` |
|        - |  8673 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  8674 | `			 * string will be 13 characters long` |
|        - |  8675 | `			 */` |
|       25 |  8676 | `		return SXERR_ABORT;` |
|        - |  8677 | `	}` |
|      169 |  8678 | `	if( nBuflen > 22 ){` |
|      ! 0 |  8679 | `		return SXERR_ABORT;` |
|        - |  8680 | `	}` |
|        - |  8681 | `	/* Safely Consume the hex stream */` |
|      169 |  8682 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  8683 | `	return SXRET_OK;` |
|       97 |  8684 |  |
|        - |  8685 | `/*` |
|        - |  8686 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  8687 | ` *  Generate a unique ID` |
|        - |  8688 | ` * Parameter` |
|        - |  8689 | ` * $prefix` |
|        - |  8690 | ` *  Append this prefix to the generated unique ID.` |
|        - |  8691 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  8692 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  8693 | ` * $more_entropy` |
|        - |  8694 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  8695 | ` *  that the result will be unique.` |
|        - |  8696 | ` * Return` |
|        - |  8697 | ` *  Returns the unique identifier, as a string.` |
|        - |  8698 | ` */` |
|       24 |  8699 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8700 |  |
|        - |  8701 | `	struct unique_id_data sUniq;` |
|        - |  8702 | `	unsigned char zDigest[20];` |
|       25 |  8703 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8704 | `	const char *zPrefix;` |
|        - |  8705 | `	SHA1Context sCtx;` |
|        - |  8706 | `	char zRandom[7];` |
|        - |  8707 | `	int nPrefix;` |
|        - |  8708 | `	int entropy;` |
|        - |  8709 | `	/* Generate a random string first */` |
|       25 |  8710 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  8711 | `	/* Initialize fields */` |
|       25 |  8712 | `	zPrefix = 0;` |
|       25 |  8713 | `	nPrefix = 0;` |
|       25 |  8714 | `	entropy = 0;` |
|       25 |  8715 | `	if( nArg > 0 ){` |
|        - |  8716 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  8717 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  8718 | `		if( nArg > 1 ){` |
|      ! 0 |  8719 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  8720 | `		}` |
|      ! 0 |  8721 | `	}` |
|       25 |  8722 | `	SHA1Init(&sCtx);` |
|        - |  8723 | `	/* Generate the random ID */` |
|       25 |  8724 | `	if( nPrefix > 0 ){` |
|      ! 0 |  8725 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  8726 | `	}` |
|        - |  8727 | `	/* Append the random ID */` |
|       25 |  8728 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  8729 | `	/* Append the random string */` |
|       25 |  8730 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  8731 | `	/* Increment the number */` |
|       25 |  8732 | `	pVm->unique_id++;` |
|       25 |  8733 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  8734 | `	/* Hexify the digest */` |
|       25 |  8735 | `	sUniq.pCtx = pCtx;` |
|       25 |  8736 | `	sUniq.entropy = entropy;` |
|       25 |  8737 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  8738 | `	/* All done */` |
|       25 |  8739 | `	return PH7_OK;` |
|        1 |  8740 |  |
|        - |  8741 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  8742 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  8743 | `/*` |
|        - |  8744 | ` * Section:` |
|        - |  8745 | ` *  Language construct implementation as foreign functions.` |
|        - |  8746 | ` * Status:` |
|        - |  8747 | ` *    Stable.` |
|        - |  8748 | ` */` |
|        - |  8749 | `/*` |
|        - |  8750 | ` * void echo($string...)` |
|        - |  8751 | ` *  Output one or more messages.` |
|        - |  8752 | ` * Parameters` |
|        - |  8753 | ` *  $string` |
|        - |  8754 | ` *   Message to output.` |
|        - |  8755 | ` * Return` |
|        - |  8756 | ` *  NULL.` |
|        - |  8757 | ` */` |
|      ! 0 |  8758 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  8759 |  |
|        - |  8760 | `	const char *zData;` |
|      ! 0 |  8761 | `	int nDataLen = 0;` |
|        - |  8762 | `	ph7_vm *pVm;` |
|        - |  8763 | `	int i,rc;` |
|        - |  8764 | `	/* Point to the target VM */` |
|      ! 0 |  8765 | `	pVm = pCtx->pVm;` |
|        - |  8766 | `	/* Output */` |
|      ! 0 |  8767 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  8768 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  8769 | `		if( nDataLen > 0 ){` |
|      ! 0 |  8770 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  8771 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  8772 | `				/* Increment output length */` |
|      ! 0 |  8773 | `				pVm->nOutputLen += nDataLen;` |
|      ! 0 |  8774 | `			}` |
|      ! 0 |  8775 | `			if( rc == SXERR_ABORT ){` |
|        - |  8776 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  8777 | `				return PH7_ABORT;` |
|        - |  8778 | `			}` |
|      ! 0 |  8779 | `		}` |
|      ! 0 |  8780 | `	}` |
|      ! 0 |  8781 | `	return SXRET_OK;` |
|      ! 0 |  8782 |  |
|        - |  8783 | `/*` |
|        - |  8784 | ` * int print($string...)` |
|        - |  8785 | ` *  Output one or more messages.` |
|        - |  8786 | ` * Parameters` |
|        - |  8787 | ` *  $string` |
|        - |  8788 | ` *   Message to output.` |
|        - |  8789 | ` * Return` |
|        - |  8790 | ` *  1 always.` |
|        - |  8791 | ` */` |
|        2 |  8792 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8793 |  |
|        - |  8794 | `	const char *zData;` |
|        3 |  8795 | `	int nDataLen = 0;` |
|        - |  8796 | `	ph7_vm *pVm;` |
|        - |  8797 | `	int i,rc;` |
|        - |  8798 | `	/* Point to the target VM */` |
|        3 |  8799 | `	pVm = pCtx->pVm;` |
|        - |  8800 | `	/* Output */` |
|        5 |  8801 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 |  8802 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 |  8803 | `		if( nDataLen > 0 ){` |
|        3 |  8804 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 |  8805 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  8806 | `				/* Increment output length */` |
|        3 |  8807 | `				pVm->nOutputLen += nDataLen;` |
|        1 |  8808 | `			}` |
|        3 |  8809 | `			if( rc == SXERR_ABORT ){` |
|        - |  8810 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  8811 | `				return PH7_ABORT;` |
|        - |  8812 | `			}` |
|        1 |  8813 | `		}` |
|        2 |  8814 | `	}` |
|        - |  8815 | `	/* Return 1 */` |
|        3 |  8816 | `	ph7_result_int(pCtx,1);` |
|        3 |  8817 | `	return SXRET_OK;` |
|        2 |  8818 |  |
|        - |  8819 | `/*` |
|        - |  8820 | ` * void exit(string $msg)` |
|        - |  8821 | ` * void exit(int $status)` |
|        - |  8822 | ` * void die(string $ms)` |
|        - |  8823 | ` * void die(int $status)` |
|        - |  8824 | ` *   Output a message and terminate program execution.` |
|        - |  8825 | ` * Parameter` |
|        - |  8826 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  8827 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  8828 | ` *  and not printed` |
|        - |  8829 | ` * Return` |
|        - |  8830 | ` *  NULL` |
|        - |  8831 | ` */` |
|      ! 0 |  8832 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  8833 |  |
|      ! 0 |  8834 | `	if( nArg > 0 ){` |
|      ! 0 |  8835 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  8836 | `			const char *zData;` |
|      ! 0 |  8837 | `			int iLen = 0;` |
|        - |  8838 | `			/* Print exit message */` |
|      ! 0 |  8839 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  8840 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  8841 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  8842 | `			sxi32 iExitStatus;` |
|        - |  8843 | `			/* Record exit status code */` |
|      ! 0 |  8844 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  8845 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  8846 | `		}` |
|      ! 0 |  8847 | `	}` |
|        - |  8848 | `	/* Check if we are in an included file */` |
|      ! 0 |  8849 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - |  8850 | `		/* Exit the entire process */` |
|      ! 0 |  8851 | `		exit(pCtx->pVm->iExitStatus);` |
|        - |  8852 | `	}` |
|        - |  8853 | `	/* Abort processing immediately */` |
|      ! 0 |  8854 | `	return PH7_ABORT;` |
|      ! 0 |  8855 |  |
|        - |  8856 | `/*` |
|        - |  8857 | ` * bool isset($var,...)` |
|        - |  8858 | ` *  Finds out whether a variable is set.` |
|        - |  8859 | ` * Parameters` |
|        - |  8860 | ` *  One or more variable to check.` |
|        - |  8861 | ` * Return` |
|        - |  8862 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |  8863 | ` */` |
|    50832 |  8864 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8865 |  |
|        - |  8866 | `	ph7_value *pObj;` |
|    50834 |  8867 | `	int res = 0;` |
|        - |  8868 | `	int i;` |
|    50834 |  8869 | `	if( nArg < 1 ){` |
|        - |  8870 | `		/* Missing arguments,return false */` |
|      ! 0 |  8871 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  8872 | `		return SXRET_OK;` |
|        - |  8873 | `	}` |
|        - |  8874 | `	/* Iterate over available arguments */` |
|    68486 |  8875 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    50834 |  8876 | `		pObj = apArg[i];` |
|    50834 |  8877 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    33146 |  8878 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  8879 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  8880 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  8881 | `			}` |
|    16572 |  8882 | `		}` |
|    50834 |  8883 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    50834 |  8884 | `		if( !res ){` |
|        - |  8885 | `			/* Variable not set,return FALSE */` |
|    33182 |  8886 | `			ph7_result_bool(pCtx,0);` |
|    33182 |  8887 | `			return SXRET_OK;` |
|        - |  8888 | `		}` |
|     8828 |  8889 | `	}` |
|        - |  8890 | `	/* All given variable are set,return TRUE */` |
|    17654 |  8891 | `	ph7_result_bool(pCtx,1);` |
|    17654 |  8892 | `	return SXRET_OK;` |
|    25418 |  8893 |  |
|        - |  8894 | `/*` |
|        - |  8895 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  8896 | ` * frame,the reference table and discard it's contents.` |
|        - |  8897 | ` * This function never fail and always return SXRET_OK.` |
|        - |  8898 | ` */` |
|   595276 |  8899 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  8900 |  |
|        - |  8901 | `	ph7_value *pObj;` |
|        - |  8902 | `	VmRefObj *pRef;` |
|   595278 |  8903 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|   595278 |  8904 | `	if( pObj ){` |
|        - |  8905 | `		/* Release the object */` |
|   595278 |  8906 | `		PH7_MemObjRelease(pObj);` |
|   297638 |  8907 | `	}` |
|        - |  8908 | `	/* Remove old reference links */` |
|   595278 |  8909 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|   595278 |  8910 | `	if( pRef ){` |
|   595258 |  8911 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  8912 | `		/* Unlink from the reference table */` |
|   595258 |  8913 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|   595258 |  8914 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  8915 | `			VmSlot sFree;` |
|        - |  8916 | `			/* Restore to the free list */` |
|   595252 |  8917 | `			sFree.nIdx = nObjIdx;` |
|   595252 |  8918 | `			sFree.pUserData = 0;` |
|   595252 |  8919 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|   297625 |  8920 | `		}` |
|   297628 |  8921 | `	}` |
|   595278 |  8922 | `	return SXRET_OK;` |
|        2 |  8923 |  |
|        - |  8924 | `/*` |
|        - |  8925 | ` * void unset($var,...)` |
|        - |  8926 | ` *   Unset one or more given variable.` |
|        - |  8927 | ` * Parameters` |
|        - |  8928 | ` *  One or more variable to unset.` |
|        - |  8929 | ` * Return` |
|        - |  8930 | ` *  Nothing.` |
|        - |  8931 | ` */` |
|     2638 |  8932 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8933 |  |
|        - |  8934 | `	ph7_value *pObj;` |
|        - |  8935 | `	ph7_vm *pVm;` |
|        - |  8936 | `	int i;` |
|        - |  8937 | `	/* Point to the target VM */` |
|     2640 |  8938 | `	pVm = pCtx->pVm;` |
|        - |  8939 | `	/* Iterate and unset */` |
|     8122 |  8940 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     5484 |  8941 | `		pObj = apArg[i];` |
|     5484 |  8942 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      700 |  8943 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  8944 | `				/* Throw an error */` |
|      ! 0 |  8945 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  8946 | `			}` |
|      351 |  8947 | `		}else{` |
|     4785 |  8948 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  8949 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     4785 |  8950 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     4779 |  8951 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     2389 |  8952 | `			}` |
|        - |  8953 | `		}` |
|     2743 |  8954 | `	}` |
|     2640 |  8955 | `	return SXRET_OK;` |
|        2 |  8956 |  |
|        - |  8957 | `/*` |
|        - |  8958 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  8959 | ` */` |
|      108 |  8960 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  8961 |  |
|      109 |  8962 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      109 |  8963 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  8964 | `	ph7_value *pObj;` |
|        - |  8965 | `	sxu32 nIdx;` |
|        - |  8966 | `	/* Extract the memory object */` |
|      109 |  8967 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      109 |  8968 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      109 |  8969 | `	if( pObj ){` |
|      109 |  8970 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      107 |  8971 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  8972 | `				SyString sName;` |
|        - |  8973 | `				ph7_value sKey;` |
|        - |  8974 | `				/* Perform the insertion */` |
|      107 |  8975 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      107 |  8976 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      107 |  8977 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      107 |  8978 | `				PH7_MemObjRelease(&sKey);` |
|       53 |  8979 | `			}` |
|       53 |  8980 | `		}` |
|       54 |  8981 | `	}` |
|      109 |  8982 | `	return SXRET_OK;` |
|        1 |  8983 |  |
|        - |  8984 | `/*` |
|        - |  8985 | ` * array get_defined_vars(void)` |
|        - |  8986 | ` *  Returns an array of all defined variables.` |
|        - |  8987 | ` * Parameter` |
|        - |  8988 | ` *  None` |
|        - |  8989 | ` * Return` |
|        - |  8990 | ` *  An array with all the variables defined in the current scope.` |
|        - |  8991 | ` */` |
|        2 |  8992 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8993 |  |
|        3 |  8994 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8995 | `	ph7_value *pArray;` |
|        - |  8996 | `	/* Create a new array */` |
|        3 |  8997 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8998 | ` 	if( pArray == 0 ){` |
|      ! 0 |  8999 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9000 | `		SXUNUSED(apArg);` |
|        - |  9001 | `		/* Return NULL */` |
|      ! 0 |  9002 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9003 | `		return SXRET_OK;` |
|        - |  9004 | `	}` |
|        - |  9005 | `	/* Superglobals first */` |
|        3 |  9006 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - |  9007 | `	/* Then variable defined in the current frame */` |
|        3 |  9008 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  9009 | `	/* Finally,return the created array */` |
|        3 |  9010 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9011 | `	return SXRET_OK;` |
|        2 |  9012 |  |
|        - |  9013 | `/*` |
|        - |  9014 | ` * bool gettype($var)` |
|        - |  9015 | ` *  Get the type of a variable` |
|        - |  9016 | ` * Parameters` |
|        - |  9017 | ` *   $var` |
|        - |  9018 | ` *    The variable being type checked.` |
|        - |  9019 | ` * Return` |
|        - |  9020 | ` *   String representation of the given variable type.` |
|        - |  9021 | ` */` |
|       30 |  9022 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9023 |  |
|       31 |  9024 | `	const char *zType = "Empty";` |
|       31 |  9025 | `	if( nArg > 0 ){` |
|       31 |  9026 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       15 |  9027 | `	}` |
|        - |  9028 | `	/* Return the variable type */` |
|       31 |  9029 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       31 |  9030 | `	return SXRET_OK;` |
|        1 |  9031 |  |
|        - |  9032 | `/*` |
|        - |  9033 | ` * string get_resource_type(resource $handle)` |
|        - |  9034 | ` *  This function gets the type of the given resource.` |
|        - |  9035 | ` * Parameters` |
|        - |  9036 | ` *  $handle` |
|        - |  9037 | ` *  The evaluated resource handle.` |
|        - |  9038 | ` * Return` |
|        - |  9039 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  9040 | ` *  representing its type. If the type is not identified by this function` |
|        - |  9041 | ` *  the return value will be the string Unknown.` |
|        - |  9042 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  9043 | ` *  is not a resource.` |
|        - |  9044 | ` */` |
|        2 |  9045 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9046 |  |
|        3 |  9047 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  9048 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  9049 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9050 | `		return PH7_OK;` |
|        - |  9051 | `	}` |
|        3 |  9052 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 |  9053 | `	return SXRET_OK;` |
|        2 |  9054 |  |
|        - |  9055 | `/*` |
|        - |  9056 | ` * void var_dump(expression,....)` |
|        - |  9057 | ` *   var_dump � Dumps information about a variable` |
|        - |  9058 | ` * Parameters` |
|        - |  9059 | ` *   One or more expression to dump.` |
|        - |  9060 | ` * Returns` |
|        - |  9061 | ` *  Nothing.` |
|        - |  9062 | ` */` |
|      248 |  9063 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9064 |  |
|        - |  9065 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  9066 | `	int i;` |
|      250 |  9067 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  9068 | `	/* Dump one or more expressions */` |
|      504 |  9069 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      256 |  9070 | `		ph7_value *pObj = apArg[i];` |
|        - |  9071 | `		/* Reset the working buffer */` |
|      256 |  9072 | `		SyBlobReset(&sDump);` |
|        - |  9073 | `		/* Dump the given expression */` |
|      256 |  9074 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  9075 | `		/* Output */` |
|      256 |  9076 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      256 |  9077 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      127 |  9078 | `		}` |
|      129 |  9079 | `	}` |
|        - |  9080 | `	/* Release the working buffer */` |
|      250 |  9081 | `	SyBlobRelease(&sDump);` |
|      250 |  9082 | `	return SXRET_OK;` |
|        2 |  9083 |  |
|        - |  9084 | `/*` |
|        - |  9085 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  9086 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  9087 | ` * Parameters` |
|        - |  9088 | ` *   expression: Expression to dump` |
|        - |  9089 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  9090 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  9091 | ` *            print_r() will return the information rather than print it.` |
|        - |  9092 | ` * Return` |
|        - |  9093 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  9094 | ` *  Otherwise, the return value is TRUE.` |
|        - |  9095 | ` */` |
|       16 |  9096 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9097 |  |
|       17 |  9098 | `	int ret_string = 0;` |
|        - |  9099 | `	SyBlob sDump;` |
|       17 |  9100 | `	if( nArg < 1 ){` |
|        - |  9101 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9102 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9103 | `		return SXRET_OK;` |
|        - |  9104 | `	}` |
|       17 |  9105 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 |  9106 | `	if ( nArg > 1 ){` |
|        - |  9107 | `		/* Where to redirect output */` |
|       11 |  9108 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 |  9109 | `	}` |
|        - |  9110 | `	/* Generate dump */` |
|       17 |  9111 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 |  9112 | `	if( !ret_string ){` |
|        - |  9113 | `		/* Output dump */` |
|        7 |  9114 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9115 | `		/* Return true */` |
|        7 |  9116 | `		ph7_result_bool(pCtx,1);` |
|        4 |  9117 | `	}else{` |
|        - |  9118 | `		/* Generated dump as return value */` |
|       11 |  9119 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9120 | `	}` |
|        - |  9121 | `	/* Release the working buffer */` |
|       17 |  9122 | `	SyBlobRelease(&sDump);` |
|       17 |  9123 | `	return SXRET_OK;` |
|        9 |  9124 |  |
|        - |  9125 | `/*` |
|        - |  9126 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  9127 | ` * Same job as print_r. (see coment above)` |
|        - |  9128 | ` */` |
|        2 |  9129 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9130 |  |
|        3 |  9131 | `	int ret_string = 0;` |
|        - |  9132 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 |  9133 | `	if( nArg < 1 ){` |
|        - |  9134 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9135 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9136 | `		return SXRET_OK;` |
|        - |  9137 | `	}` |
|        3 |  9138 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 |  9139 | `	if ( nArg > 1 ){` |
|        - |  9140 | `		/* Where to redirect output */` |
|        3 |  9141 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 |  9142 | `	}` |
|        - |  9143 | `	/* Generate dump */` |
|        3 |  9144 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 |  9145 | `	if( !ret_string ){` |
|        - |  9146 | `		/* Output dump */` |
|      ! 0 |  9147 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9148 | `		/* Return NULL */` |
|      ! 0 |  9149 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9150 | `	}else{` |
|        - |  9151 | `		/* Generated dump as return value */` |
|        3 |  9152 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9153 | `	}` |
|        - |  9154 | `	/* Release the working buffer */` |
|        3 |  9155 | `	SyBlobRelease(&sDump);` |
|        3 |  9156 | `	return SXRET_OK;` |
|        2 |  9157 |  |
|        - |  9158 | `/*` |
|        - |  9159 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - |  9160 | ` *  Set/get the various assert flags.` |
|        - |  9161 | ` * Parameter` |
|        - |  9162 | ` * $what` |
|        - |  9163 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - |  9164 | ` *   ASSERT_WARNING         Issue a warning for each failed assertion` |
|        - |  9165 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  9166 | ` *   ASSERT_QUIET_EVAL      Not used` |
|        - |  9167 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  9168 | ` * $value` |
|        - |  9169 | ` *   An optional new value for the option.` |
|        - |  9170 | ` * Return` |
|        - |  9171 | ` *  Old setting on success or FALSE on failure.` |
|        - |  9172 | ` */` |
|        8 |  9173 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9174 |  |
|        9 |  9175 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9176 | `	int iOld,iNew,iValue;` |
|        9 |  9177 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|        - |  9178 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 |  9179 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9180 | `		return PH7_OK;` |
|        - |  9181 | `	}` |
|        - |  9182 | `	/* Save old assertion flags */` |
|        9 |  9183 | `	iOld = pVm->iAssertFlags;` |
|        - |  9184 | `	/* Extract the new flags */` |
|        9 |  9185 | `	iNew = ph7_value_to_int(apArg[0]);` |
|        9 |  9186 | `	if( iNew == PH7_ASSERT_DISABLE ){` |
|        7 |  9187 | `		pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        7 |  9188 | `		if( nArg > 1 ){` |
|        5 |  9189 | `			iValue = !ph7_value_to_bool(apArg[1]);` |
|        5 |  9190 | `			if( iValue ){` |
|        - |  9191 | `				/* Disable assertion */` |
|        3 |  9192 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        1 |  9193 | `			}` |
|        3 |  9194 | `		}` |
|        6 |  9195 | `	}else if( iNew == PH7_ASSERT_WARNING ){` |
|      ! 0 |  9196 | `		pVm->iAssertFlags &= ~PH7_ASSERT_WARNING;` |
|      ! 0 |  9197 | `		if( nArg > 1 ){` |
|      ! 0 |  9198 | `			iValue = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  9199 | `			if( iValue ){` |
|        - |  9200 | `				/* Issue a warning for each failed assertion */` |
|      ! 0 |  9201 | `				pVm->iAssertFlags \|= PH7_ASSERT_WARNING;` |
|      ! 0 |  9202 | `			}` |
|      ! 0 |  9203 | `		}` |
|        3 |  9204 | `	}else if( iNew == PH7_ASSERT_BAIL ){` |
|        3 |  9205 | `		pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        3 |  9206 | `		if( nArg > 1 ){` |
|        3 |  9207 | `			iValue = ph7_value_to_bool(apArg[1]);` |
|        3 |  9208 | `			if( iValue ){` |
|        - |  9209 | `				/* Terminate execution on failed assertions */` |
|        3 |  9210 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        1 |  9211 | `			}` |
|        2 |  9212 | `		}` |
|        1 |  9213 | `	}else if( iNew == PH7_ASSERT_CALLBACK ){` |
|      ! 0 |  9214 | `		pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|      ! 0 |  9215 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|        - |  9216 | `			/* Callback to call on failed assertions */` |
|      ! 0 |  9217 | `			PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 |  9218 | `			pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 |  9219 | `		}` |
|      ! 0 |  9220 | `	}` |
|        - |  9221 | `	/* Return the old flags */` |
|        9 |  9222 | `	ph7_result_int(pCtx,iOld);` |
|        9 |  9223 | `	return PH7_OK;` |
|        5 |  9224 |  |
|        - |  9225 | `/*` |
|        - |  9226 | ` * bool assert(mixed $assertion)` |
|        - |  9227 | ` *  Checks if assertion is FALSE.` |
|        - |  9228 | ` * Parameter` |
|        - |  9229 | ` *  $assertion` |
|        - |  9230 | ` *    The assertion to test.` |
|        - |  9231 | ` * Return` |
|        - |  9232 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - |  9233 | ` */` |
|       14 |  9234 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9235 |  |
|       15 |  9236 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9237 | `	ph7_value *pAssert;` |
|        - |  9238 | `	int iFlags,iResult;` |
|       15 |  9239 | `	if( nArg < 1 ){` |
|        - |  9240 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  9241 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9242 | `		return PH7_OK;` |
|        - |  9243 | `	}` |
|       15 |  9244 | `	iFlags = pVm->iAssertFlags;` |
|       15 |  9245 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - |  9246 | `		/* Assertion is disabled,return FALSE */` |
|      ! 0 |  9247 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9248 | `		return PH7_OK;` |
|        - |  9249 | `	}` |
|       15 |  9250 | `	pAssert = apArg[0];` |
|       15 |  9251 | `	iResult = 1; /* cc warning */` |
|       15 |  9252 | `	if( pAssert->iFlags & MEMOBJ_STRING ){` |
|        - |  9253 | `		SyString sChunk;` |
|        5 |  9254 | `		SyStringInitFromBuf(&sChunk,SyBlobData(&pAssert->sBlob),SyBlobLength(&pAssert->sBlob));` |
|        5 |  9255 | `		if( sChunk.nByte > 0 ){` |
|        5 |  9256 | `			VmEvalChunk(pVm,pCtx,&sChunk,PH7_PHP_ONLY\|PH7_PHP_EXPR,FALSE);` |
|        - |  9257 | `			/* Extract evaluation result */` |
|        5 |  9258 | `			iResult = ph7_value_to_bool(pCtx->pRet);` |
|        3 |  9259 | `		}else{` |
|      ! 0 |  9260 | `			iResult = 0;` |
|        - |  9261 | `		}` |
|        3 |  9262 | `	}else{` |
|        - |  9263 | `		/* Perform a boolean cast */` |
|       11 |  9264 | `		iResult = ph7_value_to_bool(apArg[0]);` |
|        - |  9265 | `	}` |
|       15 |  9266 | `	if( !iResult ){` |
|        - |  9267 | `		/* Assertion failed */` |
|        9 |  9268 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - |  9269 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - |  9270 | `			ph7_value sFile,sLine;` |
|        - |  9271 | `			ph7_value *apCbArg[3];` |
|        - |  9272 | `			SyString *pFile;` |
|        - |  9273 | `			/* Extract the processed script */` |
|      ! 0 |  9274 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 |  9275 | `			if( pFile == 0 ){` |
|      ! 0 |  9276 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 |  9277 | `			}` |
|        - |  9278 | `			/* Invoke the callback */` |
|      ! 0 |  9279 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 |  9280 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 |  9281 | `			apCbArg[0] = &sFile;` |
|      ! 0 |  9282 | `			apCbArg[1] = &sLine;` |
|      ! 0 |  9283 | `			apCbArg[2] = pAssert;` |
|      ! 0 |  9284 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - |  9285 | `			/* Clean-up the mess left behind */` |
|      ! 0 |  9286 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 |  9287 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 |  9288 | `		}` |
|        9 |  9289 | `		if( iFlags & PH7_ASSERT_WARNING ){` |
|        - |  9290 | `			/* Emit a warning */` |
|        9 |  9291 | `			ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Assertion failed");` |
|        4 |  9292 | `		}` |
|        9 |  9293 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - |  9294 | `			/* Abort VM execution immediately */` |
|        3 |  9295 | `			return PH7_ABORT;` |
|        - |  9296 | `		}` |
|        3 |  9297 | `	}` |
|        - |  9298 | `	/* Assertion result */` |
|       13 |  9299 | `	ph7_result_bool(pCtx,iResult);` |
|       13 |  9300 | `	return PH7_OK;` |
|        8 |  9301 |  |
|        - |  9302 | `/*` |
|        - |  9303 | ` * Section:` |
|        - |  9304 | ` *  Error reporting functions.` |
|        - |  9305 | ` * Status:` |
|        - |  9306 | ` *    Stable.` |
|        - |  9307 | ` */` |
|        - |  9308 | `/*` |
|        - |  9309 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - |  9310 | ` *  Generates a user-level error/warning/notice message.` |
|        - |  9311 | ` * Parameters` |
|        - |  9312 | ` *  $error_msg` |
|        - |  9313 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - |  9314 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - |  9315 | ` * $error_type` |
|        - |  9316 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - |  9317 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - |  9318 | ` * Return` |
|        - |  9319 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - |  9320 | ` */` |
|       12 |  9321 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9322 |  |
|       14 |  9323 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 |  9324 | `	int rc = PH7_OK;` |
|       14 |  9325 | `	if( nArg > 0 ){` |
|        - |  9326 | `		const char *zErr;` |
|        - |  9327 | `		int nLen;` |
|        - |  9328 | `		/* Extract the error message */` |
|       12 |  9329 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  9330 | `		if( nArg > 1 ){` |
|        - |  9331 | `			/* Extract the error type */` |
|       12 |  9332 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 |  9333 | `			switch( nErr ){` |
|        1 |  9334 | `			case 1:   /* E_ERROR */` |
|        - |  9335 | `			case 16:  /* E_CORE_ERROR */` |
|        - |  9336 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - |  9337 | `			case 256: /* E_USER_ERROR */` |
|        3 |  9338 | `				nErr = PH7_CTX_ERR;` |
|        3 |  9339 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 |  9340 | `				break;` |
|        1 |  9341 | `			case 2:   /* E_WARNING */` |
|        - |  9342 | `			case 32:  /* E_CORE_WARNING */` |
|        - |  9343 | `			case 123: /* E_COMPILE_WARNING */` |
|        - |  9344 | `			case 512: /* E_USER_WARNING */` |
|        3 |  9345 | `				nErr = PH7_CTX_WARNING;` |
|        3 |  9346 | `				break;` |
|        3 |  9347 | `			default:` |
|        8 |  9348 | `				nErr = PH7_CTX_NOTICE;` |
|        6 |  9349 | `				break;` |
|        - |  9350 | `			}` |
|        5 |  9351 | `		}` |
|        - |  9352 | `		/* Report error */` |
|       12 |  9353 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 |  9354 | `		if( rc == PH7_ABORT ){` |
|      ! 0 |  9355 | `			return rc;` |
|        - |  9356 | `		}` |
|        - |  9357 | `		/* Return true */` |
|       12 |  9358 | `		ph7_result_bool(pCtx,1);` |
|        7 |  9359 | `	}else{` |
|        - |  9360 | `		/* Missing arguments,return FALSE */` |
|        3 |  9361 | `		ph7_result_bool(pCtx,0);` |
|        - |  9362 | `	}` |
|       14 |  9363 | `	return rc;` |
|        8 |  9364 |  |
|        - |  9365 | `/*` |
|        - |  9366 | ` * int error_reporting([int $level])` |
|        - |  9367 | ` *  Sets which PHP errors are reported.` |
|        - |  9368 | ` * Parameters` |
|        - |  9369 | ` *  $level` |
|        - |  9370 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - |  9371 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - |  9372 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - |  9373 | ` *   levels will not always behave as expected.` |
|        - |  9374 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - |  9375 | ` *   in the predefined constants.` |
|        - |  9376 | ` * Return` |
|        - |  9377 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - |  9378 | ` *   parameter is given.` |
|        - |  9379 | ` */` |
|       18 |  9380 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9381 |  |
|       19 |  9382 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9383 | `	int nOld;` |
|        - |  9384 | `	/* Extract the old reporting level */` |
|       19 |  9385 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       19 |  9386 | `	if( nArg > 0 ){` |
|        - |  9387 | `		int nNew;` |
|        - |  9388 | `		/* Extract the desired error reporting level */` |
|       11 |  9389 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       11 |  9390 | `		if( !nNew ){` |
|        - |  9391 | `			/* Do not report errors at all */` |
|        5 |  9392 | `			pVm->bErrReport = 0;` |
|        3 |  9393 | `		}else{` |
|        - |  9394 | `			/* Report all errors */` |
|        7 |  9395 | `			pVm->bErrReport = 1;` |
|        - |  9396 | `		}` |
|        5 |  9397 | `	}` |
|        - |  9398 | `	/* Return the old level */` |
|       19 |  9399 | `	ph7_result_int(pCtx,nOld);` |
|       19 |  9400 | `	return PH7_OK;` |
|        1 |  9401 |  |
|        - |  9402 | `/*` |
|        - |  9403 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - |  9404 | ` *  Send an error message somewhere.` |
|        - |  9405 | ` * Parameter` |
|        - |  9406 | ` *  $message` |
|        - |  9407 | ` *   The error message that should be logged.` |
|        - |  9408 | ` *  $message_type` |
|        - |  9409 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - |  9410 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - |  9411 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - |  9412 | ` *       This is the default option.` |
|        - |  9413 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - |  9414 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - |  9415 | ` *    2  No longer an option.` |
|        - |  9416 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - |  9417 | ` *       to the end of the message string.` |
|        - |  9418 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - |  9419 | ` *  $destination` |
|        - |  9420 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - |  9421 | ` *  $extra_headers` |
|        - |  9422 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - |  9423 | ` * Return` |
|        - |  9424 | ` *  TRUE on success or FALSE on failure.` |
|        - |  9425 | ` * NOTE:` |
|        - |  9426 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - |  9427 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - |  9428 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - |  9429 | ` *  Otherwise this function is no-op.` |
|        - |  9430 | ` */` |
|        4 |  9431 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9432 |  |
|        - |  9433 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 |  9434 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 |  9435 | `	int iType = 0;` |
|        5 |  9436 | `	if( nArg < 1 ){` |
|        - |  9437 | `		/* Missing log message,return FALSE */` |
|      ! 0 |  9438 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9439 | `		return PH7_OK;` |
|        - |  9440 | `	}` |
|        5 |  9441 | `	if( pVm->xErrLog  ){` |
|        - |  9442 | `		/* Invoke the user callback */` |
|      ! 0 |  9443 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 |  9444 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 |  9445 | `		if( nArg > 1 ){` |
|      ! 0 |  9446 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 |  9447 | `			if( nArg > 2 ){` |
|      ! 0 |  9448 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 |  9449 | `				if( nArg > 3 ){` |
|      ! 0 |  9450 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 |  9451 | `				}` |
|      ! 0 |  9452 | `			}` |
|      ! 0 |  9453 | `		}` |
|      ! 0 |  9454 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 |  9455 | `	}` |
|        - |  9456 | `	/* Retun TRUE */` |
|        5 |  9457 | `	ph7_result_bool(pCtx,1);` |
|        5 |  9458 | `	return PH7_OK;` |
|        3 |  9459 |  |
|        - |  9460 | `/*` |
|        - |  9461 | ` * bool restore_exception_handler(void)` |
|        - |  9462 | ` *  Restores the previously defined exception handler function.` |
|        - |  9463 | ` * Parameter` |
|        - |  9464 | ` *  None` |
|        - |  9465 | ` * Return` |
|        - |  9466 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - |  9467 | ` */` |
|        4 |  9468 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9469 |  |
|        5 |  9470 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9471 | `	ph7_value *pOld,*pNew;` |
|        - |  9472 | `	/* Point to the old and the new handler */` |
|        5 |  9473 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 |  9474 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 |  9475 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  9476 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  9477 | `		SXUNUSED(apArg);` |
|        - |  9478 | `		/* No installed handler,return FALSE */` |
|        5 |  9479 | `		ph7_result_bool(pCtx,0);` |
|        5 |  9480 | `		return PH7_OK;` |
|        - |  9481 | `	}` |
|        - |  9482 | `	/* Copy the old handler */` |
|      ! 0 |  9483 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  9484 | `	PH7_MemObjRelease(pOld);` |
|        - |  9485 | `	/* Return TRUE */` |
|      ! 0 |  9486 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  9487 | `	return PH7_OK;` |
|        3 |  9488 |  |
|        - |  9489 | `/*` |
|        - |  9490 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - |  9491 | ` *  Sets a user-defined exception handler function.` |
|        - |  9492 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - |  9493 | ` * NOTE` |
|        - |  9494 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - |  9495 | ` *  the satndard PHP engine.` |
|        - |  9496 | ` * Parameters` |
|        - |  9497 | ` *  $exception_handler` |
|        - |  9498 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - |  9499 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - |  9500 | ` *   that was thrown.` |
|        - |  9501 | ` *  Note:` |
|        - |  9502 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  9503 | ` * Return` |
|        - |  9504 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - |  9505 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  9506 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  9507 | ` */` |
|        4 |  9508 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9509 |  |
|        5 |  9510 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9511 | `	ph7_value *pOld,*pNew;` |
|        - |  9512 | `	/* Point to the old and the new handler */` |
|        5 |  9513 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 |  9514 | `	pNew = &pVm->aExceptionCB[1];` |
|        - |  9515 | `	/* Return the old handler */` |
|        5 |  9516 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        5 |  9517 | `	if( nArg > 0 ){` |
|        5 |  9518 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  9519 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 |  9520 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 |  9521 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 |  9522 | `		}else{` |
|        5 |  9523 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  9524 | `			/* Install the new handler */` |
|        5 |  9525 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  9526 | `		}` |
|        2 |  9527 | `	}` |
|        5 |  9528 | `	return PH7_OK;` |
|        1 |  9529 |  |
|        - |  9530 | `/*` |
|        - |  9531 | ` * bool restore_error_handler(void)` |
|        - |  9532 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  9533 | ` * Parameters:` |
|        - |  9534 | ` *  None.` |
|        - |  9535 | ` * Return` |
|        - |  9536 | ` *  Always TRUE.` |
|        - |  9537 | ` */` |
|        4 |  9538 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9539 |  |
|        5 |  9540 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9541 | `	ph7_value *pOld,*pNew;` |
|        - |  9542 | `	/* Point to the old and the new handler */` |
|        5 |  9543 | `	pOld = &pVm->aErrCB[0];` |
|        5 |  9544 | `	pNew = &pVm->aErrCB[1];` |
|        5 |  9545 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  9546 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  9547 | `		SXUNUSED(apArg);` |
|        - |  9548 | `		/* No installed callback,return FALSE */` |
|        5 |  9549 | `		ph7_result_bool(pCtx,0);` |
|        5 |  9550 | `		return PH7_OK;` |
|        - |  9551 | `	}` |
|        - |  9552 | `	/* Copy the old callback */` |
|      ! 0 |  9553 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  9554 | `	PH7_MemObjRelease(pOld);` |
|        - |  9555 | `	/* Return TRUE */` |
|      ! 0 |  9556 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  9557 | `	return PH7_OK;` |
|        3 |  9558 |  |
|        - |  9559 | `/*` |
|        - |  9560 | ` * value set_error_handler(callable $error_handler)` |
|        - |  9561 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  9562 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  9563 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  9564 | ` *  Sets a user-defined error handler function.` |
|        - |  9565 | ` *  This function can be used for defining your own way of handling errors during` |
|        - |  9566 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - |  9567 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - |  9568 | ` *  conditions (using trigger_error()).` |
|        - |  9569 | ` * Parameters` |
|        - |  9570 | ` *  $error_handler` |
|        - |  9571 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - |  9572 | ` *   describing the error.` |
|        - |  9573 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - |  9574 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - |  9575 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - |  9576 | ` *   The function can be shown as:` |
|        - |  9577 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - |  9578 | ` *     errno` |
|        - |  9579 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - |  9580 | ` *   errstr` |
|        - |  9581 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - |  9582 | ` *   errfile` |
|        - |  9583 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - |  9584 | ` *     was raised in, as a string.` |
|        - |  9585 | ` *  Note:` |
|        - |  9586 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  9587 | ` * Return` |
|        - |  9588 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - |  9589 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  9590 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  9591 | ` */` |
|     5254 |  9592 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9593 |  |
|     5256 |  9594 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9595 | `	ph7_value *pOld,*pNew;` |
|        - |  9596 | `	/* Point to the old and the new handler */` |
|     5256 |  9597 | `	pOld = &pVm->aErrCB[0];` |
|     5256 |  9598 | `	pNew = &pVm->aErrCB[1];` |
|        - |  9599 | `	/* Return the old handler */` |
|     5256 |  9600 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     5256 |  9601 | `	if( nArg > 0 ){` |
|     5256 |  9602 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  9603 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     2627 |  9604 | `			PH7_MemObjRelease(pNew);` |
|     2627 |  9605 | `			ph7_result_bool(pCtx,1);` |
|     1314 |  9606 | `		}else{` |
|     2630 |  9607 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  9608 | `			/* Install the new handler */` |
|     2630 |  9609 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  9610 | `		}` |
|     2627 |  9611 | `	}` |
|     5256 |  9612 | `	return PH7_OK;` |
|        2 |  9613 |  |
|        - |  9614 | `/*` |
|        - |  9615 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - |  9616 | ` *  Generates a backtrace.` |
|        - |  9617 | ` * Paramaeter` |
|        - |  9618 | ` *  $options` |
|        - |  9619 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - |  9620 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - |  9621 | ` *   all the function/method arguments, to save memory.` |
|        - |  9622 | ` * $limit` |
|        - |  9623 | ` *   (Not Used)` |
|        - |  9624 | ` * Return` |
|        - |  9625 | ` *  An array.The possible returned elements are as follows:` |
|        - |  9626 | ` *          Possible returned elements from debug_backtrace()` |
|        - |  9627 | ` *          Name        Type      Description` |
|        - |  9628 | ` *          ------      ------     -----------` |
|        - |  9629 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - |  9630 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - |  9631 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - |  9632 | ` *          class       string    The current class name. See also __CLASS__` |
|        - |  9633 | ` *          object      object    The current object.` |
|        - |  9634 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - |  9635 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - |  9636 | ` */` |
|       32 |  9637 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9638 |  |
|       34 |  9639 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9640 | `	ph7_value *pArray;` |
|        - |  9641 | `	ph7_class *pClass;` |
|        - |  9642 | `	ph7_value *pValue;` |
|        - |  9643 | `	SyString *pFile;` |
|        - |  9644 | `	/* Create a new array */` |
|       34 |  9645 | `	pArray = ph7_context_new_array(pCtx);` |
|       34 |  9646 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       34 |  9647 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  9648 | `		/* Out of memory,return NULL */` |
|      ! 0 |  9649 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 |  9650 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9651 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9652 | `		SXUNUSED(apArg);` |
|      ! 0 |  9653 | `		return PH7_OK;` |
|        - |  9654 | `	}` |
|        - |  9655 | `	/* Dump running function name and it's arguments  */` |
|       34 |  9656 | `	if( pVm->pFrame->pParent ){` |
|       34 |  9657 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  9658 | `		ph7_vm_func *pFunc;` |
|        - |  9659 | `		ph7_value *pArg;` |
|       34 |  9660 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  9661 | `			/* Safely ignore the exception frame */` |
|      ! 0 |  9662 | `			pFrame = pFrame->pParent;` |
|      ! 0 |  9663 | `		}` |
|       34 |  9664 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       34 |  9665 | `		if( pFrame->pParent && pFunc ){` |
|       34 |  9666 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|       34 |  9667 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|       34 |  9668 | `			ph7_value_reset_string_cursor(pValue);` |
|       16 |  9669 | `		}` |
|        - |  9670 | `		/* Function arguments */` |
|       34 |  9671 | `		pArg = ph7_context_new_array(pCtx);` |
|       34 |  9672 | `		if( pArg  ){` |
|        - |  9673 | `			ph7_value *pObj;` |
|        - |  9674 | `			VmSlot *aSlot;` |
|        - |  9675 | `			sxu32 n;` |
|        - |  9676 | `			/* Start filling the array with the given arguments */` |
|       34 |  9677 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      122 |  9678 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|       90 |  9679 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|       90 |  9680 | `				if( pObj ){` |
|       90 |  9681 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|       44 |  9682 | `				}` |
|       46 |  9683 | `			}` |
|        - |  9684 | `			/* Save the array */` |
|       34 |  9685 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|       16 |  9686 | `		}` |
|       16 |  9687 | `	}` |
|       34 |  9688 | `	ph7_value_int(pValue,1);` |
|        - |  9689 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - |  9690 | `	 * line numbers at run-time. )` |
|        - |  9691 | `	 */` |
|       34 |  9692 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - |  9693 | `	/* Current processed script */` |
|       34 |  9694 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       34 |  9695 | `	if( pFile ){` |
|       34 |  9696 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|       34 |  9697 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|       34 |  9698 | `		ph7_value_reset_string_cursor(pValue);` |
|       16 |  9699 | `	}` |
|        - |  9700 | `	/* Top class */` |
|       34 |  9701 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|       34 |  9702 | `	if( pClass ){` |
|       30 |  9703 | `		ph7_value_reset_string_cursor(pValue);` |
|       30 |  9704 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       30 |  9705 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|       14 |  9706 | `	}` |
|        - |  9707 | `	/* Return the freshly created array */` |
|       34 |  9708 | `	ph7_result_value(pCtx,pArray);` |
|        - |  9709 | `	/*` |
|        - |  9710 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - |  9711 | `	 * as soon we return from this function.` |
|        - |  9712 | `	 */` |
|       34 |  9713 | `	return PH7_OK;` |
|       18 |  9714 |  |
|        - |  9715 | `/*` |
|        - |  9716 | ` * Generate a small backtrace.` |
|        - |  9717 | ` * Store the generated dump in the given BLOB` |
|        - |  9718 | ` */` |
|        4 |  9719 | `static int VmMiniBacktrace(` |
|        - |  9720 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  9721 | `	SyBlob *pOut /* Store Dump here */` |
|        - |  9722 | `	)` |
|        1 |  9723 |  |
|        5 |  9724 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  9725 | `	ph7_vm_func *pFunc;` |
|        - |  9726 | `	ph7_class *pClass;` |
|        - |  9727 | `	SyString *pFile;` |
|        - |  9728 | `	/* Called function */` |
|        5 |  9729 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  9730 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  9731 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  9732 | `	}` |
|        5 |  9733 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 |  9734 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  9735 | `	if( pFrame->pParent && pFunc ){` |
|        5 |  9736 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 |  9737 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 |  9738 | `	}else{` |
|      ! 0 |  9739 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - |  9740 | `	}` |
|        5 |  9741 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - |  9742 | `	/* Current processed script */` |
|        5 |  9743 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 |  9744 | `	if( pFile ){` |
|        5 |  9745 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  9746 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 |  9747 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 |  9748 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 |  9749 | `	}` |
|        - |  9750 | `	/* Top class */` |
|        5 |  9751 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 |  9752 | `	if( pClass ){` |
|      ! 0 |  9753 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 |  9754 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 |  9755 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 |  9756 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 |  9757 | `	}` |
|        5 |  9758 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - |  9759 | `	/* All done */` |
|        5 |  9760 | `	return SXRET_OK;` |
|        1 |  9761 |  |
|        - |  9762 | `/*` |
|        - |  9763 | ` * void debug_print_backtrace()` |
|        - |  9764 | ` *  Prints a backtrace` |
|        - |  9765 | ` * Parameters` |
|        - |  9766 | ` * None` |
|        - |  9767 | ` * Return` |
|        - |  9768 | ` * NULL` |
|        - |  9769 | ` */` |
|        2 |  9770 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9771 |  |
|        3 |  9772 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9773 | `	SyBlob sDump;` |
|        3 |  9774 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  9775 | `	/* Generate the backtrace */` |
|        3 |  9776 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  9777 | `	/* Output backtrace */` |
|        3 |  9778 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9779 | `	/* All done,cleanup */` |
|        3 |  9780 | `	SyBlobRelease(&sDump);` |
|        1 |  9781 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  9782 | `	SXUNUSED(apArg);` |
|        3 |  9783 | `	return PH7_OK;` |
|        1 |  9784 |  |
|        - |  9785 | `/*` |
|        - |  9786 | ` * string debug_string_backtrace()` |
|        - |  9787 | ` *  Generate a backtrace` |
|        - |  9788 | ` * Parameters` |
|        - |  9789 | ` * None` |
|        - |  9790 | ` * Return` |
|        - |  9791 | ` *  A mini backtrace().` |
|        - |  9792 | ` * Note that this is a symisc extension.` |
|        - |  9793 | ` */` |
|        2 |  9794 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9795 |  |
|        3 |  9796 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9797 | `	SyBlob sDump;` |
|        3 |  9798 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  9799 | `	/* Generate the backtrace */` |
|        3 |  9800 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  9801 | `	/* Return the backtrace */` |
|        3 |  9802 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - |  9803 | `	/* All done,cleanup */` |
|        3 |  9804 | `	SyBlobRelease(&sDump);` |
|        1 |  9805 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  9806 | `	SXUNUSED(apArg);` |
|        3 |  9807 | `	return PH7_OK;` |
|        1 |  9808 |  |
|        - |  9809 | `/*` |
|        - |  9810 | ` * The following routine is invoked by the engine when an uncaught` |
|        - |  9811 | ` * exception is triggered.` |
|        - |  9812 | ` */` |
|       16 |  9813 | `static sxi32 VmUncaughtException(` |
|        - |  9814 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  9815 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  9816 | `	)` |
|        2 |  9817 |  |
|        - |  9818 | `	ph7_value *apArg[2],sArg;` |
|       18 |  9819 | `	int nArg = 1;` |
|        - |  9820 | `	sxi32 rc;` |
|       18 |  9821 | `	if( pVm->nExceptDepth > 15 ){` |
|        - |  9822 | `		/* Nesting limit reached */` |
|      ! 0 |  9823 | `		return SXRET_OK;` |
|        - |  9824 | `	}` |
|        - |  9825 | `	/* Call any exception handler if available */` |
|       18 |  9826 | `	PH7_MemObjInit(pVm,&sArg);` |
|       18 |  9827 | `	if( pThis ){` |
|        - |  9828 | `		/* Load the exception instance */` |
|       18 |  9829 | `		sArg.x.pOther = pThis;` |
|       18 |  9830 | `		pThis->iRef++;` |
|       18 |  9831 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|       10 |  9832 | `	}else{` |
|      ! 0 |  9833 | `		nArg = 0;` |
|        - |  9834 | `	}` |
|       18 |  9835 | `	apArg[0] = &sArg;` |
|        - |  9836 | `	/* Call the exception handler if available */` |
|       18 |  9837 | `	pVm->nExceptDepth++;` |
|       18 |  9838 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|       18 |  9839 | `	pVm->nExceptDepth--;` |
|       18 |  9840 | `	if( rc != SXRET_OK ){` |
|        - |  9841 | `		SyBlob sMsgBuf;` |
|       15 |  9842 | `		const char *zClass = "Exception";` |
|       15 |  9843 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - |  9844 | `		const char *zMsg;` |
|        - |  9845 | `		sxu32 nMsg;` |
|        - |  9846 | `		const char *zFuncName;` |
|        - |  9847 | `		int nFuncLen;` |
|       15 |  9848 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|       15 |  9849 | `		if( pThis ){` |
|        - |  9850 | `			ph7_class_method *pGetMessage;` |
|        - |  9851 | `			ph7_value sMsg;` |
|        - |  9852 | `			const char *zTmp;` |
|        - |  9853 | `			int nTmp;` |
|       15 |  9854 | `			zClass = pThis->pClass->sName.zString;` |
|       15 |  9855 | `			nClass = pThis->pClass->sName.nByte;` |
|       15 |  9856 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|       15 |  9857 | `			if( pGetMessage ){` |
|       15 |  9858 | `				PH7_MemObjInit(pVm,&sMsg);` |
|       15 |  9859 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|       15 |  9860 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|       15 |  9861 | `					if( zTmp && nTmp > 0 ){` |
|       15 |  9862 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|        7 |  9863 | `					}` |
|        7 |  9864 | `				}` |
|       15 |  9865 | `				PH7_MemObjRelease(&sMsg);` |
|        7 |  9866 | `			}` |
|        7 |  9867 | `		}` |
|       15 |  9868 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 |  9869 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 |  9870 | `		}` |
|       15 |  9871 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|       15 |  9872 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|       15 |  9873 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|       15 |  9874 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|       15 |  9875 | `		SyBlobRelease(&sMsgBuf);` |
|        - |  9876 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|       15 |  9877 | `		rc = SXERR_ABORT;` |
|        7 |  9878 | `	}` |
|       18 |  9879 | `	PH7_MemObjRelease(&sArg);` |
|       18 |  9880 | `	return rc;` |
|       10 |  9881 |  |
|        - |  9882 | `/*` |
|        - |  9883 | ` * Throw an user exception.` |
|        - |  9884 | ` */` |
|       30 |  9885 | `static sxi32 VmThrowException(` |
|        - |  9886 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |  9887 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  9888 | `	)` |
|        2 |  9889 |  |
|        - |  9890 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - |  9891 | `	ph7_exception **apException;` |
|        - |  9892 | `	ph7_exception *pException;` |
|        - |  9893 | `	/* Point to the stack of loaded exceptions */` |
|       32 |  9894 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       32 |  9895 | `	pException = 0;` |
|       32 |  9896 | `	pCatch = 0;` |
|       32 |  9897 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  9898 | `		ph7_exception_block *aCatch;` |
|        - |  9899 | `		ph7_class *pClass;` |
|        - |  9900 | `		sxu32 j;` |
|        - |  9901 | `		/* Locate the appropriate block to execute */` |
|       16 |  9902 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       16 |  9903 | `		(void)SySetPop(&pVm->aException);` |
|       16 |  9904 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       16 |  9905 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       16 |  9906 | `			SyString *pName = &aCatch[j].sClass;` |
|        - |  9907 | `			/* Extract the target class */` |
|       16 |  9908 | `			pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       16 |  9909 | `			if( pClass == 0 ){` |
|        - |  9910 | `				/* No such class */` |
|      ! 0 |  9911 | `				continue;` |
|        - |  9912 | `			}` |
|       16 |  9913 | `			if( VmInstanceOf(pThis->pClass,pClass) ){` |
|        - |  9914 | `				/* Catch block found,break immeditaley */` |
|       16 |  9915 | `				pCatch = &aCatch[j];` |
|       16 |  9916 | `				break;` |
|        - |  9917 | `			}` |
|      ! 0 |  9918 | `		}` |
|        7 |  9919 | `	}` |
|        - |  9920 | `	/* Execute the cached block if available */` |
|       32 |  9921 | `	if( pCatch == 0 ){` |
|        - |  9922 | `		sxi32 rc;` |
|       18 |  9923 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|       18 |  9924 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 |  9925 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 |  9926 | `			while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  9927 | `				/* Safely ignore the exception frame */` |
|      ! 0 |  9928 | `				pFrame = pFrame->pParent;` |
|      ! 0 |  9929 | `			}` |
|      ! 0 |  9930 | `			if( pException->pFrame == pFrame ){` |
|        - |  9931 | `				/* Tell the upper layer that the exception was caught */` |
|      ! 0 |  9932 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 |  9933 | `			}` |
|      ! 0 |  9934 | `		}` |
|       18 |  9935 | `		return rc;` |
|      ! 0 |  9936 | `	}else{` |
|       16 |  9937 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  9938 | `		sxi32 rc;` |
|       24 |  9939 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  9940 | `			/* Safely ignore the exception frame */` |
|       10 |  9941 | `			pFrame = pFrame->pParent;` |
|        2 |  9942 | `		}` |
|       16 |  9943 | `		if( pException->pFrame == pFrame ){` |
|        - |  9944 | `			/* Tell the upper layer that the exception was caught */` |
|        8 |  9945 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|        3 |  9946 | `		}` |
|        - |  9947 | `		/* Create a private frame first */` |
|       16 |  9948 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       16 |  9949 | `		if( rc == SXRET_OK ){` |
|        - |  9950 | `			/* Mark as catch frame */` |
|       16 |  9951 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       16 |  9952 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       16 |  9953 | `			if( pObj ){` |
|        - |  9954 | `				/* Install the exception instance */` |
|       16 |  9955 | `				pThis->iRef++; /* Increment reference count */` |
|       16 |  9956 | `				pObj->x.pOther = pThis;` |
|       16 |  9957 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|        7 |  9958 | `			}` |
|        - |  9959 | `			/* Exceute the block */` |
|       16 |  9960 | `			VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - |  9961 | `			/* Leave the frame */` |
|       16 |  9962 | `			VmLeaveFrame(&(*pVm));` |
|        7 |  9963 | `		}` |
|        - |  9964 | `	}` |
|        - |  9965 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - |  9966 | `	 * be used again if a 'goto' statement is executed.` |
|        - |  9967 | `	 */` |
|       16 |  9968 | `	return SXRET_OK;` |
|       17 |  9969 |  |
|        - |  9970 | `/*` |
|        - |  9971 | ` * Section:` |
|        - |  9972 | ` *  Version,Credits and Copyright related functions.` |
|        - |  9973 | ` * Status:` |
|        - |  9974 | ` *    Stable.` |
|        - |  9975 | ` */` |
|        - |  9976 | `/*` |
|        - |  9977 | ` * string ph7version(void)` |
|        - |  9978 | ` *  Returns the running version of the PH7 version.` |
|        - |  9979 | ` * Parameters` |
|        - |  9980 | ` *  None` |
|        - |  9981 | ` * Return` |
|        - |  9982 | ` * Current PH7 version.` |
|        - |  9983 | ` */` |
|        2 |  9984 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9985 |  |
|        1 |  9986 | `	SXUNUSED(nArg);` |
|        1 |  9987 | `	SXUNUSED(apArg); /* cc warning */` |
|        - |  9988 | `	/* Current engine version */` |
|        3 |  9989 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 |  9990 | `	return PH7_OK;` |
|        1 |  9991 |  |
|        - |  9992 | `/*` |
|        - |  9993 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - |  9994 | ` */` |
|        - |  9995 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - |  9996 | ` "<html><head>"\` |
|        - |  9997 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - |  9998 | ` "<style type=\"text/css\">"\` |
|        - |  9999 | ` "div {"\` |
|        - | 10000 | `     "border: 1px solid #cccccc;"\` |
|        - | 10001 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 10002 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 10003 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 10004 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 10005 | `     "-webkit-border-radius: 10px;"\` |
|        - | 10006 | `     "-o-border-radius: 10px;"\` |
|        - | 10007 | `     "border-radius: 10px;"\` |
|        - | 10008 | `     "padding-left: 2em;"\` |
|        - | 10009 | `     "background-color: white;"\` |
|        - | 10010 | `     "margin-left: auto;"\` |
|        - | 10011 | `     "font-family: verdana;"\` |
|        - | 10012 | `     "padding-right: 2em;"\` |
|        - | 10013 | `     "margin-right: auto;"\` |
|        - | 10014 | `     "}"\` |
|        - | 10015 | `     "body {"\` |
|        - | 10016 | `     "padding: 0.2em;"\` |
|        - | 10017 | `     "font-style: normal;"\` |
|        - | 10018 | `     "font-size: medium;"\` |
|        - | 10019 | `     "background-color: #f2f2f2;"\` |
|        - | 10020 | `     "}"\` |
|        - | 10021 | `     "hr {"\` |
|        - | 10022 | `     "border-style: solid none none;"\` |
|        - | 10023 | `     "border-width: 1px medium medium;"\` |
|        - | 10024 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 10025 | `     "height: 1px;"\` |
|        - | 10026 | `     "}"\` |
|        - | 10027 | `     "a {"\` |
|        - | 10028 | `     "color: #3366cc;"\` |
|        - | 10029 | `     "text-decoration: none;"\` |
|        - | 10030 | `     "}"\` |
|        - | 10031 | `     "a:hover {"\` |
|        - | 10032 | `     "color: #999999;"\` |
|        - | 10033 | `     "}"\` |
|        - | 10034 | `     "a:active {"\` |
|        - | 10035 | `     "color: #663399;"\` |
|        - | 10036 | `     "}"\` |
|        - | 10037 | `     "h1 {"\` |
|        - | 10038 | `     "margin: 0;"\` |
|        - | 10039 | `     "padding: 0;"\` |
|        - | 10040 | `     "font-family: Verdana;"\` |
|        - | 10041 | `     "font-weight: bold;"\` |
|        - | 10042 | `     "font-style: normal;"\` |
|        - | 10043 | `     "font-size: medium;"\` |
|        - | 10044 | `     "text-transform: capitalize;"\` |
|        - | 10045 | `     "color: #0a328c;"\` |
|        - | 10046 | `     "}"\` |
|        - | 10047 | `     "p {"\` |
|        - | 10048 | `     "margin: 0 auto;"\` |
|        - | 10049 | `     "font-size: medium;"\` |
|        - | 10050 | `     "font-style: normal;"\` |
|        - | 10051 | `     "font-family: verdana;"\` |
|        - | 10052 | `     "}"\` |
|        - | 10053 | `"</style></head><body>"\` |
|        - | 10054 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 10055 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 10056 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 10057 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 10058 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 10059 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 10060 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 10061 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 10062 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 10063 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 10064 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 10065 |  |
|        - | 10066 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10067 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 10068 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 10069 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 10070 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10071 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 10072 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10073 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 10074 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10075 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 10076 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10077 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 10078 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 10079 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 10080 |  |
|        - | 10081 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 10082 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 10083 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 10084 | `"&nbsp;*<br>"\` |
|        - | 10085 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 10086 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 10087 | `"&nbsp;* are met:<br>"\` |
|        - | 10088 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 10089 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 10090 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 10091 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 10092 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 10093 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 10094 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 10095 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 10096 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 10097 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 10098 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 10099 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 10100 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 10101 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 10102 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 10103 | `"&nbsp;*<br>"\` |
|        - | 10104 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 10105 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 10106 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 10107 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 10108 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 10109 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 10110 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 10111 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 10112 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 10113 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 10114 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 10115 | `"&nbsp;*/<br>"\` |
|        - | 10116 | `"</span></small></small></p>"\` |
|        - | 10117 | `"</div></body></html>"` |
|        - | 10118 | `/*` |
|        - | 10119 | ` * bool ph7credits(void)` |
|        - | 10120 | ` * bool ph7info(void)` |
|        - | 10121 | ` * bool ph7copyright(void)` |
|        - | 10122 | ` *  Prints out the credits for PH7 engine` |
|        - | 10123 | ` * Parameters` |
|        - | 10124 | ` *  None` |
|        - | 10125 | ` * Return` |
|        - | 10126 | ` *  Always TRUE` |
|        - | 10127 | ` */` |
|        2 | 10128 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10129 |  |
|        3 | 10130 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 10131 | `	/* Expand the HTML page above*/` |
|        3 | 10132 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 10133 | `	ph7_context_output_format(` |
|        1 | 10134 | `		pCtx,` |
|        - | 10135 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 10136 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 10137 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 10138 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 10139 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 10140 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 10141 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 10142 | `#ifdef __WINNT__` |
|        - | 10143 | `		"Windows NT"` |
|        - | 10144 | `#elif defined(__UNIXES__)` |
|        - | 10145 | `		"UNIX-Like"` |
|        - | 10146 | `#else` |
|        - | 10147 | `		"Other OS"` |
|        - | 10148 | `#endif` |
|        - | 10149 | `		);` |
|        3 | 10150 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 10151 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10152 | `	SXUNUSED(apArg);` |
|        - | 10153 | `	/* Return TRUE */` |
|        - | 10154 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 10155 | `	return PH7_OK;` |
|        1 | 10156 |  |
|        - | 10157 | `/*` |
|        - | 10158 | ` * Section:` |
|        - | 10159 | ` *    URL related routines.` |
|        - | 10160 | ` * Status:` |
|        - | 10161 | ` *    Stable.` |
|        - | 10162 | ` */` |
|        - | 10163 | `/* Forward declaration */` |
|        - | 10164 | `static sxi32 VmHttpSplitURI(SyhttpUri *pOut,const char *zUri,sxu32 nLen);` |
|        - | 10165 | `/*` |
|        - | 10166 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 10167 | ` *  Parse a URL and return its fields.` |
|        - | 10168 | ` * Parameters` |
|        - | 10169 | ` *  $url` |
|        - | 10170 | ` *   The URL to parse.` |
|        - | 10171 | ` * $component` |
|        - | 10172 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 10173 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 10174 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 10175 | ` *  in which case the return value will be an integer).` |
|        - | 10176 | ` * Return` |
|        - | 10177 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 10178 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 10179 | ` *  this array are:` |
|        - | 10180 | ` *   scheme - e.g. http` |
|        - | 10181 | ` *   host` |
|        - | 10182 | ` *   port` |
|        - | 10183 | ` *   user` |
|        - | 10184 | ` *   pass` |
|        - | 10185 | ` *   path` |
|        - | 10186 | ` *   query - after the question mark ?` |
|        - | 10187 | ` *   fragment - after the hashmark #` |
|        - | 10188 | ` * Note:` |
|        - | 10189 | ` *  FALSE is returned on failure.` |
|        - | 10190 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 10191 | ` *  with the standard PHP engine.` |
|        - | 10192 | ` */` |
|       28 | 10193 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10194 |  |
|        - | 10195 | `	const char *zStr; /* Input string */` |
|        - | 10196 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 10197 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 10198 | `	int nLen;` |
|        - | 10199 | `	sxi32 rc;` |
|       29 | 10200 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 10201 | `		/* Missing/Invalid arguments,return FALSE */` |
|        3 | 10202 | `		ph7_result_bool(pCtx,0);` |
|        3 | 10203 | `		return PH7_OK;` |
|        - | 10204 | `	}` |
|        - | 10205 | `	/* Extract the given URI */` |
|       27 | 10206 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       27 | 10207 | `	if( nLen < 1 ){` |
|        - | 10208 | `		/* Nothing to process,return FALSE */` |
|      ! 0 | 10209 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10210 | `		return PH7_OK;` |
|        - | 10211 | `	}` |
|        - | 10212 | `	/* Get a parse */` |
|       27 | 10213 | `	rc = VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 10214 | `	if( rc != SXRET_OK ){` |
|        - | 10215 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 10216 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10217 | `		return PH7_OK;` |
|        - | 10218 | `	}` |
|       27 | 10219 | `	if( nArg > 1 ){` |
|      ! 0 | 10220 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 10221 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 10222 | `		switch(nComponent){` |
|      ! 0 | 10223 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 10224 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 10225 | `			if( pComp->nByte < 1 ){` |
|        - | 10226 | `				/* No available value,return NULL */` |
|      ! 0 | 10227 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10228 | `			}else{` |
|      ! 0 | 10229 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10230 | `			}` |
|      ! 0 | 10231 | `			break;` |
|      ! 0 | 10232 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 10233 | `			pComp = &sURI.sHost;` |
|      ! 0 | 10234 | `			if( pComp->nByte < 1 ){` |
|        - | 10235 | `				/* No available value,return NULL */` |
|      ! 0 | 10236 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10237 | `			}else{` |
|      ! 0 | 10238 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10239 | `			}` |
|      ! 0 | 10240 | `			break;` |
|      ! 0 | 10241 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 10242 | `			pComp = &sURI.sPort;` |
|      ! 0 | 10243 | `			if( pComp->nByte < 1 ){` |
|        - | 10244 | `				/* No available value,return NULL */` |
|      ! 0 | 10245 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10246 | `			}else{` |
|      ! 0 | 10247 | `				int iPort = 0;` |
|        - | 10248 | `				/* Cast the value to integer */` |
|      ! 0 | 10249 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 10250 | `				ph7_result_int(pCtx,iPort);` |
|        - | 10251 | `			}` |
|      ! 0 | 10252 | `			break;` |
|      ! 0 | 10253 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 10254 | `			pComp = &sURI.sUser;` |
|      ! 0 | 10255 | `			if( pComp->nByte < 1 ){` |
|        - | 10256 | `				/* No available value,return NULL */` |
|      ! 0 | 10257 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10258 | `			}else{` |
|      ! 0 | 10259 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10260 | `			}` |
|      ! 0 | 10261 | `			break;` |
|      ! 0 | 10262 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 10263 | `			pComp = &sURI.sPass;` |
|      ! 0 | 10264 | `			if( pComp->nByte < 1 ){` |
|        - | 10265 | `				/* No available value,return NULL */` |
|      ! 0 | 10266 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10267 | `			}else{` |
|      ! 0 | 10268 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10269 | `			}` |
|      ! 0 | 10270 | `			break;` |
|      ! 0 | 10271 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 10272 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 10273 | `			if( pComp->nByte < 1 ){` |
|        - | 10274 | `				/* No available value,return NULL */` |
|      ! 0 | 10275 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10276 | `			}else{` |
|      ! 0 | 10277 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10278 | `			}` |
|      ! 0 | 10279 | `			break;` |
|      ! 0 | 10280 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 10281 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 10282 | `			if( pComp->nByte < 1 ){` |
|        - | 10283 | `				/* No available value,return NULL */` |
|      ! 0 | 10284 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10285 | `			}else{` |
|      ! 0 | 10286 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10287 | `			}` |
|      ! 0 | 10288 | `			break;` |
|      ! 0 | 10289 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 10290 | `			pComp = &sURI.sPath;` |
|      ! 0 | 10291 | `			if( pComp->nByte < 1 ){` |
|        - | 10292 | `				/* No available value,return NULL */` |
|      ! 0 | 10293 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10294 | `			}else{` |
|      ! 0 | 10295 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10296 | `			}` |
|      ! 0 | 10297 | `			break;` |
|      ! 0 | 10298 | `		default:` |
|        - | 10299 | `			/* No such entry,return NULL */` |
|      ! 0 | 10300 | `			ph7_result_null(pCtx);` |
|      ! 0 | 10301 | `			break;` |
|        - | 10302 | `		}` |
|      ! 0 | 10303 | `	}else{` |
|        - | 10304 | `		ph7_value *pArray,*pValue;` |
|        - | 10305 | `		/* Return an associative array */` |
|       27 | 10306 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 10307 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 10308 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 10309 | `			/* Out of memory */` |
|      ! 0 | 10310 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 10311 | `			/* Return false */` |
|      ! 0 | 10312 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 10313 | `			return PH7_OK;` |
|        - | 10314 | `		}` |
|        - | 10315 | `		/* Fill the array */` |
|       27 | 10316 | `		pComp = &sURI.sScheme;` |
|       27 | 10317 | `		if( pComp->nByte > 0 ){` |
|       19 | 10318 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 10319 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 10320 | `		}` |
|        - | 10321 | `		/* Reset the string cursor */` |
|       27 | 10322 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10323 | `		pComp = &sURI.sHost;` |
|       27 | 10324 | `		if( pComp->nByte > 0 ){` |
|       25 | 10325 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 10326 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 10327 | `		}` |
|        - | 10328 | `		/* Reset the string cursor */` |
|       27 | 10329 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10330 | `		pComp = &sURI.sPort;` |
|       27 | 10331 | `		if( pComp->nByte > 0 ){` |
|       11 | 10332 | `			int iPort = 0;/* cc warning */` |
|        - | 10333 | `			/* Convert to integer */` |
|       11 | 10334 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 10335 | `			ph7_value_int(pValue,iPort);` |
|       11 | 10336 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 10337 | `		}` |
|        - | 10338 | `		/* Reset the string cursor */` |
|       27 | 10339 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10340 | `		pComp = &sURI.sUser;` |
|       27 | 10341 | `		if( pComp->nByte > 0 ){` |
|        7 | 10342 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 10343 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 10344 | `		}` |
|        - | 10345 | `		/* Reset the string cursor */` |
|       27 | 10346 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10347 | `		pComp = &sURI.sPass;` |
|       27 | 10348 | `		if( pComp->nByte > 0 ){` |
|        7 | 10349 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 10350 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 10351 | `		}` |
|        - | 10352 | `		/* Reset the string cursor */` |
|       27 | 10353 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10354 | `		pComp = &sURI.sPath;` |
|       27 | 10355 | `		if( pComp->nByte > 0 ){` |
|       17 | 10356 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 10357 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 10358 | `		}` |
|        - | 10359 | `		/* Reset the string cursor */` |
|       27 | 10360 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10361 | `		pComp = &sURI.sQuery;` |
|       27 | 10362 | `		if( pComp->nByte > 0 ){` |
|        5 | 10363 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 10364 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 10365 | `		}` |
|        - | 10366 | `		/* Reset the string cursor */` |
|       27 | 10367 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10368 | `		pComp = &sURI.sFragment;` |
|       27 | 10369 | `		if( pComp->nByte > 0 ){` |
|        5 | 10370 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 10371 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 10372 | `		}` |
|        - | 10373 | `		/* Return the created array */` |
|       27 | 10374 | `		ph7_result_value(pCtx,pArray);` |
|        - | 10375 | `		/* NOTE:` |
|        - | 10376 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 10377 | `		 * automatically as soon we return from this function.` |
|        - | 10378 | `		 */` |
|        - | 10379 | `	}` |
|        - | 10380 | `	/* All done */` |
|       27 | 10381 | `	return PH7_OK;` |
|       15 | 10382 |  |
|        - | 10383 | `/*` |
|        - | 10384 | ` * Section:` |
|        - | 10385 | ` *   Array related routines.` |
|        - | 10386 | ` * Status:` |
|        - | 10387 | ` *    Stable.` |
|        - | 10388 | ` * Note 2012-5-21 01:04:15:` |
|        - | 10389 | ` *  Array related functions that need access to the underlying` |
|        - | 10390 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 10391 | ` */` |
|        - | 10392 | `/*` |
|        - | 10393 | ` * The [compact()] function store it's state information in an instance` |
|        - | 10394 | ` * of the following structure.` |
|        - | 10395 | ` */` |
|        - | 10396 | `struct compact_data` |
|        - | 10397 |  |
|        - | 10398 | `	ph7_value *pArray;  /* Target array */` |
|        - | 10399 | `	int nRecCount;      /* Recursion count */` |
|        - | 10400 | `};` |
|        - | 10401 | `/*` |
|        - | 10402 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 10403 | ` */` |
|      ! 0 | 10404 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 10405 |  |
|      ! 0 | 10406 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 10407 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 10408 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 10409 | `	/* Act according to the hashmap value */` |
|      ! 0 | 10410 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 10411 | `		SyString sVar;` |
|      ! 0 | 10412 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 10413 | `		if( sVar.nByte > 0 ){` |
|        - | 10414 | `			/* Query the current frame */` |
|      ! 0 | 10415 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 10416 | `			/* ^` |
|        - | 10417 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 10418 | `			 */` |
|      ! 0 | 10419 | `			if( pKey ){` |
|        - | 10420 | `				/* Perform the insertion */` |
|      ! 0 | 10421 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 10422 | `			}` |
|      ! 0 | 10423 | `		}` |
|      ! 0 | 10424 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 10425 | `		int rc;` |
|        - | 10426 | `		/* Recursively traverse this array */` |
|      ! 0 | 10427 | `		pData->nRecCount++;` |
|      ! 0 | 10428 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 10429 | `		pData->nRecCount--;` |
|      ! 0 | 10430 | `		return rc;` |
|        - | 10431 | `	}` |
|      ! 0 | 10432 | `	return SXRET_OK;` |
|      ! 0 | 10433 |  |
|        - | 10434 | `/*` |
|        - | 10435 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 10436 | ` *  Create array containing variables and their values.` |
|        - | 10437 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 10438 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 10439 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 10440 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 10441 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 10442 | ` * Parameters` |
|        - | 10443 | ` *  $varname` |
|        - | 10444 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 10445 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 10446 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 10447 | ` *   it recursively.` |
|        - | 10448 | ` * Return` |
|        - | 10449 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 10450 | ` */` |
|        2 | 10451 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10452 |  |
|        - | 10453 | `	ph7_value *pArray,*pObj;` |
|        3 | 10454 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10455 | `	const char *zName;` |
|        - | 10456 | `	SyString sVar;` |
|        - | 10457 | `	int i,nLen;` |
|        3 | 10458 | `	if( nArg < 1 ){` |
|        - | 10459 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 10460 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10461 | `		return PH7_OK;` |
|        - | 10462 | `	}` |
|        - | 10463 | `	/* Create the array */` |
|        3 | 10464 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 10465 | `	if( pArray == 0 ){` |
|        - | 10466 | `		/* Out of memory */` |
|      ! 0 | 10467 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 10468 | `		/* Return NULL */` |
|      ! 0 | 10469 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10470 | `		return PH7_OK;` |
|        - | 10471 | `	}` |
|        - | 10472 | `	/* Perform the requested operation */` |
|        7 | 10473 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 10474 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 10475 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 10476 | `				struct compact_data sData;` |
|      ! 0 | 10477 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 10478 | `				/* Recursively walk the array */` |
|      ! 0 | 10479 | `				sData.nRecCount = 0;` |
|      ! 0 | 10480 | `				sData.pArray = pArray;` |
|      ! 0 | 10481 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 10482 | `			}` |
|      ! 0 | 10483 | `		}else{` |
|        - | 10484 | `			/* Extract variable name */` |
|        5 | 10485 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 10486 | `			if( nLen > 0 ){` |
|        5 | 10487 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 10488 | `				/* Check if the variable is available in the current frame */` |
|        5 | 10489 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 10490 | `				if( pObj ){` |
|        5 | 10491 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 10492 | `				}` |
|        2 | 10493 | `			}` |
|        - | 10494 | `		}` |
|        3 | 10495 | `	}` |
|        - | 10496 | `	/* Return the array */` |
|        3 | 10497 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 10498 | `	return PH7_OK;` |
|        2 | 10499 |  |
|        - | 10500 | `/*` |
|        - | 10501 | ` * The [extract()] function store it's state information in an instance` |
|        - | 10502 | ` * of the following structure.` |
|        - | 10503 | ` */` |
|        - | 10504 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 10505 | `struct extract_aux_data` |
|        - | 10506 |  |
|        - | 10507 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 10508 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 10509 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 10510 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 10511 | `	int iFlags;           /* Control flags */` |
|        - | 10512 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 10513 | `};` |
|        - | 10514 | `/* Forward declaration */` |
|        - | 10515 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 10516 | `/*` |
|        - | 10517 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 10518 | ` *   Import variables into the current symbol table from an array.` |
|        - | 10519 | ` * Parameters` |
|        - | 10520 | ` * $var_array` |
|        - | 10521 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 10522 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 10523 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 10524 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 10525 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 10526 | ` * $extract_type` |
|        - | 10527 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 10528 | ` *  It can be one of the following values:` |
|        - | 10529 | ` *   EXTR_OVERWRITE` |
|        - | 10530 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 10531 | ` *   EXTR_SKIP` |
|        - | 10532 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 10533 | ` *   EXTR_PREFIX_SAME` |
|        - | 10534 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 10535 | ` *   EXTR_PREFIX_ALL` |
|        - | 10536 | ` *       Prefix all variable names with prefix.` |
|        - | 10537 | ` *   EXTR_PREFIX_INVALID` |
|        - | 10538 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 10539 | ` *   EXTR_IF_EXISTS` |
|        - | 10540 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 10541 | ` *       otherwise do nothing.` |
|        - | 10542 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 10543 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 10544 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 10545 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 10546 | ` *      the current symbol table.` |
|        - | 10547 | ` * $prefix` |
|        - | 10548 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 10549 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 10550 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 10551 | ` *  underscore character.` |
|        - | 10552 | ` * Return` |
|        - | 10553 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 10554 | ` */` |
|        4 | 10555 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10556 |  |
|        - | 10557 | `	extract_aux_data sAux;` |
|        - | 10558 | `	ph7_hashmap *pMap;` |
|        5 | 10559 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 10560 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 10561 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 10562 | `		return PH7_OK;` |
|        - | 10563 | `	}` |
|        - | 10564 | `	/* Point to the target hashmap */` |
|        5 | 10565 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 10566 | `	if( pMap->nEntry < 1 ){` |
|        - | 10567 | `		/* Empty map,return  0 */` |
|      ! 0 | 10568 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 10569 | `		return PH7_OK;` |
|        - | 10570 | `	}` |
|        - | 10571 | `	/* Prepare the aux data */` |
|        5 | 10572 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 10573 | `	if( nArg > 1 ){` |
|        3 | 10574 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 10575 | `		if( nArg > 2 ){` |
|      ! 0 | 10576 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 10577 | `		}` |
|        1 | 10578 | `	}` |
|        5 | 10579 | `	sAux.pVm = pCtx->pVm;` |
|        - | 10580 | `	/* Invoke the worker callback */` |
|        5 | 10581 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 10582 | `	/* Number of variables successfully imported */` |
|        5 | 10583 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 10584 | `	return PH7_OK;` |
|        3 | 10585 |  |
|        - | 10586 | `/*` |
|        - | 10587 | ` * Worker callback for the [extract()] function defined` |
|        - | 10588 | ` * below.` |
|        - | 10589 | ` */` |
|        8 | 10590 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 10591 |  |
|        9 | 10592 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 10593 | `	int iFlags = pAux->iFlags;` |
|        9 | 10594 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 10595 | `	ph7_value *pObj;` |
|        - | 10596 | `	SyString sVar;` |
|        9 | 10597 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 10598 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 10599 | `	}` |
|        - | 10600 | `	/* Perform a string cast */` |
|        9 | 10601 | `	PH7_MemObjToString(pKey);` |
|        9 | 10602 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 10603 | `		/* Unavailable variable name */` |
|      ! 0 | 10604 | `		return SXRET_OK;` |
|        - | 10605 | `	}` |
|        9 | 10606 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 10607 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 10608 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 10609 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 10610 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 10611 | `			);` |
|      ! 0 | 10612 | `	}else{` |
|       13 | 10613 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 10614 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 10615 | `	}` |
|        9 | 10616 | `	sVar.zString = pAux->zWorker;` |
|        - | 10617 | `	/* Try to extract the variable */` |
|        9 | 10618 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 10619 | `	if( pObj ){` |
|        - | 10620 | `		/* Collision */` |
|        3 | 10621 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 10622 | `			return SXRET_OK;` |
|        - | 10623 | `		}` |
|        3 | 10624 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 10625 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 10626 | `				/* Already prefixed */` |
|      ! 0 | 10627 | `				return SXRET_OK;` |
|        - | 10628 | `			}` |
|      ! 0 | 10629 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 10630 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 10631 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 10632 | `				);` |
|      ! 0 | 10633 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 10634 | `		}` |
|        2 | 10635 | `	}else{` |
|        - | 10636 | `		/* Create the variable */` |
|        7 | 10637 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 10638 | `	}` |
|        9 | 10639 | `	if( pObj ){` |
|        - | 10640 | `		/* Overwrite the old value */` |
|        9 | 10641 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 10642 | `		/* Increment counter */` |
|        9 | 10643 | `		pAux->iCount++;` |
|        4 | 10644 | `	}` |
|        9 | 10645 | `	return SXRET_OK;` |
|        5 | 10646 |  |
|        - | 10647 | `/*` |
|        - | 10648 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 10649 | ` * defined below.` |
|        - | 10650 | ` */` |
|        2 | 10651 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 10652 |  |
|        3 | 10653 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 10654 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 10655 | `	ph7_value *pObj;` |
|        - | 10656 | `	SyString sVar;` |
|        - | 10657 | `	/* Perform a string cast */` |
|        3 | 10658 | `	PH7_MemObjToString(pKey);` |
|        3 | 10659 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 10660 | `		/* Unavailable variable name */` |
|      ! 0 | 10661 | `		return SXRET_OK;` |
|        - | 10662 | `	}` |
|        3 | 10663 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 10664 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 10665 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 10666 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 10667 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 10668 | `			);` |
|        2 | 10669 | `	}else{` |
|      ! 0 | 10670 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 10671 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 10672 | `	}` |
|        3 | 10673 | `	sVar.zString = pAux->zWorker;` |
|        - | 10674 | `	/* Extract the variable */` |
|        3 | 10675 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 10676 | `	if( pObj ){` |
|        3 | 10677 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 10678 | `	}` |
|        3 | 10679 | `	return SXRET_OK;` |
|        2 | 10680 |  |
|        - | 10681 | `/*` |
|        - | 10682 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 10683 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 10684 | ` * Parameters` |
|        - | 10685 | ` * $types` |
|        - | 10686 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 10687 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 10688 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 10689 | ` *  POST includes the POST uploaded file information.` |
|        - | 10690 | ` *  Note:` |
|        - | 10691 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 10692 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 10693 | ` * $prefix` |
|        - | 10694 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 10695 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 10696 | ` *  variable named $pref_userid.` |
|        - | 10697 | ` * Return` |
|        - | 10698 | ` *  TRUE on success or FALSE on failure.` |
|        - | 10699 | ` */` |
|        2 | 10700 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10701 |  |
|        - | 10702 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 10703 | `	extract_aux_data sAux;` |
|        - | 10704 | `	int nLen,nPrefixLen;` |
|        - | 10705 | `	ph7_value *pSuper;` |
|        - | 10706 | `	ph7_vm *pVm;` |
|        - | 10707 | `	/* By default import only $_GET variables  */` |
|        3 | 10708 | `	zImport = "G";` |
|        3 | 10709 | `	nLen = (int)sizeof(char);` |
|        3 | 10710 | `	zPrefix = 0;` |
|        3 | 10711 | `	nPrefixLen = 0;` |
|        3 | 10712 | `	if( nArg > 0 ){` |
|        3 | 10713 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 10714 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 10715 | `		}` |
|        3 | 10716 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 10717 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 10718 | `		}` |
|        1 | 10719 | `	}` |
|        - | 10720 | `	/* Point to the underlying VM */` |
|        3 | 10721 | `	pVm = pCtx->pVm;` |
|        - | 10722 | `	/* Initialize the aux data */` |
|        3 | 10723 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 10724 | `	sAux.zPrefix = zPrefix;` |
|        3 | 10725 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 10726 | `	sAux.pVm = pVm;` |
|        - | 10727 | `	/* Extract */` |
|        3 | 10728 | `	zEnd = &zImport[nLen];` |
|        5 | 10729 | `	while( zImport < zEnd ){` |
|        3 | 10730 | `		int c = zImport[0];` |
|        3 | 10731 | `		pSuper = 0;` |
|        3 | 10732 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 10733 | `			/* Import $_GET variables */` |
|        3 | 10734 | `			pSuper = VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 10735 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 10736 | `			/* Import $_POST variables */` |
|      ! 0 | 10737 | `			pSuper = VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 10738 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 10739 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 10740 | `			pSuper = VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 10741 | `		}` |
|        3 | 10742 | `		if( pSuper ){` |
|        - | 10743 | `			/* Iterate throw array entries */` |
|        3 | 10744 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 10745 | `		}` |
|        - | 10746 | `		/* Advance the cursor */` |
|        3 | 10747 | `		zImport++;` |
|        1 | 10748 | `	}` |
|        - | 10749 | `	/* All done,return TRUE*/` |
|        3 | 10750 | `	ph7_result_bool(pCtx,0);` |
|        3 | 10751 | `	return PH7_OK;` |
|        1 | 10752 |  |
|        - | 10753 | `/*` |
|        - | 10754 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 10755 | ` * Refer to the eval() language construct implementation for more` |
|        - | 10756 | ` * information.` |
|        - | 10757 | ` */` |
|     7744 | 10758 | `static sxi32 VmEvalChunk(` |
|        - | 10759 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 10760 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 10761 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 10762 | `	int iFlags,         /* Compile flag */` |
|        - | 10763 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 10764 | `	)` |
|        2 | 10765 |  |
|        - | 10766 | `	SySet *pByteCode,aByteCode;` |
|     7746 | 10767 | `	ProcConsumer xErr = 0;` |
|     7746 | 10768 | `	void *pErrData = 0;` |
|        - | 10769 | `	/* Initialize bytecode container */` |
|     7746 | 10770 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     7746 | 10771 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 10772 | `	/* Reset the code generator */` |
|     7746 | 10773 | `	if( bTrueReturn ){` |
|        - | 10774 | `		/* Included file,log compile-time errors */` |
|     6511 | 10775 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     6511 | 10776 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     3255 | 10777 | `	}` |
|     7746 | 10778 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 10779 | `	/* Swap bytecode container */` |
|     7746 | 10780 | `	pByteCode = pVm->pByteContainer;` |
|     7746 | 10781 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 10782 | `	/* Compile the chunk */` |
|     7746 | 10783 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    11618 | 10784 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 10785 | `		/* Compilation error,return false */` |
|        3 | 10786 | `		if( pCtx ){` |
|        3 | 10787 | `			ph7_result_bool(pCtx,0);` |
|        1 | 10788 | `		}` |
|        2 | 10789 | `	}else{` |
|        - | 10790 | `		/* Mount any newly defined classes */` |
|        - | 10791 | `		SyHashEntry *pEntry;` |
|        - | 10792 | `		ph7_class *pClass;` |
|        - | 10793 | `		ph7_value sResult; /* Return value */` |
|        - | 10794 | `		sxi32 rc;` |
|     7744 | 10795 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   214991 | 10796 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   203378 | 10797 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 10798 | `			/* Only mount classes that haven't been mounted yet */` |
|   203378 | 10799 | `			if( !pClass->bMounted ){` |
|    41106 | 10800 | `				rc = VmMountUserClass(pVm,pClass);` |
|    41106 | 10801 | `				if( rc != SXRET_OK ){` |
|        - | 10802 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 10803 | `					if( pCtx ){` |
|      ! 0 | 10804 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 10805 | `					}` |
|      ! 0 | 10806 | `					goto Cleanup;` |
|        - | 10807 | `				}` |
|    20552 | 10808 | `			}` |
|        2 | 10809 | `		}` |
|     7744 | 10810 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 10811 | `			/* Out of memory */` |
|      ! 0 | 10812 | `			if( pCtx ){` |
|      ! 0 | 10813 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 10814 | `			}` |
|      ! 0 | 10815 | `			goto Cleanup;` |
|        - | 10816 | `		}` |
|     7744 | 10817 | `		if( bTrueReturn ){` |
|        - | 10818 | `			/* Assume a boolean true return value */` |
|     6511 | 10819 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     3256 | 10820 | `		}else{` |
|        - | 10821 | `			/* Assume a null return value */` |
|     1234 | 10822 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 10823 | `		}` |
|        - | 10824 | `		/* Execute the compiled chunk */` |
|     7744 | 10825 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|     7744 | 10826 | `		if( pCtx ){` |
|        - | 10827 | `			/* Set the execution result */` |
|     6528 | 10828 | `			ph7_result_value(pCtx,&sResult);` |
|     3263 | 10829 | `		}` |
|     7744 | 10830 | `		PH7_MemObjRelease(&sResult);` |
|        - | 10831 | `	}` |
|     3872 | 10832 | `Cleanup:` |
|        - | 10833 | `	/* Cleanup the mess left behind */` |
|     7746 | 10834 | `	pVm->pByteContainer = pByteCode;` |
|     7746 | 10835 | `	SySetRelease(&aByteCode);` |
|     7746 | 10836 | `	return SXRET_OK;` |
|        2 | 10837 |  |
|        - | 10838 | `/*` |
|        - | 10839 | ` * value eval(string $code)` |
|        - | 10840 | ` *   Evaluate a string as PHP code.` |
|        - | 10841 | ` * Parameter` |
|        - | 10842 | ` *  code: PHP code to evaluate.` |
|        - | 10843 | ` * Return` |
|        - | 10844 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 10845 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 10846 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 10847 | ` */` |
|       16 | 10848 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10849 |  |
|        - | 10850 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       18 | 10851 | `	if( nArg < 1 ){` |
|        - | 10852 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10853 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10854 | `		return SXRET_OK;` |
|        - | 10855 | `	}` |
|        - | 10856 | `	/* Chunk to evaluate */` |
|       18 | 10857 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       18 | 10858 | `	if( sChunk.nByte < 1 ){` |
|        - | 10859 | `		/* Empty string,return NULL */` |
|        3 | 10860 | `		ph7_result_null(pCtx);` |
|        3 | 10861 | `		return SXRET_OK;` |
|        - | 10862 | `	}` |
|        - | 10863 | `	/* Eval the chunk */` |
|       16 | 10864 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       16 | 10865 | `	return SXRET_OK;` |
|       10 | 10866 |  |
|        - | 10867 | `/*` |
|        - | 10868 | ` * Check if a file path is already included.` |
|        - | 10869 | ` */` |
|    13016 | 10870 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        1 | 10871 |  |
|        - | 10872 | `	SyString *aEntries;` |
|        - | 10873 | `	sxu32 n;` |
|    13017 | 10874 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 10875 | `	/* Perform a linear search */` |
| 42342807 | 10876 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 42329797 | 10877 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 10878 | `			/* Already included */` |
|        7 | 10879 | `			return TRUE;` |
|        - | 10880 | `		}` |
| 21164896 | 10881 | `	}` |
|    13011 | 10882 | `	return FALSE;` |
|     6509 | 10883 |  |
|        - | 10884 | `/*` |
|        - | 10885 | ` * Push a file path in the appropriate VM container.` |
|        - | 10886 | ` */` |
|    14224 | 10887 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 10888 |  |
|        - | 10889 | `	SyString sPath;` |
|        - | 10890 | `	char *zDup;` |
|        - | 10891 | `#ifdef __WINNT__` |
|        - | 10892 | `	char *zCur;` |
|        - | 10893 | `#endif` |
|        - | 10894 | `	sxi32 rc;` |
|    14226 | 10895 | `	if( nLen < 0 ){` |
|     1210 | 10896 | `		nLen = SyStrlen(zPath);` |
|      604 | 10897 | `	}` |
|        - | 10898 | `	/* Duplicate the file path first */` |
|    14226 | 10899 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    14226 | 10900 | `	if( zDup == 0 ){` |
|      ! 0 | 10901 | `		return SXERR_MEM;` |
|        - | 10902 | `	}` |
|        - | 10903 | `#ifdef __WINNT__` |
|        - | 10904 | `	/* Normalize path on windows` |
|        - | 10905 | `	 * Example:` |
|        - | 10906 | `	 *    Path/To/File.php` |
|        - | 10907 | `	 * becomes` |
|        - | 10908 | `	 *   path\to\file.php` |
|        - | 10909 | `	 */` |
|        2 | 10910 | `	zCur = zDup;` |
|        2 | 10911 | `	while( zCur[0] != 0 ){` |
|        2 | 10912 | `		if( zCur[0] == '/' ){` |
|        2 | 10913 | `			zCur[0] = '\\';` |
|        2 | 10914 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 10915 | `			int c = SyToLower(zCur[0]);` |
|        1 | 10916 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 10917 | `		}` |
|        2 | 10918 | `		zCur++;` |
|        2 | 10919 | `	}` |
|        - | 10920 | `#endif` |
|        - | 10921 | `	/* Install the file path */` |
|    14226 | 10922 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    14226 | 10923 | `	if( !bMain ){` |
|    13017 | 10924 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 10925 | `			/* Already included */` |
|        7 | 10926 | `			*pNew = 0;` |
|        4 | 10927 | `		}else{` |
|        - | 10928 | `			/* Insert in the corresponding container */` |
|    13011 | 10929 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    13011 | 10930 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 10931 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 10932 | `				return rc;` |
|        - | 10933 | `			}` |
|    13011 | 10934 | `			*pNew = 1;` |
|        - | 10935 | `		}` |
|     6508 | 10936 | `	}` |
|    14226 | 10937 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    14226 | 10938 | `	return SXRET_OK;` |
|     7114 | 10939 |  |
|        - | 10940 | `/*` |
|        - | 10941 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 10942 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 10943 | ` * indicates failure.` |
|        - | 10944 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 10945 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 10946 | ` * operations.` |
|        - | 10947 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 10948 | ` * this function is a no-op.` |
|        - | 10949 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 10950 | ` * constructs for more information.` |
|        - | 10951 | ` */` |
|     6516 | 10952 | `static sxi32 VmExecIncludedFile(` |
|        - | 10953 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 10954 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 10955 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 10956 | `	 )` |
|        2 | 10957 |  |
|        - | 10958 | `	sxi32 rc;` |
|        - | 10959 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10960 | `	const ph7_io_stream *pStream;` |
|        - | 10961 | `	SyBlob sContents;` |
|        - | 10962 | `	void *pHandle;` |
|        - | 10963 | `	ph7_vm *pVm;` |
|        - | 10964 | `	int isNew;` |
|        - | 10965 | `	/* Initialize fields */` |
|     6518 | 10966 | `	pVm = pCtx->pVm;` |
|     6518 | 10967 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     6518 | 10968 | `	isNew = 0;` |
|        - | 10969 | `	/* Extract the associated stream */` |
|     6518 | 10970 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 10971 | `	/*` |
|        - | 10972 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 10973 | `	 * in a read-only mode.` |
|        - | 10974 | `	 */` |
|     6518 | 10975 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     6518 | 10976 | `	if( pHandle == 0 ){` |
|        3 | 10977 | `		return SXERR_IO;` |
|        - | 10978 | `	}` |
|     6515 | 10979 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     6515 | 10980 | `	if( IncludeOnce && !isNew ){` |
|        - | 10981 | `		/* Already included */` |
|        5 | 10982 | `		rc = SXERR_EXISTS;` |
|        3 | 10983 | `	}else{` |
|        - | 10984 | `		/* Read the whole file contents */` |
|     6511 | 10985 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     6511 | 10986 | `		if( rc == SXRET_OK ){` |
|        - | 10987 | `			SyString sScript;` |
|        - | 10988 | `			/* Compile and execute the script */` |
|     6511 | 10989 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     6511 | 10990 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     3255 | 10991 | `		}` |
|        - | 10992 | `	}` |
|        - | 10993 | `	/* Pop from the set of included file */` |
|     6515 | 10994 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 10995 | `	/* Close the handle */` |
|     6515 | 10996 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 10997 | `	/* Release the working buffer */` |
|     6515 | 10998 | `	SyBlobRelease(&sContents);` |
|        - | 10999 | `#else` |
|        - | 11000 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 11001 | `	SXUNUSED(pPath);` |
|        - | 11002 | `	SXUNUSED(IncludeOnce);` |
|        - | 11003 | `	rc = SXERR_IO;` |
|        - | 11004 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     6515 | 11005 | `	return rc;` |
|     3260 | 11006 |  |
|        - | 11007 | `/*` |
|        - | 11008 | ` * string get_include_path(void)` |
|        - | 11009 | ` *  Gets the current include_path configuration option.` |
|        - | 11010 | ` * Parameter` |
|        - | 11011 | ` *  None` |
|        - | 11012 | ` * Return` |
|        - | 11013 | ` *  Included paths as a string` |
|        - | 11014 | ` */` |
|        2 | 11015 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11016 |  |
|        3 | 11017 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11018 | `	SyString *aEntry;` |
|        - | 11019 | `	int dir_sep;` |
|        - | 11020 | `	sxu32 n;` |
|        - | 11021 | `#ifdef __WINNT__` |
|        1 | 11022 | `	dir_sep = ';';` |
|        - | 11023 | `#else` |
|        - | 11024 | `	/* Assume UNIX path separator */` |
|        2 | 11025 | `	dir_sep = ':';` |
|        - | 11026 | `#endif` |
|        1 | 11027 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 11028 | `	SXUNUSED(apArg);` |
|        - | 11029 | `	/* Point to the list of import paths */` |
|        3 | 11030 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 11031 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 11032 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 11033 | `		if( n > 0 ){` |
|        - | 11034 | `			/* Append dir seprator */` |
|      ! 0 | 11035 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 11036 | `		}` |
|        - | 11037 | `		/* Append path */` |
|        3 | 11038 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 11039 | `	}` |
|        3 | 11040 | `	return PH7_OK;` |
|        1 | 11041 |  |
|        - | 11042 | `/*` |
|        - | 11043 | ` * string get_get_included_files(void)` |
|        - | 11044 | ` *  Gets the current include_path configuration option.` |
|        - | 11045 | ` * Parameter` |
|        - | 11046 | ` *  None` |
|        - | 11047 | ` * Return` |
|        - | 11048 | ` *  Included paths as a string` |
|        - | 11049 | ` */` |
|        2 | 11050 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11051 |  |
|        3 | 11052 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 11053 | `	ph7_value *pArray,*pWorker;` |
|        - | 11054 | `	SyString *pEntry;` |
|        - | 11055 | `	int c,d;` |
|        - | 11056 | `	/* Create an array and a working value */` |
|        3 | 11057 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 11058 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 11059 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 11060 | `		/* Out of memory,return null */` |
|      ! 0 | 11061 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11062 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11063 | `		SXUNUSED(apArg);` |
|      ! 0 | 11064 | `		return PH7_OK;` |
|        - | 11065 | `	}` |
|        3 | 11066 | `	c = d = '/';` |
|        - | 11067 | `#ifdef __WINNT__` |
|        1 | 11068 | `	d = '\\';` |
|        - | 11069 | `#endif` |
|        - | 11070 | `	/* Iterate throw entries */` |
|        3 | 11071 | `	SySetResetCursor(pFiles);` |
|     2709 | 11072 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 11073 | `		const char *zBase,*zEnd;` |
|        - | 11074 | `		int iLen;` |
|        - | 11075 | `		/* reset the string cursor */` |
|     2707 | 11076 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 11077 | `		/* Extract base name */` |
|     2707 | 11078 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 11079 | `		/* Ignore trailing '/' */` |
|     4060 | 11080 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 11081 | `			zEnd--;` |
|      ! 0 | 11082 | `		}` |
|     2707 | 11083 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|    75890 | 11084 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|    71831 | 11085 | `			zEnd--;` |
|        1 | 11086 | `		}` |
|     2707 | 11087 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     2707 | 11088 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 11089 | `		/* Copy entry name */` |
|     2707 | 11090 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 11091 | `		/* Perform the insertion */` |
|     2707 | 11092 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 11093 | `	}` |
|        - | 11094 | `	/* All done,return the created array */` |
|        3 | 11095 | `	ph7_result_value(pCtx,pArray);` |
|        - | 11096 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 11097 | `	 * by the engine as soon we return from this foreign` |
|        - | 11098 | `	 * function.` |
|        - | 11099 | `	 */` |
|        3 | 11100 | `	return PH7_OK;` |
|        2 | 11101 |  |
|        - | 11102 | `/*` |
|        - | 11103 | ` * include:` |
|        - | 11104 | ` * According to the PHP reference manual.` |
|        - | 11105 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 11106 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 11107 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 11108 | ` *  include() will finally check in the calling script's own directory` |
|        - | 11109 | ` *  and the current working directory before failing. The include()` |
|        - | 11110 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 11111 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 11112 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 11113 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 11114 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 11115 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 11116 | ` *  directory to find the requested file.` |
|        - | 11117 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 11118 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 11119 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 11120 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 11121 | ` */` |
|     6504 | 11122 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11123 |  |
|        - | 11124 | `	SyString sFile;` |
|        - | 11125 | `	sxi32 rc;` |
|     6506 | 11126 | `	if( nArg < 1 ){` |
|        - | 11127 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11128 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11129 | `		return SXRET_OK;` |
|        - | 11130 | `	}` |
|        - | 11131 | `	/* File to include */` |
|     6506 | 11132 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     6506 | 11133 | `	if( sFile.nByte < 1 ){` |
|        - | 11134 | `		/* Empty string,return NULL */` |
|      ! 0 | 11135 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11136 | `		return SXRET_OK;` |
|        - | 11137 | `	}` |
|        - | 11138 | `	/* Open,compile and execute the desired script */` |
|     6506 | 11139 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     6506 | 11140 | `	if( rc != SXRET_OK ){` |
|        - | 11141 | `		/* Emit a warning and return false */` |
|        3 | 11142 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 11143 | `		ph7_result_bool(pCtx,0);` |
|        1 | 11144 | `	}` |
|     6506 | 11145 | `	return SXRET_OK;` |
|     3254 | 11146 |  |
|        - | 11147 | `/*` |
|        - | 11148 | ` * include_once:` |
|        - | 11149 | ` *  According to the PHP reference manual.` |
|        - | 11150 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 11151 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 11152 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 11153 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 11154 | ` *   just once.` |
|        - | 11155 | ` */` |
|        4 | 11156 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11157 |  |
|        - | 11158 | `	SyString sFile;` |
|        - | 11159 | `	sxi32 rc;` |
|        5 | 11160 | `	if( nArg < 1 ){` |
|        - | 11161 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11162 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11163 | `		return SXRET_OK;` |
|        - | 11164 | `	}` |
|        - | 11165 | `	/* File to include */` |
|        5 | 11166 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11167 | `	if( sFile.nByte < 1 ){` |
|        - | 11168 | `		/* Empty string,return NULL */` |
|      ! 0 | 11169 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11170 | `		return SXRET_OK;` |
|        - | 11171 | `	}` |
|        - | 11172 | `	/* Open,compile and execute the desired script */` |
|        5 | 11173 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 11174 | `	if( rc == SXERR_EXISTS ){` |
|        - | 11175 | `		/* File already included,return TRUE */` |
|        3 | 11176 | `		ph7_result_bool(pCtx,1);` |
|        3 | 11177 | `		return SXRET_OK;` |
|        - | 11178 | `	}` |
|        3 | 11179 | `	if( rc != SXRET_OK ){` |
|        - | 11180 | `		/* Emit a warning and return false */` |
|      ! 0 | 11181 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11182 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11183 | ` 	}` |
|        3 | 11184 | `	return SXRET_OK;` |
|        3 | 11185 |  |
|        - | 11186 | `/*` |
|        - | 11187 | ` * require.` |
|        - | 11188 | ` *  According to the PHP reference manual.` |
|        - | 11189 | ` *   require() is identical to include() except upon failure it will` |
|        - | 11190 | ` *   also produce a fatal level error.` |
|        - | 11191 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 11192 | ` *   emits a warning  which allows the script to continue.` |
|        - | 11193 | ` */` |
|        4 | 11194 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11195 |  |
|        - | 11196 | `	SyString sFile;` |
|        - | 11197 | `	sxi32 rc;` |
|        5 | 11198 | `	if( nArg < 1 ){` |
|        - | 11199 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11200 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11201 | `		return SXRET_OK;` |
|        - | 11202 | `	}` |
|        - | 11203 | `	/* File to include */` |
|        5 | 11204 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11205 | `	if( sFile.nByte < 1 ){` |
|        - | 11206 | `		/* Empty string,return NULL */` |
|      ! 0 | 11207 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11208 | `		return SXRET_OK;` |
|        - | 11209 | `	}` |
|        - | 11210 | `	/* Open,compile and execute the desired script */` |
|        5 | 11211 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        5 | 11212 | `	if( rc != SXRET_OK ){` |
|        - | 11213 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 11214 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11215 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11216 | `		return PH7_ABORT;` |
|        - | 11217 | `	}` |
|        5 | 11218 | `	return SXRET_OK;` |
|        3 | 11219 |  |
|        - | 11220 | `/*` |
|        - | 11221 | ` * require_once:` |
|        - | 11222 | ` *  According to the PHP reference manual.` |
|        - | 11223 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 11224 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 11225 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 11226 | ` *   and how it differs from its non _once siblings.` |
|        - | 11227 | ` */` |
|        4 | 11228 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11229 |  |
|        - | 11230 | `	SyString sFile;` |
|        - | 11231 | `	sxi32 rc;` |
|        5 | 11232 | `	if( nArg < 1 ){` |
|        - | 11233 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11234 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11235 | `		return SXRET_OK;` |
|        - | 11236 | `	}` |
|        - | 11237 | `	/* File to include */` |
|        5 | 11238 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11239 | `	if( sFile.nByte < 1 ){` |
|        - | 11240 | `		/* Empty string,return NULL */` |
|      ! 0 | 11241 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11242 | `		return SXRET_OK;` |
|        - | 11243 | `	}` |
|        - | 11244 | `	/* Open,compile and execute the desired script */` |
|        5 | 11245 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 11246 | `	if( rc == SXERR_EXISTS ){` |
|        - | 11247 | `		/* File already included,return TRUE */` |
|        3 | 11248 | `		ph7_result_bool(pCtx,1);` |
|        3 | 11249 | `		return SXRET_OK;` |
|        - | 11250 | `	}` |
|        3 | 11251 | `	if( rc != SXRET_OK ){` |
|        - | 11252 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 11253 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11254 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11255 | `		return PH7_ABORT;` |
|        - | 11256 | `	}` |
|        3 | 11257 | `	return SXRET_OK;` |
|        3 | 11258 |  |
|        - | 11259 | `/*` |
|        - | 11260 | ` * Section:` |
|        - | 11261 | ` *  Command line arguments processing.` |
|        - | 11262 | ` * Status:` |
|        - | 11263 | ` *    Stable.` |
|        - | 11264 | ` */` |
|        - | 11265 | `/*` |
|        - | 11266 | ` * Check if a short option argument [i.e: -c] is available in the command` |
|        - | 11267 | ` * line string. Return a pointer to the start of the stream on success.` |
|        - | 11268 | ` * NULL otherwise.` |
|        - | 11269 | ` */` |
|        6 | 11270 | `static const char * VmFindShortOpt(int c,const char *zIn,const char *zEnd)` |
|        1 | 11271 |  |
|      199 | 11272 | `	while( zIn < zEnd ){` |
|      193 | 11273 | `		if( zIn[0] == '-' && &zIn[1] < zEnd && (int)zIn[1] == c ){` |
|        - | 11274 | `			/* Got one */` |
|      ! 0 | 11275 | `			return &zIn[1];` |
|        - | 11276 | `		}` |
|        - | 11277 | `		/* Advance the cursor */` |
|      193 | 11278 | `		zIn++;` |
|        1 | 11279 | `	}` |
|        - | 11280 | `	/* No such option */` |
|        7 | 11281 | `	return 0;` |
|        4 | 11282 |  |
|        - | 11283 | `/*` |
|        - | 11284 | ` * Check if a long option argument [i.e: --opt] is available in the command` |
|        - | 11285 | ` * line string. Return a pointer to the start of the stream on success.` |
|        - | 11286 | ` * NULL otherwise.` |
|        - | 11287 | ` */` |
|      ! 0 | 11288 | `static const char * VmFindLongOpt(const char *zLong,int nByte,const char *zIn,const char *zEnd)` |
|      ! 0 | 11289 |  |
|        - | 11290 | `	const char *zOpt;` |
|      ! 0 | 11291 | `	while( zIn < zEnd ){` |
|      ! 0 | 11292 | `		if( zIn[0] == '-' && &zIn[1] < zEnd && (int)zIn[1] == '-' ){` |
|      ! 0 | 11293 | `			zIn += 2;` |
|      ! 0 | 11294 | `			zOpt = zIn;` |
|      ! 0 | 11295 | `			while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|      ! 0 | 11296 | `				if( zIn[0] == '=' /* --opt=val */){` |
|      ! 0 | 11297 | `					break;` |
|        - | 11298 | `				}` |
|      ! 0 | 11299 | `				zIn++;` |
|      ! 0 | 11300 | `			}` |
|        - | 11301 | `			/* Test */` |
|      ! 0 | 11302 | `			if( (int)(zIn-zOpt) == nByte && SyMemcmp(zOpt,zLong,nByte) == 0 ){` |
|        - | 11303 | `				/* Got one,return it's value */` |
|      ! 0 | 11304 | `				return zIn;` |
|        - | 11305 | `			}` |
|        - | 11306 |  |
|      ! 0 | 11307 | `		}else{` |
|      ! 0 | 11308 | `			zIn++;` |
|        - | 11309 | `		}` |
|      ! 0 | 11310 | `	}` |
|        - | 11311 | `	/* No such option */` |
|      ! 0 | 11312 | `	return 0;` |
|      ! 0 | 11313 |  |
|        - | 11314 | `/*` |
|        - | 11315 | ` * Long option [i.e: --opt] arguments private data structure.` |
|        - | 11316 | ` */` |
|        - | 11317 | `struct getopt_long_opt` |
|        - | 11318 |  |
|        - | 11319 | `	const char *zArgIn,*zArgEnd; /* Command line arguments */` |
|        - | 11320 | `	ph7_value *pWorker;  /* Worker variable*/` |
|        - | 11321 | `	ph7_value *pArray;   /* getopt() return value */` |
|        - | 11322 | `	ph7_context *pCtx;   /* Call Context */` |
|        - | 11323 | `};` |
|        - | 11324 | `/* Forward declaration */` |
|        - | 11325 | `static int VmProcessLongOpt(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 11326 | `/*` |
|        - | 11327 | ` * Extract short or long argument option values.` |
|        - | 11328 | ` */` |
|      ! 0 | 11329 | `static void VmExtractOptArgValue(` |
|        - | 11330 | `	ph7_value *pArray,  /* getopt() return value */` |
|        - | 11331 | `	ph7_value *pWorker, /* Worker variable */` |
|        - | 11332 | `	const char *zArg,   /* Argument stream */` |
|        - | 11333 | `	const char *zArgEnd,/* End of the argument stream  */` |
|        - | 11334 | `	int need_val,       /* TRUE to fetch option argument */` |
|        - | 11335 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 11336 | `	const char *zName   /* Option name */)` |
|      ! 0 | 11337 |  |
|      ! 0 | 11338 | `	ph7_value_bool(pWorker,0);` |
|      ! 0 | 11339 | `	if( !need_val ){` |
|        - | 11340 | `		/*` |
|        - | 11341 | `		 * Option does not need arguments.` |
|        - | 11342 | `		 * Insert the option name and a boolean FALSE.` |
|        - | 11343 | `		 */` |
|      ! 0 | 11344 | `		ph7_array_add_strkey_elem(pArray,(const char *)zName,pWorker); /* Will make it's own copy */` |
|      ! 0 | 11345 | `	}else{` |
|        - | 11346 | `		const char *zCur;` |
|        - | 11347 | `		/* Extract option argument */` |
|      ! 0 | 11348 | `		zArg++;` |
|      ! 0 | 11349 | `		if( zArg < zArgEnd && zArg[0] == '=' ){` |
|      ! 0 | 11350 | `			zArg++;` |
|      ! 0 | 11351 | `		}` |
|      ! 0 | 11352 | `		while( zArg < zArgEnd && (unsigned char)zArg[0] < 0xc0 && SyisSpace(zArg[0]) ){` |
|      ! 0 | 11353 | `			zArg++;` |
|      ! 0 | 11354 | `		}` |
|      ! 0 | 11355 | `		if( zArg >= zArgEnd \|\| zArg[0] == '-' ){` |
|        - | 11356 | `			/*` |
|        - | 11357 | `			 * Argument not found.` |
|        - | 11358 | `			 * Insert the option name and a boolean FALSE.` |
|        - | 11359 | `			 */` |
|      ! 0 | 11360 | `			ph7_array_add_strkey_elem(pArray,(const char *)zName,pWorker); /* Will make it's own copy */` |
|      ! 0 | 11361 | `			return;` |
|        - | 11362 | `		}` |
|        - | 11363 | `		/* Delimit the value */` |
|      ! 0 | 11364 | `		zCur = zArg;` |
|      ! 0 | 11365 | `		if( zArg[0] == '\'' \|\| zArg[0] == '"' ){` |
|      ! 0 | 11366 | `			int d = zArg[0];` |
|        - | 11367 | `			/* Delimt the argument */` |
|      ! 0 | 11368 | `			zArg++;` |
|      ! 0 | 11369 | `			zCur = zArg;` |
|      ! 0 | 11370 | `			while( zArg < zArgEnd ){` |
|      ! 0 | 11371 | `				if( zArg[0] == d && zArg[-1] != '\\' ){` |
|        - | 11372 | `					/* Delimiter found,exit the loop  */` |
|      ! 0 | 11373 | `					break;` |
|        - | 11374 | `				}` |
|      ! 0 | 11375 | `				zArg++;` |
|      ! 0 | 11376 | `			}` |
|        - | 11377 | `			/* Save the value */` |
|      ! 0 | 11378 | `			ph7_value_string(pWorker,zCur,(int)(zArg-zCur));` |
|      ! 0 | 11379 | `			if( zArg < zArgEnd ){ zArg++; }` |
|      ! 0 | 11380 | `		}else{` |
|      ! 0 | 11381 | `			while( zArg < zArgEnd && !SyisSpace(zArg[0]) ){` |
|      ! 0 | 11382 | `				zArg++;` |
|      ! 0 | 11383 | `			}` |
|        - | 11384 | `			/* Save the value */` |
|      ! 0 | 11385 | `			ph7_value_string(pWorker,zCur,(int)(zArg-zCur));` |
|        - | 11386 | `		}` |
|        - | 11387 | `		/*` |
|        - | 11388 | `		 * Check if we are dealing with multiple values.` |
|        - | 11389 | `		 * If so,create an array to hold them,rather than a scalar variable.` |
|        - | 11390 | `		 */` |
|      ! 0 | 11391 | `		while( zArg < zArgEnd && (unsigned char)zArg[0] < 0xc0 && SyisSpace(zArg[0]) ){` |
|      ! 0 | 11392 | `			zArg++;` |
|      ! 0 | 11393 | `		}` |
|      ! 0 | 11394 | `		if( zArg < zArgEnd && zArg[0] != '-' ){` |
|        - | 11395 | `			ph7_value *pOptArg; /* Array of option arguments */` |
|      ! 0 | 11396 | `			pOptArg = ph7_context_new_array(pCtx);` |
|      ! 0 | 11397 | `			if( pOptArg == 0 ){` |
|      ! 0 | 11398 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 11399 | `			}else{` |
|        - | 11400 | `				/* Insert the first value */` |
|      ! 0 | 11401 | `				ph7_array_add_elem(pOptArg,0,pWorker); /* Will make it's own copy */` |
|      ! 0 | 11402 | `				for(;;){` |
|      ! 0 | 11403 | `					if( zArg >= zArgEnd \|\| zArg[0] == '-' ){` |
|        - | 11404 | `						/* No more value */` |
|      ! 0 | 11405 | `						break;` |
|        - | 11406 | `					}` |
|        - | 11407 | `					/* Delimit the value */` |
|      ! 0 | 11408 | `					zCur = zArg;` |
|      ! 0 | 11409 | `					if( zArg < zArgEnd && zArg[0] == '\\' ){` |
|      ! 0 | 11410 | `						zArg++;` |
|      ! 0 | 11411 | `						zCur = zArg;` |
|      ! 0 | 11412 | `					}` |
|      ! 0 | 11413 | `					while( zArg < zArgEnd && !SyisSpace(zArg[0]) ){` |
|      ! 0 | 11414 | `						zArg++;` |
|      ! 0 | 11415 | `					}` |
|        - | 11416 | `					/* Reset the string cursor */` |
|      ! 0 | 11417 | `					ph7_value_reset_string_cursor(pWorker);` |
|        - | 11418 | `					/* Save the value */` |
|      ! 0 | 11419 | `					ph7_value_string(pWorker,zCur,(int)(zArg-zCur));` |
|        - | 11420 | `					/* Insert */` |
|      ! 0 | 11421 | `					ph7_array_add_elem(pOptArg,0,pWorker); /* Will make it's own copy */` |
|        - | 11422 | `					/* Jump trailing white spaces */` |
|      ! 0 | 11423 | `					while( zArg < zArgEnd && (unsigned char)zArg[0] < 0xc0 && SyisSpace(zArg[0]) ){` |
|      ! 0 | 11424 | `						zArg++;` |
|      ! 0 | 11425 | `					}` |
|      ! 0 | 11426 | `				}` |
|        - | 11427 | `				/* Insert the option arg array */` |
|      ! 0 | 11428 | `				ph7_array_add_strkey_elem(pArray,(const char *)zName,pOptArg); /* Will make it's own copy */` |
|        - | 11429 | `				/* Safely release */` |
|      ! 0 | 11430 | `				ph7_context_release_value(pCtx,pOptArg);` |
|        - | 11431 | `			}` |
|      ! 0 | 11432 | `		}else{` |
|        - | 11433 | `			/* Single value */` |
|      ! 0 | 11434 | `			ph7_array_add_strkey_elem(pArray,(const char *)zName,pWorker); /* Will make it's own copy */` |
|        - | 11435 | `		}` |
|        - | 11436 | `	}` |
|      ! 0 | 11437 |  |
|        - | 11438 | `/*` |
|        - | 11439 | ` * array getopt(string $options[,array $longopts ])` |
|        - | 11440 | ` *   Gets options from the command line argument list.` |
|        - | 11441 | ` * Parameters` |
|        - | 11442 | ` *  $options` |
|        - | 11443 | ` *   Each character in this string will be used as option characters` |
|        - | 11444 | ` *   and matched against options passed to the script starting with` |
|        - | 11445 | ` *   a single hyphen (-). For example, an option string "x" recognizes` |
|        - | 11446 | ` *   an option -x. Only a-z, A-Z and 0-9 are allowed.` |
|        - | 11447 | ` *  $longopts` |
|        - | 11448 | ` *   An array of options. Each element in this array will be used as option` |
|        - | 11449 | ` *   strings and matched against options passed to the script starting with` |
|        - | 11450 | ` *   two hyphens (--). For example, an longopts element "opt" recognizes an` |
|        - | 11451 | ` *   option --opt.` |
|        - | 11452 | ` * Return` |
|        - | 11453 | ` *  This function will return an array of option / argument pairs or FALSE` |
|        - | 11454 | ` *  on failure.` |
|        - | 11455 | ` */` |
|        2 | 11456 | `static int vm_builtin_getopt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11457 |  |
|        - | 11458 | `	const char *zIn,*zEnd,*zArg,*zArgIn,*zArgEnd;` |
|        - | 11459 | `	struct getopt_long_opt sLong;` |
|        - | 11460 | `	ph7_value *pArray,*pWorker;` |
|        - | 11461 | `	SyBlob *pArg;` |
|        - | 11462 | `	int nByte;` |
|        3 | 11463 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 11464 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 11465 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Missing/Invalid option arguments");` |
|      ! 0 | 11466 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11467 | `		return PH7_OK;` |
|        - | 11468 | `	}` |
|        - | 11469 | `	/* Extract option arguments */` |
|        3 | 11470 | `	zIn  = ph7_value_to_string(apArg[0],&nByte);` |
|        3 | 11471 | `	zEnd = &zIn[nByte];` |
|        - | 11472 | `	/* Point to the string representation of the $argv[] array */` |
|        3 | 11473 | `	pArg = &pCtx->pVm->sArgv;` |
|        - | 11474 | `	/* Create a new empty array and a worker variable */` |
|        3 | 11475 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11476 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 11477 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|      ! 0 | 11478 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 11479 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11480 | `		return PH7_OK;` |
|        - | 11481 | `	}` |
|        3 | 11482 | `	if( SyBlobLength(pArg) < 1 ){` |
|        - | 11483 | `		/* Empty command line,return the empty array*/` |
|      ! 0 | 11484 | `		ph7_result_value(pCtx,pArray);` |
|        - | 11485 | `		/* Everything will be released automatically when we return` |
|        - | 11486 | `		 * from this function.` |
|        - | 11487 | `		 */` |
|      ! 0 | 11488 | `		return PH7_OK;` |
|        - | 11489 | `	}` |
|        3 | 11490 | `	zArgIn = (const char *)SyBlobData(pArg);` |
|        3 | 11491 | `	zArgEnd = &zArgIn[SyBlobLength(pArg)];` |
|        - | 11492 | `	/* Fill the long option structure */` |
|        3 | 11493 | `	sLong.pArray = pArray;` |
|        3 | 11494 | `	sLong.pWorker = pWorker;` |
|        3 | 11495 | `	sLong.zArgIn =  zArgIn;` |
|        3 | 11496 | `	sLong.zArgEnd = zArgEnd;` |
|        3 | 11497 | `	sLong.pCtx = pCtx;` |
|        - | 11498 | `	/* Start processing */` |
|        9 | 11499 | `	while( zIn < zEnd ){` |
|        7 | 11500 | `		int c = zIn[0];` |
|        7 | 11501 | `		int need_val = 0;` |
|        - | 11502 | `		/* Advance the stream cursor */` |
|        7 | 11503 | `		zIn++;` |
|        - | 11504 | `		/* Ignore non-alphanum characters */` |
|        7 | 11505 | `		if( !SyisAlphaNum(c) ){` |
|      ! 0 | 11506 | `			continue;` |
|        - | 11507 | `		}` |
|        7 | 11508 | `		if( zIn < zEnd && zIn[0] == ':' ){` |
|        5 | 11509 | `			zIn++;` |
|        5 | 11510 | `			need_val = 1;` |
|        5 | 11511 | `			if( zIn < zEnd && zIn[0] == ':' ){` |
|      ! 0 | 11512 | `				zIn++;` |
|      ! 0 | 11513 | `			}` |
|        2 | 11514 | `		}` |
|        - | 11515 | `		/* Find option */` |
|        7 | 11516 | `		zArg = VmFindShortOpt(c,zArgIn,zArgEnd);` |
|        7 | 11517 | `		if( zArg == 0 ){` |
|        - | 11518 | `			/* No such option */` |
|        7 | 11519 | `			continue;` |
|        - | 11520 | `		}` |
|        - | 11521 | `		/* Extract option argument value */` |
|      ! 0 | 11522 | `		VmExtractOptArgValue(pArray,pWorker,zArg,zArgEnd,need_val,pCtx,(const char *)&c);` |
|      ! 0 | 11523 | `	}` |
|        3 | 11524 | `	if( nArg > 1 && ph7_value_is_array(apArg[1]) && ph7_array_count(apArg[1]) > 0 ){` |
|        - | 11525 | `		/* Process long options */` |
|      ! 0 | 11526 | `		ph7_array_walk(apArg[1],VmProcessLongOpt,&sLong);` |
|      ! 0 | 11527 | `	}` |
|        - | 11528 | `	/* Return the option array */` |
|        3 | 11529 | `	ph7_result_value(pCtx,pArray);` |
|        - | 11530 | `	/*` |
|        - | 11531 | `	 * Don't worry about freeing memory, everything will be released` |
|        - | 11532 | `	 * automatically as soon we return from this foreign function.` |
|        - | 11533 | `	 */` |
|        3 | 11534 | `	return PH7_OK;` |
|        2 | 11535 |  |
|        - | 11536 | `/*` |
|        - | 11537 | ` * Array walker callback used for processing long options values.` |
|        - | 11538 | ` */` |
|      ! 0 | 11539 | `static int VmProcessLongOpt(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 11540 |  |
|      ! 0 | 11541 | `	struct getopt_long_opt *pOpt = (struct getopt_long_opt *)pUserData;` |
|        - | 11542 | `	const char *zArg,*zOpt,*zEnd;` |
|      ! 0 | 11543 | `	int need_value = 0;` |
|        - | 11544 | `	int nByte;` |
|        - | 11545 | `	/* Value must be of type string */` |
|      ! 0 | 11546 | `	if( !ph7_value_is_string(pValue) ){` |
|        - | 11547 | `		/* Simply ignore */` |
|      ! 0 | 11548 | `		return PH7_OK;` |
|        - | 11549 | `	}` |
|      ! 0 | 11550 | `	zOpt = ph7_value_to_string(pValue,&nByte);` |
|      ! 0 | 11551 | `	if( nByte < 1 ){` |
|        - | 11552 | `		/* Empty string,ignore */` |
|      ! 0 | 11553 | `		return PH7_OK;` |
|        - | 11554 | `	}` |
|      ! 0 | 11555 | `	zEnd = &zOpt[nByte - 1];` |
|      ! 0 | 11556 | `	if( zEnd[0] == ':' ){` |
|        - | 11557 | `		char *zTerm;` |
|        - | 11558 | `		/* Try to extract a value */` |
|      ! 0 | 11559 | `		need_value = 1;` |
|      ! 0 | 11560 | `		while( zEnd >= zOpt && zEnd[0] == ':' ){` |
|      ! 0 | 11561 | `			zEnd--;` |
|      ! 0 | 11562 | `		}` |
|      ! 0 | 11563 | `		if( zOpt >= zEnd ){` |
|        - | 11564 | `			/* Empty string,ignore */` |
|      ! 0 | 11565 | `			SXUNUSED(pKey);` |
|      ! 0 | 11566 | `			return PH7_OK;` |
|        - | 11567 | `		}` |
|      ! 0 | 11568 | `		zEnd++;` |
|      ! 0 | 11569 | `		zTerm = (char *)zEnd;` |
|      ! 0 | 11570 | `		zTerm[0] = 0;` |
|      ! 0 | 11571 | `	}else{` |
|      ! 0 | 11572 | `		zEnd = &zOpt[nByte];` |
|        - | 11573 | `	}` |
|        - | 11574 | `	/* Find the option */` |
|      ! 0 | 11575 | `	zArg = VmFindLongOpt(zOpt,(int)(zEnd-zOpt),pOpt->zArgIn,pOpt->zArgEnd);` |
|      ! 0 | 11576 | `	if( zArg == 0 ){` |
|        - | 11577 | `		/* No such option,return immediately */` |
|      ! 0 | 11578 | `		return PH7_OK;` |
|        - | 11579 | `	}` |
|        - | 11580 | `	/* Try to extract a value */` |
|      ! 0 | 11581 | `	VmExtractOptArgValue(pOpt->pArray,pOpt->pWorker,zArg,pOpt->zArgEnd,need_value,pOpt->pCtx,zOpt);` |
|      ! 0 | 11582 | `	return PH7_OK;` |
|      ! 0 | 11583 |  |
|        - | 11584 | `/*` |
|        - | 11585 | ` * Section:` |
|        - | 11586 | ` *  JSON encoding/decoding routines.` |
|        - | 11587 | ` * Status:` |
|        - | 11588 | ` *    Devel.` |
|        - | 11589 | ` */` |
|        - | 11590 | `/* Forward reference */` |
|        - | 11591 | `static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 11592 | `static int VmJsonObjectEncode(const char *zAttr,ph7_value *pValue,void *pUserData);` |
|        - | 11593 | `/*` |
|        - | 11594 | ` * JSON encoder state is stored in an instance` |
|        - | 11595 | ` * of the following structure.` |
|        - | 11596 | ` */` |
|        - | 11597 | `typedef struct json_private_data json_private_data;` |
|        - | 11598 | `struct json_private_data` |
|        - | 11599 |  |
|        - | 11600 | `	ph7_context *pCtx; /* Call context */` |
|        - | 11601 | `	int isFirst;       /* True if first encoded entry */` |
|        - | 11602 | `	int iFlags;        /* JSON encoding flags */` |
|        - | 11603 | `	int nRecCount;     /* Recursion count */` |
|        - | 11604 | `};` |
|        - | 11605 | `/*` |
|        - | 11606 | ` * Returns the JSON representation of a value.In other word perform a JSON encoding operation.` |
|        - | 11607 | ` * According to wikipedia` |
|        - | 11608 | ` * JSON's basic types are:` |
|        - | 11609 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|        - | 11610 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|        - | 11611 | ` *   Boolean (true or false)` |
|        - | 11612 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|        - | 11613 | ` *    do not need to be of the same type)` |
|        - | 11614 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|        - | 11615 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|        - | 11616 | ` *     be distinct from each other)` |
|        - | 11617 | ` *   null (empty)` |
|        - | 11618 | ` * Non-significant white space may be added freely around the "structural characters"` |
|        - | 11619 | ` * (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|        - | 11620 | ` */` |
|        8 | 11621 | `static sxi32 VmJsonEncode(` |
|        - | 11622 | `	ph7_value *pIn,          /* Encode this value */` |
|        - | 11623 | `	json_private_data *pData /* Context data */` |
|        1 | 11624 | `	){` |
|        9 | 11625 | `		ph7_context *pCtx = pData->pCtx;` |
|        9 | 11626 | `		int iFlags = pData->iFlags;` |
|        - | 11627 | `		int nByte;` |
|        9 | 11628 | `		if( ph7_value_is_null(pIn) \|\| ph7_value_is_resource(pIn)){` |
|        - | 11629 | `			/* null */` |
|      ! 0 | 11630 | `			ph7_result_string(pCtx,"null",(int)sizeof("null")-1);` |
|        9 | 11631 | `		}else if( ph7_value_is_bool(pIn) ){` |
|      ! 0 | 11632 | `			int iBool = ph7_value_to_bool(pIn);` |
|        - | 11633 | `			int iLen;` |
|        - | 11634 | `			/* true/false */` |
|      ! 0 | 11635 | `			iLen = iBool ? (int)sizeof("true") : (int)sizeof("false");` |
|      ! 0 | 11636 | `			ph7_result_string(pCtx,iBool ? "true" : "false",iLen-1);` |
|       12 | 11637 | `		}else if(  ph7_value_is_numeric(pIn) && !ph7_value_is_string(pIn) ){` |
|        - | 11638 | `			const char *zNum;` |
|        - | 11639 | `			/* Get a string representation of the number */` |
|        7 | 11640 | `			zNum = ph7_value_to_string(pIn,&nByte);` |
|        7 | 11641 | `			ph7_result_string(pCtx,zNum,nByte);` |
|        6 | 11642 | `		}else if( ph7_value_is_string(pIn) ){` |
|      ! 0 | 11643 | `			if( (iFlags & JSON_NUMERIC_CHECK) &&  ph7_value_is_numeric(pIn) ){` |
|        - | 11644 | `				const char *zNum;` |
|        - | 11645 | `				/* Encodes numeric strings as numbers. */` |
|      ! 0 | 11646 | `				PH7_MemObjToReal(pIn); /* Force a numeric cast */` |
|        - | 11647 | `				/* Get a string representation of the number */` |
|      ! 0 | 11648 | `				zNum = ph7_value_to_string(pIn,&nByte);` |
|      ! 0 | 11649 | `				ph7_result_string(pCtx,zNum,nByte);` |
|      ! 0 | 11650 | `			}else{` |
|        - | 11651 | `				const char *zIn,*zEnd;` |
|        - | 11652 | `				int c;` |
|        - | 11653 | `				/* Encode the string */` |
|      ! 0 | 11654 | `				zIn = ph7_value_to_string(pIn,&nByte);` |
|      ! 0 | 11655 | `				zEnd = &zIn[nByte];` |
|        - | 11656 | `				/* Append the double quote */` |
|      ! 0 | 11657 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      ! 0 | 11658 | `				for(;;){` |
|      ! 0 | 11659 | `					if( zIn >= zEnd ){` |
|        - | 11660 | `						/* No more input to process */` |
|      ! 0 | 11661 | `						break;` |
|        - | 11662 | `					}` |
|      ! 0 | 11663 | `					c = zIn[0];` |
|        - | 11664 | `					/* Advance the stream cursor */` |
|      ! 0 | 11665 | `					zIn++;` |
|      ! 0 | 11666 | `					if( (c == '<' \|\| c == '>') && (iFlags & JSON_HEX_TAG) ){` |
|        - | 11667 | `						/* All < and > are converted to \u003C and \u003E */` |
|      ! 0 | 11668 | `						if( c == '<' ){` |
|      ! 0 | 11669 | `							ph7_result_string(pCtx,"\\u003C",(int)sizeof("\\u003C")-1);` |
|      ! 0 | 11670 | `						}else{` |
|      ! 0 | 11671 | `							ph7_result_string(pCtx,"\\u003E",(int)sizeof("\\u003E")-1);` |
|        - | 11672 | `						}` |
|      ! 0 | 11673 | `						continue;` |
|      ! 0 | 11674 | `					}else if( c == '&' && (iFlags & JSON_HEX_AMP) ){` |
|        - | 11675 | `						/* All &s are converted to \u0026.  */` |
|      ! 0 | 11676 | `						ph7_result_string(pCtx,"\\u0026",(int)sizeof("\\u0026")-1);` |
|      ! 0 | 11677 | `						continue;` |
|      ! 0 | 11678 | `					}else if( c == '\'' && (iFlags & JSON_HEX_APOS) ){` |
|        - | 11679 | `						/* All ' are converted to \u0027.   */` |
|      ! 0 | 11680 | `						ph7_result_string(pCtx,"\\u0027",(int)sizeof("\\u0027")-1);` |
|      ! 0 | 11681 | `						continue;` |
|      ! 0 | 11682 | `					}else if( c == '"' && (iFlags & JSON_HEX_QUOT) ){` |
|        - | 11683 | `						/* All " are converted to \u0022. */` |
|      ! 0 | 11684 | `						ph7_result_string(pCtx,"\\u0022",(int)sizeof("\\u0022")-1);` |
|      ! 0 | 11685 | `						continue;` |
|        - | 11686 | `					}` |
|      ! 0 | 11687 | `					if( c == '"' \|\| (c == '\\' && ((iFlags & JSON_UNESCAPED_SLASHES)==0)) ){` |
|        - | 11688 | `						/* Unescape the character */` |
|      ! 0 | 11689 | `						ph7_result_string(pCtx,"\\",(int)sizeof(char));` |
|      ! 0 | 11690 | `					}` |
|        - | 11691 | `					/* Append character verbatim */` |
|      ! 0 | 11692 | `					ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      ! 0 | 11693 | `				}` |
|        - | 11694 | `				/* Append the double quote */` |
|      ! 0 | 11695 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      ! 0 | 11696 | `			}` |
|        3 | 11697 | `		}else if( ph7_value_is_array(pIn) ){` |
|        3 | 11698 | `			int c = '[',d = ']';` |
|        - | 11699 | `			/* Encode the array */` |
|        3 | 11700 | `			pData->isFirst = 1;` |
|        3 | 11701 | `			if( iFlags & JSON_FORCE_OBJECT ){` |
|        - | 11702 | `				/* Outputs an object rather than an array */` |
|      ! 0 | 11703 | `				c = '{';` |
|      ! 0 | 11704 | `				d = '}';` |
|      ! 0 | 11705 | `			}` |
|        - | 11706 | `			/* Append the square bracket or curly braces */` |
|        3 | 11707 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|        - | 11708 | `			/* Iterate throw array entries */` |
|        3 | 11709 | `			ph7_array_walk(pIn,VmJsonArrayEncode,pData);` |
|        - | 11710 | `			/* Append the closing square bracket or curly braces */` |
|        3 | 11711 | `			ph7_result_string(pCtx,(const char *)&d,(int)sizeof(char));` |
|        1 | 11712 | `		}else if( ph7_value_is_object(pIn) ){` |
|        - | 11713 | `			/* Encode the class instance */` |
|      ! 0 | 11714 | `			pData->isFirst = 1;` |
|        - | 11715 | `			/* Append the curly braces */` |
|      ! 0 | 11716 | `			ph7_result_string(pCtx,"{",(int)sizeof(char));` |
|        - | 11717 | `			/* Iterate throw class attribute */` |
|      ! 0 | 11718 | `			ph7_object_walk(pIn,VmJsonObjectEncode,pData);` |
|        - | 11719 | `			/* Append the closing curly braces  */` |
|      ! 0 | 11720 | `			ph7_result_string(pCtx,"}",(int)sizeof(char));` |
|      ! 0 | 11721 | `		}else{` |
|        - | 11722 | `			/* Can't happen */` |
|      ! 0 | 11723 | `			ph7_result_string(pCtx,"null",(int)sizeof("null")-1);` |
|        - | 11724 | `		}` |
|        - | 11725 | `		/* All done */` |
|        9 | 11726 | `		return PH7_OK;` |
|        1 | 11727 |  |
|        - | 11728 | `/*` |
|        - | 11729 | ` * The following walker callback is invoked each time we need` |
|        - | 11730 | ` * to encode an array to JSON.` |
|        - | 11731 | ` */` |
|        6 | 11732 | `static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 11733 |  |
|        7 | 11734 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|        7 | 11735 | `	if( pJson->nRecCount > 31 ){` |
|        - | 11736 | `		/* Recursion limit reached,return immediately */` |
|      ! 0 | 11737 | `		return PH7_OK;` |
|        - | 11738 | `	}` |
|        7 | 11739 | `	if( !pJson->isFirst ){` |
|        - | 11740 | `		/* Append the colon first */` |
|        5 | 11741 | `		ph7_result_string(pJson->pCtx,",",(int)sizeof(char));` |
|        2 | 11742 | `	}` |
|        7 | 11743 | `	if( pJson->iFlags & JSON_FORCE_OBJECT ){` |
|        - | 11744 | `		/* Outputs an object rather than an array */` |
|        - | 11745 | `		const char *zKey;` |
|        - | 11746 | `		int nByte;` |
|        - | 11747 | `		/* Extract a string representation of the key */` |
|      ! 0 | 11748 | `		zKey = ph7_value_to_string(pKey,&nByte);` |
|        - | 11749 | `		/* Append the key and the double colon */` |
|      ! 0 | 11750 | `		ph7_result_string_format(pJson->pCtx,"\"%.*s\":",nByte,zKey);` |
|      ! 0 | 11751 | `	}` |
|        - | 11752 | `	/* Encode the value */` |
|        7 | 11753 | `	pJson->nRecCount++;` |
|        7 | 11754 | `	VmJsonEncode(pValue,pJson);` |
|        7 | 11755 | `	pJson->nRecCount--;` |
|        7 | 11756 | `	pJson->isFirst = 0;` |
|        7 | 11757 | `	return PH7_OK;` |
|        4 | 11758 |  |
|        - | 11759 | `/*` |
|        - | 11760 | ` * The following walker callback is invoked each time we need to encode` |
|        - | 11761 | ` * a class instance [i.e: Object in the PHP jargon] to JSON.` |
|        - | 11762 | ` */` |
|      ! 0 | 11763 | `static int VmJsonObjectEncode(const char *zAttr,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 11764 |  |
|      ! 0 | 11765 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|      ! 0 | 11766 | `	if( pJson->nRecCount > 31 ){` |
|        - | 11767 | `		/* Recursion limit reached,return immediately */` |
|      ! 0 | 11768 | `		return PH7_OK;` |
|        - | 11769 | `	}` |
|      ! 0 | 11770 | `	if( !pJson->isFirst ){` |
|        - | 11771 | `		/* Append the colon first */` |
|      ! 0 | 11772 | `		ph7_result_string(pJson->pCtx,",",(int)sizeof(char));` |
|      ! 0 | 11773 | `	}` |
|        - | 11774 | `	/* Append the attribute name and the double colon first */` |
|      ! 0 | 11775 | `	ph7_result_string_format(pJson->pCtx,"\"%s\":",zAttr);` |
|        - | 11776 | `	/* Encode the value */` |
|      ! 0 | 11777 | `	pJson->nRecCount++;` |
|      ! 0 | 11778 | `	VmJsonEncode(pValue,pJson);` |
|      ! 0 | 11779 | `	pJson->nRecCount--;` |
|      ! 0 | 11780 | `	pJson->isFirst = 0;` |
|      ! 0 | 11781 | `	return PH7_OK;` |
|      ! 0 | 11782 |  |
|        - | 11783 | `/*` |
|        - | 11784 | ` * string json_encode(mixed $value [, int $options = 0 ])` |
|        - | 11785 | ` *  Returns a string containing the JSON representation of value.` |
|        - | 11786 | ` * Parameters` |
|        - | 11787 | ` *  $value` |
|        - | 11788 | ` *  The value being encoded. Can be any type except a resource.` |
|        - | 11789 | ` * $options` |
|        - | 11790 | ` *  Bitmask consisting of:` |
|        - | 11791 | ` *  JSON_HEX_TAG   All < and > are converted to \u003C and \u003E.` |
|        - | 11792 | ` *  JSON_HEX_AMP   All &s are converted to \u0026.` |
|        - | 11793 | ` *  JSON_HEX_APOS  All ' are converted to \u0027.` |
|        - | 11794 | ` *  JSON_HEX_QUOT  All " are converted to \u0022.` |
|        - | 11795 | ` *  JSON_FORCE_OBJECT  Outputs an object rather than an array.` |
|        - | 11796 | ` *  JSON_NUMERIC_CHECK Encodes numeric strings as numbers.` |
|        - | 11797 | ` *  JSON_BIGINT_AS_STRING   Not used` |
|        - | 11798 | ` *  JSON_PRETTY_PRINT       Use whitespace in returned data to format it.` |
|        - | 11799 | ` *  JSON_UNESCAPED_SLASHES  Don't escape '/'` |
|        - | 11800 | ` *  JSON_UNESCAPED_UNICODE  Not used.` |
|        - | 11801 | ` * Return` |
|        - | 11802 | ` *  Returns a JSON encoded string on success. FALSE otherwise` |
|        - | 11803 | ` */` |
|        2 | 11804 | `static int vm_builtin_json_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11805 |  |
|        - | 11806 | `	json_private_data sJson;` |
|        3 | 11807 | `	if( nArg < 1 ){` |
|        - | 11808 | `		/* Missing arguments,return FALSE */` |
|      ! 0 | 11809 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11810 | `		return PH7_OK;` |
|        - | 11811 | `	}` |
|        - | 11812 | `	/* Prepare the JSON data */` |
|        3 | 11813 | `	sJson.nRecCount = 0;` |
|        3 | 11814 | `	sJson.pCtx = pCtx;` |
|        3 | 11815 | `	sJson.isFirst = 1;` |
|        3 | 11816 | `	sJson.iFlags = 0;` |
|        3 | 11817 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|        - | 11818 | `		/* Extract option flags */` |
|      ! 0 | 11819 | `		sJson.iFlags = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 11820 | `	}` |
|        - | 11821 | `	/* Perform the encoding operation */` |
|        3 | 11822 | `	VmJsonEncode(apArg[0],&sJson);` |
|        - | 11823 | `	/* All done */` |
|        3 | 11824 | `	return PH7_OK;` |
|        2 | 11825 |  |
|        - | 11826 | `/*` |
|        - | 11827 | ` * int json_last_error(void)` |
|        - | 11828 | ` *  Returns the last error (if any) occurred during the last JSON encoding/decoding.` |
|        - | 11829 | ` * Parameters` |
|        - | 11830 | ` *  None` |
|        - | 11831 | ` * Return` |
|        - | 11832 | ` *  Returns an integer, the value can be one of the following constants:` |
|        - | 11833 | ` *  JSON_ERROR_NONE            No error has occurred.` |
|        - | 11834 | ` *  JSON_ERROR_DEPTH           The maximum stack depth has been exceeded.` |
|        - | 11835 | ` *  JSON_ERROR_STATE_MISMATCH  Invalid or malformed JSON.` |
|        - | 11836 | ` *  JSON_ERROR_CTRL_CHAR  	   Control character error, possibly incorrectly encoded.` |
|        - | 11837 | ` *  JSON_ERROR_SYNTAX          Syntax error.` |
|        - | 11838 | ` *  JSON_ERROR_UTF8_CHECK      Malformed UTF-8 characters.` |
|        - | 11839 | ` */` |
|        8 | 11840 | `static int vm_builtin_json_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11841 |  |
|       10 | 11842 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11843 | `	/* Return the error code */` |
|       10 | 11844 | `	ph7_result_int(pCtx,pVm->json_rc);` |
|        4 | 11845 | `	SXUNUSED(nArg); /* cc warning */` |
|        4 | 11846 | `	SXUNUSED(apArg);` |
|       10 | 11847 | `	return PH7_OK;` |
|        2 | 11848 |  |
|        - | 11849 | `/* Possible tokens from the JSON tokenization process */` |
|        - | 11850 | `#define JSON_TK_TRUE    0x001 /* Boolean true */` |
|        - | 11851 | `#define JSON_TK_FALSE   0x002 /* Boolean false */` |
|        - | 11852 | `#define JSON_TK_STR     0x004 /* String enclosed in double quotes */` |
|        - | 11853 | `#define JSON_TK_NULL    0x008 /* null */` |
|        - | 11854 | `#define JSON_TK_NUM     0x010 /* Numeric */` |
|        - | 11855 | `#define JSON_TK_OCB     0x020 /* Open curly braces '{' */` |
|        - | 11856 | `#define JSON_TK_CCB     0x040 /* Closing curly braces '}' */` |
|        - | 11857 | `#define JSON_TK_OSB     0x080 /* Open square bracke '[' */` |
|        - | 11858 | `#define JSON_TK_CSB     0x100 /* Closing square bracket ']' */` |
|        - | 11859 | `#define JSON_TK_COLON   0x200 /* Single colon ':' */` |
|        - | 11860 | `#define JSON_TK_COMMA   0x400 /* Single comma ',' */` |
|        - | 11861 | `#define JSON_TK_INVALID 0x800 /* Unexpected token */` |
|        - | 11862 | `/*` |
|        - | 11863 | ` * Tokenize an entire JSON input.` |
|        - | 11864 | ` * Get a single low-level token from the input file.` |
|        - | 11865 | ` * Update the stream pointer so that it points to the first` |
|        - | 11866 | ` * character beyond the extracted token.` |
|        - | 11867 | ` */` |
|       60 | 11868 | `static sxi32 VmJsonTokenize(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|        2 | 11869 |  |
|       62 | 11870 | `	int *pJsonErr = (int *)pUserData;` |
|        - | 11871 | `	SyString *pStr;` |
|        - | 11872 | `	int c;` |
|        - | 11873 | `	/* Ignore leading white spaces */` |
|       66 | 11874 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|        - | 11875 | `		/* Advance the stream cursor */` |
|        6 | 11876 | `		if( pStream->zText[0] == '\n' ){` |
|        - | 11877 | `			/* Update line counter */` |
|      ! 0 | 11878 | `			pStream->nLine++;` |
|      ! 0 | 11879 | `		}` |
|        6 | 11880 | `		pStream->zText++;` |
|        2 | 11881 | `	}` |
|       62 | 11882 | `	if( pStream->zText >= pStream->zEnd ){` |
|        - | 11883 | `		/* End of input reached */` |
|      ! 0 | 11884 | `		SXUNUSED(pCtxData); /* cc warning */` |
|      ! 0 | 11885 | `		return SXERR_EOF;` |
|        - | 11886 | `	}` |
|        - | 11887 | `	/* Record token starting position and line */` |
|       62 | 11888 | `	pToken->nLine = pStream->nLine;` |
|       62 | 11889 | `	pToken->pUserData = 0;` |
|       62 | 11890 | `	pStr = &pToken->sData;` |
|       62 | 11891 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
|       77 | 11892 | `	if( pStream->zText[0] == '{' \|\| pStream->zText[0] == '[' \|\| pStream->zText[0] == '}' \|\| pStream->zText[0] == ']'` |
|       44 | 11893 | `		\|\| pStream->zText[0] == ':' \|\| pStream->zText[0] == ',' ){` |
|        - | 11894 | `			/* Single character */` |
|       36 | 11895 | `			c = pStream->zText[0];` |
|        - | 11896 | `			/* Set token type */` |
|       36 | 11897 | `			switch(c){` |
|        5 | 11898 | `			case '[': pToken->nType = JSON_TK_OSB;   break;` |
|       10 | 11899 | `			case '{': pToken->nType = JSON_TK_OCB;   break;` |
|        6 | 11900 | `			case '}': pToken->nType = JSON_TK_CCB;   break;` |
|        5 | 11901 | `			case ']': pToken->nType = JSON_TK_CSB;   break;` |
|        8 | 11902 | `			case ':': pToken->nType = JSON_TK_COLON; break;` |
|        9 | 11903 | `			case ',': pToken->nType = JSON_TK_COMMA; break;` |
|      ! 0 | 11904 | `			default:` |
|      ! 0 | 11905 | `				break;` |
|        - | 11906 | `			}` |
|        - | 11907 | `			/* Advance the stream cursor */` |
|       36 | 11908 | `			pStream->zText++;` |
|       45 | 11909 | `	}else if( pStream->zText[0] == '"') {` |
|        - | 11910 | `		/* JSON string */` |
|       10 | 11911 | `		pStream->zText++;` |
|       10 | 11912 | `		pStr->zString++;` |
|        - | 11913 | `		/* Delimit the string */` |
|       32 | 11914 | `		while( pStream->zText < pStream->zEnd ){` |
|       32 | 11915 | `			if( pStream->zText[0] == '"' && pStream->zText[-1] != '\\' ){` |
|       10 | 11916 | `				break;` |
|        - | 11917 | `			}` |
|       24 | 11918 | `			if( pStream->zText[0] == '\n' ){` |
|        - | 11919 | `				/* Update line counter */` |
|      ! 0 | 11920 | `				pStream->nLine++;` |
|      ! 0 | 11921 | `			}` |
|       24 | 11922 | `			pStream->zText++;` |
|        2 | 11923 | `		}` |
|       10 | 11924 | `		if( pStream->zText >= pStream->zEnd ){` |
|        - | 11925 | `			/* Missing closing '"' */` |
|      ! 0 | 11926 | `			pToken->nType = JSON_TK_INVALID;` |
|      ! 0 | 11927 | `			*pJsonErr = JSON_ERROR_SYNTAX;` |
|      ! 0 | 11928 | `		}else{` |
|       10 | 11929 | `			pToken->nType = JSON_TK_STR;` |
|       10 | 11930 | `			pStream->zText++; /* Jump the closing double quotes */` |
|        2 | 11931 | `		}` |
|       24 | 11932 | `	}else if( pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|        - | 11933 | `		/* Number */` |
|       13 | 11934 | `		pStream->zText++;` |
|       13 | 11935 | `		pToken->nType = JSON_TK_NUM;` |
|       13 | 11936 | `		while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      ! 0 | 11937 | `			pStream->zText++;` |
|      ! 0 | 11938 | `		}` |
|       13 | 11939 | `		if( pStream->zText < pStream->zEnd ){` |
|       13 | 11940 | `			c = pStream->zText[0];` |
|       13 | 11941 | `			if( c == '.' ){` |
|        - | 11942 | `					/* Real number */` |
|      ! 0 | 11943 | `					pStream->zText++;` |
|      ! 0 | 11944 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      ! 0 | 11945 | `						pStream->zText++;` |
|      ! 0 | 11946 | `					}` |
|      ! 0 | 11947 | `					if( pStream->zText < pStream->zEnd ){` |
|      ! 0 | 11948 | `						c = pStream->zText[0];` |
|      ! 0 | 11949 | `						if( c=='e' \|\| c=='E' ){` |
|      ! 0 | 11950 | `							pStream->zText++;` |
|      ! 0 | 11951 | `							if( pStream->zText < pStream->zEnd ){` |
|      ! 0 | 11952 | `								c = pStream->zText[0];` |
|      ! 0 | 11953 | `								if( c =='+' \|\| c=='-' ){` |
|      ! 0 | 11954 | `									pStream->zText++;` |
|      ! 0 | 11955 | `								}` |
|      ! 0 | 11956 | `								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      ! 0 | 11957 | `									pStream->zText++;` |
|      ! 0 | 11958 | `								}` |
|      ! 0 | 11959 | `							}` |
|      ! 0 | 11960 | `						}` |
|      ! 0 | 11961 | `					}` |
|       13 | 11962 | `				}else if( c=='e' \|\| c=='E' ){` |
|        - | 11963 | `					/* Real number */` |
|      ! 0 | 11964 | `					pStream->zText++;` |
|      ! 0 | 11965 | `					if( pStream->zText < pStream->zEnd ){` |
|      ! 0 | 11966 | `						c = pStream->zText[0];` |
|      ! 0 | 11967 | `						if( c =='+' \|\| c=='-' ){` |
|      ! 0 | 11968 | `							pStream->zText++;` |
|      ! 0 | 11969 | `						}` |
|      ! 0 | 11970 | `						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      ! 0 | 11971 | `							pStream->zText++;` |
|      ! 0 | 11972 | `						}` |
|      ! 0 | 11973 | `					}` |
|      ! 0 | 11974 | `				}` |
|        7 | 11975 | `			}` |
|       17 | 11976 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("true") -1 &&` |
|        6 | 11977 | `		SyStrnicmp((const char *)pStream->zText,"true",sizeof("true")-1) == 0 ){` |
|        - | 11978 | `			/* boolean true */` |
|      ! 0 | 11979 | `			pToken->nType = JSON_TK_TRUE;` |
|        - | 11980 | `			/* Advance the stream cursor */` |
|      ! 0 | 11981 | `			pStream->zText += sizeof("true")-1;` |
|       11 | 11982 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("false") -1 &&` |
|        6 | 11983 | `		SyStrnicmp((const char *)pStream->zText,"false",sizeof("false")-1) == 0 ){` |
|        - | 11984 | `			/* boolean false */` |
|      ! 0 | 11985 | `			pToken->nType = JSON_TK_FALSE;` |
|        - | 11986 | `			/* Advance the stream cursor */` |
|      ! 0 | 11987 | `			pStream->zText += sizeof("false")-1;` |
|       11 | 11988 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("null") -1 &&` |
|        6 | 11989 | `		SyStrnicmp((const char *)pStream->zText,"null",sizeof("null")-1) == 0 ){` |
|        - | 11990 | `			/* NULL */` |
|      ! 0 | 11991 | `			pToken->nType = JSON_TK_NULL;` |
|        - | 11992 | `			/* Advance the stream cursor */` |
|      ! 0 | 11993 | `			pStream->zText += sizeof("null")-1;` |
|      ! 0 | 11994 | `	}else{` |
|        - | 11995 | `		/* Unexpected token */` |
|        8 | 11996 | `		pToken->nType = JSON_TK_INVALID;` |
|        - | 11997 | `		/* Advance the stream cursor */` |
|        8 | 11998 | `		pStream->zText++;` |
|        8 | 11999 | `		*pJsonErr = JSON_ERROR_SYNTAX;` |
|        - | 12000 | `		/* Abort processing immediatley */` |
|        8 | 12001 | `		return SXERR_ABORT;` |
|        - | 12002 | `	}` |
|        - | 12003 | `	/* record token length */` |
|       56 | 12004 | `	pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|       56 | 12005 | `	if( pToken->nType == JSON_TK_STR ){` |
|       10 | 12006 | `		pStr->nByte--;` |
|        4 | 12007 | `	}` |
|        - | 12008 | `	/* Return to the lexer */` |
|       56 | 12009 | `	return SXRET_OK;` |
|       32 | 12010 |  |
|        - | 12011 | `/*` |
|        - | 12012 | ` * JSON decoded input consumer callback signature.` |
|        - | 12013 | ` */` |
|        - | 12014 | `typedef int (*ProcJsonConsumer)(ph7_context *,ph7_value *,ph7_value *,void *);` |
|        - | 12015 | `/*` |
|        - | 12016 | ` * JSON decoder state is kept in the following structure.` |
|        - | 12017 | ` */` |
|        - | 12018 | `typedef struct json_decoder json_decoder;` |
|        - | 12019 | `struct json_decoder` |
|        - | 12020 |  |
|        - | 12021 | `	ph7_context *pCtx; /* Call context */` |
|        - | 12022 | `	ProcJsonConsumer xConsumer; /* Consumer callback */` |
|        - | 12023 | `	void *pUserData;   /* Last argument to xConsumer() */` |
|        - | 12024 | `	int iFlags;        /* Configuration flags */` |
|        - | 12025 | `	SyToken *pIn;      /* Token stream */` |
|        - | 12026 | `	SyToken *pEnd;     /* End of the token stream */` |
|        - | 12027 | `	int rec_depth;     /* Recursion limit */` |
|        - | 12028 | `	int rec_count;     /* Current nesting level */` |
|        - | 12029 | `	int *pErr;         /* JSON decoding error if any */` |
|        - | 12030 | `};` |
|        - | 12031 | `#define JSON_DECODE_ASSOC 0x01 /* Decode a JSON object as an associative array */` |
|        - | 12032 | `/* Forward declaration */` |
|        - | 12033 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData);` |
|        - | 12034 | `/*` |
|        - | 12035 | ` * Dequote [i.e: Resolve all backslash escapes ] a JSON string and store` |
|        - | 12036 | ` * the result in the given ph7_value.` |
|        - | 12037 | ` */` |
|        8 | 12038 | `static void VmJsonDequoteString(const SyString *pStr,ph7_value *pWorker)` |
|        2 | 12039 |  |
|       10 | 12040 | `	const char *zIn = pStr->zString;` |
|       10 | 12041 | `	const char *zEnd = &pStr->zString[pStr->nByte];` |
|        - | 12042 | `	const char *zCur;` |
|        - | 12043 | `	int c;` |
|        - | 12044 | `	/* Mark the value as a string */` |
|       10 | 12045 | `	ph7_value_string(pWorker,"",0); /* Empty string */` |
|        4 | 12046 | `	for(;;){` |
|       10 | 12047 | `		zCur = zIn;` |
|       32 | 12048 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|       24 | 12049 | `			zIn++;` |
|        2 | 12050 | `		}` |
|       10 | 12051 | `		if( zIn > zCur ){` |
|        - | 12052 | `			/* Append chunk verbatim */` |
|       10 | 12053 | `			ph7_value_string(pWorker,zCur,(int)(zIn-zCur));` |
|        4 | 12054 | `		}` |
|       10 | 12055 | `		zIn++;` |
|       10 | 12056 | `		if( zIn >= zEnd ){` |
|        - | 12057 | `			/* End of the input reached */` |
|       10 | 12058 | `			break;` |
|        - | 12059 | `		}` |
|      ! 0 | 12060 | `		c = zIn[0];` |
|        - | 12061 | `		/* Unescape the character */` |
|      ! 0 | 12062 | `		switch(c){` |
|      ! 0 | 12063 | `		case '"':  ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|      ! 0 | 12064 | `		case '\\': ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|      ! 0 | 12065 | `		case 'n':  ph7_value_string(pWorker,"\n",(int)sizeof(char)); break;` |
|      ! 0 | 12066 | `		case 'r':  ph7_value_string(pWorker,"\r",(int)sizeof(char)); break;` |
|      ! 0 | 12067 | `		case 't':  ph7_value_string(pWorker,"\t",(int)sizeof(char)); break;` |
|      ! 0 | 12068 | `		case 'f':  ph7_value_string(pWorker,"\f",(int)sizeof(char)); break;` |
|      ! 0 | 12069 | `		default:` |
|      ! 0 | 12070 | `			ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char));` |
|      ! 0 | 12071 | `			break;` |
|        - | 12072 | `		}` |
|        - | 12073 | `		/* Advance the stream cursor */` |
|      ! 0 | 12074 | `		zIn++;` |
|      ! 0 | 12075 | `	}` |
|       10 | 12076 |  |
|        - | 12077 | `/*` |
|        - | 12078 | ` * Returns a ph7_value holding the image of a JSON string. In other word perform a JSON decoding operation.` |
|        - | 12079 | ` * According to wikipedia` |
|        - | 12080 | ` * JSON's basic types are:` |
|        - | 12081 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|        - | 12082 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|        - | 12083 | ` *   Boolean (true or false)` |
|        - | 12084 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|        - | 12085 | ` *    do not need to be of the same type)` |
|        - | 12086 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|        - | 12087 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|        - | 12088 | ` *     be distinct from each other)` |
|        - | 12089 | ` *   null (empty)` |
|        - | 12090 | ` * Non-significant white space may be added freely around the "structural characters" (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|        - | 12091 | ` */` |
|       24 | 12092 | `static sxi32 VmJsonDecode(` |
|        - | 12093 | `	json_decoder *pDecoder, /* JSON decoder */` |
|        - | 12094 | `	ph7_value *pArrayKey    /* Key for the decoded array */` |
|        2 | 12095 | `	){` |
|        - | 12096 | `	ph7_value *pWorker; /* Worker variable */` |
|        - | 12097 | `	sxi32 rc;` |
|        - | 12098 | `	/* Check if we do not nest to much */` |
|       26 | 12099 | `	if( pDecoder->rec_count >= pDecoder->rec_depth ){` |
|        - | 12100 | `		/* Nesting limit reached,abort decoding immediately */` |
|      ! 0 | 12101 | `		*pDecoder->pErr = JSON_ERROR_DEPTH;` |
|      ! 0 | 12102 | `		return SXERR_ABORT;` |
|        - | 12103 | `	}` |
|       26 | 12104 | `	if( pDecoder->pIn->nType & (JSON_TK_STR\|JSON_TK_TRUE\|JSON_TK_FALSE\|JSON_TK_NULL\|JSON_TK_NUM) ){` |
|        - | 12105 | `		/* Scalar value */` |
|       16 | 12106 | `		pWorker = ph7_context_new_scalar(pDecoder->pCtx);` |
|       16 | 12107 | `		if( pWorker == 0 ){` |
|      ! 0 | 12108 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - | 12109 | `			/* Abort the decoding operation immediately */` |
|      ! 0 | 12110 | `			return SXERR_ABORT;` |
|        - | 12111 | `		}` |
|        - | 12112 | `		/* Reflect the JSON image */` |
|       16 | 12113 | `		if( pDecoder->pIn->nType & JSON_TK_NULL ){` |
|        - | 12114 | `			/* Nullify the value.*/` |
|      ! 0 | 12115 | `			ph7_value_null(pWorker);` |
|       16 | 12116 | `		}else if( pDecoder->pIn->nType & (JSON_TK_TRUE\|JSON_TK_FALSE) ){` |
|        - | 12117 | `			/* Boolean value */` |
|      ! 0 | 12118 | `			ph7_value_bool(pWorker,(pDecoder->pIn->nType & JSON_TK_TRUE) ? 1 : 0 );` |
|       16 | 12119 | `		}else if( pDecoder->pIn->nType & JSON_TK_NUM ){` |
|       13 | 12120 | `			SyString *pStr = &pDecoder->pIn->sData;` |
|        - | 12121 | `			/*` |
|        - | 12122 | `			 * Numeric value.` |
|        - | 12123 | `			 * Get a string representation first then try to get a numeric` |
|        - | 12124 | `			 * value.` |
|        - | 12125 | `			 */` |
|       13 | 12126 | `			ph7_value_string(pWorker,pStr->zString,(int)pStr->nByte);` |
|        - | 12127 | `			/* Obtain a numeric representation */` |
|       13 | 12128 | `			PH7_MemObjToNumeric(pWorker);` |
|        7 | 12129 | `		}else{` |
|        - | 12130 | `			/* Dequote the string */` |
|        3 | 12131 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pWorker);` |
|        - | 12132 | `		}` |
|        - | 12133 | `		/* Invoke the consumer callback */` |
|       16 | 12134 | `		rc = pDecoder->xConsumer(pDecoder->pCtx,pArrayKey,pWorker,pDecoder->pUserData);` |
|       16 | 12135 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 12136 | `			return SXERR_ABORT;` |
|        - | 12137 | `		}` |
|        - | 12138 | `		/* All done,advance the stream cursor */` |
|       16 | 12139 | `		pDecoder->pIn++;` |
|       19 | 12140 | `	}else if( pDecoder->pIn->nType & JSON_TK_OSB /*'[' */) {` |
|        - | 12141 | `		ProcJsonConsumer xOld;` |
|        - | 12142 | `		void *pOld;` |
|        - | 12143 | `		/* Array representation*/` |
|        5 | 12144 | `		pDecoder->pIn++;` |
|        - | 12145 | `		/* Create a working array */` |
|        5 | 12146 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|        5 | 12147 | `		if( pWorker == 0 ){` |
|      ! 0 | 12148 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - | 12149 | `			/* Abort the decoding operation immediately */` |
|      ! 0 | 12150 | `			return SXERR_ABORT;` |
|        - | 12151 | `		}` |
|        - | 12152 | `		/* Save the old consumer */` |
|        5 | 12153 | `		xOld = pDecoder->xConsumer;` |
|        5 | 12154 | `		pOld = pDecoder->pUserData;` |
|        - | 12155 | `		/* Set the new consumer */` |
|        5 | 12156 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|        5 | 12157 | `		pDecoder->pUserData = pWorker;` |
|        - | 12158 | `		/* Decode the array */` |
|        7 | 12159 | `		for(;;){` |
|        - | 12160 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|        - | 12161 | `			 * do this.` |
|        - | 12162 | `			 */` |
|       21 | 12163 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|        7 | 12164 | `				pDecoder->pIn++;` |
|        1 | 12165 | `			}` |
|       15 | 12166 | `			if( pDecoder->pIn >= pDecoder->pEnd \|\| (pDecoder->pIn->nType & JSON_TK_CSB) /*']'*/ ){` |
|        5 | 12167 | `				if( pDecoder->pIn < pDecoder->pEnd ){` |
|        5 | 12168 | `					pDecoder->pIn++; /* Jump the trailing ']' */` |
|        2 | 12169 | `				}` |
|        5 | 12170 | `				break;` |
|        - | 12171 | `			}` |
|        - | 12172 | `			/* Recurse and decode the entry */` |
|       11 | 12173 | `			pDecoder->rec_count++;` |
|       11 | 12174 | `			rc = VmJsonDecode(pDecoder,0);` |
|       11 | 12175 | `			pDecoder->rec_count--;` |
|       11 | 12176 | `			if( rc == SXERR_ABORT ){` |
|        - | 12177 | `				/* Abort processing immediately */` |
|      ! 0 | 12178 | `				return SXERR_ABORT;` |
|        - | 12179 | `			}` |
|        - | 12180 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|       11 | 12181 | `			if( (pDecoder->pIn < pDecoder->pEnd) &&` |
|       10 | 12182 | `				((pDecoder->pIn->nType & (JSON_TK_CSB/*']'*/\|JSON_TK_COMMA/*','*/))==0) ){` |
|        - | 12183 | `					/* Unexpected token,abort immediatley */` |
|      ! 0 | 12184 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|      ! 0 | 12185 | `					return SXERR_ABORT;` |
|        - | 12186 | `			}` |
|        1 | 12187 | `		}` |
|        - | 12188 | `		/* Restore the old consumer */` |
|        5 | 12189 | `		pDecoder->xConsumer = xOld;` |
|        5 | 12190 | `		pDecoder->pUserData = pOld;` |
|        - | 12191 | `		/* Invoke the old consumer on the decoded array */` |
|        5 | 12192 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|       10 | 12193 | `	}else if( pDecoder->pIn->nType & JSON_TK_OCB /*'{' */) {` |
|        - | 12194 | `		ProcJsonConsumer xOld;` |
|        - | 12195 | `		ph7_value *pKey;` |
|        - | 12196 | `		void *pOld;` |
|        - | 12197 | `		/* Object representation*/` |
|        8 | 12198 | `		pDecoder->pIn++;` |
|        - | 12199 | `		/* Return the object as an associative array */` |
|        8 | 12200 | `		if( (pDecoder->iFlags & JSON_DECODE_ASSOC) == 0 ){` |
|        3 | 12201 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_WARNING,` |
|        - | 12202 | `				"JSON Objects are always returned as an associative array"` |
|        - | 12203 | `				);` |
|        1 | 12204 | `		}` |
|        - | 12205 | `		/* Create a working array */` |
|        8 | 12206 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|        8 | 12207 | `		pKey = ph7_context_new_scalar(pDecoder->pCtx);` |
|        8 | 12208 | `		if( pWorker == 0 \|\| pKey == 0){` |
|      ! 0 | 12209 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - | 12210 | `			/* Abort the decoding operation immediately */` |
|      ! 0 | 12211 | `			return SXERR_ABORT;` |
|        - | 12212 | `		}` |
|        - | 12213 | `		/* Save the old consumer */` |
|        8 | 12214 | `		xOld = pDecoder->xConsumer;` |
|        8 | 12215 | `		pOld = pDecoder->pUserData;` |
|        - | 12216 | `		/* Set the new consumer */` |
|        8 | 12217 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|        8 | 12218 | `		pDecoder->pUserData = pWorker;` |
|        - | 12219 | `		/* Decode the object */` |
|        6 | 12220 | `		for(;;){` |
|        - | 12221 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|        - | 12222 | `			 * do this.` |
|        - | 12223 | `			 */` |
|       16 | 12224 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|        3 | 12225 | `				pDecoder->pIn++;` |
|        1 | 12226 | `			}` |
|       14 | 12227 | `			if( pDecoder->pIn >= pDecoder->pEnd \|\| (pDecoder->pIn->nType & JSON_TK_CCB) /*'}'*/ ){` |
|        8 | 12228 | `				if( pDecoder->pIn < pDecoder->pEnd ){` |
|        6 | 12229 | `					pDecoder->pIn++; /* Jump the trailing ']' */` |
|        2 | 12230 | `				}` |
|        8 | 12231 | `				break;` |
|        - | 12232 | `			}` |
|        6 | 12233 | `			if( (pDecoder->pIn->nType & JSON_TK_STR) == 0 \|\| &pDecoder->pIn[1] >= pDecoder->pEnd` |
|        8 | 12234 | `				\|\| (pDecoder->pIn[1].nType & JSON_TK_COLON) == 0){` |
|        - | 12235 | `					/* Syntax error,return immediately */` |
|      ! 0 | 12236 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|      ! 0 | 12237 | `					return SXERR_ABORT;` |
|        - | 12238 | `			}` |
|        - | 12239 | `			/* Dequote the key */` |
|        8 | 12240 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pKey);` |
|        - | 12241 | `			/* Jump the key and the colon */` |
|        8 | 12242 | `			pDecoder->pIn += 2;` |
|        - | 12243 | `			/* Recurse and decode the value */` |
|        8 | 12244 | `			pDecoder->rec_count++;` |
|        8 | 12245 | `			rc = VmJsonDecode(pDecoder,pKey);` |
|        8 | 12246 | `			pDecoder->rec_count--;` |
|        8 | 12247 | `			if( rc == SXERR_ABORT ){` |
|        - | 12248 | `				/* Abort processing immediately */` |
|      ! 0 | 12249 | `				return SXERR_ABORT;` |
|        - | 12250 | `			}` |
|        - | 12251 | `			/* Reset the internal buffer of the key */` |
|        8 | 12252 | `			ph7_value_reset_string_cursor(pKey);` |
|        - | 12253 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|        2 | 12254 | `		}` |
|        - | 12255 | `		/* Restore the old consumer */` |
|        8 | 12256 | `		pDecoder->xConsumer = xOld;` |
|        8 | 12257 | `		pDecoder->pUserData = pOld;` |
|        - | 12258 | `		/* Invoke the old consumer on the decoded object*/` |
|        8 | 12259 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|        - | 12260 | `		/* Release the key */` |
|        8 | 12261 | `		ph7_context_release_value(pDecoder->pCtx,pKey);` |
|        5 | 12262 | `	}else{` |
|        - | 12263 | `		/* Unexpected token */` |
|      ! 0 | 12264 | `		return SXERR_ABORT; /* Abort immediately */` |
|        - | 12265 | `	}` |
|        - | 12266 | `	/* Release the worker variable */` |
|       26 | 12267 | `	ph7_context_release_value(pDecoder->pCtx,pWorker);` |
|       26 | 12268 | `	return SXRET_OK;` |
|       14 | 12269 |  |
|        - | 12270 | `/*` |
|        - | 12271 | ` * The following JSON decoder callback is invoked each time` |
|        - | 12272 | ` * a JSON array representation [i.e: [15,"hello",FALSE] ]` |
|        - | 12273 | ` * is being decoded.` |
|        - | 12274 | ` */` |
|       16 | 12275 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|        2 | 12276 |  |
|       18 | 12277 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 12278 | `	/* Insert the entry */` |
|       18 | 12279 | `	ph7_array_add_elem(pArray,pKey,pWorker); /* Will make it's own copy */` |
|        8 | 12280 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 12281 | `	/* All done */` |
|       18 | 12282 | `	return SXRET_OK;` |
|        2 | 12283 |  |
|        - | 12284 | `/*` |
|        - | 12285 | ` * Standard JSON decoder callback.` |
|        - | 12286 | ` */` |
|        8 | 12287 | `static int VmJsonDefaultDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|        2 | 12288 |  |
|        - | 12289 | `	/* Return the value directly */` |
|       10 | 12290 | `	ph7_result_value(pCtx,pWorker); /* Will make it's own copy */` |
|        4 | 12291 | `	SXUNUSED(pKey); /* cc warning */` |
|        4 | 12292 | `	SXUNUSED(pUserData);` |
|        - | 12293 | `	/* All done */` |
|       10 | 12294 | `	return SXRET_OK;` |
|        2 | 12295 |  |
|        - | 12296 | `/*` |
|        - | 12297 | ` * mixed json_decode(string $json[,bool $assoc = false[,int $depth = 32[,int $options = 0 ]]])` |
|        - | 12298 | ` *  Takes a JSON encoded string and converts it into a PHP variable.` |
|        - | 12299 | ` * Parameters` |
|        - | 12300 | ` *  $json` |
|        - | 12301 | ` *    The json string being decoded.` |
|        - | 12302 | ` * $assoc` |
|        - | 12303 | ` *   When TRUE, returned objects will be converted into associative arrays.` |
|        - | 12304 | ` * $depth` |
|        - | 12305 | ` *   User specified recursion depth.` |
|        - | 12306 | ` * $options` |
|        - | 12307 | ` *   Bitmask of JSON decode options. Currently only JSON_BIGINT_AS_STRING is supported` |
|        - | 12308 | ` * (default is to cast large integers as floats)` |
|        - | 12309 | ` * Return` |
|        - | 12310 | ` *  The value encoded in json in appropriate PHP type. Values true, false and null (case-insensitive)` |
|        - | 12311 | ` *  are returned as TRUE, FALSE and NULL respectively. NULL is returned if the json cannot be decoded` |
|        - | 12312 | ` *  or if the encoded data is deeper than the recursion limit.` |
|        - | 12313 | ` */` |
|       16 | 12314 | `static int vm_builtin_json_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12315 |  |
|       18 | 12316 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12317 | `	json_decoder sDecoder;` |
|        - | 12318 | `	const char *zIn;` |
|        - | 12319 | `	SySet sToken;` |
|        - | 12320 | `	SyLex sLex;` |
|        - | 12321 | `	int nByte;` |
|        - | 12322 | `	sxi32 rc;` |
|       18 | 12323 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 12324 | `		/* Missing/Invalid arguments, return NULL */` |
|        3 | 12325 | `		ph7_result_null(pCtx);` |
|        3 | 12326 | `		return PH7_OK;` |
|        - | 12327 | `	}` |
|        - | 12328 | `	/* Extract the JSON string */` |
|       16 | 12329 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|       16 | 12330 | `	if( nByte < 1 ){` |
|        - | 12331 | `		/* Empty string,return NULL */` |
|      ! 0 | 12332 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12333 | `		return PH7_OK;` |
|        - | 12334 | `	}` |
|        - | 12335 | `	/* Clear JSON error code */` |
|       16 | 12336 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - | 12337 | `	/* Tokenize the input */` |
|       16 | 12338 | `	SySetInit(&sToken,&pVm->sAllocator,sizeof(SyToken));` |
|       16 | 12339 | `	SyLexInit(&sLex,&sToken,VmJsonTokenize,&pVm->json_rc);` |
|       16 | 12340 | `	SyLexTokenizeInput(&sLex,zIn,(sxu32)nByte,0,0,0);` |
|       16 | 12341 | `	if( pVm->json_rc != JSON_ERROR_NONE ){` |
|        - | 12342 | `		/* Something goes wrong while tokenizing input. [i.e: Unexpected token] */` |
|        8 | 12343 | `		SyLexRelease(&sLex);` |
|        8 | 12344 | `		SySetRelease(&sToken);` |
|        - | 12345 | `		/* return NULL */` |
|        8 | 12346 | `		ph7_result_null(pCtx);` |
|        8 | 12347 | `		return PH7_OK;` |
|        - | 12348 | `	}` |
|        - | 12349 | `	/* Fill the decoder */` |
|       10 | 12350 | `	sDecoder.pCtx = pCtx;` |
|       10 | 12351 | `	sDecoder.pErr = &pVm->json_rc;` |
|       10 | 12352 | `	sDecoder.pIn = (SyToken *)SySetBasePtr(&sToken);` |
|       10 | 12353 | `	sDecoder.pEnd = &sDecoder.pIn[SySetUsed(&sToken)];` |
|       10 | 12354 | `	sDecoder.iFlags = 0;` |
|       10 | 12355 | `	if( nArg > 1 && ph7_value_to_bool(apArg[1]) != 0 ){` |
|        - | 12356 | `		/* Returned objects will be converted into associative arrays */` |
|        8 | 12357 | `		sDecoder.iFlags \|= JSON_DECODE_ASSOC;` |
|        3 | 12358 | `	}` |
|       10 | 12359 | `	sDecoder.rec_depth = 32;` |
|       10 | 12360 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|      ! 0 | 12361 | `		int nDepth = ph7_value_to_int(apArg[2]);` |
|      ! 0 | 12362 | `		if( nDepth > 1 && nDepth < 32 ){` |
|      ! 0 | 12363 | `			sDecoder.rec_depth = nDepth;` |
|      ! 0 | 12364 | `		}` |
|      ! 0 | 12365 | `	}` |
|       10 | 12366 | `	sDecoder.rec_count = 0;` |
|        - | 12367 | `	/* Set a default consumer */` |
|       10 | 12368 | `	sDecoder.xConsumer = VmJsonDefaultDecoder;` |
|       10 | 12369 | `	sDecoder.pUserData = 0;` |
|        - | 12370 | `	/* Decode the raw JSON input */` |
|       10 | 12371 | `	rc = VmJsonDecode(&sDecoder,0);` |
|       10 | 12372 | `	if( rc == SXERR_ABORT \|\|  pVm->json_rc != JSON_ERROR_NONE ){` |
|        - | 12373 | `		/*` |
|        - | 12374 | `		 * Something goes wrong while decoding JSON input.Return NULL.` |
|        - | 12375 | `		 */` |
|      ! 0 | 12376 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12377 | `	}` |
|        - | 12378 | `	/* Clean-up the mess left behind */` |
|       10 | 12379 | `	SyLexRelease(&sLex);` |
|       10 | 12380 | `	SySetRelease(&sToken);` |
|        - | 12381 | `	/* All done */` |
|       10 | 12382 | `	return PH7_OK;` |
|       10 | 12383 |  |
|        - | 12384 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12385 | `/*` |
|        - | 12386 | ` * XML processing Functions.` |
|        - | 12387 | ` * Status:` |
|        - | 12388 | ` *    Devel.` |
|        - | 12389 | ` */` |
|        - | 12390 | `enum ph7_xml_handler_id{` |
|        - | 12391 | `	PH7_XML_START_TAG = 0, /* Start element handlers ID */` |
|        - | 12392 | `	PH7_XML_END_TAG,       /* End element handler ID*/` |
|        - | 12393 | `	PH7_XML_CDATA,         /* Character data handler ID*/` |
|        - | 12394 | `	PH7_XML_PI,            /* Processing instruction (PI) handler ID*/` |
|        - | 12395 | `	PH7_XML_DEF,           /* Default handler ID */` |
|        - | 12396 | `	PH7_XML_UNPED,         /* Unparsed entity declaration handler */` |
|        - | 12397 | `	PH7_XML_ND,            /* Notation declaration handler ID*/` |
|        - | 12398 | `	PH7_XML_EER,           /* External entity reference handler */` |
|        - | 12399 | `	PH7_XML_NS_START,      /* Start namespace declaration handler */` |
|        - | 12400 | `	PH7_XML_NS_END         /* End namespace declaration handler */` |
|        - | 12401 | `};` |
|        - | 12402 | `#define XML_TOTAL_HANDLER (PH7_XML_NS_END + 1)` |
|        - | 12403 | `/* An instance of the following structure describe a working` |
|        - | 12404 | ` * XML engine instance.` |
|        - | 12405 | ` */` |
|        - | 12406 | `typedef struct ph7_xml_engine ph7_xml_engine;` |
|        - | 12407 | `struct ph7_xml_engine` |
|        - | 12408 |  |
|        - | 12409 | `	ph7_vm *pVm;         /* VM that own this instance */` |
|        - | 12410 | `	ph7_context *pCtx;   /* Call context */` |
|        - | 12411 | `	SyXMLParser sParser; /* Underlying XML parser */` |
|        - | 12412 | `	ph7_value aCB[XML_TOTAL_HANDLER]; /* User-defined callbacks */` |
|        - | 12413 | `	ph7_value sParserValue; /* ph7_value holding this instance which is forwarded` |
|        - | 12414 | `							  * as the first argument to the user callbacks.` |
|        - | 12415 | `							  */` |
|        - | 12416 | `	int ns_sep;      /* Namespace separator */` |
|        - | 12417 | `	SyBlob sErr;     /* Error message consumer */` |
|        - | 12418 | `	sxi32 iErrCode;  /* Last error code */` |
|        - | 12419 | `	sxi32 iNest;     /* Nesting level */` |
|        - | 12420 | `	sxu32 nLine;     /* Last processed line */` |
|        - | 12421 | `	sxu32 nMagic;    /* Magic number so that we avoid misuse  */` |
|        - | 12422 | `};` |
|        - | 12423 | `#define XML_ENGINE_MAGIC 0x851EFC52` |
|        - | 12424 | `#define IS_INVALID_XML_ENGINE(XML) (XML == 0 \|\| (XML)->nMagic != XML_ENGINE_MAGIC)` |
|        - | 12425 | `/*` |
|        - | 12426 | ` * Allocate and initialize an XML engine.` |
|        - | 12427 | ` */` |
|       84 | 12428 | `static ph7_xml_engine * VmCreateXMLEngine(ph7_context *pCtx,int process_ns,int ns_sep)` |
|        1 | 12429 |  |
|        - | 12430 | `	ph7_xml_engine *pEngine;` |
|       85 | 12431 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12432 | `	ph7_value *pValue;` |
|        - | 12433 | `	sxu32 n;` |
|        - | 12434 | `	/* Allocate a new instance */` |
|       85 | 12435 | `	pEngine = (ph7_xml_engine *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_xml_engine));` |
|       85 | 12436 | `	if( pEngine == 0 ){` |
|        - | 12437 | `		/* Out of memory */` |
|      ! 0 | 12438 | `		return 0;` |
|        - | 12439 | `	}` |
|        - | 12440 | `	/* Zero the structure */` |
|       85 | 12441 | `	SyZero(pEngine,sizeof(ph7_xml_engine));` |
|        - | 12442 | `	/* Initialize fields */` |
|       85 | 12443 | `	pEngine->pVm = pVm;` |
|       85 | 12444 | `	pEngine->pCtx = 0;` |
|       85 | 12445 | `	pEngine->ns_sep = ns_sep;` |
|       85 | 12446 | `	SyXMLParserInit(&pEngine->sParser,&pVm->sAllocator,process_ns ? SXML_ENABLE_NAMESPACE : 0);` |
|       85 | 12447 | `	SyBlobInit(&pEngine->sErr,&pVm->sAllocator);` |
|       85 | 12448 | `	PH7_MemObjInit(pVm,&pEngine->sParserValue);` |
|      925 | 12449 | `	for( n = 0 ; n < SX_ARRAYSIZE(pEngine->aCB) ; ++n ){` |
|      841 | 12450 | `		pValue = &pEngine->aCB[n];` |
|        - | 12451 | `		/* NULLIFY the array entries,until someone register an event handler */` |
|      841 | 12452 | `		PH7_MemObjInit(&(*pVm),pValue);` |
|      421 | 12453 | `	}` |
|       85 | 12454 | `	ph7_value_resource(&pEngine->sParserValue,pEngine);` |
|       85 | 12455 | `	pEngine->iErrCode = SXML_ERROR_NONE;` |
|        - | 12456 | `	/* Finally set the magic number */` |
|       85 | 12457 | `	pEngine->nMagic = XML_ENGINE_MAGIC;` |
|       85 | 12458 | `	return pEngine;` |
|       43 | 12459 |  |
|        - | 12460 | `/*` |
|        - | 12461 | ` * Release an XML engine.` |
|        - | 12462 | ` */` |
|       84 | 12463 | `static void VmReleaseXMLEngine(ph7_xml_engine *pEngine)` |
|        1 | 12464 |  |
|       85 | 12465 | `	ph7_vm *pVm = pEngine->pVm;` |
|        - | 12466 | `	ph7_value *pValue;` |
|        - | 12467 | `	sxu32 n;` |
|        - | 12468 | `	/* Release fields */` |
|       85 | 12469 | `	SyBlobRelease(&pEngine->sErr);` |
|       85 | 12470 | `	SyXMLParserRelease(&pEngine->sParser);` |
|       85 | 12471 | `	PH7_MemObjRelease(&pEngine->sParserValue);` |
|      925 | 12472 | `	for( n = 0 ; n < SX_ARRAYSIZE(pEngine->aCB) ; ++n ){` |
|      841 | 12473 | `		pValue = &pEngine->aCB[n];` |
|      841 | 12474 | `		PH7_MemObjRelease(pValue);` |
|      421 | 12475 | `	}` |
|       85 | 12476 | `	pEngine->nMagic = 0x2621;` |
|        - | 12477 | `	/* Finally,release the whole instance */` |
|       85 | 12478 | `	SyMemBackendFree(&pVm->sAllocator,pEngine);` |
|       85 | 12479 |  |
|        - | 12480 | `/*` |
|        - | 12481 | ` * resource xml_parser_create([ string $encoding ])` |
|        - | 12482 | ` *  Create an UTF-8 XML parser.` |
|        - | 12483 | ` * Parameter` |
|        - | 12484 | ` *  $encoding` |
|        - | 12485 | ` *   (Only UTF-8 encoding is used)` |
|        - | 12486 | ` * Return` |
|        - | 12487 | ` *  Returns a resource handle for the new XML parser.` |
|        - | 12488 | ` */` |
|       80 | 12489 | `static int vm_builtin_xml_parser_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12490 |  |
|        - | 12491 | `	ph7_xml_engine *pEngine;` |
|        - | 12492 | `	/* Allocate a new instance */` |
|       81 | 12493 | `	pEngine = VmCreateXMLEngine(&(*pCtx),0,':');` |
|       81 | 12494 | `	if( pEngine == 0 ){` |
|      ! 0 | 12495 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - | 12496 | `		/* Return null */` |
|      ! 0 | 12497 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12498 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12499 | `		SXUNUSED(apArg);` |
|      ! 0 | 12500 | `		return PH7_OK;` |
|        - | 12501 | `	}` |
|        - | 12502 | `	/* Return the engine as a resource */` |
|       81 | 12503 | `	ph7_result_resource(pCtx,pEngine);` |
|       81 | 12504 | `	return PH7_OK;` |
|       41 | 12505 |  |
|        - | 12506 | `/*` |
|        - | 12507 | ` * resource xml_parser_create_ns([ string $encoding[,string $separator = ':']])` |
|        - | 12508 | ` *  Create an UTF-8 XML parser with namespace support.` |
|        - | 12509 | ` * Parameter` |
|        - | 12510 | ` *  $encoding` |
|        - | 12511 | ` *   (Only UTF-8 encoding is supported)` |
|        - | 12512 | ` *  $separtor` |
|        - | 12513 | ` *   Namespace separator (a single character)` |
|        - | 12514 | ` * Return` |
|        - | 12515 | ` *  Returns a resource handle for the new XML parser.` |
|        - | 12516 | ` */` |
|        4 | 12517 | `static int vm_builtin_xml_parser_create_ns(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12518 |  |
|        - | 12519 | `	ph7_xml_engine *pEngine;` |
|        5 | 12520 | `	int ns_sep = ':';` |
|        5 | 12521 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      ! 0 | 12522 | `		const char *zSep = ph7_value_to_string(apArg[1],0);` |
|      ! 0 | 12523 | `		if( zSep[0] != 0 ){` |
|      ! 0 | 12524 | `			ns_sep = zSep[0];` |
|      ! 0 | 12525 | `		}` |
|      ! 0 | 12526 | `	}` |
|        - | 12527 | `	/* Allocate a new instance */` |
|        5 | 12528 | `	pEngine = VmCreateXMLEngine(&(*pCtx),TRUE,ns_sep);` |
|        5 | 12529 | `	if( pEngine == 0 ){` |
|      ! 0 | 12530 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - | 12531 | `		/* Return null */` |
|      ! 0 | 12532 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12533 | `		return PH7_OK;` |
|        - | 12534 | `	}` |
|        - | 12535 | `	/* Return the engine as a resource */` |
|        5 | 12536 | `	ph7_result_resource(pCtx,pEngine);` |
|        5 | 12537 | `	return PH7_OK;` |
|        3 | 12538 |  |
|        - | 12539 | `/*` |
|        - | 12540 | ` * bool xml_parser_free(resource $parser)` |
|        - | 12541 | ` *  Release an XML engine.` |
|        - | 12542 | ` * Parameter` |
|        - | 12543 | ` *  $parser` |
|        - | 12544 | ` *   A reference to the XML parser to free.` |
|        - | 12545 | ` * Return` |
|        - | 12546 | ` *  This function returns FALSE if parser does not refer` |
|        - | 12547 | ` *  to a valid parser, or else it frees the parser and returns TRUE.` |
|        - | 12548 | ` */` |
|       84 | 12549 | `static int vm_builtin_xml_parser_free(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12550 |  |
|        - | 12551 | `	ph7_xml_engine *pEngine;` |
|       85 | 12552 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12553 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12554 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12555 | `		return PH7_OK;` |
|        - | 12556 | `	}` |
|        - | 12557 | `	/* Point to the XML engine */` |
|       85 | 12558 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|       85 | 12559 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12560 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12561 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12562 | `		return PH7_OK;` |
|        - | 12563 | `	}` |
|        - | 12564 | `	/* Safely release the engine */` |
|       85 | 12565 | `	VmReleaseXMLEngine(pEngine);` |
|        - | 12566 | `	/* Return TRUE */` |
|       85 | 12567 | `	ph7_result_bool(pCtx,1);` |
|       85 | 12568 | `	return PH7_OK;` |
|       43 | 12569 |  |
|        - | 12570 | `/*` |
|        - | 12571 | ` * bool xml_set_element_handler(resource $parser,callback $start_element_handler,[callback $end_element_handler])` |
|        - | 12572 | ` * Sets the element handler functions for the XML parser. start_element_handler and end_element_handler` |
|        - | 12573 | ` * are strings containing the names of functions.` |
|        - | 12574 | ` * Parameters` |
|        - | 12575 | ` *  $parser` |
|        - | 12576 | ` *   A reference to the XML parser to set up start and end element handler functions.` |
|        - | 12577 | ` *  $start_element_handler` |
|        - | 12578 | ` *    The function named by start_element_handler must accept three parameters:` |
|        - | 12579 | ` *    start_element_handler(resource $parser,string $name,array $attribs)` |
|        - | 12580 | ` *    $parser` |
|        - | 12581 | ` *      The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12582 | ` *   $name` |
|        - | 12583 | ` *      The second parameter, name, contains the name of the element for which this handler` |
|        - | 12584 | ` *		is called.If case-folding is in effect for this parser, the element name will be in uppercase letters.` |
|        - | 12585 | ` *  $attribs` |
|        - | 12586 | ` *      The third parameter, attribs, contains an associative array with the element's attributes (if any).` |
|        - | 12587 | ` *		The keys of this array are the attribute names, the values are the attribute values.` |
|        - | 12588 | ` *      Attribute names are case-folded on the same criteria as element names.Attribute values are not case-folded.` |
|        - | 12589 | ` *      The original order of the attributes can be retrieved by walking through attribs the normal way, using each().` |
|        - | 12590 | ` *      The first key in the array was the first attribute, and so on.` |
|        - | 12591 | ` *      Note: Instead of a function name, an array containing an object reference and a method name can also be supplied.` |
|        - | 12592 | ` * $end_element_handler` |
|        - | 12593 | ` *     The function named by end_element_handler must accept two parameters:` |
|        - | 12594 | ` *     end_element_handler(resource $parser,string $name)` |
|        - | 12595 | ` *    $parser` |
|        - | 12596 | ` *      The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12597 | ` *   $name` |
|        - | 12598 | ` *      The second parameter, name, contains the name of the element for which this handler` |
|        - | 12599 | ` *      is called.If case-folding is in effect for this parser, the element name will be in uppercase` |
|        - | 12600 | ` *      letters.` |
|        - | 12601 | ` *      If a handler function is set to an empty string, or FALSE, the handler in question is disabled.` |
|        - | 12602 | ` * Return` |
|        - | 12603 | ` * TRUE on success or FALSE on failure.` |
|        - | 12604 | ` */` |
|       66 | 12605 | `static int vm_builtin_xml_set_element_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12606 |  |
|        - | 12607 | `	ph7_xml_engine *pEngine;` |
|       67 | 12608 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12609 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12610 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12611 | `		return PH7_OK;` |
|        - | 12612 | `	}` |
|        - | 12613 | `	/* Point to the XML engine */` |
|       67 | 12614 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|       67 | 12615 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12616 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12617 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12618 | `		return PH7_OK;` |
|        - | 12619 | `	}` |
|       67 | 12620 | `	if( nArg > 1 ){` |
|        - | 12621 | `		/* Save the start_element_handler callback for later invocation */` |
|       67 | 12622 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_START_TAG]);` |
|       67 | 12623 | `		if( nArg > 2 ){` |
|        - | 12624 | `			/* Save the end_element_handler callback for later invocation */` |
|       67 | 12625 | `			PH7_MemObjStore(apArg[2]/* User callback*/,&pEngine->aCB[PH7_XML_END_TAG]);` |
|       33 | 12626 | `		}` |
|       33 | 12627 | `	}` |
|        - | 12628 | `	/* All done,return TRUE */` |
|       67 | 12629 | `	ph7_result_bool(pCtx,1);` |
|       67 | 12630 | `	return PH7_OK;` |
|       34 | 12631 |  |
|        - | 12632 | `/*` |
|        - | 12633 | ` * bool xml_set_character_data_handler(resource $parser,callback $handler)` |
|        - | 12634 | ` *  Sets the character data handler function for the XML parser parser.` |
|        - | 12635 | ` * Parameters` |
|        - | 12636 | ` * $parser` |
|        - | 12637 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12638 | ` * $handler` |
|        - | 12639 | ` *  handler is a string containing the name of the callback.` |
|        - | 12640 | ` *  The function named by handler must accept two parameters:` |
|        - | 12641 | ` *   handler(resource $parser,string $data)` |
|        - | 12642 | ` *  $parser` |
|        - | 12643 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12644 | ` *  $data` |
|        - | 12645 | ` *   The second parameter, data, contains the character data as a string.` |
|        - | 12646 | ` *   Character data handler is called for every piece of a text in the XML document.` |
|        - | 12647 | ` *   It can be called multiple times inside each fragment (e.g. for non-ASCII strings).` |
|        - | 12648 | ` *   If a handler function is set to an empty string, or FALSE, the handler in question is disabled.` |
|        - | 12649 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12650 | ` *   can also be supplied.` |
|        - | 12651 | ` * Return` |
|        - | 12652 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12653 | ` */` |
|       40 | 12654 | `static int vm_builtin_xml_set_character_data_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12655 |  |
|        - | 12656 | `	ph7_xml_engine *pEngine;` |
|       41 | 12657 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12658 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12659 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12660 | `		return PH7_OK;` |
|        - | 12661 | `	}` |
|        - | 12662 | `	/* Point to the XML engine */` |
|       41 | 12663 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|       41 | 12664 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12665 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12666 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12667 | `		return PH7_OK;` |
|        - | 12668 | `	}` |
|       41 | 12669 | `	if( nArg > 1 ){` |
|        - | 12670 | `		/* Save the user callback for later invocation */` |
|       41 | 12671 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_CDATA]);` |
|       20 | 12672 | `	}` |
|        - | 12673 | `	/* All done,return TRUE */` |
|       41 | 12674 | `	ph7_result_bool(pCtx,1);` |
|       41 | 12675 | `	return PH7_OK;` |
|       21 | 12676 |  |
|        - | 12677 | `/*` |
|        - | 12678 | ` * bool xml_set_default_handler(resource $parser,callback $handler)` |
|        - | 12679 | ` *  Set up default handler.` |
|        - | 12680 | ` * Parameters` |
|        - | 12681 | ` * $parser` |
|        - | 12682 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12683 | ` * $handler` |
|        - | 12684 | ` *  handler is a string containing the name of the callback.` |
|        - | 12685 | ` *  The function named by handler must accept two parameters:` |
|        - | 12686 | ` *   handler(resource $parser,string $data)` |
|        - | 12687 | ` *  $parser` |
|        - | 12688 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12689 | ` *  $data` |
|        - | 12690 | ` *   The second parameter, data, contains the character data.This may be the XML declaration` |
|        - | 12691 | ` *   document type declaration, entities or other data for which no other handler exists.` |
|        - | 12692 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12693 | ` *   can also be supplied.` |
|        - | 12694 | ` * Return` |
|        - | 12695 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12696 | ` */` |
|        2 | 12697 | `static int vm_builtin_xml_set_default_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12698 |  |
|        - | 12699 | `	ph7_xml_engine *pEngine;` |
|        3 | 12700 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12701 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12702 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12703 | `		return PH7_OK;` |
|        - | 12704 | `	}` |
|        - | 12705 | `	/* Point to the XML engine */` |
|        3 | 12706 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12707 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12708 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12709 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12710 | `		return PH7_OK;` |
|        - | 12711 | `	}` |
|        3 | 12712 | `	if( nArg > 1 ){` |
|        - | 12713 | `		/* Save the user callback for later invocation */` |
|        3 | 12714 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_DEF]);` |
|        1 | 12715 | `	}` |
|        - | 12716 | `	/* All done,return TRUE */` |
|        3 | 12717 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12718 | `	return PH7_OK;` |
|        2 | 12719 |  |
|        - | 12720 | `/*` |
|        - | 12721 | ` * bool xml_set_end_namespace_decl_handler(resource $parser,callback $handler)` |
|        - | 12722 | ` *  Set up end namespace declaration handler.` |
|        - | 12723 | ` * Parameters` |
|        - | 12724 | ` * $parser` |
|        - | 12725 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12726 | ` * $handler` |
|        - | 12727 | ` *  handler is a string containing the name of the callback.` |
|        - | 12728 | ` *  The function named by handler must accept two parameters:` |
|        - | 12729 | ` *   handler(resource $parser,string $prefix)` |
|        - | 12730 | ` *  $parser` |
|        - | 12731 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12732 | ` *  $prefix` |
|        - | 12733 | ` *   The prefix is a string used to reference the namespace within an XML object.` |
|        - | 12734 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12735 | ` *   can also be supplied.` |
|        - | 12736 | ` * Return` |
|        - | 12737 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12738 | ` */` |
|        2 | 12739 | `static int vm_builtin_xml_set_end_namespace_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12740 |  |
|        - | 12741 | `	ph7_xml_engine *pEngine;` |
|        3 | 12742 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12743 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12744 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12745 | `		return PH7_OK;` |
|        - | 12746 | `	}` |
|        - | 12747 | `	/* Point to the XML engine */` |
|        3 | 12748 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12749 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12750 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12751 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12752 | `		return PH7_OK;` |
|        - | 12753 | `	}` |
|        3 | 12754 | `	if( nArg > 1 ){` |
|        - | 12755 | `		/* Save the user callback for later invocation */` |
|        3 | 12756 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_NS_END]);` |
|        1 | 12757 | `	}` |
|        - | 12758 | `	/* All done,return TRUE */` |
|        3 | 12759 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12760 | `	return PH7_OK;` |
|        2 | 12761 |  |
|        - | 12762 | `/*` |
|        - | 12763 | ` * bool xml_set_start_namespace_decl_handler(resource $parser,callback $handler)` |
|        - | 12764 | ` *  Set up start namespace declaration handler.` |
|        - | 12765 | ` * Parameters` |
|        - | 12766 | ` * $parser` |
|        - | 12767 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12768 | ` * $handler` |
|        - | 12769 | ` *  handler is a string containing the name of the callback.` |
|        - | 12770 | ` *  The function named by handler must accept two parameters:` |
|        - | 12771 | ` *   handler(resource $parser,string $prefix,string $uri)` |
|        - | 12772 | ` *  $parser` |
|        - | 12773 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12774 | ` *  $prefix` |
|        - | 12775 | ` *   The prefix is a string used to reference the namespace within an XML object.` |
|        - | 12776 | ` *  $uri` |
|        - | 12777 | ` *    Uniform Resource Identifier (URI) of namespace.` |
|        - | 12778 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12779 | ` *   can also be supplied.` |
|        - | 12780 | ` * Return` |
|        - | 12781 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12782 | ` */` |
|        2 | 12783 | `static int vm_builtin_xml_set_start_namespace_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12784 |  |
|        - | 12785 | `	ph7_xml_engine *pEngine;` |
|        3 | 12786 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12787 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12788 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12789 | `		return PH7_OK;` |
|        - | 12790 | `	}` |
|        - | 12791 | `	/* Point to the XML engine */` |
|        3 | 12792 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12793 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12794 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12795 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12796 | `		return PH7_OK;` |
|        - | 12797 | `	}` |
|        3 | 12798 | `	if( nArg > 1 ){` |
|        - | 12799 | `		/* Save the user callback for later invocation */` |
|        3 | 12800 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_NS_START]);` |
|        1 | 12801 | `	}` |
|        - | 12802 | `	/* All done,return TRUE */` |
|        3 | 12803 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12804 | `	return PH7_OK;` |
|        2 | 12805 |  |
|        - | 12806 | `/*` |
|        - | 12807 | ` * bool xml_set_processing_instruction_handler(resource $parser,callback $handler)` |
|        - | 12808 | ` *  Set up processing instruction (PI) handler.` |
|        - | 12809 | ` * Parameters` |
|        - | 12810 | ` * $parser` |
|        - | 12811 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12812 | ` * $handler` |
|        - | 12813 | ` *  handler is a string containing the name of the callback.` |
|        - | 12814 | ` *  The function named by handler must accept three parameters:` |
|        - | 12815 | ` *   handler(resource $parser,string $target,string $data)` |
|        - | 12816 | ` *  $parser` |
|        - | 12817 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12818 | ` *  $target` |
|        - | 12819 | ` *   The second parameter, target, contains the PI target.` |
|        - | 12820 | ` *  $data` |
|        - | 12821 | `     The third parameter, data, contains the PI data.` |
|        - | 12822 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12823 | ` *   can also be supplied.` |
|        - | 12824 | ` * Return` |
|        - | 12825 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12826 | ` */` |
|        8 | 12827 | `static int vm_builtin_xml_set_processing_instruction_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12828 |  |
|        - | 12829 | `	ph7_xml_engine *pEngine;` |
|        9 | 12830 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12831 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12832 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12833 | `		return PH7_OK;` |
|        - | 12834 | `	}` |
|        - | 12835 | `	/* Point to the XML engine */` |
|        9 | 12836 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        9 | 12837 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12838 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12839 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12840 | `		return PH7_OK;` |
|        - | 12841 | `	}` |
|        9 | 12842 | `	if( nArg > 1 ){` |
|        - | 12843 | `		/* Save the user callback for later invocation */` |
|        9 | 12844 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_PI]);` |
|        4 | 12845 | `	}` |
|        - | 12846 | `	/* All done,return TRUE */` |
|        9 | 12847 | `	ph7_result_bool(pCtx,1);` |
|        9 | 12848 | `	return PH7_OK;` |
|        5 | 12849 |  |
|        - | 12850 | `/*` |
|        - | 12851 | ` * bool xml_set_unparsed_entity_decl_handler(resource $parser,callback $handler)` |
|        - | 12852 | ` *  Set up unparsed entity declaration handler.` |
|        - | 12853 | ` * Parameters` |
|        - | 12854 | ` * $parser` |
|        - | 12855 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12856 | ` * $handler` |
|        - | 12857 | ` *  handler is a string containing the name of the callback.` |
|        - | 12858 | ` *  The function named by handler must accept six parameters:` |
|        - | 12859 | ` *  handler(resource $parser,string $entity_name,string $base,string $system_id,string $public_id,string $notation_name)` |
|        - | 12860 | ` *  $parser` |
|        - | 12861 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12862 | ` *  $entity_name` |
|        - | 12863 | ` *   The name of the entity that is about to be defined.` |
|        - | 12864 | ` *  $base` |
|        - | 12865 | ` *   This is the base for resolving the system identifier (systemId) of the external entity.` |
|        - | 12866 | ` *   Currently this parameter will always be set to an empty string.` |
|        - | 12867 | ` *  $system_id` |
|        - | 12868 | ` *   System identifier for the external entity.` |
|        - | 12869 | ` *  $public_id` |
|        - | 12870 | ` *    Public identifier for the external entity.` |
|        - | 12871 | ` *  $notation_name` |
|        - | 12872 | ` *    Name of the notation of this entity (see xml_set_notation_decl_handler()).` |
|        - | 12873 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12874 | ` *   can also be supplied.` |
|        - | 12875 | ` * Return` |
|        - | 12876 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12877 | ` */` |
|        2 | 12878 | `static int vm_builtin_xml_set_unparsed_entity_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12879 |  |
|        - | 12880 | `	ph7_xml_engine *pEngine;` |
|        3 | 12881 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12882 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12883 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12884 | `		return PH7_OK;` |
|        - | 12885 | `	}` |
|        - | 12886 | `	/* Point to the XML engine */` |
|        3 | 12887 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12888 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12889 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12890 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12891 | `		return PH7_OK;` |
|        - | 12892 | `	}` |
|        3 | 12893 | `	if( nArg > 1 ){` |
|        - | 12894 | `		/* Save the user callback for later invocation */` |
|        3 | 12895 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_UNPED]);` |
|        1 | 12896 | `	}` |
|        - | 12897 | `	/* All done,return TRUE */` |
|        3 | 12898 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12899 | `	return PH7_OK;` |
|        2 | 12900 |  |
|        - | 12901 | `/*` |
|        - | 12902 | ` * bool xml_set_notation_decl_handler(resource $parser,callback $handler)` |
|        - | 12903 | ` *  Set up notation declaration handler.` |
|        - | 12904 | ` * Parameters` |
|        - | 12905 | ` * $parser` |
|        - | 12906 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12907 | ` * $handler` |
|        - | 12908 | ` *  handler is a string containing the name of the callback.` |
|        - | 12909 | ` *  The function named by handler must accept five parameters:` |
|        - | 12910 | ` *  handler(resource $parser,string $entity_name,string $base,string $system_id,string $public_id)` |
|        - | 12911 | ` *  $parser` |
|        - | 12912 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12913 | ` *  $entity_name` |
|        - | 12914 | ` *   The name of the entity that is about to be defined.` |
|        - | 12915 | ` *  $base` |
|        - | 12916 | ` *   This is the base for resolving the system identifier (systemId) of the external entity.` |
|        - | 12917 | ` *   Currently this parameter will always be set to an empty string.` |
|        - | 12918 | ` *  $system_id` |
|        - | 12919 | ` *   System identifier for the external entity.` |
|        - | 12920 | ` *  $public_id` |
|        - | 12921 | ` *    Public identifier for the external entity.` |
|        - | 12922 | ` *  Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12923 | ` *  can also be supplied.` |
|        - | 12924 | ` * Return` |
|        - | 12925 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12926 | ` */` |
|        2 | 12927 | `static int vm_builtin_xml_set_notation_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12928 |  |
|        - | 12929 | `	ph7_xml_engine *pEngine;` |
|        3 | 12930 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12931 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12932 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12933 | `		return PH7_OK;` |
|        - | 12934 | `	}` |
|        - | 12935 | `	/* Point to the XML engine */` |
|        3 | 12936 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12937 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12938 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12939 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12940 | `		return PH7_OK;` |
|        - | 12941 | `	}` |
|        3 | 12942 | `	if( nArg > 1 ){` |
|        - | 12943 | `		/* Save the user callback for later invocation */` |
|        3 | 12944 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_ND]);` |
|        1 | 12945 | `	}` |
|        - | 12946 | `	/* All done,return TRUE */` |
|        3 | 12947 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12948 | `	return PH7_OK;` |
|        2 | 12949 |  |
|        - | 12950 | `/*` |
|        - | 12951 | ` * bool xml_set_external_entity_ref_handler(resource $parser,callback $handler)` |
|        - | 12952 | ` *  Set up external entity reference handler.` |
|        - | 12953 | ` * Parameters` |
|        - | 12954 | ` * $parser` |
|        - | 12955 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12956 | ` * $handler` |
|        - | 12957 | ` *  handler is a string containing the name of the callback.` |
|        - | 12958 | ` *  The function named by handler must accept five parameters:` |
|        - | 12959 | ` *   handler(resource $parser,string $open_entity_names,string $base,string $system_id,string $public_id)` |
|        - | 12960 | ` *  $parser` |
|        - | 12961 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12962 | ` *  $open_entity_names` |
|        - | 12963 | ` *   The second parameter, open_entity_names, is a space-separated list of the names` |
|        - | 12964 | ` *   of the entities that are open for the parse of this entity (including the name of the referenced entity).` |
|        - | 12965 | ` *  $base` |
|        - | 12966 | ` *   This is the base for resolving the system identifier (system_id) of the external entity.` |
|        - | 12967 | ` *   Currently this parameter will always be set to an empty string.` |
|        - | 12968 | ` *  $system_id` |
|        - | 12969 | ` *   The fourth parameter, system_id, is the system identifier as specified in the entity declaration.` |
|        - | 12970 | ` *  $public_id` |
|        - | 12971 | ` *   The fifth parameter, public_id, is the public identifier as specified in the entity declaration` |
|        - | 12972 | ` *   or an empty string if none was specified; the whitespace in the public identifier will have been` |
|        - | 12973 | ` *   normalized as required by the XML spec.` |
|        - | 12974 | ` * Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12975 | ` * can also be supplied.` |
|        - | 12976 | ` * Return` |
|        - | 12977 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12978 | ` */` |
|        2 | 12979 | `static int vm_builtin_xml_set_external_entity_ref_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12980 |  |
|        - | 12981 | `	ph7_xml_engine *pEngine;` |
|        3 | 12982 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12983 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12984 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12985 | `		return PH7_OK;` |
|        - | 12986 | `	}` |
|        - | 12987 | `	/* Point to the XML engine */` |
|        3 | 12988 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12989 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12990 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12991 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12992 | `		return PH7_OK;` |
|        - | 12993 | `	}` |
|        3 | 12994 | `	if( nArg > 1 ){` |
|        - | 12995 | `		/* Save the user callback for later invocation */` |
|        3 | 12996 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_EER]);` |
|        1 | 12997 | `	}` |
|        - | 12998 | `	/* All done,return TRUE */` |
|        3 | 12999 | `	ph7_result_bool(pCtx,1);` |
|        3 | 13000 | `	return PH7_OK;` |
|        2 | 13001 |  |
|        - | 13002 | `/*` |
|        - | 13003 | ` * int xml_get_current_line_number(resource $parser)` |
|        - | 13004 | ` *  Gets the current line number for the given XML parser.` |
|        - | 13005 | ` * Parameters` |
|        - | 13006 | ` * $parser` |
|        - | 13007 | ` *   A reference to the XML parser.` |
|        - | 13008 | ` * Return` |
|        - | 13009 | ` *  This function returns FALSE if parser does not refer` |
|        - | 13010 | ` *  to a valid parser, or else it returns which line the parser` |
|        - | 13011 | ` *  is currently at in its data buffer.` |
|        - | 13012 | ` */` |
|        8 | 13013 | `static int vm_builtin_xml_get_current_line_number(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13014 |  |
|        - | 13015 | `	ph7_xml_engine *pEngine;` |
|        9 | 13016 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13017 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13018 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13019 | `		return PH7_OK;` |
|        - | 13020 | `	}` |
|        - | 13021 | `	/* Point to the XML engine */` |
|        9 | 13022 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        9 | 13023 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13024 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13025 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13026 | `		return PH7_OK;` |
|        - | 13027 | `	}` |
|        - | 13028 | `	/* Return the line number */` |
|        9 | 13029 | `	ph7_result_int(pCtx,(int)pEngine->nLine);` |
|        9 | 13030 | `	return PH7_OK;` |
|        5 | 13031 |  |
|        - | 13032 | `/*` |
|        - | 13033 | ` * int xml_get_current_byte_index(resource $parser)` |
|        - | 13034 | ` *  Gets the current byte index of the given XML parser.` |
|        - | 13035 | ` * Parameters` |
|        - | 13036 | ` * $parser` |
|        - | 13037 | ` *   A reference to the XML parser.` |
|        - | 13038 | ` * Return` |
|        - | 13039 | ` *  This function returns FALSE if parser does not refer to a valid` |
|        - | 13040 | ` *  parser, or else it returns which byte index the parser is currently` |
|        - | 13041 | ` *  at in its data buffer (starting at 0).` |
|        - | 13042 | ` */` |
|        4 | 13043 | `static int vm_builtin_xml_get_current_byte_index(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13044 |  |
|        - | 13045 | `	ph7_xml_engine *pEngine;` |
|        - | 13046 | `	SyStream *pStream;` |
|        - | 13047 | `	SyToken *pToken;` |
|        5 | 13048 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13049 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13050 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13051 | `		return PH7_OK;` |
|        - | 13052 | `	}` |
|        - | 13053 | `	/* Point to the XML engine */` |
|        5 | 13054 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        5 | 13055 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13056 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13057 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13058 | `		return PH7_OK;` |
|        - | 13059 | `	}` |
|        - | 13060 | `	/* Point to the current processed token */` |
|        5 | 13061 | `	pToken = (SyToken *)SySetPeekCurrentEntry(&pEngine->sParser.sToken);` |
|        5 | 13062 | `	if( pToken == 0 ){` |
|        - | 13063 | `		/* Stream not yet processed */` |
|        3 | 13064 | `		ph7_result_int(pCtx,0);` |
|        3 | 13065 | `		return 0;` |
|        - | 13066 | `	}` |
|        - | 13067 | `	/* Point to the input stream */` |
|        3 | 13068 | `	pStream = &pEngine->sParser.sLex.sStream;` |
|        - | 13069 | `	/* Return the byte index */` |
|        3 | 13070 | `	ph7_result_int64(pCtx,(ph7_int64)(pToken->sData.zString-(const char *)pStream->zInput));` |
|        3 | 13071 | `	return PH7_OK;` |
|        3 | 13072 |  |
|        - | 13073 | `/*` |
|        - | 13074 | ` * bool xml_set_object(resource $parser,object &$object)` |
|        - | 13075 | ` *  Use XML Parser within an object.` |
|        - | 13076 | ` * NOTE` |
|        - | 13077 | ` *  This function is depreceated and is a no-op.` |
|        - | 13078 | ` * Parameters` |
|        - | 13079 | ` * $parser` |
|        - | 13080 | ` *   A reference to the XML parser.` |
|        - | 13081 | ` * $object` |
|        - | 13082 | ` *  The object where to use the XML parser.` |
|        - | 13083 | ` * Return` |
|        - | 13084 | ` * Always FALSE.` |
|        - | 13085 | ` */` |
|        2 | 13086 | `static int vm_builtin_xml_set_object(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13087 |  |
|        - | 13088 | `	ph7_xml_engine *pEngine;` |
|        3 | 13089 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_object(apArg[1]) ){` |
|        - | 13090 | `		/* Missing/Ivalid argument,return FALSE */` |
|        3 | 13091 | `		ph7_result_bool(pCtx,0);` |
|        3 | 13092 | `		return PH7_OK;` |
|        - | 13093 | `	}` |
|        - | 13094 | `	/* Point to the XML engine */` |
|      ! 0 | 13095 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|      ! 0 | 13096 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13097 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13098 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13099 | `		return PH7_OK;` |
|        - | 13100 | `	}` |
|        - | 13101 | `	/*  Throw a notice and return */` |
|      ! 0 | 13102 | `	ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"This function is depreceated and is a no-op."` |
|        - | 13103 | `		"In order to mimic this behaviour,you can supply instead of a function name an array "` |
|        - | 13104 | `		"containing an object reference and a method name."` |
|        - | 13105 | `		);` |
|        - | 13106 | `	/* Return FALSE */` |
|      ! 0 | 13107 | `	ph7_result_bool(pCtx,0);` |
|      ! 0 | 13108 | `	return PH7_OK;` |
|        2 | 13109 |  |
|        - | 13110 | `/*` |
|        - | 13111 | ` * int xml_get_current_column_number(resource $parser)` |
|        - | 13112 | ` *  Gets the current column number of the given XML parser.` |
|        - | 13113 | ` * Parameters` |
|        - | 13114 | ` * $parser` |
|        - | 13115 | ` *   A reference to the XML parser.` |
|        - | 13116 | ` * Return` |
|        - | 13117 | ` *  This function returns FALSE if parser does not refer to a valid parser, or else it returns` |
|        - | 13118 | ` *  which column on the current line (as given by xml_get_current_line_number()) the parser` |
|        - | 13119 | ` *  is currently at.` |
|        - | 13120 | ` */` |
|        4 | 13121 | `static int vm_builtin_xml_get_current_column_number(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13122 |  |
|        - | 13123 | `	ph7_xml_engine *pEngine;` |
|        - | 13124 | `	SyStream *pStream;` |
|        - | 13125 | `	SyToken *pToken;` |
|        5 | 13126 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13127 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13128 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13129 | `		return PH7_OK;` |
|        - | 13130 | `	}` |
|        - | 13131 | `	/* Point to the XML engine */` |
|        5 | 13132 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        5 | 13133 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13134 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13135 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13136 | `		return PH7_OK;` |
|        - | 13137 | `	}` |
|        - | 13138 | `	/* Point to the current processed token */` |
|        5 | 13139 | `	pToken = (SyToken *)SySetPeekCurrentEntry(&pEngine->sParser.sToken);` |
|        5 | 13140 | `	if( pToken == 0 ){` |
|        - | 13141 | `		/* Stream not yet processed */` |
|      ! 0 | 13142 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 13143 | `		return 0;` |
|        - | 13144 | `	}` |
|        - | 13145 | `	/* Point to the input stream */` |
|        5 | 13146 | `	pStream = &pEngine->sParser.sLex.sStream;` |
|        - | 13147 | `	/* Return the byte index */` |
|        5 | 13148 | `	ph7_result_int64(pCtx,(ph7_int64)(pToken->sData.zString-(const char *)pStream->zInput)/80);` |
|        5 | 13149 | `	return PH7_OK;` |
|        3 | 13150 |  |
|        - | 13151 | `/*` |
|        - | 13152 | ` * int xml_get_error_code(resource $parser)` |
|        - | 13153 | ` *  Get XML parser error code.` |
|        - | 13154 | ` * Parameters` |
|        - | 13155 | ` * $parser` |
|        - | 13156 | ` *   A reference to the XML parser.` |
|        - | 13157 | ` * Return` |
|        - | 13158 | ` *  This function returns FALSE if parser does not refer to a valid` |
|        - | 13159 | ` *  parser, or else it returns one of the error codes listed in the error` |
|        - | 13160 | ` *  codes section.` |
|        - | 13161 | ` */` |
|       32 | 13162 | `static int vm_builtin_xml_get_error_code(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13163 |  |
|        - | 13164 | `	ph7_xml_engine *pEngine;` |
|       33 | 13165 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13166 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13167 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13168 | `		return PH7_OK;` |
|        - | 13169 | `	}` |
|        - | 13170 | `	/* Point to the XML engine */` |
|       33 | 13171 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|       33 | 13172 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13173 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13174 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13175 | `		return PH7_OK;` |
|        - | 13176 | `	}` |
|        - | 13177 | `	/* Return the error code if any */` |
|       33 | 13178 | `	ph7_result_int(pCtx,pEngine->iErrCode);` |
|       33 | 13179 | `	return PH7_OK;` |
|       17 | 13180 |  |
|        - | 13181 | `/*` |
|        - | 13182 | ` * XML parser event callbacks` |
|        - | 13183 | ` * Each time the unserlying XML parser extract a single token` |
|        - | 13184 | ` * from the input,one of the following callbacks are invoked.` |
|        - | 13185 | ` * IMP-XML-ENGINE-07-07-2012 22:02 FreeBSD [chm@symisc.net]` |
|        - | 13186 | ` */` |
|        - | 13187 | `/*` |
|        - | 13188 | ` * Create a scalar ph7_value holding the value` |
|        - | 13189 | ` * of an XML tag/attribute/CDATA and so on.` |
|        - | 13190 | ` */` |
|      148 | 13191 | `static ph7_value * VmXMLValue(ph7_xml_engine *pEngine,SyXMLRawStr *pXML,SyXMLRawStr *pNsUri)` |
|        1 | 13192 |  |
|        - | 13193 | `	ph7_value *pValue;` |
|        - | 13194 | `	/* Allocate a new scalar variable */` |
|      149 | 13195 | `	pValue = ph7_context_new_scalar(pEngine->pCtx);` |
|      149 | 13196 | `	if( pValue == 0 ){` |
|      ! 0 | 13197 | `		ph7_context_throw_error(pEngine->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 13198 | `		return 0;` |
|        - | 13199 | `	}` |
|      149 | 13200 | `	if( pNsUri && pNsUri->nByte > 0 ){` |
|        - | 13201 | `		/* Append namespace URI and the separator */` |
|        9 | 13202 | `		ph7_value_string_format(pValue,"%.*s%c",pNsUri->nByte,pNsUri->zString,pEngine->ns_sep);` |
|        4 | 13203 | `	}` |
|        - | 13204 | `	/* Copy the tag value */` |
|      149 | 13205 | `	ph7_value_string(pValue,pXML->zString,(int)pXML->nByte);` |
|      149 | 13206 | `	return pValue;` |
|       75 | 13207 |  |
|        - | 13208 | `/*` |
|        - | 13209 | ` * Create a 'ph7_value' of type array holding the values` |
|        - | 13210 | ` * of an XML tag attributes.` |
|        - | 13211 | ` */` |
|       62 | 13212 | `static ph7_value * VmXMLAttrValue(ph7_xml_engine *pEngine,SyXMLRawStr *aAttr,sxu32 nAttr)` |
|        1 | 13213 |  |
|        - | 13214 | `	ph7_value *pArray;` |
|        - | 13215 | `	/* Create an empty array */` |
|       63 | 13216 | `	pArray = ph7_context_new_array(pEngine->pCtx);` |
|       63 | 13217 | `	if( pArray == 0 ){` |
|      ! 0 | 13218 | `		ph7_context_throw_error(pEngine->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 13219 | `		return 0;` |
|        - | 13220 | `	}` |
|       63 | 13221 | `	if( nAttr > 0 ){` |
|        - | 13222 | `		ph7_value *pKey,*pValue;` |
|        - | 13223 | `		sxu32 n;` |
|        - | 13224 | `		/* Create worker variables */` |
|        5 | 13225 | `		pKey = ph7_context_new_scalar(pEngine->pCtx);` |
|        5 | 13226 | `		pValue = ph7_context_new_scalar(pEngine->pCtx);` |
|        5 | 13227 | `		if( pKey == 0 \|\| pValue == 0 ){` |
|      ! 0 | 13228 | `			ph7_context_throw_error(pEngine->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 13229 | `			return 0;` |
|        - | 13230 | `		}` |
|        - | 13231 | `		/* Copy attributes */` |
|        9 | 13232 | `		for( n = 0 ; n < nAttr ; n += 2 ){` |
|        - | 13233 | `			/* Reset string cursors */` |
|        5 | 13234 | `			ph7_value_reset_string_cursor(pKey);` |
|        5 | 13235 | `			ph7_value_reset_string_cursor(pValue);` |
|        - | 13236 | `			/* Copy attribute name and it's associated value */` |
|        5 | 13237 | `			ph7_value_string(pKey,aAttr[n].zString,(int)aAttr[n].nByte); /* Attribute name */` |
|        5 | 13238 | `			ph7_value_string(pValue,aAttr[n+1].zString,(int)aAttr[n+1].nByte); /* Attribute value */` |
|        - | 13239 | `			/* Insert in the array */` |
|        5 | 13240 | `			ph7_array_add_elem(pArray,pKey,pValue); /* Will make it's own copy */` |
|        3 | 13241 | `		}` |
|        - | 13242 | `		/* Release the worker variables */` |
|        5 | 13243 | `		ph7_context_release_value(pEngine->pCtx,pKey);` |
|        5 | 13244 | `		ph7_context_release_value(pEngine->pCtx,pValue);` |
|        2 | 13245 | `	}` |
|        - | 13246 | `	/* Return the freshly created array */` |
|       63 | 13247 | `	return pArray;` |
|       32 | 13248 |  |
|        - | 13249 | `/*` |
|        - | 13250 | ` * Start element handler.` |
|        - | 13251 | ` * The user defined callback must accept three parameters:` |
|        - | 13252 | ` *    start_element_handler(resource $parser,string $name,array $attribs )` |
|        - | 13253 | ` *    $parser` |
|        - | 13254 | ` *      The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13255 | ` *    $name` |
|        - | 13256 | ` *      The second parameter, name, contains the name of the element for which this handler` |
|        - | 13257 | ` *		is called.If case-folding is in effect for this parser, the element name will be in uppercase letters.` |
|        - | 13258 | ` *    $attribs` |
|        - | 13259 | ` *      The third parameter, attribs, contains an associative array with the element's attributes (if any).` |
|        - | 13260 | ` *		The keys of this array are the attribute names, the values are the attribute values.` |
|        - | 13261 | ` *      Attribute names are case-folded on the same criteria as element names.Attribute values are not case-folded.` |
|        - | 13262 | ` *      The original order of the attributes can be retrieved by walking through attribs the normal way, using each().` |
|        - | 13263 | ` *      The first key in the array was the first attribute, and so on.` |
|        - | 13264 | ` *      Note: Instead of a function name, an array containing an object reference and a method name can also be supplied.` |
|        - | 13265 | ` */` |
|       78 | 13266 | `static sxi32 VmXMLStartElementHandler(SyXMLRawStr *pStart,SyXMLRawStr *pNS,sxu32 nAttr,SyXMLRawStr *aAttr,void *pUserData)` |
|        1 | 13267 |  |
|       79 | 13268 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13269 | `	ph7_value *pCallback,*pTag,*pAttr;` |
|        - | 13270 | `	/* Point to the target user defined callback */` |
|       79 | 13271 | `	pCallback = &pEngine->aCB[PH7_XML_START_TAG];` |
|        - | 13272 | `	/* Make sure the given callback is callable */` |
|       79 | 13273 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13274 | `		/* Not callable,return immediately*/` |
|       17 | 13275 | `		return SXRET_OK;` |
|        - | 13276 | `	}` |
|        - | 13277 | `	/* Create a ph7_value holding the tag name */` |
|       63 | 13278 | `	pTag = VmXMLValue(pEngine,pStart,pNS);` |
|        - | 13279 | `	/* Create a ph7_value holding the tag attributes */` |
|       63 | 13280 | `	pAttr = VmXMLAttrValue(pEngine,aAttr,nAttr);` |
|       63 | 13281 | `	if( pTag == 0  \|\| pAttr == 0 ){` |
|      ! 0 | 13282 | `		SXUNUSED(pNS); /* cc warning */` |
|        - | 13283 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13284 | `		return SXRET_OK;` |
|        - | 13285 | `	}` |
|        - | 13286 | `	/* Invoke the user callback */` |
|       63 | 13287 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pTag,pAttr,(ph7_value*)0);` |
|        - | 13288 | `	/* Clean-up the mess left behind */` |
|       63 | 13289 | `	ph7_context_release_value(pEngine->pCtx,pTag);` |
|       63 | 13290 | `	ph7_context_release_value(pEngine->pCtx,pAttr);` |
|       63 | 13291 | `	return SXRET_OK;` |
|       40 | 13292 |  |
|        - | 13293 | `/*` |
|        - | 13294 | ` * End element handler.` |
|        - | 13295 | ` * The user defined callback must accept two parameters:` |
|        - | 13296 | ` *  end_element_handler(resource $parser,string $name)` |
|        - | 13297 | ` *  $parser` |
|        - | 13298 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13299 | ` *  $name` |
|        - | 13300 | ` *   The second parameter, name, contains the name of the element for which this handler is called.` |
|        - | 13301 | ` *   If case-folding is in effect for this parser, the element name will be in uppercase letters.` |
|        - | 13302 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 13303 | ` *   can also be supplied.` |
|        - | 13304 | ` */` |
|       62 | 13305 | `static sxi32 VmXMLEndElementHandler(SyXMLRawStr *pEnd,SyXMLRawStr *pNS,void *pUserData)` |
|        1 | 13306 |  |
|       63 | 13307 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13308 | `	ph7_value *pCallback,*pTag;` |
|        - | 13309 | `	/* Point to the target user defined callback */` |
|       63 | 13310 | `	pCallback = &pEngine->aCB[PH7_XML_END_TAG];` |
|        - | 13311 | `	/* Make sure the given callback is callable */` |
|       63 | 13312 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13313 | `		/* Not callable,return immediately*/` |
|        9 | 13314 | `		return SXRET_OK;` |
|        - | 13315 | `	}` |
|        - | 13316 | `	/* Create a ph7_value holding the tag name */` |
|       55 | 13317 | `	pTag = VmXMLValue(pEngine,pEnd,pNS);` |
|       55 | 13318 | `	if( pTag == 0  ){` |
|      ! 0 | 13319 | `		SXUNUSED(pNS); /* cc warning */` |
|        - | 13320 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13321 | `		return SXRET_OK;` |
|        - | 13322 | `	}` |
|        - | 13323 | `	/* Invoke the user callback */` |
|       55 | 13324 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pTag,(ph7_value*)0);` |
|        - | 13325 | `	/* Clean-up the mess left behind */` |
|       55 | 13326 | `	ph7_context_release_value(pEngine->pCtx,pTag);` |
|       55 | 13327 | `	return SXRET_OK;` |
|       32 | 13328 |  |
|        - | 13329 | `/*` |
|        - | 13330 | ` * Character data handler.` |
|        - | 13331 | ` *  The user defined callback must accept two parameters:` |
|        - | 13332 | ` *  handler(resource $parser,string $data)` |
|        - | 13333 | ` *  $parser` |
|        - | 13334 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13335 | ` *  $data` |
|        - | 13336 | ` *   The second parameter, data, contains the character data as a string.` |
|        - | 13337 | ` *   Character data handler is called for every piece of a text in the XML document.` |
|        - | 13338 | ` *   It can be called multiple times inside each fragment (e.g. for non-ASCII strings).` |
|        - | 13339 | ` *   If a handler function is set to an empty string, or FALSE, the handler in question is disabled.` |
|        - | 13340 | ` *   Note: Instead of a function name, an array containing an object reference and a method name can also be supplied.` |
|        - | 13341 | ` */` |
|       28 | 13342 | `static sxi32 VmXMLTextHandler(SyXMLRawStr *pText,void *pUserData)` |
|        1 | 13343 |  |
|       29 | 13344 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13345 | `	ph7_value *pCallback,*pData;` |
|        - | 13346 | `	/* Point to the target user defined callback */` |
|       29 | 13347 | `	pCallback = &pEngine->aCB[PH7_XML_CDATA];` |
|        - | 13348 | `	/* Make sure the given callback is callable */` |
|       29 | 13349 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13350 | `		/* Not callable,return immediately*/` |
|       11 | 13351 | `		return SXRET_OK;` |
|        - | 13352 | `	}` |
|        - | 13353 | `	/* Create a ph7_value holding the data */` |
|       19 | 13354 | `	pData = VmXMLValue(pEngine,&(*pText),0);` |
|       19 | 13355 | `	if( pData == 0  ){` |
|        - | 13356 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13357 | `		return SXRET_OK;` |
|        - | 13358 | `	}` |
|        - | 13359 | `	/* Invoke the user callback */` |
|       19 | 13360 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pData,(ph7_value*)0);` |
|        - | 13361 | `	/* Clean-up the mess left behind */` |
|       19 | 13362 | `	ph7_context_release_value(pEngine->pCtx,pData);` |
|       19 | 13363 | `	return SXRET_OK;` |
|       15 | 13364 |  |
|        - | 13365 | `/*` |
|        - | 13366 | ` * Processing instruction (PI) handler.` |
|        - | 13367 | ` * The user defined callback must accept two parameters:` |
|        - | 13368 | ` *   handler(resource $parser,string $target,string $data)` |
|        - | 13369 | ` *  $parser` |
|        - | 13370 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13371 | ` *  $target` |
|        - | 13372 | ` *   The second parameter, target, contains the PI target.` |
|        - | 13373 | ` *  $data` |
|        - | 13374 | ` *    The third parameter, data, contains the PI data.` |
|        - | 13375 | ` *    Note: Instead of a function name, an array containing an object reference` |
|        - | 13376 | ` *    and a method name can also be supplied.` |
|        - | 13377 | ` */` |
|        8 | 13378 | `static sxi32 VmXMLPIHandler(SyXMLRawStr *pTargetStr,SyXMLRawStr *pDataStr,void *pUserData)` |
|        1 | 13379 |  |
|        9 | 13380 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13381 | `	ph7_value *pCallback,*pTarget,*pData;` |
|        - | 13382 | `	/* Point to the target user defined callback */` |
|        9 | 13383 | `	pCallback = &pEngine->aCB[PH7_XML_PI];` |
|        - | 13384 | `	/* Make sure the given callback is callable */` |
|        9 | 13385 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13386 | `		/* Not callable,return immediately*/` |
|        5 | 13387 | `		return SXRET_OK;` |
|        - | 13388 | `	}` |
|        - | 13389 | `	/* Get a ph7_value holding the data */` |
|        5 | 13390 | `	pTarget = VmXMLValue(pEngine,&(*pTargetStr),0);` |
|        5 | 13391 | `	pData = VmXMLValue(pEngine,&(*pDataStr),0);` |
|        5 | 13392 | `	if( pTarget == 0 \|\| pData == 0  ){` |
|        - | 13393 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13394 | `		return SXRET_OK;` |
|        - | 13395 | `	}` |
|        - | 13396 | `	/* Invoke the user callback */` |
|        5 | 13397 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pTarget,pData,(ph7_value*)0);` |
|        - | 13398 | `	/* Clean-up the mess left behind */` |
|        5 | 13399 | `	ph7_context_release_value(pEngine->pCtx,pTarget);` |
|        5 | 13400 | `	ph7_context_release_value(pEngine->pCtx,pData);` |
|        5 | 13401 | `	return SXRET_OK;` |
|        5 | 13402 |  |
|        - | 13403 | `/*` |
|        - | 13404 | ` * Namespace declaration handler.` |
|        - | 13405 | ` * The user defined callback must accept two parameters:` |
|        - | 13406 | ` *    handler(resource $parser,string $prefix,string $uri)` |
|        - | 13407 | ` * $parser` |
|        - | 13408 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13409 | ` * $prefix` |
|        - | 13410 | ` *   The prefix is a string used to reference the namespace within an XML object.` |
|        - | 13411 | ` * $uri` |
|        - | 13412 | ` *   Uniform Resource Identifier (URI) of namespace.` |
|        - | 13413 | ` *   Note: Instead of a function name, an array containing an object reference` |
|        - | 13414 | ` *   and a method name can also be supplied.` |
|        - | 13415 | ` */` |
|        4 | 13416 | `static sxi32 VmXMLNSStartHandler(SyXMLRawStr *pUriStr,SyXMLRawStr *pPrefixStr,void *pUserData)` |
|        1 | 13417 |  |
|        5 | 13418 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13419 | `	ph7_value *pCallback,*pUri,*pPrefix;` |
|        - | 13420 | `	/* Point to the target user defined callback */` |
|        5 | 13421 | `	pCallback = &pEngine->aCB[PH7_XML_NS_START];` |
|        - | 13422 | `	/* Make sure the given callback is callable */` |
|        5 | 13423 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13424 | `		/* Not callable,return immediately*/` |
|        3 | 13425 | `		return SXRET_OK;` |
|        - | 13426 | `	}` |
|        - | 13427 | `	/* Get a ph7_value holding the PREFIX/URI */` |
|        3 | 13428 | `	pUri = VmXMLValue(pEngine,pUriStr,0);` |
|        3 | 13429 | `	pPrefix = VmXMLValue(pEngine,pPrefixStr,0);` |
|        3 | 13430 | `	if( pUri == 0 \|\| pPrefix == 0  ){` |
|        - | 13431 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13432 | `		return SXRET_OK;` |
|        - | 13433 | `	}` |
|        - | 13434 | `	/* Invoke the user callback */` |
|        3 | 13435 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pUri,pPrefix,(ph7_value*)0);` |
|        - | 13436 | `	/* Clean-up the mess left behind */` |
|        3 | 13437 | `	ph7_context_release_value(pEngine->pCtx,pUri);` |
|        3 | 13438 | `	ph7_context_release_value(pEngine->pCtx,pPrefix);` |
|        3 | 13439 | `	return SXRET_OK;` |
|        3 | 13440 |  |
|        - | 13441 | `/*` |
|        - | 13442 | ` * Namespace end declaration handler.` |
|        - | 13443 | ` * The user defined callback must accept two parameters:` |
|        - | 13444 | ` *    handler(resource $parser,string $prefix)` |
|        - | 13445 | ` * $parser` |
|        - | 13446 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13447 | ` * $prefix` |
|        - | 13448 | ` *  The prefix is a string used to reference the namespace within an XML object.` |
|        - | 13449 | ` *   Note: Instead of a function name, an array containing an object reference` |
|        - | 13450 | ` *   and a method name can also be supplied.` |
|        - | 13451 | ` */` |
|        4 | 13452 | `static sxi32 VmXMLNSEndHandler(SyXMLRawStr *pPrefixStr,void *pUserData)` |
|        1 | 13453 |  |
|        5 | 13454 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13455 | `	ph7_value *pCallback,*pPrefix;` |
|        - | 13456 | `	/* Point to the target user defined callback */` |
|        5 | 13457 | `	pCallback = &pEngine->aCB[PH7_XML_NS_END];` |
|        - | 13458 | `	/* Make sure the given callback is callable */` |
|        5 | 13459 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13460 | `		/* Not callable,return immediately*/` |
|        3 | 13461 | `		return SXRET_OK;` |
|        - | 13462 | `	}` |
|        - | 13463 | `	/* Get a ph7_value holding the prefix */` |
|        3 | 13464 | `	pPrefix = VmXMLValue(pEngine,pPrefixStr,0);` |
|        3 | 13465 | `	if( pPrefix == 0 ){` |
|        - | 13466 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13467 | `		return SXRET_OK;` |
|        - | 13468 | `	}` |
|        - | 13469 | `	/* Invoke the user callback */` |
|        3 | 13470 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pPrefix,(ph7_value*)0);` |
|        - | 13471 | `	/* Clean-up the mess left behind */` |
|        3 | 13472 | `	ph7_context_release_value(pEngine->pCtx,pPrefix);` |
|        3 | 13473 | `	return SXRET_OK;` |
|        3 | 13474 |  |
|        - | 13475 | `/*` |
|        - | 13476 | ` * Error Message consumer handler.` |
|        - | 13477 | ` * Each time the XML parser encounter a syntaxt error or any other error` |
|        - | 13478 | ` * related to XML processing,the following callback is invoked by the` |
|        - | 13479 | ` * underlying XML parser.` |
|        - | 13480 | ` */` |
|       34 | 13481 | `static sxi32 VmXMLErrorHandler(const char *zMessage,sxi32 iErrCode,SyToken *pToken,void *pUserData)` |
|        1 | 13482 |  |
|       35 | 13483 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13484 | `	/* Save the error code */` |
|       35 | 13485 | `	pEngine->iErrCode = iErrCode;` |
|       17 | 13486 | `	SXUNUSED(zMessage); /* cc warning */` |
|       35 | 13487 | `	if( pToken ){` |
|       35 | 13488 | `		pEngine->nLine = pToken->nLine;` |
|       17 | 13489 | `	}` |
|        - | 13490 | `	/* Abort XML processing immediately */` |
|       35 | 13491 | `	return SXERR_ABORT;` |
|        1 | 13492 |  |
|        - | 13493 | `/*` |
|        - | 13494 | ` * int xml_parse(resource $parser,string $data[,bool $is_final = false ])` |
|        - | 13495 | ` *  Parses an XML document. The handlers for the configured events are called` |
|        - | 13496 | ` *  as many times as necessary.` |
|        - | 13497 | ` * Parameters` |
|        - | 13498 | ` *  $parser` |
|        - | 13499 | ` *   A reference to the XML parser.` |
|        - | 13500 | ` *  $data` |
|        - | 13501 | ` *   Chunk of data to parse. A document may be parsed piece-wise by calling` |
|        - | 13502 | ` *   xml_parse() several times with new data, as long as the is_final parameter` |
|        - | 13503 | ` *   is set and TRUE when the last data is parsed.` |
|        - | 13504 | ` * $is_final` |
|        - | 13505 | ` *   NOT USED. This implementation require that all the processed input be` |
|        - | 13506 | ` *   entirely loaded in memory.` |
|        - | 13507 | ` * Return` |
|        - | 13508 | ` *  Returns 1 on success or 0 on failure.` |
|        - | 13509 | ` */` |
|       74 | 13510 | `static int vm_builtin_xml_parse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13511 |  |
|        - | 13512 | `	ph7_xml_engine *pEngine;` |
|        - | 13513 | `	SyXMLParser *pParser;` |
|        - | 13514 | `	const char *zData;` |
|        - | 13515 | `	int nByte;` |
|       75 | 13516 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|        - | 13517 | `		/* Missing/Ivalid arguments,return FALSE */` |
|        3 | 13518 | `		ph7_result_bool(pCtx,0);` |
|        3 | 13519 | `		return PH7_OK;` |
|        - | 13520 | `	}` |
|        - | 13521 | `	/* Point to the XML engine */` |
|       73 | 13522 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|       73 | 13523 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13524 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13525 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13526 | `		return PH7_OK;` |
|        - | 13527 | `	}` |
|       73 | 13528 | `	if( pEngine->iNest > 0 ){` |
|        - | 13529 | `		/* This can happen when the user callback call xml_parse() again` |
|        - | 13530 | `		 * in it's body which is forbidden.` |
|        - | 13531 | `		 */` |
|      ! 0 | 13532 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,` |
|        - | 13533 | `			"Recursive call to %s,PH7 is returning false",` |
|      ! 0 | 13534 | `			ph7_function_name(pCtx)` |
|        - | 13535 | `			);` |
|        - | 13536 | `		/* Return FALSE */` |
|      ! 0 | 13537 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13538 | `		return PH7_OK;` |
|        - | 13539 | `	}` |
|       73 | 13540 | `	pEngine->pCtx = pCtx;` |
|        - | 13541 | `	/* Point to the underlying XML parser */` |
|       73 | 13542 | `	pParser = &pEngine->sParser;` |
|        - | 13543 | `	/* Register elements handler */` |
|       73 | 13544 | `	SyXMLParserSetEventHandler(pParser,pEngine,` |
|        - | 13545 | `		VmXMLStartElementHandler,` |
|        - | 13546 | `		VmXMLTextHandler,` |
|        - | 13547 | `		VmXMLErrorHandler,` |
|        - | 13548 | `		0,` |
|        - | 13549 | `		VmXMLEndElementHandler,` |
|        - | 13550 | `		VmXMLPIHandler,` |
|        - | 13551 | `		0,` |
|        - | 13552 | `		0,` |
|        - | 13553 | `		VmXMLNSStartHandler,` |
|        - | 13554 | `		VmXMLNSEndHandler` |
|        - | 13555 | `		);` |
|       73 | 13556 | `	pEngine->iErrCode = SXML_ERROR_NONE;` |
|        - | 13557 | `	/* Extract the raw XML input */` |
|       73 | 13558 | `	zData = ph7_value_to_string(apArg[1],&nByte);` |
|        - | 13559 | `	/* Start the parse process */` |
|       73 | 13560 | `	pEngine->iNest++;` |
|       73 | 13561 | `	SyXMLProcess(pParser,zData,(sxu32)nByte);` |
|       73 | 13562 | `	pEngine->iNest--;` |
|        - | 13563 | `	/* Return the parse result */` |
|       73 | 13564 | `	ph7_result_int(pCtx,pEngine->iErrCode == SXML_ERROR_NONE ? 1 : 0);` |
|       73 | 13565 | `	return PH7_OK;` |
|       38 | 13566 |  |
|        - | 13567 | `/*` |
|        - | 13568 | ` * bool xml_parser_set_option(resource $parser,int $option,mixed $value)` |
|        - | 13569 | ` *  Sets an option in an XML parser.` |
|        - | 13570 | ` * Parameters` |
|        - | 13571 | ` *  $parser` |
|        - | 13572 | ` *   A reference to the XML parser to set an option in.` |
|        - | 13573 | ` *  $option` |
|        - | 13574 | ` *    Which option to set. See below.` |
|        - | 13575 | ` *   The following options are available:` |
|        - | 13576 | ` *   XML_OPTION_CASE_FOLDING 	integer  Controls whether case-folding is enabled for this XML parser.` |
|        - | 13577 | ` *   XML_OPTION_SKIP_TAGSTART 	integer  Specify how many characters should be skipped in the beginning of a tag name.` |
|        - | 13578 | ` *   XML_OPTION_SKIP_WHITE 	    integer  Whether to skip values consisting of whitespace characters.` |
|        - | 13579 | ` *   XML_OPTION_TARGET_ENCODING string 	 Sets which target encoding to use in this XML parser.` |
|        - | 13580 | ` * $value` |
|        - | 13581 | ` *   The option's new value.` |
|        - | 13582 | ` * Return` |
|        - | 13583 | ` *  Returns 1 on success or 0 on failure.` |
|        - | 13584 | ` * Note:` |
|        - | 13585 | ` *  Well,none of these options have meaning under the built-in XML parser so a call to this` |
|        - | 13586 | ` *  function is a no-op.` |
|        - | 13587 | ` */` |
|        6 | 13588 | `static int vm_builtin_xml_parser_set_option(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13589 |  |
|        - | 13590 | `	ph7_xml_engine *pEngine;` |
|        7 | 13591 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13592 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13593 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13594 | `		return PH7_OK;` |
|        - | 13595 | `	}` |
|        - | 13596 | `	/* Point to the XML engine */` |
|        7 | 13597 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        7 | 13598 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13599 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13600 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13601 | `		return PH7_OK;` |
|        - | 13602 | `	}` |
|        - | 13603 | `	/* Always return FALSE */` |
|        7 | 13604 | `	ph7_result_bool(pCtx,0);` |
|        7 | 13605 | `	return PH7_OK;` |
|        4 | 13606 |  |
|        - | 13607 | `/*` |
|        - | 13608 | ` * mixed xml_parser_get_option(resource $parser,int $option)` |
|        - | 13609 | ` *  Get options from an XML parser.` |
|        - | 13610 | ` * Parameters` |
|        - | 13611 | ` *  $parser` |
|        - | 13612 | ` *   A reference to the XML parser to set an option in.` |
|        - | 13613 | ` * $option` |
|        - | 13614 | ` *   Which option to fetch.` |
|        - | 13615 | ` * Return` |
|        - | 13616 | ` *  This function returns FALSE if parser does not refer to a valid parser` |
|        - | 13617 | ` *  or if option isn't valid.Else the option's value is returned.` |
|        - | 13618 | ` */` |
|        2 | 13619 | `static int vm_builtin_xml_parser_get_option(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13620 |  |
|        - | 13621 | `	ph7_xml_engine *pEngine;` |
|        - | 13622 | `	int nOp;` |
|        3 | 13623 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13624 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13625 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13626 | `		return PH7_OK;` |
|        - | 13627 | `	}` |
|        - | 13628 | `	/* Point to the XML engine */` |
|        3 | 13629 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 13630 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13631 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13632 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13633 | `		return PH7_OK;` |
|        - | 13634 | `	}` |
|        - | 13635 | `	/* Extract the option */` |
|        3 | 13636 | `	nOp = ph7_value_to_int(apArg[1]);` |
|        3 | 13637 | `	switch(nOp){` |
|      ! 0 | 13638 | `	case SXML_OPTION_SKIP_TAGSTART:` |
|        - | 13639 | `	case SXML_OPTION_SKIP_WHITE:` |
|        - | 13640 | `	case SXML_OPTION_CASE_FOLDING:` |
|      ! 0 | 13641 | `		ph7_result_int(pCtx,0); break;` |
|      ! 0 | 13642 | `	case SXML_OPTION_TARGET_ENCODING:` |
|      ! 0 | 13643 | `		ph7_result_string(pCtx,"UTF-8",(int)sizeof("UTF-8")-1);` |
|      ! 0 | 13644 | `		break;` |
|        1 | 13645 | `	default:` |
|        - | 13646 | `		/* Unknown option,return FALSE*/` |
|        3 | 13647 | `		ph7_result_bool(pCtx,0);` |
|        2 | 13648 | `		break;` |
|        - | 13649 | `	}` |
|        3 | 13650 | `	return PH7_OK;` |
|        2 | 13651 |  |
|        - | 13652 | `/*` |
|        - | 13653 | ` * string xml_error_string(int $code)` |
|        - | 13654 | ` *  Gets the XML parser error string associated with the given code.` |
|        - | 13655 | ` * Parameters` |
|        - | 13656 | ` *  $code` |
|        - | 13657 | ` *   An error code from xml_get_error_code().` |
|        - | 13658 | ` * Return` |
|        - | 13659 | ` *  Returns a string with a textual description of the error` |
|        - | 13660 | ` *  code, or FALSE if no description was found.` |
|        - | 13661 | ` */` |
|       30 | 13662 | `static int vm_builtin_xml_error_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13663 |  |
|       31 | 13664 | `	int nErr = -1;` |
|       31 | 13665 | `	if( nArg > 0 ){` |
|       31 | 13666 | `		nErr = ph7_value_to_int(apArg[0]);` |
|       15 | 13667 | `	}` |
|       31 | 13668 | `	switch(nErr){` |
|        1 | 13669 | `	case SXML_ERROR_DUPLICATE_ATTRIBUTE:` |
|        3 | 13670 | `		ph7_result_string(pCtx,"Duplicate attribute",-1/*Compute length automatically*/);` |
|        3 | 13671 | `		break;` |
|      ! 0 | 13672 | `	case SXML_ERROR_INCORRECT_ENCODING:` |
|      ! 0 | 13673 | `		ph7_result_string(pCtx,"Incorrect encoding",-1);` |
|      ! 0 | 13674 | `		break;` |
|      ! 0 | 13675 | `	case SXML_ERROR_INVALID_TOKEN:` |
|      ! 0 | 13676 | `		ph7_result_string(pCtx,"Unexpected token",-1);` |
|      ! 0 | 13677 | `		break;` |
|        3 | 13678 | `	case SXML_ERROR_MISPLACED_XML_PI:` |
|        7 | 13679 | `		ph7_result_string(pCtx,"Misplaced processing instruction",-1);` |
|        7 | 13680 | `		break;` |
|      ! 0 | 13681 | `	case SXML_ERROR_NO_MEMORY:` |
|      ! 0 | 13682 | `		ph7_result_string(pCtx,"Out of memory",-1);` |
|      ! 0 | 13683 | `		break;` |
|        1 | 13684 | `	case SXML_ERROR_NONE:` |
|        3 | 13685 | `		ph7_result_string(pCtx,"Not an error",-1);` |
|        3 | 13686 | `		break;` |
|        1 | 13687 | `	case SXML_ERROR_TAG_MISMATCH:` |
|        3 | 13688 | `		ph7_result_string(pCtx,"Tag mismatch",-1);` |
|        3 | 13689 | `		break;` |
|      ! 0 | 13690 | `	case -1:` |
|      ! 0 | 13691 | `		ph7_result_string(pCtx,"Unknown error code",-1);` |
|      ! 0 | 13692 | `		break;` |
|        9 | 13693 | `	default:` |
|       19 | 13694 | `		ph7_result_string(pCtx,"Syntax error",-1);` |
|       18 | 13695 | `		break;` |
|        - | 13696 | `	}` |
|       31 | 13697 | `	return PH7_OK;` |
|        1 | 13698 |  |
|        - | 13699 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 13700 | `/*` |
|        - | 13701 | ` * int utf8_encode(string $input)` |
|        - | 13702 | ` *  UTF-8 encoding.` |
|        - | 13703 | ` *  This function encodes the string data to UTF-8, and returns the encoded version.` |
|        - | 13704 | ` *  UTF-8 is a standard mechanism used by Unicode for encoding wide character values` |
|        - | 13705 | ` * into a byte stream. UTF-8 is transparent to plain ASCII characters, is self-synchronized` |
|        - | 13706 | ` * (meaning it is possible for a program to figure out where in the bytestream characters start)` |
|        - | 13707 | ` * and can be used with normal string comparison functions for sorting and such.` |
|        - | 13708 | ` *  Notes on UTF-8 (According to SQLite3 authors):` |
|        - | 13709 | ` *  Byte-0    Byte-1    Byte-2    Byte-3    Value` |
|        - | 13710 | ` *  0xxxxxxx                                 00000000 00000000 0xxxxxxx` |
|        - | 13711 | ` *  110yyyyy  10xxxxxx                       00000000 00000yyy yyxxxxxx` |
|        - | 13712 | ` *  1110zzzz  10yyyyyy  10xxxxxx             00000000 zzzzyyyy yyxxxxxx` |
|        - | 13713 | ` *  11110uuu  10uuzzzz  10yyyyyy  10xxxxxx   000uuuuu zzzzyyyy yyxxxxxx` |
|        - | 13714 | ` * Parameters` |
|        - | 13715 | ` * $input` |
|        - | 13716 | ` *   String to encode or NULL on failure.` |
|        - | 13717 | ` * Return` |
|        - | 13718 | ` *  An UTF-8 encoded string.` |
|        - | 13719 | ` */` |
|        2 | 13720 | `static int vm_builtin_utf8_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13721 |  |
|        - | 13722 | `	const unsigned char *zIn,*zEnd;` |
|        - | 13723 | `	int nByte,c,e;` |
|        3 | 13724 | `	if( nArg < 1 ){` |
|        - | 13725 | `		/* Missing arguments,return null */` |
|      ! 0 | 13726 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13727 | `		return PH7_OK;` |
|        - | 13728 | `	}` |
|        - | 13729 | `	/* Extract the target string */` |
|        3 | 13730 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nByte);` |
|        3 | 13731 | `	if( nByte < 1 ){` |
|        - | 13732 | `		/* Empty string,return null */` |
|      ! 0 | 13733 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13734 | `		return PH7_OK;` |
|        - | 13735 | `	}` |
|        3 | 13736 | `	zEnd = &zIn[nByte];` |
|        - | 13737 | `	/* Start the encoding process */` |
|        2 | 13738 | `	for(;;){` |
|        5 | 13739 | `		if( zIn >= zEnd ){` |
|        - | 13740 | `			/* End of input */` |
|        3 | 13741 | `			break;` |
|        - | 13742 | `		}` |
|        3 | 13743 | `		c = zIn[0];` |
|        - | 13744 | `		/* Advance the stream cursor */` |
|        3 | 13745 | `		zIn++;` |
|        - | 13746 | `		/* Encode */` |
|        3 | 13747 | `		if( c<0x00080 ){` |
|      ! 0 | 13748 | `			e = (c&0xFF);` |
|      ! 0 | 13749 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|        3 | 13750 | `		}else if( c<0x00800 ){` |
|        3 | 13751 | `			e = 0xC0 + ((c>>6)&0x1F);` |
|        3 | 13752 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|        3 | 13753 | `			e = 0x80 + (c & 0x3F);` |
|        3 | 13754 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|        1 | 13755 | `		}else if( c<0x10000 ){` |
|      ! 0 | 13756 | `			e = 0xE0 + ((c>>12)&0x0F);` |
|      ! 0 | 13757 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13758 | `			e = 0x80 + ((c>>6) & 0x3F);` |
|      ! 0 | 13759 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13760 | `			e = 0x80 + (c & 0x3F);` |
|      ! 0 | 13761 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13762 | `		}else{` |
|      ! 0 | 13763 | `			e = 0xF0 + ((c>>18) & 0x07);` |
|      ! 0 | 13764 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13765 | `			e = 0x80 + ((c>>12) & 0x3F);` |
|      ! 0 | 13766 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13767 | `			e = 0x80 + ((c>>6) & 0x3F);` |
|      ! 0 | 13768 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13769 | `			e = 0x80 + (c & 0x3F);` |
|      ! 0 | 13770 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|        - | 13771 | `		}` |
|        1 | 13772 | `	}` |
|        - | 13773 | `	/* All done */` |
|        3 | 13774 | `	return PH7_OK;` |
|        2 | 13775 |  |
|        - | 13776 | `/*` |
|        - | 13777 | ` * UTF-8 decoding routine extracted from the sqlite3 source tree.` |
|        - | 13778 | ` * Original author: D. Richard Hipp (http://www.sqlite.org)` |
|        - | 13779 | ` * Status: Public Domain` |
|        - | 13780 | ` */` |
|        - | 13781 | `/*` |
|        - | 13782 | `** This lookup table is used to help decode the first byte of` |
|        - | 13783 | `** a multi-byte UTF8 character.` |
|        - | 13784 | `*/` |
|        - | 13785 | `static const unsigned char UtfTrans1[] = {` |
|        - | 13786 | `  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,` |
|        - | 13787 | `  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,` |
|        - | 13788 | `  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,` |
|        - | 13789 | `  0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,` |
|        - | 13790 | `  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,` |
|        - | 13791 | `  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,` |
|        - | 13792 | `  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,` |
|        - | 13793 | `  0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x00, 0x00,` |
|        - | 13794 | `};` |
|        - | 13795 | `/*` |
|        - | 13796 | `** Translate a single UTF-8 character.  Return the unicode value.` |
|        - | 13797 | `**` |
|        - | 13798 | `** During translation, assume that the byte that zTerm points` |
|        - | 13799 | `** is a 0x00.` |
|        - | 13800 | `**` |
|        - | 13801 | `** Write a pointer to the next unread byte back into *pzNext.` |
|        - | 13802 | `**` |
|        - | 13803 | `** Notes On Invalid UTF-8:` |
|        - | 13804 | `**` |
|        - | 13805 | `**  *  This routine never allows a 7-bit character (0x00 through 0x7f) to` |
|        - | 13806 | `**     be encoded as a multi-byte character.  Any multi-byte character that` |
|        - | 13807 | `**     attempts to encode a value between 0x00 and 0x7f is rendered as 0xfffd.` |
|        - | 13808 | `**` |
|        - | 13809 | `**  *  This routine never allows a UTF16 surrogate value to be encoded.` |
|        - | 13810 | `**     If a multi-byte character attempts to encode a value between` |
|        - | 13811 | `**     0xd800 and 0xe000 then it is rendered as 0xfffd.` |
|        - | 13812 | `**` |
|        - | 13813 | `**  *  Bytes in the range of 0x80 through 0xbf which occur as the first` |
|        - | 13814 | `**     byte of a character are interpreted as single-byte characters` |
|        - | 13815 | `**     and rendered as themselves even though they are technically` |
|        - | 13816 | `**     invalid characters.` |
|        - | 13817 | `**` |
|        - | 13818 | `**  *  This routine accepts an infinite number of different UTF8 encodings` |
|        - | 13819 | `**     for unicode values 0x80 and greater.  It do not change over-length` |
|        - | 13820 | `**     encodings to 0xfffd as some systems recommend.` |
|        - | 13821 | `*/` |
|        - | 13822 | `#define READ_UTF8(zIn, zTerm, c)                           \` |
|        - | 13823 | `  c = *(zIn++);                                            \` |
|        - | 13824 | `  if( c>=0xc0 ){                                           \` |
|        - | 13825 | `    c = UtfTrans1[c-0xc0];                                 \` |
|        - | 13826 | `    while( zIn!=zTerm && (*zIn & 0xc0)==0x80 ){            \` |
|        - | 13827 | `      c = (c<<6) + (0x3f & *(zIn++));                      \` |
|        - | 13828 | `    }                                                      \` |
|        - | 13829 | `    if( c<0x80                                             \` |
|        - | 13830 | `        \|\| (c&0xFFFFF800)==0xD800                          \` |
|        - | 13831 | `        \|\| (c&0xFFFFFFFE)==0xFFFE ){  c = 0xFFFD; }        \` |
|        - | 13832 | `  }` |
|      150 | 13833 | `PH7_PRIVATE int PH7_Utf8Read(` |
|        - | 13834 | `  const unsigned char *z,         /* First byte of UTF-8 character */` |
|        - | 13835 | `  const unsigned char *zTerm,     /* Pretend this byte is 0x00 */` |
|        - | 13836 | `  const unsigned char **pzNext    /* Write first byte past UTF-8 char here */` |
|        1 | 13837 | `){` |
|        - | 13838 | `  int c;` |
|      153 | 13839 | `  READ_UTF8(z, zTerm, c);` |
|      151 | 13840 | `  *pzNext = z;` |
|      151 | 13841 | `  return c;` |
|        1 | 13842 |  |
|        - | 13843 | `/*` |
|        - | 13844 | ` * string utf8_decode(string $data)` |
|        - | 13845 | ` *  This function decodes data, assumed to be UTF-8 encoded, to unicode.` |
|        - | 13846 | ` * Parameters` |
|        - | 13847 | ` * data` |
|        - | 13848 | ` *  An UTF-8 encoded string.` |
|        - | 13849 | ` * Return` |
|        - | 13850 | ` *  Unicode decoded string or NULL on failure.` |
|        - | 13851 | ` */` |
|        2 | 13852 | `static int vm_builtin_utf8_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13853 |  |
|        - | 13854 | `	const unsigned char *zIn,*zEnd;` |
|        - | 13855 | `	int nByte,c;` |
|        3 | 13856 | `	if( nArg < 1 ){` |
|        - | 13857 | `		/* Missing arguments,return null */` |
|      ! 0 | 13858 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13859 | `		return PH7_OK;` |
|        - | 13860 | `	}` |
|        - | 13861 | `	/* Extract the target string */` |
|        3 | 13862 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nByte);` |
|        3 | 13863 | `	if( nByte < 1 ){` |
|        - | 13864 | `		/* Empty string,return null */` |
|      ! 0 | 13865 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13866 | `		return PH7_OK;` |
|        - | 13867 | `	}` |
|        3 | 13868 | `	zEnd = &zIn[nByte];` |
|        - | 13869 | `	/* Start the decoding process */` |
|        5 | 13870 | `	while( zIn < zEnd ){` |
|        3 | 13871 | `		c = PH7_Utf8Read(zIn,zEnd,&zIn);` |
|        3 | 13872 | `		if( c == 0x0 ){` |
|      ! 0 | 13873 | `			break;` |
|        - | 13874 | `		}` |
|        3 | 13875 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|        1 | 13876 | `	}` |
|        3 | 13877 | `	return PH7_OK;` |
|        2 | 13878 |  |
|        - | 13879 | `/* Table of built-in VM functions. */` |
|        - | 13880 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 13881 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 13882 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 13883 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 13884 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 13885 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 13886 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 13887 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 13888 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 13889 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 13890 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 13891 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 13892 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 13893 | `	    /* Constants management */` |
|        - | 13894 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 13895 | `	{ "define",   vm_builtin_define               },` |
|        - | 13896 | `	{ "constant", vm_builtin_constant             },` |
|        - | 13897 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 13898 | `	   /* Class/Object functions */` |
|        - | 13899 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 13900 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 13901 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 13902 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 13903 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 13904 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 13905 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 13906 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 13907 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 13908 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 13909 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 13910 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 13911 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 13912 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 13913 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 13914 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 13915 | `	   /* Random numbers/strings generators */` |
|        - | 13916 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 13917 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 13918 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 13919 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 13920 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 13921 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 13922 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 13923 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 13924 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 13925 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 13926 | `	   /* Language constructs functions */` |
|        - | 13927 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 13928 | `	{ "print", vm_builtin_print                   },` |
|        - | 13929 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 13930 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 13931 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 13932 | `	  /* Variable handling functions */` |
|        - | 13933 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 13934 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 13935 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 13936 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 13937 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 13938 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 13939 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 13940 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 13941 | `	  /* Ouput control functions */` |
|        - | 13942 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 13943 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 13944 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 13945 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 13946 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 13947 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 13948 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 13949 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 13950 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 13951 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 13952 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 13953 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 13954 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 13955 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 13956 | `	  /* Assertion functions */` |
|        - | 13957 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 13958 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 13959 | `	  /* Error reporting functions */` |
|        - | 13960 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 13961 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 13962 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 13963 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 13964 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 13965 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 13966 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 13967 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 13968 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 13969 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 13970 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 13971 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 13972 | `	  /* Release info */` |
|        - | 13973 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 13974 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 13975 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 13976 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 13977 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 13978 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 13979 | `	  /* hashmap */` |
|        - | 13980 | `	{"compact",          vm_builtin_compact       },` |
|        - | 13981 | `	{"extract",          vm_builtin_extract       },` |
|        - | 13982 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 13983 | `	  /* URL related function */` |
|        - | 13984 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 13985 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 13986 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 13987 | `	   /* XML processing functions */` |
|        - | 13988 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 13989 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 13990 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 13991 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 13992 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 13993 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 13994 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 13995 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 13996 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 13997 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 13998 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 13999 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 14000 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 14001 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 14002 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 14003 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 14004 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 14005 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 14006 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 14007 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 14008 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 14009 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 14010 | `	   /* UTF-8 encoding/decoding */` |
|        - | 14011 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 14012 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 14013 | `	   /* Command line processing */` |
|        - | 14014 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 14015 | `	   /* JSON encoding/decoding */` |
|        - | 14016 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 14017 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 14018 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 14019 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 14020 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 14021 | `	   /* Files/URI inclusion facility */` |
|        - | 14022 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 14023 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 14024 | `	{ "include",      vm_builtin_include          },` |
|        - | 14025 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 14026 | `	{ "require",      vm_builtin_require          },` |
|        - | 14027 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 14028 | `};` |
|        - | 14029 | `/*` |
|        - | 14030 | ` * Register the built-in VM functions defined above.` |
|        - | 14031 | ` */` |
|      956 | 14032 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 14033 |  |
|        - | 14034 | `	sxi32 rc;` |
|        - | 14035 | `	sxu32 n;` |
|   119502 | 14036 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 14037 | `		/* Note that these special functions have access` |
|        - | 14038 | `		 * to the underlying virtual machine as their` |
|        - | 14039 | `		 * private data.` |
|        - | 14040 | `		 */` |
|   118546 | 14041 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   118546 | 14042 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 14043 | `			return rc;` |
|        - | 14044 | `		}` |
|    59274 | 14045 | `	}` |
|      958 | 14046 | `	return SXRET_OK;` |
|      480 | 14047 |  |
|        - | 14048 | `/*` |
|        - | 14049 | ` * Check if the given name refer to an installed class.` |
|        - | 14050 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 14051 | ` */` |
|     5564 | 14052 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 14053 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 14054 | `	const char *zName,  /* Name of the target class */` |
|        - | 14055 | `	sxu32 nByte,        /* zName length */` |
|        - | 14056 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 14057 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 14058 | `						 */` |
|        - | 14059 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 14060 | `	)` |
|        2 | 14061 |  |
|        - | 14062 | `	SyHashEntry *pEntry;` |
|        - | 14063 | `	ph7_class *pClass;` |
|     2782 | 14064 | `		SXUNUSED(iNest);` |
|        - | 14065 | `	/* Perform a hash lookup */` |
|     5566 | 14066 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|        - | 14067 |  |
|     5566 | 14068 | `	if( pEntry == 0 ){` |
|        - | 14069 | `		/* No such entry,return NULL */` |
|      ! 0 | 14070 | `		return 0;` |
|        - | 14071 | `	}` |
|     5566 | 14072 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|     5566 | 14073 | `	if( !iLoadable ){` |
|        - | 14074 | `		/* Return the first class seen */` |
|     5028 | 14075 | `		return pClass;` |
|      ! 0 | 14076 | `	}else{` |
|        - | 14077 | `		/* Check the collision list */` |
|      540 | 14078 | `		while(pClass){` |
|      540 | 14079 | `			if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT)) == 0 ){` |
|        - | 14080 | `				/* Class is loadable */` |
|      540 | 14081 | `				return pClass;` |
|        - | 14082 | `			}` |
|        - | 14083 | `			/* Point to the next entry */` |
|      ! 0 | 14084 | `			pClass = pClass->pNextName;` |
|      ! 0 | 14085 | `		}` |
|        - | 14086 | `	}` |
|        - | 14087 | `	/* No such loadable class */` |
|      ! 0 | 14088 | `	return 0;` |
|     2784 | 14089 |  |
|        - | 14090 | `/*` |
|        - | 14091 | ` * Reference Table Implementation` |
|        - | 14092 | ` * Status: stable <chm@symisc.net>` |
|        - | 14093 | ` * Intro` |
|        - | 14094 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 14095 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 14096 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 14097 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 14098 | ` *  Refer to the official for more information on this powerful` |
|        - | 14099 | ` *  extension.` |
|        - | 14100 | ` */` |
|        - | 14101 | `/*` |
|        - | 14102 | ` * Allocate a new reference entry.` |
|        - | 14103 | ` */` |
|   611000 | 14104 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 14105 |  |
|        - | 14106 | `	VmRefObj *pRef;` |
|        - | 14107 | `	/* Allocate a new instance */` |
|   611002 | 14108 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|   611002 | 14109 | `	if( pRef == 0 ){` |
|      ! 0 | 14110 | `		return 0;` |
|        - | 14111 | `	}` |
|        - | 14112 | `	/* Zero the structure */` |
|   611002 | 14113 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 14114 | `	/* Initialize fields */` |
|   611002 | 14115 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|   611002 | 14116 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|   611002 | 14117 | `	pRef->nIdx = nIdx;` |
|   611002 | 14118 | `	return pRef;` |
|   305502 | 14119 |  |
|        - | 14120 | `/*` |
|        - | 14121 | ` * Default hash function used by the reference table` |
|        - | 14122 | ` * for lookup/insertion operations.` |
|        - | 14123 | ` */` |
|  2776208 | 14124 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 14125 |  |
|        - | 14126 | `	/* Calculate the hash based on the memory object index */` |
|  2776210 | 14127 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 14128 |  |
|        - | 14129 | `/*` |
|        - | 14130 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 14131 | ` * in the reference table.` |
|        - | 14132 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 14133 | ` * otherwise.` |
|        - | 14134 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14135 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14136 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14137 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14138 | ` * Refer to the official for more information on this powerful` |
|        - | 14139 | ` * extension.` |
|        - | 14140 | ` */` |
|  1810704 | 14141 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 14142 |  |
|        - | 14143 | `	VmRefObj *pRef;` |
|        - | 14144 | `	sxu32 nBucket;` |
|        - | 14145 | `	/* Point to the appropriate bucket */` |
|  1810706 | 14146 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 14147 | `	/* Perform the lookup */` |
|  1810706 | 14148 | `	pRef = pVm->apRefObj[nBucket];` |
|  5273770 | 14149 | `	for(;;){` |
| 10549903 | 14150 | `		if( pRef == 0 ){` |
|   660936 | 14151 | `			break;` |
|        - | 14152 | `		}` |
|  9888969 | 14153 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 14154 | `			/* Entry found */` |
|  1149772 | 14155 | `			return pRef;` |
|        - | 14156 | `		}` |
|        - | 14157 | `		/* Point to the next entry */` |
|  8739199 | 14158 | `		pRef = pRef->pNextCollide;` |
|        2 | 14159 | `	}` |
|        - | 14160 | `	/* No such entry,return NULL */` |
|   660936 | 14161 | `	return 0;` |
|   905354 | 14162 |  |
|        - | 14163 | `/*` |
|        - | 14164 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 14165 | ` *` |
|        - | 14166 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14167 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14168 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14169 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14170 | ` * Refer to the official for more information on this powerful` |
|        - | 14171 | ` * extension.` |
|        - | 14172 | ` */` |
|   611000 | 14173 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 14174 |  |
|        - | 14175 | `	sxu32 nBucket;` |
|   611002 | 14176 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 14177 | `		VmRefObj **apNew;` |
|        - | 14178 | `		sxu32 nNew;` |
|        - | 14179 | `		/* Allocate a larger table */` |
|     1152 | 14180 | `		nNew = pVm->nRefSize << 1;` |
|     1152 | 14181 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     1152 | 14182 | `		if( apNew ){` |
|     1152 | 14183 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 14184 | `			sxu32 n;` |
|        - | 14185 | `			/* Zero the structure */` |
|     1152 | 14186 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 14187 | `			/* Rehash all referenced entries */` |
|    98018 | 14188 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 14189 | `				/* Remove old collision links */` |
|    96868 | 14190 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 14191 | `				/* Point to the appropriate bucket */` |
|    96868 | 14192 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 14193 | `				/* Insert the entry  */` |
|    96868 | 14194 | `				pEntry->pNextCollide = apNew[nBucket];` |
|    96868 | 14195 | `				if( apNew[nBucket] ){` |
|    82784 | 14196 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|    41391 | 14197 | `				}` |
|    96868 | 14198 | `				apNew[nBucket] = pEntry;` |
|        - | 14199 | `				/* Point to the next entry */` |
|    96868 | 14200 | `				pEntry = pEntry->pNext;` |
|    48435 | 14201 | `			}` |
|        - | 14202 | `			/* Release the old table */` |
|     1152 | 14203 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 14204 | `			/* Install the new one */` |
|     1152 | 14205 | `			pVm->apRefObj = apNew;` |
|     1152 | 14206 | `			pVm->nRefSize = nNew;` |
|      575 | 14207 | `		}` |
|      575 | 14208 | `	}` |
|        - | 14209 | `	/* Point to the appropriate bucket */` |
|   611002 | 14210 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 14211 | `	/* Insert the entry */` |
|   611002 | 14212 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|   611002 | 14213 | `	if( pVm->apRefObj[nBucket] ){` |
|   573867 | 14214 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|   286754 | 14215 | `	}` |
|   611002 | 14216 | `	pVm->apRefObj[nBucket] = pRef;` |
|   611002 | 14217 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|   611002 | 14218 | `	pVm->nRefUsed++;` |
|   611002 | 14219 | `	return SXRET_OK;` |
|        2 | 14220 |  |
|        - | 14221 | `/*` |
|        - | 14222 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 14223 | ` * the reference table.` |
|        - | 14224 | ` * This function is invoked when the user perform an unset` |
|        - | 14225 | ` * call [i.e: unset($var); ].` |
|        - | 14226 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14227 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14228 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14229 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14230 | ` * Refer to the official for more information on this powerful` |
|        - | 14231 | ` * extension.` |
|        - | 14232 | ` */` |
|   595256 | 14233 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 14234 |  |
|        - | 14235 | `	ph7_hashmap_node **apNode;` |
|        - | 14236 | `	SyHashEntry **apEntry;` |
|        - | 14237 | `	sxu32 n;` |
|        - | 14238 | `	/* Point to the reference table */` |
|   595258 | 14239 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|   595258 | 14240 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 14241 | `	/* Unlink the entry from the reference table */` |
|   649384 | 14242 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    54128 | 14243 | `		if( apEntry[n] ){` |
|    54096 | 14244 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    27047 | 14245 | `		}` |
|    27065 | 14246 | `	}` |
|  1140274 | 14247 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|   545018 | 14248 | `		if( apNode[n] ){` |
|     4881 | 14249 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     2440 | 14250 | `		}` |
|   272510 | 14251 | `	}` |
|   595258 | 14252 | `	if( pRef->pPrevCollide ){` |
|   337620 | 14253 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   168670 | 14254 | `	}else{` |
|   257640 | 14255 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 14256 | `	}` |
|   595258 | 14257 | `	if( pRef->pNextCollide ){` |
|   552379 | 14258 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   275948 | 14259 | `	}` |
|   595258 | 14260 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 14261 | `	/* Release the node */` |
|   595258 | 14262 | `	SySetRelease(&pRef->aReference);` |
|   595258 | 14263 | `	SySetRelease(&pRef->aArrEntries);` |
|   595258 | 14264 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|   595258 | 14265 | `	pVm->nRefUsed--;` |
|   595258 | 14266 | `	return SXRET_OK;` |
|        2 | 14267 |  |
|        - | 14268 | `/*` |
|        - | 14269 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 14270 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14271 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14272 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14273 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14274 | ` * Refer to the official for more information on this powerful` |
|        - | 14275 | ` * extension.` |
|        - | 14276 | ` */` |
|   625340 | 14277 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 14278 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 14279 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 14280 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 14281 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 14282 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 14283 | `	)` |
|        2 | 14284 |  |
|   625342 | 14285 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 14286 | `	VmRefObj *pRef;` |
|        - | 14287 | `	/* Check if the referenced object already exists */` |
|   625342 | 14288 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|   625342 | 14289 | `	if( pRef == 0 ){` |
|        - | 14290 | `		/* Create a new entry */` |
|   611002 | 14291 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|   611002 | 14292 | `		if( pRef == 0 ){` |
|      ! 0 | 14293 | `			return SXERR_MEM;` |
|        - | 14294 | `		}` |
|   611002 | 14295 | `		pRef->iFlags = iFlags;` |
|        - | 14296 | `		/* Install the entry */` |
|   611002 | 14297 | `		VmRefObjInsert(&(*pVm),pRef);` |
|   305500 | 14298 | `	}` |
|   630258 | 14299 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - | 14300 | `		/* Safely ignore the exception frame */` |
|     4918 | 14301 | `		pFrame = pFrame->pParent;` |
|        2 | 14302 | `	}` |
|   625342 | 14303 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 14304 | `		VmSlot sRef;` |
|        - | 14305 | `		/* Local frame,record referenced entry so that it can` |
|        - | 14306 | `		 * be deleted when we leave this frame.` |
|        - | 14307 | `		 */` |
|    49948 | 14308 | `		sRef.nIdx = nIdx;` |
|    49948 | 14309 | `		sRef.pUserData = pEntry;` |
|    49948 | 14310 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 14311 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 14312 | `		}` |
|    24973 | 14313 | `	}` |
|   625342 | 14314 | `	if( pEntry ){` |
|        - | 14315 | `		/* Address of the hash-entry */` |
|    64122 | 14316 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    32060 | 14317 | `	}` |
|   625342 | 14318 | `	if( pMapEntry ){` |
|        - | 14319 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|   559216 | 14320 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|   279607 | 14321 | `	}` |
|   625342 | 14322 | `	return SXRET_OK;` |
|   312672 | 14323 |  |
|        - | 14324 | `/*` |
|        - | 14325 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 14326 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14327 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14328 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14329 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14330 | ` * Refer to the official for more information on this powerful` |
|        - | 14331 | ` * extension.` |
|        - | 14332 | ` */` |
|   590088 | 14333 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 14334 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 14335 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 14336 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 14337 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 14338 | `	)` |
|        2 | 14339 |  |
|        - | 14340 | `	VmRefObj *pRef;` |
|        - | 14341 | `	sxu32 n;` |
|        - | 14342 | `	/* Check if the referenced object already exists */` |
|   590090 | 14343 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|   590090 | 14344 | `	if( pRef == 0 ){` |
|        - | 14345 | `		/* Not such entry */` |
|    49916 | 14346 | `		return SXERR_NOTFOUND;` |
|        - | 14347 | `	}` |
|        - | 14348 | `	/* Remove the desired entry */` |
|   540176 | 14349 | `	if( pEntry ){` |
|        - | 14350 | `		SyHashEntry **apEntry;` |
|       33 | 14351 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      129 | 14352 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|       97 | 14353 | `			if( apEntry[n] == pEntry ){` |
|        - | 14354 | `				/* Nullify the entry */` |
|       33 | 14355 | `				apEntry[n] = 0;` |
|        - | 14356 | `				/*` |
|        - | 14357 | `				 * NOTE:` |
|        - | 14358 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 14359 | `				 * we avoid wasting spaces.` |
|        - | 14360 | `				 */` |
|       16 | 14361 | `			}` |
|       49 | 14362 | `		}` |
|       16 | 14363 | `	}` |
|   540176 | 14364 | `	if( pMapEntry ){` |
|        - | 14365 | `		ph7_hashmap_node **apNode;` |
|   540144 | 14366 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  1080374 | 14367 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|   540232 | 14368 | `			if( apNode[n] == pMapEntry ){` |
|        - | 14369 | `				/* nullify the entry */` |
|   540144 | 14370 | `				apNode[n] = 0;` |
|   270071 | 14371 | `			}` |
|   270117 | 14372 | `		}` |
|   270071 | 14373 | `	}` |
|   540176 | 14374 | `	return SXRET_OK;` |
|   295046 | 14375 |  |
|        - | 14376 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 14377 | `/*` |
|        - | 14378 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 14379 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 14380 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 14381 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 14382 | ` * For more information on how to register IO stream devices,please` |
|        - | 14383 | ` * refer to the official documentation.` |
|        - | 14384 | ` */` |
|    18376 | 14385 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 14386 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 14387 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 14388 | `	int nByte              /* *pzDevice length*/` |
|        - | 14389 | `	)` |
|        2 | 14390 |  |
|        - | 14391 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 14392 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 14393 | `	SyString sDev,sCur;` |
|        - | 14394 | `	sxu32 n,nEntry;` |
|        - | 14395 | `	int rc;` |
|        - | 14396 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    18378 | 14397 | `	zNext = zCur = zIn = *pzDevice;` |
|    18378 | 14398 | `	zEnd = &zIn[nByte];` |
|  1114559 | 14399 | `	while( zIn < zEnd ){` |
|  1096185 | 14400 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 14401 | `			/* Got one */` |
|        3 | 14402 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 14403 | `			break;` |
|        - | 14404 | `		}` |
|        - | 14405 | `		/* Advance the cursor */` |
|  1096183 | 14406 | `		zIn++;` |
|        2 | 14407 | `	}` |
|    18378 | 14408 | `	if( zIn >= zEnd ){` |
|        - | 14409 | `		/* No such scheme,return the default stream */` |
|    18376 | 14410 | `		return pVm->pDefStream;` |
|        - | 14411 | `	}` |
|        3 | 14412 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 14413 | `	/* Remove leading and trailing white spaces */` |
|        3 | 14414 | `	SyStringFullTrim(&sDev);` |
|        - | 14415 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 14416 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 14417 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 14418 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 14419 | `		pStream = apStream[n];` |
|        3 | 14420 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 14421 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 14422 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 14423 | `		if( rc == 0 ){` |
|        - | 14424 | `			/* Stream device found */` |
|        3 | 14425 | `			*pzDevice = zNext;` |
|        3 | 14426 | `			return pStream;` |
|        - | 14427 | `		}` |
|      ! 0 | 14428 | `	}` |
|        - | 14429 | `	/* No such stream,return NULL */` |
|      ! 0 | 14430 | `	return 0;` |
|     9190 | 14431 |  |
|        - | 14432 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 14433 | `/*` |
|        - | 14434 | ` * Section:` |
|        - | 14435 | ` *    HTTP/URI related routines.` |
|        - | 14436 | ` * Status:` |
|        - | 14437 | ` *    Stable.` |
|        - | 14438 | ` */` |
|        - | 14439 | ` /*` |
|        - | 14440 | `  * URI Parser: Split an URI into components [i.e: Host,Path,Query,...].` |
|        - | 14441 | `  * URI syntax: [method:/][/[user[:pwd]@]host[:port]/][document]` |
|        - | 14442 | `  * This almost, but not quite, RFC1738 URI syntax.` |
|        - | 14443 | `  * This routine is not a validator,it does not check for validity` |
|        - | 14444 | `  * nor decode URI parts,the only thing this routine does is splitting` |
|        - | 14445 | `  * the input to its fields.` |
|        - | 14446 | `  * Upper layer are responsible of decoding and validating URI parts.` |
|        - | 14447 | `  * On success,this function populate the "SyhttpUri" structure passed` |
|        - | 14448 | `  * as the first argument. Otherwise SXERR_* is returned when a malformed` |
|        - | 14449 | `  * input is encountered.` |
|        - | 14450 | `  */` |
|       26 | 14451 | ` static sxi32 VmHttpSplitURI(SyhttpUri *pOut,const char *zUri,sxu32 nLen)` |
|        1 | 14452 | ` {` |
|       27 | 14453 | `	 const char *zEnd = &zUri[nLen];` |
|       27 | 14454 | `	 sxu8 bHostOnly = FALSE;` |
|       27 | 14455 | `	 sxu8 bIPv6 = FALSE	;` |
|        - | 14456 | `	 const char *zCur;` |
|        - | 14457 | `	 SyString *pComp;` |
|       27 | 14458 | `	 sxu32 nPos = 0;` |
|        - | 14459 | `	 sxi32 rc;` |
|        - | 14460 | `	 /* Zero the structure first */` |
|       27 | 14461 | `	 SyZero(pOut,sizeof(SyhttpUri));` |
|        - | 14462 | `	 /* Remove leading and trailing white spaces  */` |
|       27 | 14463 | `	 SyStringInitFromBuf(&pOut->sRaw,zUri,nLen);` |
|       27 | 14464 | `	 SyStringFullTrim(&pOut->sRaw);` |
|        - | 14465 | `	 /* Find the first '/' separator */` |
|       27 | 14466 | `	 rc = SyByteFind(zUri,(sxu32)(zEnd - zUri),'/',&nPos);` |
|       27 | 14467 | `	 if( rc != SXRET_OK ){` |
|        - | 14468 | `		 /* Assume a host name only */` |
|        7 | 14469 | `		 zCur = zEnd;` |
|        7 | 14470 | `		 bHostOnly = TRUE;` |
|        7 | 14471 | `		 goto ProcessHost;` |
|        - | 14472 | `	 }` |
|       21 | 14473 | `	 zCur = &zUri[nPos];` |
|       21 | 14474 | `	 if( zUri != zCur && zCur[-1] == ':' ){` |
|        - | 14475 | `		 /* Extract a scheme:` |
|        - | 14476 | `		  * Not that we can get an invalid scheme here.` |
|        - | 14477 | `		  * Fortunately the caller can discard any URI by comparing this scheme with its` |
|        - | 14478 | `		  * registered schemes and will report the error as soon as his comparison function` |
|        - | 14479 | `		  * fail.` |
|        - | 14480 | `		  */` |
|       19 | 14481 | `	 	pComp = &pOut->sScheme;` |
|       19 | 14482 | `		SyStringInitFromBuf(pComp,zUri,(sxu32)(zCur - zUri - 1));` |
|       19 | 14483 | `		SyStringLeftTrim(pComp);` |
|        9 | 14484 | `	 }` |
|       21 | 14485 | `	 if( zCur[1] != '/' ){` |
|      ! 0 | 14486 | `		 if( zCur == zUri \|\| zCur[-1] == ':' ){` |
|        - | 14487 | `		  /* No authority */` |
|      ! 0 | 14488 | `		  goto PathSplit;` |
|        - | 14489 | `		}` |
|        - | 14490 | `		 /* There is something here , we will assume its an authority` |
|        - | 14491 | `		  * and someone has forgot the two prefix slashes "//",` |
|        - | 14492 | `		  * sooner or later we will detect if we are dealing with a malicious` |
|        - | 14493 | `		  * user or not,but now assume we are dealing with an authority` |
|        - | 14494 | `		  * and let the caller handle all the validation process.` |
|        - | 14495 | `		  */` |
|      ! 0 | 14496 | `		 goto ProcessHost;` |
|        - | 14497 | `	 }` |
|       21 | 14498 | `	 zUri = &zCur[2];` |
|       21 | 14499 | `	 zCur = zEnd;` |
|       21 | 14500 | `	 rc = SyByteFind(zUri,(sxu32)(zEnd - zUri),'/',&nPos);` |
|       29 | 14501 | `	 if( rc == SXRET_OK ){` |
|       17 | 14502 | `		 zCur = &zUri[nPos];` |
|        8 | 14503 | `	 }` |
|        2 | 14504 | ` ProcessHost:` |
|        - | 14505 | `	 /* Extract user information if present */` |
|       27 | 14506 | `	 rc = SyByteFind(zUri,(sxu32)(zCur - zUri),'@',&nPos);` |
|       27 | 14507 | `	 if( rc == SXRET_OK ){` |
|        7 | 14508 | `		 if( nPos > 0 ){` |
|        - | 14509 | `			 sxu32 nPassOfft; /* Password offset */` |
|        7 | 14510 | `			 pComp = &pOut->sUser;` |
|        7 | 14511 | `			 SyStringInitFromBuf(pComp,zUri,nPos);` |
|        - | 14512 | `			 /* Extract the password if available */` |
|        7 | 14513 | `			 rc = SyByteFind(zUri,(sxu32)(zCur - zUri),':',&nPassOfft);` |
|        7 | 14514 | `			 if( rc == SXRET_OK && nPassOfft < nPos){` |
|        7 | 14515 | `				 pComp->nByte = nPassOfft;` |
|        7 | 14516 | `				 pComp = &pOut->sPass;` |
|        7 | 14517 | `				 pComp->zString = &zUri[nPassOfft+sizeof(char)];` |
|        7 | 14518 | `				 pComp->nByte = nPos - nPassOfft - 1;` |
|        3 | 14519 | `			 }` |
|        - | 14520 | `			 /* Update the cursor */` |
|        7 | 14521 | `			 zUri = &zUri[nPos+1];` |
|        4 | 14522 | `		 }else{` |
|      ! 0 | 14523 | `			 zUri++;` |
|        - | 14524 | `		 }` |
|        3 | 14525 | `	 }` |
|       27 | 14526 | `	 pComp = &pOut->sHost;` |
|       27 | 14527 | `	 while( zUri < zCur && SyisSpace(zUri[0])){` |
|      ! 0 | 14528 | `		 zUri++;` |
|      ! 0 | 14529 | `	 }` |
|       27 | 14530 | `	 SyStringInitFromBuf(pComp,zUri,(sxu32)(zCur - zUri));` |
|       27 | 14531 | `	 if( pComp->zString[0] == '[' ){` |
|        - | 14532 | `		 /* An IPv6 Address: Make a simple naive test` |
|        - | 14533 | `		  */` |
|        3 | 14534 | `		 zUri++; pComp->zString++; pComp->nByte = 0;` |
|        9 | 14535 | `		 while( ((unsigned char)zUri[0] < 0xc0 && SyisHex(zUri[0])) \|\| zUri[0] == ':' ){` |
|        7 | 14536 | `			 zUri++; pComp->nByte++;` |
|        1 | 14537 | `		 }` |
|        3 | 14538 | `		 if( zUri[0] != ']' ){` |
|      ! 0 | 14539 | `			 return SXERR_CORRUPT; /* Malformed IPv6 address */` |
|        - | 14540 | `		 }` |
|        3 | 14541 | `		 zUri++;` |
|        3 | 14542 | `		 bIPv6 = TRUE;` |
|        1 | 14543 | `	 }` |
|        - | 14544 | `	 /* Extract a port number if available */` |
|       27 | 14545 | `	 rc = SyByteFind(zUri,(sxu32)(zCur - zUri),':',&nPos);` |
|       27 | 14546 | `	 if( rc == SXRET_OK ){` |
|       11 | 14547 | `		 if( bIPv6 == FALSE ){` |
|       11 | 14548 | `			 pComp->nByte = (sxu32)(&zUri[nPos] - zUri);` |
|        5 | 14549 | `		 }` |
|       11 | 14550 | `		 pComp = &pOut->sPort;` |
|       11 | 14551 | `		 SyStringInitFromBuf(pComp,&zUri[nPos+1],(sxu32)(zCur - &zUri[nPos+1]));` |
|        5 | 14552 | `	 }` |
|       27 | 14553 | `	 if( bHostOnly == TRUE ){` |
|        7 | 14554 | `		 return SXRET_OK;` |
|        - | 14555 | `	 }` |
|       10 | 14556 | `PathSplit:` |
|       21 | 14557 | `	 zUri = zCur;` |
|       21 | 14558 | `	 pComp = &pOut->sPath;` |
|       21 | 14559 | `	 SyStringInitFromBuf(pComp,zUri,(sxu32)(zEnd-zUri));` |
|       21 | 14560 | `	 if( pComp->nByte == 0 ){` |
|        5 | 14561 | `		 return SXRET_OK; /* Empty path */` |
|        - | 14562 | `	 }` |
|       17 | 14563 | `	 if( SXRET_OK == SyByteFind(zUri,(sxu32)(zEnd-zUri),'?',&nPos) ){` |
|        5 | 14564 | `		 pComp->nByte = nPos; /* Update path length */` |
|        5 | 14565 | `		 pComp = &pOut->sQuery;` |
|        5 | 14566 | `		 SyStringInitFromBuf(pComp,&zUri[nPos+1],(sxu32)(zEnd-&zUri[nPos+1]));` |
|        2 | 14567 | `	 }` |
|       17 | 14568 | `	 if( SXRET_OK == SyByteFind(zUri,(sxu32)(zEnd-zUri),'#',&nPos) ){` |
|        - | 14569 | `		 /* Update path or query length */` |
|        5 | 14570 | `		 if( pComp == &pOut->sPath ){` |
|      ! 0 | 14571 | `			 pComp->nByte = nPos;` |
|      ! 0 | 14572 | `		 }else{` |
|        5 | 14573 | `			 if( &zUri[nPos] < (char *)SyStringData(pComp) ){` |
|        - | 14574 | `				 /* Malformed syntax : Query must be present before fragment */` |
|      ! 0 | 14575 | `				 return SXERR_SYNTAX;` |
|        - | 14576 | `			 }` |
|        5 | 14577 | `			 pComp->nByte -= (sxu32)(zEnd - &zUri[nPos]);` |
|        - | 14578 | `		 }` |
|        5 | 14579 | `		 pComp = &pOut->sFragment;` |
|        5 | 14580 | `		 SyStringInitFromBuf(pComp,&zUri[nPos+1],(sxu32)(zEnd-&zUri[nPos+1]))` |
|        2 | 14581 | `	 }` |
|       17 | 14582 | `	 return SXRET_OK;` |
|       14 | 14583 | ` }` |
|        - | 14584 | ` /*` |
|        - | 14585 | ` * Extract a single line from a raw HTTP request.` |
|        - | 14586 | ` * Return SXRET_OK on success,SXERR_EOF when end of input` |
|        - | 14587 | ` * and SXERR_MORE when more input is needed.` |
|        - | 14588 | ` */` |
|      ! 0 | 14589 | `static sxi32 VmGetNextLine(SyString *pCursor,SyString *pCurrent)` |
|      ! 0 | 14590 |  |
|        - | 14591 | `  	const char *zIn;` |
|        - | 14592 | `  	sxu32 nPos;` |
|        - | 14593 | `	/* Jump leading white spaces */` |
|      ! 0 | 14594 | `	SyStringLeftTrim(pCursor);` |
|      ! 0 | 14595 | `	if( pCursor->nByte < 1 ){` |
|      ! 0 | 14596 | `		SyStringInitFromBuf(pCurrent,0,0);` |
|      ! 0 | 14597 | `		return SXERR_EOF; /* End of input */` |
|        - | 14598 | `	}` |
|      ! 0 | 14599 | `	zIn = SyStringData(pCursor);` |
|      ! 0 | 14600 | `	if( SXRET_OK != SyByteListFind(pCursor->zString,pCursor->nByte,"\r\n",&nPos) ){` |
|        - | 14601 | `		/* Line not found,tell the caller to read more input from source */` |
|      ! 0 | 14602 | `		SyStringDupPtr(pCurrent,pCursor);` |
|      ! 0 | 14603 | `		return SXERR_MORE;` |
|        - | 14604 | `	}` |
|      ! 0 | 14605 | `  	pCurrent->zString = zIn;` |
|      ! 0 | 14606 | `  	pCurrent->nByte	= nPos;` |
|        - | 14607 | `  	/* advance the cursor so we can call this routine again */` |
|      ! 0 | 14608 | `  	pCursor->zString = &zIn[nPos];` |
|      ! 0 | 14609 | `  	pCursor->nByte -= nPos;` |
|      ! 0 | 14610 | `  	return SXRET_OK;` |
|      ! 0 | 14611 | ` }` |
|        - | 14612 | ` /*` |
|        - | 14613 | `  * Split a single MIME header into a name value pair.` |
|        - | 14614 | `  * This function return SXRET_OK,SXERR_CONTINUE on success.` |
|        - | 14615 | `  * Otherwise SXERR_NEXT is returned when a malformed header` |
|        - | 14616 | `  * is encountered.` |
|        - | 14617 | `  * Note: This function handle also mult-line headers.` |
|        - | 14618 | `  */` |
|      ! 0 | 14619 | ` static sxi32 VmHttpProcessOneHeader(SyhttpHeader *pHdr,SyhttpHeader *pLast,const char *zLine,sxu32 nLen)` |
|      ! 0 | 14620 | ` {` |
|        - | 14621 | `	 SyString *pName;` |
|        - | 14622 | `	 sxu32 nPos;` |
|        - | 14623 | `	 sxi32 rc;` |
|      ! 0 | 14624 | `	 if( nLen < 1 ){` |
|      ! 0 | 14625 | `		 return SXERR_NEXT;` |
|        - | 14626 | `	 }` |
|        - | 14627 | `	 /* Check for multi-line header */` |
|      ! 0 | 14628 | `	if( pLast && (zLine[-1] == ' ' \|\| zLine[-1] == '\t') ){` |
|      ! 0 | 14629 | `		SyString *pTmp = &pLast->sValue;` |
|      ! 0 | 14630 | `		SyStringFullTrim(pTmp);` |
|      ! 0 | 14631 | `		if( pTmp->nByte == 0 ){` |
|      ! 0 | 14632 | `			SyStringInitFromBuf(pTmp,zLine,nLen);` |
|      ! 0 | 14633 | `		}else{` |
|        - | 14634 | `			/* Update header value length */` |
|      ! 0 | 14635 | `			pTmp->nByte = (sxu32)(&zLine[nLen] - pTmp->zString);` |
|        - | 14636 | `		}` |
|        - | 14637 | `		 /* Simply tell the caller to reset its states and get another line */` |
|      ! 0 | 14638 | `		 return SXERR_CONTINUE;` |
|        - | 14639 | `	 }` |
|        - | 14640 | `	/* Split the header */` |
|      ! 0 | 14641 | `	pName = &pHdr->sName;` |
|      ! 0 | 14642 | `	rc = SyByteFind(zLine,nLen,':',&nPos);` |
|      ! 0 | 14643 | `	if(rc != SXRET_OK ){` |
|      ! 0 | 14644 | `		return SXERR_NEXT; /* Malformed header;Check the next entry */` |
|        - | 14645 | `	}` |
|      ! 0 | 14646 | `	SyStringInitFromBuf(pName,zLine,nPos);` |
|      ! 0 | 14647 | `	SyStringFullTrim(pName);` |
|        - | 14648 | `	/* Extract a header value */` |
|      ! 0 | 14649 | `	SyStringInitFromBuf(&pHdr->sValue,&zLine[nPos + 1],nLen - nPos - 1);` |
|        - | 14650 | `	/* Remove leading and trailing whitespaces */` |
|      ! 0 | 14651 | `	SyStringFullTrim(&pHdr->sValue);` |
|      ! 0 | 14652 | `	return SXRET_OK;` |
|      ! 0 | 14653 | ` }` |
|        - | 14654 | ` /*` |
|        - | 14655 | `  * Extract all MIME headers associated with a HTTP request.` |
|        - | 14656 | `  * After processing the first line of a HTTP request,the following` |
|        - | 14657 | `  * routine is called in order to extract MIME headers.` |
|        - | 14658 | `  * This function return SXRET_OK on success,SXERR_MORE when it needs` |
|        - | 14659 | `  * more inputs.` |
|        - | 14660 | `  * Note: Any malformed header is simply discarded.` |
|        - | 14661 | `  */` |
|      ! 0 | 14662 | ` static sxi32 VmHttpExtractHeaders(SyString *pRequest,SySet *pOut)` |
|      ! 0 | 14663 | ` {` |
|      ! 0 | 14664 | `	 SyhttpHeader *pLast = 0;` |
|        - | 14665 | `	 SyString sCurrent;` |
|        - | 14666 | `	 SyhttpHeader sHdr;` |
|        - | 14667 | `	 sxu8 bEol;` |
|        - | 14668 | `	 sxi32 rc;` |
|      ! 0 | 14669 | `	 if( SySetUsed(pOut) > 0 ){` |
|      ! 0 | 14670 | `		 pLast = (SyhttpHeader *)SySetAt(pOut,SySetUsed(pOut)-1);` |
|      ! 0 | 14671 | `	 }` |
|      ! 0 | 14672 | `	 bEol = FALSE;` |
|      ! 0 | 14673 | `	 for(;;){` |
|      ! 0 | 14674 | `		 SyZero(&sHdr,sizeof(SyhttpHeader));` |
|        - | 14675 | `		 /* Extract a single line from the raw HTTP request */` |
|      ! 0 | 14676 | `		 rc = VmGetNextLine(pRequest,&sCurrent);` |
|      ! 0 | 14677 | `		 if(rc != SXRET_OK ){` |
|      ! 0 | 14678 | `			 if( sCurrent.nByte < 1 ){` |
|      ! 0 | 14679 | `				 break;` |
|        - | 14680 | `			 }` |
|      ! 0 | 14681 | `			 bEol = TRUE;` |
|      ! 0 | 14682 | `		 }` |
|        - | 14683 | `		 /* Process the header */` |
|      ! 0 | 14684 | `		 if( SXRET_OK == VmHttpProcessOneHeader(&sHdr,pLast,sCurrent.zString,sCurrent.nByte)){` |
|      ! 0 | 14685 | `			 if( SXRET_OK != SySetPut(pOut,(const void *)&sHdr) ){` |
|      ! 0 | 14686 | `				 break;` |
|        - | 14687 | `			 }` |
|        - | 14688 | `			 /* Retrieve the last parsed header so we can handle multi-line header` |
|        - | 14689 | `			  * in case we face one of them.` |
|        - | 14690 | `			  */` |
|      ! 0 | 14691 | `			 pLast = (SyhttpHeader *)SySetPeek(pOut);` |
|      ! 0 | 14692 | `		 }` |
|      ! 0 | 14693 | `		 if( bEol ){` |
|      ! 0 | 14694 | `			 break;` |
|        - | 14695 | `		 }` |
|      ! 0 | 14696 | `	 } /* for(;;) */` |
|      ! 0 | 14697 | `	 return SXRET_OK;` |
|      ! 0 | 14698 | ` }` |
|        - | 14699 | ` /*` |
|        - | 14700 | `  * Process the first line of a HTTP request.` |
|        - | 14701 | `  * This routine perform the following operations` |
|        - | 14702 | `  *  1) Extract the HTTP method.` |
|        - | 14703 | `  *  2) Split the request URI to it's fields [ie: host,path,query,...].` |
|        - | 14704 | `  *  3) Extract the HTTP protocol version.` |
|        - | 14705 | `  */` |
|      ! 0 | 14706 | ` static sxi32 VmHttpProcessFirstLine(` |
|        - | 14707 | `	 SyString *pRequest, /* Raw HTTP request */` |
|        - | 14708 | `	 sxi32 *pMethod,     /* OUT: HTTP method */` |
|        - | 14709 | `	 SyhttpUri *pUri,    /* OUT: Parse of the URI */` |
|        - | 14710 | `	 sxi32 *pProto       /* OUT: HTTP protocol */` |
|        - | 14711 | `	 )` |
|      ! 0 | 14712 | ` {` |
|        - | 14713 | `	 static const char *azMethods[] = { "get","post","head","put"};` |
|        - | 14714 | `	 static const sxi32 aMethods[]  = { HTTP_METHOD_GET,HTTP_METHOD_POST,HTTP_METHOD_HEAD,HTTP_METHOD_PUT};` |
|        - | 14715 | `	 const char *zIn,*zEnd,*zPtr;` |
|        - | 14716 | `	 SyString sLine;` |
|        - | 14717 | `	 sxu32 nLen;` |
|        - | 14718 | `	 sxi32 rc;` |
|        - | 14719 | `	 /* Extract the first line and update the pointer */` |
|      ! 0 | 14720 | `	 rc = VmGetNextLine(pRequest,&sLine);` |
|      ! 0 | 14721 | `	 if( rc != SXRET_OK ){` |
|      ! 0 | 14722 | `		 return rc;` |
|        - | 14723 | `	 }` |
|      ! 0 | 14724 | `	 if ( sLine.nByte < 1 ){` |
|        - | 14725 | `		 /* Empty HTTP request */` |
|      ! 0 | 14726 | `		 return SXERR_EMPTY;` |
|        - | 14727 | `	 }` |
|        - | 14728 | `	 /* Delimit the line and ignore trailing and leading white spaces */` |
|      ! 0 | 14729 | `	 zIn = sLine.zString;` |
|      ! 0 | 14730 | `	 zEnd = &zIn[sLine.nByte];` |
|      ! 0 | 14731 | `	 while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|      ! 0 | 14732 | `		 zIn++;` |
|      ! 0 | 14733 | `	 }` |
|        - | 14734 | `	 /* Extract the HTTP method */` |
|      ! 0 | 14735 | `	 zPtr = zIn;` |
|      ! 0 | 14736 | `	 while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|      ! 0 | 14737 | `		 zIn++;` |
|      ! 0 | 14738 | `	 }` |
|      ! 0 | 14739 | `	 *pMethod = HTTP_METHOD_OTHR;` |
|      ! 0 | 14740 | `	 if( zIn > zPtr ){` |
|        - | 14741 | `		 sxu32 i;` |
|      ! 0 | 14742 | `		 nLen = (sxu32)(zIn-zPtr);` |
|      ! 0 | 14743 | `		 for( i = 0 ; i < SX_ARRAYSIZE(azMethods) ; ++i ){` |
|      ! 0 | 14744 | `			 if( SyStrnicmp(azMethods[i],zPtr,nLen) == 0 ){` |
|      ! 0 | 14745 | `				 *pMethod = aMethods[i];` |
|      ! 0 | 14746 | `				 break;` |
|        - | 14747 | `			 }` |
|      ! 0 | 14748 | `		 }` |
|      ! 0 | 14749 | `	 }` |
|        - | 14750 | `	 /* Jump trailing white spaces */` |
|      ! 0 | 14751 | `	 while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|      ! 0 | 14752 | `		 zIn++;` |
|      ! 0 | 14753 | `	 }` |
|        - | 14754 | `	  /* Extract the request URI */` |
|      ! 0 | 14755 | `	 zPtr = zIn;` |
|      ! 0 | 14756 | `	 while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|      ! 0 | 14757 | `		 zIn++;` |
|      ! 0 | 14758 | `	 }` |
|      ! 0 | 14759 | `	 if( zIn > zPtr ){` |
|      ! 0 | 14760 | `		 nLen = (sxu32)(zIn-zPtr);` |
|        - | 14761 | `		 /* Split raw URI to it's fields */` |
|      ! 0 | 14762 | `		 VmHttpSplitURI(pUri,zPtr,nLen);` |
|      ! 0 | 14763 | `	 }` |
|        - | 14764 | `	 /* Jump trailing white spaces */` |
|      ! 0 | 14765 | `	 while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|      ! 0 | 14766 | `		 zIn++;` |
|      ! 0 | 14767 | `	 }` |
|        - | 14768 | `	 /* Extract the HTTP version */` |
|      ! 0 | 14769 | `	 zPtr = zIn;` |
|      ! 0 | 14770 | `	 while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|      ! 0 | 14771 | `		 zIn++;` |
|      ! 0 | 14772 | `	 }` |
|      ! 0 | 14773 | `	 *pProto = HTTP_PROTO_11; /* HTTP/1.1 */` |
|      ! 0 | 14774 | `	 rc = 1;` |
|      ! 0 | 14775 | `	 if( zIn > zPtr ){` |
|      ! 0 | 14776 | `		 rc = SyStrnicmp(zPtr,"http/1.0",(sxu32)(zIn-zPtr));` |
|      ! 0 | 14777 | `	 }` |
|      ! 0 | 14778 | `	 if( !rc ){` |
|      ! 0 | 14779 | `		 *pProto = HTTP_PROTO_10; /* HTTP/1.0 */` |
|      ! 0 | 14780 | `	 }` |
|      ! 0 | 14781 | `	 return SXRET_OK;` |
|      ! 0 | 14782 | ` }` |
|        - | 14783 | ` /*` |
|        - | 14784 | `  * Tokenize,decode and split a raw query encoded as: "x-www-form-urlencoded"` |
|        - | 14785 | `  * into a name value pair.` |
|        - | 14786 | `  * Note that this encoding is implicit in GET based requests.` |
|        - | 14787 | `  * After the tokenization process,register the decoded queries` |
|        - | 14788 | `  * in the $_GET/$_POST/$_REQUEST superglobals arrays.` |
|        - | 14789 | `  */` |
|      ! 0 | 14790 | ` static sxi32 VmHttpSplitEncodedQuery(` |
|        - | 14791 | `	 ph7_vm *pVm,       /* Target VM */` |
|        - | 14792 | `	 SyString *pQuery,  /* Raw query to decode */` |
|        - | 14793 | `	 SyBlob *pWorker,   /* Working buffer */` |
|        - | 14794 | `	 int is_post        /* TRUE if we are dealing with a POST request */` |
|        - | 14795 | `	 )` |
|      ! 0 | 14796 | ` {` |
|      ! 0 | 14797 | `	 const char *zEnd = &pQuery->zString[pQuery->nByte];` |
|      ! 0 | 14798 | `	 const char *zIn = pQuery->zString;` |
|        - | 14799 | `	 ph7_value *pGet,*pRequest;` |
|        - | 14800 | `	 SyString sName,sValue;` |
|        - | 14801 | `	 const char *zPtr;` |
|        - | 14802 | `	 sxu32 nBlobOfft;` |
|        - | 14803 | `	 /* Extract superglobals */` |
|      ! 0 | 14804 | `	 if( is_post ){` |
|        - | 14805 | `		 /* $_POST superglobal */` |
|      ! 0 | 14806 | `		 pGet = VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|      ! 0 | 14807 | `	 }else{` |
|        - | 14808 | `		 /* $_GET superglobal */` |
|      ! 0 | 14809 | `		 pGet = VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|        - | 14810 | `	 }` |
|      ! 0 | 14811 | `	 pRequest = VmExtractSuper(&(*pVm),"_REQUEST",sizeof("_REQUEST")-1);` |
|        - | 14812 | `	 /* Split up the raw query */` |
|      ! 0 | 14813 | `	 for(;;){` |
|        - | 14814 | `		 /* Jump leading white spaces */` |
|      ! 0 | 14815 | `		 while(zIn < zEnd  && SyisSpace(zIn[0]) ){` |
|      ! 0 | 14816 | `			 zIn++;` |
|      ! 0 | 14817 | `		 }` |
|      ! 0 | 14818 | `		 if( zIn >= zEnd ){` |
|      ! 0 | 14819 | `			 break;` |
|        - | 14820 | `		 }` |
|      ! 0 | 14821 | `		 zPtr = zIn;` |
|      ! 0 | 14822 | `		 while( zPtr < zEnd && zPtr[0] != '=' && zPtr[0] != '&' && zPtr[0] != ';' ){` |
|      ! 0 | 14823 | `			 zPtr++;` |
|      ! 0 | 14824 | `		 }` |
|        - | 14825 | `		 /* Reset the working buffer */` |
|      ! 0 | 14826 | `		 SyBlobReset(pWorker);` |
|        - | 14827 | `		 /* Decode the entry */` |
|      ! 0 | 14828 | `		 SyUriDecode(zIn,(sxu32)(zPtr-zIn),PH7_VmBlobConsumer,pWorker,TRUE);` |
|        - | 14829 | `		 /* Save the entry */` |
|      ! 0 | 14830 | `		 sName.nByte = SyBlobLength(pWorker);` |
|      ! 0 | 14831 | `		 sValue.zString = 0;` |
|      ! 0 | 14832 | `		 sValue.nByte = 0;` |
|      ! 0 | 14833 | `		 if( zPtr < zEnd && zPtr[0] == '=' ){` |
|      ! 0 | 14834 | `			 zPtr++;` |
|      ! 0 | 14835 | `			 zIn = zPtr;` |
|        - | 14836 | `			 /* Store field value */` |
|      ! 0 | 14837 | `			 while( zPtr < zEnd && zPtr[0] != '&' && zPtr[0] != ';' ){` |
|      ! 0 | 14838 | `				 zPtr++;` |
|      ! 0 | 14839 | `			 }` |
|      ! 0 | 14840 | `			 if( zPtr > zIn ){` |
|        - | 14841 | `				 /* Decode the value */` |
|      ! 0 | 14842 | `				  nBlobOfft = SyBlobLength(pWorker);` |
|      ! 0 | 14843 | `				  SyUriDecode(zIn,(sxu32)(zPtr-zIn),PH7_VmBlobConsumer,pWorker,TRUE);` |
|      ! 0 | 14844 | `				  sValue.zString = (const char *)SyBlobDataAt(pWorker,nBlobOfft);` |
|      ! 0 | 14845 | `				  sValue.nByte = SyBlobLength(pWorker) - nBlobOfft;` |
|        - | 14846 |  |
|      ! 0 | 14847 | `			 }` |
|        - | 14848 | `			 /* Synchronize pointers */` |
|      ! 0 | 14849 | `			 zIn = zPtr;` |
|      ! 0 | 14850 | `		 }` |
|      ! 0 | 14851 | `		 sName.zString = (const char *)SyBlobData(pWorker);` |
|        - | 14852 | `		 /* Install the decoded query in the $_GET/$_REQUEST array */` |
|      ! 0 | 14853 | `		 if( pGet && (pGet->iFlags & MEMOBJ_HASHMAP) ){` |
|      ! 0 | 14854 | `			 VmHashmapInsert((ph7_hashmap *)pGet->x.pOther,` |
|      ! 0 | 14855 | `				 sName.zString,(int)sName.nByte,` |
|      ! 0 | 14856 | `				 sValue.zString,(int)sValue.nByte` |
|        - | 14857 | `				 );` |
|      ! 0 | 14858 | `		 }` |
|      ! 0 | 14859 | `		 if( pRequest && (pRequest->iFlags & MEMOBJ_HASHMAP) ){` |
|      ! 0 | 14860 | `			 VmHashmapInsert((ph7_hashmap *)pRequest->x.pOther,` |
|      ! 0 | 14861 | `				 sName.zString,(int)sName.nByte,` |
|      ! 0 | 14862 | `				 sValue.zString,(int)sValue.nByte` |
|        - | 14863 | `					 );` |
|      ! 0 | 14864 | `		 }` |
|        - | 14865 | `		 /* Advance the pointer */` |
|      ! 0 | 14866 | `		 zIn = &zPtr[1];` |
|      ! 0 | 14867 | `	 }` |
|        - | 14868 | `	/* All done*/` |
|      ! 0 | 14869 | `	return SXRET_OK;` |
|      ! 0 | 14870 | ` }` |
|        - | 14871 | ` /*` |
|        - | 14872 | `  * Extract MIME header value from the given set.` |
|        - | 14873 | `  * Return header value on success. NULL otherwise.` |
|        - | 14874 | `  */` |
|      ! 0 | 14875 | ` static SyString * VmHttpExtractHeaderValue(SySet *pSet,const char *zMime,sxu32 nByte)` |
|      ! 0 | 14876 | ` {` |
|        - | 14877 | `	 SyhttpHeader *aMime,*pMime;` |
|        - | 14878 | `	 SyString sMime;` |
|        - | 14879 | `	 sxu32 n;` |
|      ! 0 | 14880 | `	 SyStringInitFromBuf(&sMime,zMime,nByte);` |
|        - | 14881 | `	 /* Point to the MIME entries */` |
|      ! 0 | 14882 | `	 aMime = (SyhttpHeader *)SySetBasePtr(pSet);` |
|        - | 14883 | `	 /* Perform the lookup */` |
|      ! 0 | 14884 | `	 for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|      ! 0 | 14885 | `		 pMime = &aMime[n];` |
|      ! 0 | 14886 | `		 if( SyStringCmp(&sMime,&pMime->sName,SyStrnicmp) == 0 ){` |
|        - | 14887 | `			 /* Header found,return it's associated value */` |
|      ! 0 | 14888 | `			 return &pMime->sValue;` |
|        - | 14889 | `		 }` |
|      ! 0 | 14890 | `	 }` |
|        - | 14891 | `	 /* No such MIME header */` |
|      ! 0 | 14892 | `	 return 0;` |
|      ! 0 | 14893 | ` }` |
|        - | 14894 | ` /*` |
|        - | 14895 | `  * Tokenize and decode a raw "Cookie:" MIME header into a name value pair` |
|        - | 14896 | `  * and insert it's fields [i.e name,value] in the $_COOKIE superglobal.` |
|        - | 14897 | `  */` |
|      ! 0 | 14898 | ` static sxi32 VmHttpPorcessCookie(ph7_vm *pVm,SyBlob *pWorker,const char *zIn,sxu32 nByte)` |
|      ! 0 | 14899 | ` {` |
|      ! 0 | 14900 | `	 const char *zPtr,*zDelimiter,*zEnd = &zIn[nByte];` |
|        - | 14901 | `	 SyString sName,sValue;` |
|        - | 14902 | `	 ph7_value *pCookie;` |
|        - | 14903 | `	 sxu32 nOfft;` |
|        - | 14904 | `	 /* Make sure the $_COOKIE superglobal is available */` |
|      ! 0 | 14905 | `	 pCookie = VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 14906 | `	 if( pCookie == 0 \|\| (pCookie->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 14907 | `		 /* $_COOKIE superglobal not available */` |
|      ! 0 | 14908 | `		 return SXERR_NOTFOUND;` |
|        - | 14909 | `	 }` |
|      ! 0 | 14910 | `	 for(;;){` |
|        - | 14911 | `		  /* Jump leading white spaces */` |
|      ! 0 | 14912 | `		 while( zIn < zEnd && SyisSpace(zIn[0]) ){` |
|      ! 0 | 14913 | `			 zIn++;` |
|      ! 0 | 14914 | `		 }` |
|      ! 0 | 14915 | `		 if( zIn >= zEnd ){` |
|      ! 0 | 14916 | `			 break;` |
|        - | 14917 | `		 }` |
|        - | 14918 | `		  /* Reset the working buffer */` |
|      ! 0 | 14919 | `		 SyBlobReset(pWorker);` |
|      ! 0 | 14920 | `		 zDelimiter = zIn;` |
|        - | 14921 | `		 /* Delimit the name[=value]; pair */` |
|      ! 0 | 14922 | `		 while( zDelimiter < zEnd && zDelimiter[0] != ';' ){` |
|      ! 0 | 14923 | `			 zDelimiter++;` |
|      ! 0 | 14924 | `		 }` |
|      ! 0 | 14925 | `		 zPtr = zIn;` |
|      ! 0 | 14926 | `		 while( zPtr < zDelimiter && zPtr[0] != '=' ){` |
|      ! 0 | 14927 | `			 zPtr++;` |
|      ! 0 | 14928 | `		 }` |
|        - | 14929 | `		 /* Decode the cookie */` |
|      ! 0 | 14930 | `		 SyUriDecode(zIn,(sxu32)(zPtr-zIn),PH7_VmBlobConsumer,pWorker,TRUE);` |
|      ! 0 | 14931 | `		 sName.nByte = SyBlobLength(pWorker);` |
|      ! 0 | 14932 | `		 zPtr++;` |
|      ! 0 | 14933 | `		 sValue.zString = 0;` |
|      ! 0 | 14934 | `		 sValue.nByte = 0;` |
|      ! 0 | 14935 | `		 if( zPtr < zDelimiter ){` |
|        - | 14936 | `			 /* Got a Cookie value */` |
|      ! 0 | 14937 | `			 nOfft = SyBlobLength(pWorker);` |
|      ! 0 | 14938 | `			 SyUriDecode(zPtr,(sxu32)(zDelimiter-zPtr),PH7_VmBlobConsumer,pWorker,TRUE);` |
|      ! 0 | 14939 | `			 SyStringInitFromBuf(&sValue,SyBlobDataAt(pWorker,nOfft),SyBlobLength(pWorker)-nOfft);` |
|      ! 0 | 14940 | `		 }` |
|        - | 14941 | `		 /* Synchronize pointers */` |
|      ! 0 | 14942 | `		 zIn = &zDelimiter[1];` |
|        - | 14943 | `		 /* Perform the insertion */` |
|      ! 0 | 14944 | `		 sName.zString = (const char *)SyBlobData(pWorker);` |
|      ! 0 | 14945 | `		 VmHashmapInsert((ph7_hashmap *)pCookie->x.pOther,` |
|      ! 0 | 14946 | `			 sName.zString,(int)sName.nByte,` |
|      ! 0 | 14947 | `			 sValue.zString,(int)sValue.nByte` |
|        - | 14948 | `			 );` |
|      ! 0 | 14949 | `	 }` |
|      ! 0 | 14950 | `	 return SXRET_OK;` |
|      ! 0 | 14951 | ` }` |
|        - | 14952 | ` /*` |
|        - | 14953 | `  * Process a full HTTP request and populate the appropriate arrays` |
|        - | 14954 | `  * such as $_SERVER,$_GET,$_POST,$_COOKIE,$_REQUEST,... with the information` |
|        - | 14955 | `  * extracted from the raw HTTP request. As an extension Symisc introduced` |
|        - | 14956 | `  * the $_HEADER array which hold a copy of the processed HTTP MIME headers` |
|        - | 14957 | `  * and their associated values. [i.e: $_HEADER['Server'],$_HEADER['User-Agent'],...].` |
|        - | 14958 | `  * This function return SXRET_OK on success. Any other return value indicates` |
|        - | 14959 | `  * a malformed HTTP request.` |
|        - | 14960 | `  */` |
|      ! 0 | 14961 | ` static sxi32 VmHttpProcessRequest(ph7_vm *pVm,const char *zRequest,int nByte)` |
|      ! 0 | 14962 | ` {` |
|        - | 14963 | `	 SyString *pName,*pValue,sRequest; /* Raw HTTP request */` |
|        - | 14964 | `	 ph7_value *pHeaderArray;          /* $_HEADER superglobal (Symisc eXtension to the PHP specification)*/` |
|        - | 14965 | `	 SyhttpHeader *pHeader;            /* MIME header */` |
|        - | 14966 | `	 SyhttpUri sUri;     /* Parse of the raw URI*/` |
|        - | 14967 | `	 SyBlob sWorker;     /* General purpose working buffer */` |
|        - | 14968 | `	 SySet sHeader;      /* MIME headers set */` |
|        - | 14969 | `	 sxi32 iMethod;      /* HTTP method [i.e: GET,POST,HEAD...]*/` |
|        - | 14970 | `	 sxi32 iVer;         /* HTTP protocol version */` |
|        - | 14971 | `	 sxi32 rc;` |
|      ! 0 | 14972 | `	 SyStringInitFromBuf(&sRequest,zRequest,nByte);` |
|      ! 0 | 14973 | `	 SySetInit(&sHeader,&pVm->sAllocator,sizeof(SyhttpHeader));` |
|      ! 0 | 14974 | `	 SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        - | 14975 | `	 /* Ignore leading and trailing white spaces*/` |
|      ! 0 | 14976 | `	 SyStringFullTrim(&sRequest);` |
|        - | 14977 | `	 /* Process the first line */` |
|      ! 0 | 14978 | `	 rc = VmHttpProcessFirstLine(&sRequest,&iMethod,&sUri,&iVer);` |
|      ! 0 | 14979 | `	 if( rc != SXRET_OK ){` |
|      ! 0 | 14980 | `		 return rc;` |
|        - | 14981 | `	 }` |
|        - | 14982 | `	 /* Process MIME headers */` |
|      ! 0 | 14983 | `	 VmHttpExtractHeaders(&sRequest,&sHeader);` |
|        - | 14984 | `	 /*` |
|        - | 14985 | `	  * Setup $_SERVER environments` |
|        - | 14986 | `	  */` |
|        - | 14987 | `	 /* 'SERVER_PROTOCOL': Name and revision of the information protocol via which the page was requested */` |
|      ! 0 | 14988 | `	 ph7_vm_config(pVm,` |
|        - | 14989 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 14990 | `		 "SERVER_PROTOCOL",` |
|      ! 0 | 14991 | `		 iVer == HTTP_PROTO_10 ? "HTTP/1.0" : "HTTP/1.1",` |
|        - | 14992 | `		 sizeof("HTTP/1.1")-1` |
|        - | 14993 | `		 );` |
|        - | 14994 | `	 /* 'REQUEST_METHOD':  Which request method was used to access the page */` |
|      ! 0 | 14995 | `	 ph7_vm_config(pVm,` |
|        - | 14996 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 14997 | `		 "REQUEST_METHOD",` |
|      ! 0 | 14998 | `		 iMethod == HTTP_METHOD_GET ?   "GET" :` |
|      ! 0 | 14999 | `		 (iMethod == HTTP_METHOD_POST ? "POST":` |
|      ! 0 | 15000 | `		 (iMethod == HTTP_METHOD_PUT  ? "PUT" :` |
|      ! 0 | 15001 | `		 (iMethod == HTTP_METHOD_HEAD ?  "HEAD" : "OTHER"))),` |
|        - | 15002 | `		 -1 /* Compute attribute length automatically */` |
|        - | 15003 | `		 );` |
|      ! 0 | 15004 | `	 if( SyStringLength(&sUri.sQuery) > 0 && iMethod == HTTP_METHOD_GET ){` |
|      ! 0 | 15005 | `		 pValue = &sUri.sQuery;` |
|        - | 15006 | `		 /* 'QUERY_STRING': The query string, if any, via which the page was accessed */` |
|      ! 0 | 15007 | `		 ph7_vm_config(pVm,` |
|        - | 15008 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15009 | `			 "QUERY_STRING",` |
|      ! 0 | 15010 | `			 pValue->zString,` |
|      ! 0 | 15011 | `			 pValue->nByte` |
|        - | 15012 | `			 );` |
|        - | 15013 | `		 /* Decoded the raw query */` |
|      ! 0 | 15014 | `		 VmHttpSplitEncodedQuery(&(*pVm),pValue,&sWorker,FALSE);` |
|      ! 0 | 15015 | `	 }` |
|        - | 15016 | `	 /* REQUEST_URI: The URI which was given in order to access this page; for instance, '/index.html' */` |
|      ! 0 | 15017 | `	 pValue = &sUri.sRaw;` |
|      ! 0 | 15018 | `	 ph7_vm_config(pVm,` |
|        - | 15019 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15020 | `		 "REQUEST_URI",` |
|      ! 0 | 15021 | `		 pValue->zString,` |
|      ! 0 | 15022 | `		 pValue->nByte` |
|        - | 15023 | `		 );` |
|        - | 15024 | `	 /*` |
|        - | 15025 | `	  * 'PATH_INFO'` |
|        - | 15026 | `	  * 'ORIG_PATH_INFO'` |
|        - | 15027 | `      * Contains any client-provided pathname information trailing the actual script filename but preceding` |
|        - | 15028 | `	  * the query string, if available. For instance, if the current script was accessed via the URL` |
|        - | 15029 | `	  * http://www.example.com/php/path_info.php/some/stuff?foo=bar, then $_SERVER['PATH_INFO'] would contain` |
|        - | 15030 | `	  * /some/stuff.` |
|        - | 15031 | `	  */` |
|      ! 0 | 15032 | `	 pValue = &sUri.sPath;` |
|      ! 0 | 15033 | `	 ph7_vm_config(pVm,` |
|        - | 15034 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15035 | `		 "PATH_INFO",` |
|      ! 0 | 15036 | `		 pValue->zString,` |
|      ! 0 | 15037 | `		 pValue->nByte` |
|        - | 15038 | `		 );` |
|      ! 0 | 15039 | `	 ph7_vm_config(pVm,` |
|        - | 15040 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15041 | `		 "ORIG_PATH_INFO",` |
|      ! 0 | 15042 | `		 pValue->zString,` |
|      ! 0 | 15043 | `		 pValue->nByte` |
|        - | 15044 | `		 );` |
|        - | 15045 | `	 /* 'HTTP_ACCEPT': Contents of the Accept: header from the current request, if there is one */` |
|      ! 0 | 15046 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept",sizeof("Accept")-1);` |
|      ! 0 | 15047 | `	 if( pValue ){` |
|      ! 0 | 15048 | `		 ph7_vm_config(pVm,` |
|        - | 15049 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15050 | `			 "HTTP_ACCEPT",` |
|      ! 0 | 15051 | `			 pValue->zString,` |
|      ! 0 | 15052 | `			 pValue->nByte` |
|        - | 15053 | `		 );` |
|      ! 0 | 15054 | `	 }` |
|        - | 15055 | `	 /* 'HTTP_ACCEPT_CHARSET': Contents of the Accept-Charset: header from the current request, if there is one. */` |
|      ! 0 | 15056 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept-Charset",sizeof("Accept-Charset")-1);` |
|      ! 0 | 15057 | `	 if( pValue ){` |
|      ! 0 | 15058 | `		 ph7_vm_config(pVm,` |
|        - | 15059 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15060 | `			 "HTTP_ACCEPT_CHARSET",` |
|      ! 0 | 15061 | `			 pValue->zString,` |
|      ! 0 | 15062 | `			 pValue->nByte` |
|        - | 15063 | `		 );` |
|      ! 0 | 15064 | `	 }` |
|        - | 15065 | `	 /* 'HTTP_ACCEPT_ENCODING': Contents of the Accept-Encoding: header from the current request, if there is one. */` |
|      ! 0 | 15066 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept-Encoding",sizeof("Accept-Encoding")-1);` |
|      ! 0 | 15067 | `	 if( pValue ){` |
|      ! 0 | 15068 | `		 ph7_vm_config(pVm,` |
|        - | 15069 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15070 | `			 "HTTP_ACCEPT_ENCODING",` |
|      ! 0 | 15071 | `			 pValue->zString,` |
|      ! 0 | 15072 | `			 pValue->nByte` |
|        - | 15073 | `		 );` |
|      ! 0 | 15074 | `	 }` |
|        - | 15075 | `	  /* 'HTTP_ACCEPT_LANGUAGE': Contents of the Accept-Language: header from the current request, if there is one */` |
|      ! 0 | 15076 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept-Language",sizeof("Accept-Language")-1);` |
|      ! 0 | 15077 | `	 if( pValue ){` |
|      ! 0 | 15078 | `		 ph7_vm_config(pVm,` |
|        - | 15079 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15080 | `			 "HTTP_ACCEPT_LANGUAGE",` |
|      ! 0 | 15081 | `			 pValue->zString,` |
|      ! 0 | 15082 | `			 pValue->nByte` |
|        - | 15083 | `		 );` |
|      ! 0 | 15084 | `	 }` |
|        - | 15085 | `	 /* 'HTTP_CONNECTION': Contents of the Connection: header from the current request, if there is one. */` |
|      ! 0 | 15086 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Connection",sizeof("Connection")-1);` |
|      ! 0 | 15087 | `	 if( pValue ){` |
|      ! 0 | 15088 | `		 ph7_vm_config(pVm,` |
|        - | 15089 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15090 | `			 "HTTP_CONNECTION",` |
|      ! 0 | 15091 | `			 pValue->zString,` |
|      ! 0 | 15092 | `			 pValue->nByte` |
|        - | 15093 | `		 );` |
|      ! 0 | 15094 | `	 }` |
|        - | 15095 | `	 /* 'HTTP_HOST': Contents of the Host: header from the current request, if there is one. */` |
|      ! 0 | 15096 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Host",sizeof("Host")-1);` |
|      ! 0 | 15097 | `	 if( pValue ){` |
|      ! 0 | 15098 | `		 ph7_vm_config(pVm,` |
|        - | 15099 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15100 | `			 "HTTP_HOST",` |
|      ! 0 | 15101 | `			 pValue->zString,` |
|      ! 0 | 15102 | `			 pValue->nByte` |
|        - | 15103 | `		 );` |
|      ! 0 | 15104 | `	 }` |
|        - | 15105 | `	 /* 'HTTP_REFERER': Contents of the Referer: header from the current request, if there is one. */` |
|      ! 0 | 15106 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Referer",sizeof("Referer")-1);` |
|      ! 0 | 15107 | `	 if( pValue ){` |
|      ! 0 | 15108 | `		 ph7_vm_config(pVm,` |
|        - | 15109 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15110 | `			 "HTTP_REFERER",` |
|      ! 0 | 15111 | `			 pValue->zString,` |
|      ! 0 | 15112 | `			 pValue->nByte` |
|        - | 15113 | `		 );` |
|      ! 0 | 15114 | `	 }` |
|        - | 15115 | `	 /* 'HTTP_USER_AGENT': Contents of the Referer: header from the current request, if there is one. */` |
|      ! 0 | 15116 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"User-Agent",sizeof("User-Agent")-1);` |
|      ! 0 | 15117 | `	 if( pValue ){` |
|      ! 0 | 15118 | `		 ph7_vm_config(pVm,` |
|        - | 15119 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15120 | `			 "HTTP_USER_AGENT",` |
|      ! 0 | 15121 | `			 pValue->zString,` |
|      ! 0 | 15122 | `			 pValue->nByte` |
|        - | 15123 | `		 );` |
|      ! 0 | 15124 | `	 }` |
|        - | 15125 | `	  /* 'PHP_AUTH_DIGEST': When doing Digest HTTP authentication this variable is set to the 'Authorization'` |
|        - | 15126 | `	   * header sent by the client (which you should then use to make the appropriate validation).` |
|        - | 15127 | `	   */` |
|      ! 0 | 15128 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Authorization",sizeof("Authorization")-1);` |
|      ! 0 | 15129 | `	 if( pValue ){` |
|      ! 0 | 15130 | `		 ph7_vm_config(pVm,` |
|        - | 15131 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15132 | `			 "PHP_AUTH_DIGEST",` |
|      ! 0 | 15133 | `			 pValue->zString,` |
|      ! 0 | 15134 | `			 pValue->nByte` |
|        - | 15135 | `		 );` |
|      ! 0 | 15136 | `		 ph7_vm_config(pVm,` |
|        - | 15137 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15138 | `			 "PHP_AUTH",` |
|      ! 0 | 15139 | `			 pValue->zString,` |
|      ! 0 | 15140 | `			 pValue->nByte` |
|        - | 15141 | `		 );` |
|      ! 0 | 15142 | `	 }` |
|        - | 15143 | `	 /* Install all clients HTTP headers in the $_HEADER superglobal */` |
|      ! 0 | 15144 | `	 pHeaderArray = VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|        - | 15145 | `	 /* Iterate throw the available MIME headers*/` |
|      ! 0 | 15146 | `	 SySetResetCursor(&sHeader);` |
|      ! 0 | 15147 | `	 pHeader = 0; /* stupid cc warning */` |
|      ! 0 | 15148 | `	 while( SXRET_OK == SySetGetNextEntry(&sHeader,(void **)&pHeader) ){` |
|      ! 0 | 15149 | `		 pName  = &pHeader->sName;` |
|      ! 0 | 15150 | `		 pValue = &pHeader->sValue;` |
|      ! 0 | 15151 | `		 if( pHeaderArray && (pHeaderArray->iFlags & MEMOBJ_HASHMAP)){` |
|        - | 15152 | `			 /* Insert the MIME header and it's associated value */` |
|      ! 0 | 15153 | `			 VmHashmapInsert((ph7_hashmap *)pHeaderArray->x.pOther,` |
|      ! 0 | 15154 | `				 pName->zString,(int)pName->nByte,` |
|      ! 0 | 15155 | `				 pValue->zString,(int)pValue->nByte` |
|        - | 15156 | `				 );` |
|      ! 0 | 15157 | `		 }` |
|      ! 0 | 15158 | `		 if( pName->nByte == sizeof("Cookie")-1 && SyStrnicmp(pName->zString,"Cookie",sizeof("Cookie")-1) == 0` |
|      ! 0 | 15159 | `			 && pValue->nByte > 0){` |
|        - | 15160 | `				 /* Process the name=value pair and insert them in the $_COOKIE superglobal array */` |
|      ! 0 | 15161 | `				 VmHttpPorcessCookie(&(*pVm),&sWorker,pValue->zString,pValue->nByte);` |
|      ! 0 | 15162 | `		 }` |
|      ! 0 | 15163 | `	 }` |
|      ! 0 | 15164 | `	 if( iMethod == HTTP_METHOD_POST ){` |
|        - | 15165 | `		 /* Extract raw POST data */` |
|      ! 0 | 15166 | `		 pValue = VmHttpExtractHeaderValue(&sHeader,"Content-Type",sizeof("Content-Type") - 1);` |
|      ! 0 | 15167 | `		 if( pValue && pValue->nByte >= sizeof("application/x-www-form-urlencoded") - 1 &&` |
|      ! 0 | 15168 | `			 SyMemcmp("application/x-www-form-urlencoded",pValue->zString,pValue->nByte) == 0 ){` |
|        - | 15169 | `				 /* Extract POST data length */` |
|      ! 0 | 15170 | `				 pValue = VmHttpExtractHeaderValue(&sHeader,"Content-Length",sizeof("Content-Length") - 1);` |
|      ! 0 | 15171 | `				 if( pValue ){` |
|      ! 0 | 15172 | `					 sxi32 iLen = 0; /* POST data length */` |
|      ! 0 | 15173 | `					 SyStrToInt32(pValue->zString,pValue->nByte,(void *)&iLen,0);` |
|      ! 0 | 15174 | `					 if( iLen > 0 ){` |
|        - | 15175 | `						 /* Remove leading and trailing white spaces */` |
|      ! 0 | 15176 | `						 SyStringFullTrim(&sRequest);` |
|      ! 0 | 15177 | `						 if( (int)sRequest.nByte > iLen ){` |
|      ! 0 | 15178 | `							 sRequest.nByte = (sxu32)iLen;` |
|      ! 0 | 15179 | `						 }` |
|        - | 15180 | `						 /* Decode POST data now */` |
|      ! 0 | 15181 | `						 VmHttpSplitEncodedQuery(&(*pVm),&sRequest,&sWorker,TRUE);` |
|      ! 0 | 15182 | `					 }` |
|      ! 0 | 15183 | `				 }` |
|      ! 0 | 15184 | `		 }` |
|      ! 0 | 15185 | `	 }` |
|        - | 15186 | `	 /* All done,clean-up the mess left behind */` |
|      ! 0 | 15187 | `	 SySetRelease(&sHeader);` |
|      ! 0 | 15188 | `	 SyBlobRelease(&sWorker);` |
|      ! 0 | 15189 | `	 return SXRET_OK;` |
|      ! 0 | 15190 | ` }` |
|        - | 15191 |  |
