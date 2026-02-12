# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5093/7224 lines (70.50%)

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
|        - |   111 | `/*` |
|        - |   112 | ` * Each parsed URI is recorded and stored in an instance of the following structure.` |
|        - |   113 | ` * This structure and it's related routines are taken verbatim from the xHT project` |
|        - |   114 | ` * [A modern embeddable HTTP engine implementing all the RFC2616 methods]` |
|        - |   115 | ` * the xHT project is developed internally by Symisc Systems.` |
|        - |   116 | ` */` |
|        - |   117 | `typedef struct SyhttpUri SyhttpUri;` |
|        - |   118 | `struct SyhttpUri` |
|        - |   119 |  |
|        - |   120 | `	SyString sHost;     /* Hostname or IP address */` |
|        - |   121 | `	SyString sPort;     /* Port number */` |
|        - |   122 | `	SyString sPath;     /* Mandatory resource path passed verbatim (Not decoded) */` |
|        - |   123 | `	SyString sQuery;    /* Query part */` |
|        - |   124 | `	SyString sFragment; /* Fragment part */` |
|        - |   125 | `	SyString sScheme;   /* Scheme */` |
|        - |   126 | `	SyString sUser;     /* Username */` |
|        - |   127 | `	SyString sPass;     /* Password */` |
|        - |   128 | `	SyString sRaw;      /* Raw URI */` |
|        - |   129 | `};` |
|        - |   130 | `/*` |
|        - |   131 | ` * An instance of the following structure is used to record all MIME headers seen` |
|        - |   132 | ` * during a HTTP interaction.` |
|        - |   133 | ` * This structure and it's related routines are taken verbatim from the xHT project` |
|        - |   134 | ` * [A modern embeddable HTTP engine implementing all the RFC2616 methods]` |
|        - |   135 | ` * the xHT project is developed internally by Symisc Systems.` |
|        - |   136 | ` */` |
|        - |   137 | `typedef struct SyhttpHeader SyhttpHeader;` |
|        - |   138 | `struct SyhttpHeader` |
|        - |   139 |  |
|        - |   140 | `	SyString sName;    /* Header name [i.e:"Content-Type","Host","User-Agent"]. NOT NUL TERMINATED */` |
|        - |   141 | `	SyString sValue;   /* Header values [i.e: "text/html"]. NOT NUL TERMINATED */` |
|        - |   142 | `};` |
|        - |   143 | `/*` |
|        - |   144 | ` * Supported HTTP methods.` |
|        - |   145 | ` */` |
|        - |   146 | `#define HTTP_METHOD_GET  1 /* GET */` |
|        - |   147 | `#define HTTP_METHOD_HEAD 2 /* HEAD */` |
|        - |   148 | `#define HTTP_METHOD_POST 3 /* POST */` |
|        - |   149 | `#define HTTP_METHOD_PUT  4 /* PUT */` |
|        - |   150 | `#define HTTP_METHOD_OTHR 5 /* Other HTTP methods [i.e: DELETE,TRACE,OPTIONS...]*/` |
|        - |   151 | `/*` |
|        - |   152 | ` * Supported HTTP protocol version.` |
|        - |   153 | ` */` |
|        - |   154 | `#define HTTP_PROTO_10 1 /* HTTP/1.0 */` |
|        - |   155 | `#define HTTP_PROTO_11 2 /* HTTP/1.1 */` |
|        - |   156 | `/*` |
|        - |   157 | ` * Register a constant and it's associated expansion callback so that` |
|        - |   158 | ` * it can be expanded from the target PHP program.` |
|        - |   159 | ` * The constant expansion mechanism under PH7 is extremely powerful yet` |
|        - |   160 | ` * simple and work as follows:` |
|        - |   161 | ` * Each registered constant have a C procedure associated with it.` |
|        - |   162 | ` * This procedure known as the constant expansion callback is responsible` |
|        - |   163 | ` * of expanding the invoked constant to the desired value,for example:` |
|        - |   164 | ` * The C procedure associated with the "__PI__" constant expands to 3.14 (the value of PI).` |
|        - |   165 | ` * The "__OS__" constant procedure expands to the name of the host Operating Systems` |
|        - |   166 | ` * (Windows,Linux,...) and so on.` |
|        - |   167 | ` * Please refer to the official documentation for additional information.` |
|        - |   168 | ` */` |
|   184818 |   169 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
|        - |   170 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |   171 | `	const SyString *pName,  /* Constant name */` |
|        - |   172 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|        - |   173 | `	void *pUserData         /* Last argument to xExpand() */` |
|        - |   174 | `	)` |
|        2 |   175 |  |
|        - |   176 | `	ph7_constant *pCons;` |
|        - |   177 | `	SyHashEntry *pEntry;` |
|        - |   178 | `	char *zDupName;` |
|        - |   179 | `	sxi32 rc;` |
|   184820 |   180 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   184820 |   181 | `	if( pEntry ){` |
|        - |   182 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   183 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   184 | `		pCons->xExpand = xExpand;` |
|        6 |   185 | `		pCons->pUserData = pUserData;` |
|        6 |   186 | `		return SXRET_OK;` |
|        - |   187 | `	}` |
|        - |   188 | `	/* Allocate a new constant instance */` |
|   184816 |   189 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   184816 |   190 | `	if( pCons == 0 ){` |
|      ! 0 |   191 | `		return 0;` |
|        - |   192 | `	}` |
|        - |   193 | `	/* Duplicate constant name */` |
|   184816 |   194 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   184816 |   195 | `	if( zDupName == 0 ){` |
|      ! 0 |   196 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   197 | `		return 0;` |
|        - |   198 | `	}` |
|        - |   199 | `	/* Install the constant */` |
|   184816 |   200 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   184816 |   201 | `	pCons->xExpand = xExpand;` |
|   184816 |   202 | `	pCons->pUserData = pUserData;` |
|   184816 |   203 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   184816 |   204 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   205 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   206 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   207 | `		return rc;` |
|        - |   208 | `	}` |
|        - |   209 | `	/* All done,constant can be invoked from PHP code */` |
|   184816 |   210 | `	return SXRET_OK;` |
|    92411 |   211 |  |
|        - |   212 | `/*` |
|        - |   213 | ` * Allocate a new foreign function instance.` |
|        - |   214 | ` * This function return SXRET_OK on success. Any other` |
|        - |   215 | ` * return value indicates failure.` |
|        - |   216 | ` * Please refer to the official documentation for an introduction to` |
|        - |   217 | ` * the foreign function mechanism.` |
|        - |   218 | ` */` |
|   401940 |   219 | `static sxi32 PH7_NewForeignFunction(` |
|        - |   220 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   221 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   222 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   223 | `	void *pUserData,          /* Foreign function private data */` |
|        - |   224 | `	ph7_user_func **ppOut     /* OUT: VM image of the foreign function */` |
|        - |   225 | `	)` |
|        2 |   226 |  |
|        - |   227 | `	ph7_user_func *pFunc;` |
|        - |   228 | `	char *zDup;` |
|        - |   229 | `	/* Allocate a new user function */` |
|   401942 |   230 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|   401942 |   231 | `	if( pFunc == 0 ){` |
|      ! 0 |   232 | `		return SXERR_MEM;` |
|        - |   233 | `	}` |
|        - |   234 | `	/* Duplicate function name */` |
|   401942 |   235 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   401942 |   236 | `	if( zDup == 0 ){` |
|      ! 0 |   237 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   238 | `		return SXERR_MEM;` |
|        - |   239 | `	}` |
|        - |   240 | `	/* Zero the structure */` |
|   401942 |   241 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   242 | `	/* Initialize structure fields */` |
|   401942 |   243 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|   401942 |   244 | `	pFunc->pVm   = pVm;` |
|   401942 |   245 | `	pFunc->xFunc = xFunc;` |
|   401942 |   246 | `	pFunc->pUserData = pUserData;` |
|   401942 |   247 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   248 | `	/* Write a pointer to the new function */` |
|   401942 |   249 | `	*ppOut = pFunc;` |
|   401942 |   250 | `	return SXRET_OK;` |
|   200972 |   251 |  |
|        - |   252 | `/*` |
|        - |   253 | ` * Install a foreign function and it's associated callback so that` |
|        - |   254 | ` * it can be invoked from the target PHP code.` |
|        - |   255 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   256 | ` * return value indicates failure.` |
|        - |   257 | ` * Please refer to the official documentation for an introduction to` |
|        - |   258 | ` * the foreign function mechanism.` |
|        - |   259 | ` */` |
|   402864 |   260 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
|        - |   261 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   262 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   263 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   264 | `	void *pUserData           /* Foreign function private data */` |
|        - |   265 | `	)` |
|        2 |   266 |  |
|        - |   267 | `	ph7_user_func *pFunc;` |
|        - |   268 | `	SyHashEntry *pEntry;` |
|        - |   269 | `	sxi32 rc;` |
|        - |   270 | `	/* Overwrite any previously registered function with the same name */` |
|   402866 |   271 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|   402866 |   272 | `	if( pEntry ){` |
|      926 |   273 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|      926 |   274 | `		pFunc->pUserData = pUserData;` |
|      926 |   275 | `		pFunc->xFunc = xFunc;` |
|      926 |   276 | `		SySetReset(&pFunc->aAux);` |
|      926 |   277 | `		return SXRET_OK;` |
|        - |   278 | `	}` |
|        - |   279 | `	/* Create a new user function */` |
|   401942 |   280 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|   401942 |   281 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   282 | `		return rc;` |
|        - |   283 | `	}` |
|        - |   284 | `	/* Install the function in the corresponding hashtable */` |
|   401942 |   285 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|   401942 |   286 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   287 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   288 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   289 | `		return rc;` |
|        - |   290 | `	}` |
|        - |   291 | `	/* User function successfully installed */` |
|   401942 |   292 | `	return SXRET_OK;` |
|   201434 |   293 |  |
|        - |   294 | `/*` |
|        - |   295 | ` * Initialize a VM function.` |
|        - |   296 | ` */` |
|    50326 |   297 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   298 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   299 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   300 | `	const char *zName,  /* Function name */` |
|        - |   301 | `	sxu32 nByte,        /* zName length */` |
|        - |   302 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   303 | `	void *pUserData     /* Function private data */` |
|        - |   304 | `	)` |
|        2 |   305 |  |
|        - |   306 | `	/* Zero the structure */` |
|    50328 |   307 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   308 | `	/* Initialize structure fields */` |
|        - |   309 | `	/* Arguments container */` |
|    50328 |   310 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   311 | `	/* Static variable container */` |
|    50328 |   312 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   313 | `	/* Bytecode container */` |
|    50328 |   314 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   315 | `    /* Preallocate some instruction slots */` |
|    50328 |   316 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   317 | `	/* Closure environment */` |
|    50328 |   318 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|    50328 |   319 | `	pFunc->iFlags = iFlags;` |
|    50328 |   320 | `	pFunc->pUserData = pUserData;` |
|    50328 |   321 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|    50328 |   322 | `	return SXRET_OK;` |
|        2 |   323 |  |
|        - |   324 | `/*` |
|        - |   325 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   326 | ` */` |
|    77144 |   327 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   328 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   329 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   330 | `	SyString *pName     /* Function name */` |
|        - |   331 | `	)` |
|        2 |   332 |  |
|        - |   333 | `	SyHashEntry *pEntry;` |
|        - |   334 | `	sxi32 rc;` |
|    77146 |   335 | `	if( pName == 0 ){` |
|        - |   336 | `		/* Use the built-in name */` |
|    15784 |   337 | `		pName = &pFunc->sName;` |
|     7891 |   338 | `	}` |
|        - |   339 | `	/* Check for duplicates (functions with the same name) first */` |
|    77146 |   340 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|    77146 |   341 | `	if( pEntry ){` |
|    36500 |   342 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|    36500 |   343 | `		if( pLink != pFunc ){` |
|        - |   344 | `			/* Link */` |
|      185 |   345 | `			pFunc->pNextName = pLink;` |
|      185 |   346 | `			pEntry->pUserData = pFunc;` |
|       92 |   347 | `		}` |
|    36500 |   348 | `		return SXRET_OK;` |
|        - |   349 | `	}` |
|        - |   350 | `	/* First time seen */` |
|    40648 |   351 | `	pFunc->pNextName = 0;` |
|    40648 |   352 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|    40648 |   353 | `	return rc;` |
|    38574 |   354 |  |
|        - |   355 | `/*` |
|        - |   356 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   357 | ` */` |
|     8540 |   358 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   359 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   360 | `	ph7_class *pClass /* Target Class */` |
|        - |   361 | `	)` |
|        2 |   362 |  |
|     8542 |   363 | `	SyString *pName = &pClass->sName;` |
|        - |   364 | `	SyHashEntry *pEntry;` |
|        - |   365 | `	sxi32 rc;` |
|        - |   366 | `	/* Check for duplicates */` |
|     8542 |   367 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|     8542 |   368 | `	if( pEntry ){` |
|       63 |   369 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   370 | `		/* Link entry with the same name */` |
|       63 |   371 | `		pClass->pNextName = pLink;` |
|       63 |   372 | `		pEntry->pUserData = pClass;` |
|       63 |   373 | `		return SXRET_OK;` |
|        - |   374 | `	}` |
|     8480 |   375 | `	pClass->pNextName = 0;` |
|        - |   376 | `	/* Perform a simple hashtable insertion */` |
|     8480 |   377 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|     8480 |   378 | `	return rc;` |
|     4272 |   379 |  |
|        - |   380 | `/*` |
|        - |   381 | ` * Instruction builder interface.` |
|        - |   382 | ` */` |
|  1271724 |   383 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
|        - |   384 | `	ph7_vm *pVm,  /* Target VM */` |
|        - |   385 | `	sxi32 iOp,    /* Operation to perform */` |
|        - |   386 | `	sxi32 iP1,    /* First operand */` |
|        - |   387 | `	sxu32 iP2,    /* Second operand */` |
|        - |   388 | `	void *p3,     /* Third operand */` |
|        - |   389 | `	sxu32 *pIndex /* Instruction index. NULL otherwise */` |
|        - |   390 | `	)` |
|        2 |   391 |  |
|        - |   392 | `	VmInstr sInstr;` |
|        - |   393 | `	sxi32 rc;` |
|        - |   394 | `	/* Fill the VM instruction */` |
|  1271726 |   395 | `	sInstr.iOp = (sxu8)iOp;` |
|  1271726 |   396 | `	sInstr.iP1 = iP1;` |
|  1271726 |   397 | `	sInstr.iP2 = iP2;` |
|  1271726 |   398 | `	sInstr.p3  = p3;` |
|  1271726 |   399 | `	if( pIndex ){` |
|        - |   400 | `		/* Instruction index in the bytecode array */` |
|    76402 |   401 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    38200 |   402 | `	}` |
|        - |   403 | `	/* Finally,record the instruction */` |
|  1271726 |   404 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  1271726 |   405 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   406 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   407 | `		/* Fall throw */` |
|      ! 0 |   408 | `	}` |
|  1271726 |   409 | `	return rc;` |
|        2 |   410 |  |
|        - |   411 | `/*` |
|        - |   412 | ` * Swap the current bytecode container with the given one.` |
|        - |   413 | ` */` |
|   122428 |   414 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   415 |  |
|   122430 |   416 | `	if( pContainer == 0 ){` |
|        - |   417 | `		/* Point to the default container */` |
|      ! 0 |   418 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   419 | `	}else{` |
|        - |   420 | `		/* Change container */` |
|   122430 |   421 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   422 | `	}` |
|   122430 |   423 | `	return SXRET_OK;` |
|        2 |   424 |  |
|        - |   425 | `/*` |
|        - |   426 | ` * Return the current bytecode container.` |
|        - |   427 | ` */` |
|    61214 |   428 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   429 |  |
|    61216 |   430 | `	return pVm->pByteContainer;` |
|        2 |   431 |  |
|        - |   432 | `/*` |
|        - |   433 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   434 | ` */` |
|    75146 |   435 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   436 |  |
|        - |   437 | `	VmInstr *pInstr;` |
|    75148 |   438 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|    75148 |   439 | `	return pInstr;` |
|        2 |   440 |  |
|        - |   441 | `/*` |
|        - |   442 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   443 | ` */` |
|   367618 |   444 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   445 |  |
|   367620 |   446 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   447 |  |
|        - |   448 | `/*` |
|        - |   449 | ` * Pop the last VM instruction.` |
|        - |   450 | ` */` |
|    71986 |   451 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   452 |  |
|    71988 |   453 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   454 |  |
|        - |   455 | `/*` |
|        - |   456 | ` * Peek the last VM instruction.` |
|        - |   457 | ` */` |
|   192666 |   458 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   459 |  |
|   192668 |   460 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   461 |  |
|     2588 |   462 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   463 |  |
|        - |   464 | `	VmInstr *aInstr;` |
|        - |   465 | `	sxu32 n;` |
|     2590 |   466 | `	n = SySetUsed(pVm->pByteContainer);` |
|     2590 |   467 | `	if( n < 2 ){` |
|      ! 0 |   468 | `		return 0;` |
|        - |   469 | `	}` |
|     2590 |   470 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|     2590 |   471 | `	return &aInstr[n - 2];` |
|     1296 |   472 |  |
|        - |   473 | `/*` |
|        - |   474 | ` * Allocate a new virtual machine frame.` |
|        - |   475 | ` */` |
|     6752 |   476 | `static VmFrame * VmNewFrame(` |
|        - |   477 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   478 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   479 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   480 | `	)` |
|        2 |   481 |  |
|        - |   482 | `	VmFrame *pFrame;` |
|        - |   483 | `	/* Allocate a new vm frame */` |
|     6754 |   484 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|     6754 |   485 | `	if( pFrame == 0 ){` |
|      ! 0 |   486 | `		return 0;` |
|        - |   487 | `	}` |
|        - |   488 | `	/* Zero the structure */` |
|     6754 |   489 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   490 | `	/* Initialize frame fields */` |
|     6754 |   491 | `	pFrame->pUserData = pUserData;` |
|     6754 |   492 | `	pFrame->pThis = pThis;` |
|     6754 |   493 | `	pFrame->pVm = pVm;` |
|     6754 |   494 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|     6754 |   495 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|     6754 |   496 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|     6754 |   497 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|     6754 |   498 | `	return pFrame;` |
|     3378 |   499 |  |
|        - |   500 | `/*` |
|        - |   501 | ` * Enter a VM frame.` |
|        - |   502 | ` */` |
|     6752 |   503 | `static sxi32 VmEnterFrame(` |
|        - |   504 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   505 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   506 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   507 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   508 | `	)` |
|        2 |   509 |  |
|        - |   510 | `	VmFrame *pFrame;` |
|        - |   511 | `	/* Allocate a new frame */` |
|     6754 |   512 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|     6754 |   513 | `	if( pFrame == 0 ){` |
|      ! 0 |   514 | `		return SXERR_MEM;` |
|        - |   515 | `	}` |
|        - |   516 | `	/* Link to the list of active VM frame */` |
|     6754 |   517 | `	pFrame->pParent = pVm->pFrame;` |
|     6754 |   518 | `	pVm->pFrame = pFrame;` |
|     6754 |   519 | `	if( ppFrame ){` |
|        - |   520 | `		/* Write a pointer to the new VM frame */` |
|     5570 |   521 | `		*ppFrame = pFrame;` |
|     2784 |   522 | `	}` |
|     6754 |   523 | `	return SXRET_OK;` |
|     3378 |   524 |  |
|        - |   525 | `/*` |
|        - |   526 | ` * Link a foreign variable with the TOP most active frame.` |
|        - |   527 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|        - |   528 | ` * information.` |
|        - |   529 | ` */` |
|       30 |   530 | `static sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|        1 |   531 |  |
|        - |   532 | `	VmFrame *pTarget,*pFrame;` |
|       31 |   533 | `	SyHashEntry *pEntry = 0;` |
|        - |   534 | `	sxi32 rc;` |
|        - |   535 | `	/* Point to the upper frame */` |
|       31 |   536 | `	pFrame = pVm->pFrame;` |
|       31 |   537 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |   538 | `		/* Safely ignore the exception frame */` |
|      ! 0 |   539 | `		pFrame = pFrame->pParent;` |
|      ! 0 |   540 | `	}` |
|       31 |   541 | `	pTarget = pFrame;` |
|       31 |   542 | `	pFrame = pTarget->pParent;` |
|       31 |   543 | `	while( pFrame ){` |
|       31 |   544 | `		if( (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   545 | `			/* Query the current frame */` |
|       31 |   546 | `			pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|       31 |   547 | `			if( pEntry ){` |
|        - |   548 | `				/* Variable found */` |
|       31 |   549 | `				break;` |
|        - |   550 | `			}` |
|      ! 0 |   551 | `		}` |
|        - |   552 | `		/* Point to the upper frame */` |
|      ! 0 |   553 | `		pFrame = pFrame->pParent;` |
|      ! 0 |   554 | `	}` |
|       31 |   555 | `	if( pEntry == 0 ){` |
|        - |   556 | `		/* Inexistant variable */` |
|      ! 0 |   557 | `		return SXERR_NOTFOUND;` |
|        - |   558 | `	}` |
|        - |   559 | `	/* Link to the current frame */` |
|       31 |   560 | `	rc = SyHashInsert(&pTarget->hVar,pEntry->pKey,pEntry->nKeyLen,pEntry->pUserData);` |
|       31 |   561 | `	if( rc == SXRET_OK ){` |
|        - |   562 | `		sxu32 nIdx;` |
|       31 |   563 | `		nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|       31 |   564 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pTarget->hVar),0,0);` |
|       15 |   565 | `	}` |
|       31 |   566 | `	return rc;` |
|       16 |   567 |  |
|        - |   568 | `/*` |
|        - |   569 | ` * Leave the top-most active frame.` |
|        - |   570 | ` */` |
|     5568 |   571 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   572 |  |
|     5570 |   573 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|     5570 |   574 | `	if( pCurFrame ){` |
|        - |   575 | `		/* Unlink from the list of active VM frame */` |
|     5570 |   576 | `		pVm->pFrame = pCurFrame->pParent;` |
|     5570 |   577 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   578 | `			VmSlot  *aSlot;` |
|        - |   579 | `			sxu32 n;` |
|        - |   580 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|     5552 |   581 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    41802 |   582 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   583 | `				/* Unset the local variable */` |
|    36252 |   584 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    18127 |   585 | `			}` |
|        - |   586 | `			/* Remove local reference */` |
|     5552 |   587 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    41836 |   588 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    36286 |   589 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    18144 |   590 | `			}` |
|     2775 |   591 | `		}` |
|        - |   592 | `		/* Release internal containers */` |
|     5570 |   593 | `		SyHashRelease(&pCurFrame->hVar);` |
|     5570 |   594 | `		SySetRelease(&pCurFrame->sArg);` |
|     5570 |   595 | `		SySetRelease(&pCurFrame->sLocal);` |
|     5570 |   596 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   597 | `		/* Release the whole structure */` |
|     5570 |   598 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     2784 |   599 | `	}` |
|     5570 |   600 |  |
|        - |   601 | `/*` |
|        - |   602 | ` * Compare two functions signature and return the comparison result.` |
|        - |   603 | ` */` |
|      818 |   604 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|        1 |   605 |  |
|      819 |   606 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|      819 |   607 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|      819 |   608 | `	const char *zSin = pSecond->zString;` |
|      819 |   609 | `	const char *zFin = pFirst->zString;` |
|      819 |   610 | `	const char *zPtr = zFin;` |
|      409 |   611 | `	for(;;){` |
|      819 |   612 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|      410 |   613 | `			break;` |
|        - |   614 | `		}` |
|      ! 0 |   615 | `		if( zFin[0] != zSin[0] ){` |
|        - |   616 | `			/* mismatch */` |
|      ! 0 |   617 | `			break;` |
|        - |   618 | `		}` |
|      ! 0 |   619 | `		zFin++;` |
|      ! 0 |   620 | `		zSin++;` |
|      ! 0 |   621 | `	}` |
|      819 |   622 | `	return (int)(zFin-zPtr);` |
|        1 |   623 |  |
|        - |   624 | `/*` |
|        - |   625 | ` * Select the appropriate VM function for the current call context.` |
|        - |   626 | ` * This is the implementation of the powerful 'function overloading' feature` |
|        - |   627 | ` * introduced by the version 2 of the PH7 engine.` |
|        - |   628 | ` * Refer to the official documentation for more information.` |
|        - |   629 | ` */` |
|      128 |   630 | `static ph7_vm_func * VmOverload(` |
|        - |   631 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |   632 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|        - |   633 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|        - |   634 | `	int nArg             /* Total number of passed arguments  */` |
|        - |   635 | `	)` |
|        1 |   636 |  |
|        - |   637 | `	int iTarget,i,j,iCur,iMax;` |
|        - |   638 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|        - |   639 | `	ph7_vm_func *pLink;` |
|        - |   640 | `	SyString sArgSig;` |
|        - |   641 | `	SyBlob sSig;` |
|        - |   642 |  |
|      129 |   643 | `	pLink = pList;` |
|      129 |   644 | `	i = 0;` |
|        - |   645 | `	/* Put functions expecting the same number of passed arguments */` |
|     1073 |   646 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|     1011 |   647 | `		if( pLink == 0 ){` |
|       67 |   648 | `			break;` |
|        - |   649 | `		}` |
|      945 |   650 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|        - |   651 | `			/* Candidate for overloading */` |
|      883 |   652 | `			apSet[i++] = pLink;` |
|      441 |   653 | `		}` |
|        - |   654 | `		/* Point to the next entry */` |
|      945 |   655 | `		pLink = pLink->pNextName;` |
|        1 |   656 | `	}` |
|      129 |   657 | `	if( i < 1 ){` |
|        - |   658 | `		/* No candidates,return the head of the list */` |
|      ! 0 |   659 | `		return pList;` |
|        - |   660 | `	}` |
|      129 |   661 | `	if( nArg < 1 \|\| i < 2 ){` |
|        - |   662 | `		/* Return the only candidate */` |
|       27 |   663 | `		return apSet[0];` |
|        - |   664 | `	}` |
|        - |   665 | `	/* Calculate function signature */` |
|      103 |   666 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|      355 |   667 | `	for( j = 0 ; j < nArg ; j++ ){` |
|      253 |   668 | `		int c = 'n'; /* null */` |
|      253 |   669 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|        - |   670 | `			/* Hashmap */` |
|       45 |   671 | `			c = 'h';` |
|      231 |   672 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|        - |   673 | `			/* bool */` |
|      ! 0 |   674 | `			c = 'b';` |
|      209 |   675 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|        - |   676 | `			/* int */` |
|        5 |   677 | `			c = 'i';` |
|      207 |   678 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|        - |   679 | `			/* String */` |
|      105 |   680 | `			c = 's';` |
|      153 |   681 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|        - |   682 | `			/* Float */` |
|      ! 0 |   683 | `			c = 'f';` |
|      101 |   684 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|        - |   685 | `			/* Class instance */` |
|      ! 0 |   686 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|      ! 0 |   687 | `			SyString *pName = &pClass->sName;` |
|      ! 0 |   688 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|      ! 0 |   689 | `			c = -1;` |
|      ! 0 |   690 | `		}` |
|      253 |   691 | `		if( c > 0 ){` |
|      253 |   692 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|      126 |   693 | `		}` |
|      127 |   694 | `	}` |
|      103 |   695 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|      103 |   696 | `	iTarget = 0;` |
|      103 |   697 | `	iMax = -1;` |
|        - |   698 | `	/* Select the appropriate function */` |
|      921 |   699 | `	for( j = 0 ; j < i ; j++ ){` |
|        - |   700 | `		/* Compare the two signatures */` |
|      819 |   701 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|      819 |   702 | `		if( iCur > iMax ){` |
|      103 |   703 | `			iMax = iCur;` |
|      103 |   704 | `			iTarget = j;` |
|       51 |   705 | `		}` |
|      410 |   706 | `	}` |
|      103 |   707 | `	SyBlobRelease(&sSig);` |
|        - |   708 | `	/* Appropriate function for the current call context */` |
|      103 |   709 | `	return apSet[iTarget];` |
|       65 |   710 |  |
|        - |   711 | `/* Forward declaration */` |
|        - |   712 | `static sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult);` |
|        - |   713 | `static sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...);` |
|        - |   714 | `/*` |
|        - |   715 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|        - |   716 | ` * it can be instanciated from the executed PHP script.` |
|        - |   717 | ` */` |
|    43562 |   718 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   719 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   720 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   721 | `	)` |
|        2 |   722 |  |
|        - |   723 | `	ph7_class_method *pMeth;` |
|        - |   724 | `	ph7_class_attr *pAttr;` |
|        - |   725 | `	SyHashEntry *pEntry;` |
|        - |   726 | `	sxi32 rc;` |
|        - |   727 | `	/* Reset the loop cursor */` |
|    43564 |   728 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   729 | `	/* Process only static and constant attribute */` |
|   100551 |   730 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   731 | `		/* Extract the current attribute */` |
|    35208 |   732 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|    35208 |   733 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   734 | `			ph7_value *pMemObj;` |
|        - |   735 | `			/* Reserve a memory object for this constant/static attribute */` |
|     1290 |   736 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1290 |   737 | `			if( pMemObj == 0 ){` |
|      ! 0 |   738 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   739 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   740 | `					&pClass->sName,&pAttr->sName` |
|        - |   741 | `					);` |
|      ! 0 |   742 | `				return SXERR_MEM;` |
|        - |   743 | `			}` |
|     1290 |   744 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   745 | `				/* Initialize attribute default value (any complex expression) */` |
|     1290 |   746 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      644 |   747 | `			}` |
|        - |   748 | `			/* Record attribute index */` |
|     1290 |   749 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   750 | `			/* Install static attribute in the reference table */` |
|     1290 |   751 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      644 |   752 | `		}` |
|        2 |   753 | `	}` |
|        - |   754 | `	/* Install class methods */` |
|    43564 |   755 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|        - |   756 | `		/* Do not mount interface methods since they are signatures only.` |
|        - |   757 | `		 */` |
|    34904 |   758 | `		return SXRET_OK;` |
|        - |   759 | `	}` |
|        - |   760 | `	/* Create constructor alias if not yet done */` |
|     8662 |   761 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   762 | `		/* User constructor with the same base class name */` |
|      200 |   763 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|      200 |   764 | `		if( pEntry ){` |
|      ! 0 |   765 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   766 | `			/* Create the alias */` |
|      ! 0 |   767 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   768 | `		}` |
|       99 |   769 | `	}` |
|        - |   770 | `	/* Install the methods now */` |
|     8662 |   771 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|    74360 |   772 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|    61370 |   773 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|    61370 |   774 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|    61364 |   775 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|    61364 |   776 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   777 | `				return rc;` |
|        - |   778 | `			}` |
|    30681 |   779 | `		}` |
|        2 |   780 | `	}` |
|        - |   781 | `	/* Mark class as mounted to avoid redundant mounting */` |
|     8662 |   782 | `	pClass->bMounted = TRUE;` |
|     8662 |   783 | `	return SXRET_OK;` |
|    21783 |   784 |  |
|        - |   785 | `/*` |
|        - |   786 | ` * Allocate a private frame for attributes of the given` |
|        - |   787 | ` * class instance (Object in the PHP jargon).` |
|        - |   788 | ` */` |
|      552 |   789 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   790 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   791 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   792 | `	)` |
|        2 |   793 |  |
|      554 |   794 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   795 | `	ph7_class_attr *pAttr;` |
|        - |   796 | `	SyHashEntry *pEntry;` |
|        - |   797 | `	sxi32 rc;` |
|        - |   798 | `	/* Install class attribute in the private frame associated with this instance */` |
|      554 |   799 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     1194 |   800 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   801 | `		VmClassAttr *pVmAttr;` |
|        - |   802 | `		/* Extract the current attribute */` |
|      642 |   803 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      642 |   804 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|      642 |   805 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   806 | `			return SXERR_MEM;` |
|        - |   807 | `		}` |
|      642 |   808 | `		pVmAttr->pAttr = pAttr;` |
|      642 |   809 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   810 | `			ph7_value *pMemObj;` |
|        - |   811 | `			/* Reserve a memory object for this attribute */` |
|      636 |   812 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|      636 |   813 | `			if( pMemObj == 0 ){` |
|      ! 0 |   814 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   815 | `				return SXERR_MEM;` |
|        - |   816 | `			}` |
|      636 |   817 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|      636 |   818 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   819 | `				/* Initialize attribute default value (any complex expression) */` |
|      190 |   820 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|       94 |   821 | `			}` |
|      636 |   822 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|      636 |   823 | `			if( rc != SXRET_OK ){` |
|        - |   824 | `				VmSlot sSlot;` |
|        - |   825 | `				/* Restore memory object */` |
|      ! 0 |   826 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   827 | `				sSlot.pUserData = 0;` |
|      ! 0 |   828 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   829 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   830 | `				return SXERR_MEM;` |
|        - |   831 | `			}` |
|        - |   832 | `			/* Install attribute in the reference table */` |
|      636 |   833 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      319 |   834 | `		}else{` |
|        - |   835 | `			/* Install static/constant attribute */` |
|        8 |   836 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|        8 |   837 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|        8 |   838 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   839 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   840 | `				return SXERR_MEM;` |
|        - |   841 | `			}` |
|        - |   842 | `		}` |
|        2 |   843 | `	}` |
|      554 |   844 | `	return SXRET_OK;` |
|      278 |   845 |  |
|        - |   846 | `/* Forward declaration */` |
|        - |   847 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx);` |
|        - |   848 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef);` |
|        - |   849 | `/*` |
|        - |   850 | ` * Dummy read-only buffer used for slot reservation.` |
|        - |   851 | ` */` |
|        - |   852 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|        - |   853 | `/*` |
|        - |   854 | ` * Reserve a constant memory object.` |
|        - |   855 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   856 | ` */` |
|   145616 |   857 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   858 |  |
|        - |   859 | `	ph7_value *pObj;` |
|        - |   860 | `	sxi32 rc;` |
|   145618 |   861 | `	if( pIndex ){` |
|        - |   862 | `		/* Object index in the object table */` |
|   142066 |   863 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|    71032 |   864 | `	}` |
|        - |   865 | `	/* Reserve a slot for the new object */` |
|   145618 |   866 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   145618 |   867 | `	if( rc != SXRET_OK ){` |
|        - |   868 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   869 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   870 | `		 */` |
|      ! 0 |   871 | `		return 0;` |
|        - |   872 | `	}` |
|   145618 |   873 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   145618 |   874 | `	return pObj;` |
|    72810 |   875 |  |
|        - |   876 | `/*` |
|        - |   877 | ` * Reserve a memory object.` |
|        - |   878 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   879 | ` */` |
|    74552 |   880 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   881 |  |
|        - |   882 | `	ph7_value *pObj;` |
|        - |   883 | `	sxi32 rc;` |
|    74554 |   884 | `	if( pIndex ){` |
|        - |   885 | `		/* Object index in the object table */` |
|    74554 |   886 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|    37276 |   887 | `	}` |
|        - |   888 | `	/* Reserve a slot for the new object */` |
|    74554 |   889 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|    74554 |   890 | `	if( rc != SXRET_OK ){` |
|        - |   891 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   892 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   893 | `		 */` |
|      ! 0 |   894 | `		return 0;` |
|        - |   895 | `	}` |
|    74554 |   896 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|    74554 |   897 | `	return pObj;` |
|    37278 |   898 |  |
|        - |   899 | `/* Forward declaration */` |
|        - |   900 | `static sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);` |
|        - |   901 | `/*` |
|        - |   902 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |   903 | ` * directly as foreign functions.` |
|        - |   904 | ` */` |
|        - |   905 | `#define PH7_BUILTIN_LIB \` |
|        - |   906 | `	"class Exception { "\` |
|        - |   907 | `    "protected $message = 'Unknown exception';"\` |
|        - |   908 | `    "protected $code = 0;"\` |
|        - |   909 | `    "protected $file;"\` |
|        - |   910 | `    "protected $line;"\` |
|        - |   911 | `    "protected $trace;"\` |
|        - |   912 | `    "protected $previous;"\` |
|        - |   913 | `	"public function __construct($message = null, $code = 0, Exception $previous = null){"\` |
|        - |   914 | `	"   if( isset($message) ){"\` |
|        - |   915 | `	"	  $this->message = $message;"\` |
|        - |   916 | `	"   }"\` |
|        - |   917 | `	"   $this->code = $code;"\` |
|        - |   918 | `	"   $this->file = __FILE__;"\` |
|        - |   919 | `	"   $this->line = __LINE__;"\` |
|        - |   920 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   921 | `	"   if( isset($previous) ){"\` |
|        - |   922 | `	"     $this->previous = $previous;"\` |
|        - |   923 | `	"   }"\` |
|        - |   924 | `	"}"\` |
|        - |   925 | `	"public function getMessage(){"\` |
|        - |   926 | `	"   return $this->message;"\` |
|        - |   927 | `	"}"\` |
|        - |   928 | `	" public function getCode(){"\` |
|        - |   929 | `	"  return $this->code;"\` |
|        - |   930 | `	"}"\` |
|        - |   931 | `	"public function getFile(){"\` |
|        - |   932 | `	"  return $this->file;"\` |
|        - |   933 | `	"}"\` |
|        - |   934 | `	"public function getLine(){"\` |
|        - |   935 | `	"  return $this->line;"\` |
|        - |   936 | `	"}"\` |
|        - |   937 | `	"public function getTrace(){"\` |
|        - |   938 | `	"   return $this->trace;"\` |
|        - |   939 | `	"}"\` |
|        - |   940 | `	"public function getTraceAsString(){"\` |
|        - |   941 | `	"  return debug_string_backtrace();"\` |
|        - |   942 | `	"}"\` |
|        - |   943 | `	"public function getPrevious(){"\` |
|        - |   944 | `	"    return $this->previous;"\` |
|        - |   945 | `	"}"\` |
|        - |   946 | `	"public function __toString(){"\` |
|        - |   947 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |   948 | `    "}"\` |
|        - |   949 | `	"}"\` |
|        - |   950 | `	"class ErrorException extends Exception { "\` |
|        - |   951 | `	"protected $severity;"\` |
|        - |   952 | `	"public function __construct(string $message = null,"\` |
|        - |   953 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Exception $previous = null){"\` |
|        - |   954 | `	"   if( isset($message) ){"\` |
|        - |   955 | `	"	  $this->message = $message;"\` |
|        - |   956 | `	"   }"\` |
|        - |   957 | `	"   $this->severity = $severity;"\` |
|        - |   958 | `	"   $this->code = $code;"\` |
|        - |   959 | `	"   $this->file = $filename;"\` |
|        - |   960 | `	"   $this->line = $lineno;"\` |
|        - |   961 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   962 | `	"   if( isset($previous) ){"\` |
|        - |   963 | `	"     $this->previous = $previous;"\` |
|        - |   964 | `	"   }"\` |
|        - |   965 | `	"}"\` |
|        - |   966 | `	"public function getSeverity(){"\` |
|        - |   967 | `	"   return $this->severity;"\` |
|        - |   968 | `    "}"\` |
|        - |   969 | `	"}"\` |
|        - |   970 | `	"interface Iterator {"\` |
|        - |   971 | `	"public function current();"\` |
|        - |   972 | `	"public function key();"\` |
|        - |   973 | `	"public function next();"\` |
|        - |   974 | `	"public function rewind();"\` |
|        - |   975 | `	"public function valid();"\` |
|        - |   976 | `	"}"\` |
|        - |   977 | `	"interface IteratorAggregate {"\` |
|        - |   978 | `	"public function getIterator();"\` |
|        - |   979 | `	"}"\` |
|        - |   980 | `	"interface Serializable {"\` |
|        - |   981 | `	"public function serialize();"\` |
|        - |   982 | `	"public function unserialize(string $serialized);"\` |
|        - |   983 | `	"}"\` |
|        - |   984 | `	"/* Directory releated IO */"\` |
|        - |   985 | `	"class Directory {"\` |
|        - |   986 | `	"public $handle = null;"\` |
|        - |   987 | `	"public $path  = null;"\` |
|        - |   988 | `	"public function __construct(string $path)"\` |
|        - |   989 | `	"{"\` |
|        - |   990 | `	"   $this->handle = opendir($path);"\` |
|        - |   991 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |   992 | `	"      $this->path = $path;"\` |
|        - |   993 | `	"   }"\` |
|        - |   994 | `	"}"\` |
|        - |   995 | `	"public function __destruct()"\` |
|        - |   996 | `	"{"\` |
|        - |   997 | `	"  if( $this->handle != null ){"\` |
|        - |   998 | `	"       closedir($this->handle);"\` |
|        - |   999 | `	"  }"\` |
|        - |  1000 | `	"}"\` |
|        - |  1001 | `	"public function read()"\` |
|        - |  1002 | `	"{"\` |
|        - |  1003 | `	"    return readdir($this->handle);"\` |
|        - |  1004 | `	"}"\` |
|        - |  1005 | `	"public function rewind()"\` |
|        - |  1006 | `	"{"\` |
|        - |  1007 | `	"    rewinddir($this->handle);"\` |
|        - |  1008 | `	"}"\` |
|        - |  1009 | `	"public function close()"\` |
|        - |  1010 | `	"{"\` |
|        - |  1011 | `	"    closedir($this->handle);"\` |
|        - |  1012 | `	"    $this->handle = null;"\` |
|        - |  1013 | `	"}"\` |
|        - |  1014 | `	"}"\` |
|        - |  1015 | `	"class stdClass{"\` |
|        - |  1016 | `	"  public $value;"\` |
|        - |  1017 | `	" /* Magic methods */"\` |
|        - |  1018 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1019 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1020 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1021 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1022 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1023 | `	"}"\` |
|        - |  1024 | `	"function dir(string $path){"\` |
|        - |  1025 | `	"   return new Directory($path);"\` |
|        - |  1026 | `	"}"\` |
|        - |  1027 | `	"function Dir(string $path){"\` |
|        - |  1028 | `	"   return new Directory($path);"\` |
|        - |  1029 | `	"}"\` |
|        - |  1030 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1031 | `    "{"\` |
|        - |  1032 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1033 | `	"  $aDir = array();"\` |
|        - |  1034 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1035 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1036 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1037 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1038 | `	"   }"\` |
|        - |  1039 | `	"  closedir($pHandle);"\` |
|        - |  1040 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1041 | `	"      rsort($aDir);"\` |
|        - |  1042 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1043 | `	"      sort($aDir);"\` |
|        - |  1044 | `	"  }"\` |
|        - |  1045 | `	"  return $aDir;"\` |
|        - |  1046 | `	"}"\` |
|        - |  1047 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1048 | `	"/* Open the target directory */"\` |
|        - |  1049 | `	"$zDir = dirname($pattern);"\` |
|        - |  1050 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1051 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1052 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1053 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1054 | `	"	return FALSE;"\` |
|        - |  1055 | `	"}"\` |
|        - |  1056 | `	"$pattern = basename($pattern);"\` |
|        - |  1057 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1058 | `	"/* Loop throw available entries */"\` |
|        - |  1059 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1060 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1061 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1062 | `	"	if( $rc ){"\` |
|        - |  1063 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1064 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1065 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1066 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1067 | `	"		  }"\` |
|        - |  1068 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1069 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1070 | `	"		 continue;"\` |
|        - |  1071 | `	"	   }"\` |
|        - |  1072 | `	"	   /* Add the entry */"\` |
|        - |  1073 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1074 | `	"	}"\` |
|        - |  1075 | `	" }"\` |
|        - |  1076 | `	"/* Close the handle */"\` |
|        - |  1077 | `	"closedir($pHandle);"\` |
|        - |  1078 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1079 | `	"  /* Sort the array */"\` |
|        - |  1080 | `	"  sort($pArray);"\` |
|        - |  1081 | `	"}"\` |
|        - |  1082 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1083 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1084 | `	"  $pArray[] = $pattern;"\` |
|        - |  1085 | `	"}"\` |
|        - |  1086 | `	"/* Return the created array */"\` |
|        - |  1087 | `	"return $pArray;"\` |
|        - |  1088 | `   "}"\` |
|        - |  1089 | `   "/* Creates a temporary file */"\` |
|        - |  1090 | `   "function tmpfile(){"\` |
|        - |  1091 | `   "  /* Extract the temp directory */"\` |
|        - |  1092 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1093 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1094 | `   "    /* Use the current dir */"\` |
|        - |  1095 | `   "    $zTempDir = '.';"\` |
|        - |  1096 | `   "  }"\` |
|        - |  1097 | `   "  /* Create the file */"\` |
|        - |  1098 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1099 | `   "  return $pHandle;"\` |
|        - |  1100 | `   "}"\` |
|        - |  1101 | `   "/* Creates a temporary filename */"\` |
|        - |  1102 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1103 | `   "{"\` |
|        - |  1104 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1105 | `   "}"\` |
|        - |  1106 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1107 | `   " if( func_num_args() < 1 \|\| !is_array($pArray) ){  return 0; }"\` |
|        - |  1108 | `   "/* Copy arguments */"\` |
|        - |  1109 | `   "$nArgs = func_num_args();"\` |
|        - |  1110 | `   "$pNew = array();"\` |
|        - |  1111 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1112 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1113 | `    "}"\` |
|        - |  1114 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1115 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1116 | `	"/* Erase */"\` |
|        - |  1117 | `	"array_erase($pArray);"\` |
|        - |  1118 | `	"/* Unshift */"\` |
|        - |  1119 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1120 | `	"return sizeof($pArray);"\` |
|        - |  1121 | `    "}"\` |
|        - |  1122 | `	"function array_merge_recursive($array1, $array2){"\` |
|        - |  1123 | `	"if( func_num_args() < 1 ){ return NULL; }"\` |
|        - |  1124 | `    "$arrays = func_get_args();"\` |
|        - |  1125 | `    "$narrays = count($arrays);"\` |
|        - |  1126 | `    "$ret = $arrays[0];"\` |
|        - |  1127 | `    "for ($i = 1; $i < $narrays; $i++) {"\` |
|        - |  1128 | `	 " if( array_same($ret,$arrays[$i]) ){ /* Same instance */continue;}"\` |
|        - |  1129 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1130 | `     "  if (((string) $key) === ((string) intval($key))) {"\` |
|        - |  1131 | `     "   $ret[] = $value;"\` |
|        - |  1132 | `     "  }else{"\` |
|        - |  1133 | `     "  if (is_array($value) && isset($ret[$key]) ) {"\` |
|        - |  1134 | `     "   $ret[$key] = array_merge_recursive($ret[$key], $value);"\` |
|        - |  1135 | `     " }else {"\` |
|        - |  1136 | `     "   $ret[$key] = $value;"\` |
|        - |  1137 | `     "  }"\` |
|        - |  1138 | `     " }"\` |
|        - |  1139 | `     " }"\` |
|        - |  1140 | `	 "}"\` |
|        - |  1141 | `	 " return $ret;"\` |
|        - |  1142 | `    "}"\` |
|        - |  1143 | `	"function max(){"\` |
|        - |  1144 | `    "  $pArgs = func_get_args();"\` |
|        - |  1145 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1146 | `	"  return null;"\` |
|        - |  1147 | `    " }"\` |
|        - |  1148 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1149 | `    " $pArg = $pArgs[0];"\` |
|        - |  1150 | `	" if( !is_array($pArg) ){"\` |
|        - |  1151 | `	"   return $pArg; "\` |
|        - |  1152 | `	" }"\` |
|        - |  1153 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1154 | `	"   return null;"\` |
|        - |  1155 | `	" }"\` |
|        - |  1156 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1157 | `	" reset($pArg);"\` |
|        - |  1158 | `	" $max = current($pArg);"\` |
|        - |  1159 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1160 | `	"   if( $val > $max ){"\` |
|        - |  1161 | `	"     $max = $val;"\` |
|        - |  1162 | `    " }"\` |
|        - |  1163 | `	" }"\` |
|        - |  1164 | `	" return $max;"\` |
|        - |  1165 | `    " }"\` |
|        - |  1166 | `    " $max = $pArgs[0];"\` |
|        - |  1167 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1168 | `    " $val = $pArgs[$i];"\` |
|        - |  1169 | `	"if( $val > $max ){"\` |
|        - |  1170 | `	" $max = $val;"\` |
|        - |  1171 | `	"}"\` |
|        - |  1172 | `    " }"\` |
|        - |  1173 | `	" return $max;"\` |
|        - |  1174 | `    "}"\` |
|        - |  1175 | `	"function min(){"\` |
|        - |  1176 | `    "  $pArgs = func_get_args();"\` |
|        - |  1177 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1178 | `	"  return null;"\` |
|        - |  1179 | `    " }"\` |
|        - |  1180 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1181 | `    " $pArg = $pArgs[0];"\` |
|        - |  1182 | `	" if( !is_array($pArg) ){"\` |
|        - |  1183 | `	"   return $pArg; "\` |
|        - |  1184 | `	" }"\` |
|        - |  1185 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1186 | `	"   return null;"\` |
|        - |  1187 | `	" }"\` |
|        - |  1188 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1189 | `	" reset($pArg);"\` |
|        - |  1190 | `	" $min = current($pArg);"\` |
|        - |  1191 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1192 | `	"   if( $val < $min ){"\` |
|        - |  1193 | `	"     $min = $val;"\` |
|        - |  1194 | `    " }"\` |
|        - |  1195 | `	" }"\` |
|        - |  1196 | `	" return $min;"\` |
|        - |  1197 | `    " }"\` |
|        - |  1198 | `    " $min = $pArgs[0];"\` |
|        - |  1199 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1200 | `    " $val = $pArgs[$i];"\` |
|        - |  1201 | `	"if( $val < $min ){"\` |
|        - |  1202 | `	" $min = $val;"\` |
|        - |  1203 | `	" }"\` |
|        - |  1204 | `    " }"\` |
|        - |  1205 | `	" return $min;"\` |
|        - |  1206 | `	"}"\` |
|        - |  1207 | `	"function fileowner(string $file){"\` |
|        - |  1208 | `    " $a = stat($file);"\` |
|        - |  1209 | `	" if( !is_array($a) ){"\` |
|        - |  1210 | `	"	return false;"\` |
|        - |  1211 | `	" }"\` |
|        - |  1212 | `	" return $a['uid'];"\` |
|        - |  1213 | `    "}"\` |
|        - |  1214 | `    "function filegroup(string $file){"\` |
|        - |  1215 | `	" $a = stat($file);"\` |
|        - |  1216 | `	" if( !is_array($a) ){"\` |
|        - |  1217 | `	"	return false;"\` |
|        - |  1218 | `	" }"\` |
|        - |  1219 | `	" return $a['gid'];"\` |
|        - |  1220 | `    "}"\` |
|        - |  1221 | `	 "function fileinode(string $file){"\` |
|        - |  1222 | `	" $a = stat($file);"\` |
|        - |  1223 | `	" if( !is_array($a) ){"\` |
|        - |  1224 | `	"	return false;"\` |
|        - |  1225 | `	" }"\` |
|        - |  1226 | `	" return $a['ino'];"\` |
|        - |  1227 | `    "}"` |
|        - |  1228 |  |
|        - |  1229 | `/*` |
|        - |  1230 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1231 | ` * start compiling the target PHP program.` |
|        - |  1232 | ` */` |
|     1184 |  1233 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1234 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1235 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1236 | `	 )` |
|        2 |  1237 |  |
|        - |  1238 | `	SyString sBuiltin;` |
|        - |  1239 | `	ph7_value *pObj;` |
|        - |  1240 | `	sxi32 rc;` |
|        - |  1241 | `	/* Zero the structure */` |
|     1186 |  1242 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1243 | `	/* Initialize VM fields */` |
|     1186 |  1244 | `	pVm->pEngine = &(*pEngine);` |
|     1186 |  1245 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1246 | `	/* Instructions containers */` |
|     1186 |  1247 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     1186 |  1248 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     1186 |  1249 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1250 | `	/* Object containers */` |
|     1186 |  1251 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     1186 |  1252 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1253 | `	/* Virtual machine internal containers */` |
|     1186 |  1254 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     1186 |  1255 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     1186 |  1256 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     1186 |  1257 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     1186 |  1258 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     1186 |  1259 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     1186 |  1260 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     1186 |  1261 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     1186 |  1262 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     1186 |  1263 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     1186 |  1264 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     1186 |  1265 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     1186 |  1266 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     1186 |  1267 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     1186 |  1268 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|        - |  1269 | `	/* Configuration containers */` |
|     1186 |  1270 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     1186 |  1271 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     1186 |  1272 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     1186 |  1273 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     1186 |  1274 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1275 | `	/* Error callbacks containers */` |
|     1186 |  1276 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     1186 |  1277 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     1186 |  1278 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     1186 |  1279 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     1186 |  1280 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1281 | `	/* Set a default recursion limit */` |
|        - |  1282 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     1186 |  1283 | `	pVm->nMaxDepth = 32;` |
|        - |  1284 | `#else` |
|        - |  1285 | `	pVm->nMaxDepth = 16;` |
|        - |  1286 | `#endif` |
|        - |  1287 | `	/* Default assertion flags */` |
|     1186 |  1288 | `	pVm->iAssertFlags = PH7_ASSERT_WARNING; /* Issue a warning for each failed assertion */` |
|        - |  1289 | `	/* JSON return status */` |
|     1186 |  1290 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1291 | `	/* PRNG context */` |
|     1186 |  1292 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1293 | `	/* Install the null constant */` |
|     1186 |  1294 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     1186 |  1295 | `	if( pObj == 0 ){` |
|      ! 0 |  1296 | `		rc = SXERR_MEM;` |
|      ! 0 |  1297 | `		goto Err;` |
|        - |  1298 | `	}` |
|     1186 |  1299 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1300 | `	/* Install the boolean TRUE constant */` |
|     1186 |  1301 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     1186 |  1302 | `	if( pObj == 0 ){` |
|      ! 0 |  1303 | `		rc = SXERR_MEM;` |
|      ! 0 |  1304 | `		goto Err;` |
|        - |  1305 | `	}` |
|     1186 |  1306 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1307 | `	/* Install the boolean FALSE constant */` |
|     1186 |  1308 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     1186 |  1309 | `	if( pObj == 0 ){` |
|      ! 0 |  1310 | `		rc = SXERR_MEM;` |
|      ! 0 |  1311 | `		goto Err;` |
|        - |  1312 | `	}` |
|     1186 |  1313 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1314 | `	/* Create the global frame */` |
|     1186 |  1315 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     1186 |  1316 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1317 | `		goto Err;` |
|        - |  1318 | `	}` |
|        - |  1319 | `	/* Initialize the code generator */` |
|     1186 |  1320 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     1186 |  1321 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1322 | `		goto Err;` |
|        - |  1323 | `	}` |
|        - |  1324 | `	/* VM correctly initialized,set the magic number */` |
|     1186 |  1325 | `	pVm->nMagic = PH7_VM_INIT;` |
|     1186 |  1326 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1327 | `	/* Compile the built-in library */` |
|     1186 |  1328 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1329 | `	/* Reset the code generator */` |
|     1186 |  1330 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     1186 |  1331 | `	return SXRET_OK;` |
|      ! 0 |  1332 | `Err:` |
|      ! 0 |  1333 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1334 | `	return rc;` |
|      594 |  1335 |  |
|        - |  1336 | `/*` |
|        - |  1337 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1338 | ` * routine which store the output in an internal blob.` |
|        - |  1339 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1340 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1341 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1342 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1343 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1344 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1345 | ` * to finish executing and extracting the output.` |
|        - |  1346 | ` */` |
|      ! 0 |  1347 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1348 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1349 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1350 | `	void *pUserData     /* User private data */` |
|        - |  1351 | `	)` |
|      ! 0 |  1352 |  |
|        - |  1353 | `	 sxi32 rc;` |
|        - |  1354 | `	 /* Store the output in an internal BLOB */` |
|      ! 0 |  1355 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|      ! 0 |  1356 | `	 return rc;` |
|      ! 0 |  1357 |  |
|        - |  1358 | `#define VM_STACK_GUARD 16` |
|        - |  1359 | `/*` |
|        - |  1360 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1361 | ` * our compiled PHP program.` |
|        - |  1362 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1363 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1364 | ` */` |
|    17528 |  1365 | `static ph7_value * VmNewOperandStack(` |
|        - |  1366 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1367 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1368 | `	)` |
|        2 |  1369 |  |
|        - |  1370 | `	ph7_value *pStack;` |
|        - |  1371 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1372 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1373 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1374 | `  ** on the maximum stack depth required.` |
|        - |  1375 | `  **` |
|        - |  1376 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1377 | `  */` |
|    17530 |  1378 | `	nInstr += VM_STACK_GUARD;` |
|    17530 |  1379 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    17530 |  1380 | `	if( pStack == 0 ){` |
|      ! 0 |  1381 | `		return 0;` |
|        - |  1382 | `	}` |
|        - |  1383 | `	/* Initialize the operand stack */` |
|   955214 |  1384 | `	while( nInstr > 0 ){` |
|   937686 |  1385 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|   937686 |  1386 | `		--nInstr;` |
|        2 |  1387 | `	}` |
|        - |  1388 | `	/* Ready for bytecode execution */` |
|    17530 |  1389 | `	return pStack;` |
|     8766 |  1390 |  |
|        - |  1391 | `/* Forward declaration */` |
|        - |  1392 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1393 | `static int VmInstanceOf(ph7_class *pThis,ph7_class *pClass);` |
|        - |  1394 | `static int VmClassMemberAccess(ph7_vm *pVm,ph7_class *pClass,const SyString *pAttrName,sxi32 iProtection,int bLog);` |
|        - |  1395 | `/*` |
|        - |  1396 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1397 | ` * This routine gets called by the PH7 engine after` |
|        - |  1398 | ` * successful compilation of the target PHP program.` |
|        - |  1399 | ` */` |
|      924 |  1400 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1401 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1402 | `	)` |
|        2 |  1403 |  |
|        - |  1404 | `	SyHashEntry *pEntry;` |
|        - |  1405 | `	sxi32 rc;` |
|      926 |  1406 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1407 | `		/* Initialize your VM first */` |
|      ! 0 |  1408 | `		return SXERR_CORRUPT;` |
|        - |  1409 | `	}` |
|        - |  1410 | `	/* Mark the VM ready for byte-code execution */` |
|      926 |  1411 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1412 | `	/* Release the code generator now we have compiled our program */` |
|      926 |  1413 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1414 | `	/* Emit the DONE instruction */` |
|      926 |  1415 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|      926 |  1416 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1417 | `		return SXERR_MEM;` |
|        - |  1418 | `	}` |
|        - |  1419 | `	/* Script return value */` |
|      926 |  1420 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1421 | `	/* Allocate a new operand stack */` |
|      926 |  1422 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|      926 |  1423 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1424 | `		return SXERR_MEM;` |
|        - |  1425 | `	}` |
|        - |  1426 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1427 | `	 * private data. */` |
|      926 |  1428 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|      926 |  1429 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1430 | `	/* Allocate the reference table */` |
|      926 |  1431 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|      926 |  1432 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|      926 |  1433 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1434 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1435 | `		return SXERR_MEM;` |
|        - |  1436 | `	}` |
|        - |  1437 | `	/* Zero the reference table */` |
|      926 |  1438 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1439 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|      926 |  1440 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|      926 |  1441 | `	if( rc != SXRET_OK ){` |
|        - |  1442 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1443 | `		return rc;` |
|        - |  1444 | `	}` |
|        - |  1445 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|      926 |  1446 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|      926 |  1447 | `	if( rc != SXRET_OK ){` |
|        - |  1448 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1449 | `		return rc;` |
|        - |  1450 | `	}` |
|        - |  1451 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|      926 |  1452 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1453 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|      926 |  1454 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1455 | `	/* Initialize and install static and constants class attributes */` |
|      926 |  1456 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|     7416 |  1457 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|     6492 |  1458 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|     6492 |  1459 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1460 | `			return rc;` |
|        - |  1461 | `		}` |
|        2 |  1462 | `	}` |
|        - |  1463 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|      926 |  1464 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1465 | `	/* VM is ready for bytecode execution */` |
|      926 |  1466 | `	return SXRET_OK;` |
|      464 |  1467 |  |
|        - |  1468 | `/*` |
|        - |  1469 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1470 | ` */` |
|      ! 0 |  1471 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1472 |  |
|      ! 0 |  1473 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1474 | `		return SXERR_CORRUPT;` |
|        - |  1475 | `	}` |
|        - |  1476 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1477 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1478 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1479 | `	/* Set the ready flag */` |
|      ! 0 |  1480 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1481 | `	return SXRET_OK;` |
|      ! 0 |  1482 |  |
|        - |  1483 | `/*` |
|        - |  1484 | ` * Release a Virtual Machine.` |
|        - |  1485 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1486 | ` */` |
|      916 |  1487 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1488 |  |
|        - |  1489 | `	/* Set the stale magic number */` |
|      918 |  1490 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1491 | `	/* Release the private memory subsystem */` |
|      918 |  1492 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      918 |  1493 | `	return SXRET_OK;` |
|        2 |  1494 |  |
|        - |  1495 | `/*` |
|        - |  1496 | ` * Initialize a foreign function call context.` |
|        - |  1497 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1498 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1499 | ` * functions.` |
|        - |  1500 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1501 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1502 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1503 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1504 | ` */` |
|   318709 |  1505 | `static sxi32 VmInitCallContext(` |
|        - |  1506 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1507 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1508 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1509 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1510 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1511 | `	)` |
|        2 |  1512 |  |
|   318711 |  1513 | `	pOut->pFunc = pFunc;` |
|   318711 |  1514 | `	pOut->pVm   = pVm;` |
|   318711 |  1515 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   318711 |  1516 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1517 | `	/* Assume a null return value */` |
|   318711 |  1518 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   318711 |  1519 | `	pOut->pRet = pRet;` |
|   318711 |  1520 | `	pOut->iFlags = iFlags;` |
|   318711 |  1521 | `	return SXRET_OK;` |
|        2 |  1522 |  |
|        - |  1523 | `/*` |
|        - |  1524 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1525 | ` * left behind.` |
|        - |  1526 | ` */` |
|   318709 |  1527 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1528 |  |
|        - |  1529 | `	sxu32 n;` |
|   318711 |  1530 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     3908 |  1531 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    10808 |  1532 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     6902 |  1533 | `			if( apObj[n] == 0 ){` |
|        - |  1534 | `				/* Already released */` |
|      250 |  1535 | `				continue;` |
|        - |  1536 | `			}` |
|     6654 |  1537 | `			PH7_MemObjRelease(apObj[n]);` |
|     6654 |  1538 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     3328 |  1539 | `		}` |
|     3908 |  1540 | `		SySetRelease(&pCtx->sVar);` |
|     1953 |  1541 | `	}` |
|   318711 |  1542 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1543 | `		ph7_aux_data *aAux;` |
|        - |  1544 | `		void *pChunk;` |
|        - |  1545 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1546 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1547 | `		 */` |
|        9 |  1548 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1549 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1550 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1551 | `			/* Release the chunk */` |
|       25 |  1552 | `			if( pChunk ){` |
|       25 |  1553 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1554 | `			}` |
|       13 |  1555 | `		}` |
|        9 |  1556 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1557 | `	}` |
|   318711 |  1558 |  |
|        - |  1559 | `/*` |
|        - |  1560 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1561 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1562 | ` */` |
|      248 |  1563 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1564 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1565 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1566 | `	)` |
|        2 |  1567 |  |
|      250 |  1568 | `	if( pValue == 0 ){` |
|        - |  1569 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1570 | `		return;` |
|        - |  1571 | `	}` |
|      250 |  1572 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      250 |  1573 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1574 | `		sxu32 n;` |
|      936 |  1575 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|      936 |  1576 | `			if( apObj[n] == pValue ){` |
|      250 |  1577 | `				PH7_MemObjRelease(pValue);` |
|      250 |  1578 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1579 | `				/* Mark as released */` |
|      250 |  1580 | `				apObj[n] = 0;` |
|      250 |  1581 | `				break;` |
|        - |  1582 | `			}` |
|      345 |  1583 | `		}` |
|      124 |  1584 | `	}` |
|      126 |  1585 |  |
|        - |  1586 | `/*` |
|        - |  1587 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1588 | ` */` |
|  1501219 |  1589 | `static void VmPopOperand(` |
|        - |  1590 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1591 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1592 | `	)` |
|        2 |  1593 |  |
|  1501221 |  1594 | `	ph7_value *pTos = *ppTos;` |
|  3269712 |  1595 | `	while( nPop > 0 ){` |
|  1768493 |  1596 | `		PH7_MemObjRelease(pTos);` |
|  1768493 |  1597 | `		pTos--;` |
|  1768493 |  1598 | `		nPop--;` |
|        2 |  1599 | `	}` |
|        - |  1600 | `	/* Top of the stack */` |
|  1501221 |  1601 | `	*ppTos = pTos;` |
|  1501221 |  1602 |  |
|        - |  1603 | `/*` |
|        - |  1604 | ` * Reserve a memory object.` |
|        - |  1605 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1606 | ` */` |
|   565258 |  1607 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1608 |  |
|   565260 |  1609 | `	ph7_value *pObj = 0;` |
|        - |  1610 | `	VmSlot *pSlot;` |
|        - |  1611 | `	sxu32 nIdx;` |
|        - |  1612 | `	/* Check for a free slot */` |
|   565260 |  1613 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|   565260 |  1614 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|   565260 |  1615 | `	if( pSlot ){` |
|   490708 |  1616 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   490708 |  1617 | `		nIdx = pSlot->nIdx;` |
|   245353 |  1618 | `	}` |
|   565260 |  1619 | `	if( pObj == 0 ){` |
|        - |  1620 | `		/* Reserve a new memory object */` |
|    74554 |  1621 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|    74554 |  1622 | `		if( pObj == 0 ){` |
|      ! 0 |  1623 | `			return 0;` |
|        - |  1624 | `		}` |
|    37276 |  1625 | `	}` |
|        - |  1626 | `	/* Set a null default value */` |
|   565260 |  1627 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|   565260 |  1628 | `	pObj->nIdx = nIdx;` |
|   565260 |  1629 | `	return pObj;` |
|   282631 |  1630 |  |
|        - |  1631 | `/*` |
|        - |  1632 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1633 | ` */` |
|    13782 |  1634 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1635 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1636 | `	const char *zKey,  /* Entry key */` |
|        - |  1637 | `	sxu32 nByte,       /* Key length */` |
|        - |  1638 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1639 | `	)` |
|        2 |  1640 |  |
|        - |  1641 | `	ph7_value sKey;` |
|        - |  1642 | `	sxi32 rc;` |
|    13784 |  1643 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    13784 |  1644 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1645 | `	/* Perform the insertion */` |
|    13784 |  1646 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    13784 |  1647 | `	PH7_MemObjRelease(&sKey);` |
|    13784 |  1648 | `	return rc;` |
|        2 |  1649 |  |
|        - |  1650 | `/*` |
|        - |  1651 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1652 | ` * Return a pointer to the variable value on success.` |
|        - |  1653 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1654 | ` */` |
|  1257357 |  1655 | `static ph7_value * VmExtractMemObj(` |
|        - |  1656 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1657 | `	const SyString *pName, /* Variable name */` |
|        - |  1658 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1659 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1660 | `	)` |
|        2 |  1661 |  |
|  1257359 |  1662 | `	int bNullify = FALSE;` |
|        - |  1663 | `	SyHashEntry *pEntry;` |
|        - |  1664 | `	VmFrame *pFrame;` |
|        - |  1665 | `	ph7_value *pObj;` |
|        - |  1666 | `	sxu32 nIdx;` |
|        - |  1667 | `	sxi32 rc;` |
|        - |  1668 | `	/* Point to the top active frame */` |
|  1257359 |  1669 | `	pFrame = pVm->pFrame;` |
|  1257371 |  1670 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  1671 | `		/* Safely ignore the exception frame */` |
|       13 |  1672 | `		pFrame = pFrame->pParent; /* Parent frame */` |
|        1 |  1673 | `	}` |
|        - |  1674 | `	/* Perform the lookup */` |
|  1257359 |  1675 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1676 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1677 | `		pName = &sAnnon;` |
|        - |  1678 | `		/* Always nullify the object */` |
|      ! 0 |  1679 | `		bNullify = TRUE;` |
|      ! 0 |  1680 | `		bDup = FALSE;` |
|      ! 0 |  1681 | `	}` |
|        - |  1682 | `	/* Check the superglobals table first */` |
|  1257359 |  1683 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  1257359 |  1684 | `	if( pEntry == 0 ){` |
|        - |  1685 | `		/* Query the top active frame */` |
|  1257329 |  1686 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  1257329 |  1687 | `		if( pEntry == 0 ){` |
|    41234 |  1688 | `			char *zName = (char *)pName->zString;` |
|        - |  1689 | `			VmSlot sLocal;` |
|    41234 |  1690 | `			if( !bCreate ){` |
|        - |  1691 | `				/* Do not create the variable,return NULL instead */` |
|      466 |  1692 | `				return 0;` |
|        - |  1693 | `			}` |
|        - |  1694 | `			/* No such variable,automatically create a new one and install` |
|        - |  1695 | `			 * it in the current frame.` |
|        - |  1696 | `			 */` |
|    40770 |  1697 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    40770 |  1698 | `			if( pObj == 0 ){` |
|      ! 0 |  1699 | `				return 0;` |
|        - |  1700 | `			}` |
|    40770 |  1701 | `			nIdx = pObj->nIdx;` |
|    40770 |  1702 | `			if( bDup ){` |
|        - |  1703 | `				/* Duplicate name */` |
|      115 |  1704 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      115 |  1705 | `				if( zName == 0 ){` |
|      ! 0 |  1706 | `					return 0;` |
|        - |  1707 | `				}` |
|       57 |  1708 | `			}` |
|        - |  1709 | `			/* Link to the top active VM frame */` |
|    40770 |  1710 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    40770 |  1711 | `			if( rc != SXRET_OK ){` |
|        - |  1712 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1713 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1714 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1715 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1716 | `				return 0;` |
|        - |  1717 | `			}` |
|    40770 |  1718 | `			if( pFrame->pParent != 0 ){` |
|        - |  1719 | `				/* Local variable */` |
|    36252 |  1720 | `				sLocal.nIdx = nIdx;` |
|    36252 |  1721 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    18127 |  1722 | `			}else{` |
|        - |  1723 | `				/* Register in the $GLOBALS array */` |
|     4520 |  1724 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1725 | `			}` |
|        - |  1726 | `			/* Install in the reference table */` |
|    40770 |  1727 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1728 | `			/* Save object index */` |
|    40770 |  1729 | `			pObj->nIdx = nIdx;` |
|    20386 |  1730 | `		}else{` |
|        - |  1731 | `			/* Extract variable contents */` |
|  1216097 |  1732 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  1216097 |  1733 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  1216097 |  1734 | `			if( bNullify && pObj ){` |
|      ! 0 |  1735 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1736 | `			}` |
|        - |  1737 | `		}` |
|   628441 |  1738 | `	}else{` |
|        - |  1739 | `		/* Superglobal */` |
|       31 |  1740 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       31 |  1741 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1742 | `	}` |
|  1256895 |  1743 | `	return pObj;` |
|   628688 |  1744 |  |
|        - |  1745 | `/*` |
|        - |  1746 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1747 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1748 | ` */` |
|      934 |  1749 | `static ph7_value * VmExtractSuper(` |
|        - |  1750 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1751 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1752 | `	sxu32 nByte        /* zName length */` |
|        - |  1753 | `	)` |
|        2 |  1754 |  |
|        - |  1755 | `	SyHashEntry *pEntry;` |
|        - |  1756 | `	ph7_value *pValue;` |
|        - |  1757 | `	sxu32 nIdx;` |
|        - |  1758 | `	/* Query the superglobal table */` |
|      936 |  1759 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|      936 |  1760 | `	if( pEntry == 0 ){` |
|        - |  1761 | `		/* No such entry */` |
|      ! 0 |  1762 | `		return 0;` |
|        - |  1763 | `	}` |
|        - |  1764 | `	/* Extract the superglobal index in the global object pool */` |
|      936 |  1765 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1766 | `	/* Extract the variable value  */` |
|      936 |  1767 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      936 |  1768 | `	return pValue;` |
|      469 |  1769 |  |
|        - |  1770 | `/*` |
|        - |  1771 | ` * Perform a raw hashmap insertion.` |
|        - |  1772 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1773 | ` */` |
|      932 |  1774 | `static sxi32 VmHashmapInsert(` |
|        - |  1775 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1776 | `	const char *zKey,   /* Entry key */` |
|        - |  1777 | `	int nKeylen,        /* zKey length*/` |
|        - |  1778 | `	const char *zData,  /* Entry data */` |
|        - |  1779 | `	int nLen            /* zData length */` |
|        - |  1780 | `	)` |
|        2 |  1781 |  |
|        - |  1782 | `	ph7_value sKey,sValue;` |
|        - |  1783 | `	sxi32 rc;` |
|      934 |  1784 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|      934 |  1785 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|      934 |  1786 | `	if( zKey ){` |
|      928 |  1787 | `		if( nKeylen < 0 ){` |
|      928 |  1788 | `			nKeylen = (int)SyStrlen(zKey);` |
|      463 |  1789 | `		}` |
|      928 |  1790 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|      463 |  1791 | `	}` |
|      934 |  1792 | `	if( zData ){` |
|      934 |  1793 | `		if( nLen < 0 ){` |
|        - |  1794 | `			/* Compute length automatically */` |
|      ! 0 |  1795 | `			nLen = (int)SyStrlen(zData);` |
|      ! 0 |  1796 | `		}` |
|      934 |  1797 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|      466 |  1798 | `	}` |
|        - |  1799 | `	/* Perform the insertion */` |
|      934 |  1800 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|      934 |  1801 | `	PH7_MemObjRelease(&sKey);` |
|      934 |  1802 | `	PH7_MemObjRelease(&sValue);` |
|      934 |  1803 | `	return rc;` |
|        2 |  1804 |  |
|        - |  1805 | `/* Forward declaration */` |
|        - |  1806 | `static sxi32 VmHttpProcessRequest(ph7_vm *pVm,const char *zRequest,int nByte);` |
|        - |  1807 | `/*` |
|        - |  1808 | ` * Configure a working virtual machine instance.` |
|        - |  1809 | ` *` |
|        - |  1810 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  1811 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  1812 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  1813 | ` * The second argument to this function is an integer configuration option` |
|        - |  1814 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  1815 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  1816 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  1817 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  1818 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  1819 | ` */` |
|    14792 |  1820 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1821 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1822 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1823 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1824 | `	)` |
|        2 |  1825 |  |
|    14794 |  1826 | `	sxi32 rc = SXRET_OK;` |
|    14794 |  1827 | `	switch(nOp){` |
|      462 |  1828 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|      926 |  1829 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|      926 |  1830 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1831 | `		/* VM output consumer callback */` |
|        - |  1832 | `#ifdef UNTRUST` |
|        - |  1833 | `		if( xConsumer == 0 ){` |
|        - |  1834 | `			rc = SXERR_CORRUPT;` |
|        - |  1835 | `			break;` |
|        - |  1836 | `		}` |
|        - |  1837 | `#endif` |
|        - |  1838 | `		/* Install the output consumer */` |
|      926 |  1839 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|      926 |  1840 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|      926 |  1841 | `		break;` |
|        - |  1842 | `							   }` |
|      462 |  1843 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1844 | `		/* Import path */` |
|        - |  1845 | `		  const char *zPath;` |
|        - |  1846 | `		  SyString sPath;` |
|      926 |  1847 | `		  zPath = va_arg(ap,const char *);` |
|        - |  1848 | `#if defined(UNTRUST)` |
|        - |  1849 | `		  if( zPath == 0 ){` |
|        - |  1850 | `			  rc = SXERR_EMPTY;` |
|        - |  1851 | `			  break;` |
|        - |  1852 | `		  }` |
|        - |  1853 | `#endif` |
|      926 |  1854 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  1855 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  1856 | `#ifdef __WINNT__` |
|        2 |  1857 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  1858 | `#endif` |
|     1850 |  1859 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  1860 | `		  /* Remove leading and trailing white spaces */` |
|      926 |  1861 | `		  SyStringFullTrim(&sPath);` |
|      926 |  1862 | `		  if( sPath.nByte > 0 ){` |
|        - |  1863 | `			  /* Store the path in the corresponding conatiner */` |
|      926 |  1864 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|      462 |  1865 | `		  }` |
|      926 |  1866 | `		  break;` |
|        - |  1867 | `									 }` |
|      462 |  1868 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  1869 | `		/* Run-Time Error report */` |
|      926 |  1870 | `		pVm->bErrReport = 1;` |
|      926 |  1871 | `		break;` |
|      ! 0 |  1872 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  1873 | `		/* Recursion depth */` |
|      ! 0 |  1874 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  1875 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  1876 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  1877 | `		}` |
|      ! 0 |  1878 | `		break;` |
|        - |  1879 | `									   }` |
|      ! 0 |  1880 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  1881 | `		/* VM output length in bytes */` |
|      ! 0 |  1882 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  1883 | `#ifdef UNTRUST` |
|        - |  1884 | `		if( pOut == 0 ){` |
|        - |  1885 | `			rc = SXERR_CORRUPT;` |
|        - |  1886 | `			break;` |
|        - |  1887 | `		}` |
|        - |  1888 | `#endif` |
|      ! 0 |  1889 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  1890 | `		break;` |
|        - |  1891 | `							   }` |
|        - |  1892 |  |
|     4620 |  1893 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  1894 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  1895 | `		/* Create a new superglobal/global variable */` |
|     9242 |  1896 | `		const char *zName = va_arg(ap,const char *);` |
|     9242 |  1897 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  1898 | `		SyHashEntry *pEntry;` |
|        - |  1899 | `		ph7_value *pObj;` |
|        - |  1900 | `		sxu32 nByte;` |
|        - |  1901 | `		sxu32 nIdx;` |
|        - |  1902 | `#ifdef UNTRUST` |
|        - |  1903 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  1904 | `			rc = SXERR_CORRUPT;` |
|        - |  1905 | `			break;` |
|        - |  1906 | `		}` |
|        - |  1907 | `#endif` |
|     9242 |  1908 | `		nByte = SyStrlen(zName);` |
|     9242 |  1909 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1910 | `			/* Check if the superglobal is already installed */` |
|     9242 |  1911 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     4622 |  1912 | `		}else{` |
|        - |  1913 | `			/* Query the top active VM frame */` |
|      ! 0 |  1914 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  1915 | `		}` |
|     9242 |  1916 | `		if( pEntry ){` |
|        - |  1917 | `			/* Variable already installed */` |
|      ! 0 |  1918 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1919 | `			/* Extract contents */` |
|      ! 0 |  1920 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  1921 | `			if( pObj ){` |
|        - |  1922 | `				/* Overwrite old contents */` |
|      ! 0 |  1923 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  1924 | `			}` |
|      ! 0 |  1925 | `		}else{` |
|        - |  1926 | `			/* Install a new variable */` |
|     9242 |  1927 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|     9242 |  1928 | `			if( pObj == 0 ){` |
|      ! 0 |  1929 | `				rc = SXERR_MEM;` |
|      ! 0 |  1930 | `				break;` |
|        - |  1931 | `			}` |
|     9242 |  1932 | `			nIdx = pObj->nIdx;` |
|        - |  1933 | `			/* Copy value */` |
|     9242 |  1934 | `			PH7_MemObjStore(pValue,pObj);` |
|     9242 |  1935 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1936 | `				/* Install the superglobal */` |
|     9242 |  1937 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|     4622 |  1938 | `			}else{` |
|        - |  1939 | `				/* Install in the current frame */` |
|      ! 0 |  1940 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  1941 | `			}` |
|     9242 |  1942 | `			if( rc == SXRET_OK ){` |
|        - |  1943 | `				SyHashEntry *pRef;` |
|     9242 |  1944 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|     9242 |  1945 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|     4622 |  1946 | `				}else{` |
|      ! 0 |  1947 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  1948 | `				}` |
|        - |  1949 | `				/* Install in the reference table */` |
|     9242 |  1950 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|     9242 |  1951 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  1952 | `					/* Register in the $GLOBALS array */` |
|     9242 |  1953 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|     4620 |  1954 | `				}` |
|     4620 |  1955 | `			}` |
|        - |  1956 | `		}` |
|     9242 |  1957 | `		break;` |
|        - |  1958 | `									}` |
|      463 |  1959 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  1960 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  1961 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  1962 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  1963 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  1964 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  1965 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|      928 |  1966 | `		const char *zKey   = va_arg(ap,const char *);` |
|      928 |  1967 | `		const char *zValue = va_arg(ap,const char *);` |
|      928 |  1968 | `		int nLen = va_arg(ap,int);` |
|        - |  1969 | `		ph7_hashmap *pMap;` |
|        - |  1970 | `		ph7_value *pValue;` |
|      928 |  1971 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  1972 | `			/* Extract the $_ENV superglobal */` |
|        3 |  1973 | `			pValue = VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|      927 |  1974 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  1975 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  1976 | `			pValue = VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|      926 |  1977 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  1978 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  1979 | `			pValue = VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|      926 |  1980 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  1981 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  1982 | `			pValue = VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|      926 |  1983 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  1984 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  1985 | `			pValue = VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|      926 |  1986 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  1987 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  1988 | `			pValue = VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  1989 | `		}else{` |
|        - |  1990 | `			/* Extract the $_SERVER superglobal */` |
|      926 |  1991 | `			pValue = VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  1992 | `		}` |
|      928 |  1993 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  1994 | `			/* No such entry */` |
|      ! 0 |  1995 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  1996 | `			break;` |
|        - |  1997 | `		}` |
|        - |  1998 | `		/* Point to the hashmap */` |
|      928 |  1999 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2000 | `		/* Perform the insertion */` |
|      928 |  2001 | `		rc = VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|      928 |  2002 | `		break;` |
|        - |  2003 | `								   }` |
|        3 |  2004 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2005 | `		/* Script arguments */` |
|        7 |  2006 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2007 | `		ph7_hashmap *pMap;` |
|        - |  2008 | `		ph7_value *pValue;` |
|        - |  2009 | `		sxu32 n;` |
|        7 |  2010 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2011 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2012 | `			break;` |
|        - |  2013 | `		}` |
|        - |  2014 | `		/* Extract the $argv array */` |
|        7 |  2015 | `		pValue = VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|        7 |  2016 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2017 | `			/* No such entry */` |
|      ! 0 |  2018 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2019 | `			break;` |
|        - |  2020 | `		}` |
|        - |  2021 | `		/* Point to the hashmap */` |
|        7 |  2022 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2023 | `		/* Perform the insertion */` |
|        7 |  2024 | `		n = (sxu32)SyStrlen(zValue);` |
|        7 |  2025 | `		rc = VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|        7 |  2026 | `		if( rc == SXRET_OK ){` |
|        7 |  2027 | `			if( pMap->nEntry > 1 ){` |
|        - |  2028 | `				/* Append space separator first */` |
|        3 |  2029 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        1 |  2030 | `			}` |
|        7 |  2031 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|        3 |  2032 | `		}` |
|        7 |  2033 | `		break;` |
|        - |  2034 | `								  }` |
|      ! 0 |  2035 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2036 | `		/* error_log() consumer */` |
|      ! 0 |  2037 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2038 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2039 | `		break;` |
|        - |  2040 | `										}` |
|      ! 0 |  2041 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2042 | `		/* Script return value */` |
|      ! 0 |  2043 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2044 | `#ifdef UNTRUST` |
|        - |  2045 | `		if( ppValue == 0 ){` |
|        - |  2046 | `			rc = SXERR_CORRUPT;` |
|        - |  2047 | `			break;` |
|        - |  2048 | `		}` |
|        - |  2049 | `#endif` |
|      ! 0 |  2050 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2051 | `		break;` |
|        - |  2052 | `								   }` |
|      924 |  2053 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2054 | `		/* Register an IO stream device */` |
|     1850 |  2055 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2056 | `		/* Make sure we are dealing with a valid IO stream */` |
|     2772 |  2057 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     1850 |  2058 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2059 | `				/* Invalid stream */` |
|      ! 0 |  2060 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2061 | `				break;` |
|        - |  2062 | `		}` |
|     1850 |  2063 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2064 | `			/* Make the 'file://' stream the defaut stream device */` |
|      926 |  2065 | `			pVm->pDefStream = pStream;` |
|      462 |  2066 | `		}` |
|        - |  2067 | `		/* Insert in the appropriate container */` |
|     1850 |  2068 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     1850 |  2069 | `		break;` |
|        - |  2070 | `								  }` |
|      ! 0 |  2071 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2072 | `		/* Point to the VM internal output consumer buffer */` |
|      ! 0 |  2073 | `		const void **ppOut = va_arg(ap,const void **);` |
|      ! 0 |  2074 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2075 | `#ifdef UNTRUST` |
|        - |  2076 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2077 | `			rc = SXERR_CORRUPT;` |
|        - |  2078 | `			break;` |
|        - |  2079 | `		}` |
|        - |  2080 | `#endif` |
|      ! 0 |  2081 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|      ! 0 |  2082 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|      ! 0 |  2083 | `		break;` |
|        - |  2084 | `									   }` |
|      ! 0 |  2085 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2086 | `		/* Raw HTTP request*/` |
|      ! 0 |  2087 | `		const char *zRequest = va_arg(ap,const char *);` |
|      ! 0 |  2088 | `		int nByte = va_arg(ap,int);` |
|      ! 0 |  2089 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2090 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2091 | `			break;` |
|        - |  2092 | `		}` |
|      ! 0 |  2093 | `		if( nByte < 0 ){` |
|        - |  2094 | `			/* Compute length automatically */` |
|      ! 0 |  2095 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2096 | `		}` |
|        - |  2097 | `		/* Process the request */` |
|      ! 0 |  2098 | `		rc = VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|      ! 0 |  2099 | `		break;` |
|        - |  2100 | `									}` |
|      ! 0 |  2101 | `	default:` |
|        - |  2102 | `		/* Unknown configuration option */` |
|      ! 0 |  2103 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2104 | `		break;` |
|        - |  2105 | `	}` |
|    14794 |  2106 | `	return rc;` |
|        2 |  2107 |  |
|        - |  2108 | `/* Forward declaration */` |
|        - |  2109 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2110 | `/*` |
|        - |  2111 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2112 | ` * format.` |
|        - |  2113 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2114 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2115 | ` * (STDOUT).` |
|        - |  2116 | ` */` |
|        2 |  2117 | `static sxi32 VmByteCodeDump(` |
|        - |  2118 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2119 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2120 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2121 | `	)` |
|        1 |  2122 |  |
|        - |  2123 | `	static const char zDump[] = {` |
|        - |  2124 | `		"====================================================\n"` |
|        - |  2125 | `		"PH7 VM Dump\n"` |
|        - |  2126 | `		"====================================================\n"` |
|        - |  2127 | `	};` |
|        - |  2128 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2129 | `	sxi32 rc = SXRET_OK;` |
|        - |  2130 | `	sxu32 n;` |
|        - |  2131 | `	/* Point to the PH7 instructions */` |
|        3 |  2132 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2133 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2134 | `	n = 0;` |
|        3 |  2135 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2136 | `	/* Dump instructions */` |
|        6 |  2137 | `	for(;;){` |
|       13 |  2138 | `		if( pInstr >= pEnd ){` |
|        - |  2139 | `			/* No more instructions */` |
|        3 |  2140 | `			break;` |
|        - |  2141 | `		}` |
|        - |  2142 | `		/* Format and call the consumer callback */` |
|       16 |  2143 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       10 |  2144 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       10 |  2145 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       11 |  2146 | `		if( rc != SXRET_OK ){` |
|        - |  2147 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2148 | `			return rc;` |
|        - |  2149 | `		}` |
|       11 |  2150 | `		++n;` |
|       11 |  2151 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2152 | `	}` |
|        3 |  2153 | `	return rc;` |
|        2 |  2154 |  |
|        - |  2155 | `/* Forward declaration */` |
|        - |  2156 | `static int VmObConsumer(const void *pData,unsigned int nDataLen,void *pUserData);` |
|        - |  2157 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2158 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2159 | `/*` |
|        - |  2160 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2161 | ` * consumer callback.` |
|        - |  2162 | ` */` |
|       80 |  2163 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        2 |  2164 |  |
|       82 |  2165 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|       82 |  2166 | `	sxi32 rc = SXRET_OK;` |
|        - |  2167 | `	/* Append a new line */` |
|        - |  2168 | `#ifdef __WINNT__` |
|        2 |  2169 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2170 | `#else` |
|       80 |  2171 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2172 | `#endif` |
|        - |  2173 | `	/* Invoke the output consumer callback */` |
|       82 |  2174 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|       82 |  2175 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  2176 | `		/* Increment output length */` |
|       81 |  2177 | `		pVm->nOutputLen += SyBlobLength(pMsg);` |
|       40 |  2178 | `	}` |
|       82 |  2179 | `	return rc;` |
|        2 |  2180 |  |
|        - |  2181 | `/*` |
|        - |  2182 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2183 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2184 | ` * information.` |
|        - |  2185 | ` */` |
|       88 |  2186 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, SyString *pFile, sxi32 iLine)` |
|        2 |  2187 |  |
|       90 |  2188 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2189 | `		ph7_value apArg[4];` |
|        - |  2190 | `		ph7_value *apArgPtr[4];` |
|        - |  2191 | `		ph7_value sResult;` |
|        - |  2192 | `		SyString sErr;` |
|        - |  2193 | `		/* Prepare arguments */` |
|        9 |  2194 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        9 |  2195 | `		SyStringInitFromBuf(&sErr,zMessage,SyStrlen(zMessage));` |
|        9 |  2196 | `		PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|        9 |  2197 | `		if( pFile ){` |
|        9 |  2198 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|        9 |  2199 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|        5 |  2200 | `		}else{` |
|      ! 0 |  2201 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2202 | `		}` |
|        9 |  2203 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|        9 |  2204 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2205 | `		/* Set up pointer array */` |
|        9 |  2206 | `		apArgPtr[0] = &apArg[0];` |
|        9 |  2207 | `		apArgPtr[1] = &apArg[1];` |
|        9 |  2208 | `		apArgPtr[2] = &apArg[2];` |
|        9 |  2209 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2210 | `		/* Call the handler */` |
|        9 |  2211 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2212 | `		/* Check return value */` |
|        9 |  2213 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2214 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2215 | `		}` |
|        - |  2216 | `		/* Release */` |
|        9 |  2217 | `		PH7_MemObjRelease(&apArg[0]);` |
|        9 |  2218 | `		PH7_MemObjRelease(&apArg[1]);` |
|        9 |  2219 | `		PH7_MemObjRelease(&apArg[2]);` |
|        9 |  2220 | `		PH7_MemObjRelease(&apArg[3]);` |
|        9 |  2221 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2222 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2223 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|        9 |  2224 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2225 | `	}` |
|        - |  2226 | `	/* No handler, always call error handler */` |
|       82 |  2227 | `	return TRUE;` |
|       46 |  2228 |  |
|       62 |  2229 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2230 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2231 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2232 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2233 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2234 | `	)` |
|        2 |  2235 |  |
|       64 |  2236 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2237 | `	SyString *pFile;` |
|        - |  2238 | `	char *zErr;` |
|       64 |  2239 | `	sxi32 rc = SXRET_OK;` |
|       64 |  2240 | `	if( !pVm->bErrReport ){` |
|        - |  2241 | `		/* Don't bother reporting errors */` |
|        3 |  2242 | `		return SXRET_OK;` |
|        - |  2243 | `	}` |
|        - |  2244 | `	/* Reset the working buffer */` |
|       62 |  2245 | `	SyBlobReset(pWorker);` |
|        - |  2246 | `	/* Peek the processed file if available */` |
|       62 |  2247 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       62 |  2248 | `	if( pFile ){` |
|        - |  2249 | `		/* Append file name */` |
|       62 |  2250 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       62 |  2251 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       30 |  2252 | `	}` |
|       62 |  2253 | `	zErr = "Error: ";` |
|       62 |  2254 | `	switch(iErr){` |
|       27 |  2255 | `	case PH7_CTX_WARNING: zErr = "Warning: "; break;` |
|       14 |  2256 | `	case PH7_CTX_NOTICE:  zErr = "Notice: ";  break;` |
|       11 |  2257 | `	default:` |
|       23 |  2258 | `		iErr = PH7_CTX_ERR;` |
|       22 |  2259 | `		break;` |
|        - |  2260 | `	}` |
|       62 |  2261 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       62 |  2262 | `	if( pFuncName ){` |
|        - |  2263 | `		/* Append function name first */` |
|       29 |  2264 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       29 |  2265 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       14 |  2266 | `	}` |
|       62 |  2267 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2268 | `	/* Check for user error handler */` |
|       62 |  2269 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, pFile, 0) ){` |
|       53 |  2270 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       26 |  2271 | `	}` |
|       62 |  2272 | `	return rc;` |
|       33 |  2273 |  |
|        - |  2274 | `/*` |
|        - |  2275 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2276 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2277 | ` * information.` |
|        - |  2278 | ` */` |
|       28 |  2279 | `static sxi32 VmThrowErrorAp(` |
|        - |  2280 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2281 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2282 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2283 | `	const char *zFormat, /* Format message */` |
|        - |  2284 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2285 | `	)` |
|        2 |  2286 |  |
|       30 |  2287 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2288 | `	SyBlob sMsg;` |
|        - |  2289 | `	SyString *pFile;` |
|        - |  2290 | `	char *zErr;` |
|       30 |  2291 | `	sxi32 rc = SXRET_OK;` |
|       30 |  2292 | `	if( !pVm->bErrReport ){` |
|        - |  2293 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2294 | `		return SXRET_OK;` |
|        - |  2295 | `	}` |
|        - |  2296 | `	/* Reset the working buffer */` |
|       30 |  2297 | `	SyBlobReset(pWorker);` |
|        - |  2298 | `	/* Peek the processed file if available */` |
|       30 |  2299 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       30 |  2300 | `	if( pFile ){` |
|        - |  2301 | `		/* Append file name */` |
|       30 |  2302 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       30 |  2303 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       14 |  2304 | `	}` |
|       30 |  2305 | `	zErr = "Error: ";` |
|       30 |  2306 | `	switch(iErr){` |
|       10 |  2307 | `	case PH7_CTX_WARNING: zErr = "Warning: "; break;` |
|        7 |  2308 | `	case PH7_CTX_NOTICE:  zErr = "Notice: ";  break;` |
|        7 |  2309 | `	default:` |
|       15 |  2310 | `		iErr = PH7_CTX_ERR;` |
|       14 |  2311 | `		break;` |
|        - |  2312 | `	}` |
|       30 |  2313 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       30 |  2314 | `	if( pFuncName ){` |
|        - |  2315 | `		/* Append function name first */` |
|       14 |  2316 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       14 |  2317 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|        6 |  2318 | `	}` |
|        - |  2319 | `	/* Format the raw message */` |
|       30 |  2320 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       30 |  2321 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2322 | `	/* Check if a user error handler is installed */` |
|       30 |  2323 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), pFile, 0) ){` |
|        - |  2324 | `		/* No handler or handler returned TRUE, normal processing */` |
|       30 |  2325 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       30 |  2326 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       14 |  2327 | `	}` |
|       30 |  2328 | `	SyBlobRelease(&sMsg);` |
|       30 |  2329 | `	return rc;` |
|       16 |  2330 |  |
|        - |  2331 | `/*` |
|        - |  2332 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2333 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2334 | ` * information.` |
|        - |  2335 | ` * ------------------------------------` |
|        - |  2336 | ` * Simple boring wrapper function.` |
|        - |  2337 | ` * ------------------------------------` |
|        - |  2338 | ` */` |
|       16 |  2339 | `static sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  2340 |  |
|        - |  2341 | `	va_list ap;` |
|        - |  2342 | `	sxi32 rc;` |
|       17 |  2343 | `	va_start(ap,zFormat);` |
|       17 |  2344 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       17 |  2345 | `	va_end(ap);` |
|       17 |  2346 | `	return rc;` |
|        1 |  2347 |  |
|        - |  2348 | `/*` |
|        - |  2349 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2350 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2351 | ` * information.` |
|        - |  2352 | ` * ------------------------------------` |
|        - |  2353 | ` * Simple boring wrapper function.` |
|        - |  2354 | ` * ------------------------------------` |
|        - |  2355 | ` */` |
|       12 |  2356 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  2357 |  |
|        - |  2358 | `	sxi32 rc;` |
|       14 |  2359 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       14 |  2360 | `	return rc;` |
|        2 |  2361 |  |
|        - |  2362 | `/*` |
|        - |  2363 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  2364 | ` *` |
|        - |  2365 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  2366 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  2367 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  2368 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  2369 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  2370 | ` * then the program execution is halted.` |
|        - |  2371 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  2372 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  2373 | ` * or to reset the VM to it's initial state.` |
|        - |  2374 | ` */` |
|    17528 |  2375 | `static sxi32 VmByteCodeExec(` |
|        - |  2376 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2377 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  2378 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  2379 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  2380 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  2381 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  2382 | `	int is_callback      /* TRUE if we are executing a callback */` |
|        - |  2383 | `	)` |
|        2 |  2384 |  |
|        - |  2385 | `	VmInstr *pInstr;` |
|        - |  2386 | `	ph7_value *pTos;` |
|        - |  2387 | `	SySet aArg;` |
|        - |  2388 | `	sxi32 pc;` |
|        - |  2389 | `	sxi32 rc;` |
|        - |  2390 | `	/* Argument container */` |
|    17530 |  2391 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    17530 |  2392 | `	if( nTos < 0 ){` |
|    17030 |  2393 | `		pTos = &pStack[-1];` |
|     8516 |  2394 | `	}else{` |
|      502 |  2395 | `		pTos = &pStack[nTos];` |
|        - |  2396 | `	}` |
|    17530 |  2397 | `	pc = 0;` |
|        - |  2398 | `	/* Execute as much as we can */` |
|  2231429 |  2399 | `	for(;;){` |
|        - |  2400 | `		/* Fetch the instruction to execute */` |
|  4462812 |  2401 | `		pInstr = &aInstr[pc];` |
|  4462812 |  2402 | `		rc = SXRET_OK;` |
|        - |  2403 | `/*` |
|        - |  2404 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2405 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2406 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2407 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2408 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2409 | ` */` |
|  4462812 |  2410 | `		switch(pInstr->iOp){` |
|        - |  2411 | `/*` |
|        - |  2412 | ` * DONE: P1 * *` |
|        - |  2413 | ` *` |
|        - |  2414 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2415 | ` * and return immediately.` |
|        - |  2416 | ` */` |
|     8757 |  2417 | `case PH7_OP_DONE:` |
|    17516 |  2418 | `	if( pInstr->iP1 ){` |
|        - |  2419 | `#ifdef UNTRUST` |
|        - |  2420 | `		if( pTos < pStack ){` |
|        - |  2421 | `			goto Abort;` |
|        - |  2422 | `		}` |
|        - |  2423 | `#endif` |
|     8574 |  2424 | `		if( pLastRef ){` |
|     5170 |  2425 | `			*pLastRef = pTos->nIdx;` |
|     2584 |  2426 | `		}` |
|     8574 |  2427 | `		if( pResult ){` |
|        - |  2428 | `			/* Execution result */` |
|     8288 |  2429 | `			PH7_MemObjStore(pTos,pResult);` |
|     4143 |  2430 | `		}` |
|     8574 |  2431 | `		VmPopOperand(&pTos,1);` |
|    13230 |  2432 | `	}else if( pLastRef ){` |
|        - |  2433 | `		/* Nothing referenced */` |
|      372 |  2434 | `		*pLastRef = SXU32_HIGH;` |
|      185 |  2435 | `	}` |
|    17516 |  2436 | `	goto Done;` |
|        - |  2437 | `/*` |
|        - |  2438 | ` * HALT: P1 * *` |
|        - |  2439 | ` *` |
|        - |  2440 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  2441 | ` * and abort immediately.` |
|        - |  2442 | ` */` |
|        4 |  2443 | `case PH7_OP_HALT:` |
|        9 |  2444 | `	if( pInstr->iP1 ){` |
|        - |  2445 | `#ifdef UNTRUST` |
|        - |  2446 | `		if( pTos < pStack ){` |
|        - |  2447 | `			goto Abort;` |
|        - |  2448 | `		}` |
|        - |  2449 | `#endif` |
|        9 |  2450 | `		if( pLastRef ){` |
|      ! 0 |  2451 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  2452 | `		}` |
|        9 |  2453 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  2454 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2455 | `				/* Output the exit message */` |
|        7 |  2456 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  2457 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  2458 | `				if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  2459 | `					/* Increment output length */` |
|        5 |  2460 | `					pVm->nOutputLen += SyBlobLength(&pTos->sBlob);` |
|        2 |  2461 | `				}` |
|        3 |  2462 | `			}` |
|        7 |  2463 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  2464 | `			/* Record exit status */` |
|        5 |  2465 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  2466 | `		}` |
|        9 |  2467 | `		VmPopOperand(&pTos,1);` |
|        4 |  2468 | `	}else if( pLastRef ){` |
|        - |  2469 | `		/* Nothing referenced */` |
|      ! 0 |  2470 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  2471 | `	}` |
|        - |  2472 | `	/* Check if we're in an included file context */` |
|        9 |  2473 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  2474 | `		/* Terminate the entire process */` |
|        9 |  2475 | `		exit(pVm->iExitStatus);` |
|        - |  2476 | `	}` |
|      ! 0 |  2477 | `	goto Abort;` |
|        - |  2478 | `/*` |
|        - |  2479 | ` * JMP: * P2 *` |
|        - |  2480 | ` *` |
|        - |  2481 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  2482 | ` * the one at index P2 from the beginning of the program.` |
|        - |  2483 | ` */` |
|   102699 |  2484 | `case PH7_OP_JMP:` |
|   205403 |  2485 | `	pc = pInstr->iP2 - 1;` |
|   205403 |  2486 | `	break;` |
|        - |  2487 | `/*` |
|        - |  2488 | ` * JZ: P1 P2 *` |
|        - |  2489 | ` *` |
|        - |  2490 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2491 | ` * entry in the stack if P1 is zero.` |
|        - |  2492 | ` */` |
|   214816 |  2493 | `case PH7_OP_JZ:` |
|        - |  2494 | `#ifdef UNTRUST` |
|        - |  2495 | `	if( pTos < pStack ){` |
|        - |  2496 | `		goto Abort;` |
|        - |  2497 | `	}` |
|        - |  2498 | `#endif` |
|        - |  2499 | `	/* Get a boolean value */` |
|   429640 |  2500 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       77 |  2501 | `		PH7_MemObjToBool(pTos);` |
|       38 |  2502 | `	}` |
|   429640 |  2503 | `	if( !pTos->x.iVal ){` |
|        - |  2504 | `		/* Take the jump */` |
|   213730 |  2505 | `		pc = pInstr->iP2 - 1;` |
|   106864 |  2506 | `	}` |
|   429640 |  2507 | `	if( !pInstr->iP1 ){` |
|   334265 |  2508 | `		VmPopOperand(&pTos,1);` |
|   167133 |  2509 | `	}` |
|   429640 |  2510 | `	break;` |
|        - |  2511 | `/*` |
|        - |  2512 | ` * JNZ: P1 P2 *` |
|        - |  2513 | ` *` |
|        - |  2514 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2515 | ` * entry in the stack if P1 is zero.` |
|        - |  2516 | ` */` |
|    10672 |  2517 | `case PH7_OP_JNZ:` |
|        - |  2518 | `#ifdef UNTRUST` |
|        - |  2519 | `	if( pTos < pStack ){` |
|        - |  2520 | `		goto Abort;` |
|        - |  2521 | `	}` |
|        - |  2522 | `#endif` |
|        - |  2523 | `	/* Get a boolean value */` |
|    21346 |  2524 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2525 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2526 | `	}` |
|    21346 |  2527 | `	if( pTos->x.iVal ){` |
|        - |  2528 | `		/* Take the jump */` |
|     2646 |  2529 | `		pc = pInstr->iP2 - 1;` |
|     1322 |  2530 | `	}` |
|    21346 |  2531 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2532 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2533 | `	}` |
|    21346 |  2534 | `	break;` |
|        - |  2535 | `/*` |
|        - |  2536 | ` * NOOP: * * *` |
|        - |  2537 | ` *` |
|        - |  2538 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  2539 | ` * destination.` |
|        - |  2540 | ` */` |
|      ! 0 |  2541 | `case PH7_OP_NOOP:` |
|      ! 0 |  2542 | `	break;` |
|        - |  2543 | `/*` |
|        - |  2544 | ` * POP: P1 * *` |
|        - |  2545 | ` *` |
|        - |  2546 | ` * Pop P1 elements from the operand stack.` |
|        - |  2547 | ` */` |
|   193955 |  2548 | `case PH7_OP_POP: {` |
|   387915 |  2549 | `	sxi32 n = pInstr->iP1;` |
|   387915 |  2550 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2551 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 |  2552 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 |  2553 | `	}` |
|   387915 |  2554 | `	VmPopOperand(&pTos,n);` |
|   387915 |  2555 | `	break;` |
|        - |  2556 | `				 }` |
|        - |  2557 | `/*` |
|        - |  2558 | ` * CVT_INT: * * *` |
|        - |  2559 | ` *` |
|        - |  2560 | ` * Force the top of the stack to be an integer.` |
|        - |  2561 | ` */` |
|       28 |  2562 | `case PH7_OP_CVT_INT:` |
|        - |  2563 | `#ifdef UNTRUST` |
|        - |  2564 | `	if( pTos < pStack ){` |
|        - |  2565 | `		goto Abort;` |
|        - |  2566 | `	}` |
|        - |  2567 | `#endif` |
|       57 |  2568 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|       27 |  2569 | `		PH7_MemObjToInteger(pTos);` |
|       13 |  2570 | `	}` |
|        - |  2571 | `	/* Invalidate any prior representation */` |
|       57 |  2572 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       57 |  2573 | `	break;` |
|        - |  2574 | `/*` |
|        - |  2575 | ` * CVT_REAL: * * *` |
|        - |  2576 | ` *` |
|        - |  2577 | ` * Force the top of the stack to be a real.` |
|        - |  2578 | ` */` |
|        4 |  2579 | `case PH7_OP_CVT_REAL:` |
|        - |  2580 | `#ifdef UNTRUST` |
|        - |  2581 | `	if( pTos < pStack ){` |
|        - |  2582 | `		goto Abort;` |
|        - |  2583 | `	}` |
|        - |  2584 | `#endif` |
|        9 |  2585 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  2586 | `		PH7_MemObjToReal(pTos);` |
|        2 |  2587 | `	}` |
|        - |  2588 | `	/* Invalidate any prior representation */` |
|        9 |  2589 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|        9 |  2590 | `	break;` |
|        - |  2591 | `/*` |
|        - |  2592 | ` * CVT_STR: * * *` |
|        - |  2593 | ` *` |
|        - |  2594 | ` * Force the top of the stack to be a string.` |
|        - |  2595 | ` */` |
|      136 |  2596 | `case PH7_OP_CVT_STR:` |
|        - |  2597 | `#ifdef UNTRUST` |
|        - |  2598 | `	if( pTos < pStack ){` |
|        - |  2599 | `		goto Abort;` |
|        - |  2600 | `	}` |
|        - |  2601 | `#endif` |
|      274 |  2602 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      274 |  2603 | `		PH7_MemObjToString(pTos);` |
|      136 |  2604 | `	}` |
|      274 |  2605 | `	break;` |
|        - |  2606 | `/*` |
|        - |  2607 | ` * CVT_BOOL: * * *` |
|        - |  2608 | ` *` |
|        - |  2609 | ` * Force the top of the stack to be a boolean.` |
|        - |  2610 | ` */` |
|        5 |  2611 | `case PH7_OP_CVT_BOOL:` |
|        - |  2612 | `#ifdef UNTRUST` |
|        - |  2613 | `	if( pTos < pStack ){` |
|        - |  2614 | `		goto Abort;` |
|        - |  2615 | `	}` |
|        - |  2616 | `#endif` |
|       11 |  2617 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  2618 | `		PH7_MemObjToBool(pTos);` |
|        3 |  2619 | `	}` |
|       11 |  2620 | `	break;` |
|        - |  2621 | `/*` |
|        - |  2622 | ` * CVT_NULL: * * *` |
|        - |  2623 | ` *` |
|        - |  2624 | ` * Nullify the top of the stack.` |
|        - |  2625 | ` */` |
|        3 |  2626 | `case PH7_OP_CVT_NULL:` |
|        - |  2627 | `#ifdef UNTRUST` |
|        - |  2628 | `	if( pTos < pStack ){` |
|        - |  2629 | `		goto Abort;` |
|        - |  2630 | `	}` |
|        - |  2631 | `#endif` |
|        7 |  2632 | `	PH7_MemObjRelease(pTos);` |
|        7 |  2633 | `	break;` |
|        - |  2634 | `/*` |
|        - |  2635 | ` * CVT_NUMC: * * *` |
|        - |  2636 | ` *` |
|        - |  2637 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  2638 | ` */` |
|      ! 0 |  2639 | `case PH7_OP_CVT_NUMC:` |
|        - |  2640 | `#ifdef UNTRUST` |
|        - |  2641 | `	if( pTos < pStack ){` |
|        - |  2642 | `		goto Abort;` |
|        - |  2643 | `	}` |
|        - |  2644 | `#endif` |
|        - |  2645 | `	/* Force a numeric cast */` |
|      ! 0 |  2646 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  2647 | `	break;` |
|        - |  2648 | `/*` |
|        - |  2649 | ` * CVT_ARRAY: * * *` |
|        - |  2650 | ` *` |
|        - |  2651 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  2652 | ` */` |
|       10 |  2653 | `case PH7_OP_CVT_ARRAY:` |
|        - |  2654 | `#ifdef UNTRUST` |
|        - |  2655 | `	if( pTos < pStack ){` |
|        - |  2656 | `		goto Abort;` |
|        - |  2657 | `	}` |
|        - |  2658 | `#endif` |
|        - |  2659 | `	/* Force a hashmap cast */` |
|       21 |  2660 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  2661 | `	if( rc != SXRET_OK ){` |
|        - |  2662 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  2663 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  2664 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  2665 | `	}` |
|       21 |  2666 | `	break;` |
|        - |  2667 | `/*` |
|        - |  2668 | ` * CVT_OBJ: * * *` |
|        - |  2669 | ` *` |
|        - |  2670 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  2671 | ` */` |
|        8 |  2672 | `case PH7_OP_CVT_OBJ:` |
|        - |  2673 | `#ifdef UNTRUST` |
|        - |  2674 | `	if( pTos < pStack ){` |
|        - |  2675 | `		goto Abort;` |
|        - |  2676 | `	}` |
|        - |  2677 | `#endif` |
|       17 |  2678 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  2679 | `		/* Force a 'stdClass()' cast */` |
|       17 |  2680 | `		PH7_MemObjToObject(pTos);` |
|        8 |  2681 | `	}` |
|       17 |  2682 | `	break;` |
|        - |  2683 | `/*` |
|        - |  2684 | ` * ERR_CTRL * * *` |
|        - |  2685 | ` *` |
|        - |  2686 | ` * Error control operator.` |
|        - |  2687 | ` */` |
|     6618 |  2688 | `case PH7_OP_ERR_CTRL:` |
|        - |  2689 | `	/*` |
|        - |  2690 | `	 * TICKET 1433-038:` |
|        - |  2691 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  2692 | `	 * use the public API,to control error output.` |
|        - |  2693 | `	 */` |
|    13236 |  2694 | `	break;` |
|        - |  2695 | `/*` |
|        - |  2696 | ` * IS_A * * *` |
|        - |  2697 | ` *` |
|        - |  2698 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  2699 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  2700 | ` * holding a class name or an object).` |
|        - |  2701 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  2702 | ` */` |
|       11 |  2703 | `case PH7_OP_IS_A:{` |
|       23 |  2704 | `	ph7_value *pNos = &pTos[-1];` |
|       23 |  2705 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  2706 | `#ifdef UNTRUST` |
|        - |  2707 | `	if( pNos < pStack ){` |
|        - |  2708 | `		goto Abort;` |
|        - |  2709 | `	}` |
|        - |  2710 | `#endif` |
|       23 |  2711 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       21 |  2712 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       21 |  2713 | `		ph7_class *pClass = 0;` |
|        - |  2714 | `		/* Extract the target class */` |
|       21 |  2715 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  2716 | `			/* Instance already loaded */` |
|      ! 0 |  2717 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       21 |  2718 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2719 | `			/* Perform the query */` |
|       31 |  2720 | `			pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|       20 |  2721 | `				SyBlobLength(&pTos->sBlob),FALSE,0);` |
|       10 |  2722 | `		}` |
|       21 |  2723 | `		if( pClass ){` |
|        - |  2724 | `			/* Perform the query */` |
|       21 |  2725 | `			iRes = VmInstanceOf(pThis->pClass,pClass);` |
|       10 |  2726 | `		}` |
|       10 |  2727 | `	}` |
|        - |  2728 | `	/* Push result */` |
|       23 |  2729 | `	VmPopOperand(&pTos,1);` |
|       23 |  2730 | `	PH7_MemObjRelease(pTos);` |
|       23 |  2731 | `	pTos->x.iVal = iRes;` |
|       23 |  2732 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       23 |  2733 | `	break;` |
|        - |  2734 | `				 }` |
|        - |  2735 |  |
|        - |  2736 | `/*` |
|        - |  2737 | ` * LOADC P1 P2 *` |
|        - |  2738 | ` *` |
|        - |  2739 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  2740 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  2741 | ` */` |
|   495897 |  2742 | `case PH7_OP_LOADC: {` |
|        - |  2743 | `	ph7_value *pObj;` |
|        - |  2744 | `	/* Reserve a room */` |
|   991799 |  2745 | `	pTos++;` |
|   991799 |  2746 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|   991799 |  2747 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  2748 | `			SyHashEntry *pEntry;` |
|        - |  2749 | `			/* Candidate for expansion via user defined callbacks */` |
|     9398 |  2750 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|     9398 |  2751 | `			if( pEntry ){` |
|     8482 |  2752 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  2753 | `				/* Set a NULL default value */` |
|     8482 |  2754 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|     8482 |  2755 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  2756 | `				/* Invoke the callback and deal with the expanded value */` |
|     8482 |  2757 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  2758 | `				/* Mark as constant */` |
|     8482 |  2759 | `				pTos->nIdx = SXU32_HIGH;` |
|     8482 |  2760 | `				break;` |
|        - |  2761 | `			}` |
|      458 |  2762 | `		}` |
|   983319 |  2763 | `		PH7_MemObjLoad(pObj,pTos);` |
|   491662 |  2764 | `	}else{` |
|        - |  2765 | `		/* Set a NULL value */` |
|      ! 0 |  2766 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  2767 | `	}` |
|        - |  2768 | `	/* Mark as constant */` |
|   983319 |  2769 | `	pTos->nIdx = SXU32_HIGH;` |
|   983319 |  2770 | `	break;` |
|        - |  2771 | `				  }` |
|        - |  2772 | `/*` |
|        - |  2773 | ` * LOAD: P1 * P3` |
|        - |  2774 | ` *` |
|        - |  2775 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  2776 | ` * from the P3 operand.` |
|        - |  2777 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  2778 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  2779 | ` */` |
|   511613 |  2780 | `case PH7_OP_LOAD:{` |
|        - |  2781 | `	ph7_value *pObj;` |
|        - |  2782 | `	SyString sName;` |
|  1023243 |  2783 | `	if( pInstr->p3 == 0 ){` |
|        - |  2784 | `		/* Take the variable name from the top of the stack */` |
|        - |  2785 | `#ifdef UNTRUST` |
|        - |  2786 | `		if( pTos < pStack ){` |
|        - |  2787 | `			goto Abort;` |
|        - |  2788 | `		}` |
|        - |  2789 | `#endif` |
|        - |  2790 | `		/* Force a string cast */` |
|       25 |  2791 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2792 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  2793 | `		}` |
|       25 |  2794 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       13 |  2795 | `	}else{` |
|  1023219 |  2796 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  2797 | `		/* Reserve a room for the target object */` |
|  1023219 |  2798 | `		pTos++;` |
|        - |  2799 | `	}` |
|        - |  2800 | `	/* Extract the requested memory object */` |
|  1023243 |  2801 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  1023243 |  2802 | `	if( pObj == 0 ){` |
|      456 |  2803 | `		if( pInstr->iP1 ){` |
|        - |  2804 | `			/* Variable not found,load NULL */` |
|      456 |  2805 | `			if( !pInstr->p3 ){` |
|      ! 0 |  2806 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  2807 | `			}else{` |
|      456 |  2808 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  2809 | `			}` |
|      456 |  2810 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|   511842 |  2811 | `			break;` |
|      ! 0 |  2812 | `		}else{` |
|        - |  2813 | `			/* Fatal error */` |
|      ! 0 |  2814 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  2815 | `			goto Abort;` |
|        - |  2816 | `		}` |
|        - |  2817 | `	}` |
|        - |  2818 | `	/* Load variable contents */` |
|  1022789 |  2819 | `	PH7_MemObjLoad(pObj,pTos);` |
|  1022789 |  2820 | `	pTos->nIdx = pObj->nIdx;` |
|  1022789 |  2821 | `	break;` |
|        - |  2822 | `				   }` |
|        - |  2823 | `/*` |
|        - |  2824 | ` * LOAD_MAP P1 * *` |
|        - |  2825 | ` *` |
|        - |  2826 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  2827 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  2828 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  2829 | ` */` |
|    11237 |  2830 | `case PH7_OP_LOAD_MAP: {` |
|        - |  2831 | `	ph7_hashmap *pMap;` |
|        - |  2832 | `	/* Allocate a new hashmap instance */` |
|    22476 |  2833 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    22476 |  2834 | `	if( pMap == 0 ){` |
|      ! 0 |  2835 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  2836 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  2837 | `		goto Abort;` |
|        - |  2838 | `	}` |
|    22476 |  2839 | `	if( pInstr->iP1 > 0 ){` |
|     1410 |  2840 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  2841 | `		/* Perform the insertion */` |
|     3938 |  2842 | `		while( pEntry < pTos ){` |
|     2530 |  2843 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  2844 | `				/* Insertion by reference */` |
|      142 |  2845 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  2846 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  2847 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  2848 | `					);` |
|       48 |  2849 | `			}else{` |
|        - |  2850 | `				/* Standard insertion */` |
|     3653 |  2851 | `				PH7_HashmapInsert(pMap,` |
|     2434 |  2852 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     1217 |  2853 | `					&pEntry[1]` |
|        - |  2854 | `				);` |
|        - |  2855 | `			}` |
|        - |  2856 | `			/* Next pair on the stack */` |
|     2530 |  2857 | `			pEntry += 2;` |
|        2 |  2858 | `		}` |
|        - |  2859 | `		/* Pop P1 elements */` |
|     1410 |  2860 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|      704 |  2861 | `	}` |
|        - |  2862 | `	/* Push the hashmap */` |
|    22476 |  2863 | `	pTos++;` |
|    22476 |  2864 | `	pTos->nIdx = SXU32_HIGH;` |
|    22476 |  2865 | `	pTos->x.pOther = pMap;` |
|    22476 |  2866 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    22476 |  2867 | `	break;` |
|        - |  2868 | `					  }` |
|        - |  2869 | `/*` |
|        - |  2870 | ` * LOAD_LIST: P1 * *` |
|        - |  2871 | ` *` |
|        - |  2872 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  2873 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  2874 | ` * Caveats:` |
|        - |  2875 | ` *  This implementation support only a single nesting level.` |
|        - |  2876 | ` */` |
|       17 |  2877 | `case PH7_OP_LOAD_LIST: {` |
|        - |  2878 | `	ph7_value *pEntry;` |
|       35 |  2879 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  2880 | `		/* Empty list,break immediately */` |
|      ! 0 |  2881 | `		break;` |
|        - |  2882 | `	}` |
|       35 |  2883 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  2884 | `#ifdef UNTRUST` |
|        - |  2885 | `	if( &pEntry[-1] < pStack ){` |
|        - |  2886 | `		goto Abort;` |
|        - |  2887 | `	}` |
|        - |  2888 | `#endif` |
|       35 |  2889 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       31 |  2890 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  2891 | `		ph7_hashmap_node *pNode;` |
|        - |  2892 | `		ph7_value sKey,*pObj;` |
|        - |  2893 | `		/* Start Copying */` |
|       31 |  2894 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|       99 |  2895 | `		while( pEntry <= pTos ){` |
|       69 |  2896 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|       65 |  2897 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|       65 |  2898 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       65 |  2899 | `					if( rc == SXRET_OK ){` |
|        - |  2900 | `						/* Store node value */` |
|       65 |  2901 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       33 |  2902 | `					}else{` |
|        - |  2903 | `						/* Nullify the variable */` |
|      ! 0 |  2904 | `						PH7_MemObjRelease(pObj);` |
|        - |  2905 | `					}` |
|       32 |  2906 | `				}` |
|       32 |  2907 | `			}` |
|       69 |  2908 | `			sKey.x.iVal++; /* Next numeric index */` |
|       69 |  2909 | `			pEntry++;` |
|        1 |  2910 | `		}` |
|       15 |  2911 | `	}` |
|       35 |  2912 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       35 |  2913 | `	break;` |
|        - |  2914 | `					   }` |
|        - |  2915 | `/*` |
|        - |  2916 | ` * LOAD_IDX: P1 P2 *` |
|        - |  2917 | ` *` |
|        - |  2918 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  2919 | ` * from the stack.` |
|        - |  2920 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  2921 | ` * instead.` |
|        - |  2922 | ` */` |
|    48715 |  2923 | `case PH7_OP_LOAD_IDX: {` |
|    97435 |  2924 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|    97435 |  2925 | `	ph7_hashmap *pMap = 0;` |
|        - |  2926 | `	ph7_value *pIdx;` |
|    97435 |  2927 | `	pIdx = 0;` |
|    97435 |  2928 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  2929 | `		if( !pInstr->iP2){` |
|        - |  2930 | `			/* No available index,load NULL */` |
|      ! 0 |  2931 | `			if( pTos >= pStack ){` |
|      ! 0 |  2932 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  2933 | `			}else{` |
|        - |  2934 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  2935 | `				pTos++;` |
|      ! 0 |  2936 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  2937 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  2938 | `			}` |
|        - |  2939 | `			/* Emit a notice */` |
|      ! 0 |  2940 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  2941 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  2942 | `			break;` |
|        - |  2943 | `		}` |
|      ! 0 |  2944 | `	}else{` |
|    97435 |  2945 | `		pIdx = pTos;` |
|    97435 |  2946 | `		pTos--;` |
|        - |  2947 | `	}` |
|    97435 |  2948 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  2949 | `		/* String access */` |
|    41240 |  2950 | `		if( pIdx ){` |
|        - |  2951 | `			sxu32 nOfft;` |
|    41240 |  2952 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  2953 | `				/* Force an int cast */` |
|      ! 0 |  2954 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  2955 | `			}` |
|    41240 |  2956 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|    41240 |  2957 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  2958 | `				/* Invalid offset,load null */` |
|      ! 0 |  2959 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  2960 | `			}else{` |
|    41240 |  2961 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|    41240 |  2962 | `				int c = zData[nOfft];` |
|    41240 |  2963 | `				PH7_MemObjRelease(pTos);` |
|    41240 |  2964 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|    41240 |  2965 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  2966 | `			}` |
|    20622 |  2967 | `		}else{` |
|        - |  2968 | `			/* No available index,load NULL */` |
|      ! 0 |  2969 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  2970 | `		}` |
|    41240 |  2971 | `		break;` |
|        - |  2972 | `	}` |
|    56196 |  2973 | `	if( pInstr->iP2 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  2974 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  2975 | `			ph7_value *pObj;` |
|      ! 0 |  2976 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      ! 0 |  2977 | `				PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  2978 | `				PH7_MemObjLoad(pObj,pTos);` |
|      ! 0 |  2979 | `			}` |
|      ! 0 |  2980 | `		}` |
|      ! 0 |  2981 | `	}` |
|    56196 |  2982 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    56196 |  2983 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  2984 | `		/* Point to the hashmap */` |
|    56196 |  2985 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    56196 |  2986 | `		if( pIdx ){` |
|        - |  2987 | `			/* Load the desired entry */` |
|    56196 |  2988 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    28097 |  2989 | `		}` |
|    56196 |  2990 | `		if( rc != SXRET_OK && pInstr->iP2 ){` |
|        - |  2991 | `			/* Create a new empty entry */` |
|      ! 0 |  2992 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      ! 0 |  2993 | `			if( rc == SXRET_OK ){` |
|        - |  2994 | `				/* Point to the last inserted entry */` |
|      ! 0 |  2995 | `				pNode = pMap->pLast;` |
|      ! 0 |  2996 | `			}` |
|      ! 0 |  2997 | `		}` |
|    28097 |  2998 | `	}` |
|    56196 |  2999 | `	if( pIdx ){` |
|    56196 |  3000 | `		PH7_MemObjRelease(pIdx);` |
|    28097 |  3001 | `	}` |
|    56196 |  3002 | `	if( rc == SXRET_OK ){` |
|        - |  3003 | `		/* Load entry contents */` |
|    27294 |  3004 | `		if( pMap->iRef < 2 ){` |
|        - |  3005 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3006 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3007 | `			 */` |
|      ! 0 |  3008 | `			pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  3009 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|      ! 0 |  3010 | `		}else{` |
|    27294 |  3011 | `			pTos->nIdx = pNode->nValIdx;` |
|    27294 |  3012 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    27294 |  3013 | `			PH7_HashmapUnref(pMap);` |
|        - |  3014 | `		}` |
|    13648 |  3015 | `	}else{` |
|        - |  3016 | `		/* No such entry,load NULL */` |
|    28903 |  3017 | `		PH7_MemObjRelease(pTos);` |
|    28903 |  3018 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3019 | `	}` |
|    56196 |  3020 | `	break;` |
|        - |  3021 | `					  }` |
|        - |  3022 | `/*` |
|        - |  3023 | ` * LOAD_CLOSURE * * P3` |
|        - |  3024 | ` *` |
|        - |  3025 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  3026 | ` * name in the stack.` |
|        - |  3027 | ` */` |
|        2 |  3028 | `case PH7_OP_LOAD_CLOSURE:{` |
|        5 |  3029 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|        5 |  3030 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  3031 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  3032 | `		ph7_vm_func *pClosure;` |
|        - |  3033 | `		char *zName;` |
|        - |  3034 | `		sxu32 mLen;` |
|        - |  3035 | `		sxu32 n;` |
|        - |  3036 | `		/* Create a new VM function */` |
|        5 |  3037 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  3038 | `		/* Generate an unique closure name */` |
|        5 |  3039 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|        5 |  3040 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  3041 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  3042 | `			goto Abort;` |
|        - |  3043 | `		}` |
|        5 |  3044 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|        5 |  3045 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  3046 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  3047 | `		}` |
|        - |  3048 | `		/* Zero the stucture */` |
|        5 |  3049 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  3050 | `		/* Perform a structure assignment on read-only items */` |
|        5 |  3051 | `		pClosure->aArgs = pFunc->aArgs;` |
|        5 |  3052 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|        5 |  3053 | `		pClosure->aStatic = pFunc->aStatic;` |
|        5 |  3054 | `		pClosure->iFlags = pFunc->iFlags;` |
|        5 |  3055 | `		pClosure->pUserData = pFunc->pUserData;` |
|        5 |  3056 | `		pClosure->sSignature = pFunc->sSignature;` |
|        5 |  3057 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  3058 | `		/* Register the closure */` |
|        5 |  3059 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  3060 | `		/* Set up closure environment */` |
|        5 |  3061 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        5 |  3062 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       13 |  3063 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  3064 | `			ph7_value *pValue;` |
|        9 |  3065 | `			pEnv = &aEnv[n];` |
|        9 |  3066 | `			sEnv.sName  = pEnv->sName;` |
|        9 |  3067 | `			sEnv.iFlags = pEnv->iFlags;` |
|        9 |  3068 | `			sEnv.nIdx = SXU32_HIGH;` |
|        9 |  3069 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|        9 |  3070 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  3071 | `				/* Pass by reference */` |
|      ! 0 |  3072 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  3073 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  3074 | `					);` |
|      ! 0 |  3075 | `			}` |
|        - |  3076 | `			/* Standard pass by value */` |
|        9 |  3077 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|        9 |  3078 | `			if( pValue ){` |
|        - |  3079 | `				/* Copy imported value */` |
|        5 |  3080 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|        2 |  3081 | `			}` |
|        - |  3082 | `			/* Insert the imported variable */` |
|        9 |  3083 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|        5 |  3084 | `		}` |
|        - |  3085 | `		/* Finally,load the closure name on the stack */` |
|        5 |  3086 | `		pTos++;` |
|        5 |  3087 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|        2 |  3088 | `	}` |
|        5 |  3089 | `	break;` |
|        - |  3090 | `						 }` |
|        - |  3091 | `/*` |
|        - |  3092 | ` * STORE * P2 P3` |
|        - |  3093 | ` *` |
|        - |  3094 | ` * Perform a store (Assignment) operation.` |
|        - |  3095 | ` */` |
|    64431 |  3096 | `case PH7_OP_STORE: {` |
|        - |  3097 | `	ph7_value *pObj;` |
|        - |  3098 | `	SyString sName;` |
|        - |  3099 | `#ifdef UNTRUST` |
|        - |  3100 | `	if( pTos < pStack ){` |
|        - |  3101 | `		goto Abort;` |
|        - |  3102 | `	}` |
|        - |  3103 | `#endif` |
|   128864 |  3104 | `	if( pInstr->iP2 ){` |
|        - |  3105 | `		sxu32 nIdx;` |
|        - |  3106 | `		/* Member store operation */` |
|      468 |  3107 | `		nIdx = pTos->nIdx;` |
|      468 |  3108 | `		VmPopOperand(&pTos,1);` |
|      468 |  3109 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  3110 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  3111 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  3112 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  3113 | `		}else{` |
|        - |  3114 | `			/* Point to the desired memory object */` |
|      464 |  3115 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      464 |  3116 | `			if( pObj ){` |
|        - |  3117 | `				/* Perform the store operation */` |
|      464 |  3118 | `				PH7_MemObjStore(pTos,pObj);` |
|      231 |  3119 | `			}` |
|        - |  3120 | `		}` |
|    64666 |  3121 | `		break;` |
|   128398 |  3122 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  3123 | `		/* Take the variable name from the next on the stack */` |
|        7 |  3124 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  3125 | `			/* Force a string cast */` |
|      ! 0 |  3126 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3127 | `		}` |
|        7 |  3128 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  3129 | `		pTos--;` |
|        - |  3130 | `#ifdef UNTRUST` |
|        - |  3131 | `		if( pTos < pStack  ){` |
|        - |  3132 | `			goto Abort;` |
|        - |  3133 | `		}` |
|        - |  3134 | `#endif` |
|        4 |  3135 | `	}else{` |
|   128392 |  3136 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3137 | `	}` |
|        - |  3138 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   128398 |  3139 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   128398 |  3140 | `	if( pObj == 0 ){` |
|      ! 0 |  3141 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3142 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3143 | `		goto Abort;` |
|        - |  3144 | `	}` |
|   128398 |  3145 | `	if( !pInstr->p3 ){` |
|        7 |  3146 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3147 | `	}` |
|        - |  3148 | `	/* Perform the store operation */` |
|   128398 |  3149 | `	PH7_MemObjStore(pTos,pObj);` |
|   128398 |  3150 | `	break;` |
|        - |  3151 | `				   }` |
|        - |  3152 | `/*` |
|        - |  3153 | ` * STORE_IDX:   P1 * P3` |
|        - |  3154 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3155 | ` *` |
|        - |  3156 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3157 | ` */` |
|    62490 |  3158 | `case PH7_OP_STORE_IDX:` |
|        - |  3159 | `case PH7_OP_STORE_IDX_REF: {` |
|   124981 |  3160 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3161 | `	ph7_value *pKey;` |
|        - |  3162 | `	sxu32 nIdx;` |
|   124981 |  3163 | `	if( pInstr->iP1 ){` |
|        - |  3164 | `		/* Key is next on stack */` |
|    48149 |  3165 | `		pKey = pTos;` |
|    48149 |  3166 | `		pTos--;` |
|    24075 |  3167 | `	}else{` |
|    76833 |  3168 | `		pKey = 0;` |
|        - |  3169 | `	}` |
|   124981 |  3170 | `	nIdx = pTos->nIdx;` |
|   124981 |  3171 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3172 | `		/* Hashmap already loaded */` |
|   124929 |  3173 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   124929 |  3174 | `		if( pMap->iRef < 2 ){` |
|        - |  3175 | `			/* TICKET 1433-48: Prevent garbage collection */` |
|      ! 0 |  3176 | `			pMap->iRef = 2;` |
|      ! 0 |  3177 | `		}` |
|    62465 |  3178 | `	}else{` |
|        - |  3179 | `		ph7_value *pObj;` |
|       53 |  3180 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  3181 | `		if( pObj == 0 ){` |
|      ! 0 |  3182 | `			if( pKey ){` |
|      ! 0 |  3183 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  3184 | `			}` |
|      ! 0 |  3185 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  3186 | `			break;` |
|        - |  3187 | `		}` |
|        - |  3188 | `		/* Phase#1: Load the array */` |
|       53 |  3189 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  3190 | `			VmPopOperand(&pTos,1);` |
|       53 |  3191 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  3192 | `				/* Force a string cast */` |
|      ! 0 |  3193 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  3194 | `			}` |
|       53 |  3195 | `			if( pKey == 0 ){` |
|        - |  3196 | `				/* Append string */` |
|        3 |  3197 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  3198 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  3199 | `				}` |
|        2 |  3200 | `			}else{` |
|        - |  3201 | `				sxu32 nOfft;` |
|       51 |  3202 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  3203 | `					/* Force an int cast */` |
|       51 |  3204 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  3205 | `				}` |
|       51 |  3206 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  3207 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  3208 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  3209 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  3210 | `					zData[nOfft] = zBlob[0];` |
|       26 |  3211 | `				}else{` |
|      ! 0 |  3212 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  3213 | `						/* Perform an append operation */` |
|      ! 0 |  3214 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  3215 | `					}` |
|        - |  3216 | `				}` |
|        - |  3217 | `			}` |
|       53 |  3218 | `			if( pKey ){` |
|       51 |  3219 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  3220 | `			}` |
|       53 |  3221 | `			break;` |
|      ! 0 |  3222 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  3223 | `			/* Force a hashmap cast  */` |
|      ! 0 |  3224 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3225 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3226 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  3227 | `				goto Abort;` |
|        - |  3228 | `			}` |
|      ! 0 |  3229 | `		}` |
|      ! 0 |  3230 | `		pMap = (ph7_hashmap *)pObj->x.pOther;` |
|        - |  3231 | `	}` |
|   124929 |  3232 | `	VmPopOperand(&pTos,1);` |
|        - |  3233 | `	/* Phase#2: Perform the insertion */` |
|   124929 |  3234 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3235 | `		/* Insertion by reference */` |
|       13 |  3236 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        7 |  3237 | `	}else{` |
|   124917 |  3238 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3239 | `	}` |
|   124929 |  3240 | `	if( pKey ){` |
|    48099 |  3241 | `		PH7_MemObjRelease(pKey);` |
|    24049 |  3242 | `	}` |
|   124929 |  3243 | `	break;` |
|        - |  3244 | `					   }` |
|        - |  3245 | `/*` |
|        - |  3246 | ` * INCR: P1 * *` |
|        - |  3247 | ` *` |
|        - |  3248 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3249 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3250 | ` * the stack and increment after that.` |
|        - |  3251 | ` */` |
|    42479 |  3252 | `case PH7_OP_INCR:` |
|        - |  3253 | `#ifdef UNTRUST` |
|        - |  3254 | `	if( pTos < pStack ){` |
|        - |  3255 | `		goto Abort;` |
|        - |  3256 | `	}` |
|        - |  3257 | `#endif` |
|    84963 |  3258 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|    84963 |  3259 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3260 | `			ph7_value *pObj;` |
|    84963 |  3261 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3262 | `				/* Force a numeric cast */` |
|    84963 |  3263 | `				PH7_MemObjToNumeric(pObj);` |
|    84963 |  3264 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3265 | `					pObj->rVal++;` |
|        - |  3266 | `					/* Try to get an integer representation */` |
|      ! 0 |  3267 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3268 | `				}else{` |
|    84963 |  3269 | `					pObj->x.iVal++;` |
|    84963 |  3270 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3271 | `				}` |
|    84963 |  3272 | `				if( pInstr->iP1 ){` |
|        - |  3273 | `					/* Pre-icrement */` |
|       55 |  3274 | `					PH7_MemObjStore(pObj,pTos);` |
|       27 |  3275 | `				}` |
|    42482 |  3276 | `			}` |
|    42484 |  3277 | `		}else{` |
|      ! 0 |  3278 | `			if( pInstr->iP1 ){` |
|        - |  3279 | `				/* Force a numeric cast */` |
|      ! 0 |  3280 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  3281 | `				/* Pre-increment */` |
|      ! 0 |  3282 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3283 | `					pTos->rVal++;` |
|        - |  3284 | `					/* Try to get an integer representation */` |
|      ! 0 |  3285 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3286 | `				}else{` |
|      ! 0 |  3287 | `					pTos->x.iVal++;` |
|      ! 0 |  3288 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3289 | `				}` |
|      ! 0 |  3290 | `			}` |
|        - |  3291 | `		}` |
|    42482 |  3292 | `	}` |
|    84963 |  3293 | `	break;` |
|        - |  3294 | `/*` |
|        - |  3295 | ` * DECR: P1 * *` |
|        - |  3296 | ` *` |
|        - |  3297 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  3298 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  3299 | ` * and decrement after that.` |
|        - |  3300 | ` */` |
|        2 |  3301 | `case PH7_OP_DECR:` |
|        - |  3302 | `#ifdef UNTRUST` |
|        - |  3303 | `	if( pTos < pStack ){` |
|        - |  3304 | `		goto Abort;` |
|        - |  3305 | `	}` |
|        - |  3306 | `#endif` |
|        5 |  3307 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  3308 | `		/* Force a numeric cast */` |
|        5 |  3309 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  3310 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3311 | `			ph7_value *pObj;` |
|        5 |  3312 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3313 | `				/* Force a numeric cast */` |
|        5 |  3314 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  3315 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3316 | `					pObj->rVal--;` |
|        - |  3317 | `					/* Try to get an integer representation */` |
|      ! 0 |  3318 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3319 | `				}else{` |
|        5 |  3320 | `					pObj->x.iVal--;` |
|        5 |  3321 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3322 | `				}` |
|        5 |  3323 | `				if( pInstr->iP1 ){` |
|        - |  3324 | `					/* Pre-icrement */` |
|      ! 0 |  3325 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  3326 | `				}` |
|        2 |  3327 | `			}` |
|        3 |  3328 | `		}else{` |
|      ! 0 |  3329 | `			if( pInstr->iP1 ){` |
|        - |  3330 | `				/* Pre-increment */` |
|      ! 0 |  3331 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3332 | `					pTos->rVal--;` |
|        - |  3333 | `					/* Try to get an integer representation */` |
|      ! 0 |  3334 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3335 | `				}else{` |
|      ! 0 |  3336 | `					pTos->x.iVal--;` |
|      ! 0 |  3337 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3338 | `				}` |
|      ! 0 |  3339 | `			}` |
|        - |  3340 | `		}` |
|        2 |  3341 | `	}` |
|        5 |  3342 | `	break;` |
|        - |  3343 | `/*` |
|        - |  3344 | ` * UMINUS: * * *` |
|        - |  3345 | ` *` |
|        - |  3346 | ` * Perform a unary minus operation.` |
|        - |  3347 | ` */` |
|    14467 |  3348 | `case PH7_OP_UMINUS:` |
|        - |  3349 | `#ifdef UNTRUST` |
|        - |  3350 | `	if( pTos < pStack ){` |
|        - |  3351 | `		goto Abort;` |
|        - |  3352 | `	}` |
|        - |  3353 | `#endif` |
|        - |  3354 | `	/* Force a numeric (integer,real or both) cast */` |
|    28935 |  3355 | `	PH7_MemObjToNumeric(pTos);` |
|    28935 |  3356 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       17 |  3357 | `		pTos->rVal = -pTos->rVal;` |
|        8 |  3358 | `	}` |
|    28935 |  3359 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    28919 |  3360 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    14459 |  3361 | `	}` |
|    28935 |  3362 | `	break;` |
|        - |  3363 | `/*` |
|        - |  3364 | ` * UPLUS: * * *` |
|        - |  3365 | ` *` |
|        - |  3366 | ` * Perform a unary plus operation.` |
|        - |  3367 | ` */` |
|       16 |  3368 | `case PH7_OP_UPLUS:` |
|        - |  3369 | `#ifdef UNTRUST` |
|        - |  3370 | `	if( pTos < pStack ){` |
|        - |  3371 | `		goto Abort;` |
|        - |  3372 | `	}` |
|        - |  3373 | `#endif` |
|        - |  3374 | `	/* Force a numeric (integer,real or both) cast */` |
|       33 |  3375 | `	PH7_MemObjToNumeric(pTos);` |
|       33 |  3376 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3377 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  3378 | `	}` |
|       33 |  3379 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       33 |  3380 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       16 |  3381 | `	}` |
|       33 |  3382 | `	break;` |
|        - |  3383 | `/*` |
|        - |  3384 | ` * OP_LNOT: * * *` |
|        - |  3385 | ` *` |
|        - |  3386 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  3387 | ` * with its complement.` |
|        - |  3388 | ` */` |
|     7138 |  3389 | `case PH7_OP_LNOT:` |
|        - |  3390 | `#ifdef UNTRUST` |
|        - |  3391 | `	if( pTos < pStack ){` |
|        - |  3392 | `		goto Abort;` |
|        - |  3393 | `	}` |
|        - |  3394 | `#endif` |
|        - |  3395 | `	/* Force a boolean cast */` |
|    14281 |  3396 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  3397 | `		PH7_MemObjToBool(pTos);` |
|       10 |  3398 | `	}` |
|    14281 |  3399 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    14281 |  3400 | `	break;` |
|        - |  3401 | `/*` |
|        - |  3402 | ` * OP_BITNOT: * * *` |
|        - |  3403 | ` *` |
|        - |  3404 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  3405 | ` * with its ones-complement.` |
|        - |  3406 | ` */` |
|        3 |  3407 | `case PH7_OP_BITNOT:` |
|        - |  3408 | `#ifdef UNTRUST` |
|        - |  3409 | `	if( pTos < pStack ){` |
|        - |  3410 | `		goto Abort;` |
|        - |  3411 | `	}` |
|        - |  3412 | `#endif` |
|        - |  3413 | `	/* Force an integer cast */` |
|        7 |  3414 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3415 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3416 | `	}` |
|        7 |  3417 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|        7 |  3418 | `	break;` |
|        - |  3419 | `/* OP_MUL * * *` |
|        - |  3420 | ` * OP_MUL_STORE * * *` |
|        - |  3421 | ` *` |
|        - |  3422 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  3423 | ` * and push the result back onto the stack.` |
|        - |  3424 | ` */` |
|     1231 |  3425 | `case PH7_OP_MUL:` |
|        - |  3426 | `case PH7_OP_MUL_STORE: {` |
|     2464 |  3427 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3428 | `	/* Force the operand to be numeric */` |
|        - |  3429 | `#ifdef UNTRUST` |
|        - |  3430 | `	if( pNos < pStack ){` |
|        - |  3431 | `		goto Abort;` |
|        - |  3432 | `	}` |
|        - |  3433 | `#endif` |
|     2464 |  3434 | `	PH7_MemObjToNumeric(pTos);` |
|     2464 |  3435 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  3436 | `	/* Perform the requested operation */` |
|     2464 |  3437 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3438 | `		/* Floating point arithemic */` |
|        - |  3439 | `		ph7_real a,b,r;` |
|       17 |  3440 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3441 | `			PH7_MemObjToReal(pTos);` |
|        3 |  3442 | `		}` |
|       17 |  3443 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3444 | `			PH7_MemObjToReal(pNos);` |
|        3 |  3445 | `		}` |
|       17 |  3446 | `		a = pNos->rVal;` |
|       17 |  3447 | `		b = pTos->rVal;` |
|       17 |  3448 | `		r = a * b;` |
|        - |  3449 | `		/* Push the result */` |
|       17 |  3450 | `		pNos->rVal = r;` |
|       17 |  3451 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3452 | `		/* Try to get an integer representation */` |
|       17 |  3453 | `		PH7_MemObjTryInteger(pNos);` |
|        9 |  3454 | `	}else{` |
|        - |  3455 | `		/* Integer arithmetic */` |
|        - |  3456 | `		sxi64 a,b,r;` |
|     2448 |  3457 | `		a = pNos->x.iVal;` |
|     2448 |  3458 | `		b = pTos->x.iVal;` |
|     2448 |  3459 | `		r = a * b;` |
|        - |  3460 | `		/* Push the result */` |
|     2448 |  3461 | `		pNos->x.iVal = r;` |
|     2448 |  3462 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3463 | `	}` |
|     2464 |  3464 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  3465 | `		ph7_value *pObj;` |
|       19 |  3466 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3467 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  3468 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  3469 | `			PH7_MemObjStore(pNos,pObj);` |
|        9 |  3470 | `		}` |
|        9 |  3471 | `	}` |
|     2464 |  3472 | `	VmPopOperand(&pTos,1);` |
|     2464 |  3473 | `	break;` |
|        - |  3474 | `				 }` |
|        - |  3475 | `/* OP_ADD * * *` |
|        - |  3476 | ` *` |
|        - |  3477 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3478 | ` * and push the result back onto the stack.` |
|        - |  3479 | ` */` |
|      418 |  3480 | `case PH7_OP_ADD:{` |
|      838 |  3481 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3482 | `#ifdef UNTRUST` |
|        - |  3483 | `	if( pNos < pStack ){` |
|        - |  3484 | `		goto Abort;` |
|        - |  3485 | `	}` |
|        - |  3486 | `#endif` |
|        - |  3487 | `	/* Perform the addition */` |
|      838 |  3488 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      838 |  3489 | `	VmPopOperand(&pTos,1);` |
|      838 |  3490 | `	break;` |
|        - |  3491 | `				}` |
|        - |  3492 | `/*` |
|        - |  3493 | ` * OP_ADD_STORE * * *` |
|        - |  3494 | ` *` |
|        - |  3495 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3496 | ` * and push the result back onto the stack.` |
|        - |  3497 | ` */` |
|      481 |  3498 | `case PH7_OP_ADD_STORE:{` |
|      963 |  3499 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3500 | `	ph7_value *pObj;` |
|        - |  3501 | `	sxu32 nIdx;` |
|        - |  3502 | `#ifdef UNTRUST` |
|        - |  3503 | `	if( pNos < pStack ){` |
|        - |  3504 | `		goto Abort;` |
|        - |  3505 | `	}` |
|        - |  3506 | `#endif` |
|        - |  3507 | `	/* Perform the addition */` |
|      963 |  3508 | `	nIdx = pTos->nIdx;` |
|      963 |  3509 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  3510 | `	/* Peform the store operation */` |
|      963 |  3511 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  3512 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      963 |  3513 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      963 |  3514 | `		PH7_MemObjStore(pTos,pObj);` |
|      481 |  3515 | `	}` |
|        - |  3516 | `	/* Ticket 1433-35: Perform a stack dup */` |
|      963 |  3517 | `	PH7_MemObjStore(pTos,pNos);` |
|      963 |  3518 | `	VmPopOperand(&pTos,1);` |
|      963 |  3519 | `	break;` |
|        - |  3520 | `				}` |
|        - |  3521 | `/* OP_SUB * * *` |
|        - |  3522 | ` *` |
|        - |  3523 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3524 | ` * first (what was next on the stack) from the second (the` |
|        - |  3525 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3526 | ` */` |
|      280 |  3527 | `case PH7_OP_SUB: {` |
|      561 |  3528 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3529 | `#ifdef UNTRUST` |
|        - |  3530 | `	if( pNos < pStack ){` |
|        - |  3531 | `		goto Abort;` |
|        - |  3532 | `	}` |
|        - |  3533 | `#endif` |
|      561 |  3534 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3535 | `		/* Floating point arithemic */` |
|        - |  3536 | `		ph7_real a,b,r;` |
|       73 |  3537 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3538 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3539 | `		}` |
|       73 |  3540 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  3541 | `			PH7_MemObjToReal(pNos);` |
|        2 |  3542 | `		}` |
|       73 |  3543 | `		a = pNos->rVal;` |
|       73 |  3544 | `		b = pTos->rVal;` |
|       73 |  3545 | `		r = a - b;` |
|        - |  3546 | `		/* Push the result */` |
|       73 |  3547 | `		pNos->rVal = r;` |
|       73 |  3548 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3549 | `		/* Try to get an integer representation */` |
|       73 |  3550 | `		PH7_MemObjTryInteger(pNos);` |
|       37 |  3551 | `	}else{` |
|        - |  3552 | `		/* Integer arithmetic */` |
|        - |  3553 | `		sxi64 a,b,r;` |
|      489 |  3554 | `		a = pNos->x.iVal;` |
|      489 |  3555 | `		b = pTos->x.iVal;` |
|      489 |  3556 | `		r = a - b;` |
|        - |  3557 | `		/* Push the result */` |
|      489 |  3558 | `		pNos->x.iVal = r;` |
|      489 |  3559 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3560 | `	}` |
|      561 |  3561 | `	VmPopOperand(&pTos,1);` |
|      561 |  3562 | `	break;` |
|        - |  3563 | `				 }` |
|        - |  3564 | `/* OP_SUB_STORE * * *` |
|        - |  3565 | ` *` |
|        - |  3566 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3567 | ` * first (what was next on the stack) from the second (the` |
|        - |  3568 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3569 | ` */` |
|        1 |  3570 | `case PH7_OP_SUB_STORE: {` |
|        3 |  3571 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3572 | `	ph7_value *pObj;` |
|        - |  3573 | `#ifdef UNTRUST` |
|        - |  3574 | `	if( pNos < pStack ){` |
|        - |  3575 | `		goto Abort;` |
|        - |  3576 | `	}` |
|        - |  3577 | `#endif` |
|        3 |  3578 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3579 | `		/* Floating point arithemic */` |
|        - |  3580 | `		ph7_real a,b,r;` |
|      ! 0 |  3581 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3582 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3583 | `		}` |
|      ! 0 |  3584 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3585 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  3586 | `		}` |
|      ! 0 |  3587 | `		a = pTos->rVal;` |
|      ! 0 |  3588 | `		b = pNos->rVal;` |
|      ! 0 |  3589 | `		r = a - b;` |
|        - |  3590 | `		/* Push the result */` |
|      ! 0 |  3591 | `		pNos->rVal = r;` |
|      ! 0 |  3592 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3593 | `		/* Try to get an integer representation */` |
|      ! 0 |  3594 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  3595 | `	}else{` |
|        - |  3596 | `		/* Integer arithmetic */` |
|        - |  3597 | `		sxi64 a,b,r;` |
|        3 |  3598 | `		a = pTos->x.iVal;` |
|        3 |  3599 | `		b = pNos->x.iVal;` |
|        3 |  3600 | `		r = a - b;` |
|        - |  3601 | `		/* Push the result */` |
|        3 |  3602 | `		pNos->x.iVal = r;` |
|        3 |  3603 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3604 | `	}` |
|        3 |  3605 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3606 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3607 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3608 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3609 | `	}` |
|        3 |  3610 | `	VmPopOperand(&pTos,1);` |
|        3 |  3611 | `	break;` |
|        - |  3612 | `				 }` |
|        - |  3613 |  |
|        - |  3614 | `/*` |
|        - |  3615 | ` * OP_MOD * * *` |
|        - |  3616 | ` *` |
|        - |  3617 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3618 | ` * first (what was next on the stack) from the second (the` |
|        - |  3619 | ` * top of the stack) and push the remainder after division` |
|        - |  3620 | ` * onto the stack.` |
|        - |  3621 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3622 | ` */` |
|      296 |  3623 | `case PH7_OP_MOD:{` |
|      594 |  3624 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3625 | `	sxi64 a,b,r;` |
|        - |  3626 | `#ifdef UNTRUST` |
|        - |  3627 | `	if( pNos < pStack ){` |
|        - |  3628 | `		goto Abort;` |
|        - |  3629 | `	}` |
|        - |  3630 | `#endif` |
|        - |  3631 | `	/* Force the operands to be integer */` |
|      594 |  3632 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3633 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3634 | `	}` |
|      594 |  3635 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  3636 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  3637 | `	}` |
|        - |  3638 | `	/* Perform the requested operation */` |
|      594 |  3639 | `	a = pNos->x.iVal;` |
|      594 |  3640 | `	b = pTos->x.iVal;` |
|      594 |  3641 | `	if( b == 0 ){` |
|        3 |  3642 | `		r = 0;` |
|        3 |  3643 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3644 | `		/* goto Abort; */` |
|        2 |  3645 | `	}else{` |
|      591 |  3646 | `		r = a%b;` |
|        - |  3647 | `	}` |
|        - |  3648 | `	/* Push the result */` |
|      594 |  3649 | `	pNos->x.iVal = r;` |
|      594 |  3650 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      594 |  3651 | `	VmPopOperand(&pTos,1);` |
|      594 |  3652 | `	break;` |
|        - |  3653 | `				}` |
|        - |  3654 | `/*` |
|        - |  3655 | ` * OP_MOD_STORE * * *` |
|        - |  3656 | ` *` |
|        - |  3657 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3658 | ` * first (what was next on the stack) from the second (the` |
|        - |  3659 | ` * top of the stack) and push the remainder after division` |
|        - |  3660 | ` * onto the stack.` |
|        - |  3661 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3662 | ` */` |
|        1 |  3663 | `case PH7_OP_MOD_STORE: {` |
|        3 |  3664 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3665 | `	ph7_value *pObj;` |
|        - |  3666 | `	sxi64 a,b,r;` |
|        - |  3667 | `#ifdef UNTRUST` |
|        - |  3668 | `	if( pNos < pStack ){` |
|        - |  3669 | `		goto Abort;` |
|        - |  3670 | `	}` |
|        - |  3671 | `#endif` |
|        - |  3672 | `	/* Force the operands to be integer */` |
|        3 |  3673 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3674 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3675 | `	}` |
|        3 |  3676 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3677 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  3678 | `	}` |
|        - |  3679 | `	/* Perform the requested operation */` |
|        3 |  3680 | `	a = pTos->x.iVal;` |
|        3 |  3681 | `	b = pNos->x.iVal;` |
|        3 |  3682 | `	if( b == 0 ){` |
|      ! 0 |  3683 | `		r = 0;` |
|      ! 0 |  3684 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3685 | `		/* goto Abort; */` |
|      ! 0 |  3686 | `	}else{` |
|        3 |  3687 | `		r = a%b;` |
|        - |  3688 | `	}` |
|        - |  3689 | `	/* Push the result */` |
|        3 |  3690 | `	pNos->x.iVal = r;` |
|        3 |  3691 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  3692 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3693 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3694 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3695 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3696 | `	}` |
|        3 |  3697 | `	VmPopOperand(&pTos,1);` |
|        3 |  3698 | `	break;` |
|        - |  3699 | `				}` |
|        - |  3700 | `/*` |
|        - |  3701 | ` * OP_DIV * * *` |
|        - |  3702 | ` *` |
|        - |  3703 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3704 | ` * first (what was next on the stack) from the second (the` |
|        - |  3705 | ` * top of the stack) and push the result onto the stack.` |
|        - |  3706 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  3707 | ` */` |
|       28 |  3708 | `case PH7_OP_DIV:{` |
|       58 |  3709 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3710 | `	ph7_real a,b,r;` |
|        - |  3711 | `#ifdef UNTRUST` |
|        - |  3712 | `	if( pNos < pStack ){` |
|        - |  3713 | `		goto Abort;` |
|        - |  3714 | `	}` |
|        - |  3715 | `#endif` |
|        - |  3716 | `	/* Force the operands to be real */` |
|       58 |  3717 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       54 |  3718 | `		PH7_MemObjToReal(pTos);` |
|       26 |  3719 | `	}` |
|       58 |  3720 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       20 |  3721 | `		PH7_MemObjToReal(pNos);` |
|        9 |  3722 | `	}` |
|        - |  3723 | `	/* Perform the requested operation */` |
|       58 |  3724 | `	a = pNos->rVal;` |
|       58 |  3725 | `	b = pTos->rVal;` |
|       58 |  3726 | `	if( b == 0 ){` |
|        - |  3727 | `		/* Division by zero */` |
|        3 |  3728 | `		r = 0;` |
|        3 |  3729 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  3730 | `		/* goto Abort; */` |
|        2 |  3731 | `	}else{` |
|       55 |  3732 | `		r = a/b;` |
|        - |  3733 | `		/* Push the result */` |
|       55 |  3734 | `		pNos->rVal = r;` |
|       55 |  3735 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3736 | `		/* Try to get an integer representation */` |
|       55 |  3737 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  3738 | `	}` |
|       58 |  3739 | `	VmPopOperand(&pTos,1);` |
|       58 |  3740 | `	break;` |
|        - |  3741 | `				}` |
|        - |  3742 | `/*` |
|        - |  3743 | ` * OP_DIV_STORE * * *` |
|        - |  3744 | ` *` |
|        - |  3745 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3746 | ` * first (what was next on the stack) from the second (the` |
|        - |  3747 | ` * top of the stack) and push the result onto the stack.` |
|        - |  3748 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  3749 | ` */` |
|        1 |  3750 | `case PH7_OP_DIV_STORE:{` |
|        3 |  3751 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3752 | `	ph7_value *pObj;` |
|        - |  3753 | `	ph7_real a,b,r;` |
|        - |  3754 | `#ifdef UNTRUST` |
|        - |  3755 | `	if( pNos < pStack ){` |
|        - |  3756 | `		goto Abort;` |
|        - |  3757 | `	}` |
|        - |  3758 | `#endif` |
|        - |  3759 | `	/* Force the operands to be real */` |
|        3 |  3760 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  3761 | `		PH7_MemObjToReal(pTos);` |
|        1 |  3762 | `	}` |
|        3 |  3763 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  3764 | `		PH7_MemObjToReal(pNos);` |
|        1 |  3765 | `	}` |
|        - |  3766 | `	/* Perform the requested operation */` |
|        3 |  3767 | `	a = pTos->rVal;` |
|        3 |  3768 | `	b = pNos->rVal;` |
|        3 |  3769 | `	if( b == 0 ){` |
|        - |  3770 | `		/* Division by zero */` |
|      ! 0 |  3771 | `		r = 0;` |
|      ! 0 |  3772 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  3773 | `		/* goto Abort; */` |
|      ! 0 |  3774 | `	}else{` |
|        3 |  3775 | `		r = a/b;` |
|        - |  3776 | `		/* Push the result */` |
|        3 |  3777 | `		pNos->rVal = r;` |
|        3 |  3778 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3779 | `		/* Try to get an integer representation */` |
|        3 |  3780 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  3781 | `	}` |
|        3 |  3782 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3783 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3784 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3785 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3786 | `	}` |
|        3 |  3787 | `	VmPopOperand(&pTos,1);` |
|        3 |  3788 | `	break;` |
|        - |  3789 | `				}` |
|        - |  3790 | `/* OP_BAND * * *` |
|        - |  3791 | ` *` |
|        - |  3792 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  3793 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  3794 | ` * two elements.` |
|        - |  3795 | `*/` |
|        - |  3796 | `/* OP_BOR * * *` |
|        - |  3797 | ` *` |
|        - |  3798 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  3799 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  3800 | ` * two elements.` |
|        - |  3801 | ` */` |
|        - |  3802 | `/* OP_BXOR * * *` |
|        - |  3803 | ` *` |
|        - |  3804 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  3805 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  3806 | ` * two elements.` |
|        - |  3807 | ` */` |
|       19 |  3808 | `case PH7_OP_BAND:` |
|        - |  3809 | `case PH7_OP_BOR:` |
|        - |  3810 | `case PH7_OP_BXOR:{` |
|       39 |  3811 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3812 | `	sxi64 a,b,r;` |
|        - |  3813 | `#ifdef UNTRUST` |
|        - |  3814 | `	if( pNos < pStack ){` |
|        - |  3815 | `		goto Abort;` |
|        - |  3816 | `	}` |
|        - |  3817 | `#endif` |
|        - |  3818 | `	/* Force the operands to be integer */` |
|       39 |  3819 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3820 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3821 | `	}` |
|       39 |  3822 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3823 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  3824 | `	}` |
|        - |  3825 | `	/* Perform the requested operation */` |
|       39 |  3826 | `	a = pNos->x.iVal;` |
|       39 |  3827 | `	b = pTos->x.iVal;` |
|       39 |  3828 | `	switch(pInstr->iOp){` |
|        6 |  3829 | `	case PH7_OP_BOR_STORE:` |
|       13 |  3830 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        6 |  3831 | `	case PH7_OP_BXOR_STORE:` |
|       13 |  3832 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        7 |  3833 | `	case PH7_OP_BAND_STORE:` |
|        7 |  3834 | `	case PH7_OP_BAND:` |
|       15 |  3835 | `	default:          r = a&b; break;` |
|        - |  3836 | `	}` |
|        - |  3837 | `	/* Push the result */` |
|       39 |  3838 | `	pNos->x.iVal = r;` |
|       39 |  3839 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       39 |  3840 | `	VmPopOperand(&pTos,1);` |
|       39 |  3841 | `	break;` |
|        - |  3842 | `				 }` |
|        - |  3843 | `/* OP_BAND_STORE * * *` |
|        - |  3844 | ` *` |
|        - |  3845 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  3846 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  3847 | ` * two elements.` |
|        - |  3848 | `*/` |
|        - |  3849 | `/* OP_BOR_STORE * * *` |
|        - |  3850 | ` *` |
|        - |  3851 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  3852 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  3853 | ` * two elements.` |
|        - |  3854 | ` */` |
|        - |  3855 | `/* OP_BXOR_STORE * * *` |
|        - |  3856 | ` *` |
|        - |  3857 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  3858 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  3859 | ` * two elements.` |
|        - |  3860 | ` */` |
|        7 |  3861 | `case PH7_OP_BAND_STORE:` |
|        - |  3862 | `case PH7_OP_BOR_STORE:` |
|        - |  3863 | `case PH7_OP_BXOR_STORE:{` |
|       15 |  3864 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3865 | `	ph7_value *pObj;` |
|        - |  3866 | `	sxi64 a,b,r;` |
|        - |  3867 | `#ifdef UNTRUST` |
|        - |  3868 | `	if( pNos < pStack ){` |
|        - |  3869 | `		goto Abort;` |
|        - |  3870 | `	}` |
|        - |  3871 | `#endif` |
|        - |  3872 | `	/* Force the operands to be integer */` |
|       15 |  3873 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3874 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3875 | `	}` |
|       15 |  3876 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3877 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  3878 | `	}` |
|        - |  3879 | `	/* Perform the requested operation */` |
|       15 |  3880 | `	a = pTos->x.iVal;` |
|       15 |  3881 | `	b = pNos->x.iVal;` |
|       15 |  3882 | `	switch(pInstr->iOp){` |
|        2 |  3883 | `	case PH7_OP_BOR_STORE:` |
|        5 |  3884 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        3 |  3885 | `	case PH7_OP_BXOR_STORE:` |
|        7 |  3886 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        2 |  3887 | `	case PH7_OP_BAND_STORE:` |
|        2 |  3888 | `	case PH7_OP_BAND:` |
|        5 |  3889 | `	default:          r = a&b; break;` |
|        - |  3890 | `	}` |
|        - |  3891 | `	/* Push the result */` |
|       15 |  3892 | `	pNos->x.iVal = r;` |
|       15 |  3893 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  3894 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3895 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  3896 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  3897 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  3898 | `	}` |
|       15 |  3899 | `	VmPopOperand(&pTos,1);` |
|       15 |  3900 | `	break;` |
|        - |  3901 | `				 }` |
|        - |  3902 | `/* OP_SHL * * *` |
|        - |  3903 | ` *` |
|        - |  3904 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  3905 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  3906 | ` * left by N bits where N is the top element on the stack.` |
|        - |  3907 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  3908 | ` */` |
|        - |  3909 | `/* OP_SHR * * *` |
|        - |  3910 | ` *` |
|        - |  3911 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  3912 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  3913 | ` * right by N bits where N is the top element on the stack.` |
|        - |  3914 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  3915 | ` */` |
|        9 |  3916 | `case PH7_OP_SHL:` |
|        - |  3917 | `case PH7_OP_SHR: {` |
|       19 |  3918 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3919 | `	sxi64 a,r;` |
|        - |  3920 | `	sxi32 b;` |
|        - |  3921 | `#ifdef UNTRUST` |
|        - |  3922 | `	if( pNos < pStack ){` |
|        - |  3923 | `		goto Abort;` |
|        - |  3924 | `	}` |
|        - |  3925 | `#endif` |
|        - |  3926 | `	/* Force the operands to be integer */` |
|       19 |  3927 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3928 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3929 | `	}` |
|       19 |  3930 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3931 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  3932 | `	}` |
|        - |  3933 | `	/* Perform the requested operation */` |
|       19 |  3934 | `	a = pNos->x.iVal;` |
|       19 |  3935 | `	b = (sxi32)pTos->x.iVal;` |
|       19 |  3936 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       11 |  3937 | `		r = a << b;` |
|        6 |  3938 | `	}else{` |
|        9 |  3939 | `		r = a >> b;` |
|        - |  3940 | `	}` |
|        - |  3941 | `	/* Push the result */` |
|       19 |  3942 | `	pNos->x.iVal = r;` |
|       19 |  3943 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  3944 | `	VmPopOperand(&pTos,1);` |
|       19 |  3945 | `	break;` |
|        - |  3946 | `				 }` |
|        - |  3947 | `/*  OP_SHL_STORE * * *` |
|        - |  3948 | ` *` |
|        - |  3949 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  3950 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  3951 | ` * left by N bits where N is the top element on the stack.` |
|        - |  3952 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  3953 | ` */` |
|        - |  3954 | `/* OP_SHR_STORE * * *` |
|        - |  3955 | ` *` |
|        - |  3956 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  3957 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  3958 | ` * right by N bits where N is the top element on the stack.` |
|        - |  3959 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  3960 | ` */` |
|        7 |  3961 | `case PH7_OP_SHL_STORE:` |
|        - |  3962 | `case PH7_OP_SHR_STORE: {` |
|       15 |  3963 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3964 | `	ph7_value *pObj;` |
|        - |  3965 | `	sxi64 a,r;` |
|        - |  3966 | `	sxi32 b;` |
|        - |  3967 | `#ifdef UNTRUST` |
|        - |  3968 | `	if( pNos < pStack ){` |
|        - |  3969 | `		goto Abort;` |
|        - |  3970 | `	}` |
|        - |  3971 | `#endif` |
|        - |  3972 | `	/* Force the operands to be integer */` |
|       15 |  3973 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3974 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3975 | `	}` |
|       15 |  3976 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3977 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  3978 | `	}` |
|        - |  3979 | `	/* Perform the requested operation */` |
|       15 |  3980 | `	a = pTos->x.iVal;` |
|       15 |  3981 | `	b = (sxi32)pNos->x.iVal;` |
|       15 |  3982 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        7 |  3983 | `		r = a << b;` |
|        4 |  3984 | `	}else{` |
|        9 |  3985 | `		r = a >> b;` |
|        - |  3986 | `	}` |
|        - |  3987 | `	/* Push the result */` |
|       15 |  3988 | `	pNos->x.iVal = r;` |
|       15 |  3989 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  3990 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3991 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  3992 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  3993 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  3994 | `	}` |
|       15 |  3995 | `	VmPopOperand(&pTos,1);` |
|       15 |  3996 | `	break;` |
|        - |  3997 | `				 }` |
|        - |  3998 | `/* CAT:  P1 * *` |
|        - |  3999 | ` *` |
|        - |  4000 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  4001 | ` * back.` |
|        - |  4002 | ` */` |
|    45068 |  4003 | `case PH7_OP_CAT:{` |
|        - |  4004 | `	ph7_value *pNos,*pCur;` |
|    90138 |  4005 | `	if( pInstr->iP1 < 1 ){` |
|    60890 |  4006 | `		pNos = &pTos[-1];` |
|    30446 |  4007 | `	}else{` |
|    29250 |  4008 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4009 | `	}` |
|        - |  4010 | `#ifdef UNTRUST` |
|        - |  4011 | `	if( pNos < pStack ){` |
|        - |  4012 | `		goto Abort;` |
|        - |  4013 | `	}` |
|        - |  4014 | `#endif` |
|        - |  4015 | `	/* Force a string cast */` |
|    90138 |  4016 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      536 |  4017 | `		PH7_MemObjToString(pNos);` |
|      267 |  4018 | `	}` |
|    90138 |  4019 | `	pCur = &pNos[1];` |
|   189206 |  4020 | `	while( pCur <= pTos ){` |
|    99070 |  4021 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    52748 |  4022 | `			PH7_MemObjToString(pCur);` |
|    26373 |  4023 | `		}` |
|        - |  4024 | `		/* Perform the concatenation */` |
|    99070 |  4025 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|    99030 |  4026 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    49514 |  4027 | `		}` |
|    99070 |  4028 | `		SyBlobRelease(&pCur->sBlob);` |
|    99070 |  4029 | `		pCur++;` |
|        2 |  4030 | `	}` |
|    90138 |  4031 | `	pTos = pNos;` |
|    90138 |  4032 | `	break;` |
|        - |  4033 | `				}` |
|        - |  4034 | `/*  CAT_STORE: * * *` |
|        - |  4035 | ` *` |
|        - |  4036 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  4037 | ` * back.` |
|        - |  4038 | ` */` |
|       39 |  4039 | `case PH7_OP_CAT_STORE:{` |
|       79 |  4040 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4041 | `	ph7_value *pObj;` |
|        - |  4042 | `#ifdef UNTRUST` |
|        - |  4043 | `	if( pNos < pStack ){` |
|        - |  4044 | `		goto Abort;` |
|        - |  4045 | `	}` |
|        - |  4046 | `#endif` |
|       79 |  4047 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4048 | `		/* Force a string cast */` |
|       19 |  4049 | `		PH7_MemObjToString(pTos);` |
|        9 |  4050 | `	}` |
|       79 |  4051 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4052 | `		/* Force a string cast */` |
|      ! 0 |  4053 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4054 | `	}` |
|        - |  4055 | `	/* Perform the concatenation (Reverse order) */` |
|       79 |  4056 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|       79 |  4057 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       39 |  4058 | `	}` |
|        - |  4059 | `	/* Perform the store operation */` |
|       79 |  4060 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4061 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       79 |  4062 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       79 |  4063 | `		PH7_MemObjStore(pTos,pObj);` |
|       39 |  4064 | `	}` |
|       79 |  4065 | `	PH7_MemObjStore(pTos,pNos);` |
|       79 |  4066 | `	VmPopOperand(&pTos,1);` |
|       79 |  4067 | `	break;` |
|        - |  4068 | `				}` |
|        - |  4069 | `/* OP_AND: * * *` |
|        - |  4070 | ` *` |
|        - |  4071 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  4072 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4073 | ` * stack.` |
|        - |  4074 | ` */` |
|        - |  4075 | `/* OP_OR: * * *` |
|        - |  4076 | ` *` |
|        - |  4077 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  4078 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4079 | ` * stack.` |
|        - |  4080 | ` */` |
|    19868 |  4081 | `case PH7_OP_LAND:` |
|        - |  4082 | `case PH7_OP_LOR: {` |
|    39741 |  4083 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4084 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4085 | `#ifdef UNTRUST` |
|        - |  4086 | `	if( pNos < pStack ){` |
|        - |  4087 | `		goto Abort;` |
|        - |  4088 | `	}` |
|        - |  4089 | `#endif` |
|        - |  4090 | `	/* Force a boolean cast */` |
|    39741 |  4091 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4092 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4093 | `	}` |
|    39741 |  4094 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4095 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4096 | `	}` |
|    39741 |  4097 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|    39741 |  4098 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|    39741 |  4099 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4100 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    21041 |  4101 | `		v1 = and_logic[v1*3+v2];` |
|    10523 |  4102 | `	}else{` |
|        - |  4103 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|    18702 |  4104 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4105 | `	}` |
|    39741 |  4106 | `	if( v1 == 2 ){` |
|      ! 0 |  4107 | `		v1 = 1;` |
|      ! 0 |  4108 | `	}` |
|    39741 |  4109 | `	VmPopOperand(&pTos,1);` |
|    39741 |  4110 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|    39741 |  4111 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    39741 |  4112 | `	break;` |
|        - |  4113 | `				 }` |
|        - |  4114 | `/* OP_LXOR: * * *` |
|        - |  4115 | ` *` |
|        - |  4116 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  4117 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4118 | ` * stack.` |
|        - |  4119 | ` * According to the PHP language reference manual:` |
|        - |  4120 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  4121 | ` *  TRUE,but not both.` |
|        - |  4122 | ` */` |
|        5 |  4123 | `case PH7_OP_LXOR:{` |
|       11 |  4124 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  4125 | `	sxi32 v = 0;` |
|        - |  4126 | `#ifdef UNTRUST` |
|        - |  4127 | `	if( pNos < pStack ){` |
|        - |  4128 | `		goto Abort;` |
|        - |  4129 | `	}` |
|        - |  4130 | `#endif` |
|        - |  4131 | `	/* Force a boolean cast */` |
|       11 |  4132 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4133 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4134 | `	}` |
|       11 |  4135 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4136 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4137 | `	}` |
|       11 |  4138 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  4139 | `		v = 1;` |
|        3 |  4140 | `	}` |
|       11 |  4141 | `	VmPopOperand(&pTos,1);` |
|       11 |  4142 | `	pTos->x.iVal = v;` |
|       11 |  4143 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  4144 | `	break;` |
|        - |  4145 | `				 }` |
|        - |  4146 | `/* OP_EQ P1 P2 P3` |
|        - |  4147 | ` *` |
|        - |  4148 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  4149 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4150 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4151 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4152 | ` */` |
|        - |  4153 | `/* OP_NEQ P1 P2 P3` |
|        - |  4154 | ` *` |
|        - |  4155 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  4156 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4157 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4158 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4159 | ` */` |
|     2680 |  4160 | `case PH7_OP_EQ:` |
|        - |  4161 | `case PH7_OP_NEQ: {` |
|     5362 |  4162 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4163 | `	/* Perform the comparison and act accordingly */` |
|        - |  4164 | `#ifdef UNTRUST` |
|        - |  4165 | `	if( pNos < pStack ){` |
|        - |  4166 | `		goto Abort;` |
|        - |  4167 | `	}` |
|        - |  4168 | `#endif` |
|     5362 |  4169 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     5362 |  4170 | `	if( pInstr->iOp == PH7_OP_EQ ){` |
|     5346 |  4171 | `		rc = rc == 0;` |
|     2674 |  4172 | `	}else{` |
|       18 |  4173 | `		rc = rc != 0;` |
|        - |  4174 | `	}` |
|     5362 |  4175 | `	VmPopOperand(&pTos,1);` |
|     5362 |  4176 | `	if( !pInstr->iP2 ){` |
|        - |  4177 | `		/* Push comparison result without taking the jump */` |
|     5362 |  4178 | `		PH7_MemObjRelease(pTos);` |
|     5362 |  4179 | `		pTos->x.iVal = rc;` |
|        - |  4180 | `		/* Invalidate any prior representation */` |
|     5362 |  4181 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     2682 |  4182 | `	}else{` |
|      ! 0 |  4183 | `		if( rc ){` |
|        - |  4184 | `			/* Jump to the desired location */` |
|      ! 0 |  4185 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4186 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4187 | `		}` |
|        - |  4188 | `	}` |
|     5362 |  4189 | `	break;` |
|        - |  4190 | `				 }` |
|        - |  4191 | `/* OP_TEQ P1 P2 *` |
|        - |  4192 | ` *` |
|        - |  4193 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4194 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4195 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4196 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4197 | ` */` |
|    55708 |  4198 | `case PH7_OP_TEQ: {` |
|   111418 |  4199 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4200 | `	/* Perform the comparison and act accordingly */` |
|        - |  4201 | `#ifdef UNTRUST` |
|        - |  4202 | `	if( pNos < pStack ){` |
|        - |  4203 | `		goto Abort;` |
|        - |  4204 | `	}` |
|        - |  4205 | `#endif` |
|   111418 |  4206 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0) == 0;` |
|   111418 |  4207 | `	VmPopOperand(&pTos,1);` |
|   111418 |  4208 | `	if( !pInstr->iP2 ){` |
|        - |  4209 | `		/* Push comparison result without taking the jump */` |
|   111418 |  4210 | `		PH7_MemObjRelease(pTos);` |
|   111418 |  4211 | `		pTos->x.iVal = rc;` |
|        - |  4212 | `		/* Invalidate any prior representation */` |
|   111418 |  4213 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    55710 |  4214 | `	}else{` |
|      ! 0 |  4215 | `		if( rc ){` |
|        - |  4216 | `			/* Jump to the desired location */` |
|      ! 0 |  4217 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4218 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4219 | `		}` |
|        - |  4220 | `	}` |
|   111418 |  4221 | `	break;` |
|        - |  4222 | `				 }` |
|        - |  4223 | `/* OP_TNE P1 P2 *` |
|        - |  4224 | ` *` |
|        - |  4225 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  4226 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  4227 | ` * instruction.` |
|        - |  4228 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4229 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4230 | ` *` |
|        - |  4231 | ` */` |
|    43360 |  4232 | `case PH7_OP_TNE: {` |
|    86722 |  4233 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4234 | `	/* Perform the comparison and act accordingly */` |
|        - |  4235 | `#ifdef UNTRUST` |
|        - |  4236 | `	if( pNos < pStack ){` |
|        - |  4237 | `		goto Abort;` |
|        - |  4238 | `	}` |
|        - |  4239 | `#endif` |
|    86722 |  4240 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0) != 0;` |
|    86722 |  4241 | `	VmPopOperand(&pTos,1);` |
|    86722 |  4242 | `	if( !pInstr->iP2 ){` |
|        - |  4243 | `		/* Push comparison result without taking the jump */` |
|    86722 |  4244 | `		PH7_MemObjRelease(pTos);` |
|    86722 |  4245 | `		pTos->x.iVal = rc;` |
|        - |  4246 | `		/* Invalidate any prior representation */` |
|    86722 |  4247 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    43362 |  4248 | `	}else{` |
|      ! 0 |  4249 | `		if( rc ){` |
|        - |  4250 | `			/* Jump to the desired location */` |
|      ! 0 |  4251 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4252 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4253 | `		}` |
|        - |  4254 | `	}` |
|    86722 |  4255 | `	break;` |
|        - |  4256 | `				 }` |
|        - |  4257 | `/* OP_LT P1 P2 P3` |
|        - |  4258 | ` *` |
|        - |  4259 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4260 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4261 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4262 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4263 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4264 | ` *` |
|        - |  4265 | ` */` |
|        - |  4266 | `/* OP_LE P1 P2 P3` |
|        - |  4267 | ` *` |
|        - |  4268 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4269 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4270 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4271 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4272 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4273 | ` *` |
|        - |  4274 | ` */` |
|    34687 |  4275 | `case PH7_OP_LT:` |
|        - |  4276 | `case PH7_OP_LE: {` |
|    69379 |  4277 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4278 | `	/* Perform the comparison and act accordingly */` |
|        - |  4279 | `#ifdef UNTRUST` |
|        - |  4280 | `	if( pNos < pStack ){` |
|        - |  4281 | `		goto Abort;` |
|        - |  4282 | `	}` |
|        - |  4283 | `#endif` |
|    69379 |  4284 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|    69379 |  4285 | `	if( pInstr->iOp == PH7_OP_LE ){` |
|      395 |  4286 | `		rc = rc < 1;` |
|      198 |  4287 | `	}else{` |
|    68985 |  4288 | `		rc = rc < 0;` |
|        - |  4289 | `	}` |
|    69379 |  4290 | `	VmPopOperand(&pTos,1);` |
|    69379 |  4291 | `	if( !pInstr->iP2 ){` |
|        - |  4292 | `		/* Push comparison result without taking the jump */` |
|    69379 |  4293 | `		PH7_MemObjRelease(pTos);` |
|    69379 |  4294 | `		pTos->x.iVal = rc;` |
|        - |  4295 | `		/* Invalidate any prior representation */` |
|    69379 |  4296 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    34692 |  4297 | `	}else{` |
|      ! 0 |  4298 | `		if( rc ){` |
|        - |  4299 | `			/* Jump to the desired location */` |
|      ! 0 |  4300 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4301 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4302 | `		}` |
|        - |  4303 | `	}` |
|    69379 |  4304 | `	break;` |
|        - |  4305 | `				}` |
|        - |  4306 | `/* OP_GT P1 P2 P3` |
|        - |  4307 | ` *` |
|        - |  4308 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4309 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4310 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4311 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4312 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4313 | ` *` |
|        - |  4314 | ` */` |
|        - |  4315 | `/* OP_GE P1 P2 P3` |
|        - |  4316 | ` *` |
|        - |  4317 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4318 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4319 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4320 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4321 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4322 | ` *` |
|        - |  4323 | ` */` |
|     6962 |  4324 | `case PH7_OP_GT:` |
|        - |  4325 | `case PH7_OP_GE: {` |
|    13926 |  4326 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4327 | `	/* Perform the comparison and act accordingly */` |
|        - |  4328 | `#ifdef UNTRUST` |
|        - |  4329 | `	if( pNos < pStack ){` |
|        - |  4330 | `		goto Abort;` |
|        - |  4331 | `	}` |
|        - |  4332 | `#endif` |
|    13926 |  4333 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|    13926 |  4334 | `	if( pInstr->iOp == PH7_OP_GE ){` |
|    13784 |  4335 | `		rc = rc >= 0;` |
|     6893 |  4336 | `	}else{` |
|      144 |  4337 | `		rc = rc > 0;` |
|        - |  4338 | `	}` |
|    13926 |  4339 | `	VmPopOperand(&pTos,1);` |
|    13926 |  4340 | `	if( !pInstr->iP2 ){` |
|        - |  4341 | `		/* Push comparison result without taking the jump */` |
|    13926 |  4342 | `		PH7_MemObjRelease(pTos);` |
|    13926 |  4343 | `		pTos->x.iVal = rc;` |
|        - |  4344 | `		/* Invalidate any prior representation */` |
|    13926 |  4345 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     6964 |  4346 | `	}else{` |
|      ! 0 |  4347 | `		if( rc ){` |
|        - |  4348 | `			/* Jump to the desired location */` |
|      ! 0 |  4349 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4350 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4351 | `		}` |
|        - |  4352 | `	}` |
|    13926 |  4353 | `	break;` |
|        - |  4354 | `				}` |
|        - |  4355 | `/* OP_SEQ P1 P2 *` |
|        - |  4356 | ` * Strict string comparison.` |
|        - |  4357 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  4358 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4359 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4360 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4361 | ` * use PH7_OP_EQ.` |
|        - |  4362 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4363 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4364 | ` */` |
|        - |  4365 | `/* OP_SNE P1 P2 *` |
|        - |  4366 | ` * Strict string comparison.` |
|        - |  4367 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  4368 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4369 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4370 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4371 | ` * use PH7_OP_EQ.` |
|        - |  4372 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4373 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4374 | ` */` |
|       18 |  4375 | `case PH7_OP_SEQ:` |
|        - |  4376 | `case PH7_OP_SNE: {` |
|       38 |  4377 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4378 | `	SyString s1,s2;` |
|        - |  4379 | `	/* Perform the comparison and act accordingly */` |
|        - |  4380 | `#ifdef UNTRUST` |
|        - |  4381 | `	if( pNos < pStack ){` |
|        - |  4382 | `		goto Abort;` |
|        - |  4383 | `	}` |
|        - |  4384 | `#endif` |
|        - |  4385 | `	/* Force a string cast */` |
|       38 |  4386 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        9 |  4387 | `		PH7_MemObjToString(pTos);` |
|        4 |  4388 | `	}` |
|       38 |  4389 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  4390 | `		PH7_MemObjToString(pNos);` |
|        2 |  4391 | `	}` |
|       38 |  4392 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  4393 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  4394 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  4395 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  4396 | `		rc = rc != 0;` |
|      ! 0 |  4397 | `	}else{` |
|       38 |  4398 | `		rc = rc == 0;` |
|        - |  4399 | `	}` |
|       38 |  4400 | `	VmPopOperand(&pTos,1);` |
|       38 |  4401 | `	if( !pInstr->iP2 ){` |
|        - |  4402 | `		/* Push comparison result without taking the jump */` |
|       38 |  4403 | `		PH7_MemObjRelease(pTos);` |
|       38 |  4404 | `		pTos->x.iVal = rc;` |
|        - |  4405 | `		/* Invalidate any prior representation */` |
|       38 |  4406 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  4407 | `	}else{` |
|      ! 0 |  4408 | `		if( rc ){` |
|        - |  4409 | `			/* Jump to the desired location */` |
|      ! 0 |  4410 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4411 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4412 | `		}` |
|        - |  4413 | `	}` |
|       38 |  4414 | `	break;` |
|        - |  4415 | `				 }` |
|        - |  4416 | `/*` |
|        - |  4417 | ` * OP_LOAD_REF * * *` |
|        - |  4418 | ` * Push the index of a referenced object on the stack.` |
|        - |  4419 | ` */` |
|       57 |  4420 | `case PH7_OP_LOAD_REF: {` |
|        - |  4421 | `	sxu32 nIdx;` |
|        - |  4422 | `#ifdef UNTRUST` |
|        - |  4423 | `	if( pTos < pStack ){` |
|        - |  4424 | `		goto Abort;` |
|        - |  4425 | `	}` |
|        - |  4426 | `#endif` |
|        - |  4427 | `	/* Extract memory object index */` |
|      115 |  4428 | `	nIdx = pTos->nIdx;` |
|      115 |  4429 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  4430 | `		/* Nullify the object */` |
|       95 |  4431 | `		PH7_MemObjRelease(pTos);` |
|        - |  4432 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  4433 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  4434 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  4435 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  4436 | `	}` |
|      115 |  4437 | `	break;` |
|        - |  4438 | `					  }` |
|        - |  4439 | `/*` |
|        - |  4440 | ` * OP_STORE_REF * * P3` |
|        - |  4441 | ` * Perform an assignment operation by reference.` |
|        - |  4442 | ` */` |
|       14 |  4443 | ` case PH7_OP_STORE_REF: {` |
|       30 |  4444 | `	 SyString sName = { 0 , 0 };` |
|        - |  4445 | `	 VmFrame *pFrameLocal;` |
|        - |  4446 | `	SyHashEntry *pEntry;` |
|        - |  4447 | `	sxu32 nIdx;` |
|        - |  4448 | `#ifdef UNTRUST` |
|        - |  4449 | `	if( pTos < pStack ){` |
|        - |  4450 | `		goto Abort;` |
|        - |  4451 | `	}` |
|        - |  4452 | `#endif` |
|       30 |  4453 | `	if( pInstr->p3 == 0 ){` |
|        - |  4454 | `		char *zName;` |
|        - |  4455 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  4456 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4457 | `			/* Force a string cast */` |
|      ! 0 |  4458 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4459 | `		}` |
|      ! 0 |  4460 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4461 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  4462 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4463 | `			if( zName ){` |
|      ! 0 |  4464 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4465 | `			}` |
|      ! 0 |  4466 | `		}` |
|      ! 0 |  4467 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  4468 | `		pTos--;` |
|      ! 0 |  4469 | `	}else{` |
|       30 |  4470 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4471 | `	}` |
|       30 |  4472 | `	nIdx = pTos->nIdx;` |
|       30 |  4473 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  4474 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  4475 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  4476 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  4477 | `		}else{` |
|        - |  4478 | `			ph7_value *pObj;` |
|        - |  4479 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  4480 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  4481 | `			if( pObj == 0 ){` |
|      ! 0 |  4482 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  4483 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4484 | `				goto Abort;` |
|        - |  4485 | `			}` |
|        - |  4486 | `			/* Perform the store operation */` |
|      ! 0 |  4487 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  4488 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  4489 | `		}` |
|       30 |  4490 | `	}else if( sName.nByte > 0){` |
|       30 |  4491 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  4492 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  4493 | `		}else{` |
|       30 |  4494 | `			pFrameLocal = pVm->pFrame;` |
|       30 |  4495 | `			while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4496 | `				/* Safely ignore the exception frame */` |
|      ! 0 |  4497 | `				pFrameLocal = pFrameLocal->pParent;` |
|      ! 0 |  4498 | `			}` |
|        - |  4499 | `			/* Query the local frame */` |
|       30 |  4500 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       30 |  4501 | `			if( pEntry ){` |
|      ! 0 |  4502 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  4503 | `			}else{` |
|       30 |  4504 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       30 |  4505 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  4506 | `					/* Insert in the $GLOBALS array */` |
|       26 |  4507 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       12 |  4508 | `				}` |
|       30 |  4509 | `				if( rc == SXRET_OK ){` |
|       30 |  4510 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       14 |  4511 | `				}` |
|        - |  4512 | `			}` |
|        - |  4513 | `		}` |
|       14 |  4514 | `	}` |
|       30 |  4515 | `	break;` |
|        - |  4516 | `				 }` |
|        - |  4517 | `/*` |
|        - |  4518 | ` * OP_UPLINK P1 * *` |
|        - |  4519 | ` * Link a variable to the top active VM frame.` |
|        - |  4520 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  4521 | ` */` |
|       14 |  4522 | `case PH7_OP_UPLINK: {` |
|       29 |  4523 | `	if( pVm->pFrame->pParent ){` |
|       29 |  4524 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  4525 | `		SyString sName;` |
|        - |  4526 | `		/* Perform the link */` |
|       59 |  4527 | `		while( pLink <= pTos ){` |
|       31 |  4528 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4529 | `				/* Force a string cast */` |
|      ! 0 |  4530 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  4531 | `			}` |
|       31 |  4532 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       31 |  4533 | `			if( sName.nByte > 0 ){` |
|       31 |  4534 | `				VmFrameLink(&(*pVm),&sName);` |
|       15 |  4535 | `			}` |
|       31 |  4536 | `			pLink++;` |
|        1 |  4537 | `		}` |
|       14 |  4538 | `	}` |
|       29 |  4539 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       29 |  4540 | `	break;` |
|        - |  4541 | `					}` |
|        - |  4542 | `/*` |
|        - |  4543 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  4544 | ` * Push an exception in the corresponding container so that` |
|        - |  4545 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  4546 | ` */` |
|        9 |  4547 | `case PH7_OP_LOAD_EXCEPTION: {` |
|       20 |  4548 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  4549 | `	VmFrame *pFrameLocal;` |
|       20 |  4550 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  4551 | `	/* Create the exception frame */` |
|       20 |  4552 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|       20 |  4553 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4554 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  4555 | `		goto Abort;` |
|        - |  4556 | `	}` |
|        - |  4557 | `	/* Mark the special frame */` |
|       20 |  4558 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|       20 |  4559 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  4560 | `	/* Point to the frame that trigger the exception */` |
|       20 |  4561 | `	pFrameLocal = pFrameLocal->pParent;` |
|       22 |  4562 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        3 |  4563 | `		pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4564 | `	}` |
|       20 |  4565 | `	pException->pFrame = pFrameLocal;` |
|       20 |  4566 | `	break;` |
|        - |  4567 | `							}` |
|        - |  4568 | `/*` |
|        - |  4569 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  4570 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  4571 | ` */` |
|        9 |  4572 | `case PH7_OP_POP_EXCEPTION: {` |
|       20 |  4573 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|       20 |  4574 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  4575 | `		ph7_exception **apException;` |
|        - |  4576 | `		/* Pop the loaded exception */` |
|        7 |  4577 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        7 |  4578 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|        7 |  4579 | `			(void)SySetPop(&pVm->aException);` |
|        3 |  4580 | `		}` |
|        3 |  4581 | `	}` |
|       20 |  4582 | `	pException->pFrame = 0;` |
|        - |  4583 | `	/* Leave the exception frame */` |
|       20 |  4584 | `	VmLeaveFrame(&(*pVm));` |
|       20 |  4585 | `	break;` |
|        - |  4586 | `							}` |
|        - |  4587 |  |
|        - |  4588 | `/*` |
|        - |  4589 | ` * OP_THROW * P2 *` |
|        - |  4590 | ` * Throw an user exception.` |
|        - |  4591 | ` */` |
|        8 |  4592 | `case PH7_OP_THROW: {` |
|       18 |  4593 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       18 |  4594 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  4595 | `#ifdef UNTRUST` |
|        - |  4596 | `	if( pTos < pStack ){` |
|        - |  4597 | `		goto Abort;` |
|        - |  4598 | `	}` |
|        - |  4599 | `#endif` |
|       22 |  4600 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4601 | `		/* Safely ignore the exception frame */` |
|        6 |  4602 | `		pFrameLocal = pFrameLocal->pParent;` |
|        2 |  4603 | `	}` |
|        - |  4604 | `	/* Tell the upper layer that an exception was thrown */` |
|       18 |  4605 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       18 |  4606 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       18 |  4607 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4608 | `		ph7_class *pException;` |
|        - |  4609 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  4610 | `		 */` |
|       18 |  4611 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       18 |  4612 | `		if( pException == 0 \|\| !VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  4613 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  4614 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  4615 | `			if( rc == SXERR_ABORT ){` |
|        - |  4616 | `				/* Abort processing immediately */` |
|      ! 0 |  4617 | `				goto Abort;` |
|        - |  4618 | `			}` |
|      ! 0 |  4619 | `		}else{` |
|        - |  4620 | `			/* Throw the exception */` |
|       18 |  4621 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       18 |  4622 | `			if( rc == SXERR_ABORT ){` |
|        - |  4623 | `				/* Abort processing immediately */` |
|        3 |  4624 | `				goto Abort;` |
|        - |  4625 | `			}` |
|        - |  4626 | `		}` |
|        9 |  4627 | `	}else{` |
|        - |  4628 | `		/* Expecting a class instance */` |
|      ! 0 |  4629 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  4630 | `		if( rc == SXERR_ABORT ){` |
|        - |  4631 | `			/* Abort processing immediately */` |
|      ! 0 |  4632 | `			goto Abort;` |
|        - |  4633 | `		}` |
|        - |  4634 | `	}` |
|        - |  4635 | `	/* Pop the top entry */` |
|       16 |  4636 | `	VmPopOperand(&pTos,1);` |
|        - |  4637 | `	/* Perform an unconditional jump */` |
|       16 |  4638 | `	pc = nJump - 1;` |
|       16 |  4639 | `	break;` |
|        - |  4640 | `				   }` |
|        - |  4641 | `/*` |
|        - |  4642 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  4643 | ` * Prepare a foreach step.` |
|        - |  4644 | ` */` |
|     2972 |  4645 | `case PH7_OP_FOREACH_INIT: {` |
|     5946 |  4646 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4647 | `	void *pName;` |
|        - |  4648 | `#ifdef UNTRUST` |
|        - |  4649 | `	if( pTos < pStack ){` |
|        - |  4650 | `		goto Abort;` |
|        - |  4651 | `	}` |
|        - |  4652 | `#endif` |
|     5946 |  4653 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4654 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  4655 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4656 | `			/* Force a string cast */` |
|      ! 0 |  4657 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4658 | `		}` |
|        - |  4659 | `		/* Duplicate name */` |
|      ! 0 |  4660 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4661 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4662 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4663 | `		}` |
|      ! 0 |  4664 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4665 | `	}` |
|     5946 |  4666 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  4667 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4668 | `			/* Force a string cast */` |
|      ! 0 |  4669 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4670 | `		}` |
|        - |  4671 | `		/* Duplicate name */` |
|      ! 0 |  4672 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4673 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4674 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4675 | `		}` |
|      ! 0 |  4676 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4677 | `	}` |
|        - |  4678 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|     5946 |  4679 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4680 | `		/* Jump out of the loop */` |
|      ! 0 |  4681 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  4682 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  4683 | `		}` |
|      ! 0 |  4684 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  4685 | `	}else{` |
|        - |  4686 | `		ph7_foreach_step *pStep;` |
|     5946 |  4687 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|     5946 |  4688 | `		if( pStep == 0 ){` |
|      ! 0 |  4689 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  4690 | `			/* Jump out of the loop */` |
|      ! 0 |  4691 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4692 | `		}else{` |
|        - |  4693 | `			/* Zero the structure */` |
|     5946 |  4694 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  4695 | `			/* Prepare the step */` |
|     5946 |  4696 | `			pStep->iFlags = pInfo->iFlags;` |
|     5946 |  4697 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|     5938 |  4698 | `				ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4699 | `				/* Reset the internal loop cursor */` |
|     5938 |  4700 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  4701 | `				/* Mark the step */` |
|     5938 |  4702 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|     5938 |  4703 | `				pStep->xIter.pMap = pMap;` |
|     5938 |  4704 | `				pMap->iRef++;` |
|     2970 |  4705 | `			}else{` |
|        9 |  4706 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4707 | `				/* Reset the loop cursor */` |
|        9 |  4708 | `				SyHashResetLoopCursor(&pThis->hAttr);` |
|        - |  4709 | `				/* Mark the step */` |
|        9 |  4710 | `				pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  4711 | `				pStep->xIter.pThis = pThis;` |
|        9 |  4712 | `				pThis->iRef++;` |
|        - |  4713 | `			}` |
|        - |  4714 | `		}` |
|     5946 |  4715 | `		if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  4716 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  4717 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  4718 | `			/* Jump out of the loop */` |
|      ! 0 |  4719 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4720 | `		}` |
|        - |  4721 | `	}` |
|     5946 |  4722 | `	VmPopOperand(&pTos,1);` |
|     5946 |  4723 | `	break;` |
|        - |  4724 | `						  }` |
|        - |  4725 | `/*` |
|        - |  4726 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  4727 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  4728 | ` */` |
|    50221 |  4729 | `case PH7_OP_FOREACH_STEP: {` |
|   100444 |  4730 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4731 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  4732 | `	ph7_value *pValue;` |
|        - |  4733 | `	VmFrame *pFrameLocal;` |
|        - |  4734 | `	/* Peek the last step */` |
|   100444 |  4735 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   100444 |  4736 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   100444 |  4737 | `	pFrameLocal = pVm->pFrame;` |
|   100444 |  4738 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4739 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  4740 | `		pFrameLocal = pFrameLocal->pParent;` |
|      ! 0 |  4741 | `	}` |
|   100444 |  4742 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   100420 |  4743 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  4744 | `		ph7_hashmap_node *pNode;` |
|        - |  4745 | `		/* Extract the current node value */` |
|   100420 |  4746 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   100420 |  4747 | `		if( pNode == 0 ){` |
|        - |  4748 | `			/* No more entry to process */` |
|     5938 |  4749 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|     5938 |  4750 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  4751 | `				/* Break the reference with the last element */` |
|        5 |  4752 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        2 |  4753 | `			}` |
|        - |  4754 | `			/* Automatically reset the loop cursor */` |
|     5938 |  4755 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  4756 | `			/* Cleanup the mess left behind */` |
|     5938 |  4757 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     5938 |  4758 | `			SySetPop(&pInfo->aStep);` |
|     5938 |  4759 | `			PH7_HashmapUnref(pMap);` |
|     2970 |  4760 | `		}else{` |
|    94484 |  4761 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      135 |  4762 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      135 |  4763 | `				if( pKey ){` |
|      135 |  4764 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|       67 |  4765 | `				}` |
|       67 |  4766 | `			}` |
|    94484 |  4767 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  4768 | `				SyHashEntry *pEntry;` |
|        - |  4769 | `				/* Pass by reference */` |
|       13 |  4770 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       13 |  4771 | `				if( pEntry ){` |
|       13 |  4772 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|        7 |  4773 | `				}else{` |
|      ! 0 |  4774 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  4775 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  4776 | `				}` |
|        7 |  4777 | `			}else{` |
|        - |  4778 | `				/* Make a copy of the entry value */` |
|    94472 |  4779 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|    94472 |  4780 | `				if( pValue ){` |
|    94472 |  4781 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    47235 |  4782 | `				}` |
|        - |  4783 | `			}` |
|        - |  4784 | `		}` |
|    50211 |  4785 | `	}else{` |
|       25 |  4786 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  4787 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  4788 | `		SyHashEntry *pEntry;` |
|        - |  4789 | `		/* Point to the next attribute */` |
|       29 |  4790 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  4791 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  4792 | `			/* Check access permission */` |
|       31 |  4793 | `			if( VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  4794 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  4795 | `					break; /* Access is granted */` |
|        - |  4796 | `			}` |
|        1 |  4797 | `		}` |
|       25 |  4798 | `		if( pEntry == 0 ){` |
|        - |  4799 | `			/* Clean up the mess left behind */` |
|        9 |  4800 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  4801 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  4802 | `				/* Break the reference with the last element */` |
|        3 |  4803 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  4804 | `			}` |
|        9 |  4805 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  4806 | `			SySetPop(&pInfo->aStep);` |
|        9 |  4807 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  4808 | `		}else{` |
|       17 |  4809 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  4810 | `			ph7_value *pAttrValue;` |
|       17 |  4811 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  4812 | `				/* Fill with the current attribute name */` |
|       17 |  4813 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  4814 | `				if( pKey ){` |
|       17 |  4815 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  4816 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  4817 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  4818 | `				}` |
|        8 |  4819 | `			}` |
|        - |  4820 | `			/* Extract attribute value */` |
|       17 |  4821 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  4822 | `			if( pAttrValue ){` |
|       17 |  4823 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  4824 | `					/* Pass by reference */` |
|        3 |  4825 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  4826 | `					if( pEntry ){` |
|        3 |  4827 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  4828 | `					}else{` |
|      ! 0 |  4829 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  4830 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  4831 | `					}` |
|        2 |  4832 | `				}else{` |
|        - |  4833 | `					/* Make a copy of the attribute value */` |
|       15 |  4834 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  4835 | `					if( pValue ){` |
|       15 |  4836 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  4837 | `					}` |
|        - |  4838 | `				}` |
|        8 |  4839 | `			}` |
|        - |  4840 | `		}` |
|        - |  4841 | `	}` |
|   100444 |  4842 | `	break;` |
|        - |  4843 | `						  }` |
|        - |  4844 | `/*` |
|        - |  4845 | ` * OP_MEMBER P1 P2` |
|        - |  4846 | ` * Load class attribute/method on the stack.` |
|        - |  4847 | ` */` |
|      414 |  4848 | `case PH7_OP_MEMBER: {` |
|        - |  4849 | `	ph7_class_instance *pThis;` |
|        - |  4850 | `	ph7_value *pNos;` |
|        - |  4851 | `	SyString sName;` |
|      830 |  4852 | `	if( !pInstr->iP1 ){` |
|      772 |  4853 | `		pNos = &pTos[-1];` |
|        - |  4854 | `#ifdef UNTRUST` |
|        - |  4855 | `		if( pNos < pStack ){` |
|        - |  4856 | `			goto Abort;` |
|        - |  4857 | `		}` |
|        - |  4858 | `#endif` |
|      772 |  4859 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  4860 | `			ph7_class *pClass;` |
|        - |  4861 | `			/* Class already instantiated */` |
|      772 |  4862 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  4863 | `			/* Point to the instantiated class */` |
|      772 |  4864 | `			pClass = pThis->pClass;` |
|        - |  4865 | `			/* Extract attribute name first */` |
|      772 |  4866 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      772 |  4867 | `			if( pInstr->iP2 ){` |
|        - |  4868 | `				/* Method call */` |
|      120 |  4869 | `				ph7_class_method *pMeth = 0;` |
|      120 |  4870 | `				if( sName.nByte > 0 ){` |
|        - |  4871 | `					/* Extract the target method */` |
|      120 |  4872 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       59 |  4873 | `				}` |
|      120 |  4874 | `				if( pMeth == 0 ){` |
|      ! 0 |  4875 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  4876 | `						&pClass->sName,&sName` |
|        - |  4877 | `						);` |
|        - |  4878 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  4879 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  4880 | `					/* Pop the method name from the stack */` |
|      ! 0 |  4881 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  4882 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  4883 | `				}else{` |
|        - |  4884 | `					/* Push method name on the stack */` |
|      120 |  4885 | `					PH7_MemObjRelease(pTos);` |
|      120 |  4886 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      120 |  4887 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  4888 | `				}` |
|      120 |  4889 | `				pTos->nIdx = SXU32_HIGH;` |
|       61 |  4890 | `			}else{` |
|        - |  4891 | `				/* Attribute access */` |
|      654 |  4892 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  4893 | `				SyHashEntry *pEntry;` |
|        - |  4894 | `				/* Extract the target attribute */` |
|      654 |  4895 | `				if( sName.nByte > 0 ){` |
|      654 |  4896 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|      654 |  4897 | `					if( pEntry ){` |
|        - |  4898 | `						/* Point to the attribute value */` |
|      652 |  4899 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|      325 |  4900 | `					}` |
|      326 |  4901 | `				}` |
|      654 |  4902 | `				if( pObjAttr == 0 ){` |
|        - |  4903 | `					/* No such attribute,load null */` |
|        4 |  4904 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  4905 | `						&pClass->sName,&sName);` |
|        - |  4906 | `					/* Call the __get magic method if available */` |
|        3 |  4907 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  4908 | `				}` |
|      654 |  4909 | `				VmPopOperand(&pTos,1);` |
|        - |  4910 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  4911 | `				 * This is due to the following case:` |
|        - |  4912 | `				 *     (new TestClass())->foo;` |
|        - |  4913 | `				 */` |
|      654 |  4914 | `				pThis->iRef++;` |
|      654 |  4915 | `				PH7_MemObjRelease(pTos);` |
|      654 |  4916 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|      654 |  4917 | `				if( pObjAttr ){` |
|      652 |  4918 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  4919 | `					/* Check attribute access */` |
|      652 |  4920 | `					if( VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,TRUE) ){` |
|        - |  4921 | `						/* Load attribute */` |
|      652 |  4922 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|      652 |  4923 | `						if( pValue ){` |
|      652 |  4924 | `							if( pThis->iRef < 2 ){` |
|        - |  4925 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  4926 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  4927 | `								 */` |
|        3 |  4928 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  4929 | `							}else{` |
|        - |  4930 | `								/* Simple load */` |
|      650 |  4931 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  4932 | `							}` |
|      652 |  4933 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|      650 |  4934 | `								if( pThis->iRef > 1 ){` |
|        - |  4935 | `									/* Load attribute index */` |
|      648 |  4936 | `									pTos->nIdx = pObjAttr->nIdx;` |
|      323 |  4937 | `								}` |
|      324 |  4938 | `							}` |
|      325 |  4939 | `						}` |
|      325 |  4940 | `					}` |
|      325 |  4941 | `				}` |
|        - |  4942 | `				/* Safely unreference the object */` |
|      654 |  4943 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  4944 | `			}` |
|      387 |  4945 | `		}else{` |
|      ! 0 |  4946 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  4947 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4948 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  4949 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  4950 | `		}` |
|      387 |  4951 | `	}else{` |
|        - |  4952 | `		/* Static member access using class name */` |
|       59 |  4953 | `		pNos = pTos;` |
|       59 |  4954 | `		pThis = 0;` |
|       59 |  4955 | `		if( !pInstr->p3 ){` |
|       57 |  4956 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       57 |  4957 | `			pNos--;` |
|        - |  4958 | `#ifdef UNTRUST` |
|        - |  4959 | `			if( pNos < pStack ){` |
|        - |  4960 | `				goto Abort;` |
|        - |  4961 | `			}` |
|        - |  4962 | `#endif` |
|       29 |  4963 | `		}else{` |
|        - |  4964 | `			/* Attribute name already computed */` |
|        3 |  4965 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4966 | `		}` |
|       59 |  4967 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|       59 |  4968 | `			ph7_class *pClass = 0;` |
|       59 |  4969 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  4970 | `				/* Class already instantiated */` |
|      ! 0 |  4971 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      ! 0 |  4972 | `				pClass = pThis->pClass;` |
|      ! 0 |  4973 | `				pThis->iRef++; /* Deffer garbage collection */` |
|      ! 0 |  4974 | `			}else{` |
|        - |  4975 | `				/* Try to extract the target class */` |
|       59 |  4976 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|       88 |  4977 | `					pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pNos->sBlob),` |
|       29 |  4978 | `						SyBlobLength(&pNos->sBlob),FALSE,0);` |
|       29 |  4979 | `				}` |
|        - |  4980 | `			}` |
|       59 |  4981 | `			if( pClass == 0 ){` |
|        - |  4982 | `				/* Undefined class */` |
|      ! 0 |  4983 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  4984 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  4985 | `					);` |
|      ! 0 |  4986 | `				if( !pInstr->p3 ){` |
|      ! 0 |  4987 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  4988 | `				}` |
|      ! 0 |  4989 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4990 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  4991 | `			}else{` |
|       59 |  4992 | `				if( pInstr->iP2 ){` |
|        - |  4993 | `					/* Method call */` |
|       25 |  4994 | `					ph7_class_method *pMeth = 0;` |
|       25 |  4995 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  4996 | `						/* Extract the target method */` |
|       25 |  4997 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       12 |  4998 | `					}` |
|       25 |  4999 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  5000 | `						if( pMeth ){` |
|      ! 0 |  5001 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  5002 | `								&pClass->sName,&sName` |
|        - |  5003 | `								);` |
|      ! 0 |  5004 | `						}else{` |
|      ! 0 |  5005 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5006 | `								&pClass->sName,&sName` |
|        - |  5007 | `								);` |
|        - |  5008 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  5009 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  5010 | `						}` |
|        - |  5011 | `						/* Pop the method name from the stack */` |
|      ! 0 |  5012 | `						if( !pInstr->p3 ){` |
|      ! 0 |  5013 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  5014 | `						}` |
|      ! 0 |  5015 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  5016 | `					}else{` |
|        - |  5017 | `						/* Push method name on the stack */` |
|       25 |  5018 | `						PH7_MemObjRelease(pTos);` |
|       25 |  5019 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       25 |  5020 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5021 | `					}` |
|       25 |  5022 | `					pTos->nIdx = SXU32_HIGH;` |
|       13 |  5023 | `				}else{` |
|        - |  5024 | `					/* Attribute access */` |
|       35 |  5025 | `					ph7_class_attr *pAttr = 0;` |
|        - |  5026 | `					/* Check for special ::class pseudo-constant */` |
|       49 |  5027 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       28 |  5028 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  5029 | `						/* ::class returns the fully qualified class name */` |
|        - |  5030 | `						/* Pop the attribute name from the stack */` |
|       27 |  5031 | `						if( !pInstr->p3 ){` |
|       27 |  5032 | `							VmPopOperand(&pTos,1);` |
|       13 |  5033 | `						}` |
|       27 |  5034 | `						PH7_MemObjRelease(pTos);` |
|        - |  5035 | `						/* Load the class name */` |
|       27 |  5036 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       27 |  5037 | `						pTos->nIdx = SXU32_HIGH;` |
|       14 |  5038 | `					}else{` |
|        - |  5039 | `						/* Extract the target attribute */` |
|        9 |  5040 | `						if( sName.nByte > 0 ){` |
|        9 |  5041 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|        4 |  5042 | `						}` |
|        9 |  5043 | `						if( pAttr == 0 ){` |
|        - |  5044 | `							/* No such attribute,load null */` |
|      ! 0 |  5045 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5046 | `								&pClass->sName,&sName);` |
|        - |  5047 | `							/* Call the __get magic method if available */` |
|      ! 0 |  5048 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  5049 | `						}` |
|        - |  5050 | `						/* Pop the attribute name from the stack */` |
|        9 |  5051 | `						if( !pInstr->p3 ){` |
|        7 |  5052 | `							VmPopOperand(&pTos,1);` |
|        3 |  5053 | `						}` |
|        9 |  5054 | `						PH7_MemObjRelease(pTos);` |
|        9 |  5055 | `						pTos->nIdx = SXU32_HIGH;` |
|        9 |  5056 | `						if( pAttr ){` |
|        9 |  5057 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  5058 | `								/* Access to a non static attribute */` |
|      ! 0 |  5059 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5060 | `									&pClass->sName,&pAttr->sName` |
|        - |  5061 | `									);` |
|      ! 0 |  5062 | `							}else{` |
|        - |  5063 | `								ph7_value *pValue;` |
|        - |  5064 | `								/* Check if the access to the attribute is allowed */` |
|        9 |  5065 | `								if( VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,TRUE) ){` |
|        - |  5066 | `									/* Load the desired attribute */` |
|        9 |  5067 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|        9 |  5068 | `									if( pValue ){` |
|        9 |  5069 | `										PH7_MemObjLoad(pValue,pTos);` |
|        9 |  5070 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  5071 | `											/* Load index number */` |
|        3 |  5072 | `											pTos->nIdx = pAttr->nIdx;` |
|        1 |  5073 | `										}` |
|        4 |  5074 | `									}` |
|        4 |  5075 | `								}` |
|        - |  5076 | `							}` |
|        4 |  5077 | `						}` |
|        - |  5078 | `					}` |
|        - |  5079 | `				}` |
|       59 |  5080 | `				if( pThis ){` |
|        - |  5081 | `					/* Safely unreference the object */` |
|      ! 0 |  5082 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  5083 | `				}` |
|        - |  5084 | `			}` |
|       30 |  5085 | `		}else{` |
|        - |  5086 | `			/* Pop operands */` |
|      ! 0 |  5087 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  5088 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5089 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  5090 | `			}` |
|      ! 0 |  5091 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5092 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5093 | `		}` |
|        - |  5094 | `	}` |
|      830 |  5095 | `	break;` |
|        - |  5096 | `					}` |
|        - |  5097 | `/*` |
|        - |  5098 | ` * OP_NEW P1 * * *` |
|        - |  5099 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  5100 | ` */` |
|      247 |  5101 | `case PH7_OP_NEW: {` |
|      496 |  5102 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      496 |  5103 | `	ph7_class *pClass = 0;` |
|        - |  5104 | `	ph7_class_instance *pNew;` |
|      496 |  5105 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  5106 | `		/* Try to extract the desired class */` |
|      743 |  5107 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      494 |  5108 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      247 |  5109 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5110 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  5111 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  5112 | `	}` |
|      496 |  5113 | `	if( pClass == 0 ){` |
|        - |  5114 | `		/* No such class */` |
|      ! 0 |  5115 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined,PH7 is loading NULL",` |
|      ! 0 |  5116 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  5117 | `			);` |
|      ! 0 |  5118 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5119 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5120 | `			/* Pop given arguments */` |
|      ! 0 |  5121 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5122 | `		}` |
|      ! 0 |  5123 | `	}else{` |
|        - |  5124 | `		ph7_class_method *pCons;` |
|        - |  5125 | `		/* Create a new class instance */` |
|      496 |  5126 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      496 |  5127 | `		if( pNew == 0 ){` |
|      ! 0 |  5128 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5129 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  5130 | `				&pClass->sName` |
|        - |  5131 | `			);` |
|      ! 0 |  5132 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5133 | `			if( pInstr->iP1 > 0 ){` |
|        - |  5134 | `				/* Pop given arguments */` |
|      ! 0 |  5135 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5136 | `			}` |
|      ! 0 |  5137 | `			break;` |
|        - |  5138 | `		}` |
|        - |  5139 | `		/* Check if a constructor is available */` |
|      496 |  5140 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      496 |  5141 | `		if( pCons == 0 ){` |
|      444 |  5142 | `			SyString *pName = &pClass->sName;` |
|        - |  5143 | `			/* Check for a constructor with the same base class name */` |
|      444 |  5144 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      221 |  5145 | `		}` |
|      496 |  5146 | `		if( pCons ){` |
|        - |  5147 | `			/* Call the class constructor */` |
|       54 |  5148 | `			SySetReset(&aArg);` |
|       96 |  5149 | `			while( pArg < pTos ){` |
|       44 |  5150 | `				SySetPut(&aArg,(const void *)&pArg);` |
|       44 |  5151 | `				pArg++;` |
|        2 |  5152 | `			}` |
|       54 |  5153 | `			if( pVm->bErrReport ){` |
|        - |  5154 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  5155 | `				sxu32 n;` |
|       12 |  5156 | `				n = SySetUsed(&aArg);` |
|        - |  5157 | `				/* Emit a notice for missing arguments */` |
|       28 |  5158 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       18 |  5159 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       18 |  5160 | `					if( pFuncArg ){` |
|       18 |  5161 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  5162 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  5163 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  5164 | `						}` |
|        8 |  5165 | `					}` |
|       18 |  5166 | `					n++;` |
|        2 |  5167 | `				}` |
|        5 |  5168 | `			}` |
|       54 |  5169 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5170 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|       54 |  5171 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  5172 | `				pNew->iRef = 1;` |
|      ! 0 |  5173 | `			}` |
|       26 |  5174 | `		}` |
|      496 |  5175 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5176 | `			/* Pop given arguments */` |
|       38 |  5177 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       18 |  5178 | `		}` |
|      496 |  5179 | `		PH7_MemObjRelease(pTos);` |
|      496 |  5180 | `		pTos->x.pOther = pNew;` |
|      496 |  5181 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5182 | `	}` |
|      496 |  5183 | `	break;` |
|        - |  5184 | `				 }` |
|        - |  5185 | `/*` |
|        - |  5186 | ` * OP_CLONE * * *` |
|        - |  5187 | ` * Perfome a clone operation.` |
|        - |  5188 | ` */` |
|       23 |  5189 | `case PH7_OP_CLONE: {` |
|        - |  5190 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  5191 | `#ifdef UNTRUST` |
|        - |  5192 | `	if( pTos < pStack ){` |
|        - |  5193 | `		goto Abort;` |
|        - |  5194 | `	}` |
|        - |  5195 | `#endif` |
|        - |  5196 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  5197 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  5198 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5199 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  5200 | `		PH7_MemObjRelease(pTos);` |
|        5 |  5201 | `		break;` |
|        - |  5202 | `	}` |
|        - |  5203 | `	/* Point to the source */` |
|       44 |  5204 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5205 | `	/* Perform the clone operation */` |
|       44 |  5206 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  5207 | `	PH7_MemObjRelease(pTos);` |
|       44 |  5208 | `	if( pClone == 0 ){` |
|      ! 0 |  5209 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5210 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  5211 | `	}else{` |
|        - |  5212 | `		/* Load the cloned object */` |
|       44 |  5213 | `		pTos->x.pOther = pClone;` |
|       44 |  5214 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5215 | `	}` |
|       44 |  5216 | `	break;` |
|        - |  5217 | `				   }` |
|        - |  5218 | `/*` |
|        - |  5219 | ` * OP_SWITCH * * P3` |
|        - |  5220 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  5221 | ` */` |
|       14 |  5222 | `case PH7_OP_SWITCH: {` |
|       29 |  5223 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  5224 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  5225 | `	ph7_value sValue,sCaseValue;` |
|        - |  5226 | `	sxu32 n,nEntry;` |
|        - |  5227 | `#ifdef UNTRUST` |
|        - |  5228 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  5229 | `		goto Abort;` |
|        - |  5230 | `	}` |
|        - |  5231 | `#endif` |
|        - |  5232 | `	/* Point to the case table  */` |
|       29 |  5233 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       29 |  5234 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  5235 | `	/* Select the appropriate case block to execute */` |
|       29 |  5236 | `	PH7_MemObjInit(pVm,&sValue);` |
|       29 |  5237 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|       59 |  5238 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       59 |  5239 | `		pCase = &aCase[n];` |
|       59 |  5240 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  5241 | `		/* Execute the case expression first */` |
|       59 |  5242 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  5243 | `		/* Compare the two expression */` |
|       59 |  5244 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|       59 |  5245 | `		PH7_MemObjRelease(&sValue);` |
|       59 |  5246 | `		PH7_MemObjRelease(&sCaseValue);` |
|       59 |  5247 | `		if( rc == 0 ){` |
|        - |  5248 | `			/* Value match,jump to this block */` |
|       29 |  5249 | `			pc = pCase->nStart - 1;` |
|       29 |  5250 | `			break;` |
|        - |  5251 | `		}` |
|       16 |  5252 | `	}` |
|       29 |  5253 | `	VmPopOperand(&pTos,1);` |
|       29 |  5254 | `	if( n >= nEntry ){` |
|        - |  5255 | `		/* No approprite case to execute,jump to the default case */` |
|      ! 0 |  5256 | `		if( pSwitch->nDefault > 0 ){` |
|      ! 0 |  5257 | `			pc = pSwitch->nDefault - 1;` |
|      ! 0 |  5258 | `		}else{` |
|        - |  5259 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  5260 | `			pc = pSwitch->nOut - 1;` |
|        - |  5261 | `		}` |
|      ! 0 |  5262 | `	}` |
|       29 |  5263 | `	break;` |
|        - |  5264 | `					}` |
|        - |  5265 | `/*` |
|        - |  5266 | ` * OP_CALL P1 * *` |
|        - |  5267 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  5268 | ` *  function on the stack.` |
|        - |  5269 | ` */` |
|   162126 |  5270 | `case PH7_OP_CALL: {` |
|   324257 |  5271 | `	ph7_value *pArg = &pTos[-pInstr->iP1];` |
|        - |  5272 | `	SyHashEntry *pEntry;` |
|        - |  5273 | `	SyString sName;` |
|        - |  5274 | `	/* Extract function name */` |
|   324257 |  5275 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  5276 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5277 | `			ph7_value sResult;` |
|      ! 0 |  5278 | `			SySetReset(&aArg);` |
|      ! 0 |  5279 | `			while( pArg < pTos ){` |
|      ! 0 |  5280 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  5281 | `				pArg++;` |
|      ! 0 |  5282 | `			}` |
|      ! 0 |  5283 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  5284 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  5285 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  5286 | `			SySetReset(&aArg);` |
|        - |  5287 | `			/* Pop given arguments */` |
|      ! 0 |  5288 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5289 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5290 | `			}` |
|        - |  5291 | `			/* Copy result */` |
|      ! 0 |  5292 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  5293 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  5294 | `		}else{` |
|        3 |  5295 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  5296 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5297 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  5298 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  5299 | `			}else{` |
|        - |  5300 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  5301 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  5302 | `			}` |
|        - |  5303 | `			/* Pop given arguments */` |
|        3 |  5304 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5305 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5306 | `			}` |
|        - |  5307 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5308 | `			PH7_MemObjRelease(pTos);` |
|        - |  5309 | `		}` |
|   162126 |  5310 | `		break;` |
|        - |  5311 | `	}` |
|   324255 |  5312 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  5313 | `	/* Check for a compiled function first */` |
|   324255 |  5314 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|   324255 |  5315 | `	if( pEntry ){` |
|        - |  5316 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  5317 | `		ph7_class_instance *pThis;` |
|        - |  5318 | `		ph7_value *pFrameStack;` |
|        - |  5319 | `		ph7_vm_func *pVmFunc;` |
|        - |  5320 | `		ph7_class *pSelf;` |
|        - |  5321 | `		VmFrame *pFrame;` |
|        - |  5322 | `		ph7_value *pObj;` |
|        - |  5323 | `		VmSlot sArg;` |
|        - |  5324 | `		sxu32 n;` |
|        - |  5325 | `		/* initialize fields */` |
|     5542 |  5326 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|     5542 |  5327 | `		pThis = 0;` |
|     5542 |  5328 | `		pSelf = 0;` |
|     5542 |  5329 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  5330 | `			ph7_class_method *pMeth;` |
|        - |  5331 | `			/* Class method call */` |
|      346 |  5332 | `			ph7_value *pTarget = &pTos[-1];` |
|      346 |  5333 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  5334 | `				/* Extract the 'this' pointer */` |
|      346 |  5335 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  5336 | `					/* Instance already loaded */` |
|      316 |  5337 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|      316 |  5338 | `					pThis->iRef++;` |
|      316 |  5339 | `					pSelf = pThis->pClass;` |
|      157 |  5340 | `				}` |
|      346 |  5341 | `				if( pSelf == 0 ){` |
|       31 |  5342 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  5343 | `						/* "Late Static Binding" class name */` |
|       37 |  5344 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       12 |  5345 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       12 |  5346 | `					}` |
|       31 |  5347 | `					if( pSelf == 0 ){` |
|        7 |  5348 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        3 |  5349 | `					}` |
|       15 |  5350 | `				}` |
|      346 |  5351 | `				if( pThis == 0  ){` |
|       31 |  5352 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       31 |  5353 | `					while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  5354 | `						/* Safely ignore the exception frame */` |
|      ! 0 |  5355 | `						pFrameLocal = pFrameLocal->pParent;` |
|      ! 0 |  5356 | `					}` |
|       31 |  5357 | `					if( pFrameLocal->pParent ){` |
|        - |  5358 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       19 |  5359 | `						pThis = pFrameLocal->pThis;` |
|       19 |  5360 | `						if( pThis ){` |
|       13 |  5361 | `							pThis->iRef++;` |
|        6 |  5362 | `						}` |
|        9 |  5363 | `					}` |
|       15 |  5364 | `				}` |
|      346 |  5365 | `				VmPopOperand(&pTos,1);` |
|      346 |  5366 | `				PH7_MemObjRelease(pTos);` |
|        - |  5367 | `				/* Synchronize pointers */` |
|      346 |  5368 | `				pArg = &pTos[-pInstr->iP1];` |
|        - |  5369 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  5370 | `				 * user have already computed the random generated unique class method name` |
|        - |  5371 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  5372 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  5373 | `				 */` |
|      346 |  5374 | `				while( pArg < pStack ){` |
|      ! 0 |  5375 | `					pArg++;` |
|      ! 0 |  5376 | `				}` |
|      346 |  5377 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  5378 | `					/* Check if the call is allowed */` |
|      346 |  5379 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|      346 |  5380 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        5 |  5381 | `						if( !VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,TRUE) ){` |
|        - |  5382 | `							/* Pop given arguments */` |
|      ! 0 |  5383 | `							if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5384 | `								VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5385 | `							}` |
|        - |  5386 | `							/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5387 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  5388 | `							break;` |
|        - |  5389 | `						}` |
|        2 |  5390 | `					}` |
|      172 |  5391 | `				}` |
|      172 |  5392 | `			}` |
|      172 |  5393 | `		}` |
|        - |  5394 | `		/* Check The recursion limit */` |
|     5542 |  5395 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  5396 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5397 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  5398 | `				&pVmFunc->sName);` |
|        - |  5399 | `			/* Pop given arguments */` |
|        3 |  5400 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5401 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5402 | `			}` |
|        - |  5403 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5404 | `			PH7_MemObjRelease(pTos);` |
|        3 |  5405 | `			break;` |
|        - |  5406 | `		}` |
|     5540 |  5407 | `		if( pVmFunc->pNextName ){` |
|        - |  5408 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      129 |  5409 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       64 |  5410 | `		}` |
|        - |  5411 | `		/* Extract the formal argument set */` |
|     5540 |  5412 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  5413 | `		/* Create a new VM frame  */` |
|     5540 |  5414 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|     5540 |  5415 | `		if( rc != SXRET_OK ){` |
|        - |  5416 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5417 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5418 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5419 | `				&pVmFunc->sName);` |
|        - |  5420 | `			/* Pop given arguments */` |
|      ! 0 |  5421 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5422 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5423 | `			}` |
|        - |  5424 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5425 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5426 | `			break;` |
|        - |  5427 | `		}` |
|     5540 |  5428 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  5429 | `			/* Install the '$this' variable */` |
|        - |  5430 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|      326 |  5431 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|      326 |  5432 | `			if( pObj ){` |
|        - |  5433 | `				/* Reflect the change */` |
|      326 |  5434 | `				pObj->x.pOther = pThis;` |
|      326 |  5435 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      162 |  5436 | `			}` |
|      162 |  5437 | `		}` |
|     5540 |  5438 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  5439 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  5440 | `			/* Install static variables */` |
|      ! 0 |  5441 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  5442 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  5443 | `				pStatic = &aStatic[n];` |
|      ! 0 |  5444 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  5445 | `					/* Initialize the static variables */` |
|      ! 0 |  5446 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  5447 | `					if( pObj ){` |
|        - |  5448 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  5449 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  5450 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  5451 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  5452 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  5453 | `						}` |
|      ! 0 |  5454 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  5455 | `					}else{` |
|      ! 0 |  5456 | `						continue;` |
|        - |  5457 | `					}` |
|      ! 0 |  5458 | `				}` |
|        - |  5459 | `				/* Install in the current frame */` |
|      ! 0 |  5460 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  5461 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  5462 | `			}` |
|      ! 0 |  5463 | `		}` |
|        - |  5464 | `		/* Push arguments in the local frame */` |
|     5540 |  5465 | `		n = 0;` |
|    15588 |  5466 | `		while( pArg < pTos ){` |
|    10050 |  5467 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     9950 |  5468 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  5469 | `					/* NULL values are redirected to default arguments */` |
|      661 |  5470 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      661 |  5471 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5472 | `						goto Abort;` |
|        - |  5473 | `					}` |
|      330 |  5474 | `				}` |
|        - |  5475 | `				/* Make sure the given arguments are of the correct type */` |
|     9950 |  5476 | `				if( aFormalArg[n].nType > 0 ){` |
|      898 |  5477 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  5478 | `						/* Argument must be a class instance [i.e: object] */` |
|      ! 0 |  5479 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  5480 | `						ph7_class *pClass;` |
|        - |  5481 | `						/* Try to extract the desired class */` |
|      ! 0 |  5482 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  5483 | `						if( pClass ){` |
|      ! 0 |  5484 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  5485 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5486 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5487 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5488 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5489 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5490 | `								}` |
|      ! 0 |  5491 | `							}else{` |
|        - |  5492 | `								/* reuse pThis declared in outer scope */` |
|      ! 0 |  5493 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  5494 | `								/* Make sure the object is an instance of the given class */` |
|      ! 0 |  5495 | `								if( ! VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  5496 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5497 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5498 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5499 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5500 | `								}` |
|        - |  5501 | `							}` |
|      ! 0 |  5502 | `						}` |
|      898 |  5503 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5504 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5505 | `						/* Cast to the desired type */` |
|      ! 0 |  5506 | `						xCast(pArg);` |
|      ! 0 |  5507 | `					}` |
|      448 |  5508 | `				}` |
|     9950 |  5509 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  5510 | `					/* Pass by reference */` |
|       25 |  5511 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  5512 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  5513 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  5514 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5515 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  5516 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  5517 | `						}` |
|        - |  5518 | `						/* Switch to pass by value */` |
|      ! 0 |  5519 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  5520 | `					}else{` |
|        - |  5521 | `						SyHashEntry *pRefEntry;` |
|        - |  5522 | `						/* Install the referenced variable in the private function frame */` |
|       25 |  5523 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       25 |  5524 | `						if( pRefEntry == 0 ){` |
|       37 |  5525 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       24 |  5526 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       25 |  5527 | `							sArg.nIdx = pArg->nIdx;` |
|       25 |  5528 | `							sArg.pUserData = 0;` |
|       25 |  5529 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       12 |  5530 | `						}` |
|       25 |  5531 | `						pObj = 0;` |
|        - |  5532 | `					}` |
|       13 |  5533 | `				}else{` |
|        - |  5534 | `					/* Pass by value,make a copy of the given argument */` |
|     9926 |  5535 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  5536 | `				}` |
|     4976 |  5537 | `			}else{` |
|        - |  5538 | `				char zName[32];` |
|        - |  5539 | `				SyString sArgName;` |
|        - |  5540 | `				/* Set a dummy name */` |
|      101 |  5541 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      101 |  5542 | `				sArgName.zString = zName;` |
|        - |  5543 | `				/* Annonymous argument */` |
|      101 |  5544 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  5545 | `			}` |
|    10050 |  5546 | `			if( pObj ){` |
|    10026 |  5547 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  5548 | `				/* Insert argument index  */` |
|    10026 |  5549 | `				sArg.nIdx = pObj->nIdx;` |
|    10026 |  5550 | `				sArg.pUserData = 0;` |
|    10026 |  5551 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|     5012 |  5552 | `			}` |
|    10050 |  5553 | `			PH7_MemObjRelease(pArg);` |
|    10050 |  5554 | `			pArg++;` |
|    10050 |  5555 | `			++n;` |
|        2 |  5556 | `		}` |
|        - |  5557 | `		/* Set up closure environment */` |
|     5540 |  5558 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  5559 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  5560 | `			ph7_value *pValue;` |
|        - |  5561 | `			sxu32 iEnv;` |
|        9 |  5562 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|       25 |  5563 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|       17 |  5564 | `				pEnv = &aEnv[iEnv];` |
|       17 |  5565 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  5566 | `					/* Do not install null value */` |
|        9 |  5567 | `					continue;` |
|        - |  5568 | `				}` |
|        9 |  5569 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|        9 |  5570 | `				if( pValue == 0 ){` |
|      ! 0 |  5571 | `					continue;` |
|        - |  5572 | `				}` |
|        - |  5573 | `				/* Invalidate any prior representation */` |
|        9 |  5574 | `				PH7_MemObjRelease(pValue);` |
|        - |  5575 | `				/* Duplicate bound variable value */` |
|        9 |  5576 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|        5 |  5577 | `			}` |
|        4 |  5578 | `		}` |
|        - |  5579 | `		/* Process default values */` |
|     6240 |  5580 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|      702 |  5581 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|      692 |  5582 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      692 |  5583 | `				if( pObj ){` |
|        - |  5584 | `					/* Evaluate the default value and extract it's result */` |
|      692 |  5585 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|      692 |  5586 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5587 | `						goto Abort;` |
|        - |  5588 | `					}` |
|        - |  5589 | `					/* Insert argument index */` |
|      692 |  5590 | `					sArg.nIdx = pObj->nIdx;` |
|      692 |  5591 | `					sArg.pUserData = 0;` |
|      692 |  5592 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  5593 | `					/* Make sure the default argument is of the correct type */` |
|      692 |  5594 | `					if( aFormalArg[n].nType > 0 && ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5595 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5596 | `						/* Cast to the desired type */` |
|      ! 0 |  5597 | `						xCast(pObj);` |
|      ! 0 |  5598 | `					}` |
|      345 |  5599 | `				}` |
|      345 |  5600 | `			}` |
|      702 |  5601 | `			++n;` |
|        2 |  5602 | `		}` |
|        - |  5603 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  5604 | `		 * does not return anything.` |
|        - |  5605 | `		 */` |
|     5540 |  5606 | `		PH7_MemObjRelease(pTos);` |
|     5540 |  5607 | `		pTos = &pTos[-pInstr->iP1];` |
|        - |  5608 | `		/* Allocate a new operand stack and evaluate the function body */` |
|     5540 |  5609 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|     5540 |  5610 | `		if( pFrameStack == 0 ){` |
|        - |  5611 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5612 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5613 | `				&pVmFunc->sName);` |
|      ! 0 |  5614 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5615 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5616 | `			}` |
|      ! 0 |  5617 | `			break;` |
|        - |  5618 | `		}` |
|     5540 |  5619 | `		if( pSelf ){` |
|        - |  5620 | `			/* Push class name */` |
|      344 |  5621 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|      171 |  5622 | `		}` |
|        - |  5623 | `		/* Increment nesting level */` |
|     5540 |  5624 | `		pVm->nRecursionDepth++;` |
|        - |  5625 | `		/* Execute function body */` |
|     5540 |  5626 | `		rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE);` |
|        - |  5627 | `		/* Decrement nesting level */` |
|     5540 |  5628 | `		pVm->nRecursionDepth--;` |
|     5540 |  5629 | `		if( pSelf ){` |
|        - |  5630 | `			/* Pop class name */` |
|      344 |  5631 | `			(void)SySetPop(&pVm->aSelf);` |
|      171 |  5632 | `		}` |
|        - |  5633 | `		/* Cleanup the mess left behind */` |
|     5540 |  5634 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  5635 | `			/* Return by reference,reflect that */` |
|        9 |  5636 | `			if( n != SXU32_HIGH ){` |
|        9 |  5637 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  5638 | `				sxu32 i;` |
|        - |  5639 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  5640 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  5641 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  5642 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  5643 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5644 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  5645 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  5646 | `								&pVmFunc->sName);` |
|      ! 0 |  5647 | `						}` |
|      ! 0 |  5648 | `						n = SXU32_HIGH;` |
|      ! 0 |  5649 | `						break;` |
|        - |  5650 | `					}` |
|        3 |  5651 | `				}` |
|        5 |  5652 | `			}else{` |
|      ! 0 |  5653 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5654 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  5655 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  5656 | `						&pVmFunc->sName);` |
|      ! 0 |  5657 | `				}` |
|        - |  5658 | `			}` |
|        9 |  5659 | `			pTos->nIdx = n;` |
|        4 |  5660 | `		}` |
|        - |  5661 | `		/* Cleanup the mess left behind */` |
|     5540 |  5662 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  5663 | `			/* An exception was throw in this frame */` |
|        7 |  5664 | `			pFrame = pFrame->pParent;` |
|        7 |  5665 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  5666 | `				/* Pop the resutlt */` |
|        5 |  5667 | `				VmPopOperand(&pTos,1);` |
|        - |  5668 | `				/* Jump to this destination */` |
|        5 |  5669 | `				pc = pFrame->iExceptionJump - 1;` |
|        5 |  5670 | `				rc = PH7_OK;` |
|        3 |  5671 | `			}else{` |
|        3 |  5672 | `				if( pFrame->pParent ){` |
|        3 |  5673 | `					rc = PH7_EXCEPTION;` |
|        2 |  5674 | `				}else{` |
|        - |  5675 | `					/* Continue normal execution */` |
|      ! 0 |  5676 | `					rc = PH7_OK;` |
|        - |  5677 | `				}` |
|        - |  5678 | `			}` |
|        3 |  5679 | `		}` |
|        - |  5680 | `		/* Free the operand stack */` |
|     5540 |  5681 | `		SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|        - |  5682 | `		/* Leave the frame */` |
|     5540 |  5683 | `		VmLeaveFrame(&(*pVm));` |
|     5540 |  5684 | `		if( rc == PH7_ABORT ){` |
|        - |  5685 | `			/* Abort processing immeditaley */` |
|      ! 0 |  5686 | `			goto Abort;` |
|     5540 |  5687 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  5688 | `			goto Exception;` |
|        - |  5689 | `		}` |
|     2770 |  5690 | `	}else{` |
|        - |  5691 | `		ph7_user_func *pFunc;` |
|        - |  5692 | `		ph7_context sCtx;` |
|        - |  5693 | `		ph7_value sRet;` |
|        - |  5694 | `		/* Look for an installed foreign function */` |
|   318715 |  5695 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   318715 |  5696 | `		if( pEntry == 0 ){` |
|        - |  5697 | `			/* Call to undefined function */` |
|        5 |  5698 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  5699 | `			/* Pop given arguments */` |
|        5 |  5700 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5701 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5702 | `			}` |
|        - |  5703 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  5704 | `			PH7_MemObjRelease(pTos);` |
|        5 |  5705 | `			break;` |
|        - |  5706 | `		}` |
|   318711 |  5707 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  5708 | `		/* Start collecting function arguments */` |
|   318711 |  5709 | `		SySetReset(&aArg);` |
|   886540 |  5710 | `		while( pArg < pTos ){` |
|   567831 |  5711 | `			SySetPut(&aArg,(const void *)&pArg);` |
|   567831 |  5712 | `			pArg++;` |
|        2 |  5713 | `		}` |
|        - |  5714 | `		/* Assume a null return value */` |
|   318711 |  5715 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  5716 | `		/* Init the call context */` |
|   318711 |  5717 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  5718 | `		/* Call the foreign function */` |
|   318711 |  5719 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5720 | `		/* Release the call context */` |
|   318711 |  5721 | `		VmReleaseCallContext(&sCtx);` |
|   318711 |  5722 | `		if( rc == PH7_ABORT ){` |
|        3 |  5723 | `			goto Abort;` |
|        - |  5724 | `		}` |
|   318709 |  5725 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5726 | `			/* Pop function name and arguments */` |
|   304255 |  5727 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   152128 |  5728 | `		}` |
|        - |  5729 | `		/* Save foreign function return value */` |
|   318709 |  5730 | `		PH7_MemObjStore(&sRet,pTos);` |
|   318709 |  5731 | `		PH7_MemObjRelease(&sRet);` |
|        - |  5732 | `	}` |
|   324245 |  5733 | `	break;` |
|        - |  5734 | `				  }` |
|        - |  5735 | `/*` |
|        - |  5736 | ` * OP_CONSUME: P1 * *` |
|        - |  5737 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  5738 | ` */` |
|     7846 |  5739 | `case PH7_OP_CONSUME: {` |
|    15694 |  5740 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    15694 |  5741 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  5742 |  |
|    15694 |  5743 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    15694 |  5744 | `	pCur = pOut;` |
|        - |  5745 | `	/* Start the consume process  */` |
|    31386 |  5746 | `	while( pOut <= pTos ){` |
|        - |  5747 | `		/* Force a string cast */` |
|    15694 |  5748 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|       56 |  5749 | `			PH7_MemObjToString(pOut);` |
|       27 |  5750 | `		}` |
|    15694 |  5751 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  5752 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  5753 | `			/* Invoke the output consumer callback */` |
|     8302 |  5754 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|     8302 |  5755 | `			if( pCons->xConsumer != VmObConsumer ){` |
|        - |  5756 | `				/* Increment output length */` |
|     2868 |  5757 | `				pVm->nOutputLen += SyBlobLength(&pOut->sBlob);` |
|     1433 |  5758 | `			}` |
|     8302 |  5759 | `			SyBlobRelease(&pOut->sBlob);` |
|     8302 |  5760 | `			if( rc == SXERR_ABORT ){` |
|        - |  5761 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  5762 | `				goto Abort;` |
|        - |  5763 | `			}` |
|     4150 |  5764 | `		}` |
|    15694 |  5765 | `		pOut++;` |
|        2 |  5766 | `	}` |
|    15694 |  5767 | `	pTos = &pCur[-1];` |
|    15692 |  5768 | `	break;` |
|        - |  5769 | `					 }` |
|        - |  5770 |  |
|        - |  5771 | `		} /* Switch() */` |
|  4445284 |  5772 | `		pc++; /* Next instruction in the stream */` |
|        2 |  5773 | `	} /* For(;;) */` |
|     8757 |  5774 | `Done:` |
|    17516 |  5775 | `	SySetRelease(&aArg);` |
|    17516 |  5776 | `	return SXRET_OK;` |
|        2 |  5777 | `Abort:` |
|        5 |  5778 | `	SySetRelease(&aArg);` |
|       11 |  5779 | `	while( pTos >= pStack ){` |
|        7 |  5780 | `		PH7_MemObjRelease(pTos);` |
|        7 |  5781 | `		pTos--;` |
|        1 |  5782 | `	}` |
|        5 |  5783 | `	return PH7_ABORT;` |
|        1 |  5784 | `Exception:` |
|        3 |  5785 | `	SySetRelease(&aArg);` |
|        5 |  5786 | `	while( pTos >= pStack ){` |
|        3 |  5787 | `		PH7_MemObjRelease(pTos);` |
|        3 |  5788 | `		pTos--;` |
|        1 |  5789 | `	}` |
|        3 |  5790 | `	return PH7_EXCEPTION;` |
|     8762 |  5791 |  |
|        - |  5792 | `/*` |
|        - |  5793 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  5794 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  5795 | ` * See block-comment on that function for additional information.` |
|        - |  5796 | ` */` |
|    10566 |  5797 | `static sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  5798 |  |
|        - |  5799 | `	ph7_value *pStack;` |
|        - |  5800 | `	sxi32 rc;` |
|        - |  5801 | `	/* Allocate a new operand stack */` |
|    10568 |  5802 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    10568 |  5803 | `	if( pStack == 0 ){` |
|      ! 0 |  5804 | `		return SXERR_MEM;` |
|        - |  5805 | `	}` |
|        - |  5806 | `	/* Execute the program */` |
|    10568 |  5807 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE);` |
|        - |  5808 | `	/* Free the operand stack */` |
|    10568 |  5809 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  5810 | `	/* Execution result */` |
|    10568 |  5811 | `	return rc;` |
|     5285 |  5812 |  |
|        - |  5813 | `/*` |
|        - |  5814 | ` * Invoke any installed shutdown callbacks.` |
|        - |  5815 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  5816 | ` * or more calls to [register_shutdown_function()].` |
|        - |  5817 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  5818 | ` * execution ends.` |
|        - |  5819 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  5820 | ` * additional information.` |
|        - |  5821 | ` */` |
|      916 |  5822 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  5823 |  |
|        - |  5824 | `	VmShutdownCB *pEntry;` |
|        - |  5825 | `	ph7_value *apArg[10];` |
|        - |  5826 | `	sxu32 n,nEntry;` |
|        - |  5827 | `	int i;` |
|        - |  5828 | `	/* Point to the stack of registered callbacks */` |
|      918 |  5829 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    10078 |  5830 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|     9162 |  5831 | `		apArg[i] = 0;` |
|     4582 |  5832 | `	}` |
|      920 |  5833 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  5834 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  5835 | `		if( pEntry ){` |
|        - |  5836 | `			/* Prepare callback arguments if any */` |
|        3 |  5837 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  5838 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  5839 | `					break;` |
|        - |  5840 | `				}` |
|      ! 0 |  5841 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  5842 | `			}` |
|        - |  5843 | `			/* Invoke the callback */` |
|        3 |  5844 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  5845 | `			/*` |
|        - |  5846 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  5847 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  5848 | `			 */` |
|        3 |  5849 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  5850 | `			if( pEntry ){` |
|        3 |  5851 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  5852 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  5853 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  5854 | `				}` |
|        1 |  5855 | `			}` |
|        1 |  5856 | `		}` |
|        2 |  5857 | `	}` |
|      918 |  5858 | `	SySetReset(&pVm->aShutdown);` |
|      918 |  5859 |  |
|        - |  5860 | `/*` |
|        - |  5861 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  5862 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  5863 | ` * See block-comment on that function for additional information.` |
|        - |  5864 | ` */` |
|      924 |  5865 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  5866 |  |
|        - |  5867 | `	/* Make sure we are ready to execute this program */` |
|      926 |  5868 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  5869 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  5870 | `	}` |
|        - |  5871 | `	/* Set the execution magic number  */` |
|      926 |  5872 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  5873 | `	/* Execute the program */` |
|      926 |  5874 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE);` |
|        - |  5875 | `	/* Invoke any shutdown callbacks */` |
|      922 |  5876 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  5877 | `	/*` |
|        - |  5878 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  5879 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  5880 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  5881 | `	 */` |
|      922 |  5882 | `	return SXRET_OK;` |
|      464 |  5883 |  |
|        - |  5884 | `/*` |
|        - |  5885 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  5886 | ` * the desired message.` |
|        - |  5887 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  5888 | ` * in 'api.c' for additional information.` |
|        - |  5889 | ` */` |
|      380 |  5890 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  5891 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  5892 | `	SyString *pString /* Message to output */` |
|        - |  5893 | `	)` |
|        2 |  5894 |  |
|      382 |  5895 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      382 |  5896 | `	sxi32 rc = SXRET_OK;` |
|        - |  5897 | `	/* Call the output consumer */` |
|      382 |  5898 | `	if( pString->nByte > 0 ){` |
|      382 |  5899 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      382 |  5900 | `		if( pCons->xConsumer != VmObConsumer ){` |
|        - |  5901 | `			/* Increment output length */` |
|       17 |  5902 | `			pVm->nOutputLen += pString->nByte;` |
|        8 |  5903 | `		}` |
|      190 |  5904 | `	}` |
|      382 |  5905 | `	return rc;` |
|        2 |  5906 |  |
|        - |  5907 | `/*` |
|        - |  5908 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  5909 | ` * callback to consume the formatted message.` |
|        - |  5910 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  5911 | ` * in 'api.c' for additional information.` |
|        - |  5912 | ` */` |
|        2 |  5913 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  5914 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  5915 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  5916 | `	va_list ap           /* Variable list of arguments */` |
|        - |  5917 | `	)` |
|        1 |  5918 |  |
|        3 |  5919 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  5920 | `	sxi32 rc = SXRET_OK;` |
|        - |  5921 | `	SyBlob sWorker;` |
|        - |  5922 | `	/* Format the message and call the output consumer */` |
|        3 |  5923 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  5924 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  5925 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  5926 | `		/* Consume the formatted message */` |
|        3 |  5927 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  5928 | `	}` |
|        3 |  5929 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  5930 | `		/* Increment output length */` |
|      ! 0 |  5931 | `		pVm->nOutputLen += SyBlobLength(&sWorker);` |
|      ! 0 |  5932 | `	}` |
|        - |  5933 | `	/* Release the working buffer */` |
|        3 |  5934 | `	SyBlobRelease(&sWorker);` |
|        3 |  5935 | `	return rc;` |
|        1 |  5936 |  |
|        - |  5937 | `/*` |
|        - |  5938 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  5939 | ` * This function never fail and always return a pointer` |
|        - |  5940 | ` * to a null terminated string.` |
|        - |  5941 | ` */` |
|       10 |  5942 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  5943 |  |
|       11 |  5944 | `	const char *zOp = "Unknown     ";` |
|       11 |  5945 | `	switch(nOp){` |
|        3 |  5946 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  5947 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  5948 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  5949 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  5950 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  5951 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  5952 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  5953 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  5954 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  5955 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  5956 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  5957 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  5958 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  5959 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  5960 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  5961 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  5962 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  5963 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  5964 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  5965 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  5966 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  5967 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  5968 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  5969 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  5970 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  5971 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  5972 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  5973 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  5974 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  5975 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  5976 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  5977 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  5978 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  5979 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  5980 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  5981 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  5982 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  5983 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  5984 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  5985 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  5986 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  5987 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  5988 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  5989 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  5990 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  5991 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  5992 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  5993 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  5994 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  5995 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  5996 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  5997 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  5998 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  5999 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  6000 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  6001 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  6002 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  6003 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  6004 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  6005 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  6006 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  6007 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  6008 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  6009 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  6010 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  6011 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  6012 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  6013 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  6014 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  6015 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  6016 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  6017 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  6018 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  6019 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  6020 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  6021 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  6022 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  6023 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  6024 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  6025 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  6026 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  6027 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  6028 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  6029 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  6030 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  6031 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  6032 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  6033 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  6034 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  6035 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  6036 | `	default:` |
|      ! 0 |  6037 | `		break;` |
|        - |  6038 | `	}` |
|       11 |  6039 | `	return zOp;` |
|        1 |  6040 |  |
|        - |  6041 | `/*` |
|        - |  6042 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  6043 | ` * The xConsumer() callback which is an used defined function` |
|        - |  6044 | ` * is responsible of consuming the generated dump.` |
|        - |  6045 | ` */` |
|        2 |  6046 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  6047 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  6048 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  6049 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  6050 | `	)` |
|        1 |  6051 |  |
|        - |  6052 | `	sxi32 rc;` |
|        3 |  6053 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  6054 | `	return rc;` |
|        1 |  6055 |  |
|        - |  6056 | `/*` |
|        - |  6057 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  6058 | ` * outside a class body [i.e: global or function scope].` |
|        - |  6059 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  6060 | ` * in 'compile.c' for additional information.` |
|        - |  6061 | ` */` |
|        8 |  6062 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  6063 |  |
|        9 |  6064 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  6065 | `	/* Evaluate and expand constant value */` |
|        9 |  6066 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|        9 |  6067 |  |
|        - |  6068 | `/*` |
|        - |  6069 | ` * Section:` |
|        - |  6070 | ` *  Function handling functions.` |
|        - |  6071 | ` * Status:` |
|        - |  6072 | ` *    Stable.` |
|        - |  6073 | ` */` |
|        - |  6074 | `/*` |
|        - |  6075 | ` * int func_num_args(void)` |
|        - |  6076 | ` *   Returns the number of arguments passed to the function.` |
|        - |  6077 | ` * Parameters` |
|        - |  6078 | ` *   None.` |
|        - |  6079 | ` * Return` |
|        - |  6080 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  6081 | ` *  or -1 if called from the globe scope.` |
|        - |  6082 | ` */` |
|      666 |  6083 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6084 |  |
|        - |  6085 | `	VmFrame *pFrame;` |
|        - |  6086 | `	ph7_vm *pVm;` |
|        - |  6087 | `	/* Point to the target VM */` |
|      667 |  6088 | `	pVm = pCtx->pVm;` |
|        - |  6089 | `	/* Current frame */` |
|      667 |  6090 | `	pFrame = pVm->pFrame;` |
|      667 |  6091 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6092 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6093 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6094 | `	}` |
|      667 |  6095 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  6096 | `		SXUNUSED(nArg);` |
|      ! 0 |  6097 | `		SXUNUSED(apArg);` |
|        - |  6098 | `		/* Global frame,return -1 */` |
|      ! 0 |  6099 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  6100 | `		return SXRET_OK;` |
|        - |  6101 | `	}` |
|        - |  6102 | `	/* Total number of arguments passed to the enclosing function */` |
|      667 |  6103 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      667 |  6104 | `	ph7_result_int(pCtx,nArg);` |
|      667 |  6105 | `	return SXRET_OK;` |
|      334 |  6106 |  |
|        - |  6107 | `/*` |
|        - |  6108 | ` * value func_get_arg(int $arg_num)` |
|        - |  6109 | ` *   Return an item from the argument list.` |
|        - |  6110 | ` * Parameters` |
|        - |  6111 | ` *  Argument number(index start from zero).` |
|        - |  6112 | ` * Return` |
|        - |  6113 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  6114 | ` */` |
|        6 |  6115 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6116 |  |
|        8 |  6117 | `	ph7_value *pObj = 0;` |
|        8 |  6118 | `	VmSlot *pSlot = 0;` |
|        - |  6119 | `	VmFrame *pFrame;` |
|        - |  6120 | `	ph7_vm *pVm;` |
|        - |  6121 | `	/* Point to the target VM */` |
|        8 |  6122 | `	pVm = pCtx->pVm;` |
|        - |  6123 | `	/* Current frame */` |
|        8 |  6124 | `	pFrame = pVm->pFrame;` |
|        8 |  6125 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6126 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6127 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6128 | `	}` |
|        8 |  6129 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  6130 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  6131 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  6132 | `		ph7_result_bool(pCtx,0);` |
|        3 |  6133 | `		return SXRET_OK;` |
|        - |  6134 | `	}` |
|        - |  6135 | `	/* Extract the desired index */` |
|        5 |  6136 | `	nArg = ph7_value_to_int(apArg[0]);` |
|        5 |  6137 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  6138 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  6139 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6140 | `		return SXRET_OK;` |
|        - |  6141 | `	}` |
|        - |  6142 | `	/* Extract the desired argument */` |
|        5 |  6143 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|        5 |  6144 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  6145 | `			/* Return the desired argument */` |
|        5 |  6146 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|        3 |  6147 | `		}else{` |
|        - |  6148 | `			/* No such argument,return false */` |
|      ! 0 |  6149 | `			ph7_result_bool(pCtx,0);` |
|        - |  6150 | `		}` |
|        3 |  6151 | `	}else{` |
|        - |  6152 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  6153 | `		ph7_result_bool(pCtx,0);` |
|        - |  6154 | `	}` |
|        5 |  6155 | `	return SXRET_OK;` |
|        5 |  6156 |  |
|        - |  6157 | `/*` |
|        - |  6158 | ` * array func_get_args_byref(void)` |
|        - |  6159 | ` *   Returns an array comprising a function's argument list.` |
|        - |  6160 | ` * Parameters` |
|        - |  6161 | ` *  None.` |
|        - |  6162 | ` * Return` |
|        - |  6163 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  6164 | ` *  member of the current user-defined function's argument list.` |
|        - |  6165 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6166 | ` * NOTE:` |
|        - |  6167 | ` *  Arguments are returned to the array by reference.` |
|        - |  6168 | ` */` |
|        2 |  6169 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6170 |  |
|        - |  6171 | `	ph7_value *pArray;` |
|        - |  6172 | `	VmFrame *pFrame;` |
|        - |  6173 | `	VmSlot *aSlot;` |
|        - |  6174 | `	sxu32 n;` |
|        - |  6175 | `	/* Point to the current frame */` |
|        3 |  6176 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  6177 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6178 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6179 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6180 | `	}` |
|        3 |  6181 | `	if( pFrame->pParent == 0 ){` |
|        - |  6182 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6183 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6184 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6185 | `		return SXRET_OK;` |
|        - |  6186 | `	}` |
|        - |  6187 | `	/* Create a new array */` |
|        3 |  6188 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6189 | `	if( pArray == 0 ){` |
|      ! 0 |  6190 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6191 | `		SXUNUSED(apArg);` |
|      ! 0 |  6192 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6193 | `		return SXRET_OK;` |
|        - |  6194 | `	}` |
|        - |  6195 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  6196 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  6197 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  6198 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  6199 | `	}` |
|        - |  6200 | `	/* Return the freshly created array */` |
|        3 |  6201 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6202 | `	return SXRET_OK;` |
|        2 |  6203 |  |
|        - |  6204 | `/*` |
|        - |  6205 | ` * array func_get_args(void)` |
|        - |  6206 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  6207 | ` * Parameters` |
|        - |  6208 | ` *  None.` |
|        - |  6209 | ` * Return` |
|        - |  6210 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  6211 | ` *  member of the current user-defined function's argument list.` |
|        - |  6212 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6213 | ` */` |
|       46 |  6214 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6215 |  |
|       47 |  6216 | `	ph7_value *pObj = 0;` |
|        - |  6217 | `	ph7_value *pArray;` |
|        - |  6218 | `	VmFrame *pFrame;` |
|        - |  6219 | `	VmSlot *aSlot;` |
|        - |  6220 | `	sxu32 n;` |
|        - |  6221 | `	/* Point to the current frame */` |
|       47 |  6222 | `	pFrame = pCtx->pVm->pFrame;` |
|       47 |  6223 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6224 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6225 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6226 | `	}` |
|       47 |  6227 | `	if( pFrame->pParent == 0 ){` |
|        - |  6228 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6229 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6230 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6231 | `		return SXRET_OK;` |
|        - |  6232 | `	}` |
|        - |  6233 | `	/* Create a new array */` |
|       47 |  6234 | `	pArray = ph7_context_new_array(pCtx);` |
|       47 |  6235 | `	if( pArray == 0 ){` |
|      ! 0 |  6236 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6237 | `		SXUNUSED(apArg);` |
|      ! 0 |  6238 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6239 | `		return SXRET_OK;` |
|        - |  6240 | `	}` |
|        - |  6241 | `	/* Start filling the array with the given arguments */` |
|       47 |  6242 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      143 |  6243 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|       97 |  6244 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|       97 |  6245 | `		if( pObj ){` |
|       97 |  6246 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       48 |  6247 | `		}` |
|       49 |  6248 | `	}` |
|        - |  6249 | `	/* Return the freshly created array */` |
|       47 |  6250 | `	ph7_result_value(pCtx,pArray);` |
|       47 |  6251 | `	return SXRET_OK;` |
|       24 |  6252 |  |
|        - |  6253 | `/*` |
|        - |  6254 | ` * bool function_exists(string $name)` |
|        - |  6255 | ` *  Return TRUE if the given function has been defined.` |
|        - |  6256 | ` * Parameters` |
|        - |  6257 | ` *  The name of the desired function.` |
|        - |  6258 | ` * Return` |
|        - |  6259 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  6260 | ` */` |
|     1728 |  6261 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6262 |  |
|        - |  6263 | `	const char *zName;` |
|        - |  6264 | `	ph7_vm *pVm;` |
|        - |  6265 | `	int nLen;` |
|        - |  6266 | `	int res;` |
|     1730 |  6267 | `	if( nArg < 1 ){` |
|        - |  6268 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  6269 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6270 | `		return SXRET_OK;` |
|        - |  6271 | `	}` |
|        - |  6272 | `	/* Point to the target VM */` |
|     1730 |  6273 | `	pVm = pCtx->pVm;` |
|        - |  6274 | `	/* Extract the function name */` |
|     1730 |  6275 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  6276 | `	/* Assume the function is not defined */` |
|     1730 |  6277 | `	res = 0;` |
|        - |  6278 | `	/* Perform the lookup */` |
|     2592 |  6279 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1724 |  6280 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6281 | `			/* Function is defined */` |
|      212 |  6282 | `			res = 1;` |
|      105 |  6283 | `	}` |
|     1730 |  6284 | `	ph7_result_bool(pCtx,res);` |
|     1730 |  6285 | `	return SXRET_OK;` |
|      866 |  6286 |  |
|        - |  6287 | `/* Forward declaration */` |
|        - |  6288 | `static ph7_class * VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg);` |
|        - |  6289 | `/*` |
|        - |  6290 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6291 | ` * [i.e: Whether it is callable or not].` |
|        - |  6292 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  6293 | ` */` |
|    11296 |  6294 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  6295 |  |
|    11298 |  6296 | `	int res = 0;` |
|    11298 |  6297 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  6298 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  6299 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  6300 | `		ph7_class_method *pMethod;` |
|      ! 0 |  6301 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  6302 | `		if( pMethod && CallInvoke ){` |
|        - |  6303 | `			ph7_value sResult;` |
|        - |  6304 | `			sxi32 rc;` |
|        - |  6305 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  6306 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  6307 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  6308 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  6309 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  6310 | `			}` |
|      ! 0 |  6311 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  6312 | `		}` |
|    11298 |  6313 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        7 |  6314 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        7 |  6315 | `		if( pMap->nEntry > 1 ){` |
|        - |  6316 | `			ph7_class *pClass;` |
|        - |  6317 | `			ph7_value *pV;` |
|        - |  6318 | `			/* Extract the target class */` |
|        7 |  6319 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|        7 |  6320 | `			if( pV ){` |
|        7 |  6321 | `				pClass = VmExtractClassFromValue(pVm,pV);` |
|        7 |  6322 | `				if( pClass ){` |
|        - |  6323 | `					ph7_class_method *pMethod;` |
|        - |  6324 | `					/* Extract the target method */` |
|        7 |  6325 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|        7 |  6326 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  6327 | `						/* Perform the lookup */` |
|        7 |  6328 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|        7 |  6329 | `						if( pMethod ){` |
|        - |  6330 | `							/* Method is callable */` |
|        5 |  6331 | `							res = 1;` |
|        2 |  6332 | `						}` |
|        3 |  6333 | `					}` |
|        3 |  6334 | `				}` |
|        3 |  6335 | `			}` |
|        4 |  6336 | `		}` |
|    11295 |  6337 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  6338 | `		const char *zName;` |
|        - |  6339 | `		int nLen;` |
|        - |  6340 | `		/* Extract the name */` |
|     2770 |  6341 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  6342 | `		/* Perform the lookup */` |
|     2773 |  6343 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|        6 |  6344 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6345 | `				/* Function is callable */` |
|     2766 |  6346 | `				res = 1;` |
|     1382 |  6347 | `		}` |
|     1384 |  6348 | `	}` |
|    11298 |  6349 | `	return res;` |
|        2 |  6350 |  |
|        - |  6351 | `/*` |
|        - |  6352 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  6353 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6354 | ` * Parameters` |
|        - |  6355 | ` * $name` |
|        - |  6356 | ` *    The callback function to check` |
|        - |  6357 | ` * $syntax_only` |
|        - |  6358 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  6359 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  6360 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  6361 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  6362 | ` *    a string.` |
|        - |  6363 | ` * Return` |
|        - |  6364 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  6365 | ` */` |
|       14 |  6366 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6367 |  |
|        - |  6368 | `	ph7_vm *pVm;` |
|        - |  6369 | `	int res;` |
|       15 |  6370 | `	if( nArg < 1 ){` |
|        - |  6371 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  6372 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6373 | `		return SXRET_OK;` |
|        - |  6374 | `	}` |
|        - |  6375 | `	/* Point to the target VM */` |
|       15 |  6376 | `	pVm = pCtx->pVm;` |
|        - |  6377 | `	/* Perform the requested operation */` |
|       15 |  6378 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  6379 | `	ph7_result_bool(pCtx,res);` |
|       15 |  6380 | `	return SXRET_OK;` |
|        8 |  6381 |  |
|        - |  6382 | `/*` |
|        - |  6383 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  6384 | ` * defined below.` |
|        - |  6385 | ` */` |
|     1040 |  6386 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  6387 |  |
|     1041 |  6388 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  6389 | `	ph7_value sName;` |
|        - |  6390 | `	sxi32 rc;` |
|        - |  6391 | `	/* Prepare the function name for insertion */` |
|     1041 |  6392 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1041 |  6393 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  6394 | `	/* Perform the insertion */` |
|     1041 |  6395 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1041 |  6396 | `	PH7_MemObjRelease(&sName);` |
|     1041 |  6397 | `	return rc;` |
|        1 |  6398 |  |
|        - |  6399 | `/*` |
|        - |  6400 | ` * array get_defined_functions(void)` |
|        - |  6401 | ` *  Returns an array of all defined functions.` |
|        - |  6402 | ` * Parameter` |
|        - |  6403 | ` *  None.` |
|        - |  6404 | ` * Return` |
|        - |  6405 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  6406 | ` *  both built-in (internal) and user-defined.` |
|        - |  6407 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  6408 | ` *  defined ones using $arr["user"].` |
|        - |  6409 | ` * Note:` |
|        - |  6410 | ` *  NULL is returned on failure.` |
|        - |  6411 | ` */` |
|        2 |  6412 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6413 |  |
|        - |  6414 | `	ph7_value *pArray,*pEntry;` |
|        - |  6415 | `	/* NOTE:` |
|        - |  6416 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  6417 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  6418 | `	 */` |
|        3 |  6419 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6420 | ` 	if( pArray == 0 ){` |
|      ! 0 |  6421 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6422 | `		SXUNUSED(apArg);` |
|        - |  6423 | `		/* Return NULL */` |
|      ! 0 |  6424 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6425 | `		return SXRET_OK;` |
|        - |  6426 | `	}` |
|        3 |  6427 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6428 | `	if( pEntry == 0 ){` |
|        - |  6429 | `		/* Return NULL */` |
|      ! 0 |  6430 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6431 | `		return SXRET_OK;` |
|        - |  6432 | `	}` |
|        - |  6433 | `	/* Fill with the appropriate information */` |
|        3 |  6434 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  6435 | `	/* Create the 'internal' index */` |
|        3 |  6436 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  6437 | `	/* Create the user-func array */` |
|        3 |  6438 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6439 | `	if( pEntry == 0 ){` |
|        - |  6440 | `		/* Return NULL */` |
|      ! 0 |  6441 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6442 | `		return SXRET_OK;` |
|        - |  6443 | `	}` |
|        - |  6444 | `	/* Fill with the appropriate information */` |
|        3 |  6445 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  6446 | `	/* Create the 'user' index */` |
|        3 |  6447 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  6448 | `	/* Return the multi-dimensional array */` |
|        3 |  6449 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6450 | `	return SXRET_OK;` |
|        2 |  6451 |  |
|        - |  6452 | `/*` |
|        - |  6453 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  6454 | ` *  Register a function for execution on shutdown.` |
|        - |  6455 | ` * Note` |
|        - |  6456 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  6457 | ` *  be called in the same order as they were registered.` |
|        - |  6458 | ` * Parameters` |
|        - |  6459 | ` *  $callback` |
|        - |  6460 | ` *   The shutdown callback to register.` |
|        - |  6461 | ` * $param` |
|        - |  6462 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  6463 | ` * Return` |
|        - |  6464 | ` *  Nothing.` |
|        - |  6465 | ` */` |
|        2 |  6466 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6467 |  |
|        - |  6468 | `	VmShutdownCB sEntry;` |
|        - |  6469 | `	int i,j;` |
|        3 |  6470 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  6471 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  6472 | `		return PH7_OK;` |
|        - |  6473 | `	}` |
|        - |  6474 | `	/* Zero the Entry */` |
|        3 |  6475 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  6476 | `	/* Initialize fields */` |
|        3 |  6477 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  6478 | `	/* Save the callback name for later invocation name */` |
|        3 |  6479 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  6480 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  6481 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  6482 | `	}` |
|        - |  6483 | `	/* Copy arguments */` |
|        3 |  6484 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  6485 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  6486 | `			/* Limit reached */` |
|      ! 0 |  6487 | `			break;` |
|        - |  6488 | `		}` |
|      ! 0 |  6489 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  6490 | `	}` |
|        3 |  6491 | `	sEntry.nArg = j;` |
|        - |  6492 | `	/* Install the callback */` |
|        3 |  6493 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  6494 | `	return PH7_OK;` |
|        2 |  6495 |  |
|        - |  6496 | `/*` |
|        - |  6497 | ` * Section:` |
|        - |  6498 | ` *  Class handling functions.` |
|        - |  6499 | ` * Status:` |
|        - |  6500 | ` *    Stable.` |
|        - |  6501 | ` */` |
|        - |  6502 | `/*` |
|        - |  6503 | ` * Extract the top active class. NULL is returned` |
|        - |  6504 | ` * if the class stack is empty.` |
|        - |  6505 | ` */` |
|       42 |  6506 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  6507 |  |
|       44 |  6508 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  6509 | `	ph7_class **apClass;` |
|       44 |  6510 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  6511 | `		/* Empty stack,return NULL */` |
|       15 |  6512 | `		return 0;` |
|        - |  6513 | `	}` |
|        - |  6514 | `	/* Peek the last entry */` |
|       30 |  6515 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|       30 |  6516 | `	return apClass[pSet->nUsed - 1];` |
|       23 |  6517 |  |
|        - |  6518 | `/*` |
|        - |  6519 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  6520 | ` *   Get the class that declared the currently executing method.` |
|        - |  6521 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  6522 | ` *` |
|        - |  6523 | ` * Parameters` |
|        - |  6524 | ` *   pVm: Target VM` |
|        - |  6525 | ` *` |
|        - |  6526 | ` * Return` |
|        - |  6527 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  6528 | ` *   - Not executing within a class method` |
|        - |  6529 | ` *` |
|        - |  6530 | ` * Note` |
|        - |  6531 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  6532 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  6533 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  6534 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  6535 | ` *   declaring class.` |
|        - |  6536 | ` */` |
|       18 |  6537 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        1 |  6538 |  |
|       19 |  6539 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  6540 | `	ph7_vm_func *pVmFunc;` |
|        - |  6541 |  |
|        - |  6542 | `	/* Skip exception frames to find the actual method frame */` |
|       19 |  6543 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  6544 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6545 | `	}` |
|        - |  6546 |  |
|        - |  6547 | `	/* Check if we're in a method context */` |
|       19 |  6548 | `	if( pFrame->pParent ){` |
|       15 |  6549 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       15 |  6550 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  6551 | `			/* Return the declaring class */` |
|       15 |  6552 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  6553 | `		}` |
|      ! 0 |  6554 | `	}` |
|        - |  6555 |  |
|        5 |  6556 | `	return 0;` |
|       10 |  6557 |  |
|        - |  6558 |  |
|        - |  6559 | `/*` |
|        - |  6560 | ` * string get_class ([ object $object = NULL ] )` |
|        - |  6561 | ` *   Returns the name of the class of an object` |
|        - |  6562 | ` * Parameters` |
|        - |  6563 | ` *  object` |
|        - |  6564 | ` *   The tested object. This parameter may be omitted when inside a class.` |
|        - |  6565 | ` * Return` |
|        - |  6566 | ` *  The name of the class of which object is an instance.` |
|        - |  6567 | ` *  Returns FALSE if object is not an object.` |
|        - |  6568 | ` *  If object is omitted when inside a class, the name of that class is returned.` |
|        - |  6569 | ` */` |
|       16 |  6570 | `static int vm_builtin_get_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6571 |  |
|        - |  6572 | `	ph7_class *pClass;` |
|        - |  6573 | `	SyString *pName;` |
|       18 |  6574 | `	if( nArg < 1 ){` |
|        - |  6575 | `		/* Check if we are inside a class */` |
|      ! 0 |  6576 | `		pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|      ! 0 |  6577 | `		if( pClass ){` |
|        - |  6578 | `			/* Point to the class name */` |
|      ! 0 |  6579 | `			pName = &pClass->sName;` |
|      ! 0 |  6580 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|      ! 0 |  6581 | `		}else{` |
|        - |  6582 | `			/* Not inside class,return FALSE */` |
|      ! 0 |  6583 | `			ph7_result_bool(pCtx,0);` |
|        - |  6584 | `		}` |
|      ! 0 |  6585 | `	}else{` |
|        - |  6586 | `		/* Extract the target class */` |
|       18 |  6587 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|       18 |  6588 | `		if( pClass ){` |
|       16 |  6589 | `			pName = &pClass->sName;` |
|        - |  6590 | `			/* Return the class name */` |
|       16 |  6591 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|        9 |  6592 | `		}else{` |
|        - |  6593 | `			/* Not a class instance,return FALSE */` |
|        3 |  6594 | `			ph7_result_bool(pCtx,0);` |
|        - |  6595 | `		}` |
|        - |  6596 | `	}` |
|       18 |  6597 | `	return PH7_OK;` |
|        2 |  6598 |  |
|        - |  6599 | `/*` |
|        - |  6600 | ` * string get_parent_class([object $object = NULL ] )` |
|        - |  6601 | ` *   Returns the name of the parent class of an object` |
|        - |  6602 | ` * Parameters` |
|        - |  6603 | ` *  object` |
|        - |  6604 | ` *   The tested object. This parameter may be omitted when inside a class.` |
|        - |  6605 | ` * Return` |
|        - |  6606 | ` *  The name of the parent class of which object is an instance.` |
|        - |  6607 | ` *  Returns FALSE if object is not an object or if the object does` |
|        - |  6608 | ` *  not have a parent.` |
|        - |  6609 | ` *  If object is omitted when inside a class, the name of that class is returned.` |
|        - |  6610 | ` */` |
|        8 |  6611 | `static int vm_builtin_get_parent_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6612 |  |
|        - |  6613 | `	ph7_class *pClass;` |
|        - |  6614 | `	SyString *pName;` |
|        9 |  6615 | `	if( nArg < 1 ){` |
|        - |  6616 | `		/* Check if we are inside a class [i.e: a method call]*/` |
|        3 |  6617 | `		pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|        3 |  6618 | `		if( pClass && pClass->pBase ){` |
|        - |  6619 | `			/* Point to the class name */` |
|        3 |  6620 | `			pName = &pClass->pBase->sName;` |
|        3 |  6621 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|        2 |  6622 | `		}else{` |
|        - |  6623 | `			/* Not inside class,return FALSE */` |
|      ! 0 |  6624 | `			ph7_result_bool(pCtx,0);` |
|        - |  6625 | `		}` |
|        2 |  6626 | `	}else{` |
|        - |  6627 | `		/* Extract the target class */` |
|        7 |  6628 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        7 |  6629 | `		if( pClass ){` |
|        7 |  6630 | `			if( pClass->pBase ){` |
|        5 |  6631 | `				pName = &pClass->pBase->sName;` |
|        - |  6632 | `				/* Return the parent class name */` |
|        5 |  6633 | `				ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|        3 |  6634 | `			}else{` |
|        - |  6635 | `				/* Object does not have a parent class */` |
|        3 |  6636 | `				ph7_result_bool(pCtx,0);` |
|        - |  6637 | `			}` |
|        4 |  6638 | `		}else{` |
|        - |  6639 | `			/* Not a class instance,return FALSE */` |
|      ! 0 |  6640 | `			ph7_result_bool(pCtx,0);` |
|        - |  6641 | `		}` |
|        - |  6642 | `	}` |
|        9 |  6643 | `	return PH7_OK;` |
|        1 |  6644 |  |
|        - |  6645 | `/*` |
|        - |  6646 | ` * string get_called_class(void)` |
|        - |  6647 | ` *   Gets the name of the class the static method is called in.` |
|        - |  6648 | ` * Parameters` |
|        - |  6649 | ` *  None.` |
|        - |  6650 | ` * Return` |
|        - |  6651 | ` *  Returns the class name. Returns FALSE if called from outside a class.` |
|        - |  6652 | ` */` |
|        4 |  6653 | `static int vm_builtin_get_called_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6654 |  |
|        - |  6655 | `	ph7_class *pClass;` |
|        - |  6656 | `	/* Check if we are inside a class [i.e: a method call] */` |
|        5 |  6657 | `	pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|        5 |  6658 | `	if( pClass ){` |
|        - |  6659 | `		SyString *pName;` |
|        - |  6660 | `		/* Point to the class name */` |
|        5 |  6661 | `		pName = &pClass->sName;` |
|        5 |  6662 | `		ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|        3 |  6663 | `	}else{` |
|      ! 0 |  6664 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6665 | `		SXUNUSED(apArg);` |
|        - |  6666 | `		/* Not inside class,return FALSE */` |
|      ! 0 |  6667 | `		ph7_result_bool(pCtx,0);` |
|        - |  6668 | `	}` |
|        5 |  6669 | `	return PH7_OK;` |
|        1 |  6670 |  |
|        - |  6671 | `/*` |
|        - |  6672 | ` * Extract a ph7_class from the given ph7_value.` |
|        - |  6673 | ` * The given value must be of type object [i.e: class instance] or` |
|        - |  6674 | ` * string which hold the class name.` |
|        - |  6675 | ` */` |
|       76 |  6676 | `static ph7_class * VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg)` |
|        2 |  6677 |  |
|       78 |  6678 | `	ph7_class *pClass = 0;` |
|       78 |  6679 | `	if( ph7_value_is_object(pArg) ){` |
|        - |  6680 | `		/* Class instance already loaded,no need to perform a lookup */` |
|       42 |  6681 | `		pClass = ((ph7_class_instance *)pArg->x.pOther)->pClass;` |
|       58 |  6682 | `	}else if( ph7_value_is_string(pArg) ){` |
|        - |  6683 | `		const char *zClass;` |
|        - |  6684 | `		int nLen;` |
|        - |  6685 | `		/* Extract class name */` |
|       35 |  6686 | `		zClass = ph7_value_to_string(pArg,&nLen);` |
|       35 |  6687 | `		if( nLen > 0 ){` |
|        - |  6688 | `			SyHashEntry *pEntry;` |
|        - |  6689 | `			/* Perform a lookup */` |
|       35 |  6690 | `			pEntry = SyHashGet(&pVm->hClass,(const void *)zClass,(sxu32)nLen);` |
|       35 |  6691 | `			if( pEntry ){` |
|        - |  6692 | `				/* Point to the desired class */` |
|       31 |  6693 | `				pClass = (ph7_class *)pEntry->pUserData;` |
|       15 |  6694 | `			}` |
|       17 |  6695 | `		}` |
|       17 |  6696 | `	}` |
|       78 |  6697 | `	return pClass;` |
|        2 |  6698 |  |
|        - |  6699 | `/*` |
|        - |  6700 | ` * bool property_exists(mixed $class,string $property)` |
|        - |  6701 | ` *   Checks if the object or class has a property.` |
|        - |  6702 | ` * Parameters` |
|        - |  6703 | ` *  class` |
|        - |  6704 | ` *   The class name or an object of the class to test for` |
|        - |  6705 | ` * property` |
|        - |  6706 | ` *  The name of the property` |
|        - |  6707 | ` * Return` |
|        - |  6708 | ` *   Returns TRUE if the property exists,FALSE otherwise.` |
|        - |  6709 | ` */` |
|       12 |  6710 | `static int vm_builtin_property_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6711 |  |
|       13 |  6712 | `	int res = 0; /* Assume attribute does not exists */` |
|       13 |  6713 | `	if( nArg > 1 ){` |
|        - |  6714 | `		ph7_class *pClass;` |
|       13 |  6715 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|       13 |  6716 | `		if( pClass ){` |
|        - |  6717 | `			const char *zName;` |
|        - |  6718 | `			int nLen;` |
|        - |  6719 | `			/* Extract attribute name */` |
|       13 |  6720 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|       13 |  6721 | `			if( nLen > 0 ){` |
|        - |  6722 | `				/* Perform the lookup in the attribute and method table */` |
|       12 |  6723 | `				if( SyHashGet(&pClass->hAttr,(const void *)zName,(sxu32)nLen) != 0` |
|        8 |  6724 | `					\|\| SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6725 | `						/* property exists,flag that */` |
|       11 |  6726 | `						res = 1;` |
|        5 |  6727 | `				}` |
|        6 |  6728 | `			}` |
|        6 |  6729 | `		}` |
|        6 |  6730 | `	}` |
|       13 |  6731 | `	ph7_result_bool(pCtx,res);` |
|       13 |  6732 | `	return PH7_OK;` |
|        1 |  6733 |  |
|        - |  6734 | `/*` |
|        - |  6735 | ` * bool method_exists(mixed $class,string $method)` |
|        - |  6736 | ` *   Checks if the given method is a class member.` |
|        - |  6737 | ` * Parameters` |
|        - |  6738 | ` *  class` |
|        - |  6739 | ` *   The class name or an object of the class to test for` |
|        - |  6740 | ` * property` |
|        - |  6741 | ` *  The name of the method` |
|        - |  6742 | ` * Return` |
|        - |  6743 | ` *   Returns TRUE if the method exists,FALSE otherwise.` |
|        - |  6744 | ` */` |
|        4 |  6745 | `static int vm_builtin_method_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6746 |  |
|        5 |  6747 | `	int res = 0; /* Assume method does not exists */` |
|        5 |  6748 | `	if( nArg > 1 ){` |
|        - |  6749 | `		ph7_class *pClass;` |
|        5 |  6750 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        5 |  6751 | `		if( pClass ){` |
|        - |  6752 | `			const char *zName;` |
|        - |  6753 | `			int nLen;` |
|        - |  6754 | `			/* Extract method name */` |
|        5 |  6755 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|        5 |  6756 | `			if( nLen > 0 ){` |
|        - |  6757 | `				/* Perform the lookup in the method table */` |
|        5 |  6758 | `				if( SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6759 | `					/* method exists,flag that */` |
|        3 |  6760 | `					res = 1;` |
|        1 |  6761 | `				}` |
|        2 |  6762 | `			}` |
|        2 |  6763 | `		}` |
|        2 |  6764 | `	}` |
|        5 |  6765 | `	ph7_result_bool(pCtx,res);` |
|        5 |  6766 | `	return PH7_OK;` |
|        1 |  6767 |  |
|        - |  6768 | `/*` |
|        - |  6769 | ` * bool class_exists(string $class_name [, bool $autoload = true ] )` |
|        - |  6770 | ` *   Checks if the class has been defined.` |
|        - |  6771 | ` * Parameters` |
|        - |  6772 | ` *  class_name` |
|        - |  6773 | ` *   The class name. The name is matched in a case-sensitive manner` |
|        - |  6774 | ` *   unlinke the standard PHP engine.` |
|        - |  6775 | ` *  autoload` |
|        - |  6776 | ` *   Whether or not to call __autoload by default.` |
|        - |  6777 | ` * Return` |
|        - |  6778 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|        - |  6779 | ` */` |
|       12 |  6780 | `static int vm_builtin_class_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6781 |  |
|       14 |  6782 | `	int res = 0; /* Assume class does not exists */` |
|       14 |  6783 | `	if( nArg > 0 ){` |
|        - |  6784 | `		const char *zName;` |
|        - |  6785 | `		int nLen;` |
|        - |  6786 | `		/* Extract given name */` |
|       14 |  6787 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  6788 | `		/* Perform a hashlookup */` |
|       14 |  6789 | `		if( nLen > 0 && SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6790 | `			/* class is available */` |
|       10 |  6791 | `			res = 1;` |
|        4 |  6792 | `		}` |
|        6 |  6793 | `	}` |
|       14 |  6794 | `	ph7_result_bool(pCtx,res);` |
|       14 |  6795 | `	return PH7_OK;` |
|        2 |  6796 |  |
|        - |  6797 | `/*` |
|        - |  6798 | ` * bool interface_exists(string $class_name [, bool $autoload = true ] )` |
|        - |  6799 | ` *   Checks if the interface has been defined.` |
|        - |  6800 | ` * Parameters` |
|        - |  6801 | ` *  class_name` |
|        - |  6802 | ` *   The class name. The name is matched in a case-sensitive manner` |
|        - |  6803 | ` *   unlinke the standard PHP engine.` |
|        - |  6804 | ` *  autoload` |
|        - |  6805 | ` *   Whether or not to call __autoload by default.` |
|        - |  6806 | ` * Return` |
|        - |  6807 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|        - |  6808 | ` */` |
|        6 |  6809 | `static int vm_builtin_interface_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6810 |  |
|        7 |  6811 | `	int res = 0; /* Assume class does not exists */` |
|        7 |  6812 | `	if( nArg > 0 ){` |
|        7 |  6813 | `		SyHashEntry *pEntry = 0;` |
|        - |  6814 | `		const char *zName;` |
|        - |  6815 | `		int nLen;` |
|        - |  6816 | `		/* Extract given name */` |
|        7 |  6817 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  6818 | `		/* Perform a hashlookup */` |
|        7 |  6819 | `		if( nLen > 0 ){` |
|        7 |  6820 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|        3 |  6821 | `		}` |
|        7 |  6822 | `		if( pEntry ){` |
|        5 |  6823 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|        5 |  6824 | `			while( pClass ){` |
|        5 |  6825 | `				if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|        - |  6826 | `					/* interface is available */` |
|        5 |  6827 | `					res = 1;` |
|        5 |  6828 | `					break;` |
|        - |  6829 | `				}` |
|        - |  6830 | `				/* Next with the same name */` |
|      ! 0 |  6831 | `				pClass = pClass->pNextName;` |
|      ! 0 |  6832 | `			}` |
|        2 |  6833 | `		}` |
|        3 |  6834 | `	}` |
|        7 |  6835 | `	ph7_result_bool(pCtx,res);` |
|        7 |  6836 | `	return PH7_OK;` |
|        1 |  6837 |  |
|        - |  6838 | `/*` |
|        - |  6839 | ` * bool class_alias([string $original[,string $alias ]])` |
|        - |  6840 | ` *   Creates an alias for a class.` |
|        - |  6841 | ` * Parameters` |
|        - |  6842 | ` *  original` |
|        - |  6843 | ` *    The original class.` |
|        - |  6844 | ` *  alias` |
|        - |  6845 | ` *   The alias name for the class.` |
|        - |  6846 | ` * Return` |
|        - |  6847 | ` *   Returns TRUE on success or FALSE on failure.` |
|        - |  6848 | ` */` |
|        2 |  6849 | `static int vm_builtin_class_alias(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6850 |  |
|        - |  6851 | `	const char *zOld,*zNew;` |
|        - |  6852 | `	int nOldLen,nNewLen;` |
|        - |  6853 | `	SyHashEntry *pEntry;` |
|        - |  6854 | `	ph7_class *pClass;` |
|        - |  6855 | `	char *zDup;` |
|        - |  6856 | `	sxi32 rc;` |
|        3 |  6857 | `	if( nArg < 2 ){` |
|        - |  6858 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  6859 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6860 | `		return PH7_OK;` |
|        - |  6861 | `	}` |
|        - |  6862 | `	/* Extract old class name */` |
|        3 |  6863 | `	zOld = ph7_value_to_string(apArg[0],&nOldLen);` |
|        - |  6864 | `	/* Extract alias name */` |
|        3 |  6865 | `	zNew = ph7_value_to_string(apArg[1],&nNewLen);` |
|        3 |  6866 | `	if( nNewLen < 1 ){` |
|        - |  6867 | `		/* Invalid alias name,return FALSE */` |
|      ! 0 |  6868 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6869 | `		return PH7_OK;` |
|        - |  6870 | `	}` |
|        - |  6871 | `	/* Perform a hash lookup */` |
|        3 |  6872 | `	pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zOld,(sxu32)nOldLen);` |
|        3 |  6873 | `	if( pEntry ==  0 ){` |
|        - |  6874 | `		/* No such class,return FALSE */` |
|      ! 0 |  6875 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6876 | `		return PH7_OK;` |
|        - |  6877 | `	}` |
|        - |  6878 | `	/* Point to the class */` |
|        3 |  6879 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  6880 | `	/* Duplicate alias name */` |
|        3 |  6881 | `	zDup = SyMemBackendStrDup(&pCtx->pVm->sAllocator,zNew,(sxu32)nNewLen);` |
|        3 |  6882 | `	if( zDup == 0 ){` |
|        - |  6883 | `		/* Out of memory,return FALSE */` |
|      ! 0 |  6884 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6885 | `		return PH7_OK;` |
|        - |  6886 | `	}` |
|        - |  6887 | `	/* Create the alias */` |
|        3 |  6888 | `	rc = SyHashInsert(&pCtx->pVm->hClass,(const void *)zDup,(sxu32)nNewLen,pClass);` |
|        3 |  6889 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  6890 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,zDup);` |
|      ! 0 |  6891 | `	}` |
|        3 |  6892 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|        3 |  6893 | `	return PH7_OK;` |
|        2 |  6894 |  |
|        - |  6895 | `/*` |
|        - |  6896 | ` * array get_declared_classes(void)` |
|        - |  6897 | ` *   Returns an array with the name of the defined classes` |
|        - |  6898 | ` * Parameters` |
|        - |  6899 | ` *  None` |
|        - |  6900 | ` * Return` |
|        - |  6901 | ` *   Returns an array of the names of the declared classes` |
|        - |  6902 | ` *   in the current script.` |
|        - |  6903 | ` * Note:` |
|        - |  6904 | ` *   NULL is returned on failure.` |
|        - |  6905 | ` */` |
|        2 |  6906 | `static int vm_builtin_get_declared_classes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6907 |  |
|        - |  6908 | `	ph7_value *pName,*pArray;` |
|        - |  6909 | `	SyHashEntry *pEntry;` |
|        - |  6910 | `	/* Create a new array first */` |
|        3 |  6911 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6912 | `	pName = ph7_context_new_scalar(pCtx);` |
|        3 |  6913 | `	if( pArray == 0 \|\| pName == 0){` |
|      ! 0 |  6914 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6915 | `		SXUNUSED(apArg);` |
|        - |  6916 | `		/* Out of memory,return NULL */` |
|      ! 0 |  6917 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6918 | `		return PH7_OK;` |
|        - |  6919 | `	}` |
|        - |  6920 | `	/* Fill the array with the defined classes */` |
|        3 |  6921 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|       44 |  6922 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|       41 |  6923 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  6924 | `		/* Do not register classes defined as interfaces */` |
|       41 |  6925 | `		if( (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       35 |  6926 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|        - |  6927 | `			/* insert class name */` |
|       35 |  6928 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|        - |  6929 | `			/* Reset the cursor */` |
|       35 |  6930 | `			ph7_value_reset_string_cursor(pName);` |
|       17 |  6931 | `		}` |
|        1 |  6932 | `	}` |
|        - |  6933 | `	/* Return the created array */` |
|        3 |  6934 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6935 | `	return PH7_OK;` |
|        2 |  6936 |  |
|        - |  6937 | `/*` |
|        - |  6938 | ` * array get_declared_interfaces(void)` |
|        - |  6939 | ` *   Returns an array with the name of the defined interfaces` |
|        - |  6940 | ` * Parameters` |
|        - |  6941 | ` *  None` |
|        - |  6942 | ` * Return` |
|        - |  6943 | ` *   Returns an array of the names of the declared interfaces` |
|        - |  6944 | ` *   in the current script.` |
|        - |  6945 | ` * Note:` |
|        - |  6946 | ` *   NULL is returned on failure.` |
|        - |  6947 | ` */` |
|        2 |  6948 | `static int vm_builtin_get_declared_interfaces(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6949 |  |
|        - |  6950 | `	ph7_value *pName,*pArray;` |
|        - |  6951 | `	SyHashEntry *pEntry;` |
|        - |  6952 | `	/* Create a new array first */` |
|        3 |  6953 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6954 | `	pName = ph7_context_new_scalar(pCtx);` |
|        3 |  6955 | `	if( pArray == 0 \|\| pName == 0 ){` |
|      ! 0 |  6956 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6957 | `		SXUNUSED(apArg);` |
|        - |  6958 | `		/* Out of memory,return NULL */` |
|      ! 0 |  6959 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6960 | `		return PH7_OK;` |
|        - |  6961 | `	}` |
|        - |  6962 | `	/* Fill the array with the defined classes */` |
|        3 |  6963 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|       46 |  6964 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|       43 |  6965 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  6966 | `		/* Register classes defined as interfaces only */` |
|       43 |  6967 | `		if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|        9 |  6968 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|        - |  6969 | `			/* insert interface name */` |
|        9 |  6970 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|        - |  6971 | `			/* Reset the cursor */` |
|        9 |  6972 | `			ph7_value_reset_string_cursor(pName);` |
|        4 |  6973 | `		}` |
|        1 |  6974 | `	}` |
|        - |  6975 | `	/* Return the created array */` |
|        3 |  6976 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6977 | `	return PH7_OK;` |
|        2 |  6978 |  |
|        - |  6979 | `/*` |
|        - |  6980 | ` * array get_class_methods(string/object $class_name)` |
|        - |  6981 | ` *   Returns an array with the name of the class methods` |
|        - |  6982 | ` * Parameters` |
|        - |  6983 | ` *  class_name` |
|        - |  6984 | ` *  The class name or class instance` |
|        - |  6985 | ` * Return` |
|        - |  6986 | ` *  Returns an array of method names defined for the class specified by class_name.` |
|        - |  6987 | ` *  In case of an error, it returns NULL.` |
|        - |  6988 | ` * Note:` |
|        - |  6989 | ` *   NULL is returned on failure.` |
|        - |  6990 | ` */` |
|        6 |  6991 | `static int vm_builtin_get_class_methods(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6992 |  |
|        - |  6993 | `	ph7_value *pName,*pArray;` |
|        - |  6994 | `	SyHashEntry *pEntry;` |
|        - |  6995 | `	ph7_class *pClass;` |
|        - |  6996 | `	/* Extract the target class first */` |
|        7 |  6997 | `	pClass = 0;` |
|        7 |  6998 | `	if( nArg > 0 ){` |
|        7 |  6999 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        3 |  7000 | `	}` |
|        7 |  7001 | `	if( pClass == 0 ){` |
|        - |  7002 | `		/* No such class,return NULL */` |
|        3 |  7003 | `		ph7_result_null(pCtx);` |
|        3 |  7004 | `		return PH7_OK;` |
|        - |  7005 | `	}` |
|        - |  7006 | `	/* Create a new array  */` |
|        5 |  7007 | `	pArray = ph7_context_new_array(pCtx);` |
|        5 |  7008 | `	pName = ph7_context_new_scalar(pCtx);` |
|        5 |  7009 | `	if( pArray == 0 \|\| pName == 0){` |
|        - |  7010 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7011 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7012 | `		return PH7_OK;` |
|        - |  7013 | `	}` |
|        - |  7014 | `	/* Fill the array with the defined methods */` |
|        5 |  7015 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|       17 |  7016 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|       13 |  7017 | `		ph7_class_method *pMethod = (ph7_class_method *)pEntry->pUserData;` |
|        - |  7018 | `		/* Insert method name */` |
|       13 |  7019 | `		ph7_value_string(pName,SyStringData(&pMethod->sFunc.sName),(int)SyStringLength(&pMethod->sFunc.sName));` |
|       13 |  7020 | `		ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|        - |  7021 | `		/* Reset the cursor */` |
|       13 |  7022 | `		ph7_value_reset_string_cursor(pName);` |
|        1 |  7023 | `	}` |
|        - |  7024 | `	/* Return the created array */` |
|        5 |  7025 | `	ph7_result_value(pCtx,pArray);` |
|        - |  7026 | `	/*` |
|        - |  7027 | `	 * Don't worry about freeing memory here,everything will be relased` |
|        - |  7028 | `	 * automatically as soon we return from this foreign function.` |
|        - |  7029 | `	 */` |
|        5 |  7030 | `	return PH7_OK;` |
|        4 |  7031 |  |
|        - |  7032 | `/*` |
|        - |  7033 | ` * This function return TRUE(1) if the given class attribute stored` |
|        - |  7034 | ` * in the pAttrName parameter is visible and thus can be extracted` |
|        - |  7035 | ` * from the current scope.Otherwise FALSE is returned.` |
|        - |  7036 | ` */` |
|      692 |  7037 | `static int VmClassMemberAccess(` |
|        - |  7038 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  7039 | `	ph7_class *pClass,         /* Target Class */` |
|        - |  7040 | `	const SyString *pAttrName, /* Attribute name */` |
|        - |  7041 | `	sxi32 iProtection,         /* Attribute protection level [i.e: public,protected or private] */` |
|        - |  7042 | `	int bLog                   /* TRUE to log forbidden access. */` |
|        - |  7043 | `	)` |
|        2 |  7044 |  |
|      694 |  7045 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|      130 |  7046 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  7047 | `		ph7_vm_func *pVmFunc;` |
|      130 |  7048 | `		while( pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH) ) ){` |
|        - |  7049 | `			/* Safely ignore the exception frame */` |
|      ! 0 |  7050 | `			pFrame = pFrame->pParent;` |
|      ! 0 |  7051 | `		}` |
|      130 |  7052 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      130 |  7053 | `		if( pVmFunc == 0 \|\| (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|        9 |  7054 | `			goto dis; /* Access is forbidden */` |
|        - |  7055 | `		}` |
|      122 |  7056 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|        - |  7057 | `			/* Must be the same instance */` |
|        7 |  7058 | `			if( (ph7_class *)pVmFunc->pUserData != pClass ){` |
|      ! 0 |  7059 | `				goto dis; /* Access is forbidden */` |
|        - |  7060 | `			}` |
|        4 |  7061 | `		}else{` |
|        - |  7062 | `			/* Protected */` |
|      116 |  7063 | `			ph7_class *pBase = (ph7_class *)pVmFunc->pUserData;` |
|        - |  7064 | `			/* Must be a derived class */` |
|      116 |  7065 | `			if( !VmInstanceOf(pClass,pBase) ){` |
|      ! 0 |  7066 | `				goto dis; /* Access is forbidden */` |
|        - |  7067 | `			}` |
|        - |  7068 | `		}` |
|       60 |  7069 | `	}` |
|      686 |  7070 | `	return 1; /* Access is granted */` |
|        4 |  7071 | `dis:` |
|        9 |  7072 | `	if( bLog ){` |
|      ! 0 |  7073 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7074 | `			"Access to the class attribute '%z->%z' is forbidden",` |
|      ! 0 |  7075 | `			&pClass->sName,pAttrName);` |
|      ! 0 |  7076 | `	}` |
|        9 |  7077 | `	return 0; /* Access is forbidden */` |
|      348 |  7078 |  |
|        - |  7079 | `/*` |
|        - |  7080 | ` * array get_class_vars(string/object $class_name)` |
|        - |  7081 | ` *   Get the default properties of the class` |
|        - |  7082 | ` * Parameters` |
|        - |  7083 | ` *  class_name` |
|        - |  7084 | ` *   The class name or class instance` |
|        - |  7085 | ` * Return` |
|        - |  7086 | ` *  Returns an associative array of declared properties visible from the current scope` |
|        - |  7087 | ` *  with their default value. The resulting array elements are in the form` |
|        - |  7088 | ` *  of varname => value.` |
|        - |  7089 | ` * Note:` |
|        - |  7090 | ` *   NULL is returned on failure.` |
|        - |  7091 | ` */` |
|        2 |  7092 | `static int vm_builtin_get_class_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7093 |  |
|        - |  7094 | `	ph7_value *pName,*pArray,sValue;` |
|        - |  7095 | `	SyHashEntry *pEntry;` |
|        - |  7096 | `	ph7_class *pClass;` |
|        - |  7097 | `	/* Extract the target class first */` |
|        3 |  7098 | `	pClass = 0;` |
|        3 |  7099 | `	if( nArg > 0 ){` |
|        3 |  7100 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        1 |  7101 | `	}` |
|        3 |  7102 | `	if( pClass == 0 ){` |
|        - |  7103 | `		/* No such class,return NULL */` |
|      ! 0 |  7104 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7105 | `		return PH7_OK;` |
|        - |  7106 | `	}` |
|        - |  7107 | `	/* Create a new array  */` |
|        3 |  7108 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7109 | `	pName = ph7_context_new_scalar(pCtx);` |
|        3 |  7110 | `	PH7_MemObjInit(pCtx->pVm,&sValue);` |
|        3 |  7111 | `	if( pArray == 0 \|\| pName == 0){` |
|        - |  7112 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7113 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7114 | `		return PH7_OK;` |
|        - |  7115 | `	}` |
|        - |  7116 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|        3 |  7117 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        8 |  7118 | `	while((pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        5 |  7119 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|        - |  7120 | `		/* Check if the access is allowed */` |
|        5 |  7121 | `		if( VmClassMemberAccess(pCtx->pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        5 |  7122 | `			SyString *pAttrName = &pAttr->sName;` |
|        5 |  7123 | `			ph7_value *pValue = 0;` |
|        5 |  7124 | `			if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |  7125 | `				/* Extract static attribute value which is always computed */` |
|        5 |  7126 | `				pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pAttr->nIdx);` |
|        3 |  7127 | `			}else{` |
|      ! 0 |  7128 | `				if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|      ! 0 |  7129 | `					PH7_MemObjRelease(&sValue);` |
|        - |  7130 | `					/* Compute default value (any complex expression) associated with this attribute */` |
|      ! 0 |  7131 | `					VmLocalExec(pCtx->pVm,&pAttr->aByteCode,&sValue);` |
|      ! 0 |  7132 | `					pValue = &sValue;` |
|      ! 0 |  7133 | `				}` |
|        - |  7134 | `			}` |
|        - |  7135 | `			/* Fill in the array */` |
|        5 |  7136 | `			ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|        5 |  7137 | `			ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|        - |  7138 | `			/* Reset the cursor */` |
|        5 |  7139 | `			ph7_value_reset_string_cursor(pName);` |
|        2 |  7140 | `		}` |
|        1 |  7141 | `	}` |
|        3 |  7142 | `	PH7_MemObjRelease(&sValue);` |
|        - |  7143 | `	/* Return the created array */` |
|        3 |  7144 | `	ph7_result_value(pCtx,pArray);` |
|        - |  7145 | `	/*` |
|        - |  7146 | `	 * Don't worry about freeing memory here,everything will be relased` |
|        - |  7147 | `	 * automatically as soon we return from this foreign function.` |
|        - |  7148 | `	 */` |
|        3 |  7149 | `	return PH7_OK;` |
|        2 |  7150 |  |
|        - |  7151 | `/*` |
|        - |  7152 | ` * array get_object_vars(object $this)` |
|        - |  7153 | ` *   Gets the properties of the given object` |
|        - |  7154 | ` * Parameters` |
|        - |  7155 | ` *  this` |
|        - |  7156 | ` *   A class instance` |
|        - |  7157 | ` * Return` |
|        - |  7158 | ` *  Returns an associative array of defined object accessible non-static properties` |
|        - |  7159 | ` *  for the specified object in scope. If a property have not been assigned a value` |
|        - |  7160 | ` *  it will be returned with a NULL value.` |
|        - |  7161 | ` * Note:` |
|        - |  7162 | ` *   NULL is returned on failure.` |
|        - |  7163 | ` */` |
|        2 |  7164 | `static int vm_builtin_get_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7165 |  |
|        3 |  7166 | `	ph7_class_instance *pThis = 0;` |
|        - |  7167 | `	ph7_value *pName,*pArray;` |
|        - |  7168 | `	SyHashEntry *pEntry;` |
|        3 |  7169 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|        - |  7170 | `		/* Extract the target instance */` |
|        3 |  7171 | `		pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        1 |  7172 | `	}` |
|        3 |  7173 | `	if( pThis == 0 ){` |
|        - |  7174 | `		/* No such instance,return NULL */` |
|      ! 0 |  7175 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7176 | `		return PH7_OK;` |
|        - |  7177 | `	}` |
|        - |  7178 | `	/* Create a new array  */` |
|        3 |  7179 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7180 | `	pName = ph7_context_new_scalar(pCtx);` |
|        3 |  7181 | `	if( pArray == 0 \|\| pName == 0){` |
|        - |  7182 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7183 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7184 | `		return PH7_OK;` |
|        - |  7185 | `	}` |
|        - |  7186 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|        3 |  7187 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  7188 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|        7 |  7189 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  7190 | `		SyString *pAttrName;` |
|        7 |  7191 | `		if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|        - |  7192 | `			/* Only non-static/constant attributes are extracted */` |
|      ! 0 |  7193 | `			continue;` |
|        - |  7194 | `		}` |
|        7 |  7195 | `		pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  7196 | `		/* Check if the access is allowed */` |
|        7 |  7197 | `		if( VmClassMemberAccess(pCtx->pVm,pThis->pClass,pAttrName,pVmAttr->pAttr->iProtection,FALSE) ){` |
|        3 |  7198 | `			ph7_value *pValue = 0;` |
|        - |  7199 | `			/* Extract attribute */` |
|        3 |  7200 | `			pValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|        3 |  7201 | `			if( pValue ){` |
|        - |  7202 | `				/* Insert attribute name in the array */` |
|        3 |  7203 | `				ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|        3 |  7204 | `				ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|        1 |  7205 | `			}` |
|        - |  7206 | `			/* Reset the cursor */` |
|        3 |  7207 | `			ph7_value_reset_string_cursor(pName);` |
|        1 |  7208 | `		}` |
|        1 |  7209 | `	}` |
|        - |  7210 | `	/* Return the created array */` |
|        3 |  7211 | `	ph7_result_value(pCtx,pArray);` |
|        - |  7212 | `	/*` |
|        - |  7213 | `	 * Don't worry about freeing memory here,everything will be relased` |
|        - |  7214 | `	 * automatically as soon we return from this foreign function.` |
|        - |  7215 | `	 */` |
|        3 |  7216 | `	return PH7_OK;` |
|        2 |  7217 |  |
|        - |  7218 | `/*` |
|        - |  7219 | ` * This function returns TRUE if the given class is an implemented` |
|        - |  7220 | ` * interface.Otherwise FALSE is returned.` |
|        - |  7221 | ` */` |
|       30 |  7222 | `static int VmQueryInterfaceSet(ph7_class *pClass,SySet *pSet)` |
|        1 |  7223 |  |
|        - |  7224 | `	ph7_class **apInterface;` |
|        - |  7225 | `	sxu32 n;` |
|       31 |  7226 | `	if( SySetUsed(pSet) < 1 ){` |
|        - |  7227 | `		/* Empty interface container */` |
|       29 |  7228 | `		return FALSE;` |
|        - |  7229 | `	}` |
|        - |  7230 | `	/* Point to the set of implemented interfaces */` |
|        3 |  7231 | `	apInterface = (ph7_class **)SySetBasePtr(pSet);` |
|        - |  7232 | `	/* Perform the lookup */` |
|        3 |  7233 | `	for( n = 0 ; n < SySetUsed(pSet) ; n++ ){` |
|        3 |  7234 | `		if( apInterface[n] == pClass ){` |
|        3 |  7235 | `			return TRUE;` |
|        - |  7236 | `		}` |
|      ! 0 |  7237 | `	}` |
|      ! 0 |  7238 | `	return FALSE;` |
|       16 |  7239 |  |
|        - |  7240 | `/*` |
|        - |  7241 | ` * This function returns TRUE if the given class (first argument)` |
|        - |  7242 | ` * is an instance of the main class (second argument).` |
|        - |  7243 | ` * Otherwise FALSE is returned.` |
|        - |  7244 | ` */` |
|      164 |  7245 | `static int VmInstanceOf(ph7_class *pThis,ph7_class *pClass)` |
|        2 |  7246 |  |
|        - |  7247 | `	ph7_class *pParent;` |
|        - |  7248 | `	sxi32 rc;` |
|      166 |  7249 | `	if( pThis == pClass ){` |
|        - |  7250 | `		/* Instance of the same class */` |
|      138 |  7251 | `		return TRUE;` |
|        - |  7252 | `	}` |
|        - |  7253 | `	/* Check implemented interfaces */` |
|       29 |  7254 | `	rc = VmQueryInterfaceSet(pClass,&pThis->aInterface);` |
|       29 |  7255 | `	if( rc ){` |
|        3 |  7256 | `		return TRUE;` |
|        - |  7257 | `	}` |
|        - |  7258 | `	/* Check parent classes */` |
|       27 |  7259 | `	pParent = pThis->pBase;` |
|       29 |  7260 | `	while( pParent ){` |
|       27 |  7261 | `		if( pParent == pClass ){` |
|        - |  7262 | `			/* Same instance */` |
|       25 |  7263 | `			return TRUE;` |
|        - |  7264 | `		}` |
|        - |  7265 | `		/* Check the implemented interfaces */` |
|        3 |  7266 | `		rc = VmQueryInterfaceSet(pClass,&pParent->aInterface);` |
|        3 |  7267 | `		if( rc ){` |
|      ! 0 |  7268 | `			return TRUE;` |
|        - |  7269 | `		}` |
|        - |  7270 | `		/* Point to the parent class */` |
|        3 |  7271 | `		pParent = pParent->pBase;` |
|        1 |  7272 | `	}` |
|        - |  7273 | `	/* Not an instance of the the given class */` |
|        3 |  7274 | `	return FALSE;` |
|       84 |  7275 |  |
|        - |  7276 | `/*` |
|        - |  7277 | ` * This function returns TRUE if the given class (first argument)` |
|        - |  7278 | ` * is a subclass of the main class (second argument).` |
|        - |  7279 | ` * Otherwise FALSE is returned.` |
|        - |  7280 | ` */` |
|        4 |  7281 | `static int VmSubclassOf(ph7_class *pClass,ph7_class *pBase)` |
|        1 |  7282 |  |
|        5 |  7283 | `	SySet *pInterface = &pClass->aInterface;` |
|        - |  7284 | `	SyHashEntry *pEntry;` |
|        - |  7285 | `	SyString *pName;` |
|        - |  7286 | `	sxi32 rc;` |
|        5 |  7287 | `	while( pClass ){` |
|        5 |  7288 | `		pName = &pClass->sName;` |
|        - |  7289 | `		/* Query the derived hashtable */` |
|        5 |  7290 | `		pEntry = SyHashGet(&pBase->hDerived,(const void *)pName->zString,pName->nByte);` |
|        5 |  7291 | `		if( pEntry ){` |
|        5 |  7292 | `			return TRUE;` |
|        - |  7293 | `		}` |
|      ! 0 |  7294 | `		pClass = pClass->pBase;` |
|      ! 0 |  7295 | `	}` |
|      ! 0 |  7296 | `	rc = VmQueryInterfaceSet(pBase,pInterface);` |
|      ! 0 |  7297 | `	if( rc ){` |
|      ! 0 |  7298 | `		return TRUE;` |
|        - |  7299 | `	}` |
|        - |  7300 | `	/* Not a subclass */` |
|      ! 0 |  7301 | `	return FALSE;` |
|        3 |  7302 |  |
|        - |  7303 | `/*` |
|        - |  7304 | ` * bool is_a(object $object,string $class_name)` |
|        - |  7305 | ` *   Checks if the object is of this class or has this class as one of its parents.` |
|        - |  7306 | ` * Parameters` |
|        - |  7307 | ` *  object` |
|        - |  7308 | ` *   The tested object` |
|        - |  7309 | ` * class_name` |
|        - |  7310 | ` *  The class name` |
|        - |  7311 | ` * Return` |
|        - |  7312 | ` *   Returns TRUE if the object is of this class or has this class as one of its` |
|        - |  7313 | ` *   parents, FALSE otherwise.` |
|        - |  7314 | ` */` |
|        2 |  7315 | `static int vm_builtin_is_a(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7316 |  |
|        3 |  7317 | `	int res = 0; /* Assume FALSE by default */` |
|        3 |  7318 | `	if( nArg > 1 && ph7_value_is_object(apArg[0])  ){` |
|        3 |  7319 | `		ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7320 | `		ph7_class *pClass;` |
|        - |  7321 | `		/* Extract the given class */` |
|        3 |  7322 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|        3 |  7323 | `		if( pClass ){` |
|        - |  7324 | `			/* Perform the query */` |
|        3 |  7325 | `			res = VmInstanceOf(pThis->pClass,pClass);` |
|        1 |  7326 | `		}` |
|        1 |  7327 | `	}` |
|        - |  7328 | `	/* Query result */` |
|        3 |  7329 | `	ph7_result_bool(pCtx,res);` |
|        3 |  7330 | `	return PH7_OK;` |
|        1 |  7331 |  |
|        - |  7332 | `/*` |
|        - |  7333 | ` * bool is_subclass_of(object/string $object,object/string $class_name)` |
|        - |  7334 | ` *   Checks if the object has this class as one of its parents.` |
|        - |  7335 | ` * Parameters` |
|        - |  7336 | ` *  object` |
|        - |  7337 | ` *   The tested object` |
|        - |  7338 | ` * class_name` |
|        - |  7339 | ` *  The class name` |
|        - |  7340 | ` * Return` |
|        - |  7341 | ` *  This function returns TRUE if the object , belongs to a class` |
|        - |  7342 | ` *  which is a subclass of class_name, FALSE otherwise.` |
|        - |  7343 | ` */` |
|        6 |  7344 | `static int vm_builtin_is_subclass_of(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7345 |  |
|        7 |  7346 | `	int res = 0; /* Assume FALSE by default */` |
|        7 |  7347 | `	if( nArg > 1 ){` |
|        - |  7348 | `		ph7_class *pClass,*pMain;` |
|        - |  7349 | `		/* Extract the given classes */` |
|        7 |  7350 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        7 |  7351 | `		pMain = VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|        7 |  7352 | `		if( pClass && pMain ){` |
|        - |  7353 | `			/* Perform the query */` |
|        5 |  7354 | `			res = VmSubclassOf(pClass,pMain);` |
|        2 |  7355 | `		}` |
|        3 |  7356 | `	}` |
|        - |  7357 | `	/* Query result */` |
|        7 |  7358 | `	ph7_result_bool(pCtx,res);` |
|        7 |  7359 | `	return PH7_OK;` |
|        1 |  7360 |  |
|        - |  7361 | `/*` |
|        - |  7362 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  7363 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  7364 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  7365 | ` * return value indicates failure.` |
|        - |  7366 | ` */` |
|      202 |  7367 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  7368 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  7369 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  7370 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  7371 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  7372 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  7373 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  7374 | `	)` |
|        2 |  7375 |  |
|        - |  7376 | `	ph7_value *aStack;` |
|        - |  7377 | `	VmInstr aInstr[2];` |
|        - |  7378 | `	int iCursor;` |
|        - |  7379 | `	int i;` |
|        - |  7380 | `	/* Create a new operand stack */` |
|      204 |  7381 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|      204 |  7382 | `	if( aStack == 0 ){` |
|      ! 0 |  7383 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7384 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  7385 | `		return SXERR_MEM;` |
|        - |  7386 | `	}` |
|        - |  7387 | `	/* Fill the operand stack with the given arguments */` |
|      276 |  7388 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       74 |  7389 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  7390 | `		/*` |
|        - |  7391 | `		 * Symisc eXtension:` |
|        - |  7392 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  7393 | `		 */` |
|       74 |  7394 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|       38 |  7395 | `	}` |
|      204 |  7396 | `	iCursor = nArg + 1;` |
|      204 |  7397 | `	if( pThis ){` |
|        - |  7398 | `		/*` |
|        - |  7399 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  7400 | `		 */` |
|      198 |  7401 | `		pThis->iRef++; /* Increment reference count */` |
|      198 |  7402 | `		aStack[i].x.pOther = pThis;` |
|      198 |  7403 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|       98 |  7404 | `	}` |
|      204 |  7405 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|      204 |  7406 | `	i++;` |
|        - |  7407 | `	/* Push method name */` |
|      204 |  7408 | `	SyBlobReset(&aStack[i].sBlob);` |
|      204 |  7409 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|      204 |  7410 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|      204 |  7411 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  7412 | `	/* Emit the CALL istruction */` |
|      204 |  7413 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      204 |  7414 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      204 |  7415 | `	aInstr[0].iP2 = 0;` |
|      204 |  7416 | `	aInstr[0].p3  = 0;` |
|        - |  7417 | `	/* Emit the DONE instruction */` |
|      204 |  7418 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      204 |  7419 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|      204 |  7420 | `	aInstr[1].iP2 = 0;` |
|      204 |  7421 | `	aInstr[1].p3  = 0;` |
|        - |  7422 | `	/* Execute the method body (if available) */` |
|      204 |  7423 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE);` |
|        - |  7424 | `	/* Clean up the mess left behind */` |
|      204 |  7425 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      204 |  7426 | `	return PH7_OK;` |
|      103 |  7427 |  |
|        - |  7428 | `/*` |
|        - |  7429 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  7430 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  7431 | ` * in the apArg[] array.` |
|        - |  7432 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  7433 | ` * return value indicates failure.` |
|        - |  7434 | ` */` |
|      310 |  7435 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  7436 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  7437 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  7438 | `	int nArg,          /* Total number of given arguments */` |
|        - |  7439 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  7440 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  7441 | `	)` |
|        2 |  7442 |  |
|        - |  7443 | `	ph7_value *aStack;` |
|        - |  7444 | `	VmInstr aInstr[2];` |
|        - |  7445 | `	int i;` |
|      312 |  7446 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  7447 | `		/* Don't bother processing,it's invalid anyway */` |
|        3 |  7448 | `		if( pResult ){` |
|        - |  7449 | `			/* Assume a null return value */` |
|      ! 0 |  7450 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  7451 | `		}` |
|        3 |  7452 | `		return SXERR_INVALID;` |
|        - |  7453 | `	}` |
|      310 |  7454 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  7455 | `		/* Class method */` |
|       11 |  7456 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  7457 | `		ph7_class_method *pMethod = 0;` |
|       11 |  7458 | `		ph7_class_instance *pThis = 0;` |
|       11 |  7459 | `		ph7_class *pClass = 0;` |
|        - |  7460 | `		ph7_value *pValue;` |
|        - |  7461 | `		sxi32 rc;` |
|       11 |  7462 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  7463 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  7464 | `			if( pResult ){` |
|        - |  7465 | `				/* Assume a null return value */` |
|      ! 0 |  7466 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7467 | `			}` |
|      ! 0 |  7468 | `			return SXRET_OK;` |
|        - |  7469 | `		}` |
|        - |  7470 | `		/* Extract the class name or an instance of it */` |
|       11 |  7471 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  7472 | `		if( pValue ){` |
|       11 |  7473 | `			pClass = VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  7474 | `		}` |
|       11 |  7475 | `		if( pClass == 0 ){` |
|        - |  7476 | `			/* No such class,return NULL */` |
|      ! 0 |  7477 | `			if( pResult ){` |
|      ! 0 |  7478 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7479 | `			}` |
|      ! 0 |  7480 | `			return SXRET_OK;` |
|        - |  7481 | `		}` |
|       11 |  7482 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  7483 | `			/* Point to the class instance */` |
|        5 |  7484 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  7485 | `		}` |
|        - |  7486 | `		/* Try to extract the method */` |
|       11 |  7487 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  7488 | `		if( pValue ){` |
|       11 |  7489 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  7490 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  7491 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  7492 | `			}` |
|        5 |  7493 | `		}` |
|       11 |  7494 | `		if( pMethod == 0 ){` |
|        - |  7495 | `			/* No such method,return NULL */` |
|      ! 0 |  7496 | `			if( pResult ){` |
|      ! 0 |  7497 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7498 | `			}` |
|      ! 0 |  7499 | `			return SXRET_OK;` |
|        - |  7500 | `		}` |
|        - |  7501 | `		/* Call the class method */` |
|       11 |  7502 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  7503 | `		return rc;` |
|        - |  7504 | `	}` |
|        - |  7505 | `	/* Create a new operand stack */` |
|      300 |  7506 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      300 |  7507 | `	if( aStack == 0 ){` |
|      ! 0 |  7508 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7509 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  7510 | `		if( pResult ){` |
|        - |  7511 | `			/* Assume a null return value */` |
|      ! 0 |  7512 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  7513 | `		}` |
|      ! 0 |  7514 | `		return SXERR_MEM;` |
|        - |  7515 | `	}` |
|        - |  7516 | `	/* Fill the operand stack with the given arguments */` |
|      928 |  7517 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      629 |  7518 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  7519 | `		/*` |
|        - |  7520 | `		 * Symisc eXtension:` |
|        - |  7521 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  7522 | `		 */` |
|      629 |  7523 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      315 |  7524 | `	}` |
|        - |  7525 | `	/* Push the function name */` |
|      300 |  7526 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      300 |  7527 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  7528 | `	/* Emit the CALL istruction */` |
|      300 |  7529 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      300 |  7530 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      300 |  7531 | `	aInstr[0].iP2 = 0;` |
|      300 |  7532 | `	aInstr[0].p3  = 0;` |
|        - |  7533 | `	/* Emit the DONE instruction */` |
|      300 |  7534 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      300 |  7535 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      300 |  7536 | `	aInstr[1].iP2 = 0;` |
|      300 |  7537 | `	aInstr[1].p3  = 0;` |
|        - |  7538 | `	/* Execute the function body (if available) */` |
|      300 |  7539 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE);` |
|        - |  7540 | `	/* Clean up the mess left behind */` |
|      300 |  7541 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      300 |  7542 | `	return PH7_OK;` |
|      157 |  7543 |  |
|        - |  7544 | `/*` |
|        - |  7545 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  7546 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  7547 | ` * parameter.` |
|        - |  7548 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  7549 | ` * return value indicates failure.` |
|        - |  7550 | ` */` |
|      190 |  7551 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  7552 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  7553 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  7554 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  7555 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  7556 | `	)` |
|        1 |  7557 |  |
|        - |  7558 | `	ph7_value *pArg;` |
|        - |  7559 | `	SySet aArg;` |
|        - |  7560 | `	va_list ap;` |
|        - |  7561 | `	sxi32 rc;` |
|      191 |  7562 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  7563 | `	/* Copy arguments one after one */` |
|      191 |  7564 | `	va_start(ap,pResult);` |
|      319 |  7565 | `	for(;;){` |
|      639 |  7566 | `		pArg = va_arg(ap,ph7_value *);` |
|      639 |  7567 | `		if( pArg == 0 ){` |
|      191 |  7568 | `			break;` |
|        - |  7569 | `		}` |
|      449 |  7570 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  7571 | `	}` |
|        - |  7572 | `	/* Call the core routine */` |
|      191 |  7573 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  7574 | `	/* Cleanup */` |
|      191 |  7575 | `	SySetRelease(&aArg);` |
|      191 |  7576 | `	return rc;` |
|        1 |  7577 |  |
|        - |  7578 | `/*` |
|        - |  7579 | ` * value call_user_func(callable $callback[,value $parameter[, value $... ]])` |
|        - |  7580 | ` *  Call the callback given by the first parameter.` |
|        - |  7581 | ` * Parameter` |
|        - |  7582 | ` *  $callback` |
|        - |  7583 | ` *   The callable to be called.` |
|        - |  7584 | ` *  ...` |
|        - |  7585 | ` *    Zero or more parameters to be passed to the callback.` |
|        - |  7586 | ` * Return` |
|        - |  7587 | ` *  Th return value of the callback, or FALSE on error.` |
|        - |  7588 | ` */` |
|       14 |  7589 | `static int vm_builtin_call_user_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7590 |  |
|        - |  7591 | `	ph7_value sResult; /* Store callback return value here */` |
|        - |  7592 | `	sxi32 rc;` |
|       15 |  7593 | `	if( nArg < 1 ){` |
|        - |  7594 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  7595 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7596 | `		return PH7_OK;` |
|        - |  7597 | `	}` |
|       15 |  7598 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|       15 |  7599 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  7600 | `	/* Try to invoke the callback */` |
|       15 |  7601 | `	rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult);` |
|       15 |  7602 | `	if( rc != SXRET_OK ){` |
|        - |  7603 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|      ! 0 |  7604 | `		ph7_result_bool(pCtx,0); /* return false */` |
|      ! 0 |  7605 | `	}else{` |
|        - |  7606 | `		/* Callback result */` |
|       15 |  7607 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|        - |  7608 | `	}` |
|       15 |  7609 | `	PH7_MemObjRelease(&sResult);` |
|       15 |  7610 | `	return PH7_OK;` |
|        8 |  7611 |  |
|        - |  7612 | `/*` |
|        - |  7613 | ` * value call_user_func_array(callable $callback,array $param_arr)` |
|        - |  7614 | ` *  Call a callback with an array of parameters.` |
|        - |  7615 | ` * Parameter` |
|        - |  7616 | ` *  $callback` |
|        - |  7617 | ` *   The callable to be called.` |
|        - |  7618 | ` * $param_arr` |
|        - |  7619 | ` *  The parameters to be passed to the callback, as an indexed array.` |
|        - |  7620 | ` * Return` |
|        - |  7621 | ` *  Returns the return value of the callback, or FALSE on error.` |
|        - |  7622 | ` */` |
|       10 |  7623 | `static int vm_builtin_call_user_func_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7624 |  |
|        - |  7625 | `	ph7_hashmap_node *pEntry; /* Current hashmap entry */` |
|        - |  7626 | `	ph7_value *pValue,sResult;/* Store callback return value here */` |
|        - |  7627 | `	ph7_hashmap *pMap;        /* Target hashmap */` |
|        - |  7628 | `	SySet aArg;               /* Arguments containers */` |
|        - |  7629 | `	sxi32 rc;` |
|        - |  7630 | `	sxu32 n;` |
|       11 |  7631 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|        - |  7632 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 |  7633 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7634 | `		return PH7_OK;` |
|        - |  7635 | `	}` |
|       11 |  7636 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|       11 |  7637 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  7638 | `	/* Initialize the arguments container */` |
|       11 |  7639 | `	SySetInit(&aArg,&pCtx->pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  7640 | `	/* Turn hashmap entries into callback arguments */` |
|       11 |  7641 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       11 |  7642 | `	pEntry = pMap->pFirst; /* First inserted entry */` |
|       23 |  7643 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|        - |  7644 | `		/* Extract node value */` |
|       13 |  7645 | `		if( (pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx)) != 0 ){` |
|       13 |  7646 | `			SySetPut(&aArg,(const void *)&pValue);` |
|        6 |  7647 | `		}` |
|        - |  7648 | `		/* Point to the next entry */` |
|       13 |  7649 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        7 |  7650 | `	}` |
|        - |  7651 | `	/* Try to invoke the callback */` |
|       11 |  7652 | `	rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|       11 |  7653 | `	if( rc != SXRET_OK ){` |
|        - |  7654 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|      ! 0 |  7655 | `		ph7_result_bool(pCtx,0); /* return false */` |
|      ! 0 |  7656 | `	}else{` |
|        - |  7657 | `		/* Callback result */` |
|       11 |  7658 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|        - |  7659 | `	}` |
|        - |  7660 | `	/* Cleanup the mess left behind */` |
|       11 |  7661 | `	PH7_MemObjRelease(&sResult);` |
|       11 |  7662 | `	SySetRelease(&aArg);` |
|       11 |  7663 | `	return PH7_OK;` |
|        6 |  7664 |  |
|        - |  7665 | `/*` |
|        - |  7666 | ` * bool defined(string $name)` |
|        - |  7667 | ` *  Checks whether a given named constant exists.` |
|        - |  7668 | ` * Parameter:` |
|        - |  7669 | ` *  Name of the desired constant.` |
|        - |  7670 | ` * Return` |
|        - |  7671 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  7672 | ` */` |
|       12 |  7673 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7674 |  |
|        - |  7675 | `	const char *zName;` |
|       13 |  7676 | `	int nLen = 0;` |
|       13 |  7677 | `	int res = 0;` |
|       13 |  7678 | `	if( nArg < 1 ){` |
|        - |  7679 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  7680 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  7681 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7682 | `		return SXRET_OK;` |
|        - |  7683 | `	}` |
|        - |  7684 | `	/* Extract constant name */` |
|       13 |  7685 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7686 | `	/* Perform the lookup */` |
|       13 |  7687 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  7688 | `		/* Already defined */` |
|        7 |  7689 | `		res = 1;` |
|        3 |  7690 | `	}` |
|       13 |  7691 | `	ph7_result_bool(pCtx,res);` |
|       13 |  7692 | `	return SXRET_OK;` |
|        7 |  7693 |  |
|        - |  7694 | `/*` |
|        - |  7695 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  7696 | ` * below.` |
|        - |  7697 | ` */` |
|        8 |  7698 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  7699 |  |
|       10 |  7700 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  7701 | `	/* Expand constant value */` |
|       10 |  7702 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       10 |  7703 |  |
|        - |  7704 | `/*` |
|        - |  7705 | ` * bool define(string $constant_name,expression value)` |
|        - |  7706 | ` *  Defines a named constant at runtime.` |
|        - |  7707 | ` * Parameter:` |
|        - |  7708 | ` *  $constant_name` |
|        - |  7709 | ` *   The name of the constant` |
|        - |  7710 | ` *  $value` |
|        - |  7711 | ` *   Constant value` |
|        - |  7712 | ` * Return:` |
|        - |  7713 | ` *   TRUE on success,FALSE on failure.` |
|        - |  7714 | ` */` |
|       10 |  7715 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7716 |  |
|        - |  7717 | `	const char *zName;  /* Constant name */` |
|        - |  7718 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       12 |  7719 | `	int nLen = 0;       /* Name length */` |
|        - |  7720 | `	sxi32 rc;` |
|       12 |  7721 | `	if( nArg < 2 ){` |
|        - |  7722 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  7723 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  7724 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7725 | `		return SXRET_OK;` |
|        - |  7726 | `	}` |
|       12 |  7727 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  7728 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  7729 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7730 | `		return SXRET_OK;` |
|        - |  7731 | `	}` |
|        - |  7732 | `	/* Extract constant name */` |
|       12 |  7733 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  7734 | `	if( nLen < 1 ){` |
|      ! 0 |  7735 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  7736 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7737 | `		return SXRET_OK;` |
|        - |  7738 | `	}` |
|        - |  7739 | `	/* Duplicate constant value */` |
|       12 |  7740 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       12 |  7741 | `	if( pValue == 0 ){` |
|      ! 0 |  7742 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7743 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7744 | `		return SXRET_OK;` |
|        - |  7745 | `	}` |
|        - |  7746 | `	/* Initialize the memory object */` |
|       12 |  7747 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  7748 | `	/* Register the constant */` |
|       12 |  7749 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       12 |  7750 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7751 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  7752 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7753 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7754 | `		return SXRET_OK;` |
|        - |  7755 | `	}` |
|        - |  7756 | `	/* Duplicate constant value */` |
|       12 |  7757 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       12 |  7758 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  7759 | `		/* Lower case the constant name */` |
|      ! 0 |  7760 | `		char *zCur = (char *)zName;` |
|      ! 0 |  7761 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  7762 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  7763 | `				/* UTF-8 stream */` |
|      ! 0 |  7764 | `				zCur++;` |
|      ! 0 |  7765 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  7766 | `					zCur++;` |
|      ! 0 |  7767 | `				}` |
|      ! 0 |  7768 | `				continue;` |
|        - |  7769 | `			}` |
|      ! 0 |  7770 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  7771 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  7772 | `				zCur[0] = (char)c;` |
|      ! 0 |  7773 | `			}` |
|      ! 0 |  7774 | `			zCur++;` |
|      ! 0 |  7775 | `		}` |
|        - |  7776 | `		/* Finally,register the constant */` |
|      ! 0 |  7777 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  7778 | `	}` |
|        - |  7779 | `	/* All done,return TRUE */` |
|       12 |  7780 | `	ph7_result_bool(pCtx,1);` |
|       12 |  7781 | `	return SXRET_OK;` |
|        7 |  7782 |  |
|        - |  7783 | `/*` |
|        - |  7784 | ` * value constant(string $name)` |
|        - |  7785 | ` *  Returns the value of a constant` |
|        - |  7786 | ` * Parameter` |
|        - |  7787 | ` *  $name` |
|        - |  7788 | ` *    Name of the constant.` |
|        - |  7789 | ` * Return` |
|        - |  7790 | ` *  Constant value or NULL if not defined.` |
|        - |  7791 | ` */` |
|        8 |  7792 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7793 |  |
|        - |  7794 | `	SyHashEntry *pEntry;` |
|        - |  7795 | `	ph7_constant *pCons;` |
|        - |  7796 | `	const char *zName; /* Constant name */` |
|        - |  7797 | `	ph7_value sVal;    /* Constant value */` |
|        - |  7798 | `	int nLen;` |
|       10 |  7799 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  7800 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  7801 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  7802 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7803 | `		return SXRET_OK;` |
|        - |  7804 | `	}` |
|        - |  7805 | `	/* Extract the constant name */` |
|       10 |  7806 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7807 | `	/* Perform the query */` |
|       10 |  7808 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  7809 | `	if( pEntry == 0 ){` |
|        3 |  7810 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  7811 | `		ph7_result_null(pCtx);` |
|        3 |  7812 | `		return SXRET_OK;` |
|        - |  7813 | `	}` |
|        8 |  7814 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  7815 | `	/* Point to the structure that describe the constant */` |
|        8 |  7816 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  7817 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  7818 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  7819 | `	/* Return that value */` |
|        8 |  7820 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  7821 | `	/* Cleanup */` |
|        8 |  7822 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  7823 | `	return SXRET_OK;` |
|        6 |  7824 |  |
|        - |  7825 | `/*` |
|        - |  7826 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  7827 | ` * defined below.` |
|        - |  7828 | ` */` |
|      410 |  7829 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  7830 |  |
|      411 |  7831 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  7832 | `	ph7_value sName;` |
|        - |  7833 | `	sxi32 rc;` |
|        - |  7834 | `	/* Prepare the constant name for insertion */` |
|      411 |  7835 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      411 |  7836 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  7837 | `	/* Perform the insertion */` |
|      411 |  7838 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      411 |  7839 | `	PH7_MemObjRelease(&sName);` |
|      411 |  7840 | `	return rc;` |
|        1 |  7841 |  |
|        - |  7842 | `/*` |
|        - |  7843 | ` * array get_defined_constants(void)` |
|        - |  7844 | ` *  Returns an associative array with the names of all defined` |
|        - |  7845 | ` *  constants.` |
|        - |  7846 | ` * Parameters` |
|        - |  7847 | ` *  NONE.` |
|        - |  7848 | ` * Returns` |
|        - |  7849 | ` *  Returns the names of all the constants currently defined.` |
|        - |  7850 | ` */` |
|        2 |  7851 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7852 |  |
|        - |  7853 | `	ph7_value *pArray;` |
|        - |  7854 | `	/* Create the array first*/` |
|        3 |  7855 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7856 | `	if( pArray == 0 ){` |
|      ! 0 |  7857 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7858 | `		SXUNUSED(apArg);` |
|        - |  7859 | `		/* Return NULL */` |
|      ! 0 |  7860 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7861 | `		return SXRET_OK;` |
|        - |  7862 | `	}` |
|        - |  7863 | `	/* Fill the array with the defined constants */` |
|        3 |  7864 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  7865 | `	/* Return the created array */` |
|        3 |  7866 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7867 | `	return SXRET_OK;` |
|        2 |  7868 |  |
|        - |  7869 | `/*` |
|        - |  7870 | ` * Section:` |
|        - |  7871 | ` *  Output Control (OB) functions.` |
|        - |  7872 | ` * Status:` |
|        - |  7873 | ` *    Stable.` |
|        - |  7874 | ` */` |
|        - |  7875 | `/* Forward declaration */` |
|        - |  7876 | `static void VmObRestore(ph7_vm *pVm,VmObEntry *pEntry);` |
|        - |  7877 | `/*` |
|        - |  7878 | ` * void ob_clean(void)` |
|        - |  7879 | ` *  This function discards the contents of the output buffer.` |
|        - |  7880 | ` *  This function does not destroy the output buffer like ob_end_clean() does.` |
|        - |  7881 | ` * Parameter` |
|        - |  7882 | ` *  None` |
|        - |  7883 | ` * Return` |
|        - |  7884 | ` *  No value is returned.` |
|        - |  7885 | ` */` |
|        2 |  7886 | `static int vm_builtin_ob_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7887 |  |
|        3 |  7888 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7889 | `	VmObEntry *pOb;` |
|        1 |  7890 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  7891 | `	SXUNUSED(apArg);` |
|        - |  7892 | `	/* Peek the top most OB */` |
|        3 |  7893 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|        3 |  7894 | `	if( pOb ){` |
|        3 |  7895 | `		SyBlobRelease(&pOb->sOB);` |
|        1 |  7896 | `	}` |
|        3 |  7897 | `	return PH7_OK;` |
|        1 |  7898 |  |
|        - |  7899 | `/*` |
|        - |  7900 | ` * bool ob_end_clean(void)` |
|        - |  7901 | ` *  Clean (erase) the output buffer and turn off output buffering` |
|        - |  7902 | ` *  This function discards the contents of the topmost output buffer and turns` |
|        - |  7903 | ` *  off this output buffering. If you want to further process the buffer's contents` |
|        - |  7904 | ` *  you have to call ob_get_contents() before ob_end_clean() as the buffer contents` |
|        - |  7905 | ` *  are discarded when ob_end_clean() is called.` |
|        - |  7906 | ` * Parameter` |
|        - |  7907 | ` *  None` |
|        - |  7908 | ` * Return` |
|        - |  7909 | ` *  Returns TRUE on success or FALSE on failure. Reasons for failure are first that you called` |
|        - |  7910 | ` *  the function without an active buffer or that for some reason a buffer could not be deleted` |
|        - |  7911 | ` * (possible for special buffer)` |
|        - |  7912 | ` */` |
|     2600 |  7913 | `static int vm_builtin_ob_end_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7914 |  |
|     2602 |  7915 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7916 | `	VmObEntry *pOb;` |
|        - |  7917 | `	/* Pop the top most OB */` |
|     2602 |  7918 | `	pOb = (VmObEntry *)SySetPop(&pVm->aOB);` |
|     2602 |  7919 | `	if( pOb == 0){` |
|        - |  7920 | `		/* No such OB,return FALSE */` |
|      ! 0 |  7921 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7922 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7923 | `		SXUNUSED(apArg);` |
|      ! 0 |  7924 | `	}else{` |
|        - |  7925 | `		/* Release */` |
|     2602 |  7926 | `		VmObRestore(pVm,pOb);` |
|        - |  7927 | `		/* Return true */` |
|     2602 |  7928 | `		ph7_result_bool(pCtx,1);` |
|        - |  7929 | `	}` |
|     2602 |  7930 | `	return PH7_OK;` |
|        2 |  7931 |  |
|        - |  7932 | `/*` |
|        - |  7933 | ` * string ob_get_contents(void)` |
|        - |  7934 | ` *  Gets the contents of the output buffer without clearing it.` |
|        - |  7935 | ` * Parameter` |
|        - |  7936 | ` *  None` |
|        - |  7937 | ` * Return` |
|        - |  7938 | ` *  This will return the contents of the output buffer or FALSE, if output buffering isn't active.` |
|        - |  7939 | ` */` |
|        6 |  7940 | `static int vm_builtin_ob_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7941 |  |
|        7 |  7942 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7943 | `	VmObEntry *pOb;` |
|        - |  7944 | `	/* Peek the top most OB */` |
|        7 |  7945 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|        7 |  7946 | `	if( pOb == 0 ){` |
|        - |  7947 | `		/* No active OB,return FALSE */` |
|      ! 0 |  7948 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7949 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7950 | `		SXUNUSED(apArg);` |
|      ! 0 |  7951 | `	}else{` |
|        - |  7952 | `		/* Return contents */` |
|        7 |  7953 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&pOb->sOB),(int)SyBlobLength(&pOb->sOB));` |
|        - |  7954 | `	}` |
|        7 |  7955 | `	return PH7_OK;` |
|        1 |  7956 |  |
|        - |  7957 | `/*` |
|        - |  7958 | ` * string ob_get_clean(void)` |
|        - |  7959 | ` * string ob_get_flush(void)` |
|        - |  7960 | ` *  Get current buffer contents and delete current output buffer.` |
|        - |  7961 | ` * Parameter` |
|        - |  7962 | ` *  None` |
|        - |  7963 | ` * Return` |
|        - |  7964 | ` *  This will return the contents of the output buffer or FALSE, if output buffering isn't active.` |
|        - |  7965 | ` */` |
|     3880 |  7966 | `static int vm_builtin_ob_get_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7967 |  |
|     3882 |  7968 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7969 | `	VmObEntry *pOb;` |
|        - |  7970 | `	/* Pop the top most OB */` |
|     3882 |  7971 | `	pOb = (VmObEntry *)SySetPop(&pVm->aOB);` |
|     3882 |  7972 | `	if( pOb == 0 ){` |
|        - |  7973 | `		/* No active OB,return FALSE */` |
|      ! 0 |  7974 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7975 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7976 | `		SXUNUSED(apArg);` |
|      ! 0 |  7977 | `	}else{` |
|        - |  7978 | `		/* Return contents */` |
|     3882 |  7979 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&pOb->sOB),(int)SyBlobLength(&pOb->sOB)); /* Will make it's own copy */` |
|        - |  7980 | `		/* Release */` |
|     3882 |  7981 | `		VmObRestore(pVm,pOb);` |
|        - |  7982 | `	}` |
|     3882 |  7983 | `	return PH7_OK;` |
|        2 |  7984 |  |
|        - |  7985 | `/*` |
|        - |  7986 | ` * int ob_get_length(void)` |
|        - |  7987 | ` *  Return the length of the output buffer.` |
|        - |  7988 | ` * Parameter` |
|        - |  7989 | ` *  None` |
|        - |  7990 | ` * Return` |
|        - |  7991 | ` *  Returns the length of the output buffer contents or FALSE if no buffering is active.` |
|        - |  7992 | ` */` |
|        2 |  7993 | `static int vm_builtin_ob_get_length(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7994 |  |
|        3 |  7995 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7996 | `	VmObEntry *pOb;` |
|        - |  7997 | `	/* Peek the top most OB */` |
|        3 |  7998 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|        3 |  7999 | `	if( pOb == 0 ){` |
|        - |  8000 | `		/* No active OB,return FALSE */` |
|      ! 0 |  8001 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8002 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8003 | `		SXUNUSED(apArg);` |
|      ! 0 |  8004 | `	}else{` |
|        - |  8005 | `		/* Return OB length */` |
|        3 |  8006 | `		ph7_result_int64(pCtx,(ph7_int64)SyBlobLength(&pOb->sOB));` |
|        - |  8007 | `	}` |
|        3 |  8008 | `	return PH7_OK;` |
|        1 |  8009 |  |
|        - |  8010 | `/*` |
|        - |  8011 | ` * int ob_get_level(void)` |
|        - |  8012 | ` *  Returns the nesting level of the output buffering mechanism.` |
|        - |  8013 | ` * Parameter` |
|        - |  8014 | ` *  None` |
|        - |  8015 | ` * Return` |
|        - |  8016 | ` *  Returns the level of nested output buffering handlers or zero if output buffering is not active.` |
|        - |  8017 | ` */` |
|        6 |  8018 | `static int vm_builtin_ob_get_level(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8019 |  |
|        7 |  8020 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8021 | `	int iNest;` |
|        3 |  8022 | `	SXUNUSED(nArg); /* cc warning */` |
|        3 |  8023 | `	SXUNUSED(apArg);` |
|        - |  8024 | `	/* Nesting level */` |
|        7 |  8025 | `	iNest = (int)SySetUsed(&pVm->aOB);` |
|        - |  8026 | `	/* Return the nesting value */` |
|        7 |  8027 | `	ph7_result_int(pCtx,iNest);` |
|        7 |  8028 | `	return PH7_OK;` |
|        1 |  8029 |  |
|        - |  8030 | `/*` |
|        - |  8031 | ` * Output Buffer(OB) default VM consumer routine.All VM output is now redirected` |
|        - |  8032 | ` * to a stackable internal buffer,until the user call [ob_get_clean(),ob_end_clean(),...].` |
|        - |  8033 | ` * Refer to the implementation of [ob_start()] for more information.` |
|        - |  8034 | ` */` |
|     5802 |  8035 | `static int VmObConsumer(const void *pData,unsigned int nDataLen,void *pUserData)` |
|        2 |  8036 |  |
|     5804 |  8037 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|        - |  8038 | `	VmObEntry *pEntry;` |
|        - |  8039 | `	ph7_value sResult;` |
|        - |  8040 | `	/* Peek the top most entry */` |
|     5804 |  8041 | `	pEntry = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|     5804 |  8042 | `	if( pEntry == 0 ){` |
|        - |  8043 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  8044 | `		return PH7_OK;` |
|        - |  8045 | `	}` |
|     5804 |  8046 | `	PH7_MemObjInit(pVm,&sResult);` |
|     5804 |  8047 | `	if( ph7_value_is_callable(&pEntry->sCallback) && pVm->nObDepth < 15 ){` |
|        - |  8048 | `		ph7_value sArg,*apArg[2];` |
|        - |  8049 | `		/* Fill the first argument */` |
|      ! 0 |  8050 | `		PH7_MemObjInitFromString(pVm,&sArg,0);` |
|      ! 0 |  8051 | `		PH7_MemObjStringAppend(&sArg,(const char *)pData,nDataLen);` |
|      ! 0 |  8052 | `		apArg[0] = &sArg;` |
|        - |  8053 | `		/* Call the 'filter' callback */` |
|      ! 0 |  8054 | `		pVm->nObDepth++;` |
|      ! 0 |  8055 | `		PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult);` |
|      ! 0 |  8056 | `		pVm->nObDepth--;` |
|      ! 0 |  8057 | `		if( sResult.iFlags & MEMOBJ_STRING ){` |
|        - |  8058 | `			/* Extract the function result */` |
|      ! 0 |  8059 | `			pData = SyBlobData(&sResult.sBlob);` |
|      ! 0 |  8060 | `			nDataLen = SyBlobLength(&sResult.sBlob);` |
|      ! 0 |  8061 | `		}` |
|      ! 0 |  8062 | `		PH7_MemObjRelease(&sArg);` |
|      ! 0 |  8063 | `	}` |
|     5804 |  8064 | `	if( nDataLen > 0 ){` |
|        - |  8065 | `		/* Redirect the VM output to the internal buffer */` |
|     5804 |  8066 | `		SyBlobAppend(&pEntry->sOB,pData,nDataLen);` |
|     2901 |  8067 | `	}` |
|        - |  8068 | `	/* Release */` |
|     5804 |  8069 | `	PH7_MemObjRelease(&sResult);` |
|     5804 |  8070 | `	return PH7_OK;` |
|     2903 |  8071 |  |
|        - |  8072 | `/*` |
|        - |  8073 | ` * Restore the default consumer.` |
|        - |  8074 | ` * Refer to the implementation of [ob_end_clean()] for more` |
|        - |  8075 | ` * information.` |
|        - |  8076 | ` */` |
|     6482 |  8077 | `static void VmObRestore(ph7_vm *pVm,VmObEntry *pEntry)` |
|        2 |  8078 |  |
|     6484 |  8079 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|     6484 |  8080 | `	if( SySetUsed(&pVm->aOB) < 1 ){` |
|        - |  8081 | `		/* No more stackable OB */` |
|     6466 |  8082 | `		pCons->xConsumer = pCons->xDef;` |
|     6466 |  8083 | `		pCons->pUserData = pCons->pDefData;` |
|     3232 |  8084 | `	}` |
|        - |  8085 | `	/* Release OB data */` |
|     6484 |  8086 | `	PH7_MemObjRelease(&pEntry->sCallback);` |
|     6484 |  8087 | `	SyBlobRelease(&pEntry->sOB);` |
|     6484 |  8088 |  |
|        - |  8089 | `/*` |
|        - |  8090 | ` * bool ob_start([ callback $output_callback] )` |
|        - |  8091 | ` * This function will turn output buffering on. While output buffering is active no output` |
|        - |  8092 | ` *  is sent from the script (other than headers), instead the output is stored in an internal` |
|        - |  8093 | ` *  buffer.` |
|        - |  8094 | ` * Parameter` |
|        - |  8095 | ` *  $output_callback` |
|        - |  8096 | ` *   An optional output_callback function may be specified. This function takes a string` |
|        - |  8097 | ` *   as a parameter and should return a string. The function will be called when the output` |
|        - |  8098 | ` *   buffer is flushed (sent) or cleaned (with ob_flush(), ob_clean() or similar function)` |
|        - |  8099 | ` *   or when the output buffer is flushed to the browser at the end of the request.` |
|        - |  8100 | ` *   When output_callback is called, it will receive the contents of the output buffer` |
|        - |  8101 | ` *   as its parameter and is expected to return a new output buffer as a result, which will` |
|        - |  8102 | ` *   be sent to the browser. If the output_callback is not a callable function, this function` |
|        - |  8103 | ` *   will return FALSE.` |
|        - |  8104 | ` *   If the callback function has two parameters, the second parameter is filled with` |
|        - |  8105 | ` *   a bit-field consisting of PHP_OUTPUT_HANDLER_START, PHP_OUTPUT_HANDLER_CONT` |
|        - |  8106 | ` *   and PHP_OUTPUT_HANDLER_END.` |
|        - |  8107 | ` *   If output_callback returns FALSE original input is sent to the browser.` |
|        - |  8108 | ` *   The output_callback parameter may be bypassed by passing a NULL value.` |
|        - |  8109 | ` * Return` |
|        - |  8110 | ` *   Returns TRUE on success or FALSE on failure.` |
|        - |  8111 | ` */` |
|     6482 |  8112 | `static int vm_builtin_ob_start(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8113 |  |
|     6484 |  8114 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8115 | `	VmObEntry sOb;` |
|        - |  8116 | `	sxi32 rc;` |
|        - |  8117 | `	/* Initialize the OB entry */` |
|     6484 |  8118 | `	PH7_MemObjInit(pCtx->pVm,&sOb.sCallback);` |
|     6484 |  8119 | `	SyBlobInit(&sOb.sOB,&pVm->sAllocator);` |
|     6484 |  8120 | `	if( nArg > 0 && (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) ){` |
|        - |  8121 | `		/* Save the callback name for later invocation */` |
|      ! 0 |  8122 | `		PH7_MemObjStore(apArg[0],&sOb.sCallback);` |
|      ! 0 |  8123 | `	}` |
|        - |  8124 | `	/* Push in the stack */` |
|     6484 |  8125 | `	rc = SySetPut(&pVm->aOB,(const void *)&sOb);` |
|     6484 |  8126 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8127 | `		PH7_MemObjRelease(&sOb.sCallback);` |
|      ! 0 |  8128 | `	}else{` |
|     6484 |  8129 | `		ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        - |  8130 | `		/* Substitute the default VM consumer */` |
|     6484 |  8131 | `		if( pCons->xConsumer != VmObConsumer ){` |
|     6466 |  8132 | `			pCons->xDef = pCons->xConsumer;` |
|     6466 |  8133 | `			pCons->pDefData = pCons->pUserData;` |
|        - |  8134 | `			/* Install the new consumer */` |
|     6466 |  8135 | `			pCons->xConsumer = VmObConsumer;` |
|     6466 |  8136 | `			pCons->pUserData = pVm;` |
|     3232 |  8137 | `		}` |
|        - |  8138 | `	}` |
|     6484 |  8139 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|     6484 |  8140 | `	return PH7_OK;` |
|        2 |  8141 |  |
|        - |  8142 | `/*` |
|        - |  8143 | ` * Flush Output buffer to the default VM output consumer.` |
|        - |  8144 | ` * Refer to the implementation of [ob_flush()] for more` |
|        - |  8145 | ` * information.` |
|        - |  8146 | ` */` |
|        4 |  8147 | `static sxi32 VmObFlush(ph7_vm *pVm,VmObEntry *pEntry,int bRelease)` |
|        1 |  8148 |  |
|        5 |  8149 | `	SyBlob *pBlob = &pEntry->sOB;` |
|        - |  8150 | `	sxi32 rc;` |
|        - |  8151 | `	/* Flush contents */` |
|        5 |  8152 | `	rc = PH7_OK;` |
|        5 |  8153 | `	if( SyBlobLength(pBlob) > 0 ){` |
|        - |  8154 | `		/* Call the VM output consumer */` |
|        5 |  8155 | `		rc = pVm->sVmConsumer.xDef(SyBlobData(pBlob),SyBlobLength(pBlob),pVm->sVmConsumer.pDefData);` |
|        - |  8156 | `		/* Increment VM output counter */` |
|        5 |  8157 | `		pVm->nOutputLen += SyBlobLength(pBlob);` |
|        5 |  8158 | `		if( rc != PH7_ABORT ){` |
|        5 |  8159 | `			rc = PH7_OK;` |
|        2 |  8160 | `		}` |
|        2 |  8161 | `	}` |
|        5 |  8162 | `	if( bRelease ){` |
|        3 |  8163 | `		VmObRestore(&(*pVm),pEntry);` |
|        2 |  8164 | `	}else{` |
|        - |  8165 | `		/* Reset the blob */` |
|        3 |  8166 | `		SyBlobReset(pBlob);` |
|        - |  8167 | `	}` |
|        5 |  8168 | `	return rc;` |
|        1 |  8169 |  |
|        - |  8170 | `/*` |
|        - |  8171 | ` * void ob_flush(void)` |
|        - |  8172 | ` * void flush(void)` |
|        - |  8173 | ` *  Flush (send) the output buffer.` |
|        - |  8174 | ` * Parameter` |
|        - |  8175 | ` *  None` |
|        - |  8176 | ` * Return` |
|        - |  8177 | ` *  No return value.` |
|        - |  8178 | ` */` |
|        2 |  8179 | `static int vm_builtin_ob_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8180 |  |
|        3 |  8181 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8182 | `	VmObEntry *pOb;` |
|        - |  8183 | `	sxi32 rc;` |
|        - |  8184 | `	/* Peek the top most OB entry */` |
|        3 |  8185 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|        3 |  8186 | `	if( pOb == 0 ){` |
|        - |  8187 | `		/* Empty stack,return immediately */` |
|      ! 0 |  8188 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8189 | `		SXUNUSED(apArg);` |
|      ! 0 |  8190 | `		return PH7_OK;` |
|        - |  8191 | `	}` |
|        - |  8192 | `	/* Flush contents */` |
|        3 |  8193 | `	rc = VmObFlush(pVm,pOb,FALSE);` |
|        3 |  8194 | `	return rc;` |
|        2 |  8195 |  |
|        - |  8196 | `/*` |
|        - |  8197 | ` * bool ob_end_flush(void)` |
|        - |  8198 | ` *  Flush (send) the output buffer and turn off output buffering.` |
|        - |  8199 | ` * Parameter` |
|        - |  8200 | ` *  None` |
|        - |  8201 | ` * Return` |
|        - |  8202 | ` *  Returns TRUE on success or FALSE on failure. Reasons for failure are first` |
|        - |  8203 | ` *  that you called the function without an active buffer or that for some reason` |
|        - |  8204 | ` *  a buffer could not be deleted (possible for special buffer).` |
|        - |  8205 | ` */` |
|        2 |  8206 | `static int vm_builtin_ob_end_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8207 |  |
|        3 |  8208 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8209 | `	VmObEntry *pOb;` |
|        - |  8210 | `	sxi32 rc;` |
|        - |  8211 | `	/* Pop the top most OB entry */` |
|        3 |  8212 | `	pOb = (VmObEntry *)SySetPop(&pVm->aOB);` |
|        3 |  8213 | `	if( pOb == 0 ){` |
|        - |  8214 | `		/* Empty stack,return FALSE */` |
|      ! 0 |  8215 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8216 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8217 | `		SXUNUSED(apArg);` |
|      ! 0 |  8218 | `		return PH7_OK;` |
|        - |  8219 | `	}` |
|        - |  8220 | `	/* Flush contents */` |
|        3 |  8221 | `	rc = VmObFlush(pVm,pOb,TRUE);` |
|        - |  8222 | `	/* Return true */` |
|        3 |  8223 | `	ph7_result_bool(pCtx,1);` |
|        3 |  8224 | `	return rc;` |
|        2 |  8225 |  |
|        - |  8226 | `/*` |
|        - |  8227 | ` * void ob_implicit_flush([int $flag = true ])` |
|        - |  8228 | ` *  ob_implicit_flush() will turn implicit flushing on or off.` |
|        - |  8229 | ` *  Implicit flushing will result in a flush operation after every` |
|        - |  8230 | ` *  output call, so that explicit calls to flush() will no longer be needed.` |
|        - |  8231 | ` * Parameter` |
|        - |  8232 | ` *  $flag` |
|        - |  8233 | ` *   TRUE to turn implicit flushing on, FALSE otherwise.` |
|        - |  8234 | ` * Return` |
|        - |  8235 | ` *   Nothing` |
|        - |  8236 | ` */` |
|        4 |  8237 | `static int vm_builtin_ob_implicit_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8238 |  |
|        - |  8239 | `	/* NOTE: As of this version,this function is a no-op.` |
|        - |  8240 | `	 * PH7 is smart enough to flush it's internal buffer when appropriate.` |
|        - |  8241 | `	 */` |
|        2 |  8242 | `	SXUNUSED(pCtx);` |
|        2 |  8243 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  8244 | `	SXUNUSED(apArg);` |
|        5 |  8245 | `	return PH7_OK;` |
|        1 |  8246 |  |
|        - |  8247 | `/*` |
|        - |  8248 | ` * array ob_list_handlers(void)` |
|        - |  8249 | ` *  Lists all output handlers in use.` |
|        - |  8250 | ` * Parameter` |
|        - |  8251 | ` *  None` |
|        - |  8252 | ` * Return` |
|        - |  8253 | ` *  This will return an array with the output handlers in use (if any).` |
|        - |  8254 | ` */` |
|        2 |  8255 | `static int vm_builtin_ob_list_handlers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8256 |  |
|        3 |  8257 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8258 | `	ph7_value *pArray;` |
|        - |  8259 | `	VmObEntry *aEntry;` |
|        - |  8260 | `	ph7_value sVal;` |
|        - |  8261 | `	sxu32 n;` |
|        3 |  8262 | `	if( SySetUsed(&pVm->aOB) < 1 ){` |
|        - |  8263 | `		/* Empty stack,return null */` |
|      ! 0 |  8264 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8265 | `		return PH7_OK;` |
|        - |  8266 | `	}` |
|        - |  8267 | `	/* Create a new array */` |
|        3 |  8268 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8269 | `	if( pArray == 0 ){` |
|        - |  8270 | `		/* Out of memory,return NULL */` |
|      ! 0 |  8271 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8272 | `		SXUNUSED(apArg);` |
|      ! 0 |  8273 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8274 | `		return PH7_OK;` |
|        - |  8275 | `	}` |
|        3 |  8276 | `	PH7_MemObjInit(pVm,&sVal);` |
|        - |  8277 | `	/* Point to the installed OB entries */` |
|        3 |  8278 | `	aEntry = (VmObEntry *)SySetBasePtr(&pVm->aOB);` |
|        - |  8279 | `	/* Perform the requested operation */` |
|        5 |  8280 | `	for( n = 0 ; n < SySetUsed(&pVm->aOB) ; n++ ){` |
|        3 |  8281 | `		VmObEntry *pEntry = &aEntry[n];` |
|        - |  8282 | `		/* Extract handler name */` |
|        3 |  8283 | `		SyBlobReset(&sVal.sBlob);` |
|        3 |  8284 | `		if( pEntry->sCallback.iFlags & MEMOBJ_STRING ){` |
|        - |  8285 | `			/* Callback,dup it's name */` |
|      ! 0 |  8286 | `			SyBlobDup(&pEntry->sCallback.sBlob,&sVal.sBlob);` |
|        3 |  8287 | `		}else if( pEntry->sCallback.iFlags & MEMOBJ_HASHMAP ){` |
|      ! 0 |  8288 | `			SyBlobAppend(&sVal.sBlob,"Class Method",sizeof("Class Method")-1);` |
|      ! 0 |  8289 | `		}else{` |
|        3 |  8290 | `			SyBlobAppend(&sVal.sBlob,"default output handler",sizeof("default output handler")-1);` |
|        - |  8291 | `		}` |
|        3 |  8292 | `		sVal.iFlags = MEMOBJ_STRING;` |
|        - |  8293 | `		/* Perform the insertion */` |
|        3 |  8294 | `		ph7_array_add_elem(pArray,0/* Automatic index assign */,&sVal /* Will make it's own copy */);` |
|        2 |  8295 | `	}` |
|        3 |  8296 | `	PH7_MemObjRelease(&sVal);` |
|        - |  8297 | `	/* Return the freshly created array */` |
|        3 |  8298 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8299 | `	return PH7_OK;` |
|        2 |  8300 |  |
|        - |  8301 | `/*` |
|        - |  8302 | ` * Section:` |
|        - |  8303 | ` *  Random numbers/string generators.` |
|        - |  8304 | ` * Status:` |
|        - |  8305 | ` *    Stable.` |
|        - |  8306 | ` */` |
|        - |  8307 | `/*` |
|        - |  8308 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  8309 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  8310 | ` * used by te SQLite3 library.` |
|        - |  8311 | ` */` |
|      995 |  8312 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  8313 |  |
|        - |  8314 | `	sxu32 iNum;` |
|      997 |  8315 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|      997 |  8316 | `	return iNum;` |
|        2 |  8317 |  |
|        - |  8318 | `/*` |
|        - |  8319 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  8320 | ` * Note that the generated string is NOT null terminated.` |
|        - |  8321 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  8322 | ` * by te SQLite3 library.` |
|        - |  8323 | ` */` |
|    34688 |  8324 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  8325 |  |
|        - |  8326 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  8327 | `	int i;` |
|        - |  8328 | `	/* Generate a binary string first */` |
|    34690 |  8329 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  8330 | `	/* Turn the binary string into english based alphabet */` |
|   381742 |  8331 | `	for( i = 0 ; i < nLen ; ++i ){` |
|   347054 |  8332 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   173528 |  8333 | `	 }` |
|    34690 |  8334 |  |
|        - |  8335 | `/*` |
|        - |  8336 | ` * int rand()` |
|        - |  8337 | ` * int mt_rand()` |
|        - |  8338 | ` * int rand(int $min,int $max)` |
|        - |  8339 | ` * int mt_rand(int $min,int $max)` |
|        - |  8340 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  8341 | ` * Parameter` |
|        - |  8342 | ` *  $min` |
|        - |  8343 | ` *    The lowest value to return (default: 0)` |
|        - |  8344 | ` *  $max` |
|        - |  8345 | ` *   The highest value to return (default: getrandmax())` |
|        - |  8346 | ` * Return` |
|        - |  8347 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  8348 | ` * Note:` |
|        - |  8349 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  8350 | ` *  by te SQLite3 library.` |
|        - |  8351 | ` */` |
|       20 |  8352 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8353 |  |
|        - |  8354 | `	sxu32 iNum;` |
|        - |  8355 | `	/* Generate the random number */` |
|       21 |  8356 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  8357 | `	if( nArg > 1 ){` |
|        - |  8358 | `		sxu32 iMin,iMax;` |
|        3 |  8359 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  8360 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  8361 | `		if( iMin < iMax ){` |
|        3 |  8362 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  8363 | `			if( iDiv > 0 ){` |
|        3 |  8364 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  8365 | `			}` |
|        1 |  8366 | `		}else if(iMax > 0 ){` |
|      ! 0 |  8367 | `			iNum %= iMax;` |
|      ! 0 |  8368 | `		}` |
|        1 |  8369 | `	}` |
|        - |  8370 | `	/* Return the number */` |
|       21 |  8371 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  8372 | `	return SXRET_OK;` |
|        1 |  8373 |  |
|        - |  8374 | `/*` |
|        - |  8375 | ` * int getrandmax(void)` |
|        - |  8376 | ` * int mt_getrandmax(void)` |
|        - |  8377 | ` * int rc4_getrandmax(void)` |
|        - |  8378 | ` *   Show largest possible random value` |
|        - |  8379 | ` * Return` |
|        - |  8380 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  8381 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  8382 | ` * Note:` |
|        - |  8383 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  8384 | ` *  by te SQLite3 library.` |
|        - |  8385 | ` */` |
|        4 |  8386 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8387 |  |
|        2 |  8388 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  8389 | `	SXUNUSED(apArg);` |
|        5 |  8390 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  8391 | `	return SXRET_OK;` |
|        1 |  8392 |  |
|        - |  8393 | `/*` |
|        - |  8394 | ` * string rand_str()` |
|        - |  8395 | ` * string rand_str(int $len)` |
|        - |  8396 | ` *  Generate a random string (English alphabet).` |
|        - |  8397 | ` * Parameter` |
|        - |  8398 | ` *  $len` |
|        - |  8399 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  8400 | ` * Return` |
|        - |  8401 | ` *   A pseudo random string.` |
|        - |  8402 | ` * Note:` |
|        - |  8403 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  8404 | ` *  by te SQLite3 library.` |
|        - |  8405 | ` *  This function is a symisc extension.` |
|        - |  8406 | ` */` |
|      122 |  8407 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8408 |  |
|        - |  8409 | `	char zString[1024];` |
|      124 |  8410 | `	int iLen = 0x10;` |
|      124 |  8411 | `	if( nArg > 0 ){` |
|        - |  8412 | `		/* Get the desired length */` |
|      124 |  8413 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      124 |  8414 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  8415 | `			/* Default length */` |
|        3 |  8416 | `			iLen = 0x10;` |
|        1 |  8417 | `		}` |
|       61 |  8418 | `	}` |
|        - |  8419 | `	/* Generate the random string */` |
|      124 |  8420 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  8421 | `	/* Return the generated string */` |
|      124 |  8422 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      124 |  8423 | `	return SXRET_OK;` |
|        2 |  8424 |  |
|        - |  8425 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  8426 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  8427 | `/* Unique ID private data */` |
|        - |  8428 | `struct unique_id_data` |
|        - |  8429 |  |
|        - |  8430 | `	ph7_context *pCtx; /* Call context */` |
|        - |  8431 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  8432 | `};` |
|        - |  8433 | `/*` |
|        - |  8434 | ` * Binary to hex consumer callback.` |
|        - |  8435 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  8436 | ` * defined below.` |
|        - |  8437 | ` */` |
|      192 |  8438 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  8439 |  |
|      193 |  8440 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  8441 | `	sxu32 nBuflen;` |
|        - |  8442 | `	/* Extract result buffer length */` |
|      193 |  8443 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  8444 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  8445 | `			/*` |
|        - |  8446 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  8447 | `			 * string will be 13 characters long` |
|        - |  8448 | `			 */` |
|       25 |  8449 | `		return SXERR_ABORT;` |
|        - |  8450 | `	}` |
|      169 |  8451 | `	if( nBuflen > 22 ){` |
|      ! 0 |  8452 | `		return SXERR_ABORT;` |
|        - |  8453 | `	}` |
|        - |  8454 | `	/* Safely Consume the hex stream */` |
|      169 |  8455 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  8456 | `	return SXRET_OK;` |
|       97 |  8457 |  |
|        - |  8458 | `/*` |
|        - |  8459 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  8460 | ` *  Generate a unique ID` |
|        - |  8461 | ` * Parameter` |
|        - |  8462 | ` * $prefix` |
|        - |  8463 | ` *  Append this prefix to the generated unique ID.` |
|        - |  8464 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  8465 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  8466 | ` * $more_entropy` |
|        - |  8467 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  8468 | ` *  that the result will be unique.` |
|        - |  8469 | ` * Return` |
|        - |  8470 | ` *  Returns the unique identifier, as a string.` |
|        - |  8471 | ` */` |
|       24 |  8472 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8473 |  |
|        - |  8474 | `	struct unique_id_data sUniq;` |
|        - |  8475 | `	unsigned char zDigest[20];` |
|       25 |  8476 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8477 | `	const char *zPrefix;` |
|        - |  8478 | `	SHA1Context sCtx;` |
|        - |  8479 | `	char zRandom[7];` |
|        - |  8480 | `	int nPrefix;` |
|        - |  8481 | `	int entropy;` |
|        - |  8482 | `	/* Generate a random string first */` |
|       25 |  8483 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  8484 | `	/* Initialize fields */` |
|       25 |  8485 | `	zPrefix = 0;` |
|       25 |  8486 | `	nPrefix = 0;` |
|       25 |  8487 | `	entropy = 0;` |
|       25 |  8488 | `	if( nArg > 0 ){` |
|        - |  8489 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  8490 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  8491 | `		if( nArg > 1 ){` |
|      ! 0 |  8492 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  8493 | `		}` |
|      ! 0 |  8494 | `	}` |
|       25 |  8495 | `	SHA1Init(&sCtx);` |
|        - |  8496 | `	/* Generate the random ID */` |
|       25 |  8497 | `	if( nPrefix > 0 ){` |
|      ! 0 |  8498 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  8499 | `	}` |
|        - |  8500 | `	/* Append the random ID */` |
|       25 |  8501 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  8502 | `	/* Append the random string */` |
|       25 |  8503 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  8504 | `	/* Increment the number */` |
|       25 |  8505 | `	pVm->unique_id++;` |
|       25 |  8506 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  8507 | `	/* Hexify the digest */` |
|       25 |  8508 | `	sUniq.pCtx = pCtx;` |
|       25 |  8509 | `	sUniq.entropy = entropy;` |
|       25 |  8510 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  8511 | `	/* All done */` |
|       25 |  8512 | `	return PH7_OK;` |
|        1 |  8513 |  |
|        - |  8514 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  8515 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  8516 | `/*` |
|        - |  8517 | ` * Section:` |
|        - |  8518 | ` *  Language construct implementation as foreign functions.` |
|        - |  8519 | ` * Status:` |
|        - |  8520 | ` *    Stable.` |
|        - |  8521 | ` */` |
|        - |  8522 | `/*` |
|        - |  8523 | ` * void echo($string...)` |
|        - |  8524 | ` *  Output one or more messages.` |
|        - |  8525 | ` * Parameters` |
|        - |  8526 | ` *  $string` |
|        - |  8527 | ` *   Message to output.` |
|        - |  8528 | ` * Return` |
|        - |  8529 | ` *  NULL.` |
|        - |  8530 | ` */` |
|      ! 0 |  8531 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  8532 |  |
|        - |  8533 | `	const char *zData;` |
|      ! 0 |  8534 | `	int nDataLen = 0;` |
|        - |  8535 | `	ph7_vm *pVm;` |
|        - |  8536 | `	int i,rc;` |
|        - |  8537 | `	/* Point to the target VM */` |
|      ! 0 |  8538 | `	pVm = pCtx->pVm;` |
|        - |  8539 | `	/* Output */` |
|      ! 0 |  8540 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  8541 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  8542 | `		if( nDataLen > 0 ){` |
|      ! 0 |  8543 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  8544 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  8545 | `				/* Increment output length */` |
|      ! 0 |  8546 | `				pVm->nOutputLen += nDataLen;` |
|      ! 0 |  8547 | `			}` |
|      ! 0 |  8548 | `			if( rc == SXERR_ABORT ){` |
|        - |  8549 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  8550 | `				return PH7_ABORT;` |
|        - |  8551 | `			}` |
|      ! 0 |  8552 | `		}` |
|      ! 0 |  8553 | `	}` |
|      ! 0 |  8554 | `	return SXRET_OK;` |
|      ! 0 |  8555 |  |
|        - |  8556 | `/*` |
|        - |  8557 | ` * int print($string...)` |
|        - |  8558 | ` *  Output one or more messages.` |
|        - |  8559 | ` * Parameters` |
|        - |  8560 | ` *  $string` |
|        - |  8561 | ` *   Message to output.` |
|        - |  8562 | ` * Return` |
|        - |  8563 | ` *  1 always.` |
|        - |  8564 | ` */` |
|        2 |  8565 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8566 |  |
|        - |  8567 | `	const char *zData;` |
|        3 |  8568 | `	int nDataLen = 0;` |
|        - |  8569 | `	ph7_vm *pVm;` |
|        - |  8570 | `	int i,rc;` |
|        - |  8571 | `	/* Point to the target VM */` |
|        3 |  8572 | `	pVm = pCtx->pVm;` |
|        - |  8573 | `	/* Output */` |
|        5 |  8574 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 |  8575 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 |  8576 | `		if( nDataLen > 0 ){` |
|        3 |  8577 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 |  8578 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  8579 | `				/* Increment output length */` |
|        3 |  8580 | `				pVm->nOutputLen += nDataLen;` |
|        1 |  8581 | `			}` |
|        3 |  8582 | `			if( rc == SXERR_ABORT ){` |
|        - |  8583 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  8584 | `				return PH7_ABORT;` |
|        - |  8585 | `			}` |
|        1 |  8586 | `		}` |
|        2 |  8587 | `	}` |
|        - |  8588 | `	/* Return 1 */` |
|        3 |  8589 | `	ph7_result_int(pCtx,1);` |
|        3 |  8590 | `	return SXRET_OK;` |
|        2 |  8591 |  |
|        - |  8592 | `/*` |
|        - |  8593 | ` * void exit(string $msg)` |
|        - |  8594 | ` * void exit(int $status)` |
|        - |  8595 | ` * void die(string $ms)` |
|        - |  8596 | ` * void die(int $status)` |
|        - |  8597 | ` *   Output a message and terminate program execution.` |
|        - |  8598 | ` * Parameter` |
|        - |  8599 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  8600 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  8601 | ` *  and not printed` |
|        - |  8602 | ` * Return` |
|        - |  8603 | ` *  NULL` |
|        - |  8604 | ` */` |
|      ! 0 |  8605 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  8606 |  |
|      ! 0 |  8607 | `	if( nArg > 0 ){` |
|      ! 0 |  8608 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  8609 | `			const char *zData;` |
|      ! 0 |  8610 | `			int iLen = 0;` |
|        - |  8611 | `			/* Print exit message */` |
|      ! 0 |  8612 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  8613 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  8614 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  8615 | `			sxi32 iExitStatus;` |
|        - |  8616 | `			/* Record exit status code */` |
|      ! 0 |  8617 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  8618 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  8619 | `		}` |
|      ! 0 |  8620 | `	}` |
|        - |  8621 | `	/* Check if we are in an included file */` |
|      ! 0 |  8622 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - |  8623 | `		/* Exit the entire process */` |
|      ! 0 |  8624 | `		exit(pCtx->pVm->iExitStatus);` |
|        - |  8625 | `	}` |
|        - |  8626 | `	/* Abort processing immediately */` |
|      ! 0 |  8627 | `	return PH7_ABORT;` |
|      ! 0 |  8628 |  |
|        - |  8629 | `/*` |
|        - |  8630 | ` * bool isset($var,...)` |
|        - |  8631 | ` *  Finds out whether a variable is set.` |
|        - |  8632 | ` * Parameters` |
|        - |  8633 | ` *  One or more variable to check.` |
|        - |  8634 | ` * Return` |
|        - |  8635 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |  8636 | ` */` |
|    43776 |  8637 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8638 |  |
|        - |  8639 | `	ph7_value *pObj;` |
|    43778 |  8640 | `	int res = 0;` |
|        - |  8641 | `	int i;` |
|    43778 |  8642 | `	if( nArg < 1 ){` |
|        - |  8643 | `		/* Missing arguments,return false */` |
|      ! 0 |  8644 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  8645 | `		return SXRET_OK;` |
|        - |  8646 | `	}` |
|        - |  8647 | `	/* Iterate over available arguments */` |
|    58904 |  8648 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    43778 |  8649 | `		pObj = apArg[i];` |
|    43778 |  8650 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    28629 |  8651 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  8652 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  8653 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  8654 | `			}` |
|    14314 |  8655 | `		}` |
|    43778 |  8656 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    43778 |  8657 | `		if( !res ){` |
|        - |  8658 | `			/* Variable not set,return FALSE */` |
|    28652 |  8659 | `			ph7_result_bool(pCtx,0);` |
|    28652 |  8660 | `			return SXRET_OK;` |
|        - |  8661 | `		}` |
|     7565 |  8662 | `	}` |
|        - |  8663 | `	/* All given variable are set,return TRUE */` |
|    15128 |  8664 | `	ph7_result_bool(pCtx,1);` |
|    15128 |  8665 | `	return SXRET_OK;` |
|    21890 |  8666 |  |
|        - |  8667 | `/*` |
|        - |  8668 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  8669 | ` * frame,the reference table and discard it's contents.` |
|        - |  8670 | ` * This function never fail and always return SXRET_OK.` |
|        - |  8671 | ` */` |
|   549538 |  8672 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  8673 |  |
|        - |  8674 | `	ph7_value *pObj;` |
|        - |  8675 | `	VmRefObj *pRef;` |
|   549540 |  8676 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|   549540 |  8677 | `	if( pObj ){` |
|        - |  8678 | `		/* Release the object */` |
|   549540 |  8679 | `		PH7_MemObjRelease(pObj);` |
|   274769 |  8680 | `	}` |
|        - |  8681 | `	/* Remove old reference links */` |
|   549540 |  8682 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|   549540 |  8683 | `	if( pRef ){` |
|   549520 |  8684 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  8685 | `		/* Unlink from the reference table */` |
|   549520 |  8686 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|   549520 |  8687 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  8688 | `			VmSlot sFree;` |
|        - |  8689 | `			/* Restore to the free list */` |
|   549514 |  8690 | `			sFree.nIdx = nObjIdx;` |
|   549514 |  8691 | `			sFree.pUserData = 0;` |
|   549514 |  8692 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|   274756 |  8693 | `		}` |
|   274759 |  8694 | `	}` |
|   549540 |  8695 | `	return SXRET_OK;` |
|        2 |  8696 |  |
|        - |  8697 | `/*` |
|        - |  8698 | ` * void unset($var,...)` |
|        - |  8699 | ` *   Unset one or more given variable.` |
|        - |  8700 | ` * Parameters` |
|        - |  8701 | ` *  One or more variable to unset.` |
|        - |  8702 | ` * Return` |
|        - |  8703 | ` *  Nothing.` |
|        - |  8704 | ` */` |
|     2626 |  8705 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8706 |  |
|        - |  8707 | `	ph7_value *pObj;` |
|        - |  8708 | `	ph7_vm *pVm;` |
|        - |  8709 | `	int i;` |
|        - |  8710 | `	/* Point to the target VM */` |
|     2628 |  8711 | `	pVm = pCtx->pVm;` |
|        - |  8712 | `	/* Iterate and unset */` |
|     8108 |  8713 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     5482 |  8714 | `		pObj = apArg[i];` |
|     5482 |  8715 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      700 |  8716 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  8717 | `				/* Throw an error */` |
|      ! 0 |  8718 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  8719 | `			}` |
|      351 |  8720 | `		}else{` |
|     4783 |  8721 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  8722 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     4783 |  8723 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     4777 |  8724 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     2388 |  8725 | `			}` |
|        - |  8726 | `		}` |
|     2742 |  8727 | `	}` |
|     2628 |  8728 | `	return SXRET_OK;` |
|        2 |  8729 |  |
|        - |  8730 | `/*` |
|        - |  8731 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  8732 | ` */` |
|      108 |  8733 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  8734 |  |
|      109 |  8735 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      109 |  8736 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  8737 | `	ph7_value *pObj;` |
|        - |  8738 | `	sxu32 nIdx;` |
|        - |  8739 | `	/* Extract the memory object */` |
|      109 |  8740 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      109 |  8741 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      109 |  8742 | `	if( pObj ){` |
|      109 |  8743 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      107 |  8744 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  8745 | `				SyString sName;` |
|        - |  8746 | `				ph7_value sKey;` |
|        - |  8747 | `				/* Perform the insertion */` |
|      107 |  8748 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      107 |  8749 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      107 |  8750 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      107 |  8751 | `				PH7_MemObjRelease(&sKey);` |
|       53 |  8752 | `			}` |
|       53 |  8753 | `		}` |
|       54 |  8754 | `	}` |
|      109 |  8755 | `	return SXRET_OK;` |
|        1 |  8756 |  |
|        - |  8757 | `/*` |
|        - |  8758 | ` * array get_defined_vars(void)` |
|        - |  8759 | ` *  Returns an array of all defined variables.` |
|        - |  8760 | ` * Parameter` |
|        - |  8761 | ` *  None` |
|        - |  8762 | ` * Return` |
|        - |  8763 | ` *  An array with all the variables defined in the current scope.` |
|        - |  8764 | ` */` |
|        2 |  8765 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8766 |  |
|        3 |  8767 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8768 | `	ph7_value *pArray;` |
|        - |  8769 | `	/* Create a new array */` |
|        3 |  8770 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8771 | ` 	if( pArray == 0 ){` |
|      ! 0 |  8772 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8773 | `		SXUNUSED(apArg);` |
|        - |  8774 | `		/* Return NULL */` |
|      ! 0 |  8775 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8776 | `		return SXRET_OK;` |
|        - |  8777 | `	}` |
|        - |  8778 | `	/* Superglobals first */` |
|        3 |  8779 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - |  8780 | `	/* Then variable defined in the current frame */` |
|        3 |  8781 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  8782 | `	/* Finally,return the created array */` |
|        3 |  8783 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8784 | `	return SXRET_OK;` |
|        2 |  8785 |  |
|        - |  8786 | `/*` |
|        - |  8787 | ` * bool gettype($var)` |
|        - |  8788 | ` *  Get the type of a variable` |
|        - |  8789 | ` * Parameters` |
|        - |  8790 | ` *   $var` |
|        - |  8791 | ` *    The variable being type checked.` |
|        - |  8792 | ` * Return` |
|        - |  8793 | ` *   String representation of the given variable type.` |
|        - |  8794 | ` */` |
|       26 |  8795 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8796 |  |
|       27 |  8797 | `	const char *zType = "Empty";` |
|       27 |  8798 | `	if( nArg > 0 ){` |
|       27 |  8799 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       13 |  8800 | `	}` |
|        - |  8801 | `	/* Return the variable type */` |
|       27 |  8802 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       27 |  8803 | `	return SXRET_OK;` |
|        1 |  8804 |  |
|        - |  8805 | `/*` |
|        - |  8806 | ` * string get_resource_type(resource $handle)` |
|        - |  8807 | ` *  This function gets the type of the given resource.` |
|        - |  8808 | ` * Parameters` |
|        - |  8809 | ` *  $handle` |
|        - |  8810 | ` *  The evaluated resource handle.` |
|        - |  8811 | ` * Return` |
|        - |  8812 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  8813 | ` *  representing its type. If the type is not identified by this function` |
|        - |  8814 | ` *  the return value will be the string Unknown.` |
|        - |  8815 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  8816 | ` *  is not a resource.` |
|        - |  8817 | ` */` |
|        2 |  8818 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8819 |  |
|        3 |  8820 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  8821 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  8822 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8823 | `		return PH7_OK;` |
|        - |  8824 | `	}` |
|        3 |  8825 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 |  8826 | `	return SXRET_OK;` |
|        2 |  8827 |  |
|        - |  8828 | `/*` |
|        - |  8829 | ` * void var_dump(expression,....)` |
|        - |  8830 | ` *   var_dump � Dumps information about a variable` |
|        - |  8831 | ` * Parameters` |
|        - |  8832 | ` *   One or more expression to dump.` |
|        - |  8833 | ` * Returns` |
|        - |  8834 | ` *  Nothing.` |
|        - |  8835 | ` */` |
|      248 |  8836 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8837 |  |
|        - |  8838 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  8839 | `	int i;` |
|      250 |  8840 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  8841 | `	/* Dump one or more expressions */` |
|      504 |  8842 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      256 |  8843 | `		ph7_value *pObj = apArg[i];` |
|        - |  8844 | `		/* Reset the working buffer */` |
|      256 |  8845 | `		SyBlobReset(&sDump);` |
|        - |  8846 | `		/* Dump the given expression */` |
|      256 |  8847 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  8848 | `		/* Output */` |
|      256 |  8849 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      256 |  8850 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      127 |  8851 | `		}` |
|      129 |  8852 | `	}` |
|        - |  8853 | `	/* Release the working buffer */` |
|      250 |  8854 | `	SyBlobRelease(&sDump);` |
|      250 |  8855 | `	return SXRET_OK;` |
|        2 |  8856 |  |
|        - |  8857 | `/*` |
|        - |  8858 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  8859 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  8860 | ` * Parameters` |
|        - |  8861 | ` *   expression: Expression to dump` |
|        - |  8862 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  8863 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  8864 | ` *            print_r() will return the information rather than print it.` |
|        - |  8865 | ` * Return` |
|        - |  8866 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  8867 | ` *  Otherwise, the return value is TRUE.` |
|        - |  8868 | ` */` |
|       16 |  8869 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8870 |  |
|       17 |  8871 | `	int ret_string = 0;` |
|        - |  8872 | `	SyBlob sDump;` |
|       17 |  8873 | `	if( nArg < 1 ){` |
|        - |  8874 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  8875 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8876 | `		return SXRET_OK;` |
|        - |  8877 | `	}` |
|       17 |  8878 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 |  8879 | `	if ( nArg > 1 ){` |
|        - |  8880 | `		/* Where to redirect output */` |
|       11 |  8881 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 |  8882 | `	}` |
|        - |  8883 | `	/* Generate dump */` |
|       17 |  8884 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 |  8885 | `	if( !ret_string ){` |
|        - |  8886 | `		/* Output dump */` |
|        7 |  8887 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8888 | `		/* Return true */` |
|        7 |  8889 | `		ph7_result_bool(pCtx,1);` |
|        4 |  8890 | `	}else{` |
|        - |  8891 | `		/* Generated dump as return value */` |
|       11 |  8892 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8893 | `	}` |
|        - |  8894 | `	/* Release the working buffer */` |
|       17 |  8895 | `	SyBlobRelease(&sDump);` |
|       17 |  8896 | `	return SXRET_OK;` |
|        9 |  8897 |  |
|        - |  8898 | `/*` |
|        - |  8899 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  8900 | ` * Same job as print_r. (see coment above)` |
|        - |  8901 | ` */` |
|        2 |  8902 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8903 |  |
|        3 |  8904 | `	int ret_string = 0;` |
|        - |  8905 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 |  8906 | `	if( nArg < 1 ){` |
|        - |  8907 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  8908 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8909 | `		return SXRET_OK;` |
|        - |  8910 | `	}` |
|        3 |  8911 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 |  8912 | `	if ( nArg > 1 ){` |
|        - |  8913 | `		/* Where to redirect output */` |
|        3 |  8914 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 |  8915 | `	}` |
|        - |  8916 | `	/* Generate dump */` |
|        3 |  8917 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 |  8918 | `	if( !ret_string ){` |
|        - |  8919 | `		/* Output dump */` |
|      ! 0 |  8920 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8921 | `		/* Return NULL */` |
|      ! 0 |  8922 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8923 | `	}else{` |
|        - |  8924 | `		/* Generated dump as return value */` |
|        3 |  8925 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8926 | `	}` |
|        - |  8927 | `	/* Release the working buffer */` |
|        3 |  8928 | `	SyBlobRelease(&sDump);` |
|        3 |  8929 | `	return SXRET_OK;` |
|        2 |  8930 |  |
|        - |  8931 | `/*` |
|        - |  8932 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - |  8933 | ` *  Set/get the various assert flags.` |
|        - |  8934 | ` * Parameter` |
|        - |  8935 | ` * $what` |
|        - |  8936 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - |  8937 | ` *   ASSERT_WARNING         Issue a warning for each failed assertion` |
|        - |  8938 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  8939 | ` *   ASSERT_QUIET_EVAL      Not used` |
|        - |  8940 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  8941 | ` * $value` |
|        - |  8942 | ` *   An optional new value for the option.` |
|        - |  8943 | ` * Return` |
|        - |  8944 | ` *  Old setting on success or FALSE on failure.` |
|        - |  8945 | ` */` |
|        8 |  8946 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8947 |  |
|        9 |  8948 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8949 | `	int iOld,iNew,iValue;` |
|        9 |  8950 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|        - |  8951 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 |  8952 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8953 | `		return PH7_OK;` |
|        - |  8954 | `	}` |
|        - |  8955 | `	/* Save old assertion flags */` |
|        9 |  8956 | `	iOld = pVm->iAssertFlags;` |
|        - |  8957 | `	/* Extract the new flags */` |
|        9 |  8958 | `	iNew = ph7_value_to_int(apArg[0]);` |
|        9 |  8959 | `	if( iNew == PH7_ASSERT_DISABLE ){` |
|        7 |  8960 | `		pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        7 |  8961 | `		if( nArg > 1 ){` |
|        5 |  8962 | `			iValue = !ph7_value_to_bool(apArg[1]);` |
|        5 |  8963 | `			if( iValue ){` |
|        - |  8964 | `				/* Disable assertion */` |
|        3 |  8965 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        1 |  8966 | `			}` |
|        3 |  8967 | `		}` |
|        6 |  8968 | `	}else if( iNew == PH7_ASSERT_WARNING ){` |
|      ! 0 |  8969 | `		pVm->iAssertFlags &= ~PH7_ASSERT_WARNING;` |
|      ! 0 |  8970 | `		if( nArg > 1 ){` |
|      ! 0 |  8971 | `			iValue = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  8972 | `			if( iValue ){` |
|        - |  8973 | `				/* Issue a warning for each failed assertion */` |
|      ! 0 |  8974 | `				pVm->iAssertFlags \|= PH7_ASSERT_WARNING;` |
|      ! 0 |  8975 | `			}` |
|      ! 0 |  8976 | `		}` |
|        3 |  8977 | `	}else if( iNew == PH7_ASSERT_BAIL ){` |
|        3 |  8978 | `		pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        3 |  8979 | `		if( nArg > 1 ){` |
|        3 |  8980 | `			iValue = ph7_value_to_bool(apArg[1]);` |
|        3 |  8981 | `			if( iValue ){` |
|        - |  8982 | `				/* Terminate execution on failed assertions */` |
|        3 |  8983 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        1 |  8984 | `			}` |
|        2 |  8985 | `		}` |
|        1 |  8986 | `	}else if( iNew == PH7_ASSERT_CALLBACK ){` |
|      ! 0 |  8987 | `		pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|      ! 0 |  8988 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|        - |  8989 | `			/* Callback to call on failed assertions */` |
|      ! 0 |  8990 | `			PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 |  8991 | `			pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 |  8992 | `		}` |
|      ! 0 |  8993 | `	}` |
|        - |  8994 | `	/* Return the old flags */` |
|        9 |  8995 | `	ph7_result_int(pCtx,iOld);` |
|        9 |  8996 | `	return PH7_OK;` |
|        5 |  8997 |  |
|        - |  8998 | `/*` |
|        - |  8999 | ` * bool assert(mixed $assertion)` |
|        - |  9000 | ` *  Checks if assertion is FALSE.` |
|        - |  9001 | ` * Parameter` |
|        - |  9002 | ` *  $assertion` |
|        - |  9003 | ` *    The assertion to test.` |
|        - |  9004 | ` * Return` |
|        - |  9005 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - |  9006 | ` */` |
|       14 |  9007 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9008 |  |
|       15 |  9009 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9010 | `	ph7_value *pAssert;` |
|        - |  9011 | `	int iFlags,iResult;` |
|       15 |  9012 | `	if( nArg < 1 ){` |
|        - |  9013 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  9014 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9015 | `		return PH7_OK;` |
|        - |  9016 | `	}` |
|       15 |  9017 | `	iFlags = pVm->iAssertFlags;` |
|       15 |  9018 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - |  9019 | `		/* Assertion is disabled,return FALSE */` |
|      ! 0 |  9020 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9021 | `		return PH7_OK;` |
|        - |  9022 | `	}` |
|       15 |  9023 | `	pAssert = apArg[0];` |
|       15 |  9024 | `	iResult = 1; /* cc warning */` |
|       15 |  9025 | `	if( pAssert->iFlags & MEMOBJ_STRING ){` |
|        - |  9026 | `		SyString sChunk;` |
|        5 |  9027 | `		SyStringInitFromBuf(&sChunk,SyBlobData(&pAssert->sBlob),SyBlobLength(&pAssert->sBlob));` |
|        5 |  9028 | `		if( sChunk.nByte > 0 ){` |
|        5 |  9029 | `			VmEvalChunk(pVm,pCtx,&sChunk,PH7_PHP_ONLY\|PH7_PHP_EXPR,FALSE);` |
|        - |  9030 | `			/* Extract evaluation result */` |
|        5 |  9031 | `			iResult = ph7_value_to_bool(pCtx->pRet);` |
|        3 |  9032 | `		}else{` |
|      ! 0 |  9033 | `			iResult = 0;` |
|        - |  9034 | `		}` |
|        3 |  9035 | `	}else{` |
|        - |  9036 | `		/* Perform a boolean cast */` |
|       11 |  9037 | `		iResult = ph7_value_to_bool(apArg[0]);` |
|        - |  9038 | `	}` |
|       15 |  9039 | `	if( !iResult ){` |
|        - |  9040 | `		/* Assertion failed */` |
|        9 |  9041 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - |  9042 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - |  9043 | `			ph7_value sFile,sLine;` |
|        - |  9044 | `			ph7_value *apCbArg[3];` |
|        - |  9045 | `			SyString *pFile;` |
|        - |  9046 | `			/* Extract the processed script */` |
|      ! 0 |  9047 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 |  9048 | `			if( pFile == 0 ){` |
|      ! 0 |  9049 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 |  9050 | `			}` |
|        - |  9051 | `			/* Invoke the callback */` |
|      ! 0 |  9052 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 |  9053 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 |  9054 | `			apCbArg[0] = &sFile;` |
|      ! 0 |  9055 | `			apCbArg[1] = &sLine;` |
|      ! 0 |  9056 | `			apCbArg[2] = pAssert;` |
|      ! 0 |  9057 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - |  9058 | `			/* Clean-up the mess left behind */` |
|      ! 0 |  9059 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 |  9060 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 |  9061 | `		}` |
|        9 |  9062 | `		if( iFlags & PH7_ASSERT_WARNING ){` |
|        - |  9063 | `			/* Emit a warning */` |
|        9 |  9064 | `			ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Assertion failed");` |
|        4 |  9065 | `		}` |
|        9 |  9066 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - |  9067 | `			/* Abort VM execution immediately */` |
|        3 |  9068 | `			return PH7_ABORT;` |
|        - |  9069 | `		}` |
|        3 |  9070 | `	}` |
|        - |  9071 | `	/* Assertion result */` |
|       13 |  9072 | `	ph7_result_bool(pCtx,iResult);` |
|       13 |  9073 | `	return PH7_OK;` |
|        8 |  9074 |  |
|        - |  9075 | `/*` |
|        - |  9076 | ` * Section:` |
|        - |  9077 | ` *  Error reporting functions.` |
|        - |  9078 | ` * Status:` |
|        - |  9079 | ` *    Stable.` |
|        - |  9080 | ` */` |
|        - |  9081 | `/*` |
|        - |  9082 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - |  9083 | ` *  Generates a user-level error/warning/notice message.` |
|        - |  9084 | ` * Parameters` |
|        - |  9085 | ` *  $error_msg` |
|        - |  9086 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - |  9087 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - |  9088 | ` * $error_type` |
|        - |  9089 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - |  9090 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - |  9091 | ` * Return` |
|        - |  9092 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - |  9093 | ` */` |
|       12 |  9094 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9095 |  |
|       14 |  9096 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 |  9097 | `	int rc = PH7_OK;` |
|       14 |  9098 | `	if( nArg > 0 ){` |
|        - |  9099 | `		const char *zErr;` |
|        - |  9100 | `		int nLen;` |
|        - |  9101 | `		/* Extract the error message */` |
|       12 |  9102 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  9103 | `		if( nArg > 1 ){` |
|        - |  9104 | `			/* Extract the error type */` |
|       12 |  9105 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 |  9106 | `			switch( nErr ){` |
|        1 |  9107 | `			case 1:   /* E_ERROR */` |
|        - |  9108 | `			case 16:  /* E_CORE_ERROR */` |
|        - |  9109 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - |  9110 | `			case 256: /* E_USER_ERROR */` |
|        3 |  9111 | `				nErr = PH7_CTX_ERR;` |
|        3 |  9112 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 |  9113 | `				break;` |
|        1 |  9114 | `			case 2:   /* E_WARNING */` |
|        - |  9115 | `			case 32:  /* E_CORE_WARNING */` |
|        - |  9116 | `			case 123: /* E_COMPILE_WARNING */` |
|        - |  9117 | `			case 512: /* E_USER_WARNING */` |
|        3 |  9118 | `				nErr = PH7_CTX_WARNING;` |
|        3 |  9119 | `				break;` |
|        3 |  9120 | `			default:` |
|        8 |  9121 | `				nErr = PH7_CTX_NOTICE;` |
|        6 |  9122 | `				break;` |
|        - |  9123 | `			}` |
|        5 |  9124 | `		}` |
|        - |  9125 | `		/* Report error */` |
|       12 |  9126 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 |  9127 | `		if( rc == PH7_ABORT ){` |
|      ! 0 |  9128 | `			return rc;` |
|        - |  9129 | `		}` |
|        - |  9130 | `		/* Return true */` |
|       12 |  9131 | `		ph7_result_bool(pCtx,1);` |
|        7 |  9132 | `	}else{` |
|        - |  9133 | `		/* Missing arguments,return FALSE */` |
|        3 |  9134 | `		ph7_result_bool(pCtx,0);` |
|        - |  9135 | `	}` |
|       14 |  9136 | `	return rc;` |
|        8 |  9137 |  |
|        - |  9138 | `/*` |
|        - |  9139 | ` * int error_reporting([int $level])` |
|        - |  9140 | ` *  Sets which PHP errors are reported.` |
|        - |  9141 | ` * Parameters` |
|        - |  9142 | ` *  $level` |
|        - |  9143 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - |  9144 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - |  9145 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - |  9146 | ` *   levels will not always behave as expected.` |
|        - |  9147 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - |  9148 | ` *   in the predefined constants.` |
|        - |  9149 | ` * Return` |
|        - |  9150 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - |  9151 | ` *   parameter is given.` |
|        - |  9152 | ` */` |
|       18 |  9153 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9154 |  |
|       19 |  9155 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9156 | `	int nOld;` |
|        - |  9157 | `	/* Extract the old reporting level */` |
|       19 |  9158 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       19 |  9159 | `	if( nArg > 0 ){` |
|        - |  9160 | `		int nNew;` |
|        - |  9161 | `		/* Extract the desired error reporting level */` |
|       11 |  9162 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       11 |  9163 | `		if( !nNew ){` |
|        - |  9164 | `			/* Do not report errors at all */` |
|        5 |  9165 | `			pVm->bErrReport = 0;` |
|        3 |  9166 | `		}else{` |
|        - |  9167 | `			/* Report all errors */` |
|        7 |  9168 | `			pVm->bErrReport = 1;` |
|        - |  9169 | `		}` |
|        5 |  9170 | `	}` |
|        - |  9171 | `	/* Return the old level */` |
|       19 |  9172 | `	ph7_result_int(pCtx,nOld);` |
|       19 |  9173 | `	return PH7_OK;` |
|        1 |  9174 |  |
|        - |  9175 | `/*` |
|        - |  9176 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - |  9177 | ` *  Send an error message somewhere.` |
|        - |  9178 | ` * Parameter` |
|        - |  9179 | ` *  $message` |
|        - |  9180 | ` *   The error message that should be logged.` |
|        - |  9181 | ` *  $message_type` |
|        - |  9182 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - |  9183 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - |  9184 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - |  9185 | ` *       This is the default option.` |
|        - |  9186 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - |  9187 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - |  9188 | ` *    2  No longer an option.` |
|        - |  9189 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - |  9190 | ` *       to the end of the message string.` |
|        - |  9191 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - |  9192 | ` *  $destination` |
|        - |  9193 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - |  9194 | ` *  $extra_headers` |
|        - |  9195 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - |  9196 | ` * Return` |
|        - |  9197 | ` *  TRUE on success or FALSE on failure.` |
|        - |  9198 | ` * NOTE:` |
|        - |  9199 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - |  9200 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - |  9201 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - |  9202 | ` *  Otherwise this function is no-op.` |
|        - |  9203 | ` */` |
|        4 |  9204 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9205 |  |
|        - |  9206 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 |  9207 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 |  9208 | `	int iType = 0;` |
|        5 |  9209 | `	if( nArg < 1 ){` |
|        - |  9210 | `		/* Missing log message,return FALSE */` |
|      ! 0 |  9211 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9212 | `		return PH7_OK;` |
|        - |  9213 | `	}` |
|        5 |  9214 | `	if( pVm->xErrLog  ){` |
|        - |  9215 | `		/* Invoke the user callback */` |
|      ! 0 |  9216 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 |  9217 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 |  9218 | `		if( nArg > 1 ){` |
|      ! 0 |  9219 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 |  9220 | `			if( nArg > 2 ){` |
|      ! 0 |  9221 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 |  9222 | `				if( nArg > 3 ){` |
|      ! 0 |  9223 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 |  9224 | `				}` |
|      ! 0 |  9225 | `			}` |
|      ! 0 |  9226 | `		}` |
|      ! 0 |  9227 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 |  9228 | `	}` |
|        - |  9229 | `	/* Retun TRUE */` |
|        5 |  9230 | `	ph7_result_bool(pCtx,1);` |
|        5 |  9231 | `	return PH7_OK;` |
|        3 |  9232 |  |
|        - |  9233 | `/*` |
|        - |  9234 | ` * bool restore_exception_handler(void)` |
|        - |  9235 | ` *  Restores the previously defined exception handler function.` |
|        - |  9236 | ` * Parameter` |
|        - |  9237 | ` *  None` |
|        - |  9238 | ` * Return` |
|        - |  9239 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - |  9240 | ` */` |
|        4 |  9241 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9242 |  |
|        5 |  9243 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9244 | `	ph7_value *pOld,*pNew;` |
|        - |  9245 | `	/* Point to the old and the new handler */` |
|        5 |  9246 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 |  9247 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 |  9248 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  9249 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  9250 | `		SXUNUSED(apArg);` |
|        - |  9251 | `		/* No installed handler,return FALSE */` |
|        5 |  9252 | `		ph7_result_bool(pCtx,0);` |
|        5 |  9253 | `		return PH7_OK;` |
|        - |  9254 | `	}` |
|        - |  9255 | `	/* Copy the old handler */` |
|      ! 0 |  9256 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  9257 | `	PH7_MemObjRelease(pOld);` |
|        - |  9258 | `	/* Return TRUE */` |
|      ! 0 |  9259 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  9260 | `	return PH7_OK;` |
|        3 |  9261 |  |
|        - |  9262 | `/*` |
|        - |  9263 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - |  9264 | ` *  Sets a user-defined exception handler function.` |
|        - |  9265 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - |  9266 | ` * NOTE` |
|        - |  9267 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - |  9268 | ` *  the satndard PHP engine.` |
|        - |  9269 | ` * Parameters` |
|        - |  9270 | ` *  $exception_handler` |
|        - |  9271 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - |  9272 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - |  9273 | ` *   that was thrown.` |
|        - |  9274 | ` *  Note:` |
|        - |  9275 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  9276 | ` * Return` |
|        - |  9277 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - |  9278 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  9279 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  9280 | ` */` |
|        4 |  9281 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9282 |  |
|        5 |  9283 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9284 | `	ph7_value *pOld,*pNew;` |
|        - |  9285 | `	/* Point to the old and the new handler */` |
|        5 |  9286 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 |  9287 | `	pNew = &pVm->aExceptionCB[1];` |
|        - |  9288 | `	/* Return the old handler */` |
|        5 |  9289 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        5 |  9290 | `	if( nArg > 0 ){` |
|        5 |  9291 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  9292 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 |  9293 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 |  9294 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 |  9295 | `		}else{` |
|        5 |  9296 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  9297 | `			/* Install the new handler */` |
|        5 |  9298 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  9299 | `		}` |
|        2 |  9300 | `	}` |
|        5 |  9301 | `	return PH7_OK;` |
|        1 |  9302 |  |
|        - |  9303 | `/*` |
|        - |  9304 | ` * bool restore_error_handler(void)` |
|        - |  9305 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  9306 | ` * Parameters:` |
|        - |  9307 | ` *  None.` |
|        - |  9308 | ` * Return` |
|        - |  9309 | ` *  Always TRUE.` |
|        - |  9310 | ` */` |
|        4 |  9311 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9312 |  |
|        5 |  9313 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9314 | `	ph7_value *pOld,*pNew;` |
|        - |  9315 | `	/* Point to the old and the new handler */` |
|        5 |  9316 | `	pOld = &pVm->aErrCB[0];` |
|        5 |  9317 | `	pNew = &pVm->aErrCB[1];` |
|        5 |  9318 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  9319 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  9320 | `		SXUNUSED(apArg);` |
|        - |  9321 | `		/* No installed callback,return FALSE */` |
|        5 |  9322 | `		ph7_result_bool(pCtx,0);` |
|        5 |  9323 | `		return PH7_OK;` |
|        - |  9324 | `	}` |
|        - |  9325 | `	/* Copy the old callback */` |
|      ! 0 |  9326 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  9327 | `	PH7_MemObjRelease(pOld);` |
|        - |  9328 | `	/* Return TRUE */` |
|      ! 0 |  9329 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  9330 | `	return PH7_OK;` |
|        3 |  9331 |  |
|        - |  9332 | `/*` |
|        - |  9333 | ` * value set_error_handler(callable $error_handler)` |
|        - |  9334 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  9335 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  9336 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  9337 | ` *  Sets a user-defined error handler function.` |
|        - |  9338 | ` *  This function can be used for defining your own way of handling errors during` |
|        - |  9339 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - |  9340 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - |  9341 | ` *  conditions (using trigger_error()).` |
|        - |  9342 | ` * Parameters` |
|        - |  9343 | ` *  $error_handler` |
|        - |  9344 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - |  9345 | ` *   describing the error.` |
|        - |  9346 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - |  9347 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - |  9348 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - |  9349 | ` *   The function can be shown as:` |
|        - |  9350 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - |  9351 | ` *     errno` |
|        - |  9352 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - |  9353 | ` *   errstr` |
|        - |  9354 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - |  9355 | ` *   errfile` |
|        - |  9356 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - |  9357 | ` *     was raised in, as a string.` |
|        - |  9358 | ` *  Note:` |
|        - |  9359 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  9360 | ` * Return` |
|        - |  9361 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - |  9362 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  9363 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  9364 | ` */` |
|     5198 |  9365 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9366 |  |
|     5200 |  9367 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9368 | `	ph7_value *pOld,*pNew;` |
|        - |  9369 | `	/* Point to the old and the new handler */` |
|     5200 |  9370 | `	pOld = &pVm->aErrCB[0];` |
|     5200 |  9371 | `	pNew = &pVm->aErrCB[1];` |
|        - |  9372 | `	/* Return the old handler */` |
|     5200 |  9373 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     5200 |  9374 | `	if( nArg > 0 ){` |
|     5200 |  9375 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  9376 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     2599 |  9377 | `			PH7_MemObjRelease(pNew);` |
|     2599 |  9378 | `			ph7_result_bool(pCtx,1);` |
|     1300 |  9379 | `		}else{` |
|     2602 |  9380 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  9381 | `			/* Install the new handler */` |
|     2602 |  9382 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  9383 | `		}` |
|     2599 |  9384 | `	}` |
|     5200 |  9385 | `	return PH7_OK;` |
|        2 |  9386 |  |
|        - |  9387 | `/*` |
|        - |  9388 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - |  9389 | ` *  Generates a backtrace.` |
|        - |  9390 | ` * Paramaeter` |
|        - |  9391 | ` *  $options` |
|        - |  9392 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - |  9393 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - |  9394 | ` *   all the function/method arguments, to save memory.` |
|        - |  9395 | ` * $limit` |
|        - |  9396 | ` *   (Not Used)` |
|        - |  9397 | ` * Return` |
|        - |  9398 | ` *  An array.The possible returned elements are as follows:` |
|        - |  9399 | ` *          Possible returned elements from debug_backtrace()` |
|        - |  9400 | ` *          Name        Type      Description` |
|        - |  9401 | ` *          ------      ------     -----------` |
|        - |  9402 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - |  9403 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - |  9404 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - |  9405 | ` *          class       string    The current class name. See also __CLASS__` |
|        - |  9406 | ` *          object      object    The current object.` |
|        - |  9407 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - |  9408 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - |  9409 | ` */` |
|       18 |  9410 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9411 |  |
|       20 |  9412 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9413 | `	ph7_value *pArray;` |
|        - |  9414 | `	ph7_class *pClass;` |
|        - |  9415 | `	ph7_value *pValue;` |
|        - |  9416 | `	SyString *pFile;` |
|        - |  9417 | `	/* Create a new array */` |
|       20 |  9418 | `	pArray = ph7_context_new_array(pCtx);` |
|       20 |  9419 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       20 |  9420 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  9421 | `		/* Out of memory,return NULL */` |
|      ! 0 |  9422 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 |  9423 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9424 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9425 | `		SXUNUSED(apArg);` |
|      ! 0 |  9426 | `		return PH7_OK;` |
|        - |  9427 | `	}` |
|        - |  9428 | `	/* Dump running function name and it's arguments  */` |
|       20 |  9429 | `	if( pVm->pFrame->pParent ){` |
|       20 |  9430 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  9431 | `		ph7_vm_func *pFunc;` |
|        - |  9432 | `		ph7_value *pArg;` |
|       20 |  9433 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  9434 | `			/* Safely ignore the exception frame */` |
|      ! 0 |  9435 | `			pFrame = pFrame->pParent;` |
|      ! 0 |  9436 | `		}` |
|       20 |  9437 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       20 |  9438 | `		if( pFrame->pParent && pFunc ){` |
|       20 |  9439 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|       20 |  9440 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|       20 |  9441 | `			ph7_value_reset_string_cursor(pValue);` |
|        9 |  9442 | `		}` |
|        - |  9443 | `		/* Function arguments */` |
|       20 |  9444 | `		pArg = ph7_context_new_array(pCtx);` |
|       20 |  9445 | `		if( pArg  ){` |
|        - |  9446 | `			ph7_value *pObj;` |
|        - |  9447 | `			VmSlot *aSlot;` |
|        - |  9448 | `			sxu32 n;` |
|        - |  9449 | `			/* Start filling the array with the given arguments */` |
|       20 |  9450 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|       66 |  9451 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|       48 |  9452 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|       48 |  9453 | `				if( pObj ){` |
|       48 |  9454 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|       23 |  9455 | `				}` |
|       25 |  9456 | `			}` |
|        - |  9457 | `			/* Save the array */` |
|       20 |  9458 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|        9 |  9459 | `		}` |
|        9 |  9460 | `	}` |
|       20 |  9461 | `	ph7_value_int(pValue,1);` |
|        - |  9462 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - |  9463 | `	 * line numbers at run-time. )` |
|        - |  9464 | `	 */` |
|       20 |  9465 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - |  9466 | `	/* Current processed script */` |
|       20 |  9467 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       20 |  9468 | `	if( pFile ){` |
|       20 |  9469 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|       20 |  9470 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|       20 |  9471 | `		ph7_value_reset_string_cursor(pValue);` |
|        9 |  9472 | `	}` |
|        - |  9473 | `	/* Top class */` |
|       20 |  9474 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|       20 |  9475 | `	if( pClass ){` |
|       16 |  9476 | `		ph7_value_reset_string_cursor(pValue);` |
|       16 |  9477 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       16 |  9478 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|        7 |  9479 | `	}` |
|        - |  9480 | `	/* Return the freshly created array */` |
|       20 |  9481 | `	ph7_result_value(pCtx,pArray);` |
|        - |  9482 | `	/*` |
|        - |  9483 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - |  9484 | `	 * as soon we return from this function.` |
|        - |  9485 | `	 */` |
|       20 |  9486 | `	return PH7_OK;` |
|       11 |  9487 |  |
|        - |  9488 | `/*` |
|        - |  9489 | ` * Generate a small backtrace.` |
|        - |  9490 | ` * Store the generated dump in the given BLOB` |
|        - |  9491 | ` */` |
|        4 |  9492 | `static int VmMiniBacktrace(` |
|        - |  9493 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  9494 | `	SyBlob *pOut /* Store Dump here */` |
|        - |  9495 | `	)` |
|        1 |  9496 |  |
|        5 |  9497 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  9498 | `	ph7_vm_func *pFunc;` |
|        - |  9499 | `	ph7_class *pClass;` |
|        - |  9500 | `	SyString *pFile;` |
|        - |  9501 | `	/* Called function */` |
|        5 |  9502 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  9503 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  9504 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  9505 | `	}` |
|        5 |  9506 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 |  9507 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  9508 | `	if( pFrame->pParent && pFunc ){` |
|        5 |  9509 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 |  9510 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 |  9511 | `	}else{` |
|      ! 0 |  9512 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - |  9513 | `	}` |
|        5 |  9514 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - |  9515 | `	/* Current processed script */` |
|        5 |  9516 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 |  9517 | `	if( pFile ){` |
|        5 |  9518 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  9519 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 |  9520 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 |  9521 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 |  9522 | `	}` |
|        - |  9523 | `	/* Top class */` |
|        5 |  9524 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 |  9525 | `	if( pClass ){` |
|      ! 0 |  9526 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 |  9527 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 |  9528 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 |  9529 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 |  9530 | `	}` |
|        5 |  9531 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - |  9532 | `	/* All done */` |
|        5 |  9533 | `	return SXRET_OK;` |
|        1 |  9534 |  |
|        - |  9535 | `/*` |
|        - |  9536 | ` * void debug_print_backtrace()` |
|        - |  9537 | ` *  Prints a backtrace` |
|        - |  9538 | ` * Parameters` |
|        - |  9539 | ` * None` |
|        - |  9540 | ` * Return` |
|        - |  9541 | ` * NULL` |
|        - |  9542 | ` */` |
|        2 |  9543 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9544 |  |
|        3 |  9545 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9546 | `	SyBlob sDump;` |
|        3 |  9547 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  9548 | `	/* Generate the backtrace */` |
|        3 |  9549 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  9550 | `	/* Output backtrace */` |
|        3 |  9551 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9552 | `	/* All done,cleanup */` |
|        3 |  9553 | `	SyBlobRelease(&sDump);` |
|        1 |  9554 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  9555 | `	SXUNUSED(apArg);` |
|        3 |  9556 | `	return PH7_OK;` |
|        1 |  9557 |  |
|        - |  9558 | `/*` |
|        - |  9559 | ` * string debug_string_backtrace()` |
|        - |  9560 | ` *  Generate a backtrace` |
|        - |  9561 | ` * Parameters` |
|        - |  9562 | ` * None` |
|        - |  9563 | ` * Return` |
|        - |  9564 | ` *  A mini backtrace().` |
|        - |  9565 | ` * Note that this is a symisc extension.` |
|        - |  9566 | ` */` |
|        2 |  9567 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9568 |  |
|        3 |  9569 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9570 | `	SyBlob sDump;` |
|        3 |  9571 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  9572 | `	/* Generate the backtrace */` |
|        3 |  9573 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  9574 | `	/* Return the backtrace */` |
|        3 |  9575 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - |  9576 | `	/* All done,cleanup */` |
|        3 |  9577 | `	SyBlobRelease(&sDump);` |
|        1 |  9578 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  9579 | `	SXUNUSED(apArg);` |
|        3 |  9580 | `	return PH7_OK;` |
|        1 |  9581 |  |
|        - |  9582 | `/*` |
|        - |  9583 | ` * The following routine is invoked by the engine when an uncaught` |
|        - |  9584 | ` * exception is triggered.` |
|        - |  9585 | ` */` |
|        4 |  9586 | `static sxi32 VmUncaughtException(` |
|        - |  9587 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  9588 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  9589 | `	)` |
|        2 |  9590 |  |
|        - |  9591 | `	ph7_value *apArg[2],sArg;` |
|        6 |  9592 | `	int nArg = 1;` |
|        - |  9593 | `	sxi32 rc;` |
|        6 |  9594 | `	if( pVm->nExceptDepth > 15 ){` |
|        - |  9595 | `		/* Nesting limit reached */` |
|      ! 0 |  9596 | `		return SXRET_OK;` |
|        - |  9597 | `	}` |
|        - |  9598 | `	/* Call any exception handler if available */` |
|        6 |  9599 | `	PH7_MemObjInit(pVm,&sArg);` |
|        6 |  9600 | `	if( pThis ){` |
|        - |  9601 | `		/* Load the exception instance */` |
|        6 |  9602 | `		sArg.x.pOther = pThis;` |
|        6 |  9603 | `		pThis->iRef++;` |
|        6 |  9604 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|        4 |  9605 | `	}else{` |
|      ! 0 |  9606 | `		nArg = 0;` |
|        - |  9607 | `	}` |
|        6 |  9608 | `	apArg[0] = &sArg;` |
|        - |  9609 | `	/* Call the exception handler if available */` |
|        6 |  9610 | `	pVm->nExceptDepth++;` |
|        6 |  9611 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|        6 |  9612 | `	pVm->nExceptDepth--;` |
|        6 |  9613 | `	if( rc != SXRET_OK ){` |
|        3 |  9614 | `		SyString sName = { "Exception" , sizeof("Exception") - 1 };` |
|        3 |  9615 | `		SyString sFuncName = { "Global",sizeof("Global") - 1 };` |
|        3 |  9616 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  9617 | `		/* No available handler,generate a fatal error */` |
|        3 |  9618 | `		if( pThis ){` |
|        3 |  9619 | `			SyStringDupPtr(&sName,&pThis->pClass->sName);` |
|        1 |  9620 | `		}` |
|        3 |  9621 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  9622 | `			/* Ignore exception frames */` |
|      ! 0 |  9623 | `			pFrame = pFrame->pParent;` |
|      ! 0 |  9624 | `		}` |
|        3 |  9625 | `		if( pFrame->pParent ){` |
|      ! 0 |  9626 | `			if( pFrame->iFlags & VM_FRAME_CATCH ){` |
|      ! 0 |  9627 | `				SyStringInitFromBuf(&sFuncName,"Catch_block",sizeof("Catch_block")-1);` |
|      ! 0 |  9628 | `			}else{` |
|      ! 0 |  9629 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      ! 0 |  9630 | `				if( pFunc ){` |
|      ! 0 |  9631 | `					SyStringDupPtr(&sFuncName,&pFunc->sName);` |
|      ! 0 |  9632 | `				}` |
|        - |  9633 | `			}` |
|      ! 0 |  9634 | `		}` |
|        - |  9635 | `		/* Generate a listing */` |
|        3 |  9636 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  9637 | `			"Uncaught exception '%z' in the '%z' frame context",` |
|        - |  9638 | `			&sName,&sFuncName);` |
|        - |  9639 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|        3 |  9640 | `		rc = SXERR_ABORT;` |
|        1 |  9641 | `	}` |
|        6 |  9642 | `	PH7_MemObjRelease(&sArg);` |
|        6 |  9643 | `	return rc;` |
|        4 |  9644 |  |
|        - |  9645 | `/*` |
|        - |  9646 | ` * Throw an user exception.` |
|        - |  9647 | ` */` |
|       16 |  9648 | `static sxi32 VmThrowException(` |
|        - |  9649 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |  9650 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  9651 | `	)` |
|        2 |  9652 |  |
|        - |  9653 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - |  9654 | `	ph7_exception **apException;` |
|        - |  9655 | `	ph7_exception *pException;` |
|        - |  9656 | `	/* Point to the stack of loaded exceptions */` |
|       18 |  9657 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       18 |  9658 | `	pException = 0;` |
|       18 |  9659 | `	pCatch = 0;` |
|       18 |  9660 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  9661 | `		ph7_exception_block *aCatch;` |
|        - |  9662 | `		ph7_class *pClass;` |
|        - |  9663 | `		sxu32 j;` |
|        - |  9664 | `		/* Locate the appropriate block to execute */` |
|       14 |  9665 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       14 |  9666 | `		(void)SySetPop(&pVm->aException);` |
|       14 |  9667 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       14 |  9668 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       14 |  9669 | `			SyString *pName = &aCatch[j].sClass;` |
|        - |  9670 | `			/* Extract the target class */` |
|       14 |  9671 | `			pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       14 |  9672 | `			if( pClass == 0 ){` |
|        - |  9673 | `				/* No such class */` |
|      ! 0 |  9674 | `				continue;` |
|        - |  9675 | `			}` |
|       14 |  9676 | `			if( VmInstanceOf(pThis->pClass,pClass) ){` |
|        - |  9677 | `				/* Catch block found,break immeditaley */` |
|       14 |  9678 | `				pCatch = &aCatch[j];` |
|       14 |  9679 | `				break;` |
|        - |  9680 | `			}` |
|      ! 0 |  9681 | `		}` |
|        6 |  9682 | `	}` |
|        - |  9683 | `	/* Execute the cached block if available */` |
|       18 |  9684 | `	if( pCatch == 0 ){` |
|        - |  9685 | `		sxi32 rc;` |
|        6 |  9686 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|        6 |  9687 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 |  9688 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 |  9689 | `			while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  9690 | `				/* Safely ignore the exception frame */` |
|      ! 0 |  9691 | `				pFrame = pFrame->pParent;` |
|      ! 0 |  9692 | `			}` |
|      ! 0 |  9693 | `			if( pException->pFrame == pFrame ){` |
|        - |  9694 | `				/* Tell the upper layer that the exception was caught */` |
|      ! 0 |  9695 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 |  9696 | `			}` |
|      ! 0 |  9697 | `		}` |
|        6 |  9698 | `		return rc;` |
|      ! 0 |  9699 | `	}else{` |
|       14 |  9700 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  9701 | `		sxi32 rc;` |
|       18 |  9702 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  9703 | `			/* Safely ignore the exception frame */` |
|        6 |  9704 | `			pFrame = pFrame->pParent;` |
|        2 |  9705 | `		}` |
|       14 |  9706 | `		if( pException->pFrame == pFrame ){` |
|        - |  9707 | `			/* Tell the upper layer that the exception was caught */` |
|        6 |  9708 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|        2 |  9709 | `		}` |
|        - |  9710 | `		/* Create a private frame first */` |
|       14 |  9711 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       14 |  9712 | `		if( rc == SXRET_OK ){` |
|        - |  9713 | `			/* Mark as catch frame */` |
|       14 |  9714 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       14 |  9715 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       14 |  9716 | `			if( pObj ){` |
|        - |  9717 | `				/* Install the exception instance */` |
|       14 |  9718 | `				pThis->iRef++; /* Increment reference count */` |
|       14 |  9719 | `				pObj->x.pOther = pThis;` |
|       14 |  9720 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|        6 |  9721 | `			}` |
|        - |  9722 | `			/* Exceute the block */` |
|       14 |  9723 | `			VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - |  9724 | `			/* Leave the frame */` |
|       14 |  9725 | `			VmLeaveFrame(&(*pVm));` |
|        6 |  9726 | `		}` |
|        - |  9727 | `	}` |
|        - |  9728 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - |  9729 | `	 * be used again if a 'goto' statement is executed.` |
|        - |  9730 | `	 */` |
|       14 |  9731 | `	return SXRET_OK;` |
|       10 |  9732 |  |
|        - |  9733 | `/*` |
|        - |  9734 | ` * Section:` |
|        - |  9735 | ` *  Version,Credits and Copyright related functions.` |
|        - |  9736 | ` * Status:` |
|        - |  9737 | ` *    Stable.` |
|        - |  9738 | ` */` |
|        - |  9739 | `/*` |
|        - |  9740 | ` * string ph7version(void)` |
|        - |  9741 | ` *  Returns the running version of the PH7 version.` |
|        - |  9742 | ` * Parameters` |
|        - |  9743 | ` *  None` |
|        - |  9744 | ` * Return` |
|        - |  9745 | ` * Current PH7 version.` |
|        - |  9746 | ` */` |
|        2 |  9747 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9748 |  |
|        1 |  9749 | `	SXUNUSED(nArg);` |
|        1 |  9750 | `	SXUNUSED(apArg); /* cc warning */` |
|        - |  9751 | `	/* Current engine version */` |
|        3 |  9752 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 |  9753 | `	return PH7_OK;` |
|        1 |  9754 |  |
|        - |  9755 | `/*` |
|        - |  9756 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - |  9757 | ` */` |
|        - |  9758 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - |  9759 | ` "<html><head>"\` |
|        - |  9760 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - |  9761 | ` "<style type=\"text/css\">"\` |
|        - |  9762 | ` "div {"\` |
|        - |  9763 | `     "border: 1px solid #cccccc;"\` |
|        - |  9764 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - |  9765 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - |  9766 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - |  9767 | `     "-moz-border-radius-topright: 10px;"\` |
|        - |  9768 | `     "-webkit-border-radius: 10px;"\` |
|        - |  9769 | `     "-o-border-radius: 10px;"\` |
|        - |  9770 | `     "border-radius: 10px;"\` |
|        - |  9771 | `     "padding-left: 2em;"\` |
|        - |  9772 | `     "background-color: white;"\` |
|        - |  9773 | `     "margin-left: auto;"\` |
|        - |  9774 | `     "font-family: verdana;"\` |
|        - |  9775 | `     "padding-right: 2em;"\` |
|        - |  9776 | `     "margin-right: auto;"\` |
|        - |  9777 | `     "}"\` |
|        - |  9778 | `     "body {"\` |
|        - |  9779 | `     "padding: 0.2em;"\` |
|        - |  9780 | `     "font-style: normal;"\` |
|        - |  9781 | `     "font-size: medium;"\` |
|        - |  9782 | `     "background-color: #f2f2f2;"\` |
|        - |  9783 | `     "}"\` |
|        - |  9784 | `     "hr {"\` |
|        - |  9785 | `     "border-style: solid none none;"\` |
|        - |  9786 | `     "border-width: 1px medium medium;"\` |
|        - |  9787 | `     "border-top: 1px solid #cccccc;"\` |
|        - |  9788 | `     "height: 1px;"\` |
|        - |  9789 | `     "}"\` |
|        - |  9790 | `     "a {"\` |
|        - |  9791 | `     "color: #3366cc;"\` |
|        - |  9792 | `     "text-decoration: none;"\` |
|        - |  9793 | `     "}"\` |
|        - |  9794 | `     "a:hover {"\` |
|        - |  9795 | `     "color: #999999;"\` |
|        - |  9796 | `     "}"\` |
|        - |  9797 | `     "a:active {"\` |
|        - |  9798 | `     "color: #663399;"\` |
|        - |  9799 | `     "}"\` |
|        - |  9800 | `     "h1 {"\` |
|        - |  9801 | `     "margin: 0;"\` |
|        - |  9802 | `     "padding: 0;"\` |
|        - |  9803 | `     "font-family: Verdana;"\` |
|        - |  9804 | `     "font-weight: bold;"\` |
|        - |  9805 | `     "font-style: normal;"\` |
|        - |  9806 | `     "font-size: medium;"\` |
|        - |  9807 | `     "text-transform: capitalize;"\` |
|        - |  9808 | `     "color: #0a328c;"\` |
|        - |  9809 | `     "}"\` |
|        - |  9810 | `     "p {"\` |
|        - |  9811 | `     "margin: 0 auto;"\` |
|        - |  9812 | `     "font-size: medium;"\` |
|        - |  9813 | `     "font-style: normal;"\` |
|        - |  9814 | `     "font-family: verdana;"\` |
|        - |  9815 | `     "}"\` |
|        - |  9816 | `"</style></head><body>"\` |
|        - |  9817 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - |  9818 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - |  9819 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - |  9820 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - |  9821 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - |  9822 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - |  9823 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - |  9824 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - |  9825 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - |  9826 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - |  9827 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - |  9828 |  |
|        - |  9829 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  9830 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - |  9831 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - |  9832 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - |  9833 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  9834 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - |  9835 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - |  9836 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - |  9837 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - |  9838 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - |  9839 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  9840 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - |  9841 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - |  9842 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - |  9843 |  |
|        - |  9844 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - |  9845 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - |  9846 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - |  9847 | `"&nbsp;*<br>"\` |
|        - |  9848 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - |  9849 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - |  9850 | `"&nbsp;* are met:<br>"\` |
|        - |  9851 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - |  9852 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - |  9853 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - |  9854 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - |  9855 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - |  9856 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - |  9857 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - |  9858 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - |  9859 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - |  9860 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - |  9861 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - |  9862 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - |  9863 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - |  9864 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - |  9865 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - |  9866 | `"&nbsp;*<br>"\` |
|        - |  9867 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - |  9868 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - |  9869 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - |  9870 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - |  9871 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - |  9872 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - |  9873 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - |  9874 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - |  9875 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - |  9876 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - |  9877 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - |  9878 | `"&nbsp;*/<br>"\` |
|        - |  9879 | `"</span></small></small></p>"\` |
|        - |  9880 | `"</div></body></html>"` |
|        - |  9881 | `/*` |
|        - |  9882 | ` * bool ph7credits(void)` |
|        - |  9883 | ` * bool ph7info(void)` |
|        - |  9884 | ` * bool ph7copyright(void)` |
|        - |  9885 | ` *  Prints out the credits for PH7 engine` |
|        - |  9886 | ` * Parameters` |
|        - |  9887 | ` *  None` |
|        - |  9888 | ` * Return` |
|        - |  9889 | ` *  Always TRUE` |
|        - |  9890 | ` */` |
|        2 |  9891 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9892 |  |
|        3 |  9893 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - |  9894 | `	/* Expand the HTML page above*/` |
|        3 |  9895 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 |  9896 | `	ph7_context_output_format(` |
|        1 |  9897 | `		pCtx,` |
|        - |  9898 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 |  9899 | `		ph7_lib_version(),   /* Engine version */` |
|        1 |  9900 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 |  9901 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 |  9902 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 |  9903 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 |  9904 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - |  9905 | `#ifdef __WINNT__` |
|        - |  9906 | `		"Windows NT"` |
|        - |  9907 | `#elif defined(__UNIXES__)` |
|        - |  9908 | `		"UNIX-Like"` |
|        - |  9909 | `#else` |
|        - |  9910 | `		"Other OS"` |
|        - |  9911 | `#endif` |
|        - |  9912 | `		);` |
|        3 |  9913 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 |  9914 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  9915 | `	SXUNUSED(apArg);` |
|        - |  9916 | `	/* Return TRUE */` |
|        - |  9917 | `	//ph7_result_bool(pCtx,1);` |
|        3 |  9918 | `	return PH7_OK;` |
|        1 |  9919 |  |
|        - |  9920 | `/*` |
|        - |  9921 | ` * Section:` |
|        - |  9922 | ` *    URL related routines.` |
|        - |  9923 | ` * Status:` |
|        - |  9924 | ` *    Stable.` |
|        - |  9925 | ` */` |
|        - |  9926 | `/* Forward declaration */` |
|        - |  9927 | `static sxi32 VmHttpSplitURI(SyhttpUri *pOut,const char *zUri,sxu32 nLen);` |
|        - |  9928 | `/*` |
|        - |  9929 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - |  9930 | ` *  Parse a URL and return its fields.` |
|        - |  9931 | ` * Parameters` |
|        - |  9932 | ` *  $url` |
|        - |  9933 | ` *   The URL to parse.` |
|        - |  9934 | ` * $component` |
|        - |  9935 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - |  9936 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - |  9937 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - |  9938 | ` *  in which case the return value will be an integer).` |
|        - |  9939 | ` * Return` |
|        - |  9940 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - |  9941 | ` *  At least one element will be present within the array. Potential keys within` |
|        - |  9942 | ` *  this array are:` |
|        - |  9943 | ` *   scheme - e.g. http` |
|        - |  9944 | ` *   host` |
|        - |  9945 | ` *   port` |
|        - |  9946 | ` *   user` |
|        - |  9947 | ` *   pass` |
|        - |  9948 | ` *   path` |
|        - |  9949 | ` *   query - after the question mark ?` |
|        - |  9950 | ` *   fragment - after the hashmark #` |
|        - |  9951 | ` * Note:` |
|        - |  9952 | ` *  FALSE is returned on failure.` |
|        - |  9953 | ` *  This function work with relative URL unlike the one shipped` |
|        - |  9954 | ` *  with the standard PHP engine.` |
|        - |  9955 | ` */` |
|       28 |  9956 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9957 |  |
|        - |  9958 | `	const char *zStr; /* Input string */` |
|        - |  9959 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - |  9960 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - |  9961 | `	int nLen;` |
|        - |  9962 | `	sxi32 rc;` |
|       29 |  9963 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  9964 | `		/* Missing/Invalid arguments,return FALSE */` |
|        3 |  9965 | `		ph7_result_bool(pCtx,0);` |
|        3 |  9966 | `		return PH7_OK;` |
|        - |  9967 | `	}` |
|        - |  9968 | `	/* Extract the given URI */` |
|       27 |  9969 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       27 |  9970 | `	if( nLen < 1 ){` |
|        - |  9971 | `		/* Nothing to process,return FALSE */` |
|      ! 0 |  9972 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9973 | `		return PH7_OK;` |
|        - |  9974 | `	}` |
|        - |  9975 | `	/* Get a parse */` |
|       27 |  9976 | `	rc = VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 |  9977 | `	if( rc != SXRET_OK ){` |
|        - |  9978 | `		/* Malformed input,return FALSE */` |
|      ! 0 |  9979 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9980 | `		return PH7_OK;` |
|        - |  9981 | `	}` |
|       27 |  9982 | `	if( nArg > 1 ){` |
|      ! 0 |  9983 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - |  9984 | `		/* Refer to constant.c for constants values */` |
|      ! 0 |  9985 | `		switch(nComponent){` |
|      ! 0 |  9986 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 |  9987 | `			pComp = &sURI.sScheme;` |
|      ! 0 |  9988 | `			if( pComp->nByte < 1 ){` |
|        - |  9989 | `				/* No available value,return NULL */` |
|      ! 0 |  9990 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9991 | `			}else{` |
|      ! 0 |  9992 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9993 | `			}` |
|      ! 0 |  9994 | `			break;` |
|      ! 0 |  9995 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 |  9996 | `			pComp = &sURI.sHost;` |
|      ! 0 |  9997 | `			if( pComp->nByte < 1 ){` |
|        - |  9998 | `				/* No available value,return NULL */` |
|      ! 0 |  9999 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10000 | `			}else{` |
|      ! 0 | 10001 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10002 | `			}` |
|      ! 0 | 10003 | `			break;` |
|      ! 0 | 10004 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 10005 | `			pComp = &sURI.sPort;` |
|      ! 0 | 10006 | `			if( pComp->nByte < 1 ){` |
|        - | 10007 | `				/* No available value,return NULL */` |
|      ! 0 | 10008 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10009 | `			}else{` |
|      ! 0 | 10010 | `				int iPort = 0;` |
|        - | 10011 | `				/* Cast the value to integer */` |
|      ! 0 | 10012 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 10013 | `				ph7_result_int(pCtx,iPort);` |
|        - | 10014 | `			}` |
|      ! 0 | 10015 | `			break;` |
|      ! 0 | 10016 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 10017 | `			pComp = &sURI.sUser;` |
|      ! 0 | 10018 | `			if( pComp->nByte < 1 ){` |
|        - | 10019 | `				/* No available value,return NULL */` |
|      ! 0 | 10020 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10021 | `			}else{` |
|      ! 0 | 10022 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10023 | `			}` |
|      ! 0 | 10024 | `			break;` |
|      ! 0 | 10025 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 10026 | `			pComp = &sURI.sPass;` |
|      ! 0 | 10027 | `			if( pComp->nByte < 1 ){` |
|        - | 10028 | `				/* No available value,return NULL */` |
|      ! 0 | 10029 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10030 | `			}else{` |
|      ! 0 | 10031 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10032 | `			}` |
|      ! 0 | 10033 | `			break;` |
|      ! 0 | 10034 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 10035 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 10036 | `			if( pComp->nByte < 1 ){` |
|        - | 10037 | `				/* No available value,return NULL */` |
|      ! 0 | 10038 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10039 | `			}else{` |
|      ! 0 | 10040 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10041 | `			}` |
|      ! 0 | 10042 | `			break;` |
|      ! 0 | 10043 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 10044 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 10045 | `			if( pComp->nByte < 1 ){` |
|        - | 10046 | `				/* No available value,return NULL */` |
|      ! 0 | 10047 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10048 | `			}else{` |
|      ! 0 | 10049 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10050 | `			}` |
|      ! 0 | 10051 | `			break;` |
|      ! 0 | 10052 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 10053 | `			pComp = &sURI.sPath;` |
|      ! 0 | 10054 | `			if( pComp->nByte < 1 ){` |
|        - | 10055 | `				/* No available value,return NULL */` |
|      ! 0 | 10056 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10057 | `			}else{` |
|      ! 0 | 10058 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10059 | `			}` |
|      ! 0 | 10060 | `			break;` |
|      ! 0 | 10061 | `		default:` |
|        - | 10062 | `			/* No such entry,return NULL */` |
|      ! 0 | 10063 | `			ph7_result_null(pCtx);` |
|      ! 0 | 10064 | `			break;` |
|        - | 10065 | `		}` |
|      ! 0 | 10066 | `	}else{` |
|        - | 10067 | `		ph7_value *pArray,*pValue;` |
|        - | 10068 | `		/* Return an associative array */` |
|       27 | 10069 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 10070 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 10071 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 10072 | `			/* Out of memory */` |
|      ! 0 | 10073 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 10074 | `			/* Return false */` |
|      ! 0 | 10075 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 10076 | `			return PH7_OK;` |
|        - | 10077 | `		}` |
|        - | 10078 | `		/* Fill the array */` |
|       27 | 10079 | `		pComp = &sURI.sScheme;` |
|       27 | 10080 | `		if( pComp->nByte > 0 ){` |
|       19 | 10081 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 10082 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 10083 | `		}` |
|        - | 10084 | `		/* Reset the string cursor */` |
|       27 | 10085 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10086 | `		pComp = &sURI.sHost;` |
|       27 | 10087 | `		if( pComp->nByte > 0 ){` |
|       25 | 10088 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 10089 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 10090 | `		}` |
|        - | 10091 | `		/* Reset the string cursor */` |
|       27 | 10092 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10093 | `		pComp = &sURI.sPort;` |
|       27 | 10094 | `		if( pComp->nByte > 0 ){` |
|       11 | 10095 | `			int iPort = 0;/* cc warning */` |
|        - | 10096 | `			/* Convert to integer */` |
|       11 | 10097 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 10098 | `			ph7_value_int(pValue,iPort);` |
|       11 | 10099 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 10100 | `		}` |
|        - | 10101 | `		/* Reset the string cursor */` |
|       27 | 10102 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10103 | `		pComp = &sURI.sUser;` |
|       27 | 10104 | `		if( pComp->nByte > 0 ){` |
|        7 | 10105 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 10106 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 10107 | `		}` |
|        - | 10108 | `		/* Reset the string cursor */` |
|       27 | 10109 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10110 | `		pComp = &sURI.sPass;` |
|       27 | 10111 | `		if( pComp->nByte > 0 ){` |
|        7 | 10112 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 10113 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 10114 | `		}` |
|        - | 10115 | `		/* Reset the string cursor */` |
|       27 | 10116 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10117 | `		pComp = &sURI.sPath;` |
|       27 | 10118 | `		if( pComp->nByte > 0 ){` |
|       17 | 10119 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 10120 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 10121 | `		}` |
|        - | 10122 | `		/* Reset the string cursor */` |
|       27 | 10123 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10124 | `		pComp = &sURI.sQuery;` |
|       27 | 10125 | `		if( pComp->nByte > 0 ){` |
|        5 | 10126 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 10127 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 10128 | `		}` |
|        - | 10129 | `		/* Reset the string cursor */` |
|       27 | 10130 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10131 | `		pComp = &sURI.sFragment;` |
|       27 | 10132 | `		if( pComp->nByte > 0 ){` |
|        5 | 10133 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 10134 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 10135 | `		}` |
|        - | 10136 | `		/* Return the created array */` |
|       27 | 10137 | `		ph7_result_value(pCtx,pArray);` |
|        - | 10138 | `		/* NOTE:` |
|        - | 10139 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 10140 | `		 * automatically as soon we return from this function.` |
|        - | 10141 | `		 */` |
|        - | 10142 | `	}` |
|        - | 10143 | `	/* All done */` |
|       27 | 10144 | `	return PH7_OK;` |
|       15 | 10145 |  |
|        - | 10146 | `/*` |
|        - | 10147 | ` * Section:` |
|        - | 10148 | ` *   Array related routines.` |
|        - | 10149 | ` * Status:` |
|        - | 10150 | ` *    Stable.` |
|        - | 10151 | ` * Note 2012-5-21 01:04:15:` |
|        - | 10152 | ` *  Array related functions that need access to the underlying` |
|        - | 10153 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 10154 | ` */` |
|        - | 10155 | `/*` |
|        - | 10156 | ` * The [compact()] function store it's state information in an instance` |
|        - | 10157 | ` * of the following structure.` |
|        - | 10158 | ` */` |
|        - | 10159 | `struct compact_data` |
|        - | 10160 |  |
|        - | 10161 | `	ph7_value *pArray;  /* Target array */` |
|        - | 10162 | `	int nRecCount;      /* Recursion count */` |
|        - | 10163 | `};` |
|        - | 10164 | `/*` |
|        - | 10165 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 10166 | ` */` |
|      ! 0 | 10167 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 10168 |  |
|      ! 0 | 10169 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 10170 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 10171 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 10172 | `	/* Act according to the hashmap value */` |
|      ! 0 | 10173 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 10174 | `		SyString sVar;` |
|      ! 0 | 10175 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 10176 | `		if( sVar.nByte > 0 ){` |
|        - | 10177 | `			/* Query the current frame */` |
|      ! 0 | 10178 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 10179 | `			/* ^` |
|        - | 10180 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 10181 | `			 */` |
|      ! 0 | 10182 | `			if( pKey ){` |
|        - | 10183 | `				/* Perform the insertion */` |
|      ! 0 | 10184 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 10185 | `			}` |
|      ! 0 | 10186 | `		}` |
|      ! 0 | 10187 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 10188 | `		int rc;` |
|        - | 10189 | `		/* Recursively traverse this array */` |
|      ! 0 | 10190 | `		pData->nRecCount++;` |
|      ! 0 | 10191 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 10192 | `		pData->nRecCount--;` |
|      ! 0 | 10193 | `		return rc;` |
|        - | 10194 | `	}` |
|      ! 0 | 10195 | `	return SXRET_OK;` |
|      ! 0 | 10196 |  |
|        - | 10197 | `/*` |
|        - | 10198 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 10199 | ` *  Create array containing variables and their values.` |
|        - | 10200 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 10201 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 10202 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 10203 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 10204 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 10205 | ` * Parameters` |
|        - | 10206 | ` *  $varname` |
|        - | 10207 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 10208 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 10209 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 10210 | ` *   it recursively.` |
|        - | 10211 | ` * Return` |
|        - | 10212 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 10213 | ` */` |
|        2 | 10214 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10215 |  |
|        - | 10216 | `	ph7_value *pArray,*pObj;` |
|        3 | 10217 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10218 | `	const char *zName;` |
|        - | 10219 | `	SyString sVar;` |
|        - | 10220 | `	int i,nLen;` |
|        3 | 10221 | `	if( nArg < 1 ){` |
|        - | 10222 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 10223 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10224 | `		return PH7_OK;` |
|        - | 10225 | `	}` |
|        - | 10226 | `	/* Create the array */` |
|        3 | 10227 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 10228 | `	if( pArray == 0 ){` |
|        - | 10229 | `		/* Out of memory */` |
|      ! 0 | 10230 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 10231 | `		/* Return NULL */` |
|      ! 0 | 10232 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10233 | `		return PH7_OK;` |
|        - | 10234 | `	}` |
|        - | 10235 | `	/* Perform the requested operation */` |
|        7 | 10236 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 10237 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 10238 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 10239 | `				struct compact_data sData;` |
|      ! 0 | 10240 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 10241 | `				/* Recursively walk the array */` |
|      ! 0 | 10242 | `				sData.nRecCount = 0;` |
|      ! 0 | 10243 | `				sData.pArray = pArray;` |
|      ! 0 | 10244 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 10245 | `			}` |
|      ! 0 | 10246 | `		}else{` |
|        - | 10247 | `			/* Extract variable name */` |
|        5 | 10248 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 10249 | `			if( nLen > 0 ){` |
|        5 | 10250 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 10251 | `				/* Check if the variable is available in the current frame */` |
|        5 | 10252 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 10253 | `				if( pObj ){` |
|        5 | 10254 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 10255 | `				}` |
|        2 | 10256 | `			}` |
|        - | 10257 | `		}` |
|        3 | 10258 | `	}` |
|        - | 10259 | `	/* Return the array */` |
|        3 | 10260 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 10261 | `	return PH7_OK;` |
|        2 | 10262 |  |
|        - | 10263 | `/*` |
|        - | 10264 | ` * The [extract()] function store it's state information in an instance` |
|        - | 10265 | ` * of the following structure.` |
|        - | 10266 | ` */` |
|        - | 10267 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 10268 | `struct extract_aux_data` |
|        - | 10269 |  |
|        - | 10270 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 10271 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 10272 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 10273 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 10274 | `	int iFlags;           /* Control flags */` |
|        - | 10275 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 10276 | `};` |
|        - | 10277 | `/* Forward declaration */` |
|        - | 10278 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 10279 | `/*` |
|        - | 10280 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 10281 | ` *   Import variables into the current symbol table from an array.` |
|        - | 10282 | ` * Parameters` |
|        - | 10283 | ` * $var_array` |
|        - | 10284 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 10285 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 10286 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 10287 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 10288 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 10289 | ` * $extract_type` |
|        - | 10290 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 10291 | ` *  It can be one of the following values:` |
|        - | 10292 | ` *   EXTR_OVERWRITE` |
|        - | 10293 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 10294 | ` *   EXTR_SKIP` |
|        - | 10295 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 10296 | ` *   EXTR_PREFIX_SAME` |
|        - | 10297 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 10298 | ` *   EXTR_PREFIX_ALL` |
|        - | 10299 | ` *       Prefix all variable names with prefix.` |
|        - | 10300 | ` *   EXTR_PREFIX_INVALID` |
|        - | 10301 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 10302 | ` *   EXTR_IF_EXISTS` |
|        - | 10303 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 10304 | ` *       otherwise do nothing.` |
|        - | 10305 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 10306 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 10307 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 10308 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 10309 | ` *      the current symbol table.` |
|        - | 10310 | ` * $prefix` |
|        - | 10311 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 10312 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 10313 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 10314 | ` *  underscore character.` |
|        - | 10315 | ` * Return` |
|        - | 10316 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 10317 | ` */` |
|        4 | 10318 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10319 |  |
|        - | 10320 | `	extract_aux_data sAux;` |
|        - | 10321 | `	ph7_hashmap *pMap;` |
|        5 | 10322 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 10323 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 10324 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 10325 | `		return PH7_OK;` |
|        - | 10326 | `	}` |
|        - | 10327 | `	/* Point to the target hashmap */` |
|        5 | 10328 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 10329 | `	if( pMap->nEntry < 1 ){` |
|        - | 10330 | `		/* Empty map,return  0 */` |
|      ! 0 | 10331 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 10332 | `		return PH7_OK;` |
|        - | 10333 | `	}` |
|        - | 10334 | `	/* Prepare the aux data */` |
|        5 | 10335 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 10336 | `	if( nArg > 1 ){` |
|        3 | 10337 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 10338 | `		if( nArg > 2 ){` |
|      ! 0 | 10339 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 10340 | `		}` |
|        1 | 10341 | `	}` |
|        5 | 10342 | `	sAux.pVm = pCtx->pVm;` |
|        - | 10343 | `	/* Invoke the worker callback */` |
|        5 | 10344 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 10345 | `	/* Number of variables successfully imported */` |
|        5 | 10346 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 10347 | `	return PH7_OK;` |
|        3 | 10348 |  |
|        - | 10349 | `/*` |
|        - | 10350 | ` * Worker callback for the [extract()] function defined` |
|        - | 10351 | ` * below.` |
|        - | 10352 | ` */` |
|        8 | 10353 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 10354 |  |
|        9 | 10355 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 10356 | `	int iFlags = pAux->iFlags;` |
|        9 | 10357 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 10358 | `	ph7_value *pObj;` |
|        - | 10359 | `	SyString sVar;` |
|        9 | 10360 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 10361 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 10362 | `	}` |
|        - | 10363 | `	/* Perform a string cast */` |
|        9 | 10364 | `	PH7_MemObjToString(pKey);` |
|        9 | 10365 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 10366 | `		/* Unavailable variable name */` |
|      ! 0 | 10367 | `		return SXRET_OK;` |
|        - | 10368 | `	}` |
|        9 | 10369 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 10370 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 10371 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 10372 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 10373 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 10374 | `			);` |
|      ! 0 | 10375 | `	}else{` |
|       13 | 10376 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 10377 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 10378 | `	}` |
|        9 | 10379 | `	sVar.zString = pAux->zWorker;` |
|        - | 10380 | `	/* Try to extract the variable */` |
|        9 | 10381 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 10382 | `	if( pObj ){` |
|        - | 10383 | `		/* Collision */` |
|        3 | 10384 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 10385 | `			return SXRET_OK;` |
|        - | 10386 | `		}` |
|        3 | 10387 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 10388 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 10389 | `				/* Already prefixed */` |
|      ! 0 | 10390 | `				return SXRET_OK;` |
|        - | 10391 | `			}` |
|      ! 0 | 10392 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 10393 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 10394 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 10395 | `				);` |
|      ! 0 | 10396 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 10397 | `		}` |
|        2 | 10398 | `	}else{` |
|        - | 10399 | `		/* Create the variable */` |
|        7 | 10400 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 10401 | `	}` |
|        9 | 10402 | `	if( pObj ){` |
|        - | 10403 | `		/* Overwrite the old value */` |
|        9 | 10404 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 10405 | `		/* Increment counter */` |
|        9 | 10406 | `		pAux->iCount++;` |
|        4 | 10407 | `	}` |
|        9 | 10408 | `	return SXRET_OK;` |
|        5 | 10409 |  |
|        - | 10410 | `/*` |
|        - | 10411 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 10412 | ` * defined below.` |
|        - | 10413 | ` */` |
|        2 | 10414 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 10415 |  |
|        3 | 10416 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 10417 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 10418 | `	ph7_value *pObj;` |
|        - | 10419 | `	SyString sVar;` |
|        - | 10420 | `	/* Perform a string cast */` |
|        3 | 10421 | `	PH7_MemObjToString(pKey);` |
|        3 | 10422 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 10423 | `		/* Unavailable variable name */` |
|      ! 0 | 10424 | `		return SXRET_OK;` |
|        - | 10425 | `	}` |
|        3 | 10426 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 10427 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 10428 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 10429 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 10430 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 10431 | `			);` |
|        2 | 10432 | `	}else{` |
|      ! 0 | 10433 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 10434 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 10435 | `	}` |
|        3 | 10436 | `	sVar.zString = pAux->zWorker;` |
|        - | 10437 | `	/* Extract the variable */` |
|        3 | 10438 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 10439 | `	if( pObj ){` |
|        3 | 10440 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 10441 | `	}` |
|        3 | 10442 | `	return SXRET_OK;` |
|        2 | 10443 |  |
|        - | 10444 | `/*` |
|        - | 10445 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 10446 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 10447 | ` * Parameters` |
|        - | 10448 | ` * $types` |
|        - | 10449 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 10450 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 10451 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 10452 | ` *  POST includes the POST uploaded file information.` |
|        - | 10453 | ` *  Note:` |
|        - | 10454 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 10455 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 10456 | ` * $prefix` |
|        - | 10457 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 10458 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 10459 | ` *  variable named $pref_userid.` |
|        - | 10460 | ` * Return` |
|        - | 10461 | ` *  TRUE on success or FALSE on failure.` |
|        - | 10462 | ` */` |
|        2 | 10463 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10464 |  |
|        - | 10465 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 10466 | `	extract_aux_data sAux;` |
|        - | 10467 | `	int nLen,nPrefixLen;` |
|        - | 10468 | `	ph7_value *pSuper;` |
|        - | 10469 | `	ph7_vm *pVm;` |
|        - | 10470 | `	/* By default import only $_GET variables  */` |
|        3 | 10471 | `	zImport = "G";` |
|        3 | 10472 | `	nLen = (int)sizeof(char);` |
|        3 | 10473 | `	zPrefix = 0;` |
|        3 | 10474 | `	nPrefixLen = 0;` |
|        3 | 10475 | `	if( nArg > 0 ){` |
|        3 | 10476 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 10477 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 10478 | `		}` |
|        3 | 10479 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 10480 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 10481 | `		}` |
|        1 | 10482 | `	}` |
|        - | 10483 | `	/* Point to the underlying VM */` |
|        3 | 10484 | `	pVm = pCtx->pVm;` |
|        - | 10485 | `	/* Initialize the aux data */` |
|        3 | 10486 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 10487 | `	sAux.zPrefix = zPrefix;` |
|        3 | 10488 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 10489 | `	sAux.pVm = pVm;` |
|        - | 10490 | `	/* Extract */` |
|        3 | 10491 | `	zEnd = &zImport[nLen];` |
|        5 | 10492 | `	while( zImport < zEnd ){` |
|        3 | 10493 | `		int c = zImport[0];` |
|        3 | 10494 | `		pSuper = 0;` |
|        3 | 10495 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 10496 | `			/* Import $_GET variables */` |
|        3 | 10497 | `			pSuper = VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 10498 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 10499 | `			/* Import $_POST variables */` |
|      ! 0 | 10500 | `			pSuper = VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 10501 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 10502 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 10503 | `			pSuper = VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 10504 | `		}` |
|        3 | 10505 | `		if( pSuper ){` |
|        - | 10506 | `			/* Iterate throw array entries */` |
|        3 | 10507 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 10508 | `		}` |
|        - | 10509 | `		/* Advance the cursor */` |
|        3 | 10510 | `		zImport++;` |
|        1 | 10511 | `	}` |
|        - | 10512 | `	/* All done,return TRUE*/` |
|        3 | 10513 | `	ph7_result_bool(pCtx,0);` |
|        3 | 10514 | `	return PH7_OK;` |
|        1 | 10515 |  |
|        - | 10516 | `/*` |
|        - | 10517 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 10518 | ` * Refer to the eval() language construct implementation for more` |
|        - | 10519 | ` * information.` |
|        - | 10520 | ` */` |
|     7664 | 10521 | `static sxi32 VmEvalChunk(` |
|        - | 10522 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 10523 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 10524 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 10525 | `	int iFlags,         /* Compile flag */` |
|        - | 10526 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 10527 | `	)` |
|        2 | 10528 |  |
|        - | 10529 | `	SySet *pByteCode,aByteCode;` |
|     7666 | 10530 | `	ProcConsumer xErr = 0;` |
|     7666 | 10531 | `	void *pErrData = 0;` |
|        - | 10532 | `	/* Initialize bytecode container */` |
|     7666 | 10533 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     7666 | 10534 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 10535 | `	/* Reset the code generator */` |
|     7666 | 10536 | `	if( bTrueReturn ){` |
|        - | 10537 | `		/* Included file,log compile-time errors */` |
|     6463 | 10538 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     6463 | 10539 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     3231 | 10540 | `	}` |
|     7666 | 10541 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 10542 | `	/* Swap bytecode container */` |
|     7666 | 10543 | `	pByteCode = pVm->pByteContainer;` |
|     7666 | 10544 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 10545 | `	/* Compile the chunk */` |
|     7666 | 10546 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    11498 | 10547 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 10548 | `		/* Compilation error,return false */` |
|        3 | 10549 | `		if( pCtx ){` |
|        3 | 10550 | `			ph7_result_bool(pCtx,0);` |
|        1 | 10551 | `		}` |
|        2 | 10552 | `	}else{` |
|        - | 10553 | `		/* Mount any newly defined classes */` |
|        - | 10554 | `		SyHashEntry *pEntry;` |
|        - | 10555 | `		ph7_class *pClass;` |
|        - | 10556 | `		ph7_value sResult; /* Return value */` |
|        - | 10557 | `		sxi32 rc;` |
|     7664 | 10558 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   190849 | 10559 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   179356 | 10560 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 10561 | `			/* Only mount classes that haven't been mounted yet */` |
|   179356 | 10562 | `			if( !pClass->bMounted ){` |
|    37074 | 10563 | `				rc = VmMountUserClass(pVm,pClass);` |
|    37074 | 10564 | `				if( rc != SXRET_OK ){` |
|        - | 10565 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 10566 | `					if( pCtx ){` |
|      ! 0 | 10567 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 10568 | `					}` |
|      ! 0 | 10569 | `					goto Cleanup;` |
|        - | 10570 | `				}` |
|    18536 | 10571 | `			}` |
|        2 | 10572 | `		}` |
|     7664 | 10573 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 10574 | `			/* Out of memory */` |
|      ! 0 | 10575 | `			if( pCtx ){` |
|      ! 0 | 10576 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 10577 | `			}` |
|      ! 0 | 10578 | `			goto Cleanup;` |
|        - | 10579 | `		}` |
|     7664 | 10580 | `		if( bTrueReturn ){` |
|        - | 10581 | `			/* Assume a boolean true return value */` |
|     6463 | 10582 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     3232 | 10583 | `		}else{` |
|        - | 10584 | `			/* Assume a null return value */` |
|     1202 | 10585 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 10586 | `		}` |
|        - | 10587 | `		/* Execute the compiled chunk */` |
|     7664 | 10588 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|     7664 | 10589 | `		if( pCtx ){` |
|        - | 10590 | `			/* Set the execution result */` |
|     6480 | 10591 | `			ph7_result_value(pCtx,&sResult);` |
|     3239 | 10592 | `		}` |
|     7664 | 10593 | `		PH7_MemObjRelease(&sResult);` |
|        - | 10594 | `	}` |
|     3832 | 10595 | `Cleanup:` |
|        - | 10596 | `	/* Cleanup the mess left behind */` |
|     7666 | 10597 | `	pVm->pByteContainer = pByteCode;` |
|     7666 | 10598 | `	SySetRelease(&aByteCode);` |
|     7666 | 10599 | `	return SXRET_OK;` |
|        2 | 10600 |  |
|        - | 10601 | `/*` |
|        - | 10602 | ` * value eval(string $code)` |
|        - | 10603 | ` *   Evaluate a string as PHP code.` |
|        - | 10604 | ` * Parameter` |
|        - | 10605 | ` *  code: PHP code to evaluate.` |
|        - | 10606 | ` * Return` |
|        - | 10607 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 10608 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 10609 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 10610 | ` */` |
|       16 | 10611 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10612 |  |
|        - | 10613 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       18 | 10614 | `	if( nArg < 1 ){` |
|        - | 10615 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10616 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10617 | `		return SXRET_OK;` |
|        - | 10618 | `	}` |
|        - | 10619 | `	/* Chunk to evaluate */` |
|       18 | 10620 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       18 | 10621 | `	if( sChunk.nByte < 1 ){` |
|        - | 10622 | `		/* Empty string,return NULL */` |
|        3 | 10623 | `		ph7_result_null(pCtx);` |
|        3 | 10624 | `		return SXRET_OK;` |
|        - | 10625 | `	}` |
|        - | 10626 | `	/* Eval the chunk */` |
|       16 | 10627 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       16 | 10628 | `	return SXRET_OK;` |
|       10 | 10629 |  |
|        - | 10630 | `/*` |
|        - | 10631 | ` * Check if a file path is already included.` |
|        - | 10632 | ` */` |
|    12920 | 10633 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        1 | 10634 |  |
|        - | 10635 | `	SyString *aEntries;` |
|        - | 10636 | `	sxu32 n;` |
|    12921 | 10637 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 10638 | `	/* Perform a linear search */` |
| 41720343 | 10639 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 41707429 | 10640 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 10641 | `			/* Already included */` |
|        7 | 10642 | `			return TRUE;` |
|        - | 10643 | `		}` |
| 20853712 | 10644 | `	}` |
|    12915 | 10645 | `	return FALSE;` |
|     6461 | 10646 |  |
|        - | 10647 | `/*` |
|        - | 10648 | ` * Push a file path in the appropriate VM container.` |
|        - | 10649 | ` */` |
|    14096 | 10650 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 10651 |  |
|        - | 10652 | `	SyString sPath;` |
|        - | 10653 | `	char *zDup;` |
|        - | 10654 | `#ifdef __WINNT__` |
|        - | 10655 | `	char *zCur;` |
|        - | 10656 | `#endif` |
|        - | 10657 | `	sxi32 rc;` |
|    14098 | 10658 | `	if( nLen < 0 ){` |
|     1178 | 10659 | `		nLen = SyStrlen(zPath);` |
|      588 | 10660 | `	}` |
|        - | 10661 | `	/* Duplicate the file path first */` |
|    14098 | 10662 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    14098 | 10663 | `	if( zDup == 0 ){` |
|      ! 0 | 10664 | `		return SXERR_MEM;` |
|        - | 10665 | `	}` |
|        - | 10666 | `#ifdef __WINNT__` |
|        - | 10667 | `	/* Normalize path on windows` |
|        - | 10668 | `	 * Example:` |
|        - | 10669 | `	 *    Path/To/File.php` |
|        - | 10670 | `	 * becomes` |
|        - | 10671 | `	 *   path\to\file.php` |
|        - | 10672 | `	 */` |
|        2 | 10673 | `	zCur = zDup;` |
|        2 | 10674 | `	while( zCur[0] != 0 ){` |
|        2 | 10675 | `		if( zCur[0] == '/' ){` |
|        2 | 10676 | `			zCur[0] = '\\';` |
|        2 | 10677 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 10678 | `			int c = SyToLower(zCur[0]);` |
|        1 | 10679 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 10680 | `		}` |
|        2 | 10681 | `		zCur++;` |
|        2 | 10682 | `	}` |
|        - | 10683 | `#endif` |
|        - | 10684 | `	/* Install the file path */` |
|    14098 | 10685 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    14098 | 10686 | `	if( !bMain ){` |
|    12921 | 10687 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 10688 | `			/* Already included */` |
|        7 | 10689 | `			*pNew = 0;` |
|        4 | 10690 | `		}else{` |
|        - | 10691 | `			/* Insert in the corresponding container */` |
|    12915 | 10692 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    12915 | 10693 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 10694 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 10695 | `				return rc;` |
|        - | 10696 | `			}` |
|    12915 | 10697 | `			*pNew = 1;` |
|        - | 10698 | `		}` |
|     6460 | 10699 | `	}` |
|    14098 | 10700 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    14098 | 10701 | `	return SXRET_OK;` |
|     7050 | 10702 |  |
|        - | 10703 | `/*` |
|        - | 10704 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 10705 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 10706 | ` * indicates failure.` |
|        - | 10707 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 10708 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 10709 | ` * operations.` |
|        - | 10710 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 10711 | ` * this function is a no-op.` |
|        - | 10712 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 10713 | ` * constructs for more information.` |
|        - | 10714 | ` */` |
|     6468 | 10715 | `static sxi32 VmExecIncludedFile(` |
|        - | 10716 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 10717 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 10718 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 10719 | `	 )` |
|        2 | 10720 |  |
|        - | 10721 | `	sxi32 rc;` |
|        - | 10722 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10723 | `	const ph7_io_stream *pStream;` |
|        - | 10724 | `	SyBlob sContents;` |
|        - | 10725 | `	void *pHandle;` |
|        - | 10726 | `	ph7_vm *pVm;` |
|        - | 10727 | `	int isNew;` |
|        - | 10728 | `	/* Initialize fields */` |
|     6470 | 10729 | `	pVm = pCtx->pVm;` |
|     6470 | 10730 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     6470 | 10731 | `	isNew = 0;` |
|        - | 10732 | `	/* Extract the associated stream */` |
|     6470 | 10733 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 10734 | `	/*` |
|        - | 10735 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 10736 | `	 * in a read-only mode.` |
|        - | 10737 | `	 */` |
|     6470 | 10738 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     6470 | 10739 | `	if( pHandle == 0 ){` |
|        3 | 10740 | `		return SXERR_IO;` |
|        - | 10741 | `	}` |
|     6467 | 10742 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     6467 | 10743 | `	if( IncludeOnce && !isNew ){` |
|        - | 10744 | `		/* Already included */` |
|        5 | 10745 | `		rc = SXERR_EXISTS;` |
|        3 | 10746 | `	}else{` |
|        - | 10747 | `		/* Read the whole file contents */` |
|     6463 | 10748 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     6463 | 10749 | `		if( rc == SXRET_OK ){` |
|        - | 10750 | `			SyString sScript;` |
|        - | 10751 | `			/* Compile and execute the script */` |
|     6463 | 10752 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     6463 | 10753 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     3231 | 10754 | `		}` |
|        - | 10755 | `	}` |
|        - | 10756 | `	/* Pop from the set of included file */` |
|     6467 | 10757 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 10758 | `	/* Close the handle */` |
|     6467 | 10759 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 10760 | `	/* Release the working buffer */` |
|     6467 | 10761 | `	SyBlobRelease(&sContents);` |
|        - | 10762 | `#else` |
|        - | 10763 | `	pCtx = 0; /* cc warning */` |
|        - | 10764 | `	pPath = 0;` |
|        - | 10765 | `	IncludeOnce = 0;` |
|        - | 10766 | `	rc = SXERR_IO;` |
|        - | 10767 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     6467 | 10768 | `	return rc;` |
|     3236 | 10769 |  |
|        - | 10770 | `/*` |
|        - | 10771 | ` * string get_include_path(void)` |
|        - | 10772 | ` *  Gets the current include_path configuration option.` |
|        - | 10773 | ` * Parameter` |
|        - | 10774 | ` *  None` |
|        - | 10775 | ` * Return` |
|        - | 10776 | ` *  Included paths as a string` |
|        - | 10777 | ` */` |
|        2 | 10778 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10779 |  |
|        3 | 10780 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10781 | `	SyString *aEntry;` |
|        - | 10782 | `	int dir_sep;` |
|        - | 10783 | `	sxu32 n;` |
|        - | 10784 | `#ifdef __WINNT__` |
|        1 | 10785 | `	dir_sep = ';';` |
|        - | 10786 | `#else` |
|        - | 10787 | `	/* Assume UNIX path separator */` |
|        2 | 10788 | `	dir_sep = ':';` |
|        - | 10789 | `#endif` |
|        1 | 10790 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10791 | `	SXUNUSED(apArg);` |
|        - | 10792 | `	/* Point to the list of import paths */` |
|        3 | 10793 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 10794 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 10795 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 10796 | `		if( n > 0 ){` |
|        - | 10797 | `			/* Append dir seprator */` |
|      ! 0 | 10798 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 10799 | `		}` |
|        - | 10800 | `		/* Append path */` |
|        3 | 10801 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 10802 | `	}` |
|        3 | 10803 | `	return PH7_OK;` |
|        1 | 10804 |  |
|        - | 10805 | `/*` |
|        - | 10806 | ` * string get_get_included_files(void)` |
|        - | 10807 | ` *  Gets the current include_path configuration option.` |
|        - | 10808 | ` * Parameter` |
|        - | 10809 | ` *  None` |
|        - | 10810 | ` * Return` |
|        - | 10811 | ` *  Included paths as a string` |
|        - | 10812 | ` */` |
|        2 | 10813 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10814 |  |
|        3 | 10815 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 10816 | `	ph7_value *pArray,*pWorker;` |
|        - | 10817 | `	SyString *pEntry;` |
|        - | 10818 | `	int c,d;` |
|        - | 10819 | `	/* Create an array and a working value */` |
|        3 | 10820 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 10821 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 10822 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 10823 | `		/* Out of memory,return null */` |
|      ! 0 | 10824 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10825 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10826 | `		SXUNUSED(apArg);` |
|      ! 0 | 10827 | `		return PH7_OK;` |
|        - | 10828 | `	}` |
|        3 | 10829 | `	c = d = '/';` |
|        - | 10830 | `#ifdef __WINNT__` |
|        1 | 10831 | `	d = '\\';` |
|        - | 10832 | `#endif` |
|        - | 10833 | `	/* Iterate throw entries */` |
|        3 | 10834 | `	SySetResetCursor(pFiles);` |
|     2669 | 10835 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 10836 | `		const char *zBase,*zEnd;` |
|        - | 10837 | `		int iLen;` |
|        - | 10838 | `		/* reset the string cursor */` |
|     2667 | 10839 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 10840 | `		/* Extract base name */` |
|     2667 | 10841 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 10842 | `		/* Ignore trailing '/' */` |
|     4000 | 10843 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 10844 | `			zEnd--;` |
|      ! 0 | 10845 | `		}` |
|     2667 | 10846 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|    74816 | 10847 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|    70817 | 10848 | `			zEnd--;` |
|        1 | 10849 | `		}` |
|     2667 | 10850 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     2667 | 10851 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 10852 | `		/* Copy entry name */` |
|     2667 | 10853 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 10854 | `		/* Perform the insertion */` |
|     2667 | 10855 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 10856 | `	}` |
|        - | 10857 | `	/* All done,return the created array */` |
|        3 | 10858 | `	ph7_result_value(pCtx,pArray);` |
|        - | 10859 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 10860 | `	 * by the engine as soon we return from this foreign` |
|        - | 10861 | `	 * function.` |
|        - | 10862 | `	 */` |
|        3 | 10863 | `	return PH7_OK;` |
|        2 | 10864 |  |
|        - | 10865 | `/*` |
|        - | 10866 | ` * include:` |
|        - | 10867 | ` * According to the PHP reference manual.` |
|        - | 10868 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 10869 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 10870 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 10871 | ` *  include() will finally check in the calling script's own directory` |
|        - | 10872 | ` *  and the current working directory before failing. The include()` |
|        - | 10873 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 10874 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 10875 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 10876 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 10877 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 10878 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 10879 | ` *  directory to find the requested file.` |
|        - | 10880 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 10881 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 10882 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 10883 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 10884 | ` */` |
|     6456 | 10885 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10886 |  |
|        - | 10887 | `	SyString sFile;` |
|        - | 10888 | `	sxi32 rc;` |
|     6458 | 10889 | `	if( nArg < 1 ){` |
|        - | 10890 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10891 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10892 | `		return SXRET_OK;` |
|        - | 10893 | `	}` |
|        - | 10894 | `	/* File to include */` |
|     6458 | 10895 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     6458 | 10896 | `	if( sFile.nByte < 1 ){` |
|        - | 10897 | `		/* Empty string,return NULL */` |
|      ! 0 | 10898 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10899 | `		return SXRET_OK;` |
|        - | 10900 | `	}` |
|        - | 10901 | `	/* Open,compile and execute the desired script */` |
|     6458 | 10902 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     6458 | 10903 | `	if( rc != SXRET_OK ){` |
|        - | 10904 | `		/* Emit a warning and return false */` |
|        3 | 10905 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 10906 | `		ph7_result_bool(pCtx,0);` |
|        1 | 10907 | `	}` |
|     6458 | 10908 | `	return SXRET_OK;` |
|     3230 | 10909 |  |
|        - | 10910 | `/*` |
|        - | 10911 | ` * include_once:` |
|        - | 10912 | ` *  According to the PHP reference manual.` |
|        - | 10913 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 10914 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 10915 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 10916 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 10917 | ` *   just once.` |
|        - | 10918 | ` */` |
|        4 | 10919 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10920 |  |
|        - | 10921 | `	SyString sFile;` |
|        - | 10922 | `	sxi32 rc;` |
|        5 | 10923 | `	if( nArg < 1 ){` |
|        - | 10924 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10925 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10926 | `		return SXRET_OK;` |
|        - | 10927 | `	}` |
|        - | 10928 | `	/* File to include */` |
|        5 | 10929 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 10930 | `	if( sFile.nByte < 1 ){` |
|        - | 10931 | `		/* Empty string,return NULL */` |
|      ! 0 | 10932 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10933 | `		return SXRET_OK;` |
|        - | 10934 | `	}` |
|        - | 10935 | `	/* Open,compile and execute the desired script */` |
|        5 | 10936 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 10937 | `	if( rc == SXERR_EXISTS ){` |
|        - | 10938 | `		/* File already included,return TRUE */` |
|        3 | 10939 | `		ph7_result_bool(pCtx,1);` |
|        3 | 10940 | `		return SXRET_OK;` |
|        - | 10941 | `	}` |
|        3 | 10942 | `	if( rc != SXRET_OK ){` |
|        - | 10943 | `		/* Emit a warning and return false */` |
|      ! 0 | 10944 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 10945 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10946 | ` 	}` |
|        3 | 10947 | `	return SXRET_OK;` |
|        3 | 10948 |  |
|        - | 10949 | `/*` |
|        - | 10950 | ` * require.` |
|        - | 10951 | ` *  According to the PHP reference manual.` |
|        - | 10952 | ` *   require() is identical to include() except upon failure it will` |
|        - | 10953 | ` *   also produce a fatal level error.` |
|        - | 10954 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 10955 | ` *   emits a warning  which allows the script to continue.` |
|        - | 10956 | ` */` |
|        4 | 10957 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10958 |  |
|        - | 10959 | `	SyString sFile;` |
|        - | 10960 | `	sxi32 rc;` |
|        5 | 10961 | `	if( nArg < 1 ){` |
|        - | 10962 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10963 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10964 | `		return SXRET_OK;` |
|        - | 10965 | `	}` |
|        - | 10966 | `	/* File to include */` |
|        5 | 10967 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 10968 | `	if( sFile.nByte < 1 ){` |
|        - | 10969 | `		/* Empty string,return NULL */` |
|      ! 0 | 10970 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10971 | `		return SXRET_OK;` |
|        - | 10972 | `	}` |
|        - | 10973 | `	/* Open,compile and execute the desired script */` |
|        5 | 10974 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        5 | 10975 | `	if( rc != SXRET_OK ){` |
|        - | 10976 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 10977 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 10978 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10979 | `		return PH7_ABORT;` |
|        - | 10980 | `	}` |
|        5 | 10981 | `	return SXRET_OK;` |
|        3 | 10982 |  |
|        - | 10983 | `/*` |
|        - | 10984 | ` * require_once:` |
|        - | 10985 | ` *  According to the PHP reference manual.` |
|        - | 10986 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 10987 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 10988 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 10989 | ` *   and how it differs from its non _once siblings.` |
|        - | 10990 | ` */` |
|        4 | 10991 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10992 |  |
|        - | 10993 | `	SyString sFile;` |
|        - | 10994 | `	sxi32 rc;` |
|        5 | 10995 | `	if( nArg < 1 ){` |
|        - | 10996 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10997 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10998 | `		return SXRET_OK;` |
|        - | 10999 | `	}` |
|        - | 11000 | `	/* File to include */` |
|        5 | 11001 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11002 | `	if( sFile.nByte < 1 ){` |
|        - | 11003 | `		/* Empty string,return NULL */` |
|      ! 0 | 11004 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11005 | `		return SXRET_OK;` |
|        - | 11006 | `	}` |
|        - | 11007 | `	/* Open,compile and execute the desired script */` |
|        5 | 11008 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 11009 | `	if( rc == SXERR_EXISTS ){` |
|        - | 11010 | `		/* File already included,return TRUE */` |
|        3 | 11011 | `		ph7_result_bool(pCtx,1);` |
|        3 | 11012 | `		return SXRET_OK;` |
|        - | 11013 | `	}` |
|        3 | 11014 | `	if( rc != SXRET_OK ){` |
|        - | 11015 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 11016 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11017 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11018 | `		return PH7_ABORT;` |
|        - | 11019 | `	}` |
|        3 | 11020 | `	return SXRET_OK;` |
|        3 | 11021 |  |
|        - | 11022 | `/*` |
|        - | 11023 | ` * Section:` |
|        - | 11024 | ` *  Command line arguments processing.` |
|        - | 11025 | ` * Status:` |
|        - | 11026 | ` *    Stable.` |
|        - | 11027 | ` */` |
|        - | 11028 | `/*` |
|        - | 11029 | ` * Check if a short option argument [i.e: -c] is available in the command` |
|        - | 11030 | ` * line string. Return a pointer to the start of the stream on success.` |
|        - | 11031 | ` * NULL otherwise.` |
|        - | 11032 | ` */` |
|        6 | 11033 | `static const char * VmFindShortOpt(int c,const char *zIn,const char *zEnd)` |
|        1 | 11034 |  |
|      199 | 11035 | `	while( zIn < zEnd ){` |
|      193 | 11036 | `		if( zIn[0] == '-' && &zIn[1] < zEnd && (int)zIn[1] == c ){` |
|        - | 11037 | `			/* Got one */` |
|      ! 0 | 11038 | `			return &zIn[1];` |
|        - | 11039 | `		}` |
|        - | 11040 | `		/* Advance the cursor */` |
|      193 | 11041 | `		zIn++;` |
|        1 | 11042 | `	}` |
|        - | 11043 | `	/* No such option */` |
|        7 | 11044 | `	return 0;` |
|        4 | 11045 |  |
|        - | 11046 | `/*` |
|        - | 11047 | ` * Check if a long option argument [i.e: --opt] is available in the command` |
|        - | 11048 | ` * line string. Return a pointer to the start of the stream on success.` |
|        - | 11049 | ` * NULL otherwise.` |
|        - | 11050 | ` */` |
|      ! 0 | 11051 | `static const char * VmFindLongOpt(const char *zLong,int nByte,const char *zIn,const char *zEnd)` |
|      ! 0 | 11052 |  |
|        - | 11053 | `	const char *zOpt;` |
|      ! 0 | 11054 | `	while( zIn < zEnd ){` |
|      ! 0 | 11055 | `		if( zIn[0] == '-' && &zIn[1] < zEnd && (int)zIn[1] == '-' ){` |
|      ! 0 | 11056 | `			zIn += 2;` |
|      ! 0 | 11057 | `			zOpt = zIn;` |
|      ! 0 | 11058 | `			while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|      ! 0 | 11059 | `				if( zIn[0] == '=' /* --opt=val */){` |
|      ! 0 | 11060 | `					break;` |
|        - | 11061 | `				}` |
|      ! 0 | 11062 | `				zIn++;` |
|      ! 0 | 11063 | `			}` |
|        - | 11064 | `			/* Test */` |
|      ! 0 | 11065 | `			if( (int)(zIn-zOpt) == nByte && SyMemcmp(zOpt,zLong,nByte) == 0 ){` |
|        - | 11066 | `				/* Got one,return it's value */` |
|      ! 0 | 11067 | `				return zIn;` |
|        - | 11068 | `			}` |
|        - | 11069 |  |
|      ! 0 | 11070 | `		}else{` |
|      ! 0 | 11071 | `			zIn++;` |
|        - | 11072 | `		}` |
|      ! 0 | 11073 | `	}` |
|        - | 11074 | `	/* No such option */` |
|      ! 0 | 11075 | `	return 0;` |
|      ! 0 | 11076 |  |
|        - | 11077 | `/*` |
|        - | 11078 | ` * Long option [i.e: --opt] arguments private data structure.` |
|        - | 11079 | ` */` |
|        - | 11080 | `struct getopt_long_opt` |
|        - | 11081 |  |
|        - | 11082 | `	const char *zArgIn,*zArgEnd; /* Command line arguments */` |
|        - | 11083 | `	ph7_value *pWorker;  /* Worker variable*/` |
|        - | 11084 | `	ph7_value *pArray;   /* getopt() return value */` |
|        - | 11085 | `	ph7_context *pCtx;   /* Call Context */` |
|        - | 11086 | `};` |
|        - | 11087 | `/* Forward declaration */` |
|        - | 11088 | `static int VmProcessLongOpt(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 11089 | `/*` |
|        - | 11090 | ` * Extract short or long argument option values.` |
|        - | 11091 | ` */` |
|      ! 0 | 11092 | `static void VmExtractOptArgValue(` |
|        - | 11093 | `	ph7_value *pArray,  /* getopt() return value */` |
|        - | 11094 | `	ph7_value *pWorker, /* Worker variable */` |
|        - | 11095 | `	const char *zArg,   /* Argument stream */` |
|        - | 11096 | `	const char *zArgEnd,/* End of the argument stream  */` |
|        - | 11097 | `	int need_val,       /* TRUE to fetch option argument */` |
|        - | 11098 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 11099 | `	const char *zName   /* Option name */)` |
|      ! 0 | 11100 |  |
|      ! 0 | 11101 | `	ph7_value_bool(pWorker,0);` |
|      ! 0 | 11102 | `	if( !need_val ){` |
|        - | 11103 | `		/*` |
|        - | 11104 | `		 * Option does not need arguments.` |
|        - | 11105 | `		 * Insert the option name and a boolean FALSE.` |
|        - | 11106 | `		 */` |
|      ! 0 | 11107 | `		ph7_array_add_strkey_elem(pArray,(const char *)zName,pWorker); /* Will make it's own copy */` |
|      ! 0 | 11108 | `	}else{` |
|        - | 11109 | `		const char *zCur;` |
|        - | 11110 | `		/* Extract option argument */` |
|      ! 0 | 11111 | `		zArg++;` |
|      ! 0 | 11112 | `		if( zArg < zArgEnd && zArg[0] == '=' ){` |
|      ! 0 | 11113 | `			zArg++;` |
|      ! 0 | 11114 | `		}` |
|      ! 0 | 11115 | `		while( zArg < zArgEnd && (unsigned char)zArg[0] < 0xc0 && SyisSpace(zArg[0]) ){` |
|      ! 0 | 11116 | `			zArg++;` |
|      ! 0 | 11117 | `		}` |
|      ! 0 | 11118 | `		if( zArg >= zArgEnd \|\| zArg[0] == '-' ){` |
|        - | 11119 | `			/*` |
|        - | 11120 | `			 * Argument not found.` |
|        - | 11121 | `			 * Insert the option name and a boolean FALSE.` |
|        - | 11122 | `			 */` |
|      ! 0 | 11123 | `			ph7_array_add_strkey_elem(pArray,(const char *)zName,pWorker); /* Will make it's own copy */` |
|      ! 0 | 11124 | `			return;` |
|        - | 11125 | `		}` |
|        - | 11126 | `		/* Delimit the value */` |
|      ! 0 | 11127 | `		zCur = zArg;` |
|      ! 0 | 11128 | `		if( zArg[0] == '\'' \|\| zArg[0] == '"' ){` |
|      ! 0 | 11129 | `			int d = zArg[0];` |
|        - | 11130 | `			/* Delimt the argument */` |
|      ! 0 | 11131 | `			zArg++;` |
|      ! 0 | 11132 | `			zCur = zArg;` |
|      ! 0 | 11133 | `			while( zArg < zArgEnd ){` |
|      ! 0 | 11134 | `				if( zArg[0] == d && zArg[-1] != '\\' ){` |
|        - | 11135 | `					/* Delimiter found,exit the loop  */` |
|      ! 0 | 11136 | `					break;` |
|        - | 11137 | `				}` |
|      ! 0 | 11138 | `				zArg++;` |
|      ! 0 | 11139 | `			}` |
|        - | 11140 | `			/* Save the value */` |
|      ! 0 | 11141 | `			ph7_value_string(pWorker,zCur,(int)(zArg-zCur));` |
|      ! 0 | 11142 | `			if( zArg < zArgEnd ){ zArg++; }` |
|      ! 0 | 11143 | `		}else{` |
|      ! 0 | 11144 | `			while( zArg < zArgEnd && !SyisSpace(zArg[0]) ){` |
|      ! 0 | 11145 | `				zArg++;` |
|      ! 0 | 11146 | `			}` |
|        - | 11147 | `			/* Save the value */` |
|      ! 0 | 11148 | `			ph7_value_string(pWorker,zCur,(int)(zArg-zCur));` |
|        - | 11149 | `		}` |
|        - | 11150 | `		/*` |
|        - | 11151 | `		 * Check if we are dealing with multiple values.` |
|        - | 11152 | `		 * If so,create an array to hold them,rather than a scalar variable.` |
|        - | 11153 | `		 */` |
|      ! 0 | 11154 | `		while( zArg < zArgEnd && (unsigned char)zArg[0] < 0xc0 && SyisSpace(zArg[0]) ){` |
|      ! 0 | 11155 | `			zArg++;` |
|      ! 0 | 11156 | `		}` |
|      ! 0 | 11157 | `		if( zArg < zArgEnd && zArg[0] != '-' ){` |
|        - | 11158 | `			ph7_value *pOptArg; /* Array of option arguments */` |
|      ! 0 | 11159 | `			pOptArg = ph7_context_new_array(pCtx);` |
|      ! 0 | 11160 | `			if( pOptArg == 0 ){` |
|      ! 0 | 11161 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 11162 | `			}else{` |
|        - | 11163 | `				/* Insert the first value */` |
|      ! 0 | 11164 | `				ph7_array_add_elem(pOptArg,0,pWorker); /* Will make it's own copy */` |
|      ! 0 | 11165 | `				for(;;){` |
|      ! 0 | 11166 | `					if( zArg >= zArgEnd \|\| zArg[0] == '-' ){` |
|        - | 11167 | `						/* No more value */` |
|      ! 0 | 11168 | `						break;` |
|        - | 11169 | `					}` |
|        - | 11170 | `					/* Delimit the value */` |
|      ! 0 | 11171 | `					zCur = zArg;` |
|      ! 0 | 11172 | `					if( zArg < zArgEnd && zArg[0] == '\\' ){` |
|      ! 0 | 11173 | `						zArg++;` |
|      ! 0 | 11174 | `						zCur = zArg;` |
|      ! 0 | 11175 | `					}` |
|      ! 0 | 11176 | `					while( zArg < zArgEnd && !SyisSpace(zArg[0]) ){` |
|      ! 0 | 11177 | `						zArg++;` |
|      ! 0 | 11178 | `					}` |
|        - | 11179 | `					/* Reset the string cursor */` |
|      ! 0 | 11180 | `					ph7_value_reset_string_cursor(pWorker);` |
|        - | 11181 | `					/* Save the value */` |
|      ! 0 | 11182 | `					ph7_value_string(pWorker,zCur,(int)(zArg-zCur));` |
|        - | 11183 | `					/* Insert */` |
|      ! 0 | 11184 | `					ph7_array_add_elem(pOptArg,0,pWorker); /* Will make it's own copy */` |
|        - | 11185 | `					/* Jump trailing white spaces */` |
|      ! 0 | 11186 | `					while( zArg < zArgEnd && (unsigned char)zArg[0] < 0xc0 && SyisSpace(zArg[0]) ){` |
|      ! 0 | 11187 | `						zArg++;` |
|      ! 0 | 11188 | `					}` |
|      ! 0 | 11189 | `				}` |
|        - | 11190 | `				/* Insert the option arg array */` |
|      ! 0 | 11191 | `				ph7_array_add_strkey_elem(pArray,(const char *)zName,pOptArg); /* Will make it's own copy */` |
|        - | 11192 | `				/* Safely release */` |
|      ! 0 | 11193 | `				ph7_context_release_value(pCtx,pOptArg);` |
|        - | 11194 | `			}` |
|      ! 0 | 11195 | `		}else{` |
|        - | 11196 | `			/* Single value */` |
|      ! 0 | 11197 | `			ph7_array_add_strkey_elem(pArray,(const char *)zName,pWorker); /* Will make it's own copy */` |
|        - | 11198 | `		}` |
|        - | 11199 | `	}` |
|      ! 0 | 11200 |  |
|        - | 11201 | `/*` |
|        - | 11202 | ` * array getopt(string $options[,array $longopts ])` |
|        - | 11203 | ` *   Gets options from the command line argument list.` |
|        - | 11204 | ` * Parameters` |
|        - | 11205 | ` *  $options` |
|        - | 11206 | ` *   Each character in this string will be used as option characters` |
|        - | 11207 | ` *   and matched against options passed to the script starting with` |
|        - | 11208 | ` *   a single hyphen (-). For example, an option string "x" recognizes` |
|        - | 11209 | ` *   an option -x. Only a-z, A-Z and 0-9 are allowed.` |
|        - | 11210 | ` *  $longopts` |
|        - | 11211 | ` *   An array of options. Each element in this array will be used as option` |
|        - | 11212 | ` *   strings and matched against options passed to the script starting with` |
|        - | 11213 | ` *   two hyphens (--). For example, an longopts element "opt" recognizes an` |
|        - | 11214 | ` *   option --opt.` |
|        - | 11215 | ` * Return` |
|        - | 11216 | ` *  This function will return an array of option / argument pairs or FALSE` |
|        - | 11217 | ` *  on failure.` |
|        - | 11218 | ` */` |
|        2 | 11219 | `static int vm_builtin_getopt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11220 |  |
|        - | 11221 | `	const char *zIn,*zEnd,*zArg,*zArgIn,*zArgEnd;` |
|        - | 11222 | `	struct getopt_long_opt sLong;` |
|        - | 11223 | `	ph7_value *pArray,*pWorker;` |
|        - | 11224 | `	SyBlob *pArg;` |
|        - | 11225 | `	int nByte;` |
|        3 | 11226 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 11227 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 11228 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Missing/Invalid option arguments");` |
|      ! 0 | 11229 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11230 | `		return PH7_OK;` |
|        - | 11231 | `	}` |
|        - | 11232 | `	/* Extract option arguments */` |
|        3 | 11233 | `	zIn  = ph7_value_to_string(apArg[0],&nByte);` |
|        3 | 11234 | `	zEnd = &zIn[nByte];` |
|        - | 11235 | `	/* Point to the string representation of the $argv[] array */` |
|        3 | 11236 | `	pArg = &pCtx->pVm->sArgv;` |
|        - | 11237 | `	/* Create a new empty array and a worker variable */` |
|        3 | 11238 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11239 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 11240 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|      ! 0 | 11241 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 11242 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11243 | `		return PH7_OK;` |
|        - | 11244 | `	}` |
|        3 | 11245 | `	if( SyBlobLength(pArg) < 1 ){` |
|        - | 11246 | `		/* Empty command line,return the empty array*/` |
|      ! 0 | 11247 | `		ph7_result_value(pCtx,pArray);` |
|        - | 11248 | `		/* Everything will be released automatically when we return` |
|        - | 11249 | `		 * from this function.` |
|        - | 11250 | `		 */` |
|      ! 0 | 11251 | `		return PH7_OK;` |
|        - | 11252 | `	}` |
|        3 | 11253 | `	zArgIn = (const char *)SyBlobData(pArg);` |
|        3 | 11254 | `	zArgEnd = &zArgIn[SyBlobLength(pArg)];` |
|        - | 11255 | `	/* Fill the long option structure */` |
|        3 | 11256 | `	sLong.pArray = pArray;` |
|        3 | 11257 | `	sLong.pWorker = pWorker;` |
|        3 | 11258 | `	sLong.zArgIn =  zArgIn;` |
|        3 | 11259 | `	sLong.zArgEnd = zArgEnd;` |
|        3 | 11260 | `	sLong.pCtx = pCtx;` |
|        - | 11261 | `	/* Start processing */` |
|        9 | 11262 | `	while( zIn < zEnd ){` |
|        7 | 11263 | `		int c = zIn[0];` |
|        7 | 11264 | `		int need_val = 0;` |
|        - | 11265 | `		/* Advance the stream cursor */` |
|        7 | 11266 | `		zIn++;` |
|        - | 11267 | `		/* Ignore non-alphanum characters */` |
|        7 | 11268 | `		if( !SyisAlphaNum(c) ){` |
|      ! 0 | 11269 | `			continue;` |
|        - | 11270 | `		}` |
|        7 | 11271 | `		if( zIn < zEnd && zIn[0] == ':' ){` |
|        5 | 11272 | `			zIn++;` |
|        5 | 11273 | `			need_val = 1;` |
|        5 | 11274 | `			if( zIn < zEnd && zIn[0] == ':' ){` |
|      ! 0 | 11275 | `				zIn++;` |
|      ! 0 | 11276 | `			}` |
|        2 | 11277 | `		}` |
|        - | 11278 | `		/* Find option */` |
|        7 | 11279 | `		zArg = VmFindShortOpt(c,zArgIn,zArgEnd);` |
|        7 | 11280 | `		if( zArg == 0 ){` |
|        - | 11281 | `			/* No such option */` |
|        7 | 11282 | `			continue;` |
|        - | 11283 | `		}` |
|        - | 11284 | `		/* Extract option argument value */` |
|      ! 0 | 11285 | `		VmExtractOptArgValue(pArray,pWorker,zArg,zArgEnd,need_val,pCtx,(const char *)&c);` |
|      ! 0 | 11286 | `	}` |
|        3 | 11287 | `	if( nArg > 1 && ph7_value_is_array(apArg[1]) && ph7_array_count(apArg[1]) > 0 ){` |
|        - | 11288 | `		/* Process long options */` |
|      ! 0 | 11289 | `		ph7_array_walk(apArg[1],VmProcessLongOpt,&sLong);` |
|      ! 0 | 11290 | `	}` |
|        - | 11291 | `	/* Return the option array */` |
|        3 | 11292 | `	ph7_result_value(pCtx,pArray);` |
|        - | 11293 | `	/*` |
|        - | 11294 | `	 * Don't worry about freeing memory, everything will be released` |
|        - | 11295 | `	 * automatically as soon we return from this foreign function.` |
|        - | 11296 | `	 */` |
|        3 | 11297 | `	return PH7_OK;` |
|        2 | 11298 |  |
|        - | 11299 | `/*` |
|        - | 11300 | ` * Array walker callback used for processing long options values.` |
|        - | 11301 | ` */` |
|      ! 0 | 11302 | `static int VmProcessLongOpt(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 11303 |  |
|      ! 0 | 11304 | `	struct getopt_long_opt *pOpt = (struct getopt_long_opt *)pUserData;` |
|        - | 11305 | `	const char *zArg,*zOpt,*zEnd;` |
|      ! 0 | 11306 | `	int need_value = 0;` |
|        - | 11307 | `	int nByte;` |
|        - | 11308 | `	/* Value must be of type string */` |
|      ! 0 | 11309 | `	if( !ph7_value_is_string(pValue) ){` |
|        - | 11310 | `		/* Simply ignore */` |
|      ! 0 | 11311 | `		return PH7_OK;` |
|        - | 11312 | `	}` |
|      ! 0 | 11313 | `	zOpt = ph7_value_to_string(pValue,&nByte);` |
|      ! 0 | 11314 | `	if( nByte < 1 ){` |
|        - | 11315 | `		/* Empty string,ignore */` |
|      ! 0 | 11316 | `		return PH7_OK;` |
|        - | 11317 | `	}` |
|      ! 0 | 11318 | `	zEnd = &zOpt[nByte - 1];` |
|      ! 0 | 11319 | `	if( zEnd[0] == ':' ){` |
|        - | 11320 | `		char *zTerm;` |
|        - | 11321 | `		/* Try to extract a value */` |
|      ! 0 | 11322 | `		need_value = 1;` |
|      ! 0 | 11323 | `		while( zEnd >= zOpt && zEnd[0] == ':' ){` |
|      ! 0 | 11324 | `			zEnd--;` |
|      ! 0 | 11325 | `		}` |
|      ! 0 | 11326 | `		if( zOpt >= zEnd ){` |
|        - | 11327 | `			/* Empty string,ignore */` |
|      ! 0 | 11328 | `			SXUNUSED(pKey);` |
|      ! 0 | 11329 | `			return PH7_OK;` |
|        - | 11330 | `		}` |
|      ! 0 | 11331 | `		zEnd++;` |
|      ! 0 | 11332 | `		zTerm = (char *)zEnd;` |
|      ! 0 | 11333 | `		zTerm[0] = 0;` |
|      ! 0 | 11334 | `	}else{` |
|      ! 0 | 11335 | `		zEnd = &zOpt[nByte];` |
|        - | 11336 | `	}` |
|        - | 11337 | `	/* Find the option */` |
|      ! 0 | 11338 | `	zArg = VmFindLongOpt(zOpt,(int)(zEnd-zOpt),pOpt->zArgIn,pOpt->zArgEnd);` |
|      ! 0 | 11339 | `	if( zArg == 0 ){` |
|        - | 11340 | `		/* No such option,return immediately */` |
|      ! 0 | 11341 | `		return PH7_OK;` |
|        - | 11342 | `	}` |
|        - | 11343 | `	/* Try to extract a value */` |
|      ! 0 | 11344 | `	VmExtractOptArgValue(pOpt->pArray,pOpt->pWorker,zArg,pOpt->zArgEnd,need_value,pOpt->pCtx,zOpt);` |
|      ! 0 | 11345 | `	return PH7_OK;` |
|      ! 0 | 11346 |  |
|        - | 11347 | `/*` |
|        - | 11348 | ` * Section:` |
|        - | 11349 | ` *  JSON encoding/decoding routines.` |
|        - | 11350 | ` * Status:` |
|        - | 11351 | ` *    Devel.` |
|        - | 11352 | ` */` |
|        - | 11353 | `/* Forward reference */` |
|        - | 11354 | `static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 11355 | `static int VmJsonObjectEncode(const char *zAttr,ph7_value *pValue,void *pUserData);` |
|        - | 11356 | `/*` |
|        - | 11357 | ` * JSON encoder state is stored in an instance` |
|        - | 11358 | ` * of the following structure.` |
|        - | 11359 | ` */` |
|        - | 11360 | `typedef struct json_private_data json_private_data;` |
|        - | 11361 | `struct json_private_data` |
|        - | 11362 |  |
|        - | 11363 | `	ph7_context *pCtx; /* Call context */` |
|        - | 11364 | `	int isFirst;       /* True if first encoded entry */` |
|        - | 11365 | `	int iFlags;        /* JSON encoding flags */` |
|        - | 11366 | `	int nRecCount;     /* Recursion count */` |
|        - | 11367 | `};` |
|        - | 11368 | `/*` |
|        - | 11369 | ` * Returns the JSON representation of a value.In other word perform a JSON encoding operation.` |
|        - | 11370 | ` * According to wikipedia` |
|        - | 11371 | ` * JSON's basic types are:` |
|        - | 11372 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|        - | 11373 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|        - | 11374 | ` *   Boolean (true or false)` |
|        - | 11375 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|        - | 11376 | ` *    do not need to be of the same type)` |
|        - | 11377 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|        - | 11378 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|        - | 11379 | ` *     be distinct from each other)` |
|        - | 11380 | ` *   null (empty)` |
|        - | 11381 | ` * Non-significant white space may be added freely around the "structural characters"` |
|        - | 11382 | ` * (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|        - | 11383 | ` */` |
|        8 | 11384 | `static sxi32 VmJsonEncode(` |
|        - | 11385 | `	ph7_value *pIn,          /* Encode this value */` |
|        - | 11386 | `	json_private_data *pData /* Context data */` |
|        1 | 11387 | `	){` |
|        9 | 11388 | `		ph7_context *pCtx = pData->pCtx;` |
|        9 | 11389 | `		int iFlags = pData->iFlags;` |
|        - | 11390 | `		int nByte;` |
|        9 | 11391 | `		if( ph7_value_is_null(pIn) \|\| ph7_value_is_resource(pIn)){` |
|        - | 11392 | `			/* null */` |
|      ! 0 | 11393 | `			ph7_result_string(pCtx,"null",(int)sizeof("null")-1);` |
|        9 | 11394 | `		}else if( ph7_value_is_bool(pIn) ){` |
|      ! 0 | 11395 | `			int iBool = ph7_value_to_bool(pIn);` |
|        - | 11396 | `			int iLen;` |
|        - | 11397 | `			/* true/false */` |
|      ! 0 | 11398 | `			iLen = iBool ? (int)sizeof("true") : (int)sizeof("false");` |
|      ! 0 | 11399 | `			ph7_result_string(pCtx,iBool ? "true" : "false",iLen-1);` |
|       12 | 11400 | `		}else if(  ph7_value_is_numeric(pIn) && !ph7_value_is_string(pIn) ){` |
|        - | 11401 | `			const char *zNum;` |
|        - | 11402 | `			/* Get a string representation of the number */` |
|        7 | 11403 | `			zNum = ph7_value_to_string(pIn,&nByte);` |
|        7 | 11404 | `			ph7_result_string(pCtx,zNum,nByte);` |
|        6 | 11405 | `		}else if( ph7_value_is_string(pIn) ){` |
|      ! 0 | 11406 | `			if( (iFlags & JSON_NUMERIC_CHECK) &&  ph7_value_is_numeric(pIn) ){` |
|        - | 11407 | `				const char *zNum;` |
|        - | 11408 | `				/* Encodes numeric strings as numbers. */` |
|      ! 0 | 11409 | `				PH7_MemObjToReal(pIn); /* Force a numeric cast */` |
|        - | 11410 | `				/* Get a string representation of the number */` |
|      ! 0 | 11411 | `				zNum = ph7_value_to_string(pIn,&nByte);` |
|      ! 0 | 11412 | `				ph7_result_string(pCtx,zNum,nByte);` |
|      ! 0 | 11413 | `			}else{` |
|        - | 11414 | `				const char *zIn,*zEnd;` |
|        - | 11415 | `				int c;` |
|        - | 11416 | `				/* Encode the string */` |
|      ! 0 | 11417 | `				zIn = ph7_value_to_string(pIn,&nByte);` |
|      ! 0 | 11418 | `				zEnd = &zIn[nByte];` |
|        - | 11419 | `				/* Append the double quote */` |
|      ! 0 | 11420 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      ! 0 | 11421 | `				for(;;){` |
|      ! 0 | 11422 | `					if( zIn >= zEnd ){` |
|        - | 11423 | `						/* No more input to process */` |
|      ! 0 | 11424 | `						break;` |
|        - | 11425 | `					}` |
|      ! 0 | 11426 | `					c = zIn[0];` |
|        - | 11427 | `					/* Advance the stream cursor */` |
|      ! 0 | 11428 | `					zIn++;` |
|      ! 0 | 11429 | `					if( (c == '<' \|\| c == '>') && (iFlags & JSON_HEX_TAG) ){` |
|        - | 11430 | `						/* All < and > are converted to \u003C and \u003E */` |
|      ! 0 | 11431 | `						if( c == '<' ){` |
|      ! 0 | 11432 | `							ph7_result_string(pCtx,"\\u003C",(int)sizeof("\\u003C")-1);` |
|      ! 0 | 11433 | `						}else{` |
|      ! 0 | 11434 | `							ph7_result_string(pCtx,"\\u003E",(int)sizeof("\\u003E")-1);` |
|        - | 11435 | `						}` |
|      ! 0 | 11436 | `						continue;` |
|      ! 0 | 11437 | `					}else if( c == '&' && (iFlags & JSON_HEX_AMP) ){` |
|        - | 11438 | `						/* All &s are converted to \u0026.  */` |
|      ! 0 | 11439 | `						ph7_result_string(pCtx,"\\u0026",(int)sizeof("\\u0026")-1);` |
|      ! 0 | 11440 | `						continue;` |
|      ! 0 | 11441 | `					}else if( c == '\'' && (iFlags & JSON_HEX_APOS) ){` |
|        - | 11442 | `						/* All ' are converted to \u0027.   */` |
|      ! 0 | 11443 | `						ph7_result_string(pCtx,"\\u0027",(int)sizeof("\\u0027")-1);` |
|      ! 0 | 11444 | `						continue;` |
|      ! 0 | 11445 | `					}else if( c == '"' && (iFlags & JSON_HEX_QUOT) ){` |
|        - | 11446 | `						/* All " are converted to \u0022. */` |
|      ! 0 | 11447 | `						ph7_result_string(pCtx,"\\u0022",(int)sizeof("\\u0022")-1);` |
|      ! 0 | 11448 | `						continue;` |
|        - | 11449 | `					}` |
|      ! 0 | 11450 | `					if( c == '"' \|\| (c == '\\' && ((iFlags & JSON_UNESCAPED_SLASHES)==0)) ){` |
|        - | 11451 | `						/* Unescape the character */` |
|      ! 0 | 11452 | `						ph7_result_string(pCtx,"\\",(int)sizeof(char));` |
|      ! 0 | 11453 | `					}` |
|        - | 11454 | `					/* Append character verbatim */` |
|      ! 0 | 11455 | `					ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      ! 0 | 11456 | `				}` |
|        - | 11457 | `				/* Append the double quote */` |
|      ! 0 | 11458 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      ! 0 | 11459 | `			}` |
|        3 | 11460 | `		}else if( ph7_value_is_array(pIn) ){` |
|        3 | 11461 | `			int c = '[',d = ']';` |
|        - | 11462 | `			/* Encode the array */` |
|        3 | 11463 | `			pData->isFirst = 1;` |
|        3 | 11464 | `			if( iFlags & JSON_FORCE_OBJECT ){` |
|        - | 11465 | `				/* Outputs an object rather than an array */` |
|      ! 0 | 11466 | `				c = '{';` |
|      ! 0 | 11467 | `				d = '}';` |
|      ! 0 | 11468 | `			}` |
|        - | 11469 | `			/* Append the square bracket or curly braces */` |
|        3 | 11470 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|        - | 11471 | `			/* Iterate throw array entries */` |
|        3 | 11472 | `			ph7_array_walk(pIn,VmJsonArrayEncode,pData);` |
|        - | 11473 | `			/* Append the closing square bracket or curly braces */` |
|        3 | 11474 | `			ph7_result_string(pCtx,(const char *)&d,(int)sizeof(char));` |
|        1 | 11475 | `		}else if( ph7_value_is_object(pIn) ){` |
|        - | 11476 | `			/* Encode the class instance */` |
|      ! 0 | 11477 | `			pData->isFirst = 1;` |
|        - | 11478 | `			/* Append the curly braces */` |
|      ! 0 | 11479 | `			ph7_result_string(pCtx,"{",(int)sizeof(char));` |
|        - | 11480 | `			/* Iterate throw class attribute */` |
|      ! 0 | 11481 | `			ph7_object_walk(pIn,VmJsonObjectEncode,pData);` |
|        - | 11482 | `			/* Append the closing curly braces  */` |
|      ! 0 | 11483 | `			ph7_result_string(pCtx,"}",(int)sizeof(char));` |
|      ! 0 | 11484 | `		}else{` |
|        - | 11485 | `			/* Can't happen */` |
|      ! 0 | 11486 | `			ph7_result_string(pCtx,"null",(int)sizeof("null")-1);` |
|        - | 11487 | `		}` |
|        - | 11488 | `		/* All done */` |
|        9 | 11489 | `		return PH7_OK;` |
|        1 | 11490 |  |
|        - | 11491 | `/*` |
|        - | 11492 | ` * The following walker callback is invoked each time we need` |
|        - | 11493 | ` * to encode an array to JSON.` |
|        - | 11494 | ` */` |
|        6 | 11495 | `static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 11496 |  |
|        7 | 11497 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|        7 | 11498 | `	if( pJson->nRecCount > 31 ){` |
|        - | 11499 | `		/* Recursion limit reached,return immediately */` |
|      ! 0 | 11500 | `		return PH7_OK;` |
|        - | 11501 | `	}` |
|        7 | 11502 | `	if( !pJson->isFirst ){` |
|        - | 11503 | `		/* Append the colon first */` |
|        5 | 11504 | `		ph7_result_string(pJson->pCtx,",",(int)sizeof(char));` |
|        2 | 11505 | `	}` |
|        7 | 11506 | `	if( pJson->iFlags & JSON_FORCE_OBJECT ){` |
|        - | 11507 | `		/* Outputs an object rather than an array */` |
|        - | 11508 | `		const char *zKey;` |
|        - | 11509 | `		int nByte;` |
|        - | 11510 | `		/* Extract a string representation of the key */` |
|      ! 0 | 11511 | `		zKey = ph7_value_to_string(pKey,&nByte);` |
|        - | 11512 | `		/* Append the key and the double colon */` |
|      ! 0 | 11513 | `		ph7_result_string_format(pJson->pCtx,"\"%.*s\":",nByte,zKey);` |
|      ! 0 | 11514 | `	}` |
|        - | 11515 | `	/* Encode the value */` |
|        7 | 11516 | `	pJson->nRecCount++;` |
|        7 | 11517 | `	VmJsonEncode(pValue,pJson);` |
|        7 | 11518 | `	pJson->nRecCount--;` |
|        7 | 11519 | `	pJson->isFirst = 0;` |
|        7 | 11520 | `	return PH7_OK;` |
|        4 | 11521 |  |
|        - | 11522 | `/*` |
|        - | 11523 | ` * The following walker callback is invoked each time we need to encode` |
|        - | 11524 | ` * a class instance [i.e: Object in the PHP jargon] to JSON.` |
|        - | 11525 | ` */` |
|      ! 0 | 11526 | `static int VmJsonObjectEncode(const char *zAttr,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 11527 |  |
|      ! 0 | 11528 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|      ! 0 | 11529 | `	if( pJson->nRecCount > 31 ){` |
|        - | 11530 | `		/* Recursion limit reached,return immediately */` |
|      ! 0 | 11531 | `		return PH7_OK;` |
|        - | 11532 | `	}` |
|      ! 0 | 11533 | `	if( !pJson->isFirst ){` |
|        - | 11534 | `		/* Append the colon first */` |
|      ! 0 | 11535 | `		ph7_result_string(pJson->pCtx,",",(int)sizeof(char));` |
|      ! 0 | 11536 | `	}` |
|        - | 11537 | `	/* Append the attribute name and the double colon first */` |
|      ! 0 | 11538 | `	ph7_result_string_format(pJson->pCtx,"\"%s\":",zAttr);` |
|        - | 11539 | `	/* Encode the value */` |
|      ! 0 | 11540 | `	pJson->nRecCount++;` |
|      ! 0 | 11541 | `	VmJsonEncode(pValue,pJson);` |
|      ! 0 | 11542 | `	pJson->nRecCount--;` |
|      ! 0 | 11543 | `	pJson->isFirst = 0;` |
|      ! 0 | 11544 | `	return PH7_OK;` |
|      ! 0 | 11545 |  |
|        - | 11546 | `/*` |
|        - | 11547 | ` * string json_encode(mixed $value [, int $options = 0 ])` |
|        - | 11548 | ` *  Returns a string containing the JSON representation of value.` |
|        - | 11549 | ` * Parameters` |
|        - | 11550 | ` *  $value` |
|        - | 11551 | ` *  The value being encoded. Can be any type except a resource.` |
|        - | 11552 | ` * $options` |
|        - | 11553 | ` *  Bitmask consisting of:` |
|        - | 11554 | ` *  JSON_HEX_TAG   All < and > are converted to \u003C and \u003E.` |
|        - | 11555 | ` *  JSON_HEX_AMP   All &s are converted to \u0026.` |
|        - | 11556 | ` *  JSON_HEX_APOS  All ' are converted to \u0027.` |
|        - | 11557 | ` *  JSON_HEX_QUOT  All " are converted to \u0022.` |
|        - | 11558 | ` *  JSON_FORCE_OBJECT  Outputs an object rather than an array.` |
|        - | 11559 | ` *  JSON_NUMERIC_CHECK Encodes numeric strings as numbers.` |
|        - | 11560 | ` *  JSON_BIGINT_AS_STRING   Not used` |
|        - | 11561 | ` *  JSON_PRETTY_PRINT       Use whitespace in returned data to format it.` |
|        - | 11562 | ` *  JSON_UNESCAPED_SLASHES  Don't escape '/'` |
|        - | 11563 | ` *  JSON_UNESCAPED_UNICODE  Not used.` |
|        - | 11564 | ` * Return` |
|        - | 11565 | ` *  Returns a JSON encoded string on success. FALSE otherwise` |
|        - | 11566 | ` */` |
|        2 | 11567 | `static int vm_builtin_json_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11568 |  |
|        - | 11569 | `	json_private_data sJson;` |
|        3 | 11570 | `	if( nArg < 1 ){` |
|        - | 11571 | `		/* Missing arguments,return FALSE */` |
|      ! 0 | 11572 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11573 | `		return PH7_OK;` |
|        - | 11574 | `	}` |
|        - | 11575 | `	/* Prepare the JSON data */` |
|        3 | 11576 | `	sJson.nRecCount = 0;` |
|        3 | 11577 | `	sJson.pCtx = pCtx;` |
|        3 | 11578 | `	sJson.isFirst = 1;` |
|        3 | 11579 | `	sJson.iFlags = 0;` |
|        3 | 11580 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|        - | 11581 | `		/* Extract option flags */` |
|      ! 0 | 11582 | `		sJson.iFlags = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 11583 | `	}` |
|        - | 11584 | `	/* Perform the encoding operation */` |
|        3 | 11585 | `	VmJsonEncode(apArg[0],&sJson);` |
|        - | 11586 | `	/* All done */` |
|        3 | 11587 | `	return PH7_OK;` |
|        2 | 11588 |  |
|        - | 11589 | `/*` |
|        - | 11590 | ` * int json_last_error(void)` |
|        - | 11591 | ` *  Returns the last error (if any) occurred during the last JSON encoding/decoding.` |
|        - | 11592 | ` * Parameters` |
|        - | 11593 | ` *  None` |
|        - | 11594 | ` * Return` |
|        - | 11595 | ` *  Returns an integer, the value can be one of the following constants:` |
|        - | 11596 | ` *  JSON_ERROR_NONE            No error has occurred.` |
|        - | 11597 | ` *  JSON_ERROR_DEPTH           The maximum stack depth has been exceeded.` |
|        - | 11598 | ` *  JSON_ERROR_STATE_MISMATCH  Invalid or malformed JSON.` |
|        - | 11599 | ` *  JSON_ERROR_CTRL_CHAR  	   Control character error, possibly incorrectly encoded.` |
|        - | 11600 | ` *  JSON_ERROR_SYNTAX          Syntax error.` |
|        - | 11601 | ` *  JSON_ERROR_UTF8_CHECK      Malformed UTF-8 characters.` |
|        - | 11602 | ` */` |
|        8 | 11603 | `static int vm_builtin_json_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11604 |  |
|       10 | 11605 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11606 | `	/* Return the error code */` |
|       10 | 11607 | `	ph7_result_int(pCtx,pVm->json_rc);` |
|        4 | 11608 | `	SXUNUSED(nArg); /* cc warning */` |
|        4 | 11609 | `	SXUNUSED(apArg);` |
|       10 | 11610 | `	return PH7_OK;` |
|        2 | 11611 |  |
|        - | 11612 | `/* Possible tokens from the JSON tokenization process */` |
|        - | 11613 | `#define JSON_TK_TRUE    0x001 /* Boolean true */` |
|        - | 11614 | `#define JSON_TK_FALSE   0x002 /* Boolean false */` |
|        - | 11615 | `#define JSON_TK_STR     0x004 /* String enclosed in double quotes */` |
|        - | 11616 | `#define JSON_TK_NULL    0x008 /* null */` |
|        - | 11617 | `#define JSON_TK_NUM     0x010 /* Numeric */` |
|        - | 11618 | `#define JSON_TK_OCB     0x020 /* Open curly braces '{' */` |
|        - | 11619 | `#define JSON_TK_CCB     0x040 /* Closing curly braces '}' */` |
|        - | 11620 | `#define JSON_TK_OSB     0x080 /* Open square bracke '[' */` |
|        - | 11621 | `#define JSON_TK_CSB     0x100 /* Closing square bracket ']' */` |
|        - | 11622 | `#define JSON_TK_COLON   0x200 /* Single colon ':' */` |
|        - | 11623 | `#define JSON_TK_COMMA   0x400 /* Single comma ',' */` |
|        - | 11624 | `#define JSON_TK_INVALID 0x800 /* Unexpected token */` |
|        - | 11625 | `/*` |
|        - | 11626 | ` * Tokenize an entire JSON input.` |
|        - | 11627 | ` * Get a single low-level token from the input file.` |
|        - | 11628 | ` * Update the stream pointer so that it points to the first` |
|        - | 11629 | ` * character beyond the extracted token.` |
|        - | 11630 | ` */` |
|       60 | 11631 | `static sxi32 VmJsonTokenize(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|        2 | 11632 |  |
|       62 | 11633 | `	int *pJsonErr = (int *)pUserData;` |
|        - | 11634 | `	SyString *pStr;` |
|        - | 11635 | `	int c;` |
|        - | 11636 | `	/* Ignore leading white spaces */` |
|       66 | 11637 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|        - | 11638 | `		/* Advance the stream cursor */` |
|        6 | 11639 | `		if( pStream->zText[0] == '\n' ){` |
|        - | 11640 | `			/* Update line counter */` |
|      ! 0 | 11641 | `			pStream->nLine++;` |
|      ! 0 | 11642 | `		}` |
|        6 | 11643 | `		pStream->zText++;` |
|        2 | 11644 | `	}` |
|       62 | 11645 | `	if( pStream->zText >= pStream->zEnd ){` |
|        - | 11646 | `		/* End of input reached */` |
|      ! 0 | 11647 | `		SXUNUSED(pCtxData); /* cc warning */` |
|      ! 0 | 11648 | `		return SXERR_EOF;` |
|        - | 11649 | `	}` |
|        - | 11650 | `	/* Record token starting position and line */` |
|       62 | 11651 | `	pToken->nLine = pStream->nLine;` |
|       62 | 11652 | `	pToken->pUserData = 0;` |
|       62 | 11653 | `	pStr = &pToken->sData;` |
|       62 | 11654 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
|       77 | 11655 | `	if( pStream->zText[0] == '{' \|\| pStream->zText[0] == '[' \|\| pStream->zText[0] == '}' \|\| pStream->zText[0] == ']'` |
|       44 | 11656 | `		\|\| pStream->zText[0] == ':' \|\| pStream->zText[0] == ',' ){` |
|        - | 11657 | `			/* Single character */` |
|       36 | 11658 | `			c = pStream->zText[0];` |
|        - | 11659 | `			/* Set token type */` |
|       36 | 11660 | `			switch(c){` |
|        5 | 11661 | `			case '[': pToken->nType = JSON_TK_OSB;   break;` |
|       10 | 11662 | `			case '{': pToken->nType = JSON_TK_OCB;   break;` |
|        6 | 11663 | `			case '}': pToken->nType = JSON_TK_CCB;   break;` |
|        5 | 11664 | `			case ']': pToken->nType = JSON_TK_CSB;   break;` |
|        8 | 11665 | `			case ':': pToken->nType = JSON_TK_COLON; break;` |
|        9 | 11666 | `			case ',': pToken->nType = JSON_TK_COMMA; break;` |
|      ! 0 | 11667 | `			default:` |
|      ! 0 | 11668 | `				break;` |
|        - | 11669 | `			}` |
|        - | 11670 | `			/* Advance the stream cursor */` |
|       36 | 11671 | `			pStream->zText++;` |
|       45 | 11672 | `	}else if( pStream->zText[0] == '"') {` |
|        - | 11673 | `		/* JSON string */` |
|       10 | 11674 | `		pStream->zText++;` |
|       10 | 11675 | `		pStr->zString++;` |
|        - | 11676 | `		/* Delimit the string */` |
|       32 | 11677 | `		while( pStream->zText < pStream->zEnd ){` |
|       32 | 11678 | `			if( pStream->zText[0] == '"' && pStream->zText[-1] != '\\' ){` |
|       10 | 11679 | `				break;` |
|        - | 11680 | `			}` |
|       24 | 11681 | `			if( pStream->zText[0] == '\n' ){` |
|        - | 11682 | `				/* Update line counter */` |
|      ! 0 | 11683 | `				pStream->nLine++;` |
|      ! 0 | 11684 | `			}` |
|       24 | 11685 | `			pStream->zText++;` |
|        2 | 11686 | `		}` |
|       10 | 11687 | `		if( pStream->zText >= pStream->zEnd ){` |
|        - | 11688 | `			/* Missing closing '"' */` |
|      ! 0 | 11689 | `			pToken->nType = JSON_TK_INVALID;` |
|      ! 0 | 11690 | `			*pJsonErr = JSON_ERROR_SYNTAX;` |
|      ! 0 | 11691 | `		}else{` |
|       10 | 11692 | `			pToken->nType = JSON_TK_STR;` |
|       10 | 11693 | `			pStream->zText++; /* Jump the closing double quotes */` |
|        2 | 11694 | `		}` |
|       24 | 11695 | `	}else if( pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|        - | 11696 | `		/* Number */` |
|       13 | 11697 | `		pStream->zText++;` |
|       13 | 11698 | `		pToken->nType = JSON_TK_NUM;` |
|       13 | 11699 | `		while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      ! 0 | 11700 | `			pStream->zText++;` |
|      ! 0 | 11701 | `		}` |
|       13 | 11702 | `		if( pStream->zText < pStream->zEnd ){` |
|       13 | 11703 | `			c = pStream->zText[0];` |
|       13 | 11704 | `			if( c == '.' ){` |
|        - | 11705 | `					/* Real number */` |
|      ! 0 | 11706 | `					pStream->zText++;` |
|      ! 0 | 11707 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      ! 0 | 11708 | `						pStream->zText++;` |
|      ! 0 | 11709 | `					}` |
|      ! 0 | 11710 | `					if( pStream->zText < pStream->zEnd ){` |
|      ! 0 | 11711 | `						c = pStream->zText[0];` |
|      ! 0 | 11712 | `						if( c=='e' \|\| c=='E' ){` |
|      ! 0 | 11713 | `							pStream->zText++;` |
|      ! 0 | 11714 | `							if( pStream->zText < pStream->zEnd ){` |
|      ! 0 | 11715 | `								c = pStream->zText[0];` |
|      ! 0 | 11716 | `								if( c =='+' \|\| c=='-' ){` |
|      ! 0 | 11717 | `									pStream->zText++;` |
|      ! 0 | 11718 | `								}` |
|      ! 0 | 11719 | `								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      ! 0 | 11720 | `									pStream->zText++;` |
|      ! 0 | 11721 | `								}` |
|      ! 0 | 11722 | `							}` |
|      ! 0 | 11723 | `						}` |
|      ! 0 | 11724 | `					}` |
|       13 | 11725 | `				}else if( c=='e' \|\| c=='E' ){` |
|        - | 11726 | `					/* Real number */` |
|      ! 0 | 11727 | `					pStream->zText++;` |
|      ! 0 | 11728 | `					if( pStream->zText < pStream->zEnd ){` |
|      ! 0 | 11729 | `						c = pStream->zText[0];` |
|      ! 0 | 11730 | `						if( c =='+' \|\| c=='-' ){` |
|      ! 0 | 11731 | `							pStream->zText++;` |
|      ! 0 | 11732 | `						}` |
|      ! 0 | 11733 | `						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      ! 0 | 11734 | `							pStream->zText++;` |
|      ! 0 | 11735 | `						}` |
|      ! 0 | 11736 | `					}` |
|      ! 0 | 11737 | `				}` |
|        7 | 11738 | `			}` |
|       17 | 11739 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("true") -1 &&` |
|        6 | 11740 | `		SyStrnicmp((const char *)pStream->zText,"true",sizeof("true")-1) == 0 ){` |
|        - | 11741 | `			/* boolean true */` |
|      ! 0 | 11742 | `			pToken->nType = JSON_TK_TRUE;` |
|        - | 11743 | `			/* Advance the stream cursor */` |
|      ! 0 | 11744 | `			pStream->zText += sizeof("true")-1;` |
|       11 | 11745 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("false") -1 &&` |
|        6 | 11746 | `		SyStrnicmp((const char *)pStream->zText,"false",sizeof("false")-1) == 0 ){` |
|        - | 11747 | `			/* boolean false */` |
|      ! 0 | 11748 | `			pToken->nType = JSON_TK_FALSE;` |
|        - | 11749 | `			/* Advance the stream cursor */` |
|      ! 0 | 11750 | `			pStream->zText += sizeof("false")-1;` |
|       11 | 11751 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("null") -1 &&` |
|        6 | 11752 | `		SyStrnicmp((const char *)pStream->zText,"null",sizeof("null")-1) == 0 ){` |
|        - | 11753 | `			/* NULL */` |
|      ! 0 | 11754 | `			pToken->nType = JSON_TK_NULL;` |
|        - | 11755 | `			/* Advance the stream cursor */` |
|      ! 0 | 11756 | `			pStream->zText += sizeof("null")-1;` |
|      ! 0 | 11757 | `	}else{` |
|        - | 11758 | `		/* Unexpected token */` |
|        8 | 11759 | `		pToken->nType = JSON_TK_INVALID;` |
|        - | 11760 | `		/* Advance the stream cursor */` |
|        8 | 11761 | `		pStream->zText++;` |
|        8 | 11762 | `		*pJsonErr = JSON_ERROR_SYNTAX;` |
|        - | 11763 | `		/* Abort processing immediatley */` |
|        8 | 11764 | `		return SXERR_ABORT;` |
|        - | 11765 | `	}` |
|        - | 11766 | `	/* record token length */` |
|       56 | 11767 | `	pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|       56 | 11768 | `	if( pToken->nType == JSON_TK_STR ){` |
|       10 | 11769 | `		pStr->nByte--;` |
|        4 | 11770 | `	}` |
|        - | 11771 | `	/* Return to the lexer */` |
|       56 | 11772 | `	return SXRET_OK;` |
|       32 | 11773 |  |
|        - | 11774 | `/*` |
|        - | 11775 | ` * JSON decoded input consumer callback signature.` |
|        - | 11776 | ` */` |
|        - | 11777 | `typedef int (*ProcJsonConsumer)(ph7_context *,ph7_value *,ph7_value *,void *);` |
|        - | 11778 | `/*` |
|        - | 11779 | ` * JSON decoder state is kept in the following structure.` |
|        - | 11780 | ` */` |
|        - | 11781 | `typedef struct json_decoder json_decoder;` |
|        - | 11782 | `struct json_decoder` |
|        - | 11783 |  |
|        - | 11784 | `	ph7_context *pCtx; /* Call context */` |
|        - | 11785 | `	ProcJsonConsumer xConsumer; /* Consumer callback */` |
|        - | 11786 | `	void *pUserData;   /* Last argument to xConsumer() */` |
|        - | 11787 | `	int iFlags;        /* Configuration flags */` |
|        - | 11788 | `	SyToken *pIn;      /* Token stream */` |
|        - | 11789 | `	SyToken *pEnd;     /* End of the token stream */` |
|        - | 11790 | `	int rec_depth;     /* Recursion limit */` |
|        - | 11791 | `	int rec_count;     /* Current nesting level */` |
|        - | 11792 | `	int *pErr;         /* JSON decoding error if any */` |
|        - | 11793 | `};` |
|        - | 11794 | `#define JSON_DECODE_ASSOC 0x01 /* Decode a JSON object as an associative array */` |
|        - | 11795 | `/* Forward declaration */` |
|        - | 11796 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData);` |
|        - | 11797 | `/*` |
|        - | 11798 | ` * Dequote [i.e: Resolve all backslash escapes ] a JSON string and store` |
|        - | 11799 | ` * the result in the given ph7_value.` |
|        - | 11800 | ` */` |
|        8 | 11801 | `static void VmJsonDequoteString(const SyString *pStr,ph7_value *pWorker)` |
|        2 | 11802 |  |
|       10 | 11803 | `	const char *zIn = pStr->zString;` |
|       10 | 11804 | `	const char *zEnd = &pStr->zString[pStr->nByte];` |
|        - | 11805 | `	const char *zCur;` |
|        - | 11806 | `	int c;` |
|        - | 11807 | `	/* Mark the value as a string */` |
|       10 | 11808 | `	ph7_value_string(pWorker,"",0); /* Empty string */` |
|        4 | 11809 | `	for(;;){` |
|       10 | 11810 | `		zCur = zIn;` |
|       32 | 11811 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|       24 | 11812 | `			zIn++;` |
|        2 | 11813 | `		}` |
|       10 | 11814 | `		if( zIn > zCur ){` |
|        - | 11815 | `			/* Append chunk verbatim */` |
|       10 | 11816 | `			ph7_value_string(pWorker,zCur,(int)(zIn-zCur));` |
|        4 | 11817 | `		}` |
|       10 | 11818 | `		zIn++;` |
|       10 | 11819 | `		if( zIn >= zEnd ){` |
|        - | 11820 | `			/* End of the input reached */` |
|       10 | 11821 | `			break;` |
|        - | 11822 | `		}` |
|      ! 0 | 11823 | `		c = zIn[0];` |
|        - | 11824 | `		/* Unescape the character */` |
|      ! 0 | 11825 | `		switch(c){` |
|      ! 0 | 11826 | `		case '"':  ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|      ! 0 | 11827 | `		case '\\': ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|      ! 0 | 11828 | `		case 'n':  ph7_value_string(pWorker,"\n",(int)sizeof(char)); break;` |
|      ! 0 | 11829 | `		case 'r':  ph7_value_string(pWorker,"\r",(int)sizeof(char)); break;` |
|      ! 0 | 11830 | `		case 't':  ph7_value_string(pWorker,"\t",(int)sizeof(char)); break;` |
|      ! 0 | 11831 | `		case 'f':  ph7_value_string(pWorker,"\f",(int)sizeof(char)); break;` |
|      ! 0 | 11832 | `		default:` |
|      ! 0 | 11833 | `			ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char));` |
|      ! 0 | 11834 | `			break;` |
|        - | 11835 | `		}` |
|        - | 11836 | `		/* Advance the stream cursor */` |
|      ! 0 | 11837 | `		zIn++;` |
|      ! 0 | 11838 | `	}` |
|       10 | 11839 |  |
|        - | 11840 | `/*` |
|        - | 11841 | ` * Returns a ph7_value holding the image of a JSON string. In other word perform a JSON decoding operation.` |
|        - | 11842 | ` * According to wikipedia` |
|        - | 11843 | ` * JSON's basic types are:` |
|        - | 11844 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|        - | 11845 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|        - | 11846 | ` *   Boolean (true or false)` |
|        - | 11847 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|        - | 11848 | ` *    do not need to be of the same type)` |
|        - | 11849 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|        - | 11850 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|        - | 11851 | ` *     be distinct from each other)` |
|        - | 11852 | ` *   null (empty)` |
|        - | 11853 | ` * Non-significant white space may be added freely around the "structural characters" (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|        - | 11854 | ` */` |
|       24 | 11855 | `static sxi32 VmJsonDecode(` |
|        - | 11856 | `	json_decoder *pDecoder, /* JSON decoder */` |
|        - | 11857 | `	ph7_value *pArrayKey    /* Key for the decoded array */` |
|        2 | 11858 | `	){` |
|        - | 11859 | `	ph7_value *pWorker; /* Worker variable */` |
|        - | 11860 | `	sxi32 rc;` |
|        - | 11861 | `	/* Check if we do not nest to much */` |
|       26 | 11862 | `	if( pDecoder->rec_count >= pDecoder->rec_depth ){` |
|        - | 11863 | `		/* Nesting limit reached,abort decoding immediately */` |
|      ! 0 | 11864 | `		*pDecoder->pErr = JSON_ERROR_DEPTH;` |
|      ! 0 | 11865 | `		return SXERR_ABORT;` |
|        - | 11866 | `	}` |
|       26 | 11867 | `	if( pDecoder->pIn->nType & (JSON_TK_STR\|JSON_TK_TRUE\|JSON_TK_FALSE\|JSON_TK_NULL\|JSON_TK_NUM) ){` |
|        - | 11868 | `		/* Scalar value */` |
|       16 | 11869 | `		pWorker = ph7_context_new_scalar(pDecoder->pCtx);` |
|       16 | 11870 | `		if( pWorker == 0 ){` |
|      ! 0 | 11871 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - | 11872 | `			/* Abort the decoding operation immediately */` |
|      ! 0 | 11873 | `			return SXERR_ABORT;` |
|        - | 11874 | `		}` |
|        - | 11875 | `		/* Reflect the JSON image */` |
|       16 | 11876 | `		if( pDecoder->pIn->nType & JSON_TK_NULL ){` |
|        - | 11877 | `			/* Nullify the value.*/` |
|      ! 0 | 11878 | `			ph7_value_null(pWorker);` |
|       16 | 11879 | `		}else if( pDecoder->pIn->nType & (JSON_TK_TRUE\|JSON_TK_FALSE) ){` |
|        - | 11880 | `			/* Boolean value */` |
|      ! 0 | 11881 | `			ph7_value_bool(pWorker,(pDecoder->pIn->nType & JSON_TK_TRUE) ? 1 : 0 );` |
|       16 | 11882 | `		}else if( pDecoder->pIn->nType & JSON_TK_NUM ){` |
|       13 | 11883 | `			SyString *pStr = &pDecoder->pIn->sData;` |
|        - | 11884 | `			/*` |
|        - | 11885 | `			 * Numeric value.` |
|        - | 11886 | `			 * Get a string representation first then try to get a numeric` |
|        - | 11887 | `			 * value.` |
|        - | 11888 | `			 */` |
|       13 | 11889 | `			ph7_value_string(pWorker,pStr->zString,(int)pStr->nByte);` |
|        - | 11890 | `			/* Obtain a numeric representation */` |
|       13 | 11891 | `			PH7_MemObjToNumeric(pWorker);` |
|        7 | 11892 | `		}else{` |
|        - | 11893 | `			/* Dequote the string */` |
|        3 | 11894 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pWorker);` |
|        - | 11895 | `		}` |
|        - | 11896 | `		/* Invoke the consumer callback */` |
|       16 | 11897 | `		rc = pDecoder->xConsumer(pDecoder->pCtx,pArrayKey,pWorker,pDecoder->pUserData);` |
|       16 | 11898 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11899 | `			return SXERR_ABORT;` |
|        - | 11900 | `		}` |
|        - | 11901 | `		/* All done,advance the stream cursor */` |
|       16 | 11902 | `		pDecoder->pIn++;` |
|       19 | 11903 | `	}else if( pDecoder->pIn->nType & JSON_TK_OSB /*'[' */) {` |
|        - | 11904 | `		ProcJsonConsumer xOld;` |
|        - | 11905 | `		void *pOld;` |
|        - | 11906 | `		/* Array representation*/` |
|        5 | 11907 | `		pDecoder->pIn++;` |
|        - | 11908 | `		/* Create a working array */` |
|        5 | 11909 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|        5 | 11910 | `		if( pWorker == 0 ){` |
|      ! 0 | 11911 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - | 11912 | `			/* Abort the decoding operation immediately */` |
|      ! 0 | 11913 | `			return SXERR_ABORT;` |
|        - | 11914 | `		}` |
|        - | 11915 | `		/* Save the old consumer */` |
|        5 | 11916 | `		xOld = pDecoder->xConsumer;` |
|        5 | 11917 | `		pOld = pDecoder->pUserData;` |
|        - | 11918 | `		/* Set the new consumer */` |
|        5 | 11919 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|        5 | 11920 | `		pDecoder->pUserData = pWorker;` |
|        - | 11921 | `		/* Decode the array */` |
|        7 | 11922 | `		for(;;){` |
|        - | 11923 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|        - | 11924 | `			 * do this.` |
|        - | 11925 | `			 */` |
|       21 | 11926 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|        7 | 11927 | `				pDecoder->pIn++;` |
|        1 | 11928 | `			}` |
|       15 | 11929 | `			if( pDecoder->pIn >= pDecoder->pEnd \|\| (pDecoder->pIn->nType & JSON_TK_CSB) /*']'*/ ){` |
|        5 | 11930 | `				if( pDecoder->pIn < pDecoder->pEnd ){` |
|        5 | 11931 | `					pDecoder->pIn++; /* Jump the trailing ']' */` |
|        2 | 11932 | `				}` |
|        5 | 11933 | `				break;` |
|        - | 11934 | `			}` |
|        - | 11935 | `			/* Recurse and decode the entry */` |
|       11 | 11936 | `			pDecoder->rec_count++;` |
|       11 | 11937 | `			rc = VmJsonDecode(pDecoder,0);` |
|       11 | 11938 | `			pDecoder->rec_count--;` |
|       11 | 11939 | `			if( rc == SXERR_ABORT ){` |
|        - | 11940 | `				/* Abort processing immediately */` |
|      ! 0 | 11941 | `				return SXERR_ABORT;` |
|        - | 11942 | `			}` |
|        - | 11943 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|       11 | 11944 | `			if( (pDecoder->pIn < pDecoder->pEnd) &&` |
|       10 | 11945 | `				((pDecoder->pIn->nType & (JSON_TK_CSB/*']'*/\|JSON_TK_COMMA/*','*/))==0) ){` |
|        - | 11946 | `					/* Unexpected token,abort immediatley */` |
|      ! 0 | 11947 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|      ! 0 | 11948 | `					return SXERR_ABORT;` |
|        - | 11949 | `			}` |
|        1 | 11950 | `		}` |
|        - | 11951 | `		/* Restore the old consumer */` |
|        5 | 11952 | `		pDecoder->xConsumer = xOld;` |
|        5 | 11953 | `		pDecoder->pUserData = pOld;` |
|        - | 11954 | `		/* Invoke the old consumer on the decoded array */` |
|        5 | 11955 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|       10 | 11956 | `	}else if( pDecoder->pIn->nType & JSON_TK_OCB /*'{' */) {` |
|        - | 11957 | `		ProcJsonConsumer xOld;` |
|        - | 11958 | `		ph7_value *pKey;` |
|        - | 11959 | `		void *pOld;` |
|        - | 11960 | `		/* Object representation*/` |
|        8 | 11961 | `		pDecoder->pIn++;` |
|        - | 11962 | `		/* Return the object as an associative array */` |
|        8 | 11963 | `		if( (pDecoder->iFlags & JSON_DECODE_ASSOC) == 0 ){` |
|        3 | 11964 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_WARNING,` |
|        - | 11965 | `				"JSON Objects are always returned as an associative array"` |
|        - | 11966 | `				);` |
|        1 | 11967 | `		}` |
|        - | 11968 | `		/* Create a working array */` |
|        8 | 11969 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|        8 | 11970 | `		pKey = ph7_context_new_scalar(pDecoder->pCtx);` |
|        8 | 11971 | `		if( pWorker == 0 \|\| pKey == 0){` |
|      ! 0 | 11972 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - | 11973 | `			/* Abort the decoding operation immediately */` |
|      ! 0 | 11974 | `			return SXERR_ABORT;` |
|        - | 11975 | `		}` |
|        - | 11976 | `		/* Save the old consumer */` |
|        8 | 11977 | `		xOld = pDecoder->xConsumer;` |
|        8 | 11978 | `		pOld = pDecoder->pUserData;` |
|        - | 11979 | `		/* Set the new consumer */` |
|        8 | 11980 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|        8 | 11981 | `		pDecoder->pUserData = pWorker;` |
|        - | 11982 | `		/* Decode the object */` |
|        6 | 11983 | `		for(;;){` |
|        - | 11984 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|        - | 11985 | `			 * do this.` |
|        - | 11986 | `			 */` |
|       16 | 11987 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|        3 | 11988 | `				pDecoder->pIn++;` |
|        1 | 11989 | `			}` |
|       14 | 11990 | `			if( pDecoder->pIn >= pDecoder->pEnd \|\| (pDecoder->pIn->nType & JSON_TK_CCB) /*'}'*/ ){` |
|        8 | 11991 | `				if( pDecoder->pIn < pDecoder->pEnd ){` |
|        6 | 11992 | `					pDecoder->pIn++; /* Jump the trailing ']' */` |
|        2 | 11993 | `				}` |
|        8 | 11994 | `				break;` |
|        - | 11995 | `			}` |
|        6 | 11996 | `			if( (pDecoder->pIn->nType & JSON_TK_STR) == 0 \|\| &pDecoder->pIn[1] >= pDecoder->pEnd` |
|        8 | 11997 | `				\|\| (pDecoder->pIn[1].nType & JSON_TK_COLON) == 0){` |
|        - | 11998 | `					/* Syntax error,return immediately */` |
|      ! 0 | 11999 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|      ! 0 | 12000 | `					return SXERR_ABORT;` |
|        - | 12001 | `			}` |
|        - | 12002 | `			/* Dequote the key */` |
|        8 | 12003 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pKey);` |
|        - | 12004 | `			/* Jump the key and the colon */` |
|        8 | 12005 | `			pDecoder->pIn += 2;` |
|        - | 12006 | `			/* Recurse and decode the value */` |
|        8 | 12007 | `			pDecoder->rec_count++;` |
|        8 | 12008 | `			rc = VmJsonDecode(pDecoder,pKey);` |
|        8 | 12009 | `			pDecoder->rec_count--;` |
|        8 | 12010 | `			if( rc == SXERR_ABORT ){` |
|        - | 12011 | `				/* Abort processing immediately */` |
|      ! 0 | 12012 | `				return SXERR_ABORT;` |
|        - | 12013 | `			}` |
|        - | 12014 | `			/* Reset the internal buffer of the key */` |
|        8 | 12015 | `			ph7_value_reset_string_cursor(pKey);` |
|        - | 12016 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|        2 | 12017 | `		}` |
|        - | 12018 | `		/* Restore the old consumer */` |
|        8 | 12019 | `		pDecoder->xConsumer = xOld;` |
|        8 | 12020 | `		pDecoder->pUserData = pOld;` |
|        - | 12021 | `		/* Invoke the old consumer on the decoded object*/` |
|        8 | 12022 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|        - | 12023 | `		/* Release the key */` |
|        8 | 12024 | `		ph7_context_release_value(pDecoder->pCtx,pKey);` |
|        5 | 12025 | `	}else{` |
|        - | 12026 | `		/* Unexpected token */` |
|      ! 0 | 12027 | `		return SXERR_ABORT; /* Abort immediately */` |
|        - | 12028 | `	}` |
|        - | 12029 | `	/* Release the worker variable */` |
|       26 | 12030 | `	ph7_context_release_value(pDecoder->pCtx,pWorker);` |
|       26 | 12031 | `	return SXRET_OK;` |
|       14 | 12032 |  |
|        - | 12033 | `/*` |
|        - | 12034 | ` * The following JSON decoder callback is invoked each time` |
|        - | 12035 | ` * a JSON array representation [i.e: [15,"hello",FALSE] ]` |
|        - | 12036 | ` * is being decoded.` |
|        - | 12037 | ` */` |
|       16 | 12038 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|        2 | 12039 |  |
|       18 | 12040 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 12041 | `	/* Insert the entry */` |
|       18 | 12042 | `	ph7_array_add_elem(pArray,pKey,pWorker); /* Will make it's own copy */` |
|        8 | 12043 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 12044 | `	/* All done */` |
|       18 | 12045 | `	return SXRET_OK;` |
|        2 | 12046 |  |
|        - | 12047 | `/*` |
|        - | 12048 | ` * Standard JSON decoder callback.` |
|        - | 12049 | ` */` |
|        8 | 12050 | `static int VmJsonDefaultDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|        2 | 12051 |  |
|        - | 12052 | `	/* Return the value directly */` |
|       10 | 12053 | `	ph7_result_value(pCtx,pWorker); /* Will make it's own copy */` |
|        4 | 12054 | `	SXUNUSED(pKey); /* cc warning */` |
|        4 | 12055 | `	SXUNUSED(pUserData);` |
|        - | 12056 | `	/* All done */` |
|       10 | 12057 | `	return SXRET_OK;` |
|        2 | 12058 |  |
|        - | 12059 | `/*` |
|        - | 12060 | ` * mixed json_decode(string $json[,bool $assoc = false[,int $depth = 32[,int $options = 0 ]]])` |
|        - | 12061 | ` *  Takes a JSON encoded string and converts it into a PHP variable.` |
|        - | 12062 | ` * Parameters` |
|        - | 12063 | ` *  $json` |
|        - | 12064 | ` *    The json string being decoded.` |
|        - | 12065 | ` * $assoc` |
|        - | 12066 | ` *   When TRUE, returned objects will be converted into associative arrays.` |
|        - | 12067 | ` * $depth` |
|        - | 12068 | ` *   User specified recursion depth.` |
|        - | 12069 | ` * $options` |
|        - | 12070 | ` *   Bitmask of JSON decode options. Currently only JSON_BIGINT_AS_STRING is supported` |
|        - | 12071 | ` * (default is to cast large integers as floats)` |
|        - | 12072 | ` * Return` |
|        - | 12073 | ` *  The value encoded in json in appropriate PHP type. Values true, false and null (case-insensitive)` |
|        - | 12074 | ` *  are returned as TRUE, FALSE and NULL respectively. NULL is returned if the json cannot be decoded` |
|        - | 12075 | ` *  or if the encoded data is deeper than the recursion limit.` |
|        - | 12076 | ` */` |
|       16 | 12077 | `static int vm_builtin_json_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12078 |  |
|       18 | 12079 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12080 | `	json_decoder sDecoder;` |
|        - | 12081 | `	const char *zIn;` |
|        - | 12082 | `	SySet sToken;` |
|        - | 12083 | `	SyLex sLex;` |
|        - | 12084 | `	int nByte;` |
|        - | 12085 | `	sxi32 rc;` |
|       18 | 12086 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 12087 | `		/* Missing/Invalid arguments, return NULL */` |
|        3 | 12088 | `		ph7_result_null(pCtx);` |
|        3 | 12089 | `		return PH7_OK;` |
|        - | 12090 | `	}` |
|        - | 12091 | `	/* Extract the JSON string */` |
|       16 | 12092 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|       16 | 12093 | `	if( nByte < 1 ){` |
|        - | 12094 | `		/* Empty string,return NULL */` |
|      ! 0 | 12095 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12096 | `		return PH7_OK;` |
|        - | 12097 | `	}` |
|        - | 12098 | `	/* Clear JSON error code */` |
|       16 | 12099 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - | 12100 | `	/* Tokenize the input */` |
|       16 | 12101 | `	SySetInit(&sToken,&pVm->sAllocator,sizeof(SyToken));` |
|       16 | 12102 | `	SyLexInit(&sLex,&sToken,VmJsonTokenize,&pVm->json_rc);` |
|       16 | 12103 | `	SyLexTokenizeInput(&sLex,zIn,(sxu32)nByte,0,0,0);` |
|       16 | 12104 | `	if( pVm->json_rc != JSON_ERROR_NONE ){` |
|        - | 12105 | `		/* Something goes wrong while tokenizing input. [i.e: Unexpected token] */` |
|        8 | 12106 | `		SyLexRelease(&sLex);` |
|        8 | 12107 | `		SySetRelease(&sToken);` |
|        - | 12108 | `		/* return NULL */` |
|        8 | 12109 | `		ph7_result_null(pCtx);` |
|        8 | 12110 | `		return PH7_OK;` |
|        - | 12111 | `	}` |
|        - | 12112 | `	/* Fill the decoder */` |
|       10 | 12113 | `	sDecoder.pCtx = pCtx;` |
|       10 | 12114 | `	sDecoder.pErr = &pVm->json_rc;` |
|       10 | 12115 | `	sDecoder.pIn = (SyToken *)SySetBasePtr(&sToken);` |
|       10 | 12116 | `	sDecoder.pEnd = &sDecoder.pIn[SySetUsed(&sToken)];` |
|       10 | 12117 | `	sDecoder.iFlags = 0;` |
|       10 | 12118 | `	if( nArg > 1 && ph7_value_to_bool(apArg[1]) != 0 ){` |
|        - | 12119 | `		/* Returned objects will be converted into associative arrays */` |
|        8 | 12120 | `		sDecoder.iFlags \|= JSON_DECODE_ASSOC;` |
|        3 | 12121 | `	}` |
|       10 | 12122 | `	sDecoder.rec_depth = 32;` |
|       10 | 12123 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|      ! 0 | 12124 | `		int nDepth = ph7_value_to_int(apArg[2]);` |
|      ! 0 | 12125 | `		if( nDepth > 1 && nDepth < 32 ){` |
|      ! 0 | 12126 | `			sDecoder.rec_depth = nDepth;` |
|      ! 0 | 12127 | `		}` |
|      ! 0 | 12128 | `	}` |
|       10 | 12129 | `	sDecoder.rec_count = 0;` |
|        - | 12130 | `	/* Set a default consumer */` |
|       10 | 12131 | `	sDecoder.xConsumer = VmJsonDefaultDecoder;` |
|       10 | 12132 | `	sDecoder.pUserData = 0;` |
|        - | 12133 | `	/* Decode the raw JSON input */` |
|       10 | 12134 | `	rc = VmJsonDecode(&sDecoder,0);` |
|       10 | 12135 | `	if( rc == SXERR_ABORT \|\|  pVm->json_rc != JSON_ERROR_NONE ){` |
|        - | 12136 | `		/*` |
|        - | 12137 | `		 * Something goes wrong while decoding JSON input.Return NULL.` |
|        - | 12138 | `		 */` |
|      ! 0 | 12139 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12140 | `	}` |
|        - | 12141 | `	/* Clean-up the mess left behind */` |
|       10 | 12142 | `	SyLexRelease(&sLex);` |
|       10 | 12143 | `	SySetRelease(&sToken);` |
|        - | 12144 | `	/* All done */` |
|       10 | 12145 | `	return PH7_OK;` |
|       10 | 12146 |  |
|        - | 12147 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12148 | `/*` |
|        - | 12149 | ` * XML processing Functions.` |
|        - | 12150 | ` * Status:` |
|        - | 12151 | ` *    Devel.` |
|        - | 12152 | ` */` |
|        - | 12153 | `enum ph7_xml_handler_id{` |
|        - | 12154 | `	PH7_XML_START_TAG = 0, /* Start element handlers ID */` |
|        - | 12155 | `	PH7_XML_END_TAG,       /* End element handler ID*/` |
|        - | 12156 | `	PH7_XML_CDATA,         /* Character data handler ID*/` |
|        - | 12157 | `	PH7_XML_PI,            /* Processing instruction (PI) handler ID*/` |
|        - | 12158 | `	PH7_XML_DEF,           /* Default handler ID */` |
|        - | 12159 | `	PH7_XML_UNPED,         /* Unparsed entity declaration handler */` |
|        - | 12160 | `	PH7_XML_ND,            /* Notation declaration handler ID*/` |
|        - | 12161 | `	PH7_XML_EER,           /* External entity reference handler */` |
|        - | 12162 | `	PH7_XML_NS_START,      /* Start namespace declaration handler */` |
|        - | 12163 | `	PH7_XML_NS_END         /* End namespace declaration handler */` |
|        - | 12164 | `};` |
|        - | 12165 | `#define XML_TOTAL_HANDLER (PH7_XML_NS_END + 1)` |
|        - | 12166 | `/* An instance of the following structure describe a working` |
|        - | 12167 | ` * XML engine instance.` |
|        - | 12168 | ` */` |
|        - | 12169 | `typedef struct ph7_xml_engine ph7_xml_engine;` |
|        - | 12170 | `struct ph7_xml_engine` |
|        - | 12171 |  |
|        - | 12172 | `	ph7_vm *pVm;         /* VM that own this instance */` |
|        - | 12173 | `	ph7_context *pCtx;   /* Call context */` |
|        - | 12174 | `	SyXMLParser sParser; /* Underlying XML parser */` |
|        - | 12175 | `	ph7_value aCB[XML_TOTAL_HANDLER]; /* User-defined callbacks */` |
|        - | 12176 | `	ph7_value sParserValue; /* ph7_value holding this instance which is forwarded` |
|        - | 12177 | `							  * as the first argument to the user callbacks.` |
|        - | 12178 | `							  */` |
|        - | 12179 | `	int ns_sep;      /* Namespace separator */` |
|        - | 12180 | `	SyBlob sErr;     /* Error message consumer */` |
|        - | 12181 | `	sxi32 iErrCode;  /* Last error code */` |
|        - | 12182 | `	sxi32 iNest;     /* Nesting level */` |
|        - | 12183 | `	sxu32 nLine;     /* Last processed line */` |
|        - | 12184 | `	sxu32 nMagic;    /* Magic number so that we avoid misuse  */` |
|        - | 12185 | `};` |
|        - | 12186 | `#define XML_ENGINE_MAGIC 0x851EFC52` |
|        - | 12187 | `#define IS_INVALID_XML_ENGINE(XML) (XML == 0 \|\| (XML)->nMagic != XML_ENGINE_MAGIC)` |
|        - | 12188 | `/*` |
|        - | 12189 | ` * Allocate and initialize an XML engine.` |
|        - | 12190 | ` */` |
|       84 | 12191 | `static ph7_xml_engine * VmCreateXMLEngine(ph7_context *pCtx,int process_ns,int ns_sep)` |
|        1 | 12192 |  |
|        - | 12193 | `	ph7_xml_engine *pEngine;` |
|       85 | 12194 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12195 | `	ph7_value *pValue;` |
|        - | 12196 | `	sxu32 n;` |
|        - | 12197 | `	/* Allocate a new instance */` |
|       85 | 12198 | `	pEngine = (ph7_xml_engine *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_xml_engine));` |
|       85 | 12199 | `	if( pEngine == 0 ){` |
|        - | 12200 | `		/* Out of memory */` |
|      ! 0 | 12201 | `		return 0;` |
|        - | 12202 | `	}` |
|        - | 12203 | `	/* Zero the structure */` |
|       85 | 12204 | `	SyZero(pEngine,sizeof(ph7_xml_engine));` |
|        - | 12205 | `	/* Initialize fields */` |
|       85 | 12206 | `	pEngine->pVm = pVm;` |
|       85 | 12207 | `	pEngine->pCtx = 0;` |
|       85 | 12208 | `	pEngine->ns_sep = ns_sep;` |
|       85 | 12209 | `	SyXMLParserInit(&pEngine->sParser,&pVm->sAllocator,process_ns ? SXML_ENABLE_NAMESPACE : 0);` |
|       85 | 12210 | `	SyBlobInit(&pEngine->sErr,&pVm->sAllocator);` |
|       85 | 12211 | `	PH7_MemObjInit(pVm,&pEngine->sParserValue);` |
|      925 | 12212 | `	for( n = 0 ; n < SX_ARRAYSIZE(pEngine->aCB) ; ++n ){` |
|      841 | 12213 | `		pValue = &pEngine->aCB[n];` |
|        - | 12214 | `		/* NULLIFY the array entries,until someone register an event handler */` |
|      841 | 12215 | `		PH7_MemObjInit(&(*pVm),pValue);` |
|      421 | 12216 | `	}` |
|       85 | 12217 | `	ph7_value_resource(&pEngine->sParserValue,pEngine);` |
|       85 | 12218 | `	pEngine->iErrCode = SXML_ERROR_NONE;` |
|        - | 12219 | `	/* Finally set the magic number */` |
|       85 | 12220 | `	pEngine->nMagic = XML_ENGINE_MAGIC;` |
|       85 | 12221 | `	return pEngine;` |
|       43 | 12222 |  |
|        - | 12223 | `/*` |
|        - | 12224 | ` * Release an XML engine.` |
|        - | 12225 | ` */` |
|       84 | 12226 | `static void VmReleaseXMLEngine(ph7_xml_engine *pEngine)` |
|        1 | 12227 |  |
|       85 | 12228 | `	ph7_vm *pVm = pEngine->pVm;` |
|        - | 12229 | `	ph7_value *pValue;` |
|        - | 12230 | `	sxu32 n;` |
|        - | 12231 | `	/* Release fields */` |
|       85 | 12232 | `	SyBlobRelease(&pEngine->sErr);` |
|       85 | 12233 | `	SyXMLParserRelease(&pEngine->sParser);` |
|       85 | 12234 | `	PH7_MemObjRelease(&pEngine->sParserValue);` |
|      925 | 12235 | `	for( n = 0 ; n < SX_ARRAYSIZE(pEngine->aCB) ; ++n ){` |
|      841 | 12236 | `		pValue = &pEngine->aCB[n];` |
|      841 | 12237 | `		PH7_MemObjRelease(pValue);` |
|      421 | 12238 | `	}` |
|       85 | 12239 | `	pEngine->nMagic = 0x2621;` |
|        - | 12240 | `	/* Finally,release the whole instance */` |
|       85 | 12241 | `	SyMemBackendFree(&pVm->sAllocator,pEngine);` |
|       85 | 12242 |  |
|        - | 12243 | `/*` |
|        - | 12244 | ` * resource xml_parser_create([ string $encoding ])` |
|        - | 12245 | ` *  Create an UTF-8 XML parser.` |
|        - | 12246 | ` * Parameter` |
|        - | 12247 | ` *  $encoding` |
|        - | 12248 | ` *   (Only UTF-8 encoding is used)` |
|        - | 12249 | ` * Return` |
|        - | 12250 | ` *  Returns a resource handle for the new XML parser.` |
|        - | 12251 | ` */` |
|       80 | 12252 | `static int vm_builtin_xml_parser_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12253 |  |
|        - | 12254 | `	ph7_xml_engine *pEngine;` |
|        - | 12255 | `	/* Allocate a new instance */` |
|       81 | 12256 | `	pEngine = VmCreateXMLEngine(&(*pCtx),0,':');` |
|       81 | 12257 | `	if( pEngine == 0 ){` |
|      ! 0 | 12258 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - | 12259 | `		/* Return null */` |
|      ! 0 | 12260 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12261 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12262 | `		SXUNUSED(apArg);` |
|      ! 0 | 12263 | `		return PH7_OK;` |
|        - | 12264 | `	}` |
|        - | 12265 | `	/* Return the engine as a resource */` |
|       81 | 12266 | `	ph7_result_resource(pCtx,pEngine);` |
|       81 | 12267 | `	return PH7_OK;` |
|       41 | 12268 |  |
|        - | 12269 | `/*` |
|        - | 12270 | ` * resource xml_parser_create_ns([ string $encoding[,string $separator = ':']])` |
|        - | 12271 | ` *  Create an UTF-8 XML parser with namespace support.` |
|        - | 12272 | ` * Parameter` |
|        - | 12273 | ` *  $encoding` |
|        - | 12274 | ` *   (Only UTF-8 encoding is supported)` |
|        - | 12275 | ` *  $separtor` |
|        - | 12276 | ` *   Namespace separator (a single character)` |
|        - | 12277 | ` * Return` |
|        - | 12278 | ` *  Returns a resource handle for the new XML parser.` |
|        - | 12279 | ` */` |
|        4 | 12280 | `static int vm_builtin_xml_parser_create_ns(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12281 |  |
|        - | 12282 | `	ph7_xml_engine *pEngine;` |
|        5 | 12283 | `	int ns_sep = ':';` |
|        5 | 12284 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      ! 0 | 12285 | `		const char *zSep = ph7_value_to_string(apArg[1],0);` |
|      ! 0 | 12286 | `		if( zSep[0] != 0 ){` |
|      ! 0 | 12287 | `			ns_sep = zSep[0];` |
|      ! 0 | 12288 | `		}` |
|      ! 0 | 12289 | `	}` |
|        - | 12290 | `	/* Allocate a new instance */` |
|        5 | 12291 | `	pEngine = VmCreateXMLEngine(&(*pCtx),TRUE,ns_sep);` |
|        5 | 12292 | `	if( pEngine == 0 ){` |
|      ! 0 | 12293 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - | 12294 | `		/* Return null */` |
|      ! 0 | 12295 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12296 | `		return PH7_OK;` |
|        - | 12297 | `	}` |
|        - | 12298 | `	/* Return the engine as a resource */` |
|        5 | 12299 | `	ph7_result_resource(pCtx,pEngine);` |
|        5 | 12300 | `	return PH7_OK;` |
|        3 | 12301 |  |
|        - | 12302 | `/*` |
|        - | 12303 | ` * bool xml_parser_free(resource $parser)` |
|        - | 12304 | ` *  Release an XML engine.` |
|        - | 12305 | ` * Parameter` |
|        - | 12306 | ` *  $parser` |
|        - | 12307 | ` *   A reference to the XML parser to free.` |
|        - | 12308 | ` * Return` |
|        - | 12309 | ` *  This function returns FALSE if parser does not refer` |
|        - | 12310 | ` *  to a valid parser, or else it frees the parser and returns TRUE.` |
|        - | 12311 | ` */` |
|       84 | 12312 | `static int vm_builtin_xml_parser_free(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12313 |  |
|        - | 12314 | `	ph7_xml_engine *pEngine;` |
|       85 | 12315 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12316 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12317 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12318 | `		return PH7_OK;` |
|        - | 12319 | `	}` |
|        - | 12320 | `	/* Point to the XML engine */` |
|       85 | 12321 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|       85 | 12322 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12323 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12324 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12325 | `		return PH7_OK;` |
|        - | 12326 | `	}` |
|        - | 12327 | `	/* Safely release the engine */` |
|       85 | 12328 | `	VmReleaseXMLEngine(pEngine);` |
|        - | 12329 | `	/* Return TRUE */` |
|       85 | 12330 | `	ph7_result_bool(pCtx,1);` |
|       85 | 12331 | `	return PH7_OK;` |
|       43 | 12332 |  |
|        - | 12333 | `/*` |
|        - | 12334 | ` * bool xml_set_element_handler(resource $parser,callback $start_element_handler,[callback $end_element_handler])` |
|        - | 12335 | ` * Sets the element handler functions for the XML parser. start_element_handler and end_element_handler` |
|        - | 12336 | ` * are strings containing the names of functions.` |
|        - | 12337 | ` * Parameters` |
|        - | 12338 | ` *  $parser` |
|        - | 12339 | ` *   A reference to the XML parser to set up start and end element handler functions.` |
|        - | 12340 | ` *  $start_element_handler` |
|        - | 12341 | ` *    The function named by start_element_handler must accept three parameters:` |
|        - | 12342 | ` *    start_element_handler(resource $parser,string $name,array $attribs)` |
|        - | 12343 | ` *    $parser` |
|        - | 12344 | ` *      The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12345 | ` *   $name` |
|        - | 12346 | ` *      The second parameter, name, contains the name of the element for which this handler` |
|        - | 12347 | ` *		is called.If case-folding is in effect for this parser, the element name will be in uppercase letters.` |
|        - | 12348 | ` *  $attribs` |
|        - | 12349 | ` *      The third parameter, attribs, contains an associative array with the element's attributes (if any).` |
|        - | 12350 | ` *		The keys of this array are the attribute names, the values are the attribute values.` |
|        - | 12351 | ` *      Attribute names are case-folded on the same criteria as element names.Attribute values are not case-folded.` |
|        - | 12352 | ` *      The original order of the attributes can be retrieved by walking through attribs the normal way, using each().` |
|        - | 12353 | ` *      The first key in the array was the first attribute, and so on.` |
|        - | 12354 | ` *      Note: Instead of a function name, an array containing an object reference and a method name can also be supplied.` |
|        - | 12355 | ` * $end_element_handler` |
|        - | 12356 | ` *     The function named by end_element_handler must accept two parameters:` |
|        - | 12357 | ` *     end_element_handler(resource $parser,string $name)` |
|        - | 12358 | ` *    $parser` |
|        - | 12359 | ` *      The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12360 | ` *   $name` |
|        - | 12361 | ` *      The second parameter, name, contains the name of the element for which this handler` |
|        - | 12362 | ` *      is called.If case-folding is in effect for this parser, the element name will be in uppercase` |
|        - | 12363 | ` *      letters.` |
|        - | 12364 | ` *      If a handler function is set to an empty string, or FALSE, the handler in question is disabled.` |
|        - | 12365 | ` * Return` |
|        - | 12366 | ` * TRUE on success or FALSE on failure.` |
|        - | 12367 | ` */` |
|       66 | 12368 | `static int vm_builtin_xml_set_element_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12369 |  |
|        - | 12370 | `	ph7_xml_engine *pEngine;` |
|       67 | 12371 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12372 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12373 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12374 | `		return PH7_OK;` |
|        - | 12375 | `	}` |
|        - | 12376 | `	/* Point to the XML engine */` |
|       67 | 12377 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|       67 | 12378 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12379 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12380 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12381 | `		return PH7_OK;` |
|        - | 12382 | `	}` |
|       67 | 12383 | `	if( nArg > 1 ){` |
|        - | 12384 | `		/* Save the start_element_handler callback for later invocation */` |
|       67 | 12385 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_START_TAG]);` |
|       67 | 12386 | `		if( nArg > 2 ){` |
|        - | 12387 | `			/* Save the end_element_handler callback for later invocation */` |
|       67 | 12388 | `			PH7_MemObjStore(apArg[2]/* User callback*/,&pEngine->aCB[PH7_XML_END_TAG]);` |
|       33 | 12389 | `		}` |
|       33 | 12390 | `	}` |
|        - | 12391 | `	/* All done,return TRUE */` |
|       67 | 12392 | `	ph7_result_bool(pCtx,1);` |
|       67 | 12393 | `	return PH7_OK;` |
|       34 | 12394 |  |
|        - | 12395 | `/*` |
|        - | 12396 | ` * bool xml_set_character_data_handler(resource $parser,callback $handler)` |
|        - | 12397 | ` *  Sets the character data handler function for the XML parser parser.` |
|        - | 12398 | ` * Parameters` |
|        - | 12399 | ` * $parser` |
|        - | 12400 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12401 | ` * $handler` |
|        - | 12402 | ` *  handler is a string containing the name of the callback.` |
|        - | 12403 | ` *  The function named by handler must accept two parameters:` |
|        - | 12404 | ` *   handler(resource $parser,string $data)` |
|        - | 12405 | ` *  $parser` |
|        - | 12406 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12407 | ` *  $data` |
|        - | 12408 | ` *   The second parameter, data, contains the character data as a string.` |
|        - | 12409 | ` *   Character data handler is called for every piece of a text in the XML document.` |
|        - | 12410 | ` *   It can be called multiple times inside each fragment (e.g. for non-ASCII strings).` |
|        - | 12411 | ` *   If a handler function is set to an empty string, or FALSE, the handler in question is disabled.` |
|        - | 12412 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12413 | ` *   can also be supplied.` |
|        - | 12414 | ` * Return` |
|        - | 12415 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12416 | ` */` |
|       40 | 12417 | `static int vm_builtin_xml_set_character_data_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12418 |  |
|        - | 12419 | `	ph7_xml_engine *pEngine;` |
|       41 | 12420 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12421 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12422 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12423 | `		return PH7_OK;` |
|        - | 12424 | `	}` |
|        - | 12425 | `	/* Point to the XML engine */` |
|       41 | 12426 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|       41 | 12427 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12428 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12429 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12430 | `		return PH7_OK;` |
|        - | 12431 | `	}` |
|       41 | 12432 | `	if( nArg > 1 ){` |
|        - | 12433 | `		/* Save the user callback for later invocation */` |
|       41 | 12434 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_CDATA]);` |
|       20 | 12435 | `	}` |
|        - | 12436 | `	/* All done,return TRUE */` |
|       41 | 12437 | `	ph7_result_bool(pCtx,1);` |
|       41 | 12438 | `	return PH7_OK;` |
|       21 | 12439 |  |
|        - | 12440 | `/*` |
|        - | 12441 | ` * bool xml_set_default_handler(resource $parser,callback $handler)` |
|        - | 12442 | ` *  Set up default handler.` |
|        - | 12443 | ` * Parameters` |
|        - | 12444 | ` * $parser` |
|        - | 12445 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12446 | ` * $handler` |
|        - | 12447 | ` *  handler is a string containing the name of the callback.` |
|        - | 12448 | ` *  The function named by handler must accept two parameters:` |
|        - | 12449 | ` *   handler(resource $parser,string $data)` |
|        - | 12450 | ` *  $parser` |
|        - | 12451 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12452 | ` *  $data` |
|        - | 12453 | ` *   The second parameter, data, contains the character data.This may be the XML declaration` |
|        - | 12454 | ` *   document type declaration, entities or other data for which no other handler exists.` |
|        - | 12455 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12456 | ` *   can also be supplied.` |
|        - | 12457 | ` * Return` |
|        - | 12458 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12459 | ` */` |
|        2 | 12460 | `static int vm_builtin_xml_set_default_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12461 |  |
|        - | 12462 | `	ph7_xml_engine *pEngine;` |
|        3 | 12463 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12464 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12465 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12466 | `		return PH7_OK;` |
|        - | 12467 | `	}` |
|        - | 12468 | `	/* Point to the XML engine */` |
|        3 | 12469 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12470 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12471 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12472 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12473 | `		return PH7_OK;` |
|        - | 12474 | `	}` |
|        3 | 12475 | `	if( nArg > 1 ){` |
|        - | 12476 | `		/* Save the user callback for later invocation */` |
|        3 | 12477 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_DEF]);` |
|        1 | 12478 | `	}` |
|        - | 12479 | `	/* All done,return TRUE */` |
|        3 | 12480 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12481 | `	return PH7_OK;` |
|        2 | 12482 |  |
|        - | 12483 | `/*` |
|        - | 12484 | ` * bool xml_set_end_namespace_decl_handler(resource $parser,callback $handler)` |
|        - | 12485 | ` *  Set up end namespace declaration handler.` |
|        - | 12486 | ` * Parameters` |
|        - | 12487 | ` * $parser` |
|        - | 12488 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12489 | ` * $handler` |
|        - | 12490 | ` *  handler is a string containing the name of the callback.` |
|        - | 12491 | ` *  The function named by handler must accept two parameters:` |
|        - | 12492 | ` *   handler(resource $parser,string $prefix)` |
|        - | 12493 | ` *  $parser` |
|        - | 12494 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12495 | ` *  $prefix` |
|        - | 12496 | ` *   The prefix is a string used to reference the namespace within an XML object.` |
|        - | 12497 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12498 | ` *   can also be supplied.` |
|        - | 12499 | ` * Return` |
|        - | 12500 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12501 | ` */` |
|        2 | 12502 | `static int vm_builtin_xml_set_end_namespace_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12503 |  |
|        - | 12504 | `	ph7_xml_engine *pEngine;` |
|        3 | 12505 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12506 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12507 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12508 | `		return PH7_OK;` |
|        - | 12509 | `	}` |
|        - | 12510 | `	/* Point to the XML engine */` |
|        3 | 12511 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12512 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12513 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12514 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12515 | `		return PH7_OK;` |
|        - | 12516 | `	}` |
|        3 | 12517 | `	if( nArg > 1 ){` |
|        - | 12518 | `		/* Save the user callback for later invocation */` |
|        3 | 12519 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_NS_END]);` |
|        1 | 12520 | `	}` |
|        - | 12521 | `	/* All done,return TRUE */` |
|        3 | 12522 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12523 | `	return PH7_OK;` |
|        2 | 12524 |  |
|        - | 12525 | `/*` |
|        - | 12526 | ` * bool xml_set_start_namespace_decl_handler(resource $parser,callback $handler)` |
|        - | 12527 | ` *  Set up start namespace declaration handler.` |
|        - | 12528 | ` * Parameters` |
|        - | 12529 | ` * $parser` |
|        - | 12530 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12531 | ` * $handler` |
|        - | 12532 | ` *  handler is a string containing the name of the callback.` |
|        - | 12533 | ` *  The function named by handler must accept two parameters:` |
|        - | 12534 | ` *   handler(resource $parser,string $prefix,string $uri)` |
|        - | 12535 | ` *  $parser` |
|        - | 12536 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12537 | ` *  $prefix` |
|        - | 12538 | ` *   The prefix is a string used to reference the namespace within an XML object.` |
|        - | 12539 | ` *  $uri` |
|        - | 12540 | ` *    Uniform Resource Identifier (URI) of namespace.` |
|        - | 12541 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12542 | ` *   can also be supplied.` |
|        - | 12543 | ` * Return` |
|        - | 12544 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12545 | ` */` |
|        2 | 12546 | `static int vm_builtin_xml_set_start_namespace_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12547 |  |
|        - | 12548 | `	ph7_xml_engine *pEngine;` |
|        3 | 12549 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12550 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12551 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12552 | `		return PH7_OK;` |
|        - | 12553 | `	}` |
|        - | 12554 | `	/* Point to the XML engine */` |
|        3 | 12555 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12556 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12557 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12558 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12559 | `		return PH7_OK;` |
|        - | 12560 | `	}` |
|        3 | 12561 | `	if( nArg > 1 ){` |
|        - | 12562 | `		/* Save the user callback for later invocation */` |
|        3 | 12563 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_NS_START]);` |
|        1 | 12564 | `	}` |
|        - | 12565 | `	/* All done,return TRUE */` |
|        3 | 12566 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12567 | `	return PH7_OK;` |
|        2 | 12568 |  |
|        - | 12569 | `/*` |
|        - | 12570 | ` * bool xml_set_processing_instruction_handler(resource $parser,callback $handler)` |
|        - | 12571 | ` *  Set up processing instruction (PI) handler.` |
|        - | 12572 | ` * Parameters` |
|        - | 12573 | ` * $parser` |
|        - | 12574 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12575 | ` * $handler` |
|        - | 12576 | ` *  handler is a string containing the name of the callback.` |
|        - | 12577 | ` *  The function named by handler must accept three parameters:` |
|        - | 12578 | ` *   handler(resource $parser,string $target,string $data)` |
|        - | 12579 | ` *  $parser` |
|        - | 12580 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12581 | ` *  $target` |
|        - | 12582 | ` *   The second parameter, target, contains the PI target.` |
|        - | 12583 | ` *  $data` |
|        - | 12584 | `     The third parameter, data, contains the PI data.` |
|        - | 12585 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12586 | ` *   can also be supplied.` |
|        - | 12587 | ` * Return` |
|        - | 12588 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12589 | ` */` |
|        8 | 12590 | `static int vm_builtin_xml_set_processing_instruction_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12591 |  |
|        - | 12592 | `	ph7_xml_engine *pEngine;` |
|        9 | 12593 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12594 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12595 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12596 | `		return PH7_OK;` |
|        - | 12597 | `	}` |
|        - | 12598 | `	/* Point to the XML engine */` |
|        9 | 12599 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        9 | 12600 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12601 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12602 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12603 | `		return PH7_OK;` |
|        - | 12604 | `	}` |
|        9 | 12605 | `	if( nArg > 1 ){` |
|        - | 12606 | `		/* Save the user callback for later invocation */` |
|        9 | 12607 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_PI]);` |
|        4 | 12608 | `	}` |
|        - | 12609 | `	/* All done,return TRUE */` |
|        9 | 12610 | `	ph7_result_bool(pCtx,1);` |
|        9 | 12611 | `	return PH7_OK;` |
|        5 | 12612 |  |
|        - | 12613 | `/*` |
|        - | 12614 | ` * bool xml_set_unparsed_entity_decl_handler(resource $parser,callback $handler)` |
|        - | 12615 | ` *  Set up unparsed entity declaration handler.` |
|        - | 12616 | ` * Parameters` |
|        - | 12617 | ` * $parser` |
|        - | 12618 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12619 | ` * $handler` |
|        - | 12620 | ` *  handler is a string containing the name of the callback.` |
|        - | 12621 | ` *  The function named by handler must accept six parameters:` |
|        - | 12622 | ` *  handler(resource $parser,string $entity_name,string $base,string $system_id,string $public_id,string $notation_name)` |
|        - | 12623 | ` *  $parser` |
|        - | 12624 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12625 | ` *  $entity_name` |
|        - | 12626 | ` *   The name of the entity that is about to be defined.` |
|        - | 12627 | ` *  $base` |
|        - | 12628 | ` *   This is the base for resolving the system identifier (systemId) of the external entity.` |
|        - | 12629 | ` *   Currently this parameter will always be set to an empty string.` |
|        - | 12630 | ` *  $system_id` |
|        - | 12631 | ` *   System identifier for the external entity.` |
|        - | 12632 | ` *  $public_id` |
|        - | 12633 | ` *    Public identifier for the external entity.` |
|        - | 12634 | ` *  $notation_name` |
|        - | 12635 | ` *    Name of the notation of this entity (see xml_set_notation_decl_handler()).` |
|        - | 12636 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12637 | ` *   can also be supplied.` |
|        - | 12638 | ` * Return` |
|        - | 12639 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12640 | ` */` |
|        2 | 12641 | `static int vm_builtin_xml_set_unparsed_entity_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12642 |  |
|        - | 12643 | `	ph7_xml_engine *pEngine;` |
|        3 | 12644 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12645 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12646 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12647 | `		return PH7_OK;` |
|        - | 12648 | `	}` |
|        - | 12649 | `	/* Point to the XML engine */` |
|        3 | 12650 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12651 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12652 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12653 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12654 | `		return PH7_OK;` |
|        - | 12655 | `	}` |
|        3 | 12656 | `	if( nArg > 1 ){` |
|        - | 12657 | `		/* Save the user callback for later invocation */` |
|        3 | 12658 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_UNPED]);` |
|        1 | 12659 | `	}` |
|        - | 12660 | `	/* All done,return TRUE */` |
|        3 | 12661 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12662 | `	return PH7_OK;` |
|        2 | 12663 |  |
|        - | 12664 | `/*` |
|        - | 12665 | ` * bool xml_set_notation_decl_handler(resource $parser,callback $handler)` |
|        - | 12666 | ` *  Set up notation declaration handler.` |
|        - | 12667 | ` * Parameters` |
|        - | 12668 | ` * $parser` |
|        - | 12669 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12670 | ` * $handler` |
|        - | 12671 | ` *  handler is a string containing the name of the callback.` |
|        - | 12672 | ` *  The function named by handler must accept five parameters:` |
|        - | 12673 | ` *  handler(resource $parser,string $entity_name,string $base,string $system_id,string $public_id)` |
|        - | 12674 | ` *  $parser` |
|        - | 12675 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12676 | ` *  $entity_name` |
|        - | 12677 | ` *   The name of the entity that is about to be defined.` |
|        - | 12678 | ` *  $base` |
|        - | 12679 | ` *   This is the base for resolving the system identifier (systemId) of the external entity.` |
|        - | 12680 | ` *   Currently this parameter will always be set to an empty string.` |
|        - | 12681 | ` *  $system_id` |
|        - | 12682 | ` *   System identifier for the external entity.` |
|        - | 12683 | ` *  $public_id` |
|        - | 12684 | ` *    Public identifier for the external entity.` |
|        - | 12685 | ` *  Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12686 | ` *  can also be supplied.` |
|        - | 12687 | ` * Return` |
|        - | 12688 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12689 | ` */` |
|        2 | 12690 | `static int vm_builtin_xml_set_notation_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12691 |  |
|        - | 12692 | `	ph7_xml_engine *pEngine;` |
|        3 | 12693 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12694 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12695 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12696 | `		return PH7_OK;` |
|        - | 12697 | `	}` |
|        - | 12698 | `	/* Point to the XML engine */` |
|        3 | 12699 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12700 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12701 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12702 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12703 | `		return PH7_OK;` |
|        - | 12704 | `	}` |
|        3 | 12705 | `	if( nArg > 1 ){` |
|        - | 12706 | `		/* Save the user callback for later invocation */` |
|        3 | 12707 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_ND]);` |
|        1 | 12708 | `	}` |
|        - | 12709 | `	/* All done,return TRUE */` |
|        3 | 12710 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12711 | `	return PH7_OK;` |
|        2 | 12712 |  |
|        - | 12713 | `/*` |
|        - | 12714 | ` * bool xml_set_external_entity_ref_handler(resource $parser,callback $handler)` |
|        - | 12715 | ` *  Set up external entity reference handler.` |
|        - | 12716 | ` * Parameters` |
|        - | 12717 | ` * $parser` |
|        - | 12718 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12719 | ` * $handler` |
|        - | 12720 | ` *  handler is a string containing the name of the callback.` |
|        - | 12721 | ` *  The function named by handler must accept five parameters:` |
|        - | 12722 | ` *   handler(resource $parser,string $open_entity_names,string $base,string $system_id,string $public_id)` |
|        - | 12723 | ` *  $parser` |
|        - | 12724 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12725 | ` *  $open_entity_names` |
|        - | 12726 | ` *   The second parameter, open_entity_names, is a space-separated list of the names` |
|        - | 12727 | ` *   of the entities that are open for the parse of this entity (including the name of the referenced entity).` |
|        - | 12728 | ` *  $base` |
|        - | 12729 | ` *   This is the base for resolving the system identifier (system_id) of the external entity.` |
|        - | 12730 | ` *   Currently this parameter will always be set to an empty string.` |
|        - | 12731 | ` *  $system_id` |
|        - | 12732 | ` *   The fourth parameter, system_id, is the system identifier as specified in the entity declaration.` |
|        - | 12733 | ` *  $public_id` |
|        - | 12734 | ` *   The fifth parameter, public_id, is the public identifier as specified in the entity declaration` |
|        - | 12735 | ` *   or an empty string if none was specified; the whitespace in the public identifier will have been` |
|        - | 12736 | ` *   normalized as required by the XML spec.` |
|        - | 12737 | ` * Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12738 | ` * can also be supplied.` |
|        - | 12739 | ` * Return` |
|        - | 12740 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12741 | ` */` |
|        2 | 12742 | `static int vm_builtin_xml_set_external_entity_ref_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12743 |  |
|        - | 12744 | `	ph7_xml_engine *pEngine;` |
|        3 | 12745 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12746 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12747 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12748 | `		return PH7_OK;` |
|        - | 12749 | `	}` |
|        - | 12750 | `	/* Point to the XML engine */` |
|        3 | 12751 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12752 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12753 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12754 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12755 | `		return PH7_OK;` |
|        - | 12756 | `	}` |
|        3 | 12757 | `	if( nArg > 1 ){` |
|        - | 12758 | `		/* Save the user callback for later invocation */` |
|        3 | 12759 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_EER]);` |
|        1 | 12760 | `	}` |
|        - | 12761 | `	/* All done,return TRUE */` |
|        3 | 12762 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12763 | `	return PH7_OK;` |
|        2 | 12764 |  |
|        - | 12765 | `/*` |
|        - | 12766 | ` * int xml_get_current_line_number(resource $parser)` |
|        - | 12767 | ` *  Gets the current line number for the given XML parser.` |
|        - | 12768 | ` * Parameters` |
|        - | 12769 | ` * $parser` |
|        - | 12770 | ` *   A reference to the XML parser.` |
|        - | 12771 | ` * Return` |
|        - | 12772 | ` *  This function returns FALSE if parser does not refer` |
|        - | 12773 | ` *  to a valid parser, or else it returns which line the parser` |
|        - | 12774 | ` *  is currently at in its data buffer.` |
|        - | 12775 | ` */` |
|        8 | 12776 | `static int vm_builtin_xml_get_current_line_number(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12777 |  |
|        - | 12778 | `	ph7_xml_engine *pEngine;` |
|        9 | 12779 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12780 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12781 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12782 | `		return PH7_OK;` |
|        - | 12783 | `	}` |
|        - | 12784 | `	/* Point to the XML engine */` |
|        9 | 12785 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        9 | 12786 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12787 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12788 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12789 | `		return PH7_OK;` |
|        - | 12790 | `	}` |
|        - | 12791 | `	/* Return the line number */` |
|        9 | 12792 | `	ph7_result_int(pCtx,(int)pEngine->nLine);` |
|        9 | 12793 | `	return PH7_OK;` |
|        5 | 12794 |  |
|        - | 12795 | `/*` |
|        - | 12796 | ` * int xml_get_current_byte_index(resource $parser)` |
|        - | 12797 | ` *  Gets the current byte index of the given XML parser.` |
|        - | 12798 | ` * Parameters` |
|        - | 12799 | ` * $parser` |
|        - | 12800 | ` *   A reference to the XML parser.` |
|        - | 12801 | ` * Return` |
|        - | 12802 | ` *  This function returns FALSE if parser does not refer to a valid` |
|        - | 12803 | ` *  parser, or else it returns which byte index the parser is currently` |
|        - | 12804 | ` *  at in its data buffer (starting at 0).` |
|        - | 12805 | ` */` |
|        4 | 12806 | `static int vm_builtin_xml_get_current_byte_index(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12807 |  |
|        - | 12808 | `	ph7_xml_engine *pEngine;` |
|        - | 12809 | `	SyStream *pStream;` |
|        - | 12810 | `	SyToken *pToken;` |
|        5 | 12811 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12812 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12813 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12814 | `		return PH7_OK;` |
|        - | 12815 | `	}` |
|        - | 12816 | `	/* Point to the XML engine */` |
|        5 | 12817 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        5 | 12818 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12819 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12820 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12821 | `		return PH7_OK;` |
|        - | 12822 | `	}` |
|        - | 12823 | `	/* Point to the current processed token */` |
|        5 | 12824 | `	pToken = (SyToken *)SySetPeekCurrentEntry(&pEngine->sParser.sToken);` |
|        5 | 12825 | `	if( pToken == 0 ){` |
|        - | 12826 | `		/* Stream not yet processed */` |
|        3 | 12827 | `		ph7_result_int(pCtx,0);` |
|        3 | 12828 | `		return 0;` |
|        - | 12829 | `	}` |
|        - | 12830 | `	/* Point to the input stream */` |
|        3 | 12831 | `	pStream = &pEngine->sParser.sLex.sStream;` |
|        - | 12832 | `	/* Return the byte index */` |
|        3 | 12833 | `	ph7_result_int64(pCtx,(ph7_int64)(pToken->sData.zString-(const char *)pStream->zInput));` |
|        3 | 12834 | `	return PH7_OK;` |
|        3 | 12835 |  |
|        - | 12836 | `/*` |
|        - | 12837 | ` * bool xml_set_object(resource $parser,object &$object)` |
|        - | 12838 | ` *  Use XML Parser within an object.` |
|        - | 12839 | ` * NOTE` |
|        - | 12840 | ` *  This function is depreceated and is a no-op.` |
|        - | 12841 | ` * Parameters` |
|        - | 12842 | ` * $parser` |
|        - | 12843 | ` *   A reference to the XML parser.` |
|        - | 12844 | ` * $object` |
|        - | 12845 | ` *  The object where to use the XML parser.` |
|        - | 12846 | ` * Return` |
|        - | 12847 | ` * Always FALSE.` |
|        - | 12848 | ` */` |
|        2 | 12849 | `static int vm_builtin_xml_set_object(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12850 |  |
|        - | 12851 | `	ph7_xml_engine *pEngine;` |
|        3 | 12852 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_object(apArg[1]) ){` |
|        - | 12853 | `		/* Missing/Ivalid argument,return FALSE */` |
|        3 | 12854 | `		ph7_result_bool(pCtx,0);` |
|        3 | 12855 | `		return PH7_OK;` |
|        - | 12856 | `	}` |
|        - | 12857 | `	/* Point to the XML engine */` |
|      ! 0 | 12858 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|      ! 0 | 12859 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12860 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12861 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12862 | `		return PH7_OK;` |
|        - | 12863 | `	}` |
|        - | 12864 | `	/*  Throw a notice and return */` |
|      ! 0 | 12865 | `	ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"This function is depreceated and is a no-op."` |
|        - | 12866 | `		"In order to mimic this behaviour,you can supply instead of a function name an array "` |
|        - | 12867 | `		"containing an object reference and a method name."` |
|        - | 12868 | `		);` |
|        - | 12869 | `	/* Return FALSE */` |
|      ! 0 | 12870 | `	ph7_result_bool(pCtx,0);` |
|      ! 0 | 12871 | `	return PH7_OK;` |
|        2 | 12872 |  |
|        - | 12873 | `/*` |
|        - | 12874 | ` * int xml_get_current_column_number(resource $parser)` |
|        - | 12875 | ` *  Gets the current column number of the given XML parser.` |
|        - | 12876 | ` * Parameters` |
|        - | 12877 | ` * $parser` |
|        - | 12878 | ` *   A reference to the XML parser.` |
|        - | 12879 | ` * Return` |
|        - | 12880 | ` *  This function returns FALSE if parser does not refer to a valid parser, or else it returns` |
|        - | 12881 | ` *  which column on the current line (as given by xml_get_current_line_number()) the parser` |
|        - | 12882 | ` *  is currently at.` |
|        - | 12883 | ` */` |
|        4 | 12884 | `static int vm_builtin_xml_get_current_column_number(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12885 |  |
|        - | 12886 | `	ph7_xml_engine *pEngine;` |
|        - | 12887 | `	SyStream *pStream;` |
|        - | 12888 | `	SyToken *pToken;` |
|        5 | 12889 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12890 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12891 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12892 | `		return PH7_OK;` |
|        - | 12893 | `	}` |
|        - | 12894 | `	/* Point to the XML engine */` |
|        5 | 12895 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        5 | 12896 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12897 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12898 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12899 | `		return PH7_OK;` |
|        - | 12900 | `	}` |
|        - | 12901 | `	/* Point to the current processed token */` |
|        5 | 12902 | `	pToken = (SyToken *)SySetPeekCurrentEntry(&pEngine->sParser.sToken);` |
|        5 | 12903 | `	if( pToken == 0 ){` |
|        - | 12904 | `		/* Stream not yet processed */` |
|      ! 0 | 12905 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 12906 | `		return 0;` |
|        - | 12907 | `	}` |
|        - | 12908 | `	/* Point to the input stream */` |
|        5 | 12909 | `	pStream = &pEngine->sParser.sLex.sStream;` |
|        - | 12910 | `	/* Return the byte index */` |
|        5 | 12911 | `	ph7_result_int64(pCtx,(ph7_int64)(pToken->sData.zString-(const char *)pStream->zInput)/80);` |
|        5 | 12912 | `	return PH7_OK;` |
|        3 | 12913 |  |
|        - | 12914 | `/*` |
|        - | 12915 | ` * int xml_get_error_code(resource $parser)` |
|        - | 12916 | ` *  Get XML parser error code.` |
|        - | 12917 | ` * Parameters` |
|        - | 12918 | ` * $parser` |
|        - | 12919 | ` *   A reference to the XML parser.` |
|        - | 12920 | ` * Return` |
|        - | 12921 | ` *  This function returns FALSE if parser does not refer to a valid` |
|        - | 12922 | ` *  parser, or else it returns one of the error codes listed in the error` |
|        - | 12923 | ` *  codes section.` |
|        - | 12924 | ` */` |
|       32 | 12925 | `static int vm_builtin_xml_get_error_code(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12926 |  |
|        - | 12927 | `	ph7_xml_engine *pEngine;` |
|       33 | 12928 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12929 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12930 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12931 | `		return PH7_OK;` |
|        - | 12932 | `	}` |
|        - | 12933 | `	/* Point to the XML engine */` |
|       33 | 12934 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|       33 | 12935 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12936 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12937 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12938 | `		return PH7_OK;` |
|        - | 12939 | `	}` |
|        - | 12940 | `	/* Return the error code if any */` |
|       33 | 12941 | `	ph7_result_int(pCtx,pEngine->iErrCode);` |
|       33 | 12942 | `	return PH7_OK;` |
|       17 | 12943 |  |
|        - | 12944 | `/*` |
|        - | 12945 | ` * XML parser event callbacks` |
|        - | 12946 | ` * Each time the unserlying XML parser extract a single token` |
|        - | 12947 | ` * from the input,one of the following callbacks are invoked.` |
|        - | 12948 | ` * IMP-XML-ENGINE-07-07-2012 22:02 FreeBSD [chm@symisc.net]` |
|        - | 12949 | ` */` |
|        - | 12950 | `/*` |
|        - | 12951 | ` * Create a scalar ph7_value holding the value` |
|        - | 12952 | ` * of an XML tag/attribute/CDATA and so on.` |
|        - | 12953 | ` */` |
|      148 | 12954 | `static ph7_value * VmXMLValue(ph7_xml_engine *pEngine,SyXMLRawStr *pXML,SyXMLRawStr *pNsUri)` |
|        1 | 12955 |  |
|        - | 12956 | `	ph7_value *pValue;` |
|        - | 12957 | `	/* Allocate a new scalar variable */` |
|      149 | 12958 | `	pValue = ph7_context_new_scalar(pEngine->pCtx);` |
|      149 | 12959 | `	if( pValue == 0 ){` |
|      ! 0 | 12960 | `		ph7_context_throw_error(pEngine->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 12961 | `		return 0;` |
|        - | 12962 | `	}` |
|      149 | 12963 | `	if( pNsUri && pNsUri->nByte > 0 ){` |
|        - | 12964 | `		/* Append namespace URI and the separator */` |
|        9 | 12965 | `		ph7_value_string_format(pValue,"%.*s%c",pNsUri->nByte,pNsUri->zString,pEngine->ns_sep);` |
|        4 | 12966 | `	}` |
|        - | 12967 | `	/* Copy the tag value */` |
|      149 | 12968 | `	ph7_value_string(pValue,pXML->zString,(int)pXML->nByte);` |
|      149 | 12969 | `	return pValue;` |
|       75 | 12970 |  |
|        - | 12971 | `/*` |
|        - | 12972 | ` * Create a 'ph7_value' of type array holding the values` |
|        - | 12973 | ` * of an XML tag attributes.` |
|        - | 12974 | ` */` |
|       62 | 12975 | `static ph7_value * VmXMLAttrValue(ph7_xml_engine *pEngine,SyXMLRawStr *aAttr,sxu32 nAttr)` |
|        1 | 12976 |  |
|        - | 12977 | `	ph7_value *pArray;` |
|        - | 12978 | `	/* Create an empty array */` |
|       63 | 12979 | `	pArray = ph7_context_new_array(pEngine->pCtx);` |
|       63 | 12980 | `	if( pArray == 0 ){` |
|      ! 0 | 12981 | `		ph7_context_throw_error(pEngine->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 12982 | `		return 0;` |
|        - | 12983 | `	}` |
|       63 | 12984 | `	if( nAttr > 0 ){` |
|        - | 12985 | `		ph7_value *pKey,*pValue;` |
|        - | 12986 | `		sxu32 n;` |
|        - | 12987 | `		/* Create worker variables */` |
|        5 | 12988 | `		pKey = ph7_context_new_scalar(pEngine->pCtx);` |
|        5 | 12989 | `		pValue = ph7_context_new_scalar(pEngine->pCtx);` |
|        5 | 12990 | `		if( pKey == 0 \|\| pValue == 0 ){` |
|      ! 0 | 12991 | `			ph7_context_throw_error(pEngine->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 12992 | `			return 0;` |
|        - | 12993 | `		}` |
|        - | 12994 | `		/* Copy attributes */` |
|        9 | 12995 | `		for( n = 0 ; n < nAttr ; n += 2 ){` |
|        - | 12996 | `			/* Reset string cursors */` |
|        5 | 12997 | `			ph7_value_reset_string_cursor(pKey);` |
|        5 | 12998 | `			ph7_value_reset_string_cursor(pValue);` |
|        - | 12999 | `			/* Copy attribute name and it's associated value */` |
|        5 | 13000 | `			ph7_value_string(pKey,aAttr[n].zString,(int)aAttr[n].nByte); /* Attribute name */` |
|        5 | 13001 | `			ph7_value_string(pValue,aAttr[n+1].zString,(int)aAttr[n+1].nByte); /* Attribute value */` |
|        - | 13002 | `			/* Insert in the array */` |
|        5 | 13003 | `			ph7_array_add_elem(pArray,pKey,pValue); /* Will make it's own copy */` |
|        3 | 13004 | `		}` |
|        - | 13005 | `		/* Release the worker variables */` |
|        5 | 13006 | `		ph7_context_release_value(pEngine->pCtx,pKey);` |
|        5 | 13007 | `		ph7_context_release_value(pEngine->pCtx,pValue);` |
|        2 | 13008 | `	}` |
|        - | 13009 | `	/* Return the freshly created array */` |
|       63 | 13010 | `	return pArray;` |
|       32 | 13011 |  |
|        - | 13012 | `/*` |
|        - | 13013 | ` * Start element handler.` |
|        - | 13014 | ` * The user defined callback must accept three parameters:` |
|        - | 13015 | ` *    start_element_handler(resource $parser,string $name,array $attribs )` |
|        - | 13016 | ` *    $parser` |
|        - | 13017 | ` *      The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13018 | ` *    $name` |
|        - | 13019 | ` *      The second parameter, name, contains the name of the element for which this handler` |
|        - | 13020 | ` *		is called.If case-folding is in effect for this parser, the element name will be in uppercase letters.` |
|        - | 13021 | ` *    $attribs` |
|        - | 13022 | ` *      The third parameter, attribs, contains an associative array with the element's attributes (if any).` |
|        - | 13023 | ` *		The keys of this array are the attribute names, the values are the attribute values.` |
|        - | 13024 | ` *      Attribute names are case-folded on the same criteria as element names.Attribute values are not case-folded.` |
|        - | 13025 | ` *      The original order of the attributes can be retrieved by walking through attribs the normal way, using each().` |
|        - | 13026 | ` *      The first key in the array was the first attribute, and so on.` |
|        - | 13027 | ` *      Note: Instead of a function name, an array containing an object reference and a method name can also be supplied.` |
|        - | 13028 | ` */` |
|       78 | 13029 | `static sxi32 VmXMLStartElementHandler(SyXMLRawStr *pStart,SyXMLRawStr *pNS,sxu32 nAttr,SyXMLRawStr *aAttr,void *pUserData)` |
|        1 | 13030 |  |
|       79 | 13031 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13032 | `	ph7_value *pCallback,*pTag,*pAttr;` |
|        - | 13033 | `	/* Point to the target user defined callback */` |
|       79 | 13034 | `	pCallback = &pEngine->aCB[PH7_XML_START_TAG];` |
|        - | 13035 | `	/* Make sure the given callback is callable */` |
|       79 | 13036 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13037 | `		/* Not callable,return immediately*/` |
|       17 | 13038 | `		return SXRET_OK;` |
|        - | 13039 | `	}` |
|        - | 13040 | `	/* Create a ph7_value holding the tag name */` |
|       63 | 13041 | `	pTag = VmXMLValue(pEngine,pStart,pNS);` |
|        - | 13042 | `	/* Create a ph7_value holding the tag attributes */` |
|       63 | 13043 | `	pAttr = VmXMLAttrValue(pEngine,aAttr,nAttr);` |
|       63 | 13044 | `	if( pTag == 0  \|\| pAttr == 0 ){` |
|      ! 0 | 13045 | `		SXUNUSED(pNS); /* cc warning */` |
|        - | 13046 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13047 | `		return SXRET_OK;` |
|        - | 13048 | `	}` |
|        - | 13049 | `	/* Invoke the user callback */` |
|       63 | 13050 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pTag,pAttr,(ph7_value*)0);` |
|        - | 13051 | `	/* Clean-up the mess left behind */` |
|       63 | 13052 | `	ph7_context_release_value(pEngine->pCtx,pTag);` |
|       63 | 13053 | `	ph7_context_release_value(pEngine->pCtx,pAttr);` |
|       63 | 13054 | `	return SXRET_OK;` |
|       40 | 13055 |  |
|        - | 13056 | `/*` |
|        - | 13057 | ` * End element handler.` |
|        - | 13058 | ` * The user defined callback must accept two parameters:` |
|        - | 13059 | ` *  end_element_handler(resource $parser,string $name)` |
|        - | 13060 | ` *  $parser` |
|        - | 13061 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13062 | ` *  $name` |
|        - | 13063 | ` *   The second parameter, name, contains the name of the element for which this handler is called.` |
|        - | 13064 | ` *   If case-folding is in effect for this parser, the element name will be in uppercase letters.` |
|        - | 13065 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 13066 | ` *   can also be supplied.` |
|        - | 13067 | ` */` |
|       62 | 13068 | `static sxi32 VmXMLEndElementHandler(SyXMLRawStr *pEnd,SyXMLRawStr *pNS,void *pUserData)` |
|        1 | 13069 |  |
|       63 | 13070 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13071 | `	ph7_value *pCallback,*pTag;` |
|        - | 13072 | `	/* Point to the target user defined callback */` |
|       63 | 13073 | `	pCallback = &pEngine->aCB[PH7_XML_END_TAG];` |
|        - | 13074 | `	/* Make sure the given callback is callable */` |
|       63 | 13075 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13076 | `		/* Not callable,return immediately*/` |
|        9 | 13077 | `		return SXRET_OK;` |
|        - | 13078 | `	}` |
|        - | 13079 | `	/* Create a ph7_value holding the tag name */` |
|       55 | 13080 | `	pTag = VmXMLValue(pEngine,pEnd,pNS);` |
|       55 | 13081 | `	if( pTag == 0  ){` |
|      ! 0 | 13082 | `		SXUNUSED(pNS); /* cc warning */` |
|        - | 13083 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13084 | `		return SXRET_OK;` |
|        - | 13085 | `	}` |
|        - | 13086 | `	/* Invoke the user callback */` |
|       55 | 13087 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pTag,(ph7_value*)0);` |
|        - | 13088 | `	/* Clean-up the mess left behind */` |
|       55 | 13089 | `	ph7_context_release_value(pEngine->pCtx,pTag);` |
|       55 | 13090 | `	return SXRET_OK;` |
|       32 | 13091 |  |
|        - | 13092 | `/*` |
|        - | 13093 | ` * Character data handler.` |
|        - | 13094 | ` *  The user defined callback must accept two parameters:` |
|        - | 13095 | ` *  handler(resource $parser,string $data)` |
|        - | 13096 | ` *  $parser` |
|        - | 13097 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13098 | ` *  $data` |
|        - | 13099 | ` *   The second parameter, data, contains the character data as a string.` |
|        - | 13100 | ` *   Character data handler is called for every piece of a text in the XML document.` |
|        - | 13101 | ` *   It can be called multiple times inside each fragment (e.g. for non-ASCII strings).` |
|        - | 13102 | ` *   If a handler function is set to an empty string, or FALSE, the handler in question is disabled.` |
|        - | 13103 | ` *   Note: Instead of a function name, an array containing an object reference and a method name can also be supplied.` |
|        - | 13104 | ` */` |
|       28 | 13105 | `static sxi32 VmXMLTextHandler(SyXMLRawStr *pText,void *pUserData)` |
|        1 | 13106 |  |
|       29 | 13107 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13108 | `	ph7_value *pCallback,*pData;` |
|        - | 13109 | `	/* Point to the target user defined callback */` |
|       29 | 13110 | `	pCallback = &pEngine->aCB[PH7_XML_CDATA];` |
|        - | 13111 | `	/* Make sure the given callback is callable */` |
|       29 | 13112 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13113 | `		/* Not callable,return immediately*/` |
|       11 | 13114 | `		return SXRET_OK;` |
|        - | 13115 | `	}` |
|        - | 13116 | `	/* Create a ph7_value holding the data */` |
|       19 | 13117 | `	pData = VmXMLValue(pEngine,&(*pText),0);` |
|       19 | 13118 | `	if( pData == 0  ){` |
|        - | 13119 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13120 | `		return SXRET_OK;` |
|        - | 13121 | `	}` |
|        - | 13122 | `	/* Invoke the user callback */` |
|       19 | 13123 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pData,(ph7_value*)0);` |
|        - | 13124 | `	/* Clean-up the mess left behind */` |
|       19 | 13125 | `	ph7_context_release_value(pEngine->pCtx,pData);` |
|       19 | 13126 | `	return SXRET_OK;` |
|       15 | 13127 |  |
|        - | 13128 | `/*` |
|        - | 13129 | ` * Processing instruction (PI) handler.` |
|        - | 13130 | ` * The user defined callback must accept two parameters:` |
|        - | 13131 | ` *   handler(resource $parser,string $target,string $data)` |
|        - | 13132 | ` *  $parser` |
|        - | 13133 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13134 | ` *  $target` |
|        - | 13135 | ` *   The second parameter, target, contains the PI target.` |
|        - | 13136 | ` *  $data` |
|        - | 13137 | ` *    The third parameter, data, contains the PI data.` |
|        - | 13138 | ` *    Note: Instead of a function name, an array containing an object reference` |
|        - | 13139 | ` *    and a method name can also be supplied.` |
|        - | 13140 | ` */` |
|        8 | 13141 | `static sxi32 VmXMLPIHandler(SyXMLRawStr *pTargetStr,SyXMLRawStr *pDataStr,void *pUserData)` |
|        1 | 13142 |  |
|        9 | 13143 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13144 | `	ph7_value *pCallback,*pTarget,*pData;` |
|        - | 13145 | `	/* Point to the target user defined callback */` |
|        9 | 13146 | `	pCallback = &pEngine->aCB[PH7_XML_PI];` |
|        - | 13147 | `	/* Make sure the given callback is callable */` |
|        9 | 13148 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13149 | `		/* Not callable,return immediately*/` |
|        5 | 13150 | `		return SXRET_OK;` |
|        - | 13151 | `	}` |
|        - | 13152 | `	/* Get a ph7_value holding the data */` |
|        5 | 13153 | `	pTarget = VmXMLValue(pEngine,&(*pTargetStr),0);` |
|        5 | 13154 | `	pData = VmXMLValue(pEngine,&(*pDataStr),0);` |
|        5 | 13155 | `	if( pTarget == 0 \|\| pData == 0  ){` |
|        - | 13156 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13157 | `		return SXRET_OK;` |
|        - | 13158 | `	}` |
|        - | 13159 | `	/* Invoke the user callback */` |
|        5 | 13160 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pTarget,pData,(ph7_value*)0);` |
|        - | 13161 | `	/* Clean-up the mess left behind */` |
|        5 | 13162 | `	ph7_context_release_value(pEngine->pCtx,pTarget);` |
|        5 | 13163 | `	ph7_context_release_value(pEngine->pCtx,pData);` |
|        5 | 13164 | `	return SXRET_OK;` |
|        5 | 13165 |  |
|        - | 13166 | `/*` |
|        - | 13167 | ` * Namespace declaration handler.` |
|        - | 13168 | ` * The user defined callback must accept two parameters:` |
|        - | 13169 | ` *    handler(resource $parser,string $prefix,string $uri)` |
|        - | 13170 | ` * $parser` |
|        - | 13171 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13172 | ` * $prefix` |
|        - | 13173 | ` *   The prefix is a string used to reference the namespace within an XML object.` |
|        - | 13174 | ` * $uri` |
|        - | 13175 | ` *   Uniform Resource Identifier (URI) of namespace.` |
|        - | 13176 | ` *   Note: Instead of a function name, an array containing an object reference` |
|        - | 13177 | ` *   and a method name can also be supplied.` |
|        - | 13178 | ` */` |
|        4 | 13179 | `static sxi32 VmXMLNSStartHandler(SyXMLRawStr *pUriStr,SyXMLRawStr *pPrefixStr,void *pUserData)` |
|        1 | 13180 |  |
|        5 | 13181 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13182 | `	ph7_value *pCallback,*pUri,*pPrefix;` |
|        - | 13183 | `	/* Point to the target user defined callback */` |
|        5 | 13184 | `	pCallback = &pEngine->aCB[PH7_XML_NS_START];` |
|        - | 13185 | `	/* Make sure the given callback is callable */` |
|        5 | 13186 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13187 | `		/* Not callable,return immediately*/` |
|        3 | 13188 | `		return SXRET_OK;` |
|        - | 13189 | `	}` |
|        - | 13190 | `	/* Get a ph7_value holding the PREFIX/URI */` |
|        3 | 13191 | `	pUri = VmXMLValue(pEngine,pUriStr,0);` |
|        3 | 13192 | `	pPrefix = VmXMLValue(pEngine,pPrefixStr,0);` |
|        3 | 13193 | `	if( pUri == 0 \|\| pPrefix == 0  ){` |
|        - | 13194 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13195 | `		return SXRET_OK;` |
|        - | 13196 | `	}` |
|        - | 13197 | `	/* Invoke the user callback */` |
|        3 | 13198 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pUri,pPrefix,(ph7_value*)0);` |
|        - | 13199 | `	/* Clean-up the mess left behind */` |
|        3 | 13200 | `	ph7_context_release_value(pEngine->pCtx,pUri);` |
|        3 | 13201 | `	ph7_context_release_value(pEngine->pCtx,pPrefix);` |
|        3 | 13202 | `	return SXRET_OK;` |
|        3 | 13203 |  |
|        - | 13204 | `/*` |
|        - | 13205 | ` * Namespace end declaration handler.` |
|        - | 13206 | ` * The user defined callback must accept two parameters:` |
|        - | 13207 | ` *    handler(resource $parser,string $prefix)` |
|        - | 13208 | ` * $parser` |
|        - | 13209 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13210 | ` * $prefix` |
|        - | 13211 | ` *  The prefix is a string used to reference the namespace within an XML object.` |
|        - | 13212 | ` *   Note: Instead of a function name, an array containing an object reference` |
|        - | 13213 | ` *   and a method name can also be supplied.` |
|        - | 13214 | ` */` |
|        4 | 13215 | `static sxi32 VmXMLNSEndHandler(SyXMLRawStr *pPrefixStr,void *pUserData)` |
|        1 | 13216 |  |
|        5 | 13217 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13218 | `	ph7_value *pCallback,*pPrefix;` |
|        - | 13219 | `	/* Point to the target user defined callback */` |
|        5 | 13220 | `	pCallback = &pEngine->aCB[PH7_XML_NS_END];` |
|        - | 13221 | `	/* Make sure the given callback is callable */` |
|        5 | 13222 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13223 | `		/* Not callable,return immediately*/` |
|        3 | 13224 | `		return SXRET_OK;` |
|        - | 13225 | `	}` |
|        - | 13226 | `	/* Get a ph7_value holding the prefix */` |
|        3 | 13227 | `	pPrefix = VmXMLValue(pEngine,pPrefixStr,0);` |
|        3 | 13228 | `	if( pPrefix == 0 ){` |
|        - | 13229 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13230 | `		return SXRET_OK;` |
|        - | 13231 | `	}` |
|        - | 13232 | `	/* Invoke the user callback */` |
|        3 | 13233 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pPrefix,(ph7_value*)0);` |
|        - | 13234 | `	/* Clean-up the mess left behind */` |
|        3 | 13235 | `	ph7_context_release_value(pEngine->pCtx,pPrefix);` |
|        3 | 13236 | `	return SXRET_OK;` |
|        3 | 13237 |  |
|        - | 13238 | `/*` |
|        - | 13239 | ` * Error Message consumer handler.` |
|        - | 13240 | ` * Each time the XML parser encounter a syntaxt error or any other error` |
|        - | 13241 | ` * related to XML processing,the following callback is invoked by the` |
|        - | 13242 | ` * underlying XML parser.` |
|        - | 13243 | ` */` |
|       34 | 13244 | `static sxi32 VmXMLErrorHandler(const char *zMessage,sxi32 iErrCode,SyToken *pToken,void *pUserData)` |
|        1 | 13245 |  |
|       35 | 13246 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13247 | `	/* Save the error code */` |
|       35 | 13248 | `	pEngine->iErrCode = iErrCode;` |
|       17 | 13249 | `	SXUNUSED(zMessage); /* cc warning */` |
|       35 | 13250 | `	if( pToken ){` |
|       35 | 13251 | `		pEngine->nLine = pToken->nLine;` |
|       17 | 13252 | `	}` |
|        - | 13253 | `	/* Abort XML processing immediately */` |
|       35 | 13254 | `	return SXERR_ABORT;` |
|        1 | 13255 |  |
|        - | 13256 | `/*` |
|        - | 13257 | ` * int xml_parse(resource $parser,string $data[,bool $is_final = false ])` |
|        - | 13258 | ` *  Parses an XML document. The handlers for the configured events are called` |
|        - | 13259 | ` *  as many times as necessary.` |
|        - | 13260 | ` * Parameters` |
|        - | 13261 | ` *  $parser` |
|        - | 13262 | ` *   A reference to the XML parser.` |
|        - | 13263 | ` *  $data` |
|        - | 13264 | ` *   Chunk of data to parse. A document may be parsed piece-wise by calling` |
|        - | 13265 | ` *   xml_parse() several times with new data, as long as the is_final parameter` |
|        - | 13266 | ` *   is set and TRUE when the last data is parsed.` |
|        - | 13267 | ` * $is_final` |
|        - | 13268 | ` *   NOT USED. This implementation require that all the processed input be` |
|        - | 13269 | ` *   entirely loaded in memory.` |
|        - | 13270 | ` * Return` |
|        - | 13271 | ` *  Returns 1 on success or 0 on failure.` |
|        - | 13272 | ` */` |
|       74 | 13273 | `static int vm_builtin_xml_parse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13274 |  |
|        - | 13275 | `	ph7_xml_engine *pEngine;` |
|        - | 13276 | `	SyXMLParser *pParser;` |
|        - | 13277 | `	const char *zData;` |
|        - | 13278 | `	int nByte;` |
|       75 | 13279 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|        - | 13280 | `		/* Missing/Ivalid arguments,return FALSE */` |
|        3 | 13281 | `		ph7_result_bool(pCtx,0);` |
|        3 | 13282 | `		return PH7_OK;` |
|        - | 13283 | `	}` |
|        - | 13284 | `	/* Point to the XML engine */` |
|       73 | 13285 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|       73 | 13286 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13287 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13288 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13289 | `		return PH7_OK;` |
|        - | 13290 | `	}` |
|       73 | 13291 | `	if( pEngine->iNest > 0 ){` |
|        - | 13292 | `		/* This can happen when the user callback call xml_parse() again` |
|        - | 13293 | `		 * in it's body which is forbidden.` |
|        - | 13294 | `		 */` |
|      ! 0 | 13295 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,` |
|        - | 13296 | `			"Recursive call to %s,PH7 is returning false",` |
|      ! 0 | 13297 | `			ph7_function_name(pCtx)` |
|        - | 13298 | `			);` |
|        - | 13299 | `		/* Return FALSE */` |
|      ! 0 | 13300 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13301 | `		return PH7_OK;` |
|        - | 13302 | `	}` |
|       73 | 13303 | `	pEngine->pCtx = pCtx;` |
|        - | 13304 | `	/* Point to the underlying XML parser */` |
|       73 | 13305 | `	pParser = &pEngine->sParser;` |
|        - | 13306 | `	/* Register elements handler */` |
|       73 | 13307 | `	SyXMLParserSetEventHandler(pParser,pEngine,` |
|        - | 13308 | `		VmXMLStartElementHandler,` |
|        - | 13309 | `		VmXMLTextHandler,` |
|        - | 13310 | `		VmXMLErrorHandler,` |
|        - | 13311 | `		0,` |
|        - | 13312 | `		VmXMLEndElementHandler,` |
|        - | 13313 | `		VmXMLPIHandler,` |
|        - | 13314 | `		0,` |
|        - | 13315 | `		0,` |
|        - | 13316 | `		VmXMLNSStartHandler,` |
|        - | 13317 | `		VmXMLNSEndHandler` |
|        - | 13318 | `		);` |
|       73 | 13319 | `	pEngine->iErrCode = SXML_ERROR_NONE;` |
|        - | 13320 | `	/* Extract the raw XML input */` |
|       73 | 13321 | `	zData = ph7_value_to_string(apArg[1],&nByte);` |
|        - | 13322 | `	/* Start the parse process */` |
|       73 | 13323 | `	pEngine->iNest++;` |
|       73 | 13324 | `	SyXMLProcess(pParser,zData,(sxu32)nByte);` |
|       73 | 13325 | `	pEngine->iNest--;` |
|        - | 13326 | `	/* Return the parse result */` |
|       73 | 13327 | `	ph7_result_int(pCtx,pEngine->iErrCode == SXML_ERROR_NONE ? 1 : 0);` |
|       73 | 13328 | `	return PH7_OK;` |
|       38 | 13329 |  |
|        - | 13330 | `/*` |
|        - | 13331 | ` * bool xml_parser_set_option(resource $parser,int $option,mixed $value)` |
|        - | 13332 | ` *  Sets an option in an XML parser.` |
|        - | 13333 | ` * Parameters` |
|        - | 13334 | ` *  $parser` |
|        - | 13335 | ` *   A reference to the XML parser to set an option in.` |
|        - | 13336 | ` *  $option` |
|        - | 13337 | ` *    Which option to set. See below.` |
|        - | 13338 | ` *   The following options are available:` |
|        - | 13339 | ` *   XML_OPTION_CASE_FOLDING 	integer  Controls whether case-folding is enabled for this XML parser.` |
|        - | 13340 | ` *   XML_OPTION_SKIP_TAGSTART 	integer  Specify how many characters should be skipped in the beginning of a tag name.` |
|        - | 13341 | ` *   XML_OPTION_SKIP_WHITE 	    integer  Whether to skip values consisting of whitespace characters.` |
|        - | 13342 | ` *   XML_OPTION_TARGET_ENCODING string 	 Sets which target encoding to use in this XML parser.` |
|        - | 13343 | ` * $value` |
|        - | 13344 | ` *   The option's new value.` |
|        - | 13345 | ` * Return` |
|        - | 13346 | ` *  Returns 1 on success or 0 on failure.` |
|        - | 13347 | ` * Note:` |
|        - | 13348 | ` *  Well,none of these options have meaning under the built-in XML parser so a call to this` |
|        - | 13349 | ` *  function is a no-op.` |
|        - | 13350 | ` */` |
|        6 | 13351 | `static int vm_builtin_xml_parser_set_option(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13352 |  |
|        - | 13353 | `	ph7_xml_engine *pEngine;` |
|        7 | 13354 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13355 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13356 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13357 | `		return PH7_OK;` |
|        - | 13358 | `	}` |
|        - | 13359 | `	/* Point to the XML engine */` |
|        7 | 13360 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        7 | 13361 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13362 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13363 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13364 | `		return PH7_OK;` |
|        - | 13365 | `	}` |
|        - | 13366 | `	/* Always return FALSE */` |
|        7 | 13367 | `	ph7_result_bool(pCtx,0);` |
|        7 | 13368 | `	return PH7_OK;` |
|        4 | 13369 |  |
|        - | 13370 | `/*` |
|        - | 13371 | ` * mixed xml_parser_get_option(resource $parser,int $option)` |
|        - | 13372 | ` *  Get options from an XML parser.` |
|        - | 13373 | ` * Parameters` |
|        - | 13374 | ` *  $parser` |
|        - | 13375 | ` *   A reference to the XML parser to set an option in.` |
|        - | 13376 | ` * $option` |
|        - | 13377 | ` *   Which option to fetch.` |
|        - | 13378 | ` * Return` |
|        - | 13379 | ` *  This function returns FALSE if parser does not refer to a valid parser` |
|        - | 13380 | ` *  or if option isn't valid.Else the option's value is returned.` |
|        - | 13381 | ` */` |
|        2 | 13382 | `static int vm_builtin_xml_parser_get_option(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13383 |  |
|        - | 13384 | `	ph7_xml_engine *pEngine;` |
|        - | 13385 | `	int nOp;` |
|        3 | 13386 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13387 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13388 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13389 | `		return PH7_OK;` |
|        - | 13390 | `	}` |
|        - | 13391 | `	/* Point to the XML engine */` |
|        3 | 13392 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 13393 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13394 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13395 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13396 | `		return PH7_OK;` |
|        - | 13397 | `	}` |
|        - | 13398 | `	/* Extract the option */` |
|        3 | 13399 | `	nOp = ph7_value_to_int(apArg[1]);` |
|        3 | 13400 | `	switch(nOp){` |
|      ! 0 | 13401 | `	case SXML_OPTION_SKIP_TAGSTART:` |
|        - | 13402 | `	case SXML_OPTION_SKIP_WHITE:` |
|        - | 13403 | `	case SXML_OPTION_CASE_FOLDING:` |
|      ! 0 | 13404 | `		ph7_result_int(pCtx,0); break;` |
|      ! 0 | 13405 | `	case SXML_OPTION_TARGET_ENCODING:` |
|      ! 0 | 13406 | `		ph7_result_string(pCtx,"UTF-8",(int)sizeof("UTF-8")-1);` |
|      ! 0 | 13407 | `		break;` |
|        1 | 13408 | `	default:` |
|        - | 13409 | `		/* Unknown option,return FALSE*/` |
|        3 | 13410 | `		ph7_result_bool(pCtx,0);` |
|        2 | 13411 | `		break;` |
|        - | 13412 | `	}` |
|        3 | 13413 | `	return PH7_OK;` |
|        2 | 13414 |  |
|        - | 13415 | `/*` |
|        - | 13416 | ` * string xml_error_string(int $code)` |
|        - | 13417 | ` *  Gets the XML parser error string associated with the given code.` |
|        - | 13418 | ` * Parameters` |
|        - | 13419 | ` *  $code` |
|        - | 13420 | ` *   An error code from xml_get_error_code().` |
|        - | 13421 | ` * Return` |
|        - | 13422 | ` *  Returns a string with a textual description of the error` |
|        - | 13423 | ` *  code, or FALSE if no description was found.` |
|        - | 13424 | ` */` |
|       30 | 13425 | `static int vm_builtin_xml_error_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13426 |  |
|       31 | 13427 | `	int nErr = -1;` |
|       31 | 13428 | `	if( nArg > 0 ){` |
|       31 | 13429 | `		nErr = ph7_value_to_int(apArg[0]);` |
|       15 | 13430 | `	}` |
|       31 | 13431 | `	switch(nErr){` |
|        1 | 13432 | `	case SXML_ERROR_DUPLICATE_ATTRIBUTE:` |
|        3 | 13433 | `		ph7_result_string(pCtx,"Duplicate attribute",-1/*Compute length automatically*/);` |
|        3 | 13434 | `		break;` |
|      ! 0 | 13435 | `	case SXML_ERROR_INCORRECT_ENCODING:` |
|      ! 0 | 13436 | `		ph7_result_string(pCtx,"Incorrect encoding",-1);` |
|      ! 0 | 13437 | `		break;` |
|      ! 0 | 13438 | `	case SXML_ERROR_INVALID_TOKEN:` |
|      ! 0 | 13439 | `		ph7_result_string(pCtx,"Unexpected token",-1);` |
|      ! 0 | 13440 | `		break;` |
|        3 | 13441 | `	case SXML_ERROR_MISPLACED_XML_PI:` |
|        7 | 13442 | `		ph7_result_string(pCtx,"Misplaced processing instruction",-1);` |
|        7 | 13443 | `		break;` |
|      ! 0 | 13444 | `	case SXML_ERROR_NO_MEMORY:` |
|      ! 0 | 13445 | `		ph7_result_string(pCtx,"Out of memory",-1);` |
|      ! 0 | 13446 | `		break;` |
|        1 | 13447 | `	case SXML_ERROR_NONE:` |
|        3 | 13448 | `		ph7_result_string(pCtx,"Not an error",-1);` |
|        3 | 13449 | `		break;` |
|        1 | 13450 | `	case SXML_ERROR_TAG_MISMATCH:` |
|        3 | 13451 | `		ph7_result_string(pCtx,"Tag mismatch",-1);` |
|        3 | 13452 | `		break;` |
|      ! 0 | 13453 | `	case -1:` |
|      ! 0 | 13454 | `		ph7_result_string(pCtx,"Unknown error code",-1);` |
|      ! 0 | 13455 | `		break;` |
|        9 | 13456 | `	default:` |
|       19 | 13457 | `		ph7_result_string(pCtx,"Syntax error",-1);` |
|       18 | 13458 | `		break;` |
|        - | 13459 | `	}` |
|       31 | 13460 | `	return PH7_OK;` |
|        1 | 13461 |  |
|        - | 13462 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 13463 | `/*` |
|        - | 13464 | ` * int utf8_encode(string $input)` |
|        - | 13465 | ` *  UTF-8 encoding.` |
|        - | 13466 | ` *  This function encodes the string data to UTF-8, and returns the encoded version.` |
|        - | 13467 | ` *  UTF-8 is a standard mechanism used by Unicode for encoding wide character values` |
|        - | 13468 | ` * into a byte stream. UTF-8 is transparent to plain ASCII characters, is self-synchronized` |
|        - | 13469 | ` * (meaning it is possible for a program to figure out where in the bytestream characters start)` |
|        - | 13470 | ` * and can be used with normal string comparison functions for sorting and such.` |
|        - | 13471 | ` *  Notes on UTF-8 (According to SQLite3 authors):` |
|        - | 13472 | ` *  Byte-0    Byte-1    Byte-2    Byte-3    Value` |
|        - | 13473 | ` *  0xxxxxxx                                 00000000 00000000 0xxxxxxx` |
|        - | 13474 | ` *  110yyyyy  10xxxxxx                       00000000 00000yyy yyxxxxxx` |
|        - | 13475 | ` *  1110zzzz  10yyyyyy  10xxxxxx             00000000 zzzzyyyy yyxxxxxx` |
|        - | 13476 | ` *  11110uuu  10uuzzzz  10yyyyyy  10xxxxxx   000uuuuu zzzzyyyy yyxxxxxx` |
|        - | 13477 | ` * Parameters` |
|        - | 13478 | ` * $input` |
|        - | 13479 | ` *   String to encode or NULL on failure.` |
|        - | 13480 | ` * Return` |
|        - | 13481 | ` *  An UTF-8 encoded string.` |
|        - | 13482 | ` */` |
|        2 | 13483 | `static int vm_builtin_utf8_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13484 |  |
|        - | 13485 | `	const unsigned char *zIn,*zEnd;` |
|        - | 13486 | `	int nByte,c,e;` |
|        3 | 13487 | `	if( nArg < 1 ){` |
|        - | 13488 | `		/* Missing arguments,return null */` |
|      ! 0 | 13489 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13490 | `		return PH7_OK;` |
|        - | 13491 | `	}` |
|        - | 13492 | `	/* Extract the target string */` |
|        3 | 13493 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nByte);` |
|        3 | 13494 | `	if( nByte < 1 ){` |
|        - | 13495 | `		/* Empty string,return null */` |
|      ! 0 | 13496 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13497 | `		return PH7_OK;` |
|        - | 13498 | `	}` |
|        3 | 13499 | `	zEnd = &zIn[nByte];` |
|        - | 13500 | `	/* Start the encoding process */` |
|        2 | 13501 | `	for(;;){` |
|        5 | 13502 | `		if( zIn >= zEnd ){` |
|        - | 13503 | `			/* End of input */` |
|        3 | 13504 | `			break;` |
|        - | 13505 | `		}` |
|        3 | 13506 | `		c = zIn[0];` |
|        - | 13507 | `		/* Advance the stream cursor */` |
|        3 | 13508 | `		zIn++;` |
|        - | 13509 | `		/* Encode */` |
|        3 | 13510 | `		if( c<0x00080 ){` |
|      ! 0 | 13511 | `			e = (c&0xFF);` |
|      ! 0 | 13512 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|        3 | 13513 | `		}else if( c<0x00800 ){` |
|        3 | 13514 | `			e = 0xC0 + ((c>>6)&0x1F);` |
|        3 | 13515 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|        3 | 13516 | `			e = 0x80 + (c & 0x3F);` |
|        3 | 13517 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|        1 | 13518 | `		}else if( c<0x10000 ){` |
|      ! 0 | 13519 | `			e = 0xE0 + ((c>>12)&0x0F);` |
|      ! 0 | 13520 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13521 | `			e = 0x80 + ((c>>6) & 0x3F);` |
|      ! 0 | 13522 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13523 | `			e = 0x80 + (c & 0x3F);` |
|      ! 0 | 13524 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13525 | `		}else{` |
|      ! 0 | 13526 | `			e = 0xF0 + ((c>>18) & 0x07);` |
|      ! 0 | 13527 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13528 | `			e = 0x80 + ((c>>12) & 0x3F);` |
|      ! 0 | 13529 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13530 | `			e = 0x80 + ((c>>6) & 0x3F);` |
|      ! 0 | 13531 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13532 | `			e = 0x80 + (c & 0x3F);` |
|      ! 0 | 13533 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|        - | 13534 | `		}` |
|        1 | 13535 | `	}` |
|        - | 13536 | `	/* All done */` |
|        3 | 13537 | `	return PH7_OK;` |
|        2 | 13538 |  |
|        - | 13539 | `/*` |
|        - | 13540 | ` * UTF-8 decoding routine extracted from the sqlite3 source tree.` |
|        - | 13541 | ` * Original author: D. Richard Hipp (http://www.sqlite.org)` |
|        - | 13542 | ` * Status: Public Domain` |
|        - | 13543 | ` */` |
|        - | 13544 | `/*` |
|        - | 13545 | `** This lookup table is used to help decode the first byte of` |
|        - | 13546 | `** a multi-byte UTF8 character.` |
|        - | 13547 | `*/` |
|        - | 13548 | `static const unsigned char UtfTrans1[] = {` |
|        - | 13549 | `  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,` |
|        - | 13550 | `  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,` |
|        - | 13551 | `  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,` |
|        - | 13552 | `  0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,` |
|        - | 13553 | `  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,` |
|        - | 13554 | `  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,` |
|        - | 13555 | `  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,` |
|        - | 13556 | `  0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x00, 0x00,` |
|        - | 13557 | `};` |
|        - | 13558 | `/*` |
|        - | 13559 | `** Translate a single UTF-8 character.  Return the unicode value.` |
|        - | 13560 | `**` |
|        - | 13561 | `** During translation, assume that the byte that zTerm points` |
|        - | 13562 | `** is a 0x00.` |
|        - | 13563 | `**` |
|        - | 13564 | `** Write a pointer to the next unread byte back into *pzNext.` |
|        - | 13565 | `**` |
|        - | 13566 | `** Notes On Invalid UTF-8:` |
|        - | 13567 | `**` |
|        - | 13568 | `**  *  This routine never allows a 7-bit character (0x00 through 0x7f) to` |
|        - | 13569 | `**     be encoded as a multi-byte character.  Any multi-byte character that` |
|        - | 13570 | `**     attempts to encode a value between 0x00 and 0x7f is rendered as 0xfffd.` |
|        - | 13571 | `**` |
|        - | 13572 | `**  *  This routine never allows a UTF16 surrogate value to be encoded.` |
|        - | 13573 | `**     If a multi-byte character attempts to encode a value between` |
|        - | 13574 | `**     0xd800 and 0xe000 then it is rendered as 0xfffd.` |
|        - | 13575 | `**` |
|        - | 13576 | `**  *  Bytes in the range of 0x80 through 0xbf which occur as the first` |
|        - | 13577 | `**     byte of a character are interpreted as single-byte characters` |
|        - | 13578 | `**     and rendered as themselves even though they are technically` |
|        - | 13579 | `**     invalid characters.` |
|        - | 13580 | `**` |
|        - | 13581 | `**  *  This routine accepts an infinite number of different UTF8 encodings` |
|        - | 13582 | `**     for unicode values 0x80 and greater.  It do not change over-length` |
|        - | 13583 | `**     encodings to 0xfffd as some systems recommend.` |
|        - | 13584 | `*/` |
|        - | 13585 | `#define READ_UTF8(zIn, zTerm, c)                           \` |
|        - | 13586 | `  c = *(zIn++);                                            \` |
|        - | 13587 | `  if( c>=0xc0 ){                                           \` |
|        - | 13588 | `    c = UtfTrans1[c-0xc0];                                 \` |
|        - | 13589 | `    while( zIn!=zTerm && (*zIn & 0xc0)==0x80 ){            \` |
|        - | 13590 | `      c = (c<<6) + (0x3f & *(zIn++));                      \` |
|        - | 13591 | `    }                                                      \` |
|        - | 13592 | `    if( c<0x80                                             \` |
|        - | 13593 | `        \|\| (c&0xFFFFF800)==0xD800                          \` |
|        - | 13594 | `        \|\| (c&0xFFFFFFFE)==0xFFFE ){  c = 0xFFFD; }        \` |
|        - | 13595 | `  }` |
|      150 | 13596 | `PH7_PRIVATE int PH7_Utf8Read(` |
|        - | 13597 | `  const unsigned char *z,         /* First byte of UTF-8 character */` |
|        - | 13598 | `  const unsigned char *zTerm,     /* Pretend this byte is 0x00 */` |
|        - | 13599 | `  const unsigned char **pzNext    /* Write first byte past UTF-8 char here */` |
|        1 | 13600 | `){` |
|        - | 13601 | `  int c;` |
|      153 | 13602 | `  READ_UTF8(z, zTerm, c);` |
|      151 | 13603 | `  *pzNext = z;` |
|      151 | 13604 | `  return c;` |
|        1 | 13605 |  |
|        - | 13606 | `/*` |
|        - | 13607 | ` * string utf8_decode(string $data)` |
|        - | 13608 | ` *  This function decodes data, assumed to be UTF-8 encoded, to unicode.` |
|        - | 13609 | ` * Parameters` |
|        - | 13610 | ` * data` |
|        - | 13611 | ` *  An UTF-8 encoded string.` |
|        - | 13612 | ` * Return` |
|        - | 13613 | ` *  Unicode decoded string or NULL on failure.` |
|        - | 13614 | ` */` |
|        2 | 13615 | `static int vm_builtin_utf8_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13616 |  |
|        - | 13617 | `	const unsigned char *zIn,*zEnd;` |
|        - | 13618 | `	int nByte,c;` |
|        3 | 13619 | `	if( nArg < 1 ){` |
|        - | 13620 | `		/* Missing arguments,return null */` |
|      ! 0 | 13621 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13622 | `		return PH7_OK;` |
|        - | 13623 | `	}` |
|        - | 13624 | `	/* Extract the target string */` |
|        3 | 13625 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nByte);` |
|        3 | 13626 | `	if( nByte < 1 ){` |
|        - | 13627 | `		/* Empty string,return null */` |
|      ! 0 | 13628 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13629 | `		return PH7_OK;` |
|        - | 13630 | `	}` |
|        3 | 13631 | `	zEnd = &zIn[nByte];` |
|        - | 13632 | `	/* Start the decoding process */` |
|        5 | 13633 | `	while( zIn < zEnd ){` |
|        3 | 13634 | `		c = PH7_Utf8Read(zIn,zEnd,&zIn);` |
|        3 | 13635 | `		if( c == 0x0 ){` |
|      ! 0 | 13636 | `			break;` |
|        - | 13637 | `		}` |
|        3 | 13638 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|        1 | 13639 | `	}` |
|        3 | 13640 | `	return PH7_OK;` |
|        2 | 13641 |  |
|        - | 13642 | `/* Table of built-in VM functions. */` |
|        - | 13643 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 13644 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 13645 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 13646 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 13647 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 13648 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 13649 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 13650 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 13651 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 13652 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 13653 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 13654 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 13655 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 13656 | `	    /* Constants management */` |
|        - | 13657 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 13658 | `	{ "define",   vm_builtin_define               },` |
|        - | 13659 | `	{ "constant", vm_builtin_constant             },` |
|        - | 13660 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 13661 | `	   /* Class/Object functions */` |
|        - | 13662 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 13663 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 13664 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 13665 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 13666 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 13667 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 13668 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 13669 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 13670 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 13671 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 13672 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 13673 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 13674 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 13675 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 13676 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 13677 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 13678 | `	   /* Random numbers/strings generators */` |
|        - | 13679 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 13680 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 13681 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 13682 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 13683 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 13684 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 13685 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 13686 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 13687 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 13688 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 13689 | `	   /* Language constructs functions */` |
|        - | 13690 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 13691 | `	{ "print", vm_builtin_print                   },` |
|        - | 13692 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 13693 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 13694 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 13695 | `	  /* Variable handling functions */` |
|        - | 13696 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 13697 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 13698 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 13699 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 13700 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 13701 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 13702 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 13703 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 13704 | `	  /* Ouput control functions */` |
|        - | 13705 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 13706 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 13707 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 13708 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 13709 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 13710 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 13711 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 13712 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 13713 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 13714 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 13715 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 13716 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 13717 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 13718 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 13719 | `	  /* Assertion functions */` |
|        - | 13720 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 13721 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 13722 | `	  /* Error reporting functions */` |
|        - | 13723 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 13724 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 13725 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 13726 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 13727 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 13728 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 13729 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 13730 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 13731 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 13732 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 13733 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 13734 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 13735 | `	  /* Release info */` |
|        - | 13736 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 13737 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 13738 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 13739 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 13740 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 13741 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 13742 | `	  /* hashmap */` |
|        - | 13743 | `	{"compact",          vm_builtin_compact       },` |
|        - | 13744 | `	{"extract",          vm_builtin_extract       },` |
|        - | 13745 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 13746 | `	  /* URL related function */` |
|        - | 13747 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 13748 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 13749 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 13750 | `	   /* XML processing functions */` |
|        - | 13751 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 13752 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 13753 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 13754 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 13755 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 13756 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 13757 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 13758 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 13759 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 13760 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 13761 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 13762 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 13763 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 13764 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 13765 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 13766 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 13767 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 13768 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 13769 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 13770 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 13771 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 13772 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 13773 | `	   /* UTF-8 encoding/decoding */` |
|        - | 13774 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 13775 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 13776 | `	   /* Command line processing */` |
|        - | 13777 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 13778 | `	   /* JSON encoding/decoding */` |
|        - | 13779 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 13780 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 13781 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 13782 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 13783 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 13784 | `	   /* Files/URI inclusion facility */` |
|        - | 13785 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 13786 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 13787 | `	{ "include",      vm_builtin_include          },` |
|        - | 13788 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 13789 | `	{ "require",      vm_builtin_require          },` |
|        - | 13790 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 13791 | `};` |
|        - | 13792 | `/*` |
|        - | 13793 | ` * Register the built-in VM functions defined above.` |
|        - | 13794 | ` */` |
|      924 | 13795 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 13796 |  |
|        - | 13797 | `	sxi32 rc;` |
|        - | 13798 | `	sxu32 n;` |
|   115502 | 13799 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 13800 | `		/* Note that these special functions have access` |
|        - | 13801 | `		 * to the underlying virtual machine as their` |
|        - | 13802 | `		 * private data.` |
|        - | 13803 | `		 */` |
|   114578 | 13804 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   114578 | 13805 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 13806 | `			return rc;` |
|        - | 13807 | `		}` |
|    57290 | 13808 | `	}` |
|      926 | 13809 | `	return SXRET_OK;` |
|      464 | 13810 |  |
|        - | 13811 | `/*` |
|        - | 13812 | ` * Check if the given name refer to an installed class.` |
|        - | 13813 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 13814 | ` */` |
|     1868 | 13815 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 13816 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 13817 | `	const char *zName,  /* Name of the target class */` |
|        - | 13818 | `	sxu32 nByte,        /* zName length */` |
|        - | 13819 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 13820 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 13821 | `						 */` |
|        - | 13822 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 13823 | `	)` |
|        2 | 13824 |  |
|        - | 13825 | `	SyHashEntry *pEntry;` |
|        - | 13826 | `	ph7_class *pClass;` |
|      934 | 13827 | `		SXUNUSED(iNest);` |
|        - | 13828 | `	/* Perform a hash lookup */` |
|     1870 | 13829 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|        - | 13830 |  |
|     1870 | 13831 | `	if( pEntry == 0 ){` |
|        - | 13832 | `		/* No such entry,return NULL */` |
|      ! 0 | 13833 | `		return 0;` |
|        - | 13834 | `	}` |
|     1870 | 13835 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|     1870 | 13836 | `	if( !iLoadable ){` |
|        - | 13837 | `		/* Return the first class seen */` |
|     1348 | 13838 | `		return pClass;` |
|      ! 0 | 13839 | `	}else{` |
|        - | 13840 | `		/* Check the collision list */` |
|      524 | 13841 | `		while(pClass){` |
|      524 | 13842 | `			if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT)) == 0 ){` |
|        - | 13843 | `				/* Class is loadable */` |
|      524 | 13844 | `				return pClass;` |
|        - | 13845 | `			}` |
|        - | 13846 | `			/* Point to the next entry */` |
|      ! 0 | 13847 | `			pClass = pClass->pNextName;` |
|      ! 0 | 13848 | `		}` |
|        - | 13849 | `	}` |
|        - | 13850 | `	/* No such loadable class */` |
|      ! 0 | 13851 | `	return 0;` |
|      936 | 13852 |  |
|        - | 13853 | `/*` |
|        - | 13854 | ` * Reference Table Implementation` |
|        - | 13855 | ` * Status: stable <chm@symisc.net>` |
|        - | 13856 | ` * Intro` |
|        - | 13857 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 13858 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 13859 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 13860 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 13861 | ` *  Refer to the official for more information on this powerful` |
|        - | 13862 | ` *  extension.` |
|        - | 13863 | ` */` |
|        - | 13864 | `/*` |
|        - | 13865 | ` * Allocate a new reference entry.` |
|        - | 13866 | ` */` |
|   564334 | 13867 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 13868 |  |
|        - | 13869 | `	VmRefObj *pRef;` |
|        - | 13870 | `	/* Allocate a new instance */` |
|   564336 | 13871 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|   564336 | 13872 | `	if( pRef == 0 ){` |
|      ! 0 | 13873 | `		return 0;` |
|        - | 13874 | `	}` |
|        - | 13875 | `	/* Zero the structure */` |
|   564336 | 13876 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 13877 | `	/* Initialize fields */` |
|   564336 | 13878 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|   564336 | 13879 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|   564336 | 13880 | `	pRef->nIdx = nIdx;` |
|   564336 | 13881 | `	return pRef;` |
|   282169 | 13882 |  |
|        - | 13883 | `/*` |
|        - | 13884 | ` * Default hash function used by the reference table` |
|        - | 13885 | ` * for lookup/insertion operations.` |
|        - | 13886 | ` */` |
|  2548589 | 13887 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 13888 |  |
|        - | 13889 | `	/* Calculate the hash based on the memory object index */` |
|  2548591 | 13890 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 13891 |  |
|        - | 13892 | `/*` |
|        - | 13893 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 13894 | ` * in the reference table.` |
|        - | 13895 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 13896 | ` * otherwise.` |
|        - | 13897 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 13898 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 13899 | ` * the reference implementation is consistent,solid and it's` |
|        - | 13900 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 13901 | ` * Refer to the official for more information on this powerful` |
|        - | 13902 | ` * extension.` |
|        - | 13903 | ` */` |
|  1672256 | 13904 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 13905 |  |
|        - | 13906 | `	VmRefObj *pRef;` |
|        - | 13907 | `	sxu32 nBucket;` |
|        - | 13908 | `	/* Point to the appropriate bucket */` |
|  1672258 | 13909 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 13910 | `	/* Perform the lookup */` |
|  1672258 | 13911 | `	pRef = pVm->apRefObj[nBucket];` |
|  5168230 | 13912 | `	for(;;){` |
| 10329693 | 13913 | `		if( pRef == 0 ){` |
|   600608 | 13914 | `			break;` |
|        - | 13915 | `		}` |
|  9729087 | 13916 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 13917 | `			/* Entry found */` |
|  1071652 | 13918 | `			return pRef;` |
|        - | 13919 | `		}` |
|        - | 13920 | `		/* Point to the next entry */` |
|  8657436 | 13921 | `		pRef = pRef->pNextCollide;` |
|        1 | 13922 | `	}` |
|        - | 13923 | `	/* No such entry,return NULL */` |
|   600608 | 13924 | `	return 0;` |
|   836130 | 13925 |  |
|        - | 13926 | `/*` |
|        - | 13927 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 13928 | ` *` |
|        - | 13929 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 13930 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 13931 | ` * the reference implementation is consistent,solid and it's` |
|        - | 13932 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 13933 | ` * Refer to the official for more information on this powerful` |
|        - | 13934 | ` * extension.` |
|        - | 13935 | ` */` |
|   564334 | 13936 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 13937 |  |
|        - | 13938 | `	sxu32 nBucket;` |
|   564336 | 13939 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 13940 | `		VmRefObj **apNew;` |
|        - | 13941 | `		sxu32 nNew;` |
|        - | 13942 | `		/* Allocate a larger table */` |
|     1084 | 13943 | `		nNew = pVm->nRefSize << 1;` |
|     1084 | 13944 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     1084 | 13945 | `		if( apNew ){` |
|     1084 | 13946 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 13947 | `			sxu32 n;` |
|        - | 13948 | `			/* Zero the structure */` |
|     1084 | 13949 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 13950 | `			/* Rehash all referenced entries */` |
|    96012 | 13951 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 13952 | `				/* Remove old collision links */` |
|    94930 | 13953 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 13954 | `				/* Point to the appropriate bucket */` |
|    94930 | 13955 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 13956 | `				/* Insert the entry  */` |
|    94930 | 13957 | `				pEntry->pNextCollide = apNew[nBucket];` |
|    94930 | 13958 | `				if( apNew[nBucket] ){` |
|    82611 | 13959 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|    41305 | 13960 | `				}` |
|    94930 | 13961 | `				apNew[nBucket] = pEntry;` |
|        - | 13962 | `				/* Point to the next entry */` |
|    94930 | 13963 | `				pEntry = pEntry->pNext;` |
|    47466 | 13964 | `			}` |
|        - | 13965 | `			/* Release the old table */` |
|     1084 | 13966 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 13967 | `			/* Install the new one */` |
|     1084 | 13968 | `			pVm->apRefObj = apNew;` |
|     1084 | 13969 | `			pVm->nRefSize = nNew;` |
|      541 | 13970 | `		}` |
|      541 | 13971 | `	}` |
|        - | 13972 | `	/* Point to the appropriate bucket */` |
|   564336 | 13973 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 13974 | `	/* Insert the entry */` |
|   564336 | 13975 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|   564336 | 13976 | `	if( pVm->apRefObj[nBucket] ){` |
|   547736 | 13977 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|   273653 | 13978 | `	}` |
|   564336 | 13979 | `	pVm->apRefObj[nBucket] = pRef;` |
|   564336 | 13980 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|   564336 | 13981 | `	pVm->nRefUsed++;` |
|   564336 | 13982 | `	return SXRET_OK;` |
|        2 | 13983 |  |
|        - | 13984 | `/*` |
|        - | 13985 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 13986 | ` * the reference table.` |
|        - | 13987 | ` * This function is invoked when the user perform an unset` |
|        - | 13988 | ` * call [i.e: unset($var); ].` |
|        - | 13989 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 13990 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 13991 | ` * the reference implementation is consistent,solid and it's` |
|        - | 13992 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 13993 | ` * Refer to the official for more information on this powerful` |
|        - | 13994 | ` * extension.` |
|        - | 13995 | ` */` |
|   549518 | 13996 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 13997 |  |
|        - | 13998 | `	ph7_hashmap_node **apNode;` |
|        - | 13999 | `	SyHashEntry **apEntry;` |
|        - | 14000 | `	sxu32 n;` |
|        - | 14001 | `	/* Point to the reference table */` |
|   549520 | 14002 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|   549520 | 14003 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 14004 | `	/* Unlink the entry from the reference table */` |
|   589982 | 14005 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    40464 | 14006 | `		if( apEntry[n] ){` |
|    40432 | 14007 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    20215 | 14008 | `		}` |
|    20233 | 14009 | `	}` |
|  1062544 | 14010 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|   513026 | 14011 | `		if( apNode[n] ){` |
|     4879 | 14012 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     2439 | 14013 | `		}` |
|   256514 | 14014 | `	}` |
|   549520 | 14015 | `	if( pRef->pPrevCollide ){` |
|   332448 | 14016 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   166273 | 14017 | `	}else{` |
|   217073 | 14018 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 14019 | `	}` |
|   549520 | 14020 | `	if( pRef->pNextCollide ){` |
|   527764 | 14021 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   263503 | 14022 | `	}` |
|   549520 | 14023 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 14024 | `	/* Release the node */` |
|   549520 | 14025 | `	SySetRelease(&pRef->aReference);` |
|   549520 | 14026 | `	SySetRelease(&pRef->aArrEntries);` |
|   549520 | 14027 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|   549520 | 14028 | `	pVm->nRefUsed--;` |
|   549520 | 14029 | `	return SXRET_OK;` |
|        2 | 14030 |  |
|        - | 14031 | `/*` |
|        - | 14032 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 14033 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14034 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14035 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14036 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14037 | ` * Refer to the official for more information on this powerful` |
|        - | 14038 | ` * extension.` |
|        - | 14039 | ` */` |
|   578282 | 14040 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 14041 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 14042 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 14043 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 14044 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 14045 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 14046 | `	)` |
|        2 | 14047 |  |
|   578284 | 14048 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 14049 | `	VmRefObj *pRef;` |
|        - | 14050 | `	/* Check if the referenced object already exists */` |
|   578284 | 14051 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|   578284 | 14052 | `	if( pRef == 0 ){` |
|        - | 14053 | `		/* Create a new entry */` |
|   564336 | 14054 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|   564336 | 14055 | `		if( pRef == 0 ){` |
|      ! 0 | 14056 | `			return SXERR_MEM;` |
|        - | 14057 | `		}` |
|   564336 | 14058 | `		pRef->iFlags = iFlags;` |
|        - | 14059 | `		/* Install the entry */` |
|   564336 | 14060 | `		VmRefObjInsert(&(*pVm),pRef);` |
|   282167 | 14061 | `	}` |
|   578324 | 14062 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - | 14063 | `		/* Safely ignore the exception frame */` |
|       42 | 14064 | `		pFrame = pFrame->pParent;` |
|        2 | 14065 | `	}` |
|   578284 | 14066 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 14067 | `		VmSlot sRef;` |
|        - | 14068 | `		/* Local frame,record referenced entry so that it can` |
|        - | 14069 | `		 * be deleted when we leave this frame.` |
|        - | 14070 | `		 */` |
|    36286 | 14071 | `		sRef.nIdx = nIdx;` |
|    36286 | 14072 | `		sRef.pUserData = pEntry;` |
|    36286 | 14073 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 14074 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 14075 | `		}` |
|    18142 | 14076 | `	}` |
|   578284 | 14077 | `	if( pEntry ){` |
|        - | 14078 | `		/* Address of the hash-entry */` |
|    50068 | 14079 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    25033 | 14080 | `	}` |
|   578284 | 14081 | `	if( pMapEntry ){` |
|        - | 14082 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|   526296 | 14083 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|   263147 | 14084 | `	}` |
|   578284 | 14085 | `	return SXRET_OK;` |
|   289143 | 14086 |  |
|        - | 14087 | `/*` |
|        - | 14088 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 14089 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14090 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14091 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14092 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14093 | ` * Refer to the official for more information on this powerful` |
|        - | 14094 | ` * extension.` |
|        - | 14095 | ` */` |
|   544436 | 14096 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 14097 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 14098 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 14099 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 14100 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 14101 | `	)` |
|        2 | 14102 |  |
|        - | 14103 | `	VmRefObj *pRef;` |
|        - | 14104 | `	sxu32 n;` |
|        - | 14105 | `	/* Check if the referenced object already exists */` |
|   544438 | 14106 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|   544438 | 14107 | `	if( pRef == 0 ){` |
|        - | 14108 | `		/* Not such entry */` |
|    36254 | 14109 | `		return SXERR_NOTFOUND;` |
|        - | 14110 | `	}` |
|        - | 14111 | `	/* Remove the desired entry */` |
|   508186 | 14112 | `	if( pEntry ){` |
|        - | 14113 | `		SyHashEntry **apEntry;` |
|       33 | 14114 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      129 | 14115 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|       97 | 14116 | `			if( apEntry[n] == pEntry ){` |
|        - | 14117 | `				/* Nullify the entry */` |
|       33 | 14118 | `				apEntry[n] = 0;` |
|        - | 14119 | `				/*` |
|        - | 14120 | `				 * NOTE:` |
|        - | 14121 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 14122 | `				 * we avoid wasting spaces.` |
|        - | 14123 | `				 */` |
|       16 | 14124 | `			}` |
|       49 | 14125 | `		}` |
|       16 | 14126 | `	}` |
|   508186 | 14127 | `	if( pMapEntry ){` |
|        - | 14128 | `		ph7_hashmap_node **apNode;` |
|   508154 | 14129 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  1016394 | 14130 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|   508242 | 14131 | `			if( apNode[n] == pMapEntry ){` |
|        - | 14132 | `				/* nullify the entry */` |
|   508154 | 14133 | `				apNode[n] = 0;` |
|   254076 | 14134 | `			}` |
|   254122 | 14135 | `		}` |
|   254076 | 14136 | `	}` |
|   508186 | 14137 | `	return SXRET_OK;` |
|   272220 | 14138 |  |
|        - | 14139 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 14140 | `/*` |
|        - | 14141 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 14142 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 14143 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 14144 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 14145 | ` * For more information on how to register IO stream devices,please` |
|        - | 14146 | ` * refer to the official documentation.` |
|        - | 14147 | ` */` |
|    16542 | 14148 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 14149 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 14150 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 14151 | `	int nByte              /* *pzDevice length*/` |
|        - | 14152 | `	)` |
|        2 | 14153 |  |
|        - | 14154 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 14155 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 14156 | `	SyString sDev,sCur;` |
|        - | 14157 | `	sxu32 n,nEntry;` |
|        - | 14158 | `	int rc;` |
|        - | 14159 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    16544 | 14160 | `	zNext = zCur = zIn = *pzDevice;` |
|    16544 | 14161 | `	zEnd = &zIn[nByte];` |
|   998911 | 14162 | `	while( zIn < zEnd ){` |
|   982371 | 14163 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 14164 | `			/* Got one */` |
|        3 | 14165 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 14166 | `			break;` |
|        - | 14167 | `		}` |
|        - | 14168 | `		/* Advance the cursor */` |
|   982369 | 14169 | `		zIn++;` |
|        2 | 14170 | `	}` |
|    16544 | 14171 | `	if( zIn >= zEnd ){` |
|        - | 14172 | `		/* No such scheme,return the default stream */` |
|    16542 | 14173 | `		return pVm->pDefStream;` |
|        - | 14174 | `	}` |
|        3 | 14175 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 14176 | `	/* Remove leading and trailing white spaces */` |
|        3 | 14177 | `	SyStringFullTrim(&sDev);` |
|        - | 14178 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 14179 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 14180 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 14181 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 14182 | `		pStream = apStream[n];` |
|        3 | 14183 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 14184 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 14185 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 14186 | `		if( rc == 0 ){` |
|        - | 14187 | `			/* Stream device found */` |
|        3 | 14188 | `			*pzDevice = zNext;` |
|        3 | 14189 | `			return pStream;` |
|        - | 14190 | `		}` |
|      ! 0 | 14191 | `	}` |
|        - | 14192 | `	/* No such stream,return NULL */` |
|      ! 0 | 14193 | `	return 0;` |
|     8273 | 14194 |  |
|        - | 14195 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 14196 | `/*` |
|        - | 14197 | ` * Section:` |
|        - | 14198 | ` *    HTTP/URI related routines.` |
|        - | 14199 | ` * Status:` |
|        - | 14200 | ` *    Stable.` |
|        - | 14201 | ` */` |
|        - | 14202 | ` /*` |
|        - | 14203 | `  * URI Parser: Split an URI into components [i.e: Host,Path,Query,...].` |
|        - | 14204 | `  * URI syntax: [method:/][/[user[:pwd]@]host[:port]/][document]` |
|        - | 14205 | `  * This almost, but not quite, RFC1738 URI syntax.` |
|        - | 14206 | `  * This routine is not a validator,it does not check for validity` |
|        - | 14207 | `  * nor decode URI parts,the only thing this routine does is splitting` |
|        - | 14208 | `  * the input to its fields.` |
|        - | 14209 | `  * Upper layer are responsible of decoding and validating URI parts.` |
|        - | 14210 | `  * On success,this function populate the "SyhttpUri" structure passed` |
|        - | 14211 | `  * as the first argument. Otherwise SXERR_* is returned when a malformed` |
|        - | 14212 | `  * input is encountered.` |
|        - | 14213 | `  */` |
|       26 | 14214 | ` static sxi32 VmHttpSplitURI(SyhttpUri *pOut,const char *zUri,sxu32 nLen)` |
|        1 | 14215 | ` {` |
|       27 | 14216 | `	 const char *zEnd = &zUri[nLen];` |
|       27 | 14217 | `	 sxu8 bHostOnly = FALSE;` |
|       27 | 14218 | `	 sxu8 bIPv6 = FALSE	;` |
|        - | 14219 | `	 const char *zCur;` |
|        - | 14220 | `	 SyString *pComp;` |
|       27 | 14221 | `	 sxu32 nPos = 0;` |
|        - | 14222 | `	 sxi32 rc;` |
|        - | 14223 | `	 /* Zero the structure first */` |
|       27 | 14224 | `	 SyZero(pOut,sizeof(SyhttpUri));` |
|        - | 14225 | `	 /* Remove leading and trailing white spaces  */` |
|       27 | 14226 | `	 SyStringInitFromBuf(&pOut->sRaw,zUri,nLen);` |
|       27 | 14227 | `	 SyStringFullTrim(&pOut->sRaw);` |
|        - | 14228 | `	 /* Find the first '/' separator */` |
|       27 | 14229 | `	 rc = SyByteFind(zUri,(sxu32)(zEnd - zUri),'/',&nPos);` |
|       27 | 14230 | `	 if( rc != SXRET_OK ){` |
|        - | 14231 | `		 /* Assume a host name only */` |
|        7 | 14232 | `		 zCur = zEnd;` |
|        7 | 14233 | `		 bHostOnly = TRUE;` |
|        7 | 14234 | `		 goto ProcessHost;` |
|        - | 14235 | `	 }` |
|       21 | 14236 | `	 zCur = &zUri[nPos];` |
|       21 | 14237 | `	 if( zUri != zCur && zCur[-1] == ':' ){` |
|        - | 14238 | `		 /* Extract a scheme:` |
|        - | 14239 | `		  * Not that we can get an invalid scheme here.` |
|        - | 14240 | `		  * Fortunately the caller can discard any URI by comparing this scheme with its` |
|        - | 14241 | `		  * registered schemes and will report the error as soon as his comparison function` |
|        - | 14242 | `		  * fail.` |
|        - | 14243 | `		  */` |
|       19 | 14244 | `	 	pComp = &pOut->sScheme;` |
|       19 | 14245 | `		SyStringInitFromBuf(pComp,zUri,(sxu32)(zCur - zUri - 1));` |
|       19 | 14246 | `		SyStringLeftTrim(pComp);` |
|        9 | 14247 | `	 }` |
|       21 | 14248 | `	 if( zCur[1] != '/' ){` |
|      ! 0 | 14249 | `		 if( zCur == zUri \|\| zCur[-1] == ':' ){` |
|        - | 14250 | `		  /* No authority */` |
|      ! 0 | 14251 | `		  goto PathSplit;` |
|        - | 14252 | `		}` |
|        - | 14253 | `		 /* There is something here , we will assume its an authority` |
|        - | 14254 | `		  * and someone has forgot the two prefix slashes "//",` |
|        - | 14255 | `		  * sooner or later we will detect if we are dealing with a malicious` |
|        - | 14256 | `		  * user or not,but now assume we are dealing with an authority` |
|        - | 14257 | `		  * and let the caller handle all the validation process.` |
|        - | 14258 | `		  */` |
|      ! 0 | 14259 | `		 goto ProcessHost;` |
|        - | 14260 | `	 }` |
|       21 | 14261 | `	 zUri = &zCur[2];` |
|       21 | 14262 | `	 zCur = zEnd;` |
|       21 | 14263 | `	 rc = SyByteFind(zUri,(sxu32)(zEnd - zUri),'/',&nPos);` |
|       29 | 14264 | `	 if( rc == SXRET_OK ){` |
|       17 | 14265 | `		 zCur = &zUri[nPos];` |
|        8 | 14266 | `	 }` |
|        2 | 14267 | ` ProcessHost:` |
|        - | 14268 | `	 /* Extract user information if present */` |
|       27 | 14269 | `	 rc = SyByteFind(zUri,(sxu32)(zCur - zUri),'@',&nPos);` |
|       27 | 14270 | `	 if( rc == SXRET_OK ){` |
|        7 | 14271 | `		 if( nPos > 0 ){` |
|        - | 14272 | `			 sxu32 nPassOfft; /* Password offset */` |
|        7 | 14273 | `			 pComp = &pOut->sUser;` |
|        7 | 14274 | `			 SyStringInitFromBuf(pComp,zUri,nPos);` |
|        - | 14275 | `			 /* Extract the password if available */` |
|        7 | 14276 | `			 rc = SyByteFind(zUri,(sxu32)(zCur - zUri),':',&nPassOfft);` |
|        7 | 14277 | `			 if( rc == SXRET_OK && nPassOfft < nPos){` |
|        7 | 14278 | `				 pComp->nByte = nPassOfft;` |
|        7 | 14279 | `				 pComp = &pOut->sPass;` |
|        7 | 14280 | `				 pComp->zString = &zUri[nPassOfft+sizeof(char)];` |
|        7 | 14281 | `				 pComp->nByte = nPos - nPassOfft - 1;` |
|        3 | 14282 | `			 }` |
|        - | 14283 | `			 /* Update the cursor */` |
|        7 | 14284 | `			 zUri = &zUri[nPos+1];` |
|        4 | 14285 | `		 }else{` |
|      ! 0 | 14286 | `			 zUri++;` |
|        - | 14287 | `		 }` |
|        3 | 14288 | `	 }` |
|       27 | 14289 | `	 pComp = &pOut->sHost;` |
|       27 | 14290 | `	 while( zUri < zCur && SyisSpace(zUri[0])){` |
|      ! 0 | 14291 | `		 zUri++;` |
|      ! 0 | 14292 | `	 }` |
|       27 | 14293 | `	 SyStringInitFromBuf(pComp,zUri,(sxu32)(zCur - zUri));` |
|       27 | 14294 | `	 if( pComp->zString[0] == '[' ){` |
|        - | 14295 | `		 /* An IPv6 Address: Make a simple naive test` |
|        - | 14296 | `		  */` |
|        3 | 14297 | `		 zUri++; pComp->zString++; pComp->nByte = 0;` |
|        9 | 14298 | `		 while( ((unsigned char)zUri[0] < 0xc0 && SyisHex(zUri[0])) \|\| zUri[0] == ':' ){` |
|        7 | 14299 | `			 zUri++; pComp->nByte++;` |
|        1 | 14300 | `		 }` |
|        3 | 14301 | `		 if( zUri[0] != ']' ){` |
|      ! 0 | 14302 | `			 return SXERR_CORRUPT; /* Malformed IPv6 address */` |
|        - | 14303 | `		 }` |
|        3 | 14304 | `		 zUri++;` |
|        3 | 14305 | `		 bIPv6 = TRUE;` |
|        1 | 14306 | `	 }` |
|        - | 14307 | `	 /* Extract a port number if available */` |
|       27 | 14308 | `	 rc = SyByteFind(zUri,(sxu32)(zCur - zUri),':',&nPos);` |
|       27 | 14309 | `	 if( rc == SXRET_OK ){` |
|       11 | 14310 | `		 if( bIPv6 == FALSE ){` |
|       11 | 14311 | `			 pComp->nByte = (sxu32)(&zUri[nPos] - zUri);` |
|        5 | 14312 | `		 }` |
|       11 | 14313 | `		 pComp = &pOut->sPort;` |
|       11 | 14314 | `		 SyStringInitFromBuf(pComp,&zUri[nPos+1],(sxu32)(zCur - &zUri[nPos+1]));` |
|        5 | 14315 | `	 }` |
|       27 | 14316 | `	 if( bHostOnly == TRUE ){` |
|        7 | 14317 | `		 return SXRET_OK;` |
|        - | 14318 | `	 }` |
|       10 | 14319 | `PathSplit:` |
|       21 | 14320 | `	 zUri = zCur;` |
|       21 | 14321 | `	 pComp = &pOut->sPath;` |
|       21 | 14322 | `	 SyStringInitFromBuf(pComp,zUri,(sxu32)(zEnd-zUri));` |
|       21 | 14323 | `	 if( pComp->nByte == 0 ){` |
|        5 | 14324 | `		 return SXRET_OK; /* Empty path */` |
|        - | 14325 | `	 }` |
|       17 | 14326 | `	 if( SXRET_OK == SyByteFind(zUri,(sxu32)(zEnd-zUri),'?',&nPos) ){` |
|        5 | 14327 | `		 pComp->nByte = nPos; /* Update path length */` |
|        5 | 14328 | `		 pComp = &pOut->sQuery;` |
|        5 | 14329 | `		 SyStringInitFromBuf(pComp,&zUri[nPos+1],(sxu32)(zEnd-&zUri[nPos+1]));` |
|        2 | 14330 | `	 }` |
|       17 | 14331 | `	 if( SXRET_OK == SyByteFind(zUri,(sxu32)(zEnd-zUri),'#',&nPos) ){` |
|        - | 14332 | `		 /* Update path or query length */` |
|        5 | 14333 | `		 if( pComp == &pOut->sPath ){` |
|      ! 0 | 14334 | `			 pComp->nByte = nPos;` |
|      ! 0 | 14335 | `		 }else{` |
|        5 | 14336 | `			 if( &zUri[nPos] < (char *)SyStringData(pComp) ){` |
|        - | 14337 | `				 /* Malformed syntax : Query must be present before fragment */` |
|      ! 0 | 14338 | `				 return SXERR_SYNTAX;` |
|        - | 14339 | `			 }` |
|        5 | 14340 | `			 pComp->nByte -= (sxu32)(zEnd - &zUri[nPos]);` |
|        - | 14341 | `		 }` |
|        5 | 14342 | `		 pComp = &pOut->sFragment;` |
|        5 | 14343 | `		 SyStringInitFromBuf(pComp,&zUri[nPos+1],(sxu32)(zEnd-&zUri[nPos+1]))` |
|        2 | 14344 | `	 }` |
|       17 | 14345 | `	 return SXRET_OK;` |
|       14 | 14346 | ` }` |
|        - | 14347 | ` /*` |
|        - | 14348 | ` * Extract a single line from a raw HTTP request.` |
|        - | 14349 | ` * Return SXRET_OK on success,SXERR_EOF when end of input` |
|        - | 14350 | ` * and SXERR_MORE when more input is needed.` |
|        - | 14351 | ` */` |
|      ! 0 | 14352 | `static sxi32 VmGetNextLine(SyString *pCursor,SyString *pCurrent)` |
|      ! 0 | 14353 |  |
|        - | 14354 | `  	const char *zIn;` |
|        - | 14355 | `  	sxu32 nPos;` |
|        - | 14356 | `	/* Jump leading white spaces */` |
|      ! 0 | 14357 | `	SyStringLeftTrim(pCursor);` |
|      ! 0 | 14358 | `	if( pCursor->nByte < 1 ){` |
|      ! 0 | 14359 | `		SyStringInitFromBuf(pCurrent,0,0);` |
|      ! 0 | 14360 | `		return SXERR_EOF; /* End of input */` |
|        - | 14361 | `	}` |
|      ! 0 | 14362 | `	zIn = SyStringData(pCursor);` |
|      ! 0 | 14363 | `	if( SXRET_OK != SyByteListFind(pCursor->zString,pCursor->nByte,"\r\n",&nPos) ){` |
|        - | 14364 | `		/* Line not found,tell the caller to read more input from source */` |
|      ! 0 | 14365 | `		SyStringDupPtr(pCurrent,pCursor);` |
|      ! 0 | 14366 | `		return SXERR_MORE;` |
|        - | 14367 | `	}` |
|      ! 0 | 14368 | `  	pCurrent->zString = zIn;` |
|      ! 0 | 14369 | `  	pCurrent->nByte	= nPos;` |
|        - | 14370 | `  	/* advance the cursor so we can call this routine again */` |
|      ! 0 | 14371 | `  	pCursor->zString = &zIn[nPos];` |
|      ! 0 | 14372 | `  	pCursor->nByte -= nPos;` |
|      ! 0 | 14373 | `  	return SXRET_OK;` |
|      ! 0 | 14374 | ` }` |
|        - | 14375 | ` /*` |
|        - | 14376 | `  * Split a single MIME header into a name value pair.` |
|        - | 14377 | `  * This function return SXRET_OK,SXERR_CONTINUE on success.` |
|        - | 14378 | `  * Otherwise SXERR_NEXT is returned when a malformed header` |
|        - | 14379 | `  * is encountered.` |
|        - | 14380 | `  * Note: This function handle also mult-line headers.` |
|        - | 14381 | `  */` |
|      ! 0 | 14382 | ` static sxi32 VmHttpProcessOneHeader(SyhttpHeader *pHdr,SyhttpHeader *pLast,const char *zLine,sxu32 nLen)` |
|      ! 0 | 14383 | ` {` |
|        - | 14384 | `	 SyString *pName;` |
|        - | 14385 | `	 sxu32 nPos;` |
|        - | 14386 | `	 sxi32 rc;` |
|      ! 0 | 14387 | `	 if( nLen < 1 ){` |
|      ! 0 | 14388 | `		 return SXERR_NEXT;` |
|        - | 14389 | `	 }` |
|        - | 14390 | `	 /* Check for multi-line header */` |
|      ! 0 | 14391 | `	if( pLast && (zLine[-1] == ' ' \|\| zLine[-1] == '\t') ){` |
|      ! 0 | 14392 | `		SyString *pTmp = &pLast->sValue;` |
|      ! 0 | 14393 | `		SyStringFullTrim(pTmp);` |
|      ! 0 | 14394 | `		if( pTmp->nByte == 0 ){` |
|      ! 0 | 14395 | `			SyStringInitFromBuf(pTmp,zLine,nLen);` |
|      ! 0 | 14396 | `		}else{` |
|        - | 14397 | `			/* Update header value length */` |
|      ! 0 | 14398 | `			pTmp->nByte = (sxu32)(&zLine[nLen] - pTmp->zString);` |
|        - | 14399 | `		}` |
|        - | 14400 | `		 /* Simply tell the caller to reset its states and get another line */` |
|      ! 0 | 14401 | `		 return SXERR_CONTINUE;` |
|        - | 14402 | `	 }` |
|        - | 14403 | `	/* Split the header */` |
|      ! 0 | 14404 | `	pName = &pHdr->sName;` |
|      ! 0 | 14405 | `	rc = SyByteFind(zLine,nLen,':',&nPos);` |
|      ! 0 | 14406 | `	if(rc != SXRET_OK ){` |
|      ! 0 | 14407 | `		return SXERR_NEXT; /* Malformed header;Check the next entry */` |
|        - | 14408 | `	}` |
|      ! 0 | 14409 | `	SyStringInitFromBuf(pName,zLine,nPos);` |
|      ! 0 | 14410 | `	SyStringFullTrim(pName);` |
|        - | 14411 | `	/* Extract a header value */` |
|      ! 0 | 14412 | `	SyStringInitFromBuf(&pHdr->sValue,&zLine[nPos + 1],nLen - nPos - 1);` |
|        - | 14413 | `	/* Remove leading and trailing whitespaces */` |
|      ! 0 | 14414 | `	SyStringFullTrim(&pHdr->sValue);` |
|      ! 0 | 14415 | `	return SXRET_OK;` |
|      ! 0 | 14416 | ` }` |
|        - | 14417 | ` /*` |
|        - | 14418 | `  * Extract all MIME headers associated with a HTTP request.` |
|        - | 14419 | `  * After processing the first line of a HTTP request,the following` |
|        - | 14420 | `  * routine is called in order to extract MIME headers.` |
|        - | 14421 | `  * This function return SXRET_OK on success,SXERR_MORE when it needs` |
|        - | 14422 | `  * more inputs.` |
|        - | 14423 | `  * Note: Any malformed header is simply discarded.` |
|        - | 14424 | `  */` |
|      ! 0 | 14425 | ` static sxi32 VmHttpExtractHeaders(SyString *pRequest,SySet *pOut)` |
|      ! 0 | 14426 | ` {` |
|      ! 0 | 14427 | `	 SyhttpHeader *pLast = 0;` |
|        - | 14428 | `	 SyString sCurrent;` |
|        - | 14429 | `	 SyhttpHeader sHdr;` |
|        - | 14430 | `	 sxu8 bEol;` |
|        - | 14431 | `	 sxi32 rc;` |
|      ! 0 | 14432 | `	 if( SySetUsed(pOut) > 0 ){` |
|      ! 0 | 14433 | `		 pLast = (SyhttpHeader *)SySetAt(pOut,SySetUsed(pOut)-1);` |
|      ! 0 | 14434 | `	 }` |
|      ! 0 | 14435 | `	 bEol = FALSE;` |
|      ! 0 | 14436 | `	 for(;;){` |
|      ! 0 | 14437 | `		 SyZero(&sHdr,sizeof(SyhttpHeader));` |
|        - | 14438 | `		 /* Extract a single line from the raw HTTP request */` |
|      ! 0 | 14439 | `		 rc = VmGetNextLine(pRequest,&sCurrent);` |
|      ! 0 | 14440 | `		 if(rc != SXRET_OK ){` |
|      ! 0 | 14441 | `			 if( sCurrent.nByte < 1 ){` |
|      ! 0 | 14442 | `				 break;` |
|        - | 14443 | `			 }` |
|      ! 0 | 14444 | `			 bEol = TRUE;` |
|      ! 0 | 14445 | `		 }` |
|        - | 14446 | `		 /* Process the header */` |
|      ! 0 | 14447 | `		 if( SXRET_OK == VmHttpProcessOneHeader(&sHdr,pLast,sCurrent.zString,sCurrent.nByte)){` |
|      ! 0 | 14448 | `			 if( SXRET_OK != SySetPut(pOut,(const void *)&sHdr) ){` |
|      ! 0 | 14449 | `				 break;` |
|        - | 14450 | `			 }` |
|        - | 14451 | `			 /* Retrieve the last parsed header so we can handle multi-line header` |
|        - | 14452 | `			  * in case we face one of them.` |
|        - | 14453 | `			  */` |
|      ! 0 | 14454 | `			 pLast = (SyhttpHeader *)SySetPeek(pOut);` |
|      ! 0 | 14455 | `		 }` |
|      ! 0 | 14456 | `		 if( bEol ){` |
|      ! 0 | 14457 | `			 break;` |
|        - | 14458 | `		 }` |
|      ! 0 | 14459 | `	 } /* for(;;) */` |
|      ! 0 | 14460 | `	 return SXRET_OK;` |
|      ! 0 | 14461 | ` }` |
|        - | 14462 | ` /*` |
|        - | 14463 | `  * Process the first line of a HTTP request.` |
|        - | 14464 | `  * This routine perform the following operations` |
|        - | 14465 | `  *  1) Extract the HTTP method.` |
|        - | 14466 | `  *  2) Split the request URI to it's fields [ie: host,path,query,...].` |
|        - | 14467 | `  *  3) Extract the HTTP protocol version.` |
|        - | 14468 | `  */` |
|      ! 0 | 14469 | ` static sxi32 VmHttpProcessFirstLine(` |
|        - | 14470 | `	 SyString *pRequest, /* Raw HTTP request */` |
|        - | 14471 | `	 sxi32 *pMethod,     /* OUT: HTTP method */` |
|        - | 14472 | `	 SyhttpUri *pUri,    /* OUT: Parse of the URI */` |
|        - | 14473 | `	 sxi32 *pProto       /* OUT: HTTP protocol */` |
|        - | 14474 | `	 )` |
|      ! 0 | 14475 | ` {` |
|        - | 14476 | `	 static const char *azMethods[] = { "get","post","head","put"};` |
|        - | 14477 | `	 static const sxi32 aMethods[]  = { HTTP_METHOD_GET,HTTP_METHOD_POST,HTTP_METHOD_HEAD,HTTP_METHOD_PUT};` |
|        - | 14478 | `	 const char *zIn,*zEnd,*zPtr;` |
|        - | 14479 | `	 SyString sLine;` |
|        - | 14480 | `	 sxu32 nLen;` |
|        - | 14481 | `	 sxi32 rc;` |
|        - | 14482 | `	 /* Extract the first line and update the pointer */` |
|      ! 0 | 14483 | `	 rc = VmGetNextLine(pRequest,&sLine);` |
|      ! 0 | 14484 | `	 if( rc != SXRET_OK ){` |
|      ! 0 | 14485 | `		 return rc;` |
|        - | 14486 | `	 }` |
|      ! 0 | 14487 | `	 if ( sLine.nByte < 1 ){` |
|        - | 14488 | `		 /* Empty HTTP request */` |
|      ! 0 | 14489 | `		 return SXERR_EMPTY;` |
|        - | 14490 | `	 }` |
|        - | 14491 | `	 /* Delimit the line and ignore trailing and leading white spaces */` |
|      ! 0 | 14492 | `	 zIn = sLine.zString;` |
|      ! 0 | 14493 | `	 zEnd = &zIn[sLine.nByte];` |
|      ! 0 | 14494 | `	 while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|      ! 0 | 14495 | `		 zIn++;` |
|      ! 0 | 14496 | `	 }` |
|        - | 14497 | `	 /* Extract the HTTP method */` |
|      ! 0 | 14498 | `	 zPtr = zIn;` |
|      ! 0 | 14499 | `	 while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|      ! 0 | 14500 | `		 zIn++;` |
|      ! 0 | 14501 | `	 }` |
|      ! 0 | 14502 | `	 *pMethod = HTTP_METHOD_OTHR;` |
|      ! 0 | 14503 | `	 if( zIn > zPtr ){` |
|        - | 14504 | `		 sxu32 i;` |
|      ! 0 | 14505 | `		 nLen = (sxu32)(zIn-zPtr);` |
|      ! 0 | 14506 | `		 for( i = 0 ; i < SX_ARRAYSIZE(azMethods) ; ++i ){` |
|      ! 0 | 14507 | `			 if( SyStrnicmp(azMethods[i],zPtr,nLen) == 0 ){` |
|      ! 0 | 14508 | `				 *pMethod = aMethods[i];` |
|      ! 0 | 14509 | `				 break;` |
|        - | 14510 | `			 }` |
|      ! 0 | 14511 | `		 }` |
|      ! 0 | 14512 | `	 }` |
|        - | 14513 | `	 /* Jump trailing white spaces */` |
|      ! 0 | 14514 | `	 while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|      ! 0 | 14515 | `		 zIn++;` |
|      ! 0 | 14516 | `	 }` |
|        - | 14517 | `	  /* Extract the request URI */` |
|      ! 0 | 14518 | `	 zPtr = zIn;` |
|      ! 0 | 14519 | `	 while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|      ! 0 | 14520 | `		 zIn++;` |
|      ! 0 | 14521 | `	 }` |
|      ! 0 | 14522 | `	 if( zIn > zPtr ){` |
|      ! 0 | 14523 | `		 nLen = (sxu32)(zIn-zPtr);` |
|        - | 14524 | `		 /* Split raw URI to it's fields */` |
|      ! 0 | 14525 | `		 VmHttpSplitURI(pUri,zPtr,nLen);` |
|      ! 0 | 14526 | `	 }` |
|        - | 14527 | `	 /* Jump trailing white spaces */` |
|      ! 0 | 14528 | `	 while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|      ! 0 | 14529 | `		 zIn++;` |
|      ! 0 | 14530 | `	 }` |
|        - | 14531 | `	 /* Extract the HTTP version */` |
|      ! 0 | 14532 | `	 zPtr = zIn;` |
|      ! 0 | 14533 | `	 while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|      ! 0 | 14534 | `		 zIn++;` |
|      ! 0 | 14535 | `	 }` |
|      ! 0 | 14536 | `	 *pProto = HTTP_PROTO_11; /* HTTP/1.1 */` |
|      ! 0 | 14537 | `	 rc = 1;` |
|      ! 0 | 14538 | `	 if( zIn > zPtr ){` |
|      ! 0 | 14539 | `		 rc = SyStrnicmp(zPtr,"http/1.0",(sxu32)(zIn-zPtr));` |
|      ! 0 | 14540 | `	 }` |
|      ! 0 | 14541 | `	 if( !rc ){` |
|      ! 0 | 14542 | `		 *pProto = HTTP_PROTO_10; /* HTTP/1.0 */` |
|      ! 0 | 14543 | `	 }` |
|      ! 0 | 14544 | `	 return SXRET_OK;` |
|      ! 0 | 14545 | ` }` |
|        - | 14546 | ` /*` |
|        - | 14547 | `  * Tokenize,decode and split a raw query encoded as: "x-www-form-urlencoded"` |
|        - | 14548 | `  * into a name value pair.` |
|        - | 14549 | `  * Note that this encoding is implicit in GET based requests.` |
|        - | 14550 | `  * After the tokenization process,register the decoded queries` |
|        - | 14551 | `  * in the $_GET/$_POST/$_REQUEST superglobals arrays.` |
|        - | 14552 | `  */` |
|      ! 0 | 14553 | ` static sxi32 VmHttpSplitEncodedQuery(` |
|        - | 14554 | `	 ph7_vm *pVm,       /* Target VM */` |
|        - | 14555 | `	 SyString *pQuery,  /* Raw query to decode */` |
|        - | 14556 | `	 SyBlob *pWorker,   /* Working buffer */` |
|        - | 14557 | `	 int is_post        /* TRUE if we are dealing with a POST request */` |
|        - | 14558 | `	 )` |
|      ! 0 | 14559 | ` {` |
|      ! 0 | 14560 | `	 const char *zEnd = &pQuery->zString[pQuery->nByte];` |
|      ! 0 | 14561 | `	 const char *zIn = pQuery->zString;` |
|        - | 14562 | `	 ph7_value *pGet,*pRequest;` |
|        - | 14563 | `	 SyString sName,sValue;` |
|        - | 14564 | `	 const char *zPtr;` |
|        - | 14565 | `	 sxu32 nBlobOfft;` |
|        - | 14566 | `	 /* Extract superglobals */` |
|      ! 0 | 14567 | `	 if( is_post ){` |
|        - | 14568 | `		 /* $_POST superglobal */` |
|      ! 0 | 14569 | `		 pGet = VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|      ! 0 | 14570 | `	 }else{` |
|        - | 14571 | `		 /* $_GET superglobal */` |
|      ! 0 | 14572 | `		 pGet = VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|        - | 14573 | `	 }` |
|      ! 0 | 14574 | `	 pRequest = VmExtractSuper(&(*pVm),"_REQUEST",sizeof("_REQUEST")-1);` |
|        - | 14575 | `	 /* Split up the raw query */` |
|      ! 0 | 14576 | `	 for(;;){` |
|        - | 14577 | `		 /* Jump leading white spaces */` |
|      ! 0 | 14578 | `		 while(zIn < zEnd  && SyisSpace(zIn[0]) ){` |
|      ! 0 | 14579 | `			 zIn++;` |
|      ! 0 | 14580 | `		 }` |
|      ! 0 | 14581 | `		 if( zIn >= zEnd ){` |
|      ! 0 | 14582 | `			 break;` |
|        - | 14583 | `		 }` |
|      ! 0 | 14584 | `		 zPtr = zIn;` |
|      ! 0 | 14585 | `		 while( zPtr < zEnd && zPtr[0] != '=' && zPtr[0] != '&' && zPtr[0] != ';' ){` |
|      ! 0 | 14586 | `			 zPtr++;` |
|      ! 0 | 14587 | `		 }` |
|        - | 14588 | `		 /* Reset the working buffer */` |
|      ! 0 | 14589 | `		 SyBlobReset(pWorker);` |
|        - | 14590 | `		 /* Decode the entry */` |
|      ! 0 | 14591 | `		 SyUriDecode(zIn,(sxu32)(zPtr-zIn),PH7_VmBlobConsumer,pWorker,TRUE);` |
|        - | 14592 | `		 /* Save the entry */` |
|      ! 0 | 14593 | `		 sName.nByte = SyBlobLength(pWorker);` |
|      ! 0 | 14594 | `		 sValue.zString = 0;` |
|      ! 0 | 14595 | `		 sValue.nByte = 0;` |
|      ! 0 | 14596 | `		 if( zPtr < zEnd && zPtr[0] == '=' ){` |
|      ! 0 | 14597 | `			 zPtr++;` |
|      ! 0 | 14598 | `			 zIn = zPtr;` |
|        - | 14599 | `			 /* Store field value */` |
|      ! 0 | 14600 | `			 while( zPtr < zEnd && zPtr[0] != '&' && zPtr[0] != ';' ){` |
|      ! 0 | 14601 | `				 zPtr++;` |
|      ! 0 | 14602 | `			 }` |
|      ! 0 | 14603 | `			 if( zPtr > zIn ){` |
|        - | 14604 | `				 /* Decode the value */` |
|      ! 0 | 14605 | `				  nBlobOfft = SyBlobLength(pWorker);` |
|      ! 0 | 14606 | `				  SyUriDecode(zIn,(sxu32)(zPtr-zIn),PH7_VmBlobConsumer,pWorker,TRUE);` |
|      ! 0 | 14607 | `				  sValue.zString = (const char *)SyBlobDataAt(pWorker,nBlobOfft);` |
|      ! 0 | 14608 | `				  sValue.nByte = SyBlobLength(pWorker) - nBlobOfft;` |
|        - | 14609 |  |
|      ! 0 | 14610 | `			 }` |
|        - | 14611 | `			 /* Synchronize pointers */` |
|      ! 0 | 14612 | `			 zIn = zPtr;` |
|      ! 0 | 14613 | `		 }` |
|      ! 0 | 14614 | `		 sName.zString = (const char *)SyBlobData(pWorker);` |
|        - | 14615 | `		 /* Install the decoded query in the $_GET/$_REQUEST array */` |
|      ! 0 | 14616 | `		 if( pGet && (pGet->iFlags & MEMOBJ_HASHMAP) ){` |
|      ! 0 | 14617 | `			 VmHashmapInsert((ph7_hashmap *)pGet->x.pOther,` |
|      ! 0 | 14618 | `				 sName.zString,(int)sName.nByte,` |
|      ! 0 | 14619 | `				 sValue.zString,(int)sValue.nByte` |
|        - | 14620 | `				 );` |
|      ! 0 | 14621 | `		 }` |
|      ! 0 | 14622 | `		 if( pRequest && (pRequest->iFlags & MEMOBJ_HASHMAP) ){` |
|      ! 0 | 14623 | `			 VmHashmapInsert((ph7_hashmap *)pRequest->x.pOther,` |
|      ! 0 | 14624 | `				 sName.zString,(int)sName.nByte,` |
|      ! 0 | 14625 | `				 sValue.zString,(int)sValue.nByte` |
|        - | 14626 | `					 );` |
|      ! 0 | 14627 | `		 }` |
|        - | 14628 | `		 /* Advance the pointer */` |
|      ! 0 | 14629 | `		 zIn = &zPtr[1];` |
|      ! 0 | 14630 | `	 }` |
|        - | 14631 | `	/* All done*/` |
|      ! 0 | 14632 | `	return SXRET_OK;` |
|      ! 0 | 14633 | ` }` |
|        - | 14634 | ` /*` |
|        - | 14635 | `  * Extract MIME header value from the given set.` |
|        - | 14636 | `  * Return header value on success. NULL otherwise.` |
|        - | 14637 | `  */` |
|      ! 0 | 14638 | ` static SyString * VmHttpExtractHeaderValue(SySet *pSet,const char *zMime,sxu32 nByte)` |
|      ! 0 | 14639 | ` {` |
|        - | 14640 | `	 SyhttpHeader *aMime,*pMime;` |
|        - | 14641 | `	 SyString sMime;` |
|        - | 14642 | `	 sxu32 n;` |
|      ! 0 | 14643 | `	 SyStringInitFromBuf(&sMime,zMime,nByte);` |
|        - | 14644 | `	 /* Point to the MIME entries */` |
|      ! 0 | 14645 | `	 aMime = (SyhttpHeader *)SySetBasePtr(pSet);` |
|        - | 14646 | `	 /* Perform the lookup */` |
|      ! 0 | 14647 | `	 for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|      ! 0 | 14648 | `		 pMime = &aMime[n];` |
|      ! 0 | 14649 | `		 if( SyStringCmp(&sMime,&pMime->sName,SyStrnicmp) == 0 ){` |
|        - | 14650 | `			 /* Header found,return it's associated value */` |
|      ! 0 | 14651 | `			 return &pMime->sValue;` |
|        - | 14652 | `		 }` |
|      ! 0 | 14653 | `	 }` |
|        - | 14654 | `	 /* No such MIME header */` |
|      ! 0 | 14655 | `	 return 0;` |
|      ! 0 | 14656 | ` }` |
|        - | 14657 | ` /*` |
|        - | 14658 | `  * Tokenize and decode a raw "Cookie:" MIME header into a name value pair` |
|        - | 14659 | `  * and insert it's fields [i.e name,value] in the $_COOKIE superglobal.` |
|        - | 14660 | `  */` |
|      ! 0 | 14661 | ` static sxi32 VmHttpPorcessCookie(ph7_vm *pVm,SyBlob *pWorker,const char *zIn,sxu32 nByte)` |
|      ! 0 | 14662 | ` {` |
|      ! 0 | 14663 | `	 const char *zPtr,*zDelimiter,*zEnd = &zIn[nByte];` |
|        - | 14664 | `	 SyString sName,sValue;` |
|        - | 14665 | `	 ph7_value *pCookie;` |
|        - | 14666 | `	 sxu32 nOfft;` |
|        - | 14667 | `	 /* Make sure the $_COOKIE superglobal is available */` |
|      ! 0 | 14668 | `	 pCookie = VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 14669 | `	 if( pCookie == 0 \|\| (pCookie->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 14670 | `		 /* $_COOKIE superglobal not available */` |
|      ! 0 | 14671 | `		 return SXERR_NOTFOUND;` |
|        - | 14672 | `	 }` |
|      ! 0 | 14673 | `	 for(;;){` |
|        - | 14674 | `		  /* Jump leading white spaces */` |
|      ! 0 | 14675 | `		 while( zIn < zEnd && SyisSpace(zIn[0]) ){` |
|      ! 0 | 14676 | `			 zIn++;` |
|      ! 0 | 14677 | `		 }` |
|      ! 0 | 14678 | `		 if( zIn >= zEnd ){` |
|      ! 0 | 14679 | `			 break;` |
|        - | 14680 | `		 }` |
|        - | 14681 | `		  /* Reset the working buffer */` |
|      ! 0 | 14682 | `		 SyBlobReset(pWorker);` |
|      ! 0 | 14683 | `		 zDelimiter = zIn;` |
|        - | 14684 | `		 /* Delimit the name[=value]; pair */` |
|      ! 0 | 14685 | `		 while( zDelimiter < zEnd && zDelimiter[0] != ';' ){` |
|      ! 0 | 14686 | `			 zDelimiter++;` |
|      ! 0 | 14687 | `		 }` |
|      ! 0 | 14688 | `		 zPtr = zIn;` |
|      ! 0 | 14689 | `		 while( zPtr < zDelimiter && zPtr[0] != '=' ){` |
|      ! 0 | 14690 | `			 zPtr++;` |
|      ! 0 | 14691 | `		 }` |
|        - | 14692 | `		 /* Decode the cookie */` |
|      ! 0 | 14693 | `		 SyUriDecode(zIn,(sxu32)(zPtr-zIn),PH7_VmBlobConsumer,pWorker,TRUE);` |
|      ! 0 | 14694 | `		 sName.nByte = SyBlobLength(pWorker);` |
|      ! 0 | 14695 | `		 zPtr++;` |
|      ! 0 | 14696 | `		 sValue.zString = 0;` |
|      ! 0 | 14697 | `		 sValue.nByte = 0;` |
|      ! 0 | 14698 | `		 if( zPtr < zDelimiter ){` |
|        - | 14699 | `			 /* Got a Cookie value */` |
|      ! 0 | 14700 | `			 nOfft = SyBlobLength(pWorker);` |
|      ! 0 | 14701 | `			 SyUriDecode(zPtr,(sxu32)(zDelimiter-zPtr),PH7_VmBlobConsumer,pWorker,TRUE);` |
|      ! 0 | 14702 | `			 SyStringInitFromBuf(&sValue,SyBlobDataAt(pWorker,nOfft),SyBlobLength(pWorker)-nOfft);` |
|      ! 0 | 14703 | `		 }` |
|        - | 14704 | `		 /* Synchronize pointers */` |
|      ! 0 | 14705 | `		 zIn = &zDelimiter[1];` |
|        - | 14706 | `		 /* Perform the insertion */` |
|      ! 0 | 14707 | `		 sName.zString = (const char *)SyBlobData(pWorker);` |
|      ! 0 | 14708 | `		 VmHashmapInsert((ph7_hashmap *)pCookie->x.pOther,` |
|      ! 0 | 14709 | `			 sName.zString,(int)sName.nByte,` |
|      ! 0 | 14710 | `			 sValue.zString,(int)sValue.nByte` |
|        - | 14711 | `			 );` |
|      ! 0 | 14712 | `	 }` |
|      ! 0 | 14713 | `	 return SXRET_OK;` |
|      ! 0 | 14714 | ` }` |
|        - | 14715 | ` /*` |
|        - | 14716 | `  * Process a full HTTP request and populate the appropriate arrays` |
|        - | 14717 | `  * such as $_SERVER,$_GET,$_POST,$_COOKIE,$_REQUEST,... with the information` |
|        - | 14718 | `  * extracted from the raw HTTP request. As an extension Symisc introduced` |
|        - | 14719 | `  * the $_HEADER array which hold a copy of the processed HTTP MIME headers` |
|        - | 14720 | `  * and their associated values. [i.e: $_HEADER['Server'],$_HEADER['User-Agent'],...].` |
|        - | 14721 | `  * This function return SXRET_OK on success. Any other return value indicates` |
|        - | 14722 | `  * a malformed HTTP request.` |
|        - | 14723 | `  */` |
|      ! 0 | 14724 | ` static sxi32 VmHttpProcessRequest(ph7_vm *pVm,const char *zRequest,int nByte)` |
|      ! 0 | 14725 | ` {` |
|        - | 14726 | `	 SyString *pName,*pValue,sRequest; /* Raw HTTP request */` |
|        - | 14727 | `	 ph7_value *pHeaderArray;          /* $_HEADER superglobal (Symisc eXtension to the PHP specification)*/` |
|        - | 14728 | `	 SyhttpHeader *pHeader;            /* MIME header */` |
|        - | 14729 | `	 SyhttpUri sUri;     /* Parse of the raw URI*/` |
|        - | 14730 | `	 SyBlob sWorker;     /* General purpose working buffer */` |
|        - | 14731 | `	 SySet sHeader;      /* MIME headers set */` |
|        - | 14732 | `	 sxi32 iMethod;      /* HTTP method [i.e: GET,POST,HEAD...]*/` |
|        - | 14733 | `	 sxi32 iVer;         /* HTTP protocol version */` |
|        - | 14734 | `	 sxi32 rc;` |
|      ! 0 | 14735 | `	 SyStringInitFromBuf(&sRequest,zRequest,nByte);` |
|      ! 0 | 14736 | `	 SySetInit(&sHeader,&pVm->sAllocator,sizeof(SyhttpHeader));` |
|      ! 0 | 14737 | `	 SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        - | 14738 | `	 /* Ignore leading and trailing white spaces*/` |
|      ! 0 | 14739 | `	 SyStringFullTrim(&sRequest);` |
|        - | 14740 | `	 /* Process the first line */` |
|      ! 0 | 14741 | `	 rc = VmHttpProcessFirstLine(&sRequest,&iMethod,&sUri,&iVer);` |
|      ! 0 | 14742 | `	 if( rc != SXRET_OK ){` |
|      ! 0 | 14743 | `		 return rc;` |
|        - | 14744 | `	 }` |
|        - | 14745 | `	 /* Process MIME headers */` |
|      ! 0 | 14746 | `	 VmHttpExtractHeaders(&sRequest,&sHeader);` |
|        - | 14747 | `	 /*` |
|        - | 14748 | `	  * Setup $_SERVER environments` |
|        - | 14749 | `	  */` |
|        - | 14750 | `	 /* 'SERVER_PROTOCOL': Name and revision of the information protocol via which the page was requested */` |
|      ! 0 | 14751 | `	 ph7_vm_config(pVm,` |
|        - | 14752 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 14753 | `		 "SERVER_PROTOCOL",` |
|      ! 0 | 14754 | `		 iVer == HTTP_PROTO_10 ? "HTTP/1.0" : "HTTP/1.1",` |
|        - | 14755 | `		 sizeof("HTTP/1.1")-1` |
|        - | 14756 | `		 );` |
|        - | 14757 | `	 /* 'REQUEST_METHOD':  Which request method was used to access the page */` |
|      ! 0 | 14758 | `	 ph7_vm_config(pVm,` |
|        - | 14759 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 14760 | `		 "REQUEST_METHOD",` |
|      ! 0 | 14761 | `		 iMethod == HTTP_METHOD_GET ?   "GET" :` |
|      ! 0 | 14762 | `		 (iMethod == HTTP_METHOD_POST ? "POST":` |
|      ! 0 | 14763 | `		 (iMethod == HTTP_METHOD_PUT  ? "PUT" :` |
|      ! 0 | 14764 | `		 (iMethod == HTTP_METHOD_HEAD ?  "HEAD" : "OTHER"))),` |
|        - | 14765 | `		 -1 /* Compute attribute length automatically */` |
|        - | 14766 | `		 );` |
|      ! 0 | 14767 | `	 if( SyStringLength(&sUri.sQuery) > 0 && iMethod == HTTP_METHOD_GET ){` |
|      ! 0 | 14768 | `		 pValue = &sUri.sQuery;` |
|        - | 14769 | `		 /* 'QUERY_STRING': The query string, if any, via which the page was accessed */` |
|      ! 0 | 14770 | `		 ph7_vm_config(pVm,` |
|        - | 14771 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 14772 | `			 "QUERY_STRING",` |
|      ! 0 | 14773 | `			 pValue->zString,` |
|      ! 0 | 14774 | `			 pValue->nByte` |
|        - | 14775 | `			 );` |
|        - | 14776 | `		 /* Decoded the raw query */` |
|      ! 0 | 14777 | `		 VmHttpSplitEncodedQuery(&(*pVm),pValue,&sWorker,FALSE);` |
|      ! 0 | 14778 | `	 }` |
|        - | 14779 | `	 /* REQUEST_URI: The URI which was given in order to access this page; for instance, '/index.html' */` |
|      ! 0 | 14780 | `	 pValue = &sUri.sRaw;` |
|      ! 0 | 14781 | `	 ph7_vm_config(pVm,` |
|        - | 14782 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 14783 | `		 "REQUEST_URI",` |
|      ! 0 | 14784 | `		 pValue->zString,` |
|      ! 0 | 14785 | `		 pValue->nByte` |
|        - | 14786 | `		 );` |
|        - | 14787 | `	 /*` |
|        - | 14788 | `	  * 'PATH_INFO'` |
|        - | 14789 | `	  * 'ORIG_PATH_INFO'` |
|        - | 14790 | `      * Contains any client-provided pathname information trailing the actual script filename but preceding` |
|        - | 14791 | `	  * the query string, if available. For instance, if the current script was accessed via the URL` |
|        - | 14792 | `	  * http://www.example.com/php/path_info.php/some/stuff?foo=bar, then $_SERVER['PATH_INFO'] would contain` |
|        - | 14793 | `	  * /some/stuff.` |
|        - | 14794 | `	  */` |
|      ! 0 | 14795 | `	 pValue = &sUri.sPath;` |
|      ! 0 | 14796 | `	 ph7_vm_config(pVm,` |
|        - | 14797 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 14798 | `		 "PATH_INFO",` |
|      ! 0 | 14799 | `		 pValue->zString,` |
|      ! 0 | 14800 | `		 pValue->nByte` |
|        - | 14801 | `		 );` |
|      ! 0 | 14802 | `	 ph7_vm_config(pVm,` |
|        - | 14803 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 14804 | `		 "ORIG_PATH_INFO",` |
|      ! 0 | 14805 | `		 pValue->zString,` |
|      ! 0 | 14806 | `		 pValue->nByte` |
|        - | 14807 | `		 );` |
|        - | 14808 | `	 /* 'HTTP_ACCEPT': Contents of the Accept: header from the current request, if there is one */` |
|      ! 0 | 14809 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept",sizeof("Accept")-1);` |
|      ! 0 | 14810 | `	 if( pValue ){` |
|      ! 0 | 14811 | `		 ph7_vm_config(pVm,` |
|        - | 14812 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 14813 | `			 "HTTP_ACCEPT",` |
|      ! 0 | 14814 | `			 pValue->zString,` |
|      ! 0 | 14815 | `			 pValue->nByte` |
|        - | 14816 | `		 );` |
|      ! 0 | 14817 | `	 }` |
|        - | 14818 | `	 /* 'HTTP_ACCEPT_CHARSET': Contents of the Accept-Charset: header from the current request, if there is one. */` |
|      ! 0 | 14819 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept-Charset",sizeof("Accept-Charset")-1);` |
|      ! 0 | 14820 | `	 if( pValue ){` |
|      ! 0 | 14821 | `		 ph7_vm_config(pVm,` |
|        - | 14822 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 14823 | `			 "HTTP_ACCEPT_CHARSET",` |
|      ! 0 | 14824 | `			 pValue->zString,` |
|      ! 0 | 14825 | `			 pValue->nByte` |
|        - | 14826 | `		 );` |
|      ! 0 | 14827 | `	 }` |
|        - | 14828 | `	 /* 'HTTP_ACCEPT_ENCODING': Contents of the Accept-Encoding: header from the current request, if there is one. */` |
|      ! 0 | 14829 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept-Encoding",sizeof("Accept-Encoding")-1);` |
|      ! 0 | 14830 | `	 if( pValue ){` |
|      ! 0 | 14831 | `		 ph7_vm_config(pVm,` |
|        - | 14832 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 14833 | `			 "HTTP_ACCEPT_ENCODING",` |
|      ! 0 | 14834 | `			 pValue->zString,` |
|      ! 0 | 14835 | `			 pValue->nByte` |
|        - | 14836 | `		 );` |
|      ! 0 | 14837 | `	 }` |
|        - | 14838 | `	  /* 'HTTP_ACCEPT_LANGUAGE': Contents of the Accept-Language: header from the current request, if there is one */` |
|      ! 0 | 14839 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept-Language",sizeof("Accept-Language")-1);` |
|      ! 0 | 14840 | `	 if( pValue ){` |
|      ! 0 | 14841 | `		 ph7_vm_config(pVm,` |
|        - | 14842 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 14843 | `			 "HTTP_ACCEPT_LANGUAGE",` |
|      ! 0 | 14844 | `			 pValue->zString,` |
|      ! 0 | 14845 | `			 pValue->nByte` |
|        - | 14846 | `		 );` |
|      ! 0 | 14847 | `	 }` |
|        - | 14848 | `	 /* 'HTTP_CONNECTION': Contents of the Connection: header from the current request, if there is one. */` |
|      ! 0 | 14849 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Connection",sizeof("Connection")-1);` |
|      ! 0 | 14850 | `	 if( pValue ){` |
|      ! 0 | 14851 | `		 ph7_vm_config(pVm,` |
|        - | 14852 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 14853 | `			 "HTTP_CONNECTION",` |
|      ! 0 | 14854 | `			 pValue->zString,` |
|      ! 0 | 14855 | `			 pValue->nByte` |
|        - | 14856 | `		 );` |
|      ! 0 | 14857 | `	 }` |
|        - | 14858 | `	 /* 'HTTP_HOST': Contents of the Host: header from the current request, if there is one. */` |
|      ! 0 | 14859 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Host",sizeof("Host")-1);` |
|      ! 0 | 14860 | `	 if( pValue ){` |
|      ! 0 | 14861 | `		 ph7_vm_config(pVm,` |
|        - | 14862 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 14863 | `			 "HTTP_HOST",` |
|      ! 0 | 14864 | `			 pValue->zString,` |
|      ! 0 | 14865 | `			 pValue->nByte` |
|        - | 14866 | `		 );` |
|      ! 0 | 14867 | `	 }` |
|        - | 14868 | `	 /* 'HTTP_REFERER': Contents of the Referer: header from the current request, if there is one. */` |
|      ! 0 | 14869 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Referer",sizeof("Referer")-1);` |
|      ! 0 | 14870 | `	 if( pValue ){` |
|      ! 0 | 14871 | `		 ph7_vm_config(pVm,` |
|        - | 14872 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 14873 | `			 "HTTP_REFERER",` |
|      ! 0 | 14874 | `			 pValue->zString,` |
|      ! 0 | 14875 | `			 pValue->nByte` |
|        - | 14876 | `		 );` |
|      ! 0 | 14877 | `	 }` |
|        - | 14878 | `	 /* 'HTTP_USER_AGENT': Contents of the Referer: header from the current request, if there is one. */` |
|      ! 0 | 14879 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"User-Agent",sizeof("User-Agent")-1);` |
|      ! 0 | 14880 | `	 if( pValue ){` |
|      ! 0 | 14881 | `		 ph7_vm_config(pVm,` |
|        - | 14882 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 14883 | `			 "HTTP_USER_AGENT",` |
|      ! 0 | 14884 | `			 pValue->zString,` |
|      ! 0 | 14885 | `			 pValue->nByte` |
|        - | 14886 | `		 );` |
|      ! 0 | 14887 | `	 }` |
|        - | 14888 | `	  /* 'PHP_AUTH_DIGEST': When doing Digest HTTP authentication this variable is set to the 'Authorization'` |
|        - | 14889 | `	   * header sent by the client (which you should then use to make the appropriate validation).` |
|        - | 14890 | `	   */` |
|      ! 0 | 14891 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Authorization",sizeof("Authorization")-1);` |
|      ! 0 | 14892 | `	 if( pValue ){` |
|      ! 0 | 14893 | `		 ph7_vm_config(pVm,` |
|        - | 14894 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 14895 | `			 "PHP_AUTH_DIGEST",` |
|      ! 0 | 14896 | `			 pValue->zString,` |
|      ! 0 | 14897 | `			 pValue->nByte` |
|        - | 14898 | `		 );` |
|      ! 0 | 14899 | `		 ph7_vm_config(pVm,` |
|        - | 14900 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 14901 | `			 "PHP_AUTH",` |
|      ! 0 | 14902 | `			 pValue->zString,` |
|      ! 0 | 14903 | `			 pValue->nByte` |
|        - | 14904 | `		 );` |
|      ! 0 | 14905 | `	 }` |
|        - | 14906 | `	 /* Install all clients HTTP headers in the $_HEADER superglobal */` |
|      ! 0 | 14907 | `	 pHeaderArray = VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|        - | 14908 | `	 /* Iterate throw the available MIME headers*/` |
|      ! 0 | 14909 | `	 SySetResetCursor(&sHeader);` |
|      ! 0 | 14910 | `	 pHeader = 0; /* stupid cc warning */` |
|      ! 0 | 14911 | `	 while( SXRET_OK == SySetGetNextEntry(&sHeader,(void **)&pHeader) ){` |
|      ! 0 | 14912 | `		 pName  = &pHeader->sName;` |
|      ! 0 | 14913 | `		 pValue = &pHeader->sValue;` |
|      ! 0 | 14914 | `		 if( pHeaderArray && (pHeaderArray->iFlags & MEMOBJ_HASHMAP)){` |
|        - | 14915 | `			 /* Insert the MIME header and it's associated value */` |
|      ! 0 | 14916 | `			 VmHashmapInsert((ph7_hashmap *)pHeaderArray->x.pOther,` |
|      ! 0 | 14917 | `				 pName->zString,(int)pName->nByte,` |
|      ! 0 | 14918 | `				 pValue->zString,(int)pValue->nByte` |
|        - | 14919 | `				 );` |
|      ! 0 | 14920 | `		 }` |
|      ! 0 | 14921 | `		 if( pName->nByte == sizeof("Cookie")-1 && SyStrnicmp(pName->zString,"Cookie",sizeof("Cookie")-1) == 0` |
|      ! 0 | 14922 | `			 && pValue->nByte > 0){` |
|        - | 14923 | `				 /* Process the name=value pair and insert them in the $_COOKIE superglobal array */` |
|      ! 0 | 14924 | `				 VmHttpPorcessCookie(&(*pVm),&sWorker,pValue->zString,pValue->nByte);` |
|      ! 0 | 14925 | `		 }` |
|      ! 0 | 14926 | `	 }` |
|      ! 0 | 14927 | `	 if( iMethod == HTTP_METHOD_POST ){` |
|        - | 14928 | `		 /* Extract raw POST data */` |
|      ! 0 | 14929 | `		 pValue = VmHttpExtractHeaderValue(&sHeader,"Content-Type",sizeof("Content-Type") - 1);` |
|      ! 0 | 14930 | `		 if( pValue && pValue->nByte >= sizeof("application/x-www-form-urlencoded") - 1 &&` |
|      ! 0 | 14931 | `			 SyMemcmp("application/x-www-form-urlencoded",pValue->zString,pValue->nByte) == 0 ){` |
|        - | 14932 | `				 /* Extract POST data length */` |
|      ! 0 | 14933 | `				 pValue = VmHttpExtractHeaderValue(&sHeader,"Content-Length",sizeof("Content-Length") - 1);` |
|      ! 0 | 14934 | `				 if( pValue ){` |
|      ! 0 | 14935 | `					 sxi32 iLen = 0; /* POST data length */` |
|      ! 0 | 14936 | `					 SyStrToInt32(pValue->zString,pValue->nByte,(void *)&iLen,0);` |
|      ! 0 | 14937 | `					 if( iLen > 0 ){` |
|        - | 14938 | `						 /* Remove leading and trailing white spaces */` |
|      ! 0 | 14939 | `						 SyStringFullTrim(&sRequest);` |
|      ! 0 | 14940 | `						 if( (int)sRequest.nByte > iLen ){` |
|      ! 0 | 14941 | `							 sRequest.nByte = (sxu32)iLen;` |
|      ! 0 | 14942 | `						 }` |
|        - | 14943 | `						 /* Decode POST data now */` |
|      ! 0 | 14944 | `						 VmHttpSplitEncodedQuery(&(*pVm),&sRequest,&sWorker,TRUE);` |
|      ! 0 | 14945 | `					 }` |
|      ! 0 | 14946 | `				 }` |
|      ! 0 | 14947 | `		 }` |
|      ! 0 | 14948 | `	 }` |
|        - | 14949 | `	 /* All done,clean-up the mess left behind */` |
|      ! 0 | 14950 | `	 SySetRelease(&sHeader);` |
|      ! 0 | 14951 | `	 SyBlobRelease(&sWorker);` |
|      ! 0 | 14952 | `	 return SXRET_OK;` |
|      ! 0 | 14953 | ` }` |
|        - | 14954 |  |
