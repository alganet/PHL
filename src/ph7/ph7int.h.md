# src/ph7/ph7int.h

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 53/53 lines (100.00%)

[Root index](../../index.md) | [Directory index](index.md)

|      Hits | Line | Source |
| --------: | ---: | :--- |
|         - |    1 | `/**` |
|         - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|         - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|         - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|         - |    5 | ` */` |
|         - |    6 | `#ifndef __PH7INT_H__` |
|         - |    7 | `#define __PH7INT_H__` |
|         - |    8 | `/* Internal interface definitions for PH7. */` |
|         - |    9 | `#define PH7_PRIVATE` |
|         - |   10 | `#include "ph7.h"` |
|         - |   11 |  |
|         - |   12 | `/* Return a human-readable PHP type name for a memory object value. */` |
|         - |   13 | `PH7_PRIVATE const char *ph7_type_name(ph7_value *pVal);` |
|         - |   14 |  |
|         - |   15 | `/* Granular SX library includes */` |
|         - |   16 | `#include "sxtypes.h"      /* Base types: sxi32, sxu32, sxptr, sxreal, etc. */` |
|         - |   17 | `#include "sxmacros.h"     /* SyString macros, linked list macros, byte operations */` |
|         - |   18 | `#include "sxset.h"        /* SySet and SyBlob structures */` |
|         - |   19 | `#include "sxmem.h"        /* SyMemBackend, SyMemBlock, SyMemHeader */` |
|         - |   20 | `#include "sxmutex.h"      /* Mutex types and macros */` |
|         - |   21 | `#include "sxhash.h"       /* Hash functions: SyBinHash, SyStrHash */` |
|         - |   22 | `#include "sxhashtable.h"  /* SyHash, SyHashEntry structures */` |
|         - |   23 | `#include "sxrand.h"       /* SyPRNGCtx structure */` |
|         - |   24 | `#include "sxlex.h"        /* SyLex, SyToken, SyStream structures */` |
|         - |   25 | `#include "sxfmt.h"        /* Formatting functions */` |
|         - |   26 | `#include "sxstr.h"        /* String functions */` |
|         - |   27 | `#include "sxutils.h"      /* Numeric parsing functions */` |
|         - |   28 | `#include "sxbase64.h"     /* Base64 encode/decode */` |
|         - |   29 | `#include "sxuri.h"        /* URI encode/decode */` |
|         - |   30 | `#include "sxtime.h"       /* Time utilities */` |
|         - |   31 | `#include "sxdigest.h"     /* MD5Context, SHA1Context, digest functions */` |
|         - |   32 | `#include "sxblowfish.h"   /* bcrypt (Blowfish) password hashing */` |
|         - |   33 | `#include "sxcrypt.h"      /* Unix crypt(3): DES/MD5/SHA-crypt behind crypt() */` |
|         - |   34 | `#include "sxargon2.h"     /* Argon2i/id (RFC 9106) behind password_hash() */` |
|         - |   35 |  |
|         - |   36 | `#ifndef PH7_PI` |
|         - |   37 | `/* Value of PI */` |
|         - |   38 | `/* pi to DOUBLE precision. It used to be 3.1415926535898 -- only 14 significant` |
|         - |   39 | ` * digits -- so M_PI differed from php's in the 13th place and every trig result` |
|         - |   40 | ` * built on it was quietly off (rad2deg(M_PI) gave 180.0000000000004). */` |
|         - |   41 | `#define PH7_PI 3.14159265358979323846` |
|         - |   42 | `#endif` |
|         - |   43 | `/* Uncaught/in-flight exception code value. A foreign function (built-in) that` |
|         - |   44 | ` * propagates a callback-raised exception returns this so the OP_CALL dispatcher` |
|         - |   45 | ` * unwinds through the nearest try/catch. */` |
|         - |   46 | `#define PH7_EXCEPTION -255` |
|         - |   47 | `/*` |
|         - |   48 | ` * Constants for the largest and smallest possible 64-bit signed integers.` |
|         - |   49 | ` * These macros are designed to work correctly on both 32-bit and 64-bit` |
|         - |   50 | ` * compilers.` |
|         - |   51 | ` */` |
|         - |   52 | `#ifndef LARGEST_INT64` |
|         - |   53 | `#define LARGEST_INT64  (0xffffffff\|(((sxi64)0x7fffffff)<<32))` |
|         - |   54 | `#endif` |
|         - |   55 | `#ifndef SMALLEST_INT64` |
|         - |   56 | `#define SMALLEST_INT64 (((sxi64)-1) - LARGEST_INT64)` |
|         - |   57 | `#endif` |
|         - |   58 | `/* Maximum input size for ph7_compile() in bytes. Override at build time with` |
|         - |   59 | ` * -DPH7_MAX_INPUT_SIZE=N (e.g. a smaller value for embedded/tiny targets).` |
|         - |   60 | ` * Runtime override: ph7_config(engine, PH7_CONFIG_MAX_INPUT, n). */` |
|         - |   61 | `#ifndef PH7_MAX_INPUT_SIZE` |
|         - |   62 | `#define PH7_MAX_INPUT_SIZE (64u*1024u*1024u)` |
|         - |   63 | `#endif` |
|         - |   64 | `/* Forward declaration of private structures */` |
|         - |   65 | `typedef struct ph7_class_instance ph7_class_instance;` |
|         - |   66 | `typedef struct ph7_foreach_info   ph7_foreach_info;` |
|         - |   67 | `typedef struct ph7_foreach_step   ph7_foreach_step;` |
|         - |   68 | `typedef struct ph7_hashmap_node   ph7_hashmap_node;` |
|         - |   69 | `typedef struct ph7_hashmap        ph7_hashmap;` |
|         - |   70 | `typedef struct ph7_class          ph7_class;` |
|         - |   71 | `typedef struct PH7_NativeIterVtab PH7_NativeIterVtab;` |
|         - |   72 |  |
|         - |   73 |  |
|         - |   74 | `/* PH7 private declaration */` |
|         - |   75 | `/*` |
|         - |   76 | ` * Memory Objects.` |
|         - |   77 | ` * Internally, the PH7 virtual machine manipulates nearly all PHP values` |
|         - |   78 | ` * [i.e: string, int, float, resource, object, bool, null] as ph7_values structures.` |
|         - |   79 | ` * Each ph7_values struct may cache multiple representations (string, integer etc.)` |
|         - |   80 | ` * of the same value.` |
|         - |   81 | ` */` |
|         - |   82 | `struct ph7_value` |
|         - |   83 | `{` |
|         - |   84 | `	ph7_real rVal;      /* Real value */` |
|         - |   85 | `	union{` |
|         - |   86 | `		sxi64 iVal;     /* Integer value */` |
|         - |   87 | `		void *pOther;   /* Other values (Object, Array, Resource, Namespace, etc.) */` |
|         - |   88 | `	}x;` |
|         - |   89 | `	/* iFlags and nIdx are adjacent on purpose: each is four bytes and each used to` |
|         - |   90 | `	 * sit alone in front of a pointer, so the struct carried eight bytes of padding` |
|         - |   91 | `	 * it did nothing with. This is the engine's per-VALUE size -- every variable,` |
|         - |   92 | `	 * every array element and every operand-stack cell is one -- so those eight` |
|         - |   93 | `	 * bytes were 11% of it. Order only; no field changed meaning. */` |
|         - |   94 | `	sxi32 iFlags;       /* Control flags (see below) */` |
|         - |   95 | `	sxu32 nIdx;         /* Index number of this entry in the global object allocator */` |
|         - |   96 | `	ph7_vm *pVm;        /* Virtual machine that own this instance */` |
|         - |   97 | `	SyBlob sBlob;       /* String values */` |
|         - |   98 | `};` |
|         - |   99 | `/*` |
|         - |  100 | ` * Copy the SCALAR half of a value -- rVal, x and iFlags, the three leading fields --` |
|         - |  101 | ` * leaving nIdx, pVm and sBlob as the destination already had them. That is what every` |
|         - |  102 | ` * value copy in the engine starts with (PH7_MemObjLoad and PH7_MemObjStore both do it,` |
|         - |  103 | ` * and between them they run on nearly every instruction).` |
|         - |  104 | ` *` |
|         - |  105 | `` * Written out field by field rather than as one SyMemcpy of `sizeof(ph7_value) -`` |
|         - |  106 | `` * (sizeof(ph7_vm*) + sizeof(SyBlob) + sizeof(sxu32))`, which is what it used to be. That`` |
|         - |  107 | ` * spelling was exact -- the three fields ARE the first twenty bytes -- but it went out` |
|         - |  108 | `` * of line into SyMemcpy and from there into the C library's `memcpy`, whose`` |
|         - |  109 | ` * AVX-with-ERMS entry sequence costs far more than the twenty bytes it moves: a phpcs` |
|         - |  110 | `` * profile put `__memcpy_avx_unaligned_erms` at 6.7% of the run with two thirds of it`` |
|         - |  111 | ` * arriving from these two callers. Three assignments compile to three loads and three` |
|         - |  112 | ` * stores with no call at all, and they say what is copied instead of computing it from` |
|         - |  113 | ` * the size of what is not.` |
|         - |  114 | ` */` |
|         - |  115 | `#define PH7_MEMOBJ_COPY_SCALAR(pDst,pSrc) \` |
|         - |  116 | `	do{ (pDst)->rVal = (pSrc)->rVal; (pDst)->x = (pSrc)->x; (pDst)->iFlags = (pSrc)->iFlags; }while(0)` |
|         - |  117 | `/*` |
|         - |  118 | ` * What one PH7_OP_CALL site last learned about its own callee.` |
|         - |  119 | ` *` |
|         - |  120 | ` * Resolving a call's NAME is the single most expensive thing the dispatch loop does:` |
|         - |  121 | ` * php's order is the user function table first and the host table second, and a call` |
|         - |  122 | ` * written inside a namespace is spelled QUALIFIED by the compiler, so an ordinary` |
|         - |  123 | `` * `count($a)` in a namespaced file asks four case-insensitive hash questions --`` |
|         - |  124 | `` * `Ns\count` and `count` of hFunction, then the same pair of hHostFunction -- before`` |
|         - |  125 | ` * anything runs. Measured on the ecosystem gate's phpcs step that was 9.2% of the whole` |
|         - |  126 | ` * run, and it is the same four answers every time the site executes.` |
|         - |  127 | ` *` |
|         - |  128 | ` * So each site remembers them. The record is keyed by the callee name it answers for` |
|         - |  129 | ` * (the interned copy in ph7_vm::hCallName, compared byte for byte -- NOT by the name's` |
|         - |  130 | ` * ADDRESS, which a freed and reused heap block could repeat) and stamped with the` |
|         - |  131 | ` * pVm->nCallableGen it was resolved at, so declaring anything retires every site at` |
|         - |  132 | ` * once, and a stale pEntry is never even read. A site whose callee is a VARIABLE would` |
|         - |  133 | ` * thrash this, so the name is recorded ONCE: a site that later asks about a different` |
|         - |  134 | ` * name is marked dead and resolves the long way for ever after.` |
|         - |  135 | ` *` |
|         - |  136 | ` * What is cached is the table ENTRY, never the ph7_vm_func behind it: installing an` |
|         - |  137 | ` * overload re-points an existing entry's pUserData without touching the entry, and` |
|         - |  138 | ` * without bumping the generation.` |
|         - |  139 | ` */` |
|         - |  140 | `typedef struct VmCallSite VmCallSite;` |
|         - |  141 | `struct VmCallSite` |
|         - |  142 | `{` |
|         - |  143 | `	/* One record per BYTECODE SITE that has resolved a name and may resolve it again.` |
|         - |  144 | `	 * PH7_OP_CALL owns most of them (see VmCallSiteFor); PH7_OP_LOADC owns the rest` |
|         - |  145 | `	 * (PH7_VmConstSiteAnswer), and uses only pEntry, nGen and nNextFree -- a LOADC's` |
|         - |  146 | `	 * name is fixed by its instruction, so there is nothing for zName to guard. One` |
|         - |  147 | `	 * set means one free list and one release sweep for both. */` |
|         - |  148 | `	const char *zName;   /* the callee name this record answers for (interned in hCallName,` |
|         - |  149 | `	                      * which owns it -- a record never frees it); 0 for a LOADC site */` |
|         - |  150 | `	SyHashEntry *pEntry; /* the entry it resolved to */` |
|         - |  151 | `	sxu32 nName;         /* zName length */` |
|         - |  152 | `	sxu32 nGen;          /* pVm->nCallableGen it was resolved at (0 = not resolved yet) */` |
|         - |  153 | `	sxu32 nNextFree;     /* free-list link (index + 1) while this record is unclaimed */` |
|         - |  154 | `	sxu8 bHost;          /* 1 = pEntry is in hHostFunction, 0 = in hFunction */` |
|         - |  155 | `	sxu8 bEngine;        /* the bEngineCallee the answer was resolved under */` |
|         - |  156 | `	sxu8 bDead;          /* 1 = this site has seen more than one name; never cache it */` |
|         - |  157 | `};` |
|         - |  158 | `/* One declaration a compile in flight has installed (ph7_vm::aUnitDecl). php compiles a` |
|         - |  159 | ` * unit whole before it binds anything, so an eval() or include that fails to PARSE` |
|         - |  160 | ` * declares none of the functions, classes, interfaces or traits written in it -- where` |
|         - |  161 | ` * PHL hoists each one as its declaration compiles, and the ones above the error stayed` |
|         - |  162 | ` * callable (and made the next correct declaration of the name a redeclaration fatal). */` |
|         - |  163 | `typedef struct VmUnitDecl VmUnitDecl;` |
|         - |  164 | `struct VmUnitDecl` |
|         - |  165 | `{` |
|         - |  166 | `	void *pDecl;   /* the ph7_vm_func or ph7_class installed */` |
|         - |  167 | `	SyString sName;/* the name it was installed under */` |
|         - |  168 | `	sxu8 bClass;   /* 1 = pDecl is a ph7_class in hClass, 0 = a ph7_vm_func in hFunction */` |
|         - |  169 | `};` |
|         - |  170 | ``/* The pending offset of a `$s[k] ??= v`, owned by its MEMOBJ_AUX_COALSTROFF peek result.`` |
|         - |  171 | ` * Carries its own allocator so PH7_MemObjRelease can free it without a VM pointer, exactly` |
|         - |  172 | ` * like VmDeferredPath. */` |
|         - |  173 | `typedef struct VmCoalStrOff VmCoalStrOff;` |
|         - |  174 | `struct VmCoalStrOff` |
|         - |  175 | `{` |
|         - |  176 | `	SyMemBackend *pAlloc;` |
|         - |  177 | `	ph7_value sKey;      /* the RAW offset, unresolved: the store re-resolves it LOUDLY */` |
|         - |  178 | `};` |
|         - |  179 | `/* The pending __call / __callStatic routing, owned by its MEMOBJ_AUX_MAGICCALL carrier` |
|         - |  180 | ` * slot. Carries its own allocator so PH7_MemObjRelease can free it without a VM pointer,` |
|         - |  181 | ` * exactly like VmCoalStrOff.` |
|         - |  182 | ` *` |
|         - |  183 | ` * It used to be three fields on the VM, set by OP_MEMBER and read by the OP_CALL that` |
|         - |  184 | ` * followed. That only held while the arguments were evaluated BEFORE the member op; once` |
|         - |  185 | ` * the callee is resolved first — php's order — an argument that is itself a routed call` |
|         - |  186 | `` * (`$o->outer($o->inner(1))`) runs in between and would overwrite the outer routing. The`` |
|         - |  187 | ` * record rides the slot instead, so it nests, and an abandoned call cannot leak the` |
|         - |  188 | ` * receiver reference. */` |
|         - |  189 | `typedef struct VmMagicCall VmMagicCall;` |
|         - |  190 | `struct VmMagicCall` |
|         - |  191 | `{` |
|         - |  192 | `	SyMemBackend *pAlloc;` |
|         - |  193 | `	ph7_class_instance *pRecv; /* OWNED receiver reference; 0 for a static routing */` |
|         - |  194 | `	ph7_class *pClass;         /* the class whose handler answers */` |
|         - |  195 | ``	ph7_class *pLsb;           /* `static::` inside __callStatic when the call FORWARDED it`` |
|         - |  196 | `	                            * (self::/parent::/static::); 0 means pClass */` |
|         - |  197 | `	SyBlob sName;              /* the method name as the CALL SITE spelled it */` |
|         - |  198 | `};` |
|         - |  199 | `/*` |
|         - |  200 | ` * D1 commit 2: a captured lvalue path for a deferred by-ref/by-value call argument` |
|         - |  201 | ` * ($a["k"], $o->p, and nested/undefined-base forms). Built on a lookup MISS by the` |
|         - |  202 | ` * LOAD_IDX/MEMBER record modes and re-walked by VmResolveDeferredArgs at OP_CALL once the` |
|         - |  203 | ` * callee's by-ref shape is known. Owned by a MEMOBJ_AUX_DEFPATH stack slot's x.pOther.` |
|         - |  204 | ` */` |
|         - |  205 | `typedef struct VmDeferStep VmDeferStep;` |
|         - |  206 | `typedef struct VmDeferredPath VmDeferredPath;` |
|         - |  207 | `struct VmDeferStep {` |
|         - |  208 | `	int       isProp;    /* 0 = subscript element, 1 = object property */` |
|         - |  209 | ``	int       bAppend;   /* element step with NO key: `f($a[])`, php's append. Only a by-REF`` |
|         - |  210 | `	                      * parameter may take one — a by-value binding is php's runtime` |
|         - |  211 | ``	                      * `Cannot use [] for reading` Error. */`` |
|         - |  212 | `	ph7_value sKey;      /* element: deep-copied index value (copied before pIdx is released) */` |
|         - |  213 | `	SyString  sProp;     /* property: name, into pName below (owned by the path allocation) */` |
|         - |  214 | `	char     *zProp;     /* property: owned copy of the name bytes (freed with the path) */` |
|         - |  215 | `};` |
|         - |  216 | `struct VmDeferredPath {` |
|         - |  217 | `	SyMemBackend *pAlloc;    /* allocator, so PH7_MemObjRelease can self-free without a pVm */` |
|         - |  218 | `	int           eRoot;     /* 0 = real container nIdx, 1 = undefined-var name, 2 = string base,` |
|         - |  219 | `	                          * 3 = a value ALREADY FETCHED (VM_DEFER_ROOT_PREFETCH below) */` |
|         - |  220 | `	sxu32         nRootIdx;  /* eRoot==0: aMemObj slot of the root container ($a/$o) */` |
|         - |  221 | `	SyString      sRootName; /* eRoot==1: variable name (VM-lifetime bytecode string, borrowed) */` |
|         - |  222 | `	sxu8          nOverKind; /* eRoot==3: which verdict a by-REFERENCE binding gets (VM_OVER_*) */` |
|         - |  223 | `	ph7_class    *pOverClass;/* eRoot==3: the class that answered — php's message names it */` |
|         - |  224 | `	SyString      sOverName; /* eRoot==3: the PROPERTY name (empty for an element) */` |
|         - |  225 | `	char         *zOverName; /* eRoot==3: owned bytes behind sOverName */` |
|         - |  226 | `	ph7_value     sPrefetch; /* eRoot==3: what the accessor answered */` |
|         - |  227 | `	sxu32         nStep;     /* number of captured steps (outer-to-inner) */` |
|         - |  228 | `	sxu32         nAlloc;    /* capacity of aStep */` |
|         - |  229 | `	VmDeferStep  *aStep;     /* captured steps */` |
|         - |  230 | `};` |
|         - |  231 | `/*` |
|         - |  232 | `` * eRoot == 3. Some fetches cannot be DEFERRED at all: a userland `offsetGet` (or a`` |
|         - |  233 | ` * subclass override of a native one) is a method call, and php runs it where the` |
|         - |  234 | ` * subscript is WRITTEN, whatever the parameter turns out to be. So the accessor runs` |
|         - |  235 | ` * at the fetch and the carrier holds its RESULT — the by-ref decision still arrives at` |
|         - |  236 | ``  * OP_CALL, and all it decides is php's `Indirect modification of overloaded element` `` |
|         - |  237 | ` * notice, since a value the container copied has no slot to alias either way.` |
|         - |  238 | ` */` |
|         - |  239 | `#define VM_DEFER_ROOT_PREFETCH 3` |
|         - |  240 | `/* What a by-REFERENCE binding of a prefetched value is, in php's words. All three are` |
|         - |  241 | ` * decided at the CALL because none of them is about the fetch: the same fetch feeding a` |
|         - |  242 | ` * by-VALUE parameter is silent. */` |
|         - |  243 | `#define VM_OVER_ELEM 0 /* Notice: Indirect modification of overloaded element of C has no effect */` |
|         - |  244 | `#define VM_OVER_PROP 1 /* Notice: Indirect modification of overloaded property C::$p has no effect */` |
|         - |  245 | `#define VM_OVER_HOOK 2 /* Error:  Indirect modification of C::$p is not allowed */` |
|         - |  246 | `/* Allowed value types.` |
|         - |  247 | ` */` |
|         - |  248 | `#define MEMOBJ_STRING    0x001  /* Memory value is a UTF-8 string */` |
|         - |  249 | `#define MEMOBJ_INT       0x002  /* Memory value is an integer */` |
|         - |  250 | `#define MEMOBJ_REAL      0x004  /* Memory value is a real number */` |
|         - |  251 | `#define MEMOBJ_BOOL      0x008  /* Memory value is a boolean */` |
|         - |  252 | `#define MEMOBJ_NULL      0x020  /* Memory value is NULL */` |
|         - |  253 | `#define MEMOBJ_HASHMAP   0x040  /* Memory value is a hashmap aka 'array' in the PHP jargon */` |
|         - |  254 | `#define MEMOBJ_OBJ       0x080  /* Memory value is an object [i.e: class instance] */` |
|         - |  255 | `#define MEMOBJ_RES       0x100  /* Memory value is a resource [User private data] */` |
|         - |  256 | `#define MEMOBJ_VOID      0x200  /* Pseudo-type: function must not return a value */` |
|         - |  257 | `#define MEMOBJ_REFERENCE 0x400  /* Memory value hold a reference (64-bit index) of another ph7_value */` |
|         - |  258 | `#define MEMOBJ_AUX_SPREAD 0x800 /* Stack-only marker: this value is a spread source for the next LOAD_MAP */` |
|         - |  259 | `#define MEMOBJ_NEVER     0x1000 /* Pseudo-type (return-only): never-returning function must not return at all */` |
|         - |  260 | `#define MEMOBJ_AUX_NOKEY 0x2000 /* Stack-only marker: absent array-literal key (see PH7_LOADC_NOKEY) */` |
|         - |  261 | `#define MEMOBJ_AUX_CUFVAL 0x4000 /* Stack-only marker: the ENGINE deliberately handed this by-ref` |
|         - |  262 | `                                  * argument a by-value copy, so the by-ref binder must NOT raise its` |
|         - |  263 | `                                  * "could not be passed by reference" Error for it. Two producers:` |
|         - |  264 | `                                  * call_user_func(), which php warns about and copies; and an` |
|         - |  265 | ``                                  * argument UNPACKED out of a temporary array (`f(...[1])`), whose`` |
|         - |  266 | `                                  * element php binds into a temporary nothing can observe. */` |
|         - |  267 | `#define MEMOBJ_AUX_DEFERRED 0x8000 /* Stack-only marker (D1): a deferred call argument whose target did` |
|         - |  268 | `                                    * not exist at load time. The value is NULL; x.pOther carries the` |
|         - |  269 | `                                    * lazy-lvalue descriptor (a plain-variable name pointer, or an` |
|         - |  270 | `                                    * element/property descriptor). OP_CALL's VmResolveDeferredArgs` |
|         - |  271 | `                                    * materializes it for a by-ref parameter or warns+passes NULL for a` |
|         - |  272 | `                                    * by-value one, clearing this flag. Never survives into a stored` |
|         - |  273 | `                                    * value: it is part of MEMOBJ_AUX, so MemObjStore strips it. */` |
|         - |  274 | `#define MEMOBJ_AUX_DEFPATH 0x10000 /* Stack-only marker (D1 commit 2): a deferred call argument that is an` |
|         - |  275 | `                                    * array-element ($a["k"]) or property ($o->p) lvalue whose target was` |
|         - |  276 | `                                    * ABSENT at load time. The value is NULL; x.pOther owns a heap` |
|         - |  277 | `                                    * VmDeferredPath (captured lvalue chain). VmResolveDeferredArgs re-walks` |
|         - |  278 | `                                    * it in vivify-mode (by-ref) or read+warn-mode (by-value). Unlike` |
|         - |  279 | `                                    * MEMOBJ_AUX_DEFERRED (a borrowed name pointer), this OWNS heap memory:` |
|         - |  280 | `                                    * PH7_MemObjRelease frees it at the TOP, before its MEMOBJ_NULL` |
|         - |  281 | `                                    * short-circuit, so every pop/abort/exception path releases it. Part of` |
|         - |  282 | `                                    * MEMOBJ_AUX, so MemObjStore strips the flag on copy. */` |
|         - |  283 | `#define MEMOBJ_AUX_COALSTROFF 0x40000 /* Stack-only marker: this NULL is the peek result of a` |
|         - |  284 | ``                                       * `$s[k] ??= v` over a STRING, and it OWNS a heap`` |
|         - |  285 | `                                       * VmCoalStrOff holding the RAW offset (x.pOther) for the` |
|         - |  286 | `                                       * OP_NULLC_STORE that follows — which has to write into` |
|         - |  287 | `                                       * the string OFFSET, and by then the offset value is gone.` |
|         - |  288 | `                                       * Same ownership contract as MEMOBJ_AUX_DEFPATH:` |
|         - |  289 | `                                       * PH7_MemObjRelease frees it, so an abandoned statement` |
|         - |  290 | `                                       * cannot leak it, and it nests (one carrier per pending` |
|         - |  291 | `                                       * ??= on the operand stack) where a single VM-wide slot` |
|         - |  292 | `                                       * could not. */` |
|         - |  293 | `#define MEMOBJ_AUX_MAGICCALL 0x80000 /* Stack-only marker: this callee slot is the engine's own` |
|         - |  294 | `                                      * __call/__callStatic dispatch, not a callable at all. OP_MEMBER` |
|         - |  295 | `                                      * sets it (with the receiver/class/original name latched on the` |
|         - |  296 | `                                      * VM) where a missing or inaccessible method must route through` |
|         - |  297 | `                                      * the magic handler; OP_CALL sees the mark BEFORE any callable` |
|         - |  298 | `                                      * decode and runs the packing body directly. It is what replaced` |
|         - |  299 | `                                      * the "__phl_magic_call" NAME the four OP_MEMBER sites used to` |
|         - |  300 | `                                      * write into this slot -- a hidden global function that` |
|         - |  301 | `                                      * function_exists() and get_defined_functions() both reported.` |
|         - |  302 | `                                      * The slot itself stays NULL-typed. Part of MEMOBJ_AUX, so a` |
|         - |  303 | `                                      * copy can never carry it. */` |
|         - |  304 | `#define MEMOBJ_AUX_MEMBERCALL 0x100000 /* Stack-only marker: this callee slot came out of an OP_MEMBER` |
|         - |  305 | `                                       * method resolution, which already DECIDED the call's` |
|         - |  306 | `                                       * visibility against the entry it actually chose. OP_CALL's` |
|         - |  307 | `                                       * own screen must then stand down: it re-derives the method` |
|         - |  308 | `                                       * from the FUNCTION's name against its declaring class, and` |
|         - |  309 | ``                                       * a trait adaptation splits those apart — `pub as private`` |
|         - |  310 | ``                                       * pHi` and `prot as public opened` share one struct name and`` |
|         - |  311 | `                                       * one sVmName with the method they were made from, so the` |
|         - |  312 | `                                       * re-derivation answered for the ORIGINAL and got the rule` |
|         - |  313 | `                                       * backwards in both directions. The mark rides the exact` |
|         - |  314 | `                                       * stack slot the call consumes (like MEMOBJ_AUX_MAGICCALL),` |
|         - |  315 | `                                       * so it cannot leak to another call the way a VM-wide latch` |
|         - |  316 | `                                       * could. Part of MEMOBJ_AUX, so a copy can never carry it. */` |
|         - |  317 | `#define MEMOBJ_AUX_ENGINEFN 0x200000 /* Stack-only marker: this callee STRING is one of the engine's` |
|         - |  318 | ``                                      * own function-table names (`[closure_N]`, and the`` |
|         - |  319 | ``                                      * `[__Class@meth_xxxxxxxxxx]` a mounted method is keyed under),`` |
|         - |  320 | `                                      * put there by the ENGINE rather than written by the program.` |
|         - |  321 | `                                      * It is what lets PH7_VmGetUserFunction refuse those names to a` |
|         - |  322 | `                                      * script -- function_exists() reported them, and calling a` |
|         - |  323 | `                                      * method's one underflowed the operand stack -- while the` |
|         - |  324 | `                                      * engine's own by-name dispatch of the same entries resolves.` |
|         - |  325 | `                                      * Set by the two SYNTHETIC call builders, which have no OP_MEMBER` |
|         - |  326 | `                                      * ahead of them to leave a mark: VmCallClassMethodLsb (a method's` |
|         - |  327 | `                                      * sVmName) and PH7_VmCallUserFunctionWithMap (a Closure unwrapped` |
|         - |  328 | ``                                      * to its `[closure_N]`). The in-line OP_CALL cases carry the same`` |
|         - |  329 | `                                      * verdict in a local instead -- MEMOBJ_AUX_MEMBERCALL for a` |
|         - |  330 | `                                      * resolved method, the unwrap branch for a closure -- because the` |
|         - |  331 | `                                      * member mark is consumed before the lookup. Part of MEMOBJ_AUX,` |
|         - |  332 | `                                      * so a copy can never carry it. */` |
|         - |  333 | `#define MEMOBJ_AUX_STROFFSET 0x20000 /* Stack-only marker: this value was READ OUT of a string by a` |
|         - |  334 | `                                      * subscript ($s[1]). It carries the BASE's slot index like any` |
|         - |  335 | `                                      * other element read, but a string offset is not a slot: php` |
|         - |  336 | `                                      * refuses to make a reference to one` |
|         - |  337 | `                                      * ("Cannot create references to/from string offsets"), and` |
|         - |  338 | `                                      * binding the index anyway aliased the WHOLE STRING — writing` |
|         - |  339 | `                                      * through the reference replaced it. The reference-binding` |
|         - |  340 | `                                      * sites test this. Part of MEMOBJ_AUX, so MemObjStore strips` |
|         - |  341 | ``                                      * it: a plain `$c = $s[1]` copy carries nothing. */`` |
|         - |  342 | `#define MEMOBJ_AUX_NATIVEPROP 0x400000 /* Stack-only marker: this value was read out of a NATIVE` |
|         - |  343 | `                                      * class's handler-backed property (PH7_CLASS_ATTR_NATIVE_SET),` |
|         - |  344 | `                                      * which is a field of php's own C struct rather than storage a` |
|         - |  345 | `                                      * script may alias. It says the value carries no slot on` |
|         - |  346 | `                                      * purpose, so the reference-binding site makes a silent COPY` |
|         - |  347 | `                                      * instead of raising the "require a variable not a constant"` |
|         - |  348 | ``                                      * diagnostic — php binds `$r = &$i->f` to a temporary and says`` |
|         - |  349 | `                                      * nothing. Part of MEMOBJ_AUX, so a copy can never carry it. */` |
|         - |  350 | `#define MEMOBJ_STREAMRES 0x1000000 /* This MEMOBJ_RES names a STREAM HANDLE (an io_private),` |
|         - |  351 | `                                    * and the value holds one of that handle's counted` |
|         - |  352 | `                                    * references. It marks the VALUE rather than the object` |
|         - |  353 | `                                    * because the object is the one thing a release cannot` |
|         - |  354 | `                                    * look at: a Generator's context resource is FREED by its` |
|         - |  355 | `                                    * own destructor before the slot naming it is released, so` |
|         - |  356 | `                                    * a probe of the pointer -- at any offset -- is a` |
|         - |  357 | `                                    * use-after-free. Not part of MEMOBJ_AUX, so it survives a` |
|         - |  358 | `                                    * store the way the type bits do; MemObjSetType clears it` |
|         - |  359 | `                                    * with them, so a slot retyped to another resource kind` |
|         - |  360 | `                                    * cannot inherit it. */` |
|         - |  361 | `/* Mask of all known types */` |
|         - |  362 | `#define MEMOBJ_ALL (MEMOBJ_STRING\|MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)` |
|         - |  363 | `/* Scalar variables` |
|         - |  364 | ` * According to the PHP language reference manual` |
|         - |  365 | ` *  Scalar variables are those containing an integer, float, string or boolean.` |
|         - |  366 | ` *  Types array, object and resource are not scalar.` |
|         - |  367 | ` */` |
|         - |  368 | `#define MEMOBJ_AUX_REFRET 0x800000 /* Stack-only marker: this value is the result of a call to a` |
|         - |  369 | `                                    * function DECLARED to return by reference. php raises` |
|         - |  370 | ``                                     * `Only variable references should be returned by reference` `` |
|         - |  371 | `                                    * at the RETURN when such a function has no variable to` |
|         - |  372 | `                                    * bind, and says nothing more at the call site -- so the` |
|         - |  373 | ``                                    * `Only variables should be assigned by reference` notice,`` |
|         - |  374 | `                                    * which is about a callee that never promised a reference,` |
|         - |  375 | `                                    * stands down for a value carrying this. */` |
|         - |  376 | `#define MEMOBJ_POOLFREE 0x1000000  /* NOT a type or a stack marker: pool bookkeeping. This slot is` |
|         - |  377 | `                                    * ON the value pool's intrusive free list (see VmMemPool), so` |
|         - |  378 | `                                    * its nIdx word is the link to the next free slot and NOT its` |
|         - |  379 | `                                    * own index. Set by VmMemPoolFreeSlot, cleared by the` |
|         - |  380 | `                                    * PH7_MemObjInit every acquire runs and by VmMemPoolTruncate` |
|         - |  381 | `                                    * when it abandons the chain. It exists to make a double free` |
|         - |  382 | `                                    * a no-op: the link lives inside the slot, so freeing the same` |
|         - |  383 | `                                    * index twice would point the head at itself and hand that one` |
|         - |  384 | `                                    * slot out for the rest of the run. Deliberately survives` |
|         - |  385 | `                                    * PH7_MemObjRelease, which leaves iFlags alone once a value is` |
|         - |  386 | `                                    * already MEMOBJ_NULL -- and a slot on the list always is. */` |
|         - |  387 | `#define MEMOBJ_SCALAR (MEMOBJ_STRING\|MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_NULL)` |
|         - |  388 | `#define MEMOBJ_AUX (MEMOBJ_REFERENCE\|MEMOBJ_AUX_SPREAD\|MEMOBJ_AUX_NOKEY\|MEMOBJ_AUX_CUFVAL\|MEMOBJ_AUX_DEFERRED\|MEMOBJ_AUX_DEFPATH\|MEMOBJ_AUX_STROFFSET\|MEMOBJ_AUX_COALSTROFF\|MEMOBJ_AUX_MAGICCALL\|MEMOBJ_AUX_MEMBERCALL\|MEMOBJ_AUX_ENGINEFN\|MEMOBJ_AUX_NATIVEPROP\|MEMOBJ_AUX_REFRET)` |
|         - |  389 | `/* Closure-instance flags (ph7_class_instance.iFlags), shared by vm_exec.c's OP_LOAD_FCC` |
|         - |  390 | ` * and vm_exec_ctx.c's closure machinery. Distinct from CLASS_INSTANCE_DESTROYED 0x001` |
|         - |  391 | ` * (oo.c) and VM_INSTANCE_DUMPING 0x002, which share the same word. */` |
|         - |  392 | `/* ph7_class_instance.iFlags bit: this Closure is a bound/static first-class callable and` |
|         - |  393 | ` * carries $__this/$__scope. Lets the hot plain-closure unwrap skip those attribute lookups.` |
|         - |  394 | ` * (Distinct from CLASS_INSTANCE_DESTROYED 0x001 and VM_INSTANCE_DUMPING 0x002.) */` |
|         - |  395 | `#define VM_INSTANCE_FCC_BOUND 0x004` |
|         - |  396 | `/* ph7_class_instance.iFlags bit: this Closure wraps an __invoke OBJECT, and the engine —` |
|         - |  397 | `` * not the source — is what named `__invoke` (Closure::fromCallable($obj)). php resolves it`` |
|         - |  398 | `` * the way it resolves `$obj()`, so a non-public __invoke is dispatched rather than denied;`` |
|         - |  399 | `` * `$obj->__invoke(...)` and `[$obj,'__invoke']`, which the SOURCE names, stay denied and`` |
|         - |  400 | ` * never carry this bit. Read by VmClosureUnwrap, which arms the engine's magic latch. */` |
|         - |  401 | `#define VM_INSTANCE_FCC_INVOKE_OBJ 0x010` |
|         - |  402 | `/* ph7_class_instance.iFlags bit: this Closure's $__fn names a METHOD of $__this's class (or` |
|         - |  403 | `` * of $__scope) — `$o->m(...)`, `C::m(...)`, `Closure::fromCallable([$o,'m'])`. Without it the`` |
|         - |  404 | ` * unwrap had to GUESS, by asking whether the class declares a method of that name, and it` |
|         - |  405 | ` * guessed wrong in both directions: a name the class answers only through __call fell through` |
|         - |  406 | ` * to the plain-function route and failed with "Call to undefined function m()", and a name` |
|         - |  407 | ` * that happened to match a global function ran the FUNCTION. A bound plain closure` |
|         - |  408 | `` * (`function(){…}->bindTo($o)`) never carries this bit, which is what the guess was really`` |
|         - |  409 | ` * trying to detect. */` |
|         - |  410 | `#define VM_INSTANCE_FCC_METHOD 0x020` |
|         - |  411 | `/* ph7_class_instance.iFlags bit: this Closure's callee was RESOLVED to a real, directly` |
|         - |  412 | ` * callable method when the closure was BUILT — the way php resolves one, keeping the` |
|         - |  413 | ` * function itself rather than a name. No dispatch site may re-decide its visibility against` |
|         - |  414 | `` * the CALLER: that is what killed an escaped `$this->priv(...)` php runs anywhere. A closure`` |
|         - |  415 | ` * whose creation resolved to the class's __call/__callStatic TRAMPOLINE instead (a missing or` |
|         - |  416 | ` * inaccessible name on a class that declares one) deliberately does NOT carry the bit — its` |
|         - |  417 | ` * dispatch has to reach the catch-all, as php's does. */` |
|         - |  418 | `#define VM_INSTANCE_FCC_SCREENED 0x040` |
|         - |  419 | ``/* ph7_class_instance.iFlags bit: this Closure was minted by the `(...)` SYNTAX -- `$o->m(...)`,`` |
|         - |  420 | `` * `C::m(...)`, `$f(...)` -- rather than by Closure::fromCallable(). The two agree on everything`` |
|         - |  421 | ` * but a call TRAMPOLINE's signature: php builds the syntax's one with a single variadic` |
|         - |  422 | `` * `...$arguments` and fromCallable's with none, and the debug dump and Reflection both show it.`` |
|         - |  423 | ` * A rebind's clone keeps it. 0x200: 0x001..0x100 are claimed on this word. */` |
|         - |  424 | `#define VM_INSTANCE_FCC_SYNTAX 0x200` |
|         - |  425 | `/* ph7_class_instance.iFlags bit: this Closure is a bind/bindTo clone, so its $__this/$__scope` |
|         - |  426 | ` * are the whole answer to "what is it bound to" -- an EMPTY pair means unbound. A closure` |
|         - |  427 | ` * expression that no rebind has touched carries neither attribute: its scope and receiver` |
|         - |  428 | ` * are still where OP_LOAD_CLOSURE put them, on its per-instantiation function. */` |
|         - |  429 | `#define VM_INSTANCE_FCC_REBOUND 0x400` |
|         - |  430 | ``#define PH7_CLOSURE_UNSCOPED 2 /* bClosureUnbound: no `$this` AND no class scope */`` |
|         - |  431 | `/*` |
|         - |  432 | ` * The following macro clear the current ph7_value type and replace` |
|         - |  433 | ` * it with the given one.` |
|         - |  434 | ` */` |
|         - |  435 | `#define MemObjSetType(OBJ,TYPE) ((OBJ)->iFlags = ((OBJ)->iFlags&~(MEMOBJ_ALL\|MEMOBJ_STREAMRES))\|TYPE)` |
|         - |  436 | `/*` |
|         - |  437 | ` * Signed 64-bit arithmetic with overflow detection. PHP promotes an integer` |
|         - |  438 | ` * operation that overflows sxi64 to a floating-point result, so the executor` |
|         - |  439 | ` * checks for overflow on every +,-,* (and ++/--) and re-runs the operation in` |
|         - |  440 | ` * double precision when it trips. GCC/Clang expose the __builtin_*_overflow` |
|         - |  441 | ` * intrinsics (zero cost, no UB); MSVC lacks them, so we fall back to portable` |
|         - |  442 | ` * implementations defined in memobj.c. Each macro sets *pR to the wrapped` |
|         - |  443 | ` * result and evaluates to non-zero on overflow.` |
|         - |  444 | ` */` |
|         - |  445 | `#if defined(__GNUC__) \|\| defined(__clang__)` |
|         - |  446 | `#define PH7_ADD_OVERFLOW64(a,b,pR) __builtin_add_overflow((a),(b),(pR))` |
|         - |  447 | `#define PH7_SUB_OVERFLOW64(a,b,pR) __builtin_sub_overflow((a),(b),(pR))` |
|         - |  448 | `#define PH7_MUL_OVERFLOW64(a,b,pR) __builtin_mul_overflow((a),(b),(pR))` |
|         - |  449 | `#else` |
|         - |  450 | `PH7_PRIVATE int PH7_AddOverflow64(sxi64 a,sxi64 b,sxi64 *pR);` |
|         - |  451 | `PH7_PRIVATE int PH7_SubOverflow64(sxi64 a,sxi64 b,sxi64 *pR);` |
|         - |  452 | `PH7_PRIVATE int PH7_MulOverflow64(sxi64 a,sxi64 b,sxi64 *pR);` |
|         - |  453 | `#define PH7_ADD_OVERFLOW64(a,b,pR) PH7_AddOverflow64((a),(b),(pR))` |
|         - |  454 | `#define PH7_SUB_OVERFLOW64(a,b,pR) PH7_SubOverflow64((a),(b),(pR))` |
|         - |  455 | `#define PH7_MUL_OVERFLOW64(a,b,pR) PH7_MulOverflow64((a),(b),(pR))` |
|         - |  456 | `#endif` |
|         - |  457 | `/* ph7_value cast method signature */` |
|         - |  458 | `typedef sxi32 (*ProcMemObjCast)(ph7_value *);` |
|         - |  459 | `/* Forward reference */` |
|         - |  460 | `typedef struct ph7_output_consumer ph7_output_consumer;` |
|         - |  461 | `/*` |
|         - |  462 | ` * One parameter of a builtin's declared signature, as the shared argument screen` |
|         - |  463 | ` * (VmEnforceBuiltinArgTypes) needs to see it: the type TEXT, the parameter name, and` |
|         - |  464 | `` * the two markers the text carries -- `&` for by-reference and a leading `~` for`` |
|         - |  465 | ` * php's stub-versus-body disagreement. Every pointer is INTO zSig, which is static` |
|         - |  466 | ` * storage; nothing here is copied and nothing here is freed.` |
|         - |  467 | ` *` |
|         - |  468 | ` * Getting this out of zSig is a walk -- skip spaces, find the comma that ends the` |
|         - |  469 | ` * parameter (honouring a quoted default, which can contain one), find the '$', check` |
|         - |  470 | ` * for a variadic '...', trim the type's trailing spaces and '&'. The screen did that` |
|         - |  471 | ` * walk for every argument of every builtin call: 18,901,261 parameters on the` |
|         - |  472 | ` * ecosystem gate's phpcs step, for an answer that is a property of the DECLARATION` |
|         - |  473 | ` * and cannot change between two calls.` |
|         - |  474 | ` */` |
|         - |  475 | `typedef struct VmArgScreenParam VmArgScreenParam;` |
|         - |  476 | `struct VmArgScreenParam` |
|         - |  477 | `{` |
|         - |  478 | `	const char *zType;   /* into zSig; nType 0 means untyped and unscreened */` |
|         - |  479 | `	const char *zName;   /* into zSig, past the '$' */` |
|         - |  480 | `	sxu16 nType;` |
|         - |  481 | `	sxu16 nName;` |
|         - |  482 | `	sxu8 bByRef;         /* "array &$array" */` |
|         - |  483 | `	sxu8 bStub;          /* "~Type $p": the builtin raises its own TypeError */` |
|         - |  484 | `	sxu32 nMask;         /* VMSIG_* -- which arms this declared type has. The screen asks` |
|         - |  485 | `	                      * that question up to forty-four times per argument, and every` |
|         - |  486 | `	                      * ask was a split-on-'\|' walk over the same text. The bits are` |
|         - |  487 | `	                      * SET by calling the very functions they replace (see` |
|         - |  488 | `	                      * VmArgScreenNext), so a bit cannot mean something the walk did` |
|         - |  489 | `	                      * not say. */` |
|         - |  490 | `};` |
|         - |  491 | `/* The arms VmArgScreenParam::nMask records. The first thirteen are VmSigTypeHas() tokens;` |
|         - |  492 | ` * the last three are the three composite questions the screen asks about a whole type. */` |
|         - |  493 | `#define VMSIG_MIXED      0x00000001` |
|         - |  494 | `#define VMSIG_ARRAY      0x00000002` |
|         - |  495 | `#define VMSIG_ITERABLE   0x00000004` |
|         - |  496 | `#define VMSIG_CALLABLE   0x00000008` |
|         - |  497 | `#define VMSIG_OBJECT     0x00000010` |
|         - |  498 | `#define VMSIG_STRING     0x00000020` |
|         - |  499 | `#define VMSIG_NULL       0x00000040` |
|         - |  500 | `#define VMSIG_INT        0x00000080` |
|         - |  501 | `#define VMSIG_FLOAT      0x00000100` |
|         - |  502 | `#define VMSIG_BOOL       0x00000200` |
|         - |  503 | `#define VMSIG_TRUE       0x00000400` |
|         - |  504 | `#define VMSIG_FALSE      0x00000800` |
|         - |  505 | `#define VMSIG_RESOURCE   0x00001000` |
|         - |  506 | `#define VMSIG_CLASS      0x00002000   /* VmSigTypeHasClass: an arm that is not a builtin type */` |
|         - |  507 | `#define VMSIG_INTONLY    0x00004000   /* VmSigTypeIsIntOnly */` |
|         - |  508 | `#define VMSIG_ARRAYONLY  0x00008000   /* VmSigTypeIsArrayOnly */` |
|         - |  509 | `typedef struct ph7_user_func ph7_user_func;` |
|         - |  510 | `typedef struct ph7_conf ph7_conf;` |
|         - |  511 | `/*` |
|         - |  512 | ` * An instance of the following structure store the default VM output` |
|         - |  513 | ` * consumer and it's private data.` |
|         - |  514 | ` * Client-programs can register their own output consumer callback` |
|         - |  515 | ` * via the [PH7_VM_CONFIG_OUTPUT] configuration directive.` |
|         - |  516 | ` * Please refer to the official documentation for more information` |
|         - |  517 | ` * on how to register an output consumer callback.` |
|         - |  518 | ` */` |
|         - |  519 | `struct ph7_output_consumer` |
|         - |  520 | `{` |
|         - |  521 | `	ProcConsumer xConsumer; /* VM output consumer routine */` |
|         - |  522 | `	void *pUserData;        /* Third argument to xConsumer() */` |
|         - |  523 | `	ProcConsumer xDef;      /* Default output consumer routine */` |
|         - |  524 | `	void *pDefData;         /* Third argument to xDef() */` |
|         - |  525 | `};` |
|         - |  526 | `/*` |
|         - |  527 | ` * PH7 engine [i.e: ph7 instance] configuration is stored in` |
|         - |  528 | ` * an instance of the following structure.` |
|         - |  529 | ` * Please refer to the official documentation for more information` |
|         - |  530 | ` * on how to configure your ph7 engine instance.` |
|         - |  531 | ` */` |
|         - |  532 | `struct ph7_conf` |
|         - |  533 | `{` |
|         - |  534 | `	ProcConsumer xErr;   /* Compile-time error consumer callback (the LOG channel) */` |
|         - |  535 | `	void *pErrData;      /* Third argument to xErr() */` |
|         - |  536 | `	ProcConsumer xOut;   /* Program-output consumer for a compile diagnostic's DISPLAY copy` |
|         - |  537 | `	                      * [PH7_CONFIG_OUTPUT]. The VM's own output consumer does not exist` |
|         - |  538 | `	                      * yet while the main script compiles, and the display copy is` |
|         - |  539 | `	                      * program output — falling back to xErr put it on the log stream. */` |
|         - |  540 | `	void *pOutData;      /* Third argument to xOut() */` |
|         - |  541 | `	SyBlob sErrConsumer; /* Default error consumer */` |
|         - |  542 | `	ph7_clock xClock;    /* Optional embedder clock [PH7_CONFIG_CLOCK]; NULL => platform default */` |
|         - |  543 | `	void *pClockData;    /* Third argument to xClock() */` |
|         - |  544 | `	sxu32 nMaxInput;     /* Per-compile input byte cap [PH7_CONFIG_MAX_INPUT]; 0 = PH7_MAX_INPUT_SIZE */` |
|         - |  545 | `	SySet aIniEntry;     /* php.ini directives applied to every VM at birth [PH7_CONFIG_INI_ENTRY]:` |
|         - |  546 | `	                      * VmIniEntry copies on the ENGINE allocator, replayed by ph7VmInit` |
|         - |  547 | `	                      * before the unit is compiled. A directive that only ever reached a` |
|         - |  548 | `	                      * finished VM could not gate that unit's own compile diagnostics:` |
|         - |  549 | `	                      * ph7_compile_file is what CREATES the VM. */` |
|         - |  550 | `	int bErrReport;      /* PH7_CONFIG_ERR_REPORT: seed a new VM's reporting mask to E_ALL,` |
|         - |  551 | ``	                      * before the aIniEntry replay so `-d error_reporting=` still wins */`` |
|         - |  552 | `	SyString sIniFile;   /* PH7_CONFIG_INI_FILE: the php.ini file that was read, canonical, on` |
|         - |  553 | `	                      * the ENGINE allocator. Empty when none was; php_ini_loaded_file()` |
|         - |  554 | ``	                      * answers FALSE then, which is php's own answer to `php -n`. */`` |
|         - |  555 | `};` |
|         - |  556 | `/*` |
|         - |  557 | ` * Signature of the C function responsible of expanding constant values.` |
|         - |  558 | ` */` |
|         - |  559 | `typedef void (*ProcConstant)(ph7_value *,void *);` |
|         - |  560 | `/*` |
|         - |  561 | ` * Each registered constant [i.e: __TIME__, __DATE__, PHP_OS, INT_MAX, etc.] is stored` |
|         - |  562 | ` * in an instance of the following structure.` |
|         - |  563 | ` * Please refer to the official documentation for more information` |
|         - |  564 | ` * on how to create/install foreign constants.` |
|         - |  565 | ` */` |
|         - |  566 | `typedef struct ph7_constant ph7_constant;` |
|         - |  567 | `struct ph7_constant` |
|         - |  568 | `{` |
|         - |  569 | `	SyString sName;        /* Constant name, as it was DECLARED -- what` |
|         - |  570 | `	                        * get_defined_constants() and Reflection answer */` |
|         - |  571 | `	char *zKey;            /* hConstant key when it differs from sName's bytes` |
|         - |  572 | `	                        * (a namespaced name, folded -- PH7_VmConstantFetch);` |
|         - |  573 | `	                        * NULL when the key aliases sName and is freed with it */` |
|         - |  574 | `	ProcConstant xExpand;  /* Function responsible of expanding constant value */` |
|         - |  575 | `	void *pUserData;       /* Last argument to xExpand() */` |
|         - |  576 | `	SyString sFile;        /* Defining file (aliases the VM-lifetime dup in pVm->aFiles);` |
|         - |  577 | `	                        * nByte == 0 = unknown/engine constant */` |
|         - |  578 | ``	sxu32 nLine;           /* Declaration line for `const`; 0 for define()/engine */`` |
|         - |  579 | `	sxu8 bUserDefined;     /* 1 when created by user code (const / define()):` |
|         - |  580 | `	                        * Reflection isInternal()/getFileName() input */` |
|         - |  581 | `	const char *zDeprecated; /* php's reason clause when the SYMBOL is deprecated` |
|         - |  582 | `	                        * ("8.1, as the constant has no effect"), else NULL.` |
|         - |  583 | `	                        * Static storage. Naming the constant raises php's` |
|         - |  584 | `	                        * E_DEPRECATED; LISTING the table does not. */` |
|         - |  585 | `	SySet aAttrs;          /* Declared #[...] attributes (ph7_attribute records) —` |
|         - |  586 | ``	                        * php 8.5 attributes on `const` statements */	sxu32 nRunGen;         /* pVm->nRunGen when it was last installed: a reused VM`` |
|         - |  587 | `	                        * (PH7_VmReset) lets a define() made by an EARLIER run be` |
|         - |  588 | `	                        * made again, where php's fresh process would never see it */` |
|         - |  589 | `};` |
|         - |  590 | `typedef struct ph7_aux_data ph7_aux_data;` |
|         - |  591 | `/*` |
|         - |  592 | ` * Auxiliary data associated with each foreign function is stored` |
|         - |  593 | ` * in a stack of the following structure.` |
|         - |  594 | ` * Note that automatic tracked chunks are also stored in an instance` |
|         - |  595 | ` * of this structure.` |
|         - |  596 | ` */` |
|         - |  597 | `struct ph7_aux_data` |
|         - |  598 | `{` |
|         - |  599 | `	void *pAuxData; /* Aux data */` |
|         - |  600 | `};` |
|         - |  601 | `/* Foreign functions signature */` |
|         - |  602 | `typedef int (*ProchHostFunction)(ph7_context *,int,ph7_value **);` |
|         - |  603 | `/*` |
|         - |  604 | ` * Each installed foreign function is recored in an instance of the following` |
|         - |  605 | ` * structure.` |
|         - |  606 | ` * Please refer to the official documentation for more information on how` |
|         - |  607 | ` * to create/install foreign functions.` |
|         - |  608 | ` */` |
|         - |  609 | `/*` |
|         - |  610 | ` * One name php 8.x deprecated, and the clause it ends the notice with.` |
|         - |  611 | ` *` |
|         - |  612 | `` * The SUBJECT is php's own spelling -- `curl_close` for a function,`` |
|         - |  613 | `` * `SplObjectStorage::attach` for a method, where php always names the`` |
|         - |  614 | ` * DECLARING class even for a call through a subclass -- so the raise site` |
|         - |  615 | ` * needs no class lookup of its own. Static storage, shared by the E_DEPRECATED` |
|         - |  616 | `` * notice and the export format's `<internal, deprecated:EXT>` tag.`` |
|         - |  617 | ` */` |
|         - |  618 | `typedef struct ph7_deprecated_name ph7_deprecated_name;` |
|         - |  619 | `struct ph7_deprecated_name` |
|         - |  620 | `{` |
|         - |  621 | `	const char *zName;  /* php's subject, without the trailing "()" */` |
|         - |  622 | `	const char *zWhy;   /* what follows "is deprecated since ": "8.2",` |
|         - |  623 | `	                     * "8.5, use method SplObjectStorage::offsetSet() instead" */` |
|         - |  624 | `};` |
|         - |  625 | `struct ph7_user_func` |
|         - |  626 | `{` |
|         - |  627 | `	ph7_vm *pVm;              /* VM that own this instance */` |
|         - |  628 | `	SyString sName;           /* Foreign function name */` |
|         - |  629 | `	ProchHostFunction xFunc;  /* Implementation of the foreign function */` |
|         - |  630 | `	void *pUserData;          /* User private data [Refer to the official documentation for more information]*/` |
|         - |  631 | `	SySet aAux;               /* Stack of auxiliary data [Refer to the official documentation for more information]*/` |
|         - |  632 | `	sxi16 nMinArg;            /* Minimum required arguments for the PHP-8 ArgumentCountError` |
|         - |  633 | `	                           * check at the OP_CALL choke point; 0 = no central enforcement` |
|         - |  634 | `	                           * (the builtin self-validates, or genuinely accepts zero args). */` |
|         - |  635 | `	sxu8 bAtLeast;            /* 0 -> "expects exactly N", 1 -> "expects at least N" (the` |
|         - |  636 | `	                           * wording depends on whether the builtin has optional params). */` |
|         - |  637 | `	sxu8 bHasMaxArg;          /* 0 -> no central too-many-arguments check (the SAFE default: this` |
|         - |  638 | `	                           * struct is SyZero'd on creation, so an unstamped builtin must mean` |
|         - |  639 | `	                           * "unenforced", never "accepts at most zero"). 1 -> nMaxArg applies. */` |
|         - |  640 | `	sxi16 nMaxArg;            /* Maximum accepted arguments when bHasMaxArg; derived from the` |
|         - |  641 | `	                           * signature. A variadic parameter leaves bHasMaxArg at 0. */` |
|         - |  642 | `	const char *zSig;         /* PHP-style parameter list ("string $s, int $o = 0") from the` |
|         - |  643 | `	                           * static signature table, or NULL: ReflectionFunction input for` |
|         - |  644 | `	                           * internal functions. Points at static storage — never freed. */` |
|         - |  645 | `	const char *zRet;         /* Return-type text from the same table, or NULL */` |
|         - |  646 | `	const ph7_deprecated_name *pDeprecated; /* php's deprecation for this name, or NULL. A` |
|         - |  647 | `	                           * NATIVE method reaches it through ph7_vm_func::pNative, so one` |
|         - |  648 | `	                           * field covers both a C builtin and a native class method --` |
|         - |  649 | `	                           * they share this struct and the same OP_CALL block. */` |
|         - |  650 | ``	sxu32 nByRefMask;         /* D1: bit N set => positional parameter N is by-reference (`&$p` in zSig),`` |
|         - |  651 | `	                           * derived once in VmSetBuiltinSignatures. Lets OP_CALL materialize a` |
|         - |  652 | `	                           * deferred by-ref out-param regardless of how the builtin was reached` |
|         - |  653 | ``	                           * (bare name, dynamic `$f=...`, or callable) — the compile-time`` |
|         - |  654 | `	                           * GenStateByRefBuiltinMask only sees the bare-name case. 0 when unstamped. */` |
|         - |  655 | `	sxu32 nPathMask;          /* Which of this builtin's arguments php reads with Z_PARAM_PATH,` |
|         - |  656 | `	                           * so a NUL inside one is a catchable ValueError rather than a` |
|         - |  657 | `	                           * truncated read. Derived from a ~70-name table, and the two` |
|         - |  658 | `	                           * questions the shared argument screen used to ask by SCANNING` |
|         - |  659 | `	                           * that table (and a second one) on every single builtin call.` |
|         - |  660 | `	                           * Both answers depend on the NAME alone, so they are worked out` |
|         - |  661 | `	                           * the first time this function is called and kept here. */` |
|         - |  662 | `	sxu8 bConstruct;          /* This name is a language CONSTRUCT, not a function: php has no` |
|         - |  663 | ``	                           * `empty`, `isset`, `unset`, `eval`, `print`, `include`,`` |
|         - |  664 | ``	                           * `include_once`, `require` or `require_once` in its function`` |
|         - |  665 | `	                           * table, and every door that answers about a name says so.` |
|         - |  666 | `	                           * The record stays -- the compiler dispatches the construct` |
|         - |  667 | `	                           * through it -- but PH7_VmGetHostFunction hides it from every` |
|         - |  668 | `	                           * SCRIPT-spelled lookup. See PH7_CALL_CONSTRUCT. */` |
|         - |  669 | `	sxu8 bSelfChecked;        /* This builtin words its own argument refusals and must not be` |
|         - |  670 | `	                           * pre-empted by the shared screen (php overloads it on arity, or` |
|         - |  671 | `	                           * its declared type and its refusal text disagree). */` |
|         - |  672 | `	sxu8 bScreenStamped;      /* Everything this screen keeps on the record -- nPathMask,` |
|         - |  673 | `	                           * bSelfChecked, nSigLen and the aSigParam table -- has been` |
|         - |  674 | `	                           * worked out. The struct is SyZero'd at creation, so 0 means` |
|         - |  675 | `	                           * "not yet" and never "no".` |
|         - |  676 | `	                           * Stamped lazily rather than at VM init because a NATIVE` |
|         - |  677 | `	                           * METHOD's record is reached through ph7_vm_func::pNative and` |
|         - |  678 | `	                           * is not in the host function table the init pass walks. */` |
|         - |  679 | `	VmArgScreenParam *aSigParam; /* zSig's parameters, parsed ONCE (see the struct above and` |
|         - |  680 | `	                           * VmArgScreenStamp). 0 when there are none, or when the` |
|         - |  681 | `	                           * allocation failed -- the screen then walks the text, which` |
|         - |  682 | `	                           * is the same code and the same answers, just per call. */` |
|         - |  683 | `	sxu16 nSigParam;          /* how many aSigParam holds; beyond it the screen stops, which` |
|         - |  684 | `	                           * is what the walk did at a variadic tail or a malformed row. */` |
|         - |  685 | `	sxu32 nSigLen;            /* SyStrlen(zSig), worked out with the rest. zSig is a` |
|         - |  686 | `	                           * literal from aBuiltinSig[] (or a native method's table) and` |
|         - |  687 | `	                           * is assigned exactly once, so its length is a constant of the` |
|         - |  688 | `	                           * DECLARATION -- but the shared argument screen measured it on` |
|         - |  689 | `	                           * every call, which on the ecosystem gate's phpcs step was` |
|         - |  690 | `	                           * 425,987,828 bytes of strlen across 11,391,725 calls.` |
|         - |  691 | `	                           * Only meaningful once bScreenStamped. */` |
|         - |  692 | `};` |
|         - |  693 | `/*` |
|         - |  694 | ` * The 'context' argument for an installable function. A pointer to an` |
|         - |  695 | ` * instance of this structure is the first argument to the routines used` |
|         - |  696 | ` * implement the foreign functions.` |
|         - |  697 | ` */` |
|         - |  698 | `typedef struct VmCallArgMap VmCallArgMap; /* Forward decl; full definition below. */` |
|         - |  699 | `struct ph7_context` |
|         - |  700 | `{` |
|         - |  701 | `	ph7_user_func *pFunc;   /* Function information. */` |
|         - |  702 | `	ph7_value *pRet;        /* Return value is stored here. */` |
|         - |  703 | `	SySet sVar;             /* Container of dynamically allocated ph7_values` |
|         - |  704 | `							 * [i.e: Garbage collection purposes.]` |
|         - |  705 | `							 */` |
|         - |  706 | `	SySet sChunk;           /* Track dynamically allocated chunks [ph7_aux_data instance].` |
|         - |  707 | `							 * [i.e: Garbage collection purposes.]` |
|         - |  708 | `							 */` |
|         - |  709 | `	ph7_vm *pVm;            /* Virtual machine that own this context */` |
|         - |  710 | `	sxi32 iFlags;           /* Call flags (PH7_CTX_CALL_*) */` |
|         - |  711 | `	sxi32 nThrowRc;         /* Status of a throw this host function raised through` |
|         - |  712 | `	                         * PH7_VmThrowException (0 when it never threw). The` |
|         - |  713 | `	                         * OP_CALL boundary re-reads it: a builtin that threw and` |
|         - |  714 | `	                         * still returned PH7_OK would otherwise let the VM carry` |
|         - |  715 | `	                         * on inside the try the throw abandoned. See` |
|         - |  716 | `	                         * VmHostFuncThrowRc(). */` |
|         - |  717 | `	VmCallArgMap *pArgMap;  /* Call-site named-argument map (or 0). Lets a builtin` |
|         - |  718 | `	                         * such as call_user_func forward its callers' name:` |
|         - |  719 | `	                         * arguments to the inner callback. */` |
|         - |  720 | `	ph7_class_instance *pThis; /* VM_FUNC_NATIVE method only: the receiver, or 0 for a static` |
|         - |  721 | `	                         * call and for every plain host function. Read through` |
|         - |  722 | `	                         * PH7_ContextThis(); the reference is owned by the CALLER for the` |
|         - |  723 | `	                         * duration of the call, so a native body must not unref it. */` |
|         - |  724 | `	ph7_class *pCalledClass;/* VM_FUNC_NATIVE method only: the class the call was made` |
|         - |  725 | `	                         * THROUGH (php's late-static-binding target), which for an` |
|         - |  726 | `	                         * inherited method is the subclass, not the declaring class. 0 for` |
|         - |  727 | `	                         * a plain host function. */` |
|         - |  728 | `	ph7_value sThis;        /* Scratch MEMOBJ_OBJ view of pThis, materialized on the first` |
|         - |  729 | `	                         * PH7_ContextThisValue() call so a native body can reach the` |
|         - |  730 | `	                         * receiver through the ordinary ph7_value object helpers` |
|         - |  731 | `	                         * (ph7_object_fetch_attr & co). bThisInit gates the lazy init;` |
|         - |  732 | `	                         * VmReleaseCallContext tears it down. */` |
|         - |  733 | `	sxu8 bThisInit;         /* 1 once sThis has been initialized */` |
|         - |  734 | `	struct PH7_NativePropCtx *pPropCtx; /* Non-zero while this SCRATCH context is running a native` |
|         - |  735 | `	                         * class's property handler (ph7_class::xProp). A handler shares` |
|         - |  736 | `	                         * its bodies with the ordinary method path, and those raise a` |
|         - |  737 | `	                         * refusal by throwing -- which here would run the enclosing catch` |
|         - |  738 | `	                         * in the middle of the member opcode. With this set the DOM` |
|         - |  739 | `	                         * refusal helpers RECORD into the hook's context instead, and the` |
|         - |  740 | `	                         * opcode raises it where the access would have landed. */` |
|         - |  741 | `};` |
|         - |  742 | `/* ph7_context::iFlags */` |
|         - |  743 | `#define PH7_CTX_CALL_DYNAMIC 0x01 /* php's ZEND_CALL_DYNAMIC: this host function was reached` |
|         - |  744 | ``                                   * through a name the program computed (`$n()`), a Closure`` |
|         - |  745 | ``                                   * (`compact(...)`, Closure::fromCallable), or an internal`` |
|         - |  746 | `                                   * function driving a callback (array_map, usort,` |
|         - |  747 | `                                   * Reflection's invoke, a call_user_func php's compiler could` |
|         - |  748 | `                                   * not fold into a direct call). The six functions that read` |
|         - |  749 | `                                   * their CALLER's frame (compact, extract, get_defined_vars,` |
|         - |  750 | `                                   * func_get_args, func_num_args, func_get_arg) refuse such a` |
|         - |  751 | `                                   * call the way php does -- see PH7_VmForbidDynamicCall. */` |
|         - |  752 | `#define PH7_CTX_CALL_CT_BOUND 0x02 /* A compiled call naming this host function by a literal` |
|         - |  753 | `                                   * name php's compiler binds (not unqualified inside a` |
|         - |  754 | ``                                   * namespace), with no spread and no `name:` argument: the`` |
|         - |  755 | `                                   * shape php compiles call_user_func()/_array() into` |
|         - |  756 | `                                   * ZEND_INIT_USER_CALL for, which screens the callable` |
|         - |  757 | `                                   * itself (see VmForwardScreen). */` |
|         - |  758 | `#define PH7_CTX_CALL_FOLDED 0x04 /* php's compiler rewrote THIS call into an opcode of its` |
|         - |  759 | `                                  * own (strlen, count, array_key_exists, ...), so it has no` |
|         - |  760 | `                                  * frame and speaks the opcode's words: array_key_exists()'s` |
|         - |  761 | `                                  * illegal key is the engine's offset Error there and its` |
|         - |  762 | `                                  * ZPP TypeError through any real call. */` |
|         - |  763 | `/*` |
|         - |  764 | ` * Each hashmap entry [i.e: array(4,5,6)] is recorded in an instance` |
|         - |  765 | ` * of the following structure.` |
|         - |  766 | ` */` |
|         - |  767 | `/* Allowed hashmap node key types (iType below) */` |
|         - |  768 | `#define HASHMAP_INT_NODE   1  /* Node with an int [i.e: 64-bit integer] key */` |
|         - |  769 | `#define HASHMAP_BLOB_NODE  2  /* Node with a string/BLOB key */` |
|         - |  770 | `/* Node control flags (iFlags below) */` |
|         - |  771 | `#define HASHMAP_NODE_FOREIGN_OBJ 0x001 /* Node holds a reference to a foreign ph7_value` |
|         - |  772 | `                                        * [i.e: array(&var) / $a[] =& $var ] */` |
|         - |  773 | `/*` |
|         - |  774 | ` * A string key of up to this many bytes lives INSIDE its node, and xKey.sKey is pointed` |
|         - |  775 | ` * at it (SXBLOB_STATIC, so nothing grows it and nothing frees it). Every reader still` |
|         - |  776 | ` * goes through SyBlobData()/SyBlobLength(), so none of the sixty-odd places that read a` |
|         - |  777 | ` * node's key changed.` |
|         - |  778 | ` *` |
|         - |  779 | ` * Why it is free: the node is 96 bytes and the pool serves it out of a 128-byte chunk` |
|         - |  780 | ` * (96 + the pool's own 8-byte header rounds up), so 24 bytes were already being paid for` |
|         - |  781 | ` * and thrown away. 96 + 24 = 120, +8 = 128 -- the same chunk, to the byte.` |
|         - |  782 | ` *` |
|         - |  783 | ` * Why it is worth having: a string-keyed lookup compares the key bytes through` |
|         - |  784 | ` * sKey.pBlob, which used to be a SEPARATE allocation somewhere else in the heap. Counted` |
|         - |  785 | ` * on the ecosystem gate's phpcs step, that is 114,418,815 dereferences to a foreign cache` |
|         - |  786 | ` * line in one run, for keys averaging 7.7 bytes -- while the three tests in front of them` |
|         - |  787 | ` * (iType, nHash, length) all live in the node's first cache line. It also deletes one` |
|         - |  788 | ` * allocation per string-keyed node.` |
|         - |  789 | ` *` |
|         - |  790 | ` * A key LONGER than this keeps the old arrangement, so the size is a tuning constant and` |
|         - |  791 | ` * not a limit. Nothing appends to a node's key after HashmapNewBlobNode builds it -- if` |
|         - |  792 | ` * that ever changes, the LOCKED blob would silently truncate rather than grow.` |
|         - |  793 | ` */` |
|         - |  794 | `#define HASHMAP_NODE_INLINE_KEY 24` |
|         - |  795 | `struct ph7_hashmap_node` |
|         - |  796 | `{` |
|         - |  797 | `	ph7_hashmap *pMap;     /* Hashmap that own this instance */` |
|         - |  798 | `	sxi32 iType;           /* Node type */` |
|         - |  799 | `	union{` |
|         - |  800 | `		sxi64 iKey;        /* Int key */` |
|         - |  801 | `		SyBlob sKey;       /* Blob key */` |
|         - |  802 | `	}xKey;` |
|         - |  803 | `	sxi32 iFlags;          /* Control flags */` |
|         - |  804 | `	sxu32 nHash;           /* Key hash value */` |
|         - |  805 | `	sxu32 nValIdx;         /* Value stored in this node */` |
|         - |  806 | `	ph7_hashmap_node *pNext,*pPrev;               /* Link to other entries [i.e: linear traversal] */` |
|         - |  807 | `	ph7_hashmap_node *pNextCollide,*pPrevCollide; /* Collision chain */` |
|         - |  808 | `	char zKey[HASHMAP_NODE_INLINE_KEY];           /* A short blob key, in the node itself */` |
|         - |  809 | `};` |
|         - |  810 | `/*` |
|         - |  811 | ` * Each active hashmap aka array in the PHP jargon is represented` |
|         - |  812 | ` * by an instance of the following structure.` |
|         - |  813 | ` */` |
|         - |  814 | `struct ph7_hashmap` |
|         - |  815 | `{` |
|         - |  816 | `	ph7_vm *pVm;                  /* VM that own this instance */` |
|         - |  817 | `	ph7_hashmap_node **apBucket;  /* Hash bucket */` |
|         - |  818 | `	ph7_hashmap_node *pFirst;     /* First inserted entry */` |
|         - |  819 | `	ph7_hashmap_node *pLast;      /* Last inserted entry */` |
|         - |  820 | `	ph7_hashmap_node *pCur;       /* Current entry */` |
|         - |  821 | `	sxu32 nSize;                  /* Bucket size */` |
|         - |  822 | `	sxu32 nEntry;                 /* Total number of inserted entries */` |
|         - |  823 | `	sxu32 (*xIntHash)(sxi64);     /* Hash function for int_keys */` |
|         - |  824 | `	sxu32 (*xBlobHash)(const void *,sxu32); /* Hash function for blob_keys */` |
|         - |  825 | `	sxi64 iNextIdx;               /* Next available automatically assigned index */` |
|         - |  826 | `	sxu8 bIntKeySeen;             /* An integer key has been inserted at least once. php 8.3` |
|         - |  827 | `	                               * carries the auto-index through NEGATIVE keys: the first` |
|         - |  828 | `	                               * int key sets the next index to key+1 even when negative` |
|         - |  829 | `	                               * ($a[-4]=x; $a[]=y stores y at -3), where it used to` |
|         - |  830 | `	                               * restart at 0. Only the FIRST key may move the index` |
|         - |  831 | `	                               * downwards, hence the flag. */` |
|         - |  832 | `	sxi64 iMaxIntKey;             /* Upper bound on the integer keys this map has held. Read` |
|         - |  833 | `	                               * only to decide whether the auto-index advance has to` |
|         - |  834 | `	                               * SCAN for a free slot: it can find one occupied only when` |
|         - |  835 | `	                               * a key ABOVE the one just inserted exists, which is` |
|         - |  836 | ``	                               * exactly `iNextIdx <= iMaxIntKey`. Without the test every`` |
|         - |  837 | `` 	                               * int-keyed store paid a failing hash lookup -- `$a[$i]=$i` `` |
|         - |  838 | ``	                               * over 200k keys ran 6x slower than `$a[]=$i`, and the`` |
|         - |  839 | `	                               * key-preserving array builtins inherited it. A stale-HIGH` |
|         - |  840 | `	                               * bound (a key that was since removed, a renumbering) only` |
|         - |  841 | `	                               * costs an extra scan, so it is never lowered. */` |
|         - |  842 | `	sxi32 iRef;                   /* Reference count. INVARIANT: the number of` |
|         - |  843 | `								   * SHARERS for copy-on-write purposes is` |
|         - |  844 | `								   * iRef minus the by-REFERENCE foreach steps` |
|         - |  845 | `								   * on pActiveSteps (a by-ref loop iterates` |
|         - |  846 | `								   * the LIVE map, php semantics) — any future` |
|         - |  847 | `								   * separate/dup gate must use the discounted` |
|         - |  848 | `								   * count like PH7_HashmapCowSeparate, never` |
|         - |  849 | `								   * raw iRef. */` |
|         - |  850 | `	sxi32 iFlags;                 /* Control flags (see HASHMAP_* below) */` |
|         - |  851 | `	sxu32 nGcRoot;                /* 1-based row in the collector's root buffer, 0 while unbuffered */` |
|         - |  852 | `	sxu8 iGcColor;                /* PH7_GC_* -- see vm_gc.c */` |
|         - |  853 | `	ph7_foreach_step *pActiveSteps; /* foreach steps currently iterating this map` |
|         - |  854 | `									 * (per-step cursors — PH7_HashmapUnlinkNode` |
|         - |  855 | `									 * advances any cursor parked on a dying node,` |
|         - |  856 | `									 * node link re-arms parked cursors) */` |
|         - |  857 | `};` |
|         - |  858 | `/*` |
|         - |  859 | ` * Hashmap control flags.` |
|         - |  860 | ` */` |
|         - |  861 | `#define HASHMAP_COUNTING 0x01 /* Set during recursive count to detect cycles */` |
|         - |  862 | `#define HASHMAP_DUMPING  0x02 /* php's GC_PROTECT_RECURSION for an array: set while a` |
|         - |  863 | `                               * dump is INSIDE this map, so a map that is its own` |
|         - |  864 | `                               * descendant renders php's *RECURSION* marker instead of` |
|         - |  865 | `                               * recursing. One bit shared by var_dump, print_r and` |
|         - |  866 | `                               * var_export, exactly as php shares its own. Read only` |
|         - |  867 | `                               * through PH7_MemObjDumpIsRecursive(). */` |
|         - |  868 | `#define HASHMAP_COMPARING 0x04 /* php's GC_PROTECT_RECURSION for an array, worn by the` |
|         - |  869 | `                               * COMPARISON walk instead of the dump walk. Set on the LEFT` |
|         - |  870 | `                               * map while PH7_HashmapCmp is inside it, so a map that is` |
|         - |  871 | `                               * its own descendant is refused with php's` |
|         - |  872 | `                               * "Nesting level too deep - recursive dependency?" instead` |
|         - |  873 | `                               * of recursing forever. Only the left one is marked --` |
|         - |  874 | `                               * zend_hash_compare protects ht1 alone, because a map` |
|         - |  875 | `                               * reachable from BOTH sides is not a cycle and marking both` |
|         - |  876 | `                               * would report one. A separate bit from HASHMAP_DUMPING: a` |
|         - |  877 | `                               * comparison can run a __toString that dumps, and a dump can` |
|         - |  878 | `                               * run a __toString that compares. */` |
|         - |  879 | `/* An instance of the following structure is the context` |
|         - |  880 | ` * for the FOREACH_STEP/FOREACH_INIT VM instructions.` |
|         - |  881 | ` * Those instructions are used to implement the 'foreach'` |
|         - |  882 | ` * statement.` |
|         - |  883 | ` * This structure is made available to these instructions` |
|         - |  884 | ` * as the P3 operand.` |
|         - |  885 | ` */` |
|         - |  886 | `struct ph7_foreach_info` |
|         - |  887 | `{` |
|         - |  888 | `	SyString sKey;      /* Key name. Empty otherwise*/` |
|         - |  889 | `	SyString sValue;    /* Value name */` |
|         - |  890 | `	sxi32 iFlags;       /* Control flags */` |
|         - |  891 | `	SySet aStep;        /* Stack of steps [i.e: ph7_foreach_step instance] */` |
|         - |  892 | `};` |
|         - |  893 | `/*` |
|         - |  894 | ` * One live walk of ONE object's property table.` |
|         - |  895 | ` *` |
|         - |  896 | ` * An object's attributes live in a SyHash, and SyHashGetNextEntry() shares a` |
|         - |  897 | `` * single cursor embedded in the table — which is wrong twice for `foreach`:`` |
|         - |  898 | ` * nested loops over one object rewind each other (an infinite loop, since the` |
|         - |  899 | ` * inner walk always leaves the cursor at the head), and the cursor is advanced` |
|         - |  900 | ` * before the body runs, so a body that unset()s the property the walk is about` |
|         - |  901 | ` * to reach freed the entry the cursor held. This is the object twin of the` |
|         - |  902 | `` * hashmap's per-loop `ph7_foreach_step::pCursor` + `pActiveSteps` pair: every`` |
|         - |  903 | ` * walker keeps its own position, and the instance keeps the list of walkers so` |
|         - |  904 | ` * an attribute added or removed under them can fix their cursors up.` |
|         - |  905 | ` */` |
|         - |  906 | `typedef struct PH7_AttrIter PH7_AttrIter;` |
|         - |  907 | `struct PH7_AttrIter` |
|         - |  908 | `{` |
|         - |  909 | `	SyHashEntry *pCursor;   /* Next attribute entry to yield; 0 once exhausted */` |
|         - |  910 | `	PH7_AttrIter *pNextIter;/* Next live walker on the instance */` |
|         - |  911 | `};` |
|         - |  912 | `struct ph7_foreach_step` |
|         - |  913 | `{` |
|         - |  914 | `	sxi32 iFlags;                   /* Control flags (see below) */` |
|         - |  915 | `	/* Iterate on those values */` |
|         - |  916 | `	union {` |
|         - |  917 | `		ph7_hashmap *pMap;          /* Hashmap [i.e: array in the PHP jargon] iteration` |
|         - |  918 | `									 * Ex: foreach(array(1,2,3) as $key=>$value){}` |
|         - |  919 | `									 */` |
|         - |  920 | `		ph7_class_instance *pThis;  /* Class instance [i.e: object] iteration */` |
|         - |  921 | `	}xIter;` |
|         - |  922 | `	ph7_class_instance *pOwner;     /* IteratorAggregate: keeps aggregate alive during foreach */` |
|         - |  923 | `	ph7_hashmap_node *pCursor;      /* Hashmap iteration: this loop's PRIVATE cursor.` |
|         - |  924 | `									 * php iterates each foreach independently — the map's` |
|         - |  925 | `									 * shared pCur would make nested loops over one array` |
|         - |  926 | `									 * rewind each other (infinite loop). */` |
|         - |  927 | `	struct VmFrame *pFrame;         /* Owning activation's frame (normalized past exception` |
|         - |  928 | `									 * frames). aStep is per-STATEMENT and shared by every` |
|         - |  929 | `									 * activation; OP_FOREACH_STEP selects the step whose` |
|         - |  930 | `									 * pFrame matches the running activation so two suspended` |
|         - |  931 | `									 * instances of one generator/fiber (or a recursive call)` |
|         - |  932 | `									 * paused in the same textual foreach cannot clash on` |
|         - |  933 | `									 * each other's cursor. */` |
|         - |  934 | `	ph7_foreach_step *pNextActive;  /* Next step on the map's pActiveSteps list */` |
|         - |  935 | `	ph7_foreach_info *pInfo;        /* The statement this step belongs to. Carried so the OWNING` |
|         - |  936 | `	                                 * FRAME can tear the step down without knowing which foreach` |
|         - |  937 | `	                                 * it came from (see pNextFrameStep). */` |
|         - |  938 | `	ph7_foreach_step *pNextFrameStep;/* Next step owned by the same activation (VmFrame::pForeachSteps).` |
|         - |  939 | `	                                 * A loop left through break/return/goto/an exception never` |
|         - |  940 | `	                                 * reaches the "no more entries" arm, so its step used to sit on` |
|         - |  941 | `	                                 * the per-STATEMENT aStep until the VM died -- reclaimed only if` |
|         - |  942 | `	                                 * a LATER activation happened to be handed the same frame` |
|         - |  943 | `	                                 * address. aStep therefore grew without bound, and INIT's` |
|         - |  944 | `	                                 * linear reclaim scan over it made every foreach in the program` |
|         - |  945 | `	                                 * quadratic. The frame that owns a step is the one that can` |
|         - |  946 | `	                                 * always end it: this list is how it finds them. */` |
|         - |  947 | `	PH7_AttrIter sAttrIter;         /* Object iteration: this loop's PRIVATE cursor over the` |
|         - |  948 | `	                                 * instance's property table (see PH7_AttrIter) */` |
|         - |  949 | `};` |
|         - |  950 | `/* Foreach step control flags */` |
|         - |  951 | `#define PH7_4EACH_STEP_HASHMAP 0x001 /* Hashmap iteration */` |
|         - |  952 | `#define PH7_4EACH_STEP_OBJECT  0x002 /* Object  iteration */` |
|         - |  953 | `#define PH7_4EACH_STEP_KEY     0x004 /* Make Key available */` |
|         - |  954 | `#define PH7_4EACH_STEP_REF     0x008 /* Pass value by reference not copy */` |
|         - |  955 | `#define PH7_4EACH_STEP_LIST    0x010 /* Value target is list() — destructure */` |
|         - |  956 | `#define PH7_4EACH_STEP_ITERATOR 0x020 /* Object implements Iterator */` |
|         - |  957 | `#define PH7_4EACH_STEP_FIRST    0x040 /* First iteration (skip next() call) */` |
|         - |  958 | `/*` |
|         - |  959 | ` * Each PH7 engine is identified by an instance of the following structure.` |
|         - |  960 | ` * Please refer to the official documentation for more information` |
|         - |  961 | ` * on how to configure your PH7 engine instance.` |
|         - |  962 | ` */` |
|         - |  963 | `struct ph7` |
|         - |  964 | `{` |
|         - |  965 | `	SyMemBackend sAllocator;     /* Low level memory allocation subsystem */` |
|         - |  966 | `	const ph7_vfs *pVfs;         /* Underlying Virtual File System */` |
|         - |  967 | `	ph7_conf xConf;              /* Configuration */` |
|         - |  968 | `#if defined(PH7_ENABLE_THREADS)` |
|         - |  969 | `	const SyMutexMethods *pMethods;  /* Mutex methods */` |
|         - |  970 | `	SyMutex *pMutex;                 /* Per-engine mutex */` |
|         - |  971 | `#endif` |
|         - |  972 | `	ph7_vm *pVms;      /* List of active VM */` |
|         - |  973 | `	sxi32 iVm;         /* Total number of active VM */` |
|         - |  974 | `	ph7 *pNext,*pPrev; /* List of active engines */` |
|         - |  975 | `	sxu32 nMagic;      /* Sanity check against misuse */` |
|         - |  976 | `};` |
|         - |  977 | `/* Code generation data structures */` |
|         - |  978 | `typedef sxi32 (*ProcErrorGen)(void *,sxi32,sxu32,const char *,...);` |
|         - |  979 | `typedef struct ph7_expr_node   ph7_expr_node;` |
|         - |  980 | `typedef struct ph7_expr_op     ph7_expr_op;` |
|         - |  981 | `typedef struct ph7_gen_state   ph7_gen_state;` |
|         - |  982 | `/*` |
|         - |  983 | ` * Lexer trivia sidecar record: a doc-comment (or, later, an attribute` |
|         - |  984 | ` * group) captured OUT of the token stream, keyed by the index the NEXT` |
|         - |  985 | ` * real token receives in the chunk's token set. sText points into the` |
|         - |  986 | ` * raw script buffer — consumers must duplicate before the buffer dies.` |
|         - |  987 | ` */` |
|         - |  988 | `typedef struct ph7_trivia ph7_trivia;` |
|         - |  989 | `struct ph7_trivia` |
|         - |  990 | `{` |
|         - |  991 | `	sxu32 nTokIdx;   /* Index of the next real token in the chunk token set */` |
|         - |  992 | `	sxu8  iKind;     /* PH7_TRIVIA_* */` |
|         - |  993 | `	SyString sText;  /* Raw span (docblock includes its delimiters) */` |
|         - |  994 | `	sxu32 nLine;     /* Line the trivia starts on */` |
|         - |  995 | `};` |
|         - |  996 | `#define PH7_TRIVIA_DOC  1 /* A doc-comment: slash-star-star ... star-slash */` |
|         - |  997 | `#define PH7_TRIVIA_ATTR 2 /* An attribute group: the span between #[ and its ] */` |
|         - |  998 | `/*` |
|         - |  999 | ` * One compiled attribute argument: an optional name (named argument) and` |
|         - | 1000 | ` * the constant expression's bytecode, evaluated lazily at` |
|         - | 1001 | ` * ReflectionAttribute::getArguments()/newInstance() time (PHP's` |
|         - | 1002 | ` * lazy-instantiation semantics).` |
|         - | 1003 | ` */` |
|         - | 1004 | `typedef struct ph7_attr_arg ph7_attr_arg;` |
|         - | 1005 | `struct ph7_attr_arg` |
|         - | 1006 | `{` |
|         - | 1007 | `	SyString sName;   /* Named-argument name (duplicated); nByte == 0 = positional */` |
|         - | 1008 | `	SySet aByteCode;  /* Compiled expression, OP_DONE(p1=1) terminated (VmInstr) */` |
|         - | 1009 | `	const void *pNativeValue; /* A NATIVE attribute's literal (PH7_NativeConstDef *), or 0.` |
|         - | 1010 | ``	                   * php declares `#[Attribute(Attribute::TARGET_CLASS)]` on two of its`` |
|         - | 1011 | `	                   * own classes, and a class declared from C has no compiler to emit` |
|         - | 1012 | `	                   * byte-code for the argument — so the value rides as a literal and` |
|         - | 1013 | `	                   * every reader takes this branch when aByteCode is empty. */` |
|         - | 1014 | `};` |
|         - | 1015 | `/*` |
|         - | 1016 | ` * One #[...] attribute as declared: the compile-time-resolved FQN and its` |
|         - | 1017 | ` * argument list.` |
|         - | 1018 | ` */` |
|         - | 1019 | `typedef struct ph7_attribute ph7_attribute;` |
|         - | 1020 | `struct ph7_attribute` |
|         - | 1021 | `{` |
|         - | 1022 | `	SyString sName;   /* Fully-qualified class name (resolved via use imports /` |
|         - | 1023 | `	                   * current namespace at compile time; duplicated) */` |
|         - | 1024 | `	SySet aArgs;      /* ph7_attr_arg records */` |
|         - | 1025 | `	sxu32 nLine;      /* Line the attribute appears on */` |
|         - | 1026 | `};` |
|         - | 1027 | `typedef struct GenBlock        GenBlock;` |
|         - | 1028 | `typedef sxi32 (*ProcLangConstruct)(ph7_gen_state *);` |
|         - | 1029 | `typedef sxi32 (*ProcNodeConstruct)(ph7_gen_state *,sxi32);` |
|         - | 1030 | `/*` |
|         - | 1031 | ` * Each supported operator [i.e: +, -, ==, *, %, >>, >=, new, etc.] is represented` |
|         - | 1032 | ` * by an instance of the following structure.` |
|         - | 1033 | ` * The PH7 parser does not use any external tools and is 100% handcoded.` |
|         - | 1034 | ` * That is, the PH7 parser is thread-safe ,full reentrant, produce consistant` |
|         - | 1035 | ` * compile-time errrors and at least 7 times faster than the standard PHP parser.` |
|         - | 1036 | ` */` |
|         - | 1037 | `struct ph7_expr_op` |
|         - | 1038 | `{` |
|         - | 1039 | `	SyString sOp;   /* String representation of the operator [i.e: "+","*","=="...] */` |
|         - | 1040 | `	sxi32 iOp;      /* Operator ID */` |
|         - | 1041 | `	sxi32 iPrec;    /* Operator precedence: 1 == Highest */` |
|         - | 1042 | `	sxi32 iAssoc;   /* Operator associativity (either left,right or non-associative) */` |
|         - | 1043 | `	sxi32 iVmOp;    /* VM OP code for this operator [i.e: PH7_OP_EQ,PH7_OP_LT,PH7_OP_MUL...]*/` |
|         - | 1044 | `};` |
|         - | 1045 | `/*` |
|         - | 1046 | ` * Each expression node is parsed out and recorded` |
|         - | 1047 | ` * in an instance of the following structure.` |
|         - | 1048 | ` */` |
|         - | 1049 | `struct ph7_expr_node` |
|         - | 1050 | `{` |
|         - | 1051 | `	const ph7_expr_op *pOp;  /* Operator ID or NULL if literal, constant, variable, function or class method call */` |
|         - | 1052 | `	ph7_expr_node *pLeft;    /* Left expression tree */` |
|         - | 1053 | `	ph7_expr_node *pRight;   /* Right expression tree */` |
|         - | 1054 | `	SyToken *pStart;         /* Stream of tokens that belong to this node */` |
|         - | 1055 | `	SyToken *pEnd;           /* End of token stream */` |
|         - | 1056 | `	sxi32 iFlags;            /* Node construct flags */` |
|         - | 1057 | `	ProcNodeConstruct xCode; /* C routine responsible of compiling this node */` |
|         - | 1058 | `	SySet aNodeArgs;         /* Node arguments. Only used by postfix operators [i.e: function call]*/` |
|         - | 1059 | `	SyString sArgName;       /* Named argument label (empty if positional) */` |
|         - | 1060 | `	ph7_expr_node *pCond;    /* Condition: Only used by the ternary operator '?:' */` |
|         - | 1061 | `};` |
|         - | 1062 | `/* Node Construct flags */` |
|         - | 1063 | `#define EXPR_NODE_PRE_INCR    0x01 /* Pre-icrement/decrement [i.e: ++$i,--$j] node */` |
|         - | 1064 | `#define EXPR_NODE_SPREAD      0x02 /* Argument unpacking: ...$expr */` |
|         - | 1065 | `#define EXPR_NODE_NAMED_ARG   0x04 /* Named argument: name: $expr */` |
|         - | 1066 | `#define EXPR_NODE_PARENS      0x08 /* Root of a parenthesized sub-expression */` |
|         - | 1067 | ``#define EXPR_NODE_FCC         0x10 /* First-class callable marker: a lone `...` as the`` |
|         - | 1068 | `                                    * whole argument list, e.g. f(...) — wrap the callee` |
|         - | 1069 | `                                    * in a Closure instead of calling it. */` |
|         - | 1070 | `/*` |
|         - | 1071 | ` * A block of instructions is recorded in an instance of the following structure.` |
|         - | 1072 | ` * This structure is used only during compile-time and have no meaning` |
|         - | 1073 | ` * during bytecode execution.` |
|         - | 1074 | ` */` |
|         - | 1075 | `struct GenBlock` |
|         - | 1076 | `{` |
|         - | 1077 | `	ph7_gen_state *pGen;  /* State of the code generator */` |
|         - | 1078 | `	GenBlock *pParent;    /* Upper block or NULL if global */` |
|         - | 1079 | `	sxu32 nFirstInstr;    /* First instruction to execute  */` |
|         - | 1080 | `	sxi32 iFlags;         /* Block control flags (see below) */` |
|         - | 1081 | `	SySet aJumpFix;       /* Jump fixup (JumpFixup instance) */` |
|         - | 1082 | `	void *pUserData;      /* Upper layer private data */` |
|         - | 1083 | `	sxu32 nLoopId;        /* This block's loop/switch id (0 when it is neither) */` |
|         - | 1084 | `	sxu32 nOuterLoopId;   /* Loop/switch that was innermost when this one was entered */` |
|         - | 1085 | `	sxu32 nScopeId;       /* Try/catch scope in effect INSIDE this block (0 = none). An` |
|         - | 1086 | `	                       * exception block mints its own; every other block inherits. */` |
|         - | 1087 | `	sxu32 nOuterScopeId;  /* Scope that was innermost when this block was entered */` |
|         - | 1088 | `	const char *zInnerTail; /* php's ", expecting" tail when a statement its grammar` |
|         - | 1089 | ``	                         * takes only at the top level (`const`) opens a statement`` |
|         - | 1090 | `	                         * directly in this block: an alternative-syntax if/elseif` |
|         - | 1091 | `	                         * body or a switch's case list name what they wanted; every` |
|         - | 1092 | `	                         * other block, NULL, names nothing */` |
|         - | 1093 | `	/* The following two fields are used only when compiling` |
|         - | 1094 | `	 * the 'do..while()' language construct.` |
|         - | 1095 | `	 */` |
|         - | 1096 | `	sxu8 bPostContinue;    /* TRUE when compiling the do..while() statement */` |
|         - | 1097 | `	SySet aPostContFix;    /* Post-continue jump fix */` |
|         - | 1098 | `};` |
|         - | 1099 | `/*` |
|         - | 1100 | ` * Code generator state is remembered in an instance of the following` |
|         - | 1101 | ` * structure. We put the information in this structure and pass around` |
|         - | 1102 | ` * a pointer to this structure, rather than pass around  all of the` |
|         - | 1103 | ` * information separately. This helps reduce the number of  arguments` |
|         - | 1104 | ` * to generator functions.` |
|         - | 1105 | ` * This structure is used only during compile-time and have no meaning` |
|         - | 1106 | ` * during bytecode execution.` |
|         - | 1107 | ` */` |
|         - | 1108 | `#define PHL_BRACE_CARRY 64` |
|         - | 1109 | `struct ph7_gen_state` |
|         - | 1110 | `{` |
|         - | 1111 | `	ph7_vm *pVm;         /* VM that own this instance */` |
|         - | 1112 | `	SyHash hLiteral;     /* Constant string Literals table */` |
|         - | 1113 | `	SyHash hNumLiteral;  /* Numeric literals table */` |
|         - | 1114 | `	SyHash hVar;         /* Collected variable hashtable */` |
|         - | 1115 | `	GenBlock *pCurrent;  /* Current processed block */` |
|         - | 1116 | `	ph7_class *pCurClass; /* Class/interface/trait/enum whose BODY is currently being compiled` |
|         - | 1117 | `	                       * (0 at top level). Saved/restored around each class-body compiler so` |
|         - | 1118 | `	                       * a nested anonymous class overrides it. Lets a const-expression that` |
|         - | 1119 | `	                       * compiles OUTSIDE any function block — a property default or a` |
|         - | 1120 | `	                       * parameter default — resolve __TRAIT__ to the enclosing trait, which` |
|         - | 1121 | `	                       * the block-chain walk alone cannot see (no func block on the chain). */` |
|         - | 1122 | `	ph7_class *pCurBase; /* The BASE CLASS of pCurClass, known while its body compiles --` |
|         - | 1123 | `	                       * pCurClass->pBase is only filled at inheritance, which runs` |
|         - | 1124 | `	                       * AFTER the body. Saved/restored with pCurClass. 0 for an` |
|         - | 1125 | ``	                       * interface (php gives one no `parent` however many it extends)`` |
|         - | 1126 | `	                       * and for a trait (which defers the question to composition). */` |
|         - | 1127 | `	GenBlock *pCurClassBlock; /* The block pCurClass's body was entered from. A function block` |
|         - | 1128 | `	                       * at or above it is OUTSIDE that body (a class declared inside a` |
|         - | 1129 | `	                       * function), so its const-expressions are not that function's code.` |
|         - | 1130 | `	                       * Saved/restored with pCurClass. */` |
|         - | 1131 | `	/* Whose SIGNATURE is being parsed, for php's scope-keyword screen -- see iSigScope. */` |
|         - | 1132 | `#define PH7_SIGSCOPE_MEMBER  0` |
|         - | 1133 | `#define PH7_SIGSCOPE_CLOSURE 1` |
|         - | 1134 | `#define PH7_SIGSCOPE_FUNC    2` |
|         - | 1135 | `	int iSigScope;       /* Whose SIGNATURE is being parsed, for php's scope-keyword screen` |
|         - | 1136 | ``	                       * (`self`/`parent`/`static` in a type). Saved and restored around`` |
|         - | 1137 | `	                       * each signature, so a nested one answers for itself:` |
|         - | 1138 | `	                       *   PH7_SIGSCOPE_MEMBER  -- a method, property or class constant:` |
|         - | 1139 | `	                       *                          the enclosing class body's scope applies` |
|         - | 1140 | `	                       *   PH7_SIGSCOPE_CLOSURE -- a closure or arrow function: EXEMPT, its` |
|         - | 1141 | `	                       *                          scope is decided when it is bound` |
|         - | 1142 | `	                       *   PH7_SIGSCOPE_FUNC    -- a named function: NO class scope, even` |
|         - | 1143 | `	                       *                          written inside a method body */` |
|         - | 1144 | `	int iInMemberDefault; /* > 0 while compiling a property/parameter DEFAULT value. Such a` |
|         - | 1145 | `	                       * const-expression belongs to pCurClass, never to a lexically-` |
|         - | 1146 | `	                       * enclosing method, so __TRAIT__ reads pCurClass directly rather than` |
|         - | 1147 | `	                       * walking the block chain (which would leak into the enclosing` |
|         - | 1148 | `	                       * function — e.g. an anonymous class's default inside a trait method). */` |
|         - | 1149 | `	GenBlock sGlobal;    /* Global block */` |
|         - | 1150 | `	ProcConsumer xErr;   /* Error consumer callback */` |
|         - | 1151 | `	void *pErrData;      /* Third argument to xErr() */` |
|         - | 1152 | `	SySet aLabel;        /* Label table */` |
|         - | 1153 | `	SySet aGoto;         /* Gotos table */` |
|         - | 1154 | `	SySet aNullsafeJmp;  /* Pending NULLSAFE_JMP instruction indices (sxu32) */` |
|         - | 1155 | `	/* An assignment whose TARGET carries dynamic subscript/property names evaluates them` |
|         - | 1156 | `	 * before the assigned value (php's order) by pushing them first and emitting the access` |
|         - | 1157 | `	 * chain last; while that chain is being emitted these nodes must not be compiled again,` |
|         - | 1158 | `	 * they are read back off the stack with PH7_OP_PICK. Installed for exactly that emission` |
|         - | 1159 | `	 * and saved/restored around it, so a nested assignment answers for itself. */` |
|         - | 1160 | `#define PH7_STORE_KEY_MAX 8` |
|         - | 1161 | `	ph7_expr_node **apStoreKey; /* the parked name expressions, in push order -- the emitting` |
|         - | 1162 | `	                             * frame's own array, borrowed for the length of that emission */` |
|         - | 1163 | `	int nStoreKey;       /* how many are parked (0 = no assignment target is being emitted) */` |
|         - | 1164 | `	int bInFramelessNsArgs; /* 1 while compiling the ARGUMENTS of an unqualified namespaced` |
|         - | 1165 | `	                      * call php makes frameless: a namespaced call inside is marked` |
|         - | 1166 | `	                      * never-frameless (VmCallArgMap.bNotFrameless), php's` |
|         - | 1167 | `	                      * in_jmp_frameless_branch. Per function body, like php's. */` |
|         - | 1168 | `	int nCommaExprOk;    /* > 0 while compiling a for() clause, the ONLY place php's grammar` |
|         - | 1169 | `	                      * allows a comma-separated expression list (PH7's comma OPERATOR` |
|         - | 1170 | `	                      * is otherwise a PH7-ism php rejects, and we remove) */` |
|         - | 1171 | `	const char *zClauseCloser; /* When an expression has a trailing token the grammar can't` |
|         - | 1172 | `	                      * absorb, the tree builder names it and, if this is set, says what` |
|         - | 1173 | `` 	                      * the enclosing construct expected: `;` after `return`, `,`/`;` `` |
|         - | 1174 | ``	                      * after `echo`, `)` for a for() post clause, `]` inside an array`` |
|         - | 1175 | `	                      * literal, and so on. Each construct saves/sets/restores it around` |
|         - | 1176 | `	                      * its expression compile. NULL means "no expecting clause" — php` |
|         - | 1177 | `	                      * prints none for a plain expression statement. */` |
|         - | 1178 | `	sxu32 nLoopId;       /* Monotonic id handed to each loop/switch block as it is entered */` |
|         - | 1179 | `	sxu32 nCurLoopId;    /* Innermost loop/switch currently open (0 = none) */` |
|         - | 1180 | `	SySet aLoopParent;   /* aLoopParent[id-1] = enclosing loop id, so the ancestry of any loop` |
|         - | 1181 | `	                      * can be walked after compilation. php's only goto restriction is` |
|         - | 1182 | `	                      * "'goto' into loop or switch statement is disallowed": a jump is` |
|         - | 1183 | `	                      * illegal exactly when the LABEL sits in a loop that does not also` |
|         - | 1184 | `	                      * enclose the GOTO. Both ends record their loop id; the fixup pass` |
|         - | 1185 | `	                      * walks up from the goto's to look for the label's. */` |
|         - | 1186 | `	sxu32 nScopeId;      /* Monotonic id handed to each try/catch/finally block entered */` |
|         - | 1187 | `	sxu32 nCurScopeId;   /* Innermost such block currently open (0 = none) */` |
|         - | 1188 | `	SySet aScope;        /* aScope[id-1] = that block's GenScope: its enclosing scope id and` |
|         - | 1189 | `	                      * its kind. Same shape and purpose as aLoopParent above, for the` |
|         - | 1190 | `	                      * other after-the-fact goto question: a jump out of a try/catch is` |
|         - | 1191 | `	                      * legal exactly when the LABEL's scope also ENCLOSES the goto, and` |
|         - | 1192 | `	                      * what it must unwind on the way is read off the chain between them` |
|         - | 1193 | `	                      * (GenStateJumpScope). Comparing NESTING DEPTHS instead cannot tell` |
|         - | 1194 | `	                      * two sibling trys apart, which let a goto jump into one — skipping` |
|         - | 1195 | `	                      * its OP_LOAD_EXCEPTION, or landing in another bytecode array. */` |
|         - | 1196 | `	SyBlob sWorker;      /* General purpose working buffer */` |
|         - | 1197 | `	SyBlob sErrBuf;      /* Error buffer */` |
|         - | 1198 | `	SyBlob sFirstErr;    /* The BARE text of the FIRST refusal in this unit -- php reports one` |
|         - | 1199 | `	                      * compile-time refusal and stops, and an include's parse error is` |
|         - | 1200 | `	                      * handed to the caller as the message of php's ParseError. */` |
|         - | 1201 | `	sxu32 nFirstErrLine; /* ...and the line it was raised on. */` |
|         - | 1202 | `	sxi32 nFatal;        /* Refusals of E_ERROR severity in this unit. php's E_COMPILE_ERROR:` |
|         - | 1203 | `	                      * uncatchable, where a PARSE error is a catchable ParseError -- so` |
|         - | 1204 | `	                      * nErr says the unit failed and this says WHICH WAY. */` |
|         - | 1205 | `	int iFatalTrace;     /* WHICH stack trace php prints under the refusal being raised -- php's` |
|         - | 1206 | `	                      * three phases, and they answer differently:` |
|         - | 1207 | `	                      *   PH7_FATAL_TRACE_COMPILE (0) the compiler refused: the active frames,` |
|         - | 1208 | `	                      *     WITHOUT the include/require/eval that is loading this unit (php` |
|         - | 1209 | `	                      *     raises it before pushing that activation) -- the common case;` |
|         - | 1210 | `	                      *   PH7_FATAL_TRACE_RUNTIME (1) php makes this one at RUN time, so the` |
|         - | 1211 | `	                      *     activation IS on its trace. A class REDECLARATION is the only one:` |
|         - | 1212 | `	                      *     php cannot early-bind a name it already holds, so DECLARE_CLASS` |
|         - | 1213 | `	                      *     reports it;` |
|         - | 1214 | `	                      *   PH7_FATAL_TRACE_NONE (2) php's PARSER refused, while reading a` |
|         - | 1215 | `	                      *     modifier run, before any op array exists -- it prints no trace at` |
|         - | 1216 | `	                      *     all.` |
|         - | 1217 | `	                      * A ONE-SHOT: the call sites that need a non-default set it just before` |
|         - | 1218 | `	                      * raising, and PH7_GenCompileError consumes it. */` |
|         - | 1219 | ``	sxi8 bRefElemIsThis; /* One-shot, set by the array-literal `&` element validator: the`` |
|         - | 1220 | ``	                      * element IS `$this`. php cannot take a reference to it (it is`` |
|         - | 1221 | `	                      * not a variable slot there), so the entry copies the object and` |
|         - | 1222 | `	                      * a write through the entry leaves the receiver alone -- where` |
|         - | 1223 | `	                      * an OP_LOAD_REF would alias the receiver and re-point it. */` |
|         - | 1224 | `	int bDeclCheck;      /* Compiling a DEFERRED declaration (conditional, or waiting on an` |
|         - | 1225 | `	                      * autoloader) once at file-compile time ONLY for php's compile-time` |
|         - | 1226 | `	                      * refusals: nothing it names is looked up or bound, nothing is` |
|         - | 1227 | `	                      * installed, and the real compile still happens where it runs. */` |
|         - | 1228 | `	int bDeclQuiet;      /* This unit is such a declaration's REAL compile: its non-fatal` |
|         - | 1229 | `	                      * diagnostics were already printed by the check, so they are not` |
|         - | 1230 | `	                      * printed twice. */` |
|         - | 1231 | `	int bParseThrows;    /* This unit's parse errors are the CALLER's to raise (include/require:` |
|         - | 1232 | `	                      * php throws a ParseError there and prints nothing until it goes` |
|         - | 1233 | `	                      * uncaught). A refusal of E_ERROR severity still prints at once. */` |
|         - | 1234 | `	struct VmClassObligeSet *pOblige; /* Where the class being linked records a variance` |
|         - | 1235 | `	                      * pair only a class nothing has loaded can decide (0: nowhere, and` |
|         - | 1236 | `	                      * the pair is accepted). php declares such a class when its` |
|         - | 1237 | `	                      * statement RUNS and settles the pair there (PH7_OP_CLASS_OBLIGE). */` |
|         - | 1238 | `	int bObligeRun;      /* ...and that settling, at run time: an unresolved pair is final. */` |
|         - | 1239 | `	ph7_class *pDeclClass; /* The class the declaration just compiled installed (0: none). */` |
|         - | 1240 | `	SyBlob sNamespace;   /* Current namespace path (e.g. "App\\Models") */` |
|         - | 1241 | `	SyHash hUseImports;      /* use imports: short alias -> FQN (classes) */` |
|         - | 1242 | `	SyHash hUseFuncImports;  /* use function imports: short alias -> FQN */` |
|         - | 1243 | `	SyHash hUseConstImports; /* use const imports: short alias -> FQN */` |
|         - | 1244 | `	SyHash hSeenClass;       /* FQNs of the classes DECLARED so far in this compile unit */` |
|         - | 1245 | `	SyHash hSeenFunc;        /* FQNs of the functions DECLARED so far in this compile unit` |
|         - | 1246 | `	                          * (both: php refuses an import a declaration already took —` |
|         - | 1247 | `	                          * these outlive a namespace switch, unlike the import tables) */` |
|         - | 1248 | `	SyToken *pIn;        /* Current processed token */` |
|         - | 1249 | `	SyToken *pEnd;       /* Last token in the stream */` |
|         - | 1250 | `	sxu32 nErr;          /* Total number of compilation error */` |
|         - | 1251 | `	SyToken *pRawIn;     /* Current processed raw token */` |
|         - | 1252 | `	SyToken *pRawEnd;    /* Last raw token in the stream */` |
|         - | 1253 | `	SySet   *pTokenSet;  /* Token containers */` |
|         - | 1254 | `	sxi8 bStrictTypes;       /* Current file's strict_types mode (0 = weak/unset, 1 = strict) */` |
|         - | 1255 | `	sxi8 bStrictTypesLocked; /* 1 once the current file has emitted any non-declare, non-empty` |
|         - | 1256 | `	                          * top-level statement (php's zend_is_first_statement with nops allowed) */` |
|         - | 1257 | `	sxi8 bNsNamed;           /* php's FC(current_namespace): a NAMED namespace is in effect */` |
|         - | 1258 | ``	sxi8 bNsBracketed;       /* php's FC(has_bracketed_namespaces): this file used `namespace X { }` */`` |
|         - | 1259 | `	sxi8 bInNsBlock;         /* php's FC(in_namespace): the cursor is inside such a block */` |
|         - | 1260 | `	sxi8 bChunkAtEof;        /* 1 when the PHP chunk being compiled ran into the end of the` |
|         - | 1261 | ``	                          * FILE rather than being closed by a `?>`. php reads the closing`` |
|         - | 1262 | `	                          * tag as a statement terminator, so only this chunk can leave one` |
|         - | 1263 | `	                          * unfinished -- and that is a parse error there. */` |
|         - | 1264 | `	sxi8 bChunkLast;         /* The same fact with nothing standing it down: bChunkAtEof is` |
|         - | 1265 | ``	                          * cleared while a `{` is open, and a bracket the input never`` |
|         - | 1266 | `	                          * closes still has to know the input ended. */` |
|         - | 1267 | `	sxu32 nChunkEofLine;     /* Line the chunk's end-of-input sits on -- its last line, which is` |
|         - | 1268 | `	                          * NOT the last TOKEN's line when trailing blank lines follow. php` |
|         - | 1269 | ``	                          * reports `unexpected end of file` at the former. */`` |
|         - | 1270 | ``	sxi32 nBraceNet;         /* `{` minus `}` over every chunk of the unit tokenized so far.`` |
|         - | 1271 | ``	                          * php's scanner refuses a `{` still open at the end of the file`` |
|         - | 1272 | `	                          * ahead of whatever statement its parser was inside. */` |
|         - | 1273 | ``	sxu32 aBraceCarry[PHL_BRACE_CARRY]; /* Lines of the `{` the unit's EARLIER chunks left open,`` |
|         - | 1274 | `	                          * innermost last -- the bottom of php's scanner stack under the` |
|         - | 1275 | ``	                          * chunk being compiled. Only a `{` outlives a `?>` in a program`` |
|         - | 1276 | `	                          * php accepts. Entries past the cap are counted, not kept. */` |
|         - | 1277 | `	sxi32 nBraceCarry;` |
|         - | 1278 | ``	sxi8 bHalted;            /* 1 once `__halt_compiler();` has been compiled in this file:`` |
|         - | 1279 | `	                          * everything after it -- the rest of the chunk, every later chunk` |
|         - | 1280 | `	                          * and every byte of inline text between them -- is DATA, and the` |
|         - | 1281 | `	                          * chunk loop stops. */` |
|         - | 1282 | `	sxi8 bHaltSeen;          /* 1 when the file HAS a halt (found by the pre-scan below, which` |
|         - | 1283 | `	                          * runs before any of it compiles because the offset may be read` |
|         - | 1284 | `	                          * ahead of the statement that sets it). */` |
|         - | 1285 | ``	sxu32 nHaltOffset;       /* What `__COMPILER_HALT_OFFSET__` expands to: the byte offset in`` |
|         - | 1286 | ``	                          * the FILE just past the halt statement's `;` -- a shebang line`` |
|         - | 1287 | `	                          * this compiler skipped included, since php counts from the first` |
|         - | 1288 | `	                          * byte on disk. Meaningful only while bHaltSeen. */` |
|         - | 1289 | `	const char *zScriptBase; /* First byte of the whole script, for the offset above. */` |
|         - | 1290 | `	sxi8 bListSrcNotRef;     /* 1 while compiling the TARGET list of an assignment whose SOURCE` |
|         - | 1291 | ``	                          * cannot hold a reference (`[&$r] = [7];`). php checks this at`` |
|         - | 1292 | `	                          * compile time, where it still knows what the right-hand side was` |
|         - | 1293 | `	                          * written as; by the time a by-ref entry is emitted the source is` |
|         - | 1294 | `	                          * an anonymous value on the stack, so the answer is carried here.` |
|         - | 1295 | ``	                          * A foreach `as` list has no such source and leaves it clear. */`` |
|         - | 1296 | `	sxi8 bInGenerator;       /* ROOT C: 1 while compiling a generator function body (a yield appears at` |
|         - | 1297 | `` 	                          * this function's own level). Gates inline try/catch/finally so `yield` `` |
|         - | 1298 | `	                          * inside a catch/finally suspends correctly; non-generators keep the` |
|         - | 1299 | `	                          * legacy detached-mini-program path. Saved/restored across nested funcs. */` |
|         - | 1300 | `	SySet aTrivia;       /* Trivia sidecar for the current chunk (ph7_trivia records from the` |
|         - | 1301 | `	                      * main-chunk tokenize calls; reset with the token set) */` |
|         - | 1302 | `	SyString sPendingDoc;/* Docblock immediately preceding the statement being dispatched;` |
|         - | 1303 | `	                      * consumed by the declaration compilers, discarded at the next` |
|         - | 1304 | `	                      * statement boundary (points into the raw script buffer) */` |
|         - | 1305 | `	SySet aPendingAttrs; /* Attribute-group trivia (ph7_trivia) bound to the statement being` |
|         - | 1306 | `	                      * dispatched; unlike docs, PHP requires attributes to be adjacent,` |
|         - | 1307 | `	                      * so this resets at every boundary */` |
|         - | 1308 | ``	SyString sPendingClosureName; /* php's `{closure:SCOPE:LINE}` name built for the closure`` |
|         - | 1309 | `	                      * whose body GenStateCompileFunc is about to compile. The caller` |
|         - | 1310 | `	                      * knows the 'function' keyword's LINE and the ENCLOSING scope; the` |
|         - | 1311 | `	                      * name must be on the ph7_vm_func before the body compiles, because` |
|         - | 1312 | `	                      * __FUNCTION__ inside it resolves at compile time. Consumed (and` |
|         - | 1313 | `	                      * cleared) the moment the function state is initialized. */` |
|         - | 1314 | `	SyString sPendingClosureScope; /* ...and the CLASS that closure belongs to, when it was written` |
|         - | 1315 | `	                               * inside a method (or inside a closure that was). php prefixes an` |
|         - | 1316 | ``	                               * argument diagnostic with it -- `C::{closure:C::m():5}` -- and`` |
|         - | 1317 | `	                               * the name above cannot be taken apart for it: a top-level` |
|         - | 1318 | ``	                               * closure's is a FILE PATH, which may hold a `::` of its own. */`` |
|         - | 1319 | `};` |
|         - | 1320 | `/* Forward references */` |
|         - | 1321 | `typedef struct ph7_vm_func_closure_env ph7_vm_func_closure_env;` |
|         - | 1322 | `typedef struct ph7_vm_func_static_var  ph7_vm_func_static_var;` |
|         - | 1323 | `typedef struct ph7_vm_func_arg ph7_vm_func_arg;` |
|         - | 1324 | `typedef struct ph7_vm_func ph7_vm_func;` |
|         - | 1325 | `/*` |
|         - | 1326 | ` * One ACTIVE include/require/eval, as php reports it in a backtrace: a frame whose` |
|         - | 1327 | ` * function is the construct's name, whose file and line are the CALL SITE, and whose` |
|         - | 1328 | ` * single argument is the unit being loaded.` |
|         - | 1329 | ` */` |
|         - | 1330 | `#define PH7_FATAL_TRACE_COMPILE 0 /* see ph7_gen_state::iFatalTrace */` |
|         - | 1331 | `#define PH7_FATAL_TRACE_RUNTIME 1` |
|         - | 1332 | `#define PH7_FATAL_TRACE_NONE    2` |
|         - | 1333 | `typedef struct VmIncFrame VmIncFrame;` |
|         - | 1334 | `struct VmIncFrame` |
|         - | 1335 | `{` |
|         - | 1336 | `	void *pFrame;      /* the VmFrame this activation was started from -- where it belongs in` |
|         - | 1337 | `	                    * the walk (php's trace is ordered by activation, and an include is` |
|         - | 1338 | `	                    * INNER to the function that wrote it) */` |
|         - | 1339 | `	SyString sFile;    /* the file the construct is written in ... */` |
|         - | 1340 | `	sxu32 nLine;       /* ...and the line */` |
|         - | 1341 | `	SyString sPath;    /* the unit being loaded: php's single argument for the frame, rendered` |
|         - | 1342 | ``	                    * as `'...'`. Empty for eval(), which php shows argument-less. */`` |
|         - | 1343 | `	const char *zName; /* "include" / "include_once" / "require" / "require_once" / "eval" */` |
|         - | 1344 | `	struct VmNativeCall *pNat; /* the running INTERNAL function that loaded the unit itself` |
|         - | 1345 | `	                    * (spl_autoload()), or 0 for a construct. php has no include frame` |
|         - | 1346 | `	                    * then -- the builtin, and whatever internal call reached for it,` |
|         - | 1347 | `	                    * are the frames, so the walk renders those records here instead. */` |
|         - | 1348 | `};` |
|         - | 1349 | `typedef struct VmFrame VmFrame;` |
|         - | 1350 | `typedef struct VmInstr VmInstr;   /* defined below; a frame names the body it is numbered for */` |
|         - | 1351 | `/* How many of a body's variables get a NUMBER (see VmFrame's aLocalSlot and` |
|         - | 1352 | ` * VmNumberLocals). The frame's 512-byte pool bucket has 136 bytes spare once the` |
|         - | 1353 | ` * struct's named fields are laid out; a code pointer takes 8 of them and 28 slots` |
|         - | 1354 | ` * take the other 112, with room left over -- raising it past 32 reallocates every` |
|         - | 1355 | ` * frame out of the 512-byte bucket into the 1024-byte one, which is a doubling of` |
|         - | 1356 | ` * the engine's per-activation memory for the tail of the distribution. A static scan` |
|         - | 1357 | ` * of the ecosystem gate's phpcs sources puts 98% of function bodies at 28 distinct` |
|         - | 1358 | ` * variable names or fewer, and a body with more than that still numbers its 28` |
|         - | 1359 | ` * most-REFERENCED ones -- so the cap costs the tail its cold names, not its hot ones. */` |
|         - | 1360 | `#define PH7_VAR_SLOT_MAX 28` |
|         - | 1361 | `/* Chains in ph7_vm::apIdleOperandStack. A parked operand stack is reusable only by a` |
|         - | 1362 | `` * call of EXACTLY its slot count, so the count picks the chain: `nCap & (N-1)`, with`` |
|         - | 1363 | ` * the exact size still checked on each node (sizes sharing the low bits share a` |
|         - | 1364 | ` * chain). Sixty-four heads over a pool capped at 256 buffers is ~4 compares. */` |
|         - | 1365 | `#define PH7_STACK_POOL_BUCKETS 64` |
|         - | 1366 | `struct VmFrame` |
|         - | 1367 | `{` |
|         - | 1368 | `	VmFrame *pParent; /* Parent frame or NULL if global scope */` |
|         - | 1369 | `	void *pUserData;  /* Upper layer private data associated with this frame */` |
|         - | 1370 | `	ph7_class_instance *pThis; /* Current class instance [i.e: the '$this' variable].NULL otherwise */` |
|         - | 1371 | `	ph7_class *pBoundScope; /* Closure::bindTo/call scope override for private/protected access (Increment 2) */` |
|         - | 1372 | `	ph7_class *pSelfClass;  /* The class this activation was reached THROUGH (php's called-scope):` |
|         - | 1373 | `	                         * the receiver's class for an instance call, the named class for a` |
|         - | 1374 | `	                         * static one, 0 for a plain function. Only a trait method needs it --` |
|         - | 1375 | `	                         * its declaring class is the TRAIT, and the class php composed it` |
|         - | 1376 | `	                         * into is found by walking this one's ancestry (a STATIC trait method` |
|         - | 1377 | `	                         * has no $this to walk from). */` |
|         - | 1378 | `	SySet sLocal;     /* Local variables container (VmSlot instance) */` |
|         - | 1379 | `	ph7_vm *pVm;      /* VM that own this frame */` |
|         - | 1380 | `	SyHash hVar;      /* Variable hashtable for fast lookup */` |
|         - | 1381 | `	SySet sArg;       /* Function arguments container */` |
|         - | 1382 | `	SySet sRef;       /* Local reference table (VmSlot instance) */` |
|         - | 1383 | `	sxi32 iFlags;     /* Frame configuration flags (See below)*/` |
|         - | 1384 | `	sxu32 iExceptionJump; /* Exception jump destination */` |
|         - | 1385 | ``	ph7_value sRet;   /* Deferred catch/finally `return` value targeting THIS body frame */`` |
|         - | 1386 | `	int bHasRet;      /* TRUE when sRet holds a live pending return */` |
|         - | 1387 | `	sxu32 nRetGen;    /* Bumped on every sRet write (see VmThrowException finally path) */` |
|         - | 1388 | `	sxu32 nCatchJmpPc;/* Pending loop jump parked by a break/continue that left a DETACHED catch` |
|         - | 1389 | `	                   * mini-program (OP_CATCH_JMP): its target pc in this body frame's` |
|         - | 1390 | `	                   * bytecode, 0 when none is armed. The sibling of bHasRet/sRet — the same` |
|         - | 1391 | `	                   * park-here, act-at-the-landing-pad contract, cleared by the same` |
|         - | 1392 | `	                   * VmClearFramePending. Consumed by the owning try's OP_POP_EXCEPTION. */` |
|         - | 1393 | `	sxu16 nCatchJmpLevels;/* Detached-container boundaries still to leave before it is taken */` |
|         - | 1394 | `	sxu16 nCatchJmpCross; /* Enclosing try activations to drain (run their finally) before it */` |
|         - | 1395 | `	sxu32 nCallLine;  /* Line of the OP_CALL that pushed this frame (0 for the global frame).` |
|         - | 1396 | `	                   * debug_backtrace() reports a frame's line as the line of the call` |
|         - | 1397 | `	                   * SITE, not of the code running inside it. */` |
|         - | 1398 | `	SyString sCallFile;/* ...and the FILE that call site is in, captured when the frame is` |
|         - | 1399 | `	                   * pushed. It cannot be derived afterwards: the caller's own file is the` |
|         - | 1400 | `	                   * defining file of the CALLER's function, and for a call made by` |
|         - | 1401 | `	                   * top-level code it is whichever included unit was executing THEN --` |
|         - | 1402 | `	                   * the include stack has moved on by the time a trace is taken. Aliases` |
|         - | 1403 | `	                   * a VM-lifetime string (a function's sFile, or an aFiles entry). */` |
|         - | 1404 | `	SyString *pNativeCaller;/* When VM_FRAME_NATIVE_CALLER is set: the name of the INTERNAL` |
|         - | 1405 | `	                   * function that reached for this callback (pVm->pCalleeName at the` |
|         - | 1406 | `	                   * dispatch, which aliases the host function's own sName and so lives` |
|         - | 1407 | `	                   * as long as the VM). php shows that builtin as a FRAME OF ITS OWN in` |
|         - | 1408 | `	                   * a backtrace, carrying the userland call site, while the callback's` |
|         - | 1409 | `	                   * own frame carries no file or line at all -- see VmBuildBacktrace. */` |
|         - | 1410 | `	ph7_class_instance *pNativeCallerThis;/* ...and, when that internal function is a METHOD,` |
|         - | 1411 | `	                   * its receiver: the trace frame then names the receiver's class` |
|         - | 1412 | ``	                   * (`Fiber->start()`) and, under PROVIDE_OBJECT, the object. Borrowed:`` |
|         - | 1413 | `	                   * the running method's own $this keeps it alive for as long as this` |
|         - | 1414 | `	                   * frame is on the chain. Only a fiber's body frame sets it today. */` |
|         - | 1415 | `	struct VmNativeCall *pNativeCallerRec;/* ...and a fiber body's: the running record of the` |
|         - | 1416 | `	                   * Fiber method that entered it last, whose arguments the trace frame` |
|         - | 1417 | `	                   * lists. Borrowed from that method's C stack, so valid only while it` |
|         - | 1418 | `	                   * runs -- re-stamped on every entry, cleared before the one entry no` |
|         - | 1419 | `	                   * Fiber method makes (the unwind of an abandoned fiber). */` |
|         - | 1420 | `	ph7_foreach_step *pForeachSteps; /* Foreach steps this activation still owns, newest first.` |
|         - | 1421 | `	                   * Every step OP_FOREACH_INIT pushes is linked here and unlinked by the one` |
|         - | 1422 | `	                   * teardown door (VmForeachStepUnlink); whatever is left when the frame dies` |
|         - | 1423 | `	                   * is released with it. Without this a broken loop's step outlived its` |
|         - | 1424 | `	                   * activation for the life of the VM -- ~140 bytes plus a retain of the` |
|         - | 1425 | `	                   * subject each -- and INIT's reclaim scan walked every one of them. */` |
|         - | 1426 | `	int nActualArgs;  /* Actual call arity (band A #4): how many arguments the CALLER passed,` |
|         - | 1427 | `	                   * stamped by the OP_CALL / generator-fiber install sites; -1 when` |
|         - | 1428 | `	                   * unknown (non-call frames) - func_num_args()/func_get_args() then fall` |
|         - | 1429 | `	                   * back to the installed-formals count. Unlike sArg this excludes` |
|         - | 1430 | `	                   * defaulted params and counts variadic-packed args individually. */` |
|         - | 1431 | `	/* Where this activation's variables live, BY NUMBER: aLocalSlot[k] is the value` |
|         - | 1432 | `	 * slot the body's k-th variable name is bound to, plus one (0 = not resolved yet).` |
|         - | 1433 | `	 * The number comes from the bytecode, not from the name -- VmNumberLocals walks a` |
|         - | 1434 | `	 * body once and writes each variable instruction's number into its nSite -- so a` |
|         - | 1435 | `	 * read is an array index and a name is hashed at most ONCE per activation instead` |
|         - | 1436 | `	 * of once per access.` |
|         - | 1437 | `	 *` |
|         - | 1438 | `	 * It replaced an eight-entry memo keyed by the name's ADDRESS, which missed 28.5%` |
|         - | 1439 | `	 * of reads on the ecosystem gate's phpcs step and whose misses were LUCK: the` |
|         - | 1440 | `	 * entry a name landed in depended on where the compiler's pool happened to intern` |
|         - | 1441 | `	 * it, so the same commit measured 343.6M, 349.4M and 414.0M frame lookups in three` |
|         - | 1442 | `	 * builds. A number the bytecode carries has none of that in it.` |
|         - | 1443 | `	 *` |
|         - | 1444 | `	 * pCodeBase is what makes a number MEAN anything: it is the instruction array this` |
|         - | 1445 | `	 * frame's numbers were assigned against, so a body sharing the frame but not the` |
|         - | 1446 | `	 * numbering -- an include, an eval, a default-argument mini-program -- is told` |
|         - | 1447 | `	 * apart by one compare and falls back to the hash. Emptied by the three doors that` |
|         - | 1448 | `	 * can move a name to another slot -- PH7_VmBindVarSlot, PH7_VmRebindVarSlot and` |
|         - | 1449 | `	 * VmUnsetVarByNameEx -- see VmVarMemoFlush. */` |
|         - | 1450 | `	const VmInstr *pCodeBase;             /* the body aLocalSlot is numbered for, 0 = none */` |
|         - | 1451 | `	sxu32 aLocalSlot[PH7_VAR_SLOT_MAX];   /* slot index + 1, 0 = this name is unresolved here */` |
|         - | 1452 | `};` |
|         - | 1453 | `/*` |
|         - | 1454 | ` * One INTERNAL function or method that is RUNNING right now.` |
|         - | 1455 | ` *` |
|         - | 1456 | ` * php gives every internal call an execute_data of its own, so a throw raised` |
|         - | 1457 | `` * inside a C body leaves a trace frame naming that body -- `#0 file(line):`` |
|         - | 1458 | `` * str_repeat()`, `#0 file(line): SplFileObject->__construct()`. PHL pushes no`` |
|         - | 1459 | ` * VmFrame for a native call, so the trace started at the CALLER and a program` |
|         - | 1460 | `` * reading it saw `#0 {main}` where php names the function that failed.`` |
|         - | 1461 | ` *` |
|         - | 1462 | ` * The records live on the C stack of the dispatch that entered them (no` |
|         - | 1463 | ` * allocation), chained newest-first through pVm->pNativeCall. pFrame is the` |
|         - | 1464 | ` * userland activation that was current at entry, which is what says whether the` |
|         - | 1465 | ` * caller was bytecode or another internal function: a record whose predecessor` |
|         - | 1466 | ` * shares its pFrame was reached from inside that predecessor, and php prints` |
|         - | 1467 | `` * such a frame with NO file and NO line at all (`#0 [internal function]:`` |
|         - | 1468 | `` * str_repeat()` under `array_map('str_repeat', ...)`).`` |
|         - | 1469 | ` */` |
|         - | 1470 | `typedef struct VmNativeCall VmNativeCall;` |
|         - | 1471 | `struct VmNativeCall` |
|         - | 1472 | `{` |
|         - | 1473 | `	SyString *pName;        /* the internal function's or method's own name */` |
|         - | 1474 | `	ph7_class *pClass;      /* declaring class for a native METHOD, 0 for a function */` |
|         - | 1475 | ``	int bStatic;            /* a method declared static: php's separator is `::` */`` |
|         - | 1476 | `	sxu32 nLine;            /* the line the call was WRITTEN on (pVm->nCurLine at entry) */` |
|         - | 1477 | `	void *pFrame;           /* VmFrame current at entry -- borrowed, never dereferenced */` |
|         - | 1478 | `	int bElided;            /* a call_user_func/_array php's compiler folded away: it has` |
|         - | 1479 | `	                         * no frame in php's trace, so the walks step over it (set by` |
|         - | 1480 | `	                         * the forward itself once it knows; see VmNativeCallPrev) */` |
|         - | 1481 | `	int bFrameless;         /* a call php's compiler turns into a FRAMELESS one (a literal` |
|         - | 1482 | ``	                         * `implode($s, $a)`): php pushes no frame for it, so what it`` |
|         - | 1483 | `	                         * calls back is called from the USER frame -- its trace still` |
|         - | 1484 | `	                         * names the builtin, but the callee binds in the caller's mode` |
|         - | 1485 | `	                         * and its diagnostics name the call site (PH7_VmCallUserFunction) */` |
|         - | 1486 | `	VmNativeCall *pPrev;    /* the internal call this one was made from, or 0 */` |
|         - | 1487 | `	sxu32 nIncDepth;        /* how many include/require/eval activations were running at` |
|         - | 1488 | `	                         * entry. Code an INTERNAL function loads (spl_autoload()) runs in` |
|         - | 1489 | `	                         * its caller's activation, so two records sharing pFrame are one` |
|         - | 1490 | `	                         * nest of internal calls only when this matches too. */` |
|         - | 1491 | ``	ph7_value **apArg;      /* the arguments as bound, for a trace's `args` -- borrowed from`` |
|         - | 1492 | `	                         * the dispatch's own vector, so read live, the way php reads its` |
|         - | 1493 | `	                         * frame's (a by-reference array shows what the body did to it` |
|         - | 1494 | `	                         * so far); 0 until the screens above the C body are passed */` |
|         - | 1495 | `	int nArg;` |
|         - | 1496 | `};` |
|         - | 1497 | `#define VM_FRAME_EXCEPTION  0x01 /* Special Exception frame */` |
|         - | 1498 | `#define VM_FRAME_THROW      0x02 /* An exception was thrown */` |
|         - | 1499 | `#define VM_FRAME_CATCH      0x04 /* Catch frame */` |
|         - | 1500 | `#define VM_FRAME_NATIVE_CALLER 0x08 /* This activation was entered by an INTERNAL function` |
|         - | 1501 | `                                  * reaching for a userland callback (array_map, usort,` |
|         - | 1502 | `                                  * array_walk, preg_replace_callback, a shutdown function,` |
|         - | 1503 | `                                  * Reflection's invoke), so there is no userland call SITE` |
|         - | 1504 | `                                  * above it. php asks the same question of` |
|         - | 1505 | `                                  * prev_execute_data -- and it asks about the frame` |
|         - | 1506 | `                                  * IMMEDIATELY above, never walking past an internal one --` |
|         - | 1507 | `                                  * to decide whether an argument diagnostic ends with` |
|         - | 1508 | ``                                  * `, called in FILE on line N`.`` |
|         - | 1509 | `                                  *` |
|         - | 1510 | `                                  * Set from the SAME latch (bCallbackWeak) that binds such` |
|         - | 1511 | `                                  * a call's arguments weakly, and the two dispatches that` |
|         - | 1512 | `                                  * do not set it are the two php also treats as userland:` |
|         - | 1513 | `                                  * call_user_func()/call_user_func_array(), whose frame` |
|         - | 1514 | `                                  * php's compiler elides, and the AUTOLOAD call, which php` |
|         - | 1515 | `                                  * makes on behalf of the code that named the class and` |
|         - | 1516 | `                                  * whose diagnostic names that code's file and line. */` |
|         - | 1517 | `#define VM_FRAME_FIBER      0x10 /* The body frame of a Fiber (VmNewExecCtx's, bytecode body` |
|         - | 1518 | `                                  * or the trampoline's transparent one). php runs a fiber's` |
|         - | 1519 | `                                  * body as a callback of whichever Fiber method entered it` |
|         - | 1520 | `                                  * LAST -- start(), resume() or throw() -- so a trace shows` |
|         - | 1521 | `                                  * the body with no file or line and that method as a frame` |
|         - | 1522 | `                                  * of its own at the resumer's site. pNativeCaller and` |
|         - | 1523 | `                                  * pNativeCallerThis name it, re-stamped on every entry;` |
|         - | 1524 | `                                  * unlike VM_FRAME_NATIVE_CALLER this says nothing about how` |
|         - | 1525 | `                                  * the body's ARGUMENTS were bound. */` |
|         - | 1526 | `#define VM_FRAME_NATIVE_TRACE 0x20 /* An INTERNAL function running in the frame above asked` |
|         - | 1527 | `                                  * for this activation, but php makes the call on behalf of` |
|         - | 1528 | `                                  * the code that called that function: an AUTOLOADER a` |
|         - | 1529 | ``                                  * builtin triggered (class_exists(), is_a(), `new`` |
|         - | 1530 | ``                                  * ReflectionClass`). php's trace gives the builtin a frame`` |
|         - | 1531 | `                                  * of its own and this one no file or line, exactly as for` |
|         - | 1532 | `                                  * VM_FRAME_NATIVE_CALLER -- but the argument binding, the` |
|         - | 1533 | ``                                  * `, called in FILE on line N` tail and the too-few wording`` |
|         - | 1534 | `                                  * all stay the caller's, which is why it is a bit of its` |
|         - | 1535 | `                                  * own. pNativeCaller names the builtin. */` |
|         - | 1536 | `#define VM_FRAME_UNSCOPED   0x40 /* A rebind gave this closure activation NO class scope` |
|         - | 1537 | ``                                  * (`bindTo(null, null)` with no `$this`): `self::`,`` |
|         - | 1538 | ``                                  * `static::` and the private members of the class it was`` |
|         - | 1539 | `                                  * written in are out of reach, as at the top level. */` |
|         - | 1540 | `/*` |
|         - | 1541 | ` * One entry of a userland handler STACK (set_error_handler /` |
|         - | 1542 | ` * set_exception_handler). php's stack has no depth limit and every entry is a` |
|         - | 1543 | `` * real one -- the `null` a reset pushes included -- so each restore brings back`` |
|         - | 1544 | ` * exactly what the matching set replaced, and nothing under it is lost.` |
|         - | 1545 | ` */` |
|         - | 1546 | `typedef struct VmHandlerSlot VmHandlerSlot;` |
|         - | 1547 | `struct VmHandlerSlot {` |
|         - | 1548 | `	ph7_value sCb;   /* the saved handler, MEMOBJ_NULL for a reset entry */` |
|         - | 1549 | `	sxi64 iLevels;   /* its set_error_handler() $error_levels (E_ALL elsewhere) */` |
|         - | 1550 | `};` |
|         - | 1551 | `/*` |
|         - | 1552 | ` * php 8's E_ALL. The default of error_reporting() AND of set_error_handler()'s` |
|         - | 1553 | ` * $error_levels, so both read it from here (E_STRICT/2048 left the set in php 8).` |
|         - | 1554 | ` */` |
|         - | 1555 | `#define PH7_E_ALL_MASK 30719` |
|         - | 1556 | `/*` |
|         - | 1557 | ` * Suspendable execution context.` |
|         - | 1558 | ` * Used by Fiber and Generator to save/restore execution state.` |
|         - | 1559 | ` */` |
|         - | 1560 | `typedef struct ph7_exec_ctx ph7_exec_ctx;` |
|         - | 1561 | `/* Execution context states */` |
|         - | 1562 | `#define PH7_CTX_STATE_CREATED    0  /* Allocated but never started */` |
|         - | 1563 | `#define PH7_CTX_STATE_RUNNING    1  /* Currently executing */` |
|         - | 1564 | `#define PH7_CTX_STATE_SUSPENDED  2  /* Paused at suspend point */` |
|         - | 1565 | `#define PH7_CTX_STATE_COMPLETED  3  /* Returned normally */` |
|         - | 1566 | `#define PH7_CTX_STATE_CLOSED     4  /* Destroyed */` |
|         - | 1567 | `/*` |
|         - | 1568 | ` * REAL COROUTINE STACKS.` |
|         - | 1569 | ` *` |
|         - | 1570 | `` * A `Fiber::suspend()` reached through a C->PHP callback -- `array_map()`'s`` |
|         - | 1571 | ``  * callback, a `usort()` comparator, `call_user_func()`, `preg_replace_callback()` `` |
|         - | 1572 | ` * -- has to park the C frame of the builtin's own loop along with the PHP one.` |
|         - | 1573 | ` * The trampoline cannot: it flattens PHP->PHP calls into records inside ONE` |
|         - | 1574 | ` * native VmByteCodeExec activation, and a builtin's loop is a real C activation` |
|         - | 1575 | ` * above it. php switches native stacks; so does this. A fiber body runs on its` |
|         - | 1576 | ` * OWN C stack, a suspend switches back to the resumer's, and everything between` |
|         - | 1577 | ` * the two -- builtin frames, mini-programs, eval'd code, catch/finally bodies --` |
|         - | 1578 | ` * simply stays where it is.` |
|         - | 1579 | ` *` |
|         - | 1580 | ` * Three ways to switch, in preference order:` |
|         - | 1581 | ` *` |
|         - | 1582 | ` *  - Win32 fibers on Windows. The OS owns the stack and the switch.` |
|         - | 1583 | ` *  - A HAND-WRITTEN switch on x86-64 ELF: six callee-saved registers, the two` |
|         - | 1584 | ` *    floating-point control words, and the stack pointer. It is preferred over` |
|         - | 1585 | `` *    ucontext for a reason that is not speed: ASan intercepts `swapcontext` and`` |
|         - | 1586 | ` *    prints "ASan doesn't fully support makecontext/swapcontext functions"` |
|         - | 1587 | ` *    unconditionally on the first call, which lands in the middle of every .phpt` |
|         - | 1588 | ` *    an ASan build runs -- and the ASan corpora are a gate. Owning the switch` |
|         - | 1589 | ` *    also drops the sigprocmask syscall glibc's swapcontext makes.` |
|         - | 1590 | `` *  - `<ucontext.h>` on the other unixes. Correct everywhere it exists; only an`` |
|         - | 1591 | ` *    ASan build on such a platform sees that warning, and none is gated.` |
|         - | 1592 | ` *` |
|         - | 1593 | ` * Elsewhere (the ESP32 port; anything with no ucontext in its libc)` |
|         - | 1594 | ` * PH7_CORO_STACK is undefined, fibers keep the record-parking path, and a` |
|         - | 1595 | ` * suspend across a C boundary keeps raising the FiberError it raised before.` |
|         - | 1596 | ` * PH7_DISABLE_CORO_STACK forces that fallback; PH7_DISABLE_CORO_ASM keeps the` |
|         - | 1597 | ` * coroutine stacks but takes ucontext instead of the written switch (the escape` |
|         - | 1598 | ` * hatch if a CET shadow stack is ever turned on by default -- glibc's` |
|         - | 1599 | `` * swapcontext knows about it and a bare `ret` to a seeded frame does not).`` |
|         - | 1600 | ` */` |
|         - | 1601 | `#if !defined(PH7_DISABLE_CORO_STACK)` |
|         - | 1602 | `# if defined(__WINNT__)` |
|         - | 1603 | `#  define PH7_CORO_STACK 1` |
|         - | 1604 | `#  define PH7_CORO_WIN32 1` |
|         - | 1605 | `# elif defined(__x86_64__) && defined(__ELF__) && !defined(PH7_DISABLE_CORO_ASM) \` |
|         - | 1606 | `    && (defined(__GNUC__) \|\| defined(__clang__))` |
|         - | 1607 | `#  define PH7_CORO_STACK 1` |
|         - | 1608 | `#  define PH7_CORO_ASM_X64 1` |
|         - | 1609 | `# elif defined(__linux__) \|\| defined(__GLIBC__) \|\| defined(__APPLE__) \` |
|         - | 1610 | `    \|\| defined(__FreeBSD__) \|\| defined(__NetBSD__) \|\| defined(__DragonFly__)` |
|         - | 1611 | `#  define PH7_CORO_STACK 1` |
|         - | 1612 | `#  define PH7_CORO_UCONTEXT 1` |
|         - | 1613 | `# endif` |
|         - | 1614 | `#endif` |
|         - | 1615 | `#ifdef PH7_CORO_STACK` |
|         - | 1616 | `/* The switchable native stack itself (vm_exec_ctx.c owns the definition: a` |
|         - | 1617 | ` * mapping plus whatever the chosen backend needs to point at it -- two saved` |
|         - | 1618 | ` * stack pointers, a ucontext_t pair, or one Win32 fiber handle). */` |
|         - | 1619 | `typedef struct VmCoro VmCoro;` |
|         - | 1620 | `/*` |
|         - | 1621 | ` * What "which side is running" means to the VM, swapped at every stack switch.` |
|         - | 1622 | ` *` |
|         - | 1623 | ` * A suspended fiber's C frames stay FROZEN, so every piece of VM state such a` |
|         - | 1624 | ` * frame reaches for -- a scalar it saved a copy of and will restore when it` |
|         - | 1625 | ` * eventually unwinds, or a stack it recorded an index into -- has to travel` |
|         - | 1626 | ` * with the fiber instead of leaking into the resumer.` |
|         - | 1627 | ` *` |
|         - | 1628 | ` * Swapping is symmetric: switching in saves the resumer's set and installs the` |
|         - | 1629 | ` * fiber's, switching out does the reverse, so a value the fiber never touches` |
|         - | 1630 | ` * comes back to the resumer unchanged either way. A fresh fiber inherits the` |
|         - | 1631 | ` * resumer's scalars, except the ones that describe the C STACK or the three` |
|         - | 1632 | ` * stacks it owns privately -- it gets a new one of each (VmCoroStateInit).` |
|         - | 1633 | ` *` |
|         - | 1634 | ` * The frame CHAIN is deliberately not here: the ordinary attach/detach a body` |
|         - | 1635 | ` * run already does (VmStartCtx / VmSuspendCtxDetach / VmFinishCtxRun) moves it,` |
|         - | 1636 | ` * and pCoroTop alone records where inside the fiber to come back to.` |
|         - | 1637 | ` */` |
|         - | 1638 | `typedef struct VmCoroVmState VmCoroVmState;` |
|         - | 1639 | `struct VmCoroVmState` |
|         - | 1640 | `{` |
|         - | 1641 | `	/* The three stacks a coroutine used to park SLICES of. With a real stack the` |
|         - | 1642 | `	 * fiber owns them OUTRIGHT: its frozen C activations recorded absolute floors` |
|         - | 1643 | `	 * into these sets (an activation's nExceptionBase, nFinallyActBase) as C` |
|         - | 1644 | `	 * LOCALS on the fiber's own stack, which nothing can reach to rebase — so the` |
|         - | 1645 | `	 * sets must never shift under them. A private set never does, and the fiber` |
|         - | 1646 | `	 * resumes at whatever depth the resumer happens to be at with no rebasing at` |
|         - | 1647 | `	 * all. An exception the fiber body does not catch therefore leaves the body` |
|         - | 1648 | `	 * as a status rather than finding the RESUMER's handler from inside the` |
|         - | 1649 | `	 * fiber's frames, which is php's model too (it is re-raised at start()/` |
|         - | 1650 | `	 * resume()). */` |
|         - | 1651 | `	SySet aException;            /* pVm->aException: this side's live try handlers */` |
|         - | 1652 | `	SySet aFinallyAction;        /* ...its pending finally actions */` |
|         - | 1653 | `	SySet aSelf;                 /* ...and its self::/static:: class stack */` |
|         - | 1654 | `	/* Then every scalar a frozen C frame has a saved copy of. A suspended fiber's` |
|         - | 1655 | `	 * frames stay put, so each of these has to travel with the fiber rather than` |
|         - | 1656 | `	 * leak into the resumer -- and come back untouched when the fiber is resumed. */` |
|         - | 1657 | `	int nVmExecDepth;            /* native activations live on THIS C stack */` |
|         - | 1658 | `	int nRecursionDepth;         /* PHP call depth this side has open */` |
|         - | 1659 | `	sxu32 nCurLine;              /* saved+restored per native activation (VmByteCodeExec) */` |
|         - | 1660 | `	sxi32 nBoundaryRc;           /* likewise: the parked C-boundary throw status */` |
|         - | 1661 | `	SyString *pCalleeName;       /* the three the OP_CALL native branch saves around xFunc */` |
|         - | 1662 | `	SyString *pNativeFrameName;` |
|         - | 1663 | `	int bHostDiscard;` |
|         - | 1664 | ``	int nErrSuppress;            /* '@' depth: a suspend inside `@f()` must not mute the resumer */`` |
|         - | 1665 | `	int nExceptDepth;` |
|         - | 1666 | `	int nExcCtorDepth;` |
|         - | 1667 | `	int nMuteThrow;              /* the muted / speculative / const-eval windows: all three are */` |
|         - | 1668 | `	int nSpeculative;            /* C regions with a matched decrement the fiber has not reached */` |
|         - | 1669 | `	sxi32 nConstEvalDepth;` |
|         - | 1670 | `	sxu32 nLazyInitLine;         /* and the lazy-initializer line override, which is depth-keyed */` |
|         - | 1671 | `	sxi32 nLazyInitDepth;        /* on nVmExecDepth and so is meaningless on the other stack */` |
|         - | 1672 | `	int nObDepth;                /* "inside an output handler": the handler's C frame is on one` |
|         - | 1673 | `	                              * stack only, so ob_get_level() must answer for the side asking */` |
|         - | 1674 | `	sxu32 nObActive;` |
|         - | 1675 | `	VmFrame *pObFrame;` |
|         - | 1676 | `	ph7_exec_ctx *pCoroCtx;      /* which fiber's stack this side is (NULL for a resumer) */` |
|         - | 1677 | `};` |
|         - | 1678 | `#endif /* PH7_CORO_STACK */` |
|         - | 1679 | `struct ph7_exec_ctx` |
|         - | 1680 | `{` |
|         - | 1681 | `	ph7_vm *pVm;              /* Owning VM */` |
|         - | 1682 | `	ph7_vm_func *pFunc;       /* The function being executed */` |
|         - | 1683 | `	VmFrame *pFrame;          /* Detached execution frame */` |
|         - | 1684 | `	ph7_value *pStack;        /* Private operand stack */` |
|         - | 1685 | `	sxu32 nStackCap;          /* Its allocated slot count (VmNewOperandStack size); grows` |
|         - | 1686 | `	                           * with pStack when an OP_SPREAD in this body reallocs it */` |
|         - | 1687 | `	sxu32 nStackOrig;         /* The ORIGINAL (ungrown) capacity — fixed at creation and used` |
|         - | 1688 | `	                           * to seed each resume's headroom reference, so a spread inside a` |
|         - | 1689 | `	                           * yield loop can't ratchet the stack up across resumes */` |
|         - | 1690 | `	sxi32 nTos;               /* Saved top-of-stack index */` |
|         - | 1691 | `	sxi32 pc;                 /* Saved program counter (resume point) */` |
|         - | 1692 | `	sxi32 iState;             /* One of PH7_CTX_STATE_* */` |
|         - | 1693 | `	sxu8 bThrew;              /* The body ENDED by letting an exception escape. php keeps the two` |
|         - | 1694 | `	                           * apart: such a fiber is terminated like any other, but getReturn()` |
|         - | 1695 | `	                           * says it threw rather than that it has not returned. */` |
|         - | 1696 | ``	sxu8 bUnboundThis;        /* The body is a Closure a rebind left with no `$this`: the receiver`` |
|         - | 1697 | `	                           * its function captured where it was created is not installed. */` |
|         - | 1698 | `	ph7_value sSuspendValue;  /* Value passed out via Fiber::suspend() / yield */` |
|         - | 1699 | `	ph7_value sRetValue;      /* Final return value */` |
|         - | 1700 | `	sxu32 nExceptionBase;     /* Exception-stack depth below this body's own handlers` |
|         - | 1701 | `	                           * (caller depth); refreshed at each resume */` |
|         - | 1702 | `	SySet aSavedException;    /* This body's own exception handlers (ph7_exception*),` |
|         - | 1703 | `	                           * parked here while suspended so a generator/fiber that` |
|         - | 1704 | `	                           * suspends inside a try does not corrupt the caller's` |
|         - | 1705 | `	                           * exception stack */` |
|         - | 1706 | `	SySet aSavedFinally;      /* ROOT C: this body's own pending finally actions` |
|         - | 1707 | `	                           * (VmFinallyAction), parked while suspended so a generator` |
|         - | 1708 | `	                           * that yields inside a finally reached by return/break/rethrow` |
|         - | 1709 | `	                           * does not leave its record on the shared VM stack (where an` |
|         - | 1710 | `	                           * out-of-order-resumed sibling generator would mis-pop it) */` |
|         - | 1711 | `	sxu32 nFinallyBase;       /* aFinallyAction depth below this body's own records */` |
|         - | 1712 | `	SySet aSavedSelf;         /* Stage 4: this coroutine's own aSelf (self::/static::)` |
|         - | 1713 | `	                           * entries, parked while suspended (ph7_class* pointers) */` |
|         - | 1714 | `	sxu32 nSelfBase;          /* aSelf depth below this coroutine's own pushes */` |
|         - | 1715 | `	ph7_class *pLsbClass;     /* The late-static-binding class the body runs under, captured` |
|         - | 1716 | `	                           * when the coroutine was CREATED. A generator body resumes long` |
|         - | 1717 | `	                           * after the call that made it returned, so pVm->aSelf no longer` |
|         - | 1718 | `` 	                           * carries the class the method was called through and `static::` `` |
|         - | 1719 | `	                           * inside the body answered "Class \"static\" not found" -- for` |
|         - | 1720 | ``	                           * `new static`, `static::method()` and `static::class` alike.`` |
|         - | 1721 | `	                           * php binds the called scope to the generator at creation and` |
|         - | 1722 | `	                           * restores it on every resume; this is that scope. Borrowed. */` |
|         - | 1723 | `	SySet aByRefArg;          /* Caller slots (sxu32) this body's by-REFERENCE parameters` |
|         - | 1724 | `	                           * alias. The body outlives its caller's frame, so whichever` |
|         - | 1725 | `	                           * of the two dies last releases the slot: the caller's` |
|         - | 1726 | `	                           * teardown counts this frame's name as a holder and skips it,` |
|         - | 1727 | `	                           * and this ctx's teardown asks PH7_VmReleaseUnheldSlot once` |
|         - | 1728 | `	                           * its own names are gone. */` |
|         - | 1729 | `	void *pPrivate;           /* Generator wrapper (ph7_generator*) or NULL for fibers */` |
|         - | 1730 | `	ph7_class_instance *pInjected; /* Generator::throw() inject-at-yield: exception to raise at` |
|         - | 1731 | `	                                * the suspended yield on the next resume, or NULL. One-shot:` |
|         - | 1732 | `	                                * consumed (cleared) by the loop-top inject check. Holds a` |
|         - | 1733 | `	                                * reference for the duration of the resume. */` |
|         - | 1734 | `	sxu8 bClosing;                 /* Set while VmCloseCtx force-drives this suspended generator's` |
|         - | 1735 | ``	                                * pending `finally` blocks at destruction (unset / out-of-scope`` |
|         - | 1736 | `	                                * / GC before completion). The body-resume entry redirects into` |
|         - | 1737 | `	                                * the innermost open try's finally chain instead of resuming at` |
|         - | 1738 | `	                                * the yield, and OP_YIELD raises PHP's "Cannot yield from finally` |
|         - | 1739 | `	                                * in a force-closed generator". Stays set for the whole close run. */` |
|         - | 1740 | ``	/* `yield from` delegation state — per generator instance, so independent`` |
|         - | 1741 | `	 * instances never clash (unlike the shared foreach aStep). */` |
|         - | 1742 | `	ph7_value sDelegate;             /* The iterable being delegated (kept alive) */` |
|         - | 1743 | `	ph7_hashmap_node *pDelegateNode; /* Array cursor: next node to read, else 0 */` |
|         - | 1744 | `	sxi32 iDelegateState;            /* 0=inactive, 1=array, 2=iterator, 3=generator */` |
|         - | 1745 | `	/* BYTECODE stage 4: deep Fiber::suspend() record-segment parking. */` |
|         - | 1746 | `	void *pParkedSegment;            /* VmParkedSegment* (opaque here): the trampoline` |
|         - | 1747 | `	                                  * record chain + innermost activation parked when a` |
|         - | 1748 | `	                                  * suspend fires inside a nested PHP call; NULL when` |
|         - | 1749 | `	                                  * suspended at the body level (pc/nTos above suffice) */` |
|         - | 1750 | `	int nBodyExecDepth;              /* pVm->nVmExecDepth of this ctx's body invocation. A` |
|         - | 1751 | `	                                  * suspend at a DEEPER native depth is inside a C->PHP` |
|         - | 1752 | `	                                  * callback (usort comparator, etc.) and cannot park` |
|         - | 1753 | `	                                  * across the native frame — it raises a catchable` |
|         - | 1754 | `	                                  * FiberError instead. Only meaningful on the fallback` |
|         - | 1755 | `	                                  * path: a fiber running on its own stack (pCoro != 0)` |
|         - | 1756 | `	                                  * suspends from any depth. */` |
|         - | 1757 | `#ifdef PH7_CORO_STACK` |
|         - | 1758 | `	VmCoro *pCoro;                   /* This fiber's own native stack, or NULL: generators never` |
|         - | 1759 | ``	                                  * take one (a `yield` is lexically in the body, so it never`` |
|         - | 1760 | `	                                  * crosses a C frame), and neither does a fiber on a build` |
|         - | 1761 | `	                                  * with no stack-switch primitive. */` |
|         - | 1762 | `	VmCoroVmState sSaved;            /* The fiber side's VM state while the resumer runs */` |
|         - | 1763 | `	VmCoroVmState sHost;             /* The resumer's, while the fiber runs */` |
|         - | 1764 | `	VmFrame *pCoroTop;               /* pVm->pFrame at the suspend the fiber is parked on --` |
|         - | 1765 | `	                                  * its innermost callee or open-try wrapper, which the` |
|         - | 1766 | `	                                  * resume makes current again so the frozen stack picks` |
|         - | 1767 | `	                                  * up where it stopped. The frame chain itself is not` |
|         - | 1768 | `	                                  * swapped: the ordinary attach/detach around a body run` |
|         - | 1769 | `	                                  * already moves it. */` |
|         - | 1770 | `	sxi32 iCoroRc;                   /* What the body invocation returned, read by the resumer` |
|         - | 1771 | `	                                  * after the final switch back (SXRET_OK / PH7_ABORT /` |
|         - | 1772 | `	                                  * PH7_EXCEPTION); PH7_SUSPEND is never stored — a suspend` |
|         - | 1773 | `	                                  * is a switch, not a return. */` |
|         - | 1774 | `	sxu8 bCoroDone;                  /* The body ran off its end: the stack is spent and must` |
|         - | 1775 | `	                                  * not be switched into again. */` |
|         - | 1776 | `	sxu8 bCoroKill;                  /* Set by the teardown before the last switch in: the` |
|         - | 1777 | `	                                  * suspend the fiber is parked on returns PH7_ABORT instead` |
|         - | 1778 | `	                                  * of a value, so its C frames unwind and free what they own` |
|         - | 1779 | `	                                  * rather than being freed underneath. */` |
|         - | 1780 | `	ph7_value sTramp;                /* The callable a body with no bytecode of its own is (a` |
|         - | 1781 | `	                                  * generator function, an internal function, a name routed` |
|         - | 1782 | `	                                  * through __call/__callStatic), run through the ordinary` |
|         - | 1783 | `	                                  * callback dispatch on this stack instead; NULL otherwise. */` |
|         - | 1784 | `	ph7_value *aTrampArg;            /* Its arguments, owned: the start() slots they came from` |
|         - | 1785 | `	                                  * are gone by the time a suspended callee reads them again */` |
|         - | 1786 | `	sxu32 nTrampArg;` |
|         - | 1787 | `	VmCallArgMap *pTrampMap;         /* start()'s named-argument map; read only by the first` |
|         - | 1788 | `	                                  * switch in, which start() itself is still waiting on */` |
|         - | 1789 | `	sxu8 bTramp;` |
|         - | 1790 | `	ph7_class_instance *pEscaped;    /* A throw the body did not catch. A fiber on its own stack` |
|         - | 1791 | `	                                  * has its own handler stack, so an unmatched throw is not` |
|         - | 1792 | `	                                  * "uncaught" -- it LEAVES the fiber, and php re-raises it` |
|         - | 1793 | `	                                  * at the start()/resume() that ran the body. This carries` |
|         - | 1794 | `	                                  * the instance across (holding a reference) for` |
|         - | 1795 | `	                                  * VmFiberRaiseEscaped to re-throw in the resumer's frame. */` |
|         - | 1796 | `#endif /* PH7_CORO_STACK */` |
|         - | 1797 | `};` |
|         - | 1798 | `/* Special return code from VmByteCodeExec signaling fiber suspension */` |
|         - | 1799 | `#define PH7_SUSPEND  0x100` |
|         - | 1800 | `/*` |
|         - | 1801 | ` * Generator wrapper around ph7_exec_ctx.` |
|         - | 1802 | ` * Adds yield key tracking on top of the suspendable execution context.` |
|         - | 1803 | ` */` |
|         - | 1804 | `typedef struct ph7_generator ph7_generator;` |
|         - | 1805 | `struct ph7_generator` |
|         - | 1806 | `{` |
|         - | 1807 | `	ph7_exec_ctx *pCtx;       /* Execution context (allocated separately) */` |
|         - | 1808 | `	ph7_value sYieldValue;    /* Last yielded value (for current()) */` |
|         - | 1809 | `	ph7_value sYieldKey;      /* Last yielded key (for key()) */` |
|         - | 1810 | `	sxi64 iImplicitKey;       /* Auto-increment key counter */` |
|         - | 1811 | `	sxu8 bAtFirstYield;       /* php's ZEND_GENERATOR_AT_FIRST_YIELD: set when the` |
|         - | 1812 | `	                           * PRIMING run suspends, cleared by every resume after` |
|         - | 1813 | `	                           * it. It is the whole of php's rewind rule — a` |
|         - | 1814 | `	                           * generator that has moved past its first yield, or` |
|         - | 1815 | `	                           * finished, cannot be rewound. */` |
|         - | 1816 | `};` |
|         - | 1817 | `/*` |
|         - | 1818 | ` * Output control buffer entry.` |
|         - | 1819 | ` */` |
|         - | 1820 | `typedef struct VmObEntry VmObEntry;` |
|         - | 1821 | `struct VmObEntry` |
|         - | 1822 | `{` |
|         - | 1823 | `	ph7_value sCallback; /* User defined callback */` |
|         - | 1824 | `	SyBlob sOB;          /* Output buffer consumer (RAW bytes: php runs the` |
|         - | 1825 | `	                      * handler on the way OUT, not on the way in) */` |
|         - | 1826 | `	ph7_int64 iFlags;    /* PH7_OB_* below, php's own numeric values. 64 bits wide` |
|         - | 1827 | `	                      * because php stores whatever it was given (minus the two` |
|         - | 1828 | `	                      * nibbles it reserves) and reports it back verbatim. */` |
|         - | 1829 | `	ph7_int64 nChunk;    /* ob_start()'s $chunk_size (0 or negative: buffer` |
|         - | 1830 | `	                      * everything). 64 bits: php accepts a chunk larger than a` |
|         - | 1831 | `	                      * 32-bit count and reports it back. */` |
|         - | 1832 | `	ph7_int64 nSize;     /* php's ALLOCATION for this buffer, which ob_get_status()` |
|         - | 1833 | `	                      * reports: 16 KB, or the chunk size rounded up to 4 KB, and` |
|         - | 1834 | `	                      * grown by php's own rule on each write. Tracked rather than` |
|         - | 1835 | `	                      * derived because the answer depends on how the bytes` |
|         - | 1836 | `	                      * ARRIVED — 40 writes of 1000 give 49152 where one write of` |
|         - | 1837 | `	                      * 40000 gives 40960. */` |
|         - | 1838 | `};` |
|         - | 1839 | `/*` |
|         - | 1840 | ` * Output-handler flags and phases. These are php's own values: the first group is` |
|         - | 1841 | `` * what ob_get_status() reports in its `flags` entry, the second what the handler`` |
|         - | 1842 | `` * receives as its `$phase` argument.`` |
|         - | 1843 | ` */` |
|         - | 1844 | `#define PH7_OB_USER      0x0001 /* Handler is a userland callback */` |
|         - | 1845 | `#define PH7_OB_CLEANABLE 0x0010` |
|         - | 1846 | `#define PH7_OB_FLUSHABLE 0x0020` |
|         - | 1847 | `#define PH7_OB_REMOVABLE 0x0040` |
|         - | 1848 | `#define PH7_OB_STDFLAGS  0x0070` |
|         - | 1849 | `#define PH7_OB_STARTED   0x1000 /* Handler has been invoked at least once */` |
|         - | 1850 | `#define PH7_OB_DISABLED  0x2000 /* Handler answered FALSE: never called again */` |
|         - | 1851 | `#define PH7_OB_PROCESSED 0x4000 /* Handler has produced output */` |
|         - | 1852 | `#define PH7_OB_PRODUCED  0x8000 /* Something wrote into this buffer while a handler ran */` |
|         - | 1853 | `/* What ob_start() keeps of the $flags it is given: everything except the phase` |
|         - | 1854 | ` * nibble and the state nibble, which are the engine's own to set. */` |
|         - | 1855 | `#define PH7_OB_FLAGMASK  (~(ph7_int64)0xF00F)` |
|         - | 1856 | `/* Phases (an op, plus PH7_OB_START until the handler has run once) */` |
|         - | 1857 | `#define PH7_OB_WRITE 0` |
|         - | 1858 | `#define PH7_OB_START 1` |
|         - | 1859 | `#define PH7_OB_CLEAN 2` |
|         - | 1860 | `#define PH7_OB_FLUSH 4` |
|         - | 1861 | `#define PH7_OB_FINAL 8` |
|         - | 1862 | `/*` |
|         - | 1863 | ` * mt_srand()/srand()'s $mode. php compares the argument against MT_RAND_PHP for` |
|         - | 1864 | ` * EQUALITY, so every other value — including an out-of-range one — selects the` |
|         - | 1865 | ` * standard generator.` |
|         - | 1866 | ` */` |
|         - | 1867 | `/* stream_wrapper_register()'s $flags: php defines this one bit. A wrapper that` |
|         - | 1868 | ` * declares itself a URL is the one allow_url_fopen and allow_url_include gate. */` |
|         - | 1869 | `#define PH7_STREAM_IS_URL 1` |
|         - | 1870 | `/*` |
|         - | 1871 | ` * The rest of the streamWrapper protocol's vocabulary, in php's own numbers.` |
|         - | 1872 | ` *` |
|         - | 1873 | `` * USE_PATH / IGNORE_URL / REPORT_ERRORS / MUST_SEEK are the `$options` bits`` |
|         - | 1874 | ` * stream_open() is handed; URL_STAT_LINK / URL_STAT_QUIET are url_stat()'s` |
|         - | 1875 | `` * `$flags` (LINK means lstat, QUIET means report a miss in silence), and NOCACHE`` |
|         - | 1876 | ` * rides beside them on every ask php's stat family makes because php's own` |
|         - | 1877 | ` * one-entry stat cache sits ABOVE that door; MKDIR_RECURSIVE is mkdir()'s;` |
|         - | 1878 | ` * META_* names the verb stream_metadata() is asked for; the OPTION_ and BUFFER_` |
|         - | 1879 | ` * pair belong to stream_set_option(), and CAST_ to stream_cast().` |
|         - | 1880 | ` */` |
|         - | 1881 | `#define PH7_STREAM_USE_PATH           1` |
|         - | 1882 | `#define PH7_STREAM_IGNORE_URL         2` |
|         - | 1883 | `#define PH7_STREAM_REPORT_ERRORS      8` |
|         - | 1884 | `#define PH7_STREAM_MUST_SEEK          16` |
|         - | 1885 | `#define PH7_URL_STAT_LINK             1` |
|         - | 1886 | `#define PH7_URL_STAT_QUIET            2` |
|         - | 1887 | `#define PH7_URL_STAT_NOCACHE          4` |
|         - | 1888 | `#define PH7_STREAM_MKDIR_RECURSIVE    1` |
|         - | 1889 | `#define PH7_STREAM_META_TOUCH         1` |
|         - | 1890 | `#define PH7_STREAM_META_OWNER_NAME    2` |
|         - | 1891 | `#define PH7_STREAM_META_OWNER         3` |
|         - | 1892 | `#define PH7_STREAM_META_GROUP_NAME    4` |
|         - | 1893 | `#define PH7_STREAM_META_GROUP         5` |
|         - | 1894 | `#define PH7_STREAM_META_ACCESS        6` |
|         - | 1895 | `#define PH7_STREAM_OPTION_BLOCKING    1` |
|         - | 1896 | `#define PH7_STREAM_OPTION_READ_BUFFER 2` |
|         - | 1897 | `#define PH7_STREAM_OPTION_WRITE_BUFFER 3` |
|         - | 1898 | `#define PH7_STREAM_OPTION_READ_TIMEOUT 4` |
|         - | 1899 | `#define PH7_STREAM_BUFFER_NONE        0` |
|         - | 1900 | `#define PH7_STREAM_BUFFER_LINE        1` |
|         - | 1901 | `#define PH7_STREAM_BUFFER_FULL        2` |
|         - | 1902 | `#define PH7_STREAM_CAST_AS_STREAM     0` |
|         - | 1903 | `#define PH7_STREAM_CAST_FOR_SELECT    3` |
|         - | 1904 | `/*` |
|         - | 1905 | ` * What PH7_StreamUserUrlStat() answered: the wrapper filled the record, the` |
|         - | 1906 | ` * wrapper declined, or no userland wrapper owns this path at all (the caller then` |
|         - | 1907 | ` * asks the VFS exactly as it always did).` |
|         - | 1908 | ` */` |
|         - | 1909 | `#define PHL_URLSTAT_OK      0` |
|         - | 1910 | `#define PHL_URLSTAT_FAIL    1` |
|         - | 1911 | `#define PHL_URLSTAT_NOWRAP (-1)` |
|         - | 1912 | `/*` |
|         - | 1913 | ` * Which member of the stat family is asking. php routes them all through one` |
|         - | 1914 | `` * `php_stat`, and the code decides three things: the flags the wrapper is handed,`` |
|         - | 1915 | ` * whether a miss is silent, and which of the thirteen fields answers.` |
|         - | 1916 | ` * The seven QUIET ones come first on purpose -- that ORDER is the test.` |
|         - | 1917 | ` */` |
|         - | 1918 | `#define PH7_STAT_ASK_EXISTS   0` |
|         - | 1919 | `#define PH7_STAT_ASK_IS_FILE  1` |
|         - | 1920 | `#define PH7_STAT_ASK_IS_DIR   2` |
|         - | 1921 | `#define PH7_STAT_ASK_IS_LINK  3` |
|         - | 1922 | `#define PH7_STAT_ASK_IS_R     4` |
|         - | 1923 | `#define PH7_STAT_ASK_IS_W     5` |
|         - | 1924 | `#define PH7_STAT_ASK_IS_X     6` |
|         - | 1925 | `#define PH7_STAT_ASK_SIZE     7` |
|         - | 1926 | `#define PH7_STAT_ASK_ATIME    8` |
|         - | 1927 | `#define PH7_STAT_ASK_MTIME    9` |
|         - | 1928 | `#define PH7_STAT_ASK_CTIME    10` |
|         - | 1929 | `#define PH7_STAT_ASK_OWNER    11` |
|         - | 1930 | `#define PH7_STAT_ASK_GROUP    12` |
|         - | 1931 | `#define PH7_STAT_ASK_INODE    13` |
|         - | 1932 | `#define PH7_STAT_ASK_PERMS    14` |
|         - | 1933 | `#define PH7_STAT_ASK_TYPE     15` |
|         - | 1934 | `#define PH7_STAT_ASK_STAT     16` |
|         - | 1935 | `#define PH7_STAT_ASK_LSTAT    17` |
|         - | 1936 | `/* php's S_IFMT decode, spelled in octal so it means the same on every port. */` |
|         - | 1937 | `#define PH7_S_IFMT   0170000` |
|         - | 1938 | `#define PH7_S_IFIFO  0010000` |
|         - | 1939 | `#define PH7_S_IFCHR  0020000` |
|         - | 1940 | `#define PH7_S_IFDIR  0040000` |
|         - | 1941 | `#define PH7_S_IFBLK  0060000` |
|         - | 1942 | `#define PH7_S_IFREG  0100000` |
|         - | 1943 | `#define PH7_S_IFLNK  0120000` |
|         - | 1944 | `#define PH7_S_IFSOCK 0140000` |
|         - | 1945 | `/* stream_socket_client()'s $flags. CONNECT is its default; without it php` |
|         - | 1946 | ` * creates no socket at all. PERSISTENT is what pfsockopen() means, and is the` |
|         - | 1947 | ` * only one that changes what a second call ANSWERS. */` |
|         - | 1948 | `#define PH7_STREAM_CLIENT_PERSISTENT    1` |
|         - | 1949 | `#define PH7_STREAM_CLIENT_ASYNC_CONNECT 2` |
|         - | 1950 | `#define PH7_STREAM_CLIENT_CONNECT       4` |
|         - | 1951 | `/* One live persistent socket: php's registry key is the address the opener was` |
|         - | 1952 | ` * given, spelling included ("localhost:80" and "127.0.0.1:80" are two). The` |
|         - | 1953 | ` * handle itself is the VFS's io_private, declared with the rest of that layer. */` |
|         - | 1954 | `typedef struct io_private io_private;` |
|         - | 1955 | `typedef struct VmPersistSock VmPersistSock;` |
|         - | 1956 | `struct VmPersistSock` |
|         - | 1957 | `{` |
|         - | 1958 | `	char zKey[320];` |
|         - | 1959 | `	io_private *pDev;` |
|         - | 1960 | `};` |
|         - | 1961 | `/* stream_socket_server()'s $flags. php keeps the two apart because a DATAGRAM` |
|         - | 1962 | ` * server is bound and never listens; LISTEN is what makes a bound socket a` |
|         - | 1963 | ` * stream server, and dropping it leaves a socket nothing can connect to. */` |
|         - | 1964 | `#define PH7_STREAM_SERVER_BIND   4` |
|         - | 1965 | `#define PH7_STREAM_SERVER_LISTEN 8` |
|         - | 1966 | `/* stream_socket_shutdown()'s $mode and the recvfrom/sendto flags: php's own` |
|         - | 1967 | ` * numbering, which is NOT the OS's (MSG_OOB and MSG_PEEK are mapped in net.c). */` |
|         - | 1968 | `#define PH7_STREAM_SHUT_RD   0` |
|         - | 1969 | `#define PH7_STREAM_SHUT_WR   1` |
|         - | 1970 | `#define PH7_STREAM_SHUT_RDWR 2` |
|         - | 1971 | `#define PH7_STREAM_OOB       1` |
|         - | 1972 | `#define PH7_STREAM_PEEK      2` |
|         - | 1973 | `#define PH7_MT_RAND_MT19937 0` |
|         - | 1974 | `#define PH7_MT_RAND_PHP     1` |
|         - | 1975 | `/*` |
|         - | 1976 | ` * HTTP response header entry.` |
|         - | 1977 | ` * Stored in ph7_vm.aResponseHeaders (a SySet of VmResponseHeader).` |
|         - | 1978 | ` */` |
|         - | 1979 | `typedef struct VmResponseHeader VmResponseHeader;` |
|         - | 1980 | `struct VmResponseHeader` |
|         - | 1981 | `{` |
|         - | 1982 | `	SyString sName;   /* Header name (e.g. "Content-Type"), case-preserving */` |
|         - | 1983 | `	SyString sValue;  /* Header value (e.g. "text/html") */` |
|         - | 1984 | `};` |
|         - | 1985 | `/*` |
|         - | 1986 | ` * Each collected function argument is recorded in an instance` |
|         - | 1987 | ` * of the following structure.` |
|         - | 1988 | ` * Note that as an extension, PH7 implements full type hinting` |
|         - | 1989 | ` * which mean that any function can have it's own signature.` |
|         - | 1990 | ` * Example:` |
|         - | 1991 | ` *      function foo(int $a,string $b,float $c,ClassInstance $d){}` |
|         - | 1992 | ` * This is how the powerful function overloading mechanism is` |
|         - | 1993 | ` * implemented.` |
|         - | 1994 | ` * Note that as an extension, PH7 allow function arguments to have` |
|         - | 1995 | ` * any complex default value associated with them unlike the standard` |
|         - | 1996 | ` * PHP engine.` |
|         - | 1997 | ` * Example:` |
|         - | 1998 | ` *    function foo(int $a = rand() & 1023){}` |
|         - | 1999 | ` *    now, when foo is called without arguments [i.e: foo()] the` |
|         - | 2000 | ` *    $a variable (first parameter) will be set to a random number` |
|         - | 2001 | ` *    between 0 and 1023 inclusive.` |
|         - | 2002 | ` * Refer to the official documentation for more information on this` |
|         - | 2003 | ` * mechanism and other extension introduced by the PH7 engine.` |
|         - | 2004 | ` */` |
|         - | 2005 | `struct ph7_vm_func_arg` |
|         - | 2006 | `{` |
|         - | 2007 | `	SyString sName;      /* Argument name */` |
|         - | 2008 | `	SySet aByteCode;     /* Compiled default value associated with this argument */` |
|         - | 2009 | `	sxu32 nType;         /* Type of this argument [i.e: array, int, string, float, object, etc.] */` |
|         - | 2010 | `	SyString sClass;     /* Class name if the argument expect a class instance [i.e: function foo(BaseClass $bar){} ] */` |
|         - | 2011 | `	sxi32 iFlags;        /* Configuration flags */` |
|         - | 2012 | `	SySet aUnionAlts;    /* Union type alternatives (ph7_type_alt). Empty unless VM_FUNC_ARG_UNION is set. */` |
|         - | 2013 | `	SyString sTypeName;  /* Original type text for error messages, normalized in canonical PHP order */` |
|         - | 2014 | `	sxi32 iPromoteVis;   /* PH7_CLASS_PROT_* when VM_FUNC_ARG_PROMOTED is set */` |
|         - | 2015 | `	SySet aAttrs;        /* Declared #[...] attributes (ph7_attribute records) */` |
|         - | 2016 | `	sxu32 nLine;         /* The line php's RECV for this parameter carries -- its TYPE's` |
|         - | 2017 | ``	                      * first token, else its `$name` (modifiers and attributes do not`` |
|         - | 2018 | `	                      * count). An argument refused against it is reported THERE, not` |
|         - | 2019 | `	                      * at the call. 0 = not compiled from source (native, synthesized). */` |
|         - | 2020 | `};` |
|         - | 2021 | `/*` |
|         - | 2022 | ` * One alternative within a union type declaration. Used by parameters,` |
|         - | 2023 | `` * return types, and properties when the declaration is `T1\|T2\|...`,`` |
|         - | 2024 | `` * `A&B` (intersection), or `(A&B)\|C` (DNF).`` |
|         - | 2025 | ` */` |
|         - | 2026 | `typedef struct ph7_type_alt ph7_type_alt;` |
|         - | 2027 | `struct ph7_type_alt` |
|         - | 2028 | `{` |
|         - | 2029 | `	sxu32 nType;     /* MEMOBJ_* bitmask, or SXU32_HIGH for a class/interface alternative */` |
|         - | 2030 | `	SyString sClass; /* Class/interface name when nType == SXU32_HIGH */` |
|         - | 2031 | `	sxu32 nGroup;    /* Intersection-group id: atoms sharing a group are ANDed (A&B),` |
|         - | 2032 | `	                  * distinct groups are ORed. A pure union is one atom per group. */` |
|         - | 2033 | `};` |
|         - | 2034 | `/* Maximum alternatives in one type declaration; bounds the on-stack atom array` |
|         - | 2035 | ` * in the parser and the per-group tally in the enforcer. Larger than any real` |
|         - | 2036 | ` * union/DNF type. */` |
|         - | 2037 | `#define PHL_UNION_MAX_ALTS 32` |
|         - | 2038 | `/*` |
|         - | 2039 | ` * Each static variable is parsed out and remembered in an instance` |
|         - | 2040 | ` * of the following structure.` |
|         - | 2041 | ` * Note that as an extension, PH7 allow static variable have` |
|         - | 2042 | ` * any complex default value associated with them unlike the standard` |
|         - | 2043 | ` * PHP engine.` |
|         - | 2044 | ` * Example:` |
|         - | 2045 | ` *   static $rand_str = 'PH7'.rand_str(3); // Concatenate 'PH7' with` |
|         - | 2046 | ` *                                         // a random three characters(English alphabet)` |
|         - | 2047 | ` *   var_dump($rand_str);` |
|         - | 2048 | ` *   //You should see something like this` |
|         - | 2049 | ` *   string(6 'PH7awt');` |
|         - | 2050 | ` */` |
|         - | 2051 | `struct ph7_vm_func_static_var` |
|         - | 2052 | `{` |
|         - | 2053 | `	SyString sName;   /* Static variable name */` |
|         - | 2054 | `	SySet aByteCode;  /* Compiled initialization expression  */` |
|         - | 2055 | `	sxu32 nIdx;       /* Object index in the global memory object container */` |
|         - | 2056 | `};` |
|         - | 2057 | `/*` |
|         - | 2058 | ` * Each imported variable from the outside closure environnment is recoded` |
|         - | 2059 | ` * in an instance of the following structure.` |
|         - | 2060 | ` */` |
|         - | 2061 | `struct ph7_vm_func_closure_env` |
|         - | 2062 | `{` |
|         - | 2063 | `	SyString sName;   /* Imported variable name */` |
|         - | 2064 | `	int iFlags;       /* Control flags */` |
|         - | 2065 | ``	sxu32 nLine;      /* Source line of this `use ($x)` capture (0 = unknown/implicit):`` |
|         - | 2066 | `					   * php reports an undefined by-value capture's E_WARNING at the` |
|         - | 2067 | `					   * variable's own line, which can differ from the closure keyword's` |
|         - | 2068 | ``					   * line when the `use` clause wraps. Set only for explicit captures. */`` |
|         - | 2069 | `	ph7_value sValue; /* Imported variable value */` |
|         - | 2070 | `	sxu32 nIdx;       /* Reference to the bounded variable if passed by reference` |
|         - | 2071 | `					   *[Example:` |
|         - | 2072 | `					   *  $x = 1;` |
|         - | 2073 | `					   *  $closure = function() use (&$x) { ++$x; }` |
|         - | 2074 | `					   *  $closure();` |
|         - | 2075 | `					   *]` |
|         - | 2076 | `					   */` |
|         - | 2077 | `};` |
|         - | 2078 | `/* Function configuration flags */` |
|         - | 2079 | `#define VM_FUNC_ARG_BY_REF   0x001 /* Argument passed by reference */` |
|         - | 2080 | `#define VM_FUNC_ARG_HAS_DEF  0x002 /* Argument has default value associated with it */` |
|         - | 2081 | `#define VM_FUNC_REF_RETURN   0x004 /* Return by reference */` |
|         - | 2082 | `#define VM_FUNC_CLASS_METHOD 0x008 /* VM function is in fact a class method */` |
|         - | 2083 | `#define VM_FUNC_CLOSURE      0x010 /* VM function is a closure */` |
|         - | 2084 | `#define VM_FUNC_ARG_IGNORE   0x020 /* Do not install argument in the current frame */` |
|         - | 2085 | `#define VM_FUNC_GENERATOR    0x040 /* VM function is a generator (contains yield) */` |
|         - | 2086 | `#define VM_FUNC_ARG_VARIADIC 0x080 /* Argument is variadic (...$args) */` |
|         - | 2087 | `#define VM_FUNC_ARG_NULLABLE 0x100 /* Argument type is nullable (?type or T\|null) */` |
|         - | 2088 | `#define VM_FUNC_ARG_UNION    0x200 /* Argument has a union type (use aUnionAlts) */` |
|         - | 2089 | `#define VM_FUNC_ARG_PROMOTED 0x400 /* Constructor promoted property (iPromoteVis holds visibility) */` |
|         - | 2090 | `#define VM_FUNC_ARG_READONLY 0x800 /* Promoted property is readonly (PHP 8.1) */` |
|         - | 2091 | `#define VM_FUNC_RETURN_NULLABLE 0x1000 /* Return type is nullable (?T, T\|null, A\|B\|null) — func-level */` |
|         - | 2092 | `#define VM_FUNC_INTERNAL     0x2000 /* Function was defined while compiling a builtin chunk` |
|         - | 2093 | `                                     * (embedded PHP library). Reflection reports it as internal:` |
|         - | 2094 | `                                     * isInternal() true, getFileName() false. */` |
|         - | 2095 | ``#define VM_FUNC_STATIC_CL    0x4000 /* Static closure/arrow fn (`static function () {}` /`` |
|         - | 2096 | ``                                     * `static fn () =>`): no $this auto-capture, bind refused. */`` |
|         - | 2097 | `#define VM_FUNC_ARG_PRIV_SET 0x8000  /* Promoted property is private(set) (PHP 8.4) */` |
|         - | 2098 | `#define VM_FUNC_ARG_PROT_SET 0x10000 /* Promoted property is protected(set) (PHP 8.4) */` |
|         - | 2099 | ``#define VM_FUNC_HOOK_SET_EXPR 0x20000 /* `set => expr` property hook (PHP 8.4): the dispatcher`` |
|         - | 2100 | `                                       * stores the implicit return value into the backing slot */` |
|         - | 2101 | `#define VM_FUNC_BOUND        0x40000 /* Bound by an UNCONDITIONAL top-level declaration; a second such` |
|         - | 2102 | `                                      * binding of the same name fatals ("Cannot redeclare function ..."),` |
|         - | 2103 | `                                      * matching PHP. Conditional declarations are not marked. */` |
|         - | 2104 | ``#define VM_FUNC_ARROW        0x80000 /* Arrow function (`fn()=>expr`): its aClosureEnv captures are ALL`` |
|         - | 2105 | ``                                       * implicit (auto-scanned from the body), never an explicit `use` `` |
|         - | 2106 | `                                      * clause. php does not warn about an undefined auto-capture at` |
|         - | 2107 | `                                      * closure creation — the read fires the warning inside the body —` |
|         - | 2108 | `                                      * so the OP_LOAD_CLOSURE undefined-capture warning is suppressed. */` |
|         - | 2109 | `#define VM_FUNC_NATIVE       0x100000 /* The body is a C routine, not bytecode: ph7_vm_func::pNative` |
|         - | 2110 | `                                       * holds it and aByteCode stays EMPTY. OP_CALL branches to the` |
|         - | 2111 | `                                       * host-function path (no frame, no call record, no operand` |
|         - | 2112 | `                                       * stack) while every step BEFORE the branch — the sVmName` |
|         - | 2113 | `                                       * lookup, $this/self resolution, visibility — runs unchanged,` |
|         - | 2114 | `                                       * so a native method inherits, overrides and dispatches like` |
|         - | 2115 | `                                       * any other. This is what lets a builtin class own its C code` |
|         - | 2116 | ``                                       * as a METHOD instead of a global `__prefix_verb` thunk. */`` |
|         - | 2117 | `#define VM_FUNC_NATIVE_STATIC 0x200000 /* A VM_FUNC_NATIVE method declared static. Staticness is` |
|         - | 2118 | `                                       * otherwise recorded only on ph7_class_method::iFlags, which` |
|         - | 2119 | `                                       * the OP_CALL dispatcher does not hold — and it must know,` |
|         - | 2120 | `                                       * because the method path falls back to the CALLER's $this` |
|         - | 2121 | `                                       * when the target slot carries a class name rather than an` |
|         - | 2122 | `                                       * object. For a bytecode method that fallback is harmless;` |
|         - | 2123 | `                                       * for a native one it would hand the body a receiver on a` |
|         - | 2124 | ``                                       * `C::m()` call and shift how it reads its arguments. Set by`` |
|         - | 2125 | `                                       * the native builder only: the compiler's behaviour for` |
|         - | 2126 | `                                       * bytecode methods is deliberately left untouched. */` |
|         - | 2127 | `#define VM_FUNC_NODISCARD 0x400000 /* php 8.5's #[\NoDiscard]: a caller that DROPS this` |
|         - | 2128 | `                                       * function's answer is warned at the call site. Set by` |
|         - | 2129 | `                                       * the compiler from the declared attribute, and by the` |
|         - | 2130 | `                                       * native builder for the internal members php marks` |
|         - | 2131 | `                                       * (PH7_VmFuncSetNoDiscard). The message, when there is` |
|         - | 2132 | `                                       * one, comes from zNoDiscard for a native member and` |
|         - | 2133 | `                                       * from the attribute's own argument for a compiled one. */` |
|         - | 2134 | ``#define VM_FUNC_ARG_FINAL 0x800000 /* PHP 8.4's `final` on a PROMOTED property. Kept apart from`` |
|         - | 2135 | `                                    * the class-body rule it mirrors: php refuses` |
|         - | 2136 | ``                                    * `final private $p` in a class body and ACCEPTS the same`` |
|         - | 2137 | `                                    * pair here (modifiers 36), so the screen cannot be shared. */` |
|         - | 2138 | ``#define VM_FUNC_USES_THIS 0x1000000 /* php's ZEND_ACC_USES_THIS: the body names `$this` literally`` |
|         - | 2139 | `                                    * (a nested closure's or arrow fn's body counts for that one,` |
|         - | 2140 | ``                                    * not this; `$$n` and eval() never count). Set by`` |
|         - | 2141 | `                                    * PH7_CompileVariable. What it decides: a closure that uses its` |
|         - | 2142 | ``                                    * `$this` refuses to be unbound, one that does not drops it. */`` |
|         - | 2143 | `/* next free bit: 0x2000000 */` |
|         - | 2144 | `/*` |
|         - | 2145 | ` * Each user defined function is parsed out and stored in an instance` |
|         - | 2146 | ` * of the following structure.` |
|         - | 2147 | ` * PH7 introduced some powerfull extensions to the PHP 5 programming` |
|         - | 2148 | ` * language like function overloading, type hinting, complex default` |
|         - | 2149 | ` * arguments values and many more.` |
|         - | 2150 | ` * Please refer to the official documentation for more information.` |
|         - | 2151 | ` */` |
|         - | 2152 | `struct ph7_vm_func` |
|         - | 2153 | `{` |
|         - | 2154 | `	SySet aArgs;         /* Expected arguments (ph7_vm_func_arg instance) */` |
|         - | 2155 | `	SySet aStatic;       /* Static variable (ph7_vm_func_static_var instance) */` |
|         - | 2156 | `	SyString sName;      /* Function name */` |
|         - | 2157 | `	SySet aByteCode;     /* Compiled function body */` |
|         - | 2158 | `	SySet aClosureEnv;   /* Closure environment (ph7_vm_func_closure_env instace) */` |
|         - | 2159 | `	sxi32 iFlags;        /* VM function configuration */` |
|         - | 2160 | `	SyString sSignature; /* Function signature used to implement function overloading` |
|         - | 2161 | `						  * (Refer to the official docuemntation for more information` |
|         - | 2162 | `						  *  on this powerfull feature)` |
|         - | 2163 | `						  */` |
|         - | 2164 | `	sxu32 nReturnType;   /* Return type hint (MEMOBJ_* constant, MEMOBJ_VOID, or SXU32_HIGH for class) */` |
|         - | 2165 | `	SyString sReturnClass; /* Class name when nReturnType == SXU32_HIGH */` |
|         - | 2166 | `	SySet aReturnUnion;  /* Return-type union alternatives (ph7_type_alt). Empty unless union return. */` |
|         - | 2167 | `	SyString sReturnTypeName; /* Original return-type text for error messages, in canonical PHP order */` |
|         - | 2168 | `	sxu8 bStrictTypes;   /* 1 if defining file declared strict_types=1 (governs return-value coercion) */` |
|         - | 2169 | `	sxu16 nLocalName;    /* How many of this body's variable names VmNumberLocals gave a` |
|         - | 2170 | `	                      * number to, capped at PH7_VAR_SLOT_MAX. Meaningful only once` |
|         - | 2171 | `	                      * bNumbered is set; 0 with bNumbered set means the body names no` |
|         - | 2172 | `	                      * variable the compiler wrote down. */` |
|         - | 2173 | `	sxu8 bNumbered;      /* 1 = VmNumberLocals has walked this body. Lazily, on the first` |
|         - | 2174 | `	                      * activation, exactly like nMaxStack below and for the same` |
|         - | 2175 | `	                      * reason: a body that has not been walked yet just walks, where a` |
|         - | 2176 | `	                      * compile-time pass would have to answer for every path that can` |
|         - | 2177 | `	                      * build one. */` |
|         - | 2178 | `	sxu32 nMaxStack;     /* Cached operand-stack depth for this body (BYTECODE stage 7):` |
|         - | 2179 | `						  * 0 = not yet computed; otherwise the number of slots to allocate` |
|         - | 2180 | `						  * per call (a tight bound from VmComputeMaxStack, or the whole` |
|         - | 2181 | `						  * instruction count when the body is not statically modelable). */` |
|         - | 2182 | `	SySet aAttrs;        /* Declared #[...] attributes (ph7_attribute records) */` |
|         - | 2183 | `	SyString sDoc;       /* Doc-comment immediately preceding the declaration, delimiters` |
|         - | 2184 | `						  * included (duplicated into the VM allocator); nByte == 0 = none,` |
|         - | 2185 | `						  * Reflection getDocComment() then reports false. */` |
|         - | 2186 | `	SyString sFile;      /* Path of the defining file (aliases the VM-lifetime dup in pVm->aFiles).` |
|         - | 2187 | `						  * nByte == 0 when unknown (builtin chunk, eval, direct API compile):` |
|         - | 2188 | `						  * Reflection getFileName() then reports false. */` |
|         - | 2189 | `	sxu32 nLine;         /* Line of the 'function'/'fn' keyword (Reflection getStartLine) */` |
|         - | 2190 | `	sxu32 nEndLine;      /* Line of the closing brace of the body (Reflection getEndLine) */` |
|         - | 2191 | `	SyString sClosureName; /* A closure/arrow function's php-VISIBLE name, php 8.4's` |
|         - | 2192 | ``	                      * `{closure:SCOPE:LINE}` (Zend/zend_compile.c, zend_begin_func_decl).`` |
|         - | 2193 | `	                      * Built at COMPILE time — the scope part names the ENCLOSING` |
|         - | 2194 | `	                      * function, which only the compiler knows — and duplicated into the` |
|         - | 2195 | `	                      * VM allocator. sName stays the synthesized unique lookup key` |
|         - | 2196 | `	                      * ("[lambda_3]"); this is what __FUNCTION__, a backtrace, Reflection` |
|         - | 2197 | `	                      * and is_callable() report. nByte == 0 for anything but a closure. */` |
|         - | 2198 | ``	SyString sClosureScope;/* The class the closure was written inside, for the `C::` php prefixes`` |
|         - | 2199 | `	                        * its argument diagnostics with. Empty for a top-level one. */` |
|         - | 2200 | `	void *pUserData;     /* Upper layer private data associated with this instance */` |
|         - | 2201 | `	sxu8 bQueued;        /* VM_FUNC_CLOSURE only: already on the VM's pending-free list */` |
|         - | 2202 | `	sxi32 nRef;          /* VM_FUNC_CLOSURE only: how many things still need this` |
|         - | 2203 | `` 	                      * per-instantiation copy -- the Closure OBJECTS whose `$__fn` `` |
|         - | 2204 | `	                      * names it, plus every activation currently running it. At zero` |
|         - | 2205 | `	                      * it is unregistered from hFunction and freed. Every closure` |
|         - | 2206 | `	                      * expression evaluated used to mint one of these and leave it in` |
|         - | 2207 | `	                      * the function table for the life of the VM: ~3 KB per closure,` |
|         - | 2208 | `	                      * which on a real workload (phpcs) was over half the engine's` |
|         - | 2209 | `	                      * whole memory footprint. */` |
|         - | 2210 | `	void *pLsbClass;     /* For a closure: the late-static-binding class captured at its` |
|         - | 2211 | ``	                      * creation site (ph7_class*), so `static::` inside the body`` |
|         - | 2212 | `	                      * resolves like php. NULL for a plain function/method. */` |
|         - | 2213 | `	ph7_user_func *pNative; /* VM_FUNC_NATIVE only: the C body. A ph7_user_func rather than a` |
|         - | 2214 | `	                      * bespoke record because that struct ALREADY carries everything the` |
|         - | 2215 | `	                      * host-call path reads — xFunc, pUserData, sName, the min/max arity` |
|         - | 2216 | `	                      * bounds, zSig/zRet and nByRefMask — so the existing OP_CALL foreign` |
|         - | 2217 | `	                      * block, VmInitCallContext, ph7_context_user_data(), ph7_function_name()` |
|         - | 2218 | `	                      * and VmEnforceBuiltinArgTypes all work on it verbatim. It is NOT` |
|         - | 2219 | `	                      * registered in pVm->hHostFunction: it hangs off this method alone and` |
|         - | 2220 | `	                      * is reachable only through the method, never as a global name. */` |
|         - | 2221 | `	ph7_vm_func *pNextName; /* Next VM function with the same name as this one */` |
|         - | 2222 | `};` |
|         - | 2223 | `/* Forward reference */` |
|         - | 2224 | `typedef struct ph7_builtin_constant ph7_builtin_constant;` |
|         - | 2225 | `typedef struct ph7_builtin_func ph7_builtin_func;` |
|         - | 2226 | `/*` |
|         - | 2227 | ` * Each built-in foreign function (C function) is stored in an` |
|         - | 2228 | ` * instance of the following structure.` |
|         - | 2229 | ` * Please refer to the official documentation for more information` |
|         - | 2230 | ` * on how to create/install foreign functions.` |
|         - | 2231 | ` */` |
|         - | 2232 | `struct ph7_builtin_func` |
|         - | 2233 | `{` |
|         - | 2234 | `	const char *zName;        /* Function name [i.e: strlen(), rand(), array_merge(), etc.]*/` |
|         - | 2235 | `	ProchHostFunction xFunc;  /* C routine performing the computation */` |
|         - | 2236 | `};` |
|         - | 2237 | `/*` |
|         - | 2238 | ` * Each built-in foreign constant is stored in an instance` |
|         - | 2239 | ` * of the following structure.` |
|         - | 2240 | ` * Please refer to the official documentation for more information` |
|         - | 2241 | ` * on how to create/install foreign constants.` |
|         - | 2242 | ` */` |
|         - | 2243 | `struct ph7_builtin_constant` |
|         - | 2244 | `{` |
|         - | 2245 | `	const char *zName;     /* Constant name */` |
|         - | 2246 | `	ProcConstant xExpand;  /* C routine responsible of expanding constant value*/` |
|         - | 2247 | `};` |
|         - | 2248 | `/* Forward reference */` |
|         - | 2249 | `typedef struct ph7_class_method ph7_class_method;` |
|         - | 2250 | `typedef struct ph7_class_attr   ph7_class_attr;` |
|         - | 2251 | `/*` |
|         - | 2252 | ` * One subscript asked of a native class through ph7_class::xDim -- php's` |
|         - | 2253 | ` * read_dimension / has_dimension handlers, as one call.` |
|         - | 2254 | ` *` |
|         - | 2255 | ` * The hook answers by writing pResult (left NULL for a miss, which the ISSET` |
|         - | 2256 | ` * mode reads as "not set"), or REFUSES by naming an exception class in` |
|         - | 2257 | ` * zThrowClass and wording it in zThrowMsg. The refusal is carried back rather` |
|         - | 2258 | ` * than raised here because only the opcode knows how to route a throw out of a` |
|         - | 2259 | ` * mid-expression read, and because php's own two modes disagree about it: an` |
|         - | 2260 | `` * out-of-range `$map[-1]` is a ValueError to a READ (and to `??`, which reads)`` |
|         - | 2261 | `` * and a plain FALSE to `isset()`.`` |
|         - | 2262 | ` */` |
|         - | 2263 | `typedef struct PH7_NativeDimCtx PH7_NativeDimCtx;` |
|         - | 2264 | `#define PH7_NATIVE_DIM_READ  0 /* php's read_dimension: the value, or NULL for a miss */` |
|         - | 2265 | `#define PH7_NATIVE_DIM_ISSET 1 /* php's has_dimension: presence only, and never a refusal */` |
|         - | 2266 | `/*` |
|         - | 2267 | ` * The WRITE side. Two kinds of class arrive here.` |
|         - | 2268 | ` *` |
|         - | 2269 | ` * One answers reads and stores NOTHING, and may only REFUSE: php's` |
|         - | 2270 | ` * write_dimension and unset_dimension for such a container. The engine's own` |
|         - | 2271 | `` * sentence is `Cannot use object of type C as array`, and a class states its`` |
|         - | 2272 | `` * own here -- PDORow's three are `Cannot write to PDORow offset`, `Cannot`` |
|         - | 2273 | `` * append to PDORow offset` and `Cannot unset PDORow offset`. A refusal is`` |
|         - | 2274 | ` * asked with neither pOffset nor pResult (none of php's wordings names the` |
|         - | 2275 | ` * offset, and there is no answer to write): a hook that does not word one of` |
|         - | 2276 | ` * these must return without touching either.` |
|         - | 2277 | ` *` |
|         - | 2278 | `` * The other really STORES: `$x['a'] = '1'` on a SimpleXMLElement writes an`` |
|         - | 2279 | ` * attribute, and php's handler is a write_dimension like any other. Those` |
|         - | 2280 | ``  * three modes are asked a second way -- with pOffset (0 for the keyless `$o[]` `` |
|         - | 2281 | ` * spelling) and with pResult carrying the INCOMING VALUE -- and the hook says` |
|         - | 2282 | ` * it took the write by setting bStored. A hook that leaves bStored at 0 is the` |
|         - | 2283 | ` * first kind and the caller falls back to the refusal above, which is what` |
|         - | 2284 | ` * keeps DOMNodeList and PDORow answering exactly as they did.` |
|         - | 2285 | ` */` |
|         - | 2286 | `#define PH7_NATIVE_DIM_WRITE  2 /* php's write_dimension with a key */` |
|         - | 2287 | ``#define PH7_NATIVE_DIM_APPEND 3 /* ...and its keyless `$o[] = v` spelling */`` |
|         - | 2288 | `#define PH7_NATIVE_DIM_UNSET  4 /* php's unset_dimension */` |
|         - | 2289 | `/*` |
|         - | 2290 | `` * php's has_dimension asked the way `empty()` asks it -- a non-zero`` |
|         - | 2291 | `` * `check_empty`, which its handlers read as the EMPTINESS question rather than`` |
|         - | 2292 | ` * the null one. SimpleXMLElement is the one that answers it differently:` |
|         - | 2293 | `` * `empty($x['a'])` on `a="0"` is TRUE, judged on the ATTRIBUTE'S TEXT, where`` |
|         - | 2294 | ` * reading the same offset hands back a truthy object. A hook that has no such` |
|         - | 2295 | ` * distinction leaves pResult alone and the caller falls back to reading the` |
|         - | 2296 | ` * value and judging that, which is every other class's answer.` |
|         - | 2297 | ` */` |
|         - | 2298 | `#define PH7_NATIVE_DIM_NOTEMPTY 5` |
|         - | 2299 | `struct PH7_NativeDimCtx` |
|         - | 2300 | `{` |
|         - | 2301 | `	int iMode;               /* PH7_NATIVE_DIM_* */` |
|         - | 2302 | ``	ph7_value *pOffset;      /* The subscript. 0 for the keyless `$o[]` spelling. */`` |
|         - | 2303 | `	ph7_value *pResult;      /* READ: where the answer goes (the caller inits it NULL).` |
|         - | 2304 | `	                          * ISSET: set to a bool by the hook. */` |
|         - | 2305 | `	const char *zThrowClass; /* Set by the hook to refuse; 0 (the caller's init) means answered */` |
|         - | 2306 | `	char zThrowMsg[160];     /* ...and its message, formatted by the hook */` |
|         - | 2307 | `	int bStored;             /* WRITE/APPEND/UNSET only: the hook TOOK the write. 0 -- the` |
|         - | 2308 | `	                          * caller's init -- means it did not, and the access takes the` |
|         - | 2309 | ``	                          * `Cannot use object of type C as array` refusal (or the hook's`` |
|         - | 2310 | `	                          * own wording of it) instead. */` |
|         - | 2311 | `};` |
|         - | 2312 | `/*` |
|         - | 2313 | ` * One property WRITE asked of a native class through ph7_class::xSet -- php's` |
|         - | 2314 | ` * write_property handler.` |
|         - | 2315 | ` *` |
|         - | 2316 | ` * A native class whose properties are php's OWN C struct rather than real slots` |
|         - | 2317 | ` * states this: php converts the incoming value the way its struct field demands` |
|         - | 2318 | ``  * and stores THAT, so `$i->y = 1.5` reads back int(1) and `$i->f = 0.1234567` `` |
|         - | 2319 | ` * reads back 0.123456 (an int64 count of microseconds, shown divided). The hook` |
|         - | 2320 | ` * rewrites pValue IN PLACE to whatever must land in the slot -- it runs on every` |
|         - | 2321 | ` * write shape (a plain store, a compound assign, ++/--, a list() target, a` |
|         - | 2322 | ` * foreach target), because it hangs off the same store filter the typed-property` |
|         - | 2323 | ` * enforcement does.` |
|         - | 2324 | ` *` |
|         - | 2325 | ` * Refusing works the way the dimension hook's does: name an exception class in` |
|         - | 2326 | ` * zThrowClass and word it in zThrowMsg, and the filter raises it where the store` |
|         - | 2327 | ` * would have landed. A property php only lets a script write by CREATING a` |
|         - | 2328 | `` * deprecated dynamic one (DateInterval's `days`) is refused here, the scope policy.`` |
|         - | 2329 | ` */` |
|         - | 2330 | `typedef struct PH7_NativeSetCtx PH7_NativeSetCtx;` |
|         - | 2331 | `struct PH7_NativeSetCtx` |
|         - | 2332 | `{` |
|         - | 2333 | `	const SyString *pName;   /* The property being written */` |
|         - | 2334 | `	ph7_value *pValue;       /* The incoming value; the hook rewrites it in place */` |
|         - | 2335 | `	const char *zThrowClass; /* Set by the hook to refuse; 0 (the caller's init) means stored */` |
|         - | 2336 | `	char zThrowMsg[160];     /* ...and its message, formatted by the hook */` |
|         - | 2337 | `};` |
|         - | 2338 | `/*` |
|         - | 2339 | ` * One PROPERTY access asked of a native class through ph7_class::xProp --` |
|         - | 2340 | ` * php's read_property / has_property / write_property / unset_property` |
|         - | 2341 | ` * handlers, as one call told which is asking.` |
|         - | 2342 | ` *` |
|         - | 2343 | ` * This is the hook for a class whose properties are not storage at all: php's` |
|         - | 2344 | ` * PDORow answers every read from the statement's CURRENT row, so the object` |
|         - | 2345 | `` * holds no slot for any of them, `get_object_vars()` is EMPTY beside a read`` |
|         - | 2346 | ` * that works, and a write is a refusal rather than a store. It is asked only` |
|         - | 2347 | ` * where the instance has NO slot of that name, which for such a class is` |
|         - | 2348 | ` * everywhere -- a native class that keeps real slots and only CONVERTS what` |
|         - | 2349 | ` * lands in them wants ph7_class::xSet instead.` |
|         - | 2350 | ` *` |
|         - | 2351 | ` * READ answers by writing pResult (left NULL for a name the class does not` |
|         - | 2352 | `` * know, which is php's own answer -- not an `Undefined property` warning);`` |
|         - | 2353 | ` * ISSET and EXISTS answer by setting pResult to a bool. Either may DECLINE by leaving` |
|         - | 2354 | ` * bAnswered at 0, which puts the name back on the ordinary path. WRITE and` |
|         - | 2355 | ` * UNSET exist only to refuse, the way the dimension hook's write modes do.` |
|         - | 2356 | ` *` |
|         - | 2357 | ` * A refusal is carried back in zThrowClass/zThrowMsg rather than raised here,` |
|         - | 2358 | ` * exactly as PH7_NativeDimCtx's is: only the opcode knows how to route a throw` |
|         - | 2359 | ` * out of a mid-expression access.` |
|         - | 2360 | ` */` |
|         - | 2361 | `#define PH7_NATIVE_PROP_READ  0` |
|         - | 2362 | `#define PH7_NATIVE_PROP_ISSET 1` |
|         - | 2363 | `#define PH7_NATIVE_PROP_WRITE 2` |
|         - | 2364 | `#define PH7_NATIVE_PROP_UNSET 3` |
|         - | 2365 | `/*` |
|         - | 2366 | `` * php's has_property asked the way `empty()` asks it -- ZEND_PROPERTY_NOT_EMPTY,`` |
|         - | 2367 | `` * a non-zero `check_empty`, which its handlers read as the EMPTINESS question`` |
|         - | 2368 | ` * rather than the null one. It is the same handler and a different answer: a` |
|         - | 2369 | `` * PDORow column holding 0 or "" is `isset()` and is not this.`` |
|         - | 2370 | ` */` |
|         - | 2371 | `#define PH7_NATIVE_PROP_NOTEMPTY 4` |
|         - | 2372 | `/*` |
|         - | 2373 | ` * php's write_property, asked at the point the VALUE exists.` |
|         - | 2374 | ` *` |
|         - | 2375 | ` * The modes above are asked by the member opcode, which runs BEFORE the store` |
|         - | 2376 | ` * that carries the value -- enough for a class that only ever refuses a write` |
|         - | 2377 | ` * (PDORow), and not enough for one whose handler really stores (ext/dom's` |
|         - | 2378 | `` * `$el->nodeValue = 'x'`). STORE is the second half: pResult carries the`` |
|         - | 2379 | ` * incoming value, and the hook writes it or refuses. It is dispatched from the` |
|         - | 2380 | ` * one place every overloaded write funnels through, so a plain store, a` |
|         - | 2381 | `` * compound assign, a `??=` and Reflection's setValue() all reach it.`` |
|         - | 2382 | ` */` |
|         - | 2383 | `#define PH7_NATIVE_PROP_STORE  5` |
|         - | 2384 | `/*` |
|         - | 2385 | ` * php's has_property asked the third way -- ZEND_PROPERTY_EXISTS, which is what` |
|         - | 2386 | `` * `property_exists()` passes and nothing else does. A handler may answer it`` |
|         - | 2387 | ` * differently from the emptiness question, and ArrayObject's does: a storage key` |
|         - | 2388 | `` * holding 0 EXISTS and is not `empty()`-false, where PDORow's handler makes no`` |
|         - | 2389 | ` * distinction and answers both by truth.` |
|         - | 2390 | ` */` |
|         - | 2391 | `#define PH7_NATIVE_PROP_EXISTS 6` |
|         - | 2392 | `/*` |
|         - | 2393 | ` * "Would you take a WRITE of this name?", asked where the value does not exist` |
|         - | 2394 | ` * yet -- the member opcode's write shapes, which have to decide between the` |
|         - | 2395 | `` * handler, a magic `__set` and creating a property before the store runs.`` |
|         - | 2396 | ` *` |
|         - | 2397 | ` * Answering (bAnswered) means the write is the handler's and the rails route it` |
|         - | 2398 | ` * to STORE above; declining leaves the name on the ordinary path. It is the one` |
|         - | 2399 | ` * question a handler must answer without seeing a value, so it is about the NAME` |
|         - | 2400 | ` * and the object's state alone: ext/dom answers it from the class's virtual` |
|         - | 2401 | ` * declarations, ArrayObject from its ARRAY_AS_PROPS flag.` |
|         - | 2402 | ` */` |
|         - | 2403 | `#define PH7_NATIVE_PROP_OWNS   7` |
|         - | 2404 | `typedef struct PH7_NativePropCtx PH7_NativePropCtx;` |
|         - | 2405 | `struct PH7_NativePropCtx` |
|         - | 2406 | `{` |
|         - | 2407 | `	int iMode;               /* PH7_NATIVE_PROP_* */` |
|         - | 2408 | `	const SyString *pName;   /* The property being asked about */` |
|         - | 2409 | `	ph7_value *pResult;      /* READ: where the answer goes (the caller inits it NULL).` |
|         - | 2410 | `	                          * ISSET/NOTEMPTY/EXISTS: set to a bool by the hook.` |
|         - | 2411 | `	                          * STORE: the INCOMING value, which the hook stores. */` |
|         - | 2412 | `	int bAnswered;           /* Set by the hook when it OWNS this name; 0 (the caller's` |
|         - | 2413 | `	                          * init) leaves the access to the ordinary path */` |
|         - | 2414 | `	const char *zThrowClass; /* Set by the hook to refuse; 0 (the caller's init) means answered */` |
|         - | 2415 | `	char zThrowMsg[160];     /* ...and its message, formatted by the hook */` |
|         - | 2416 | `	sxi32 iThrowCode;        /* ...and its php $code. A DOM refusal is a DOMException whose` |
|         - | 2417 | `	                          * code a program reads (DOM_NOT_FOUND_ERR & co), so the number` |
|         - | 2418 | `	                          * has to survive the trip out to the site that raises. 0 -- the` |
|         - | 2419 | `	                          * caller's init -- is every other class's answer. */` |
|         - | 2420 | ``	int bQuiet;              /* READ only: this fetch is a LOOKUP (`$o->p ?? d`), php's third`` |
|         - | 2421 | `	                          * accessor level -- it takes the VALUE and says nothing about a` |
|         - | 2422 | `	                          * name that is not there, where a plain read reports it. */` |
|         - | 2423 | ``	int bWriteCtx;           /* READ only: this fetch is the BASE of a write -- `$o->p[0] = 1`,`` |
|         - | 2424 | ``	                          * `$o->p[] = 1`, a destructuring target -- so php asks`` |
|         - | 2425 | `	                          * get_property_ptr_ptr rather than read_property and a handler` |
|         - | 2426 | `	                          * that CAN hand out a real slot should create the name it is` |
|         - | 2427 | `	                          * missing. Set by the member opcode; ignored by a handler with` |
|         - | 2428 | `	                          * no slot to give. */` |
|         - | 2429 | `	sxu32 nSlot;             /* The memobj index the answer LIVES in, for a handler whose` |
|         - | 2430 | `	                          * property is a real element of something the object owns` |
|         - | 2431 | `	                          * (ArrayObject's storage). SXU32_HIGH -- the caller's init --` |
|         - | 2432 | `	                          * means the answer is a value and the access is not an lvalue,` |
|         - | 2433 | `	                          * which is every virtual property's answer. */` |
|         - | 2434 | `};` |
|         - | 2435 | `/*` |
|         - | 2436 | ` * One COMPARISON asked of a native class through ph7_class::xCmp -- php's` |
|         - | 2437 | ` * compare handler.` |
|         - | 2438 | ` *` |
|         - | 2439 | ` * php asks the LEFT operand's handler and takes whatever it answers, so the` |
|         - | 2440 | ` * handler decides for the pair: what the two objects are compared BY (a` |
|         - | 2441 | ` * DateTime is its instant, and neither its zone nor any property), whether the` |
|         - | 2442 | ` * right operand is even a partner it recognizes, and whether the pair is` |
|         - | 2443 | `` * comparable at all. Asked only for `==`/`<`/`<=>` and friends -- `===` is`` |
|         - | 2444 | ` * identity in php and never reaches a handler.` |
|         - | 2445 | ` *` |
|         - | 2446 | ` * The answer is an ordering in iResult. php's ZEND_UNCOMPARABLE is the value 1,` |
|         - | 2447 | ` * which the comparator already uses for every unordered pair (a NaN, two arrays` |
|         - | 2448 | `` * neither containing the other): the operator arms ask `<` from the other side`` |
|         - | 2449 | ` * rather than reading one side's sign, so 1 from BOTH directions leaves every` |
|         - | 2450 | `` * relational spelling false and `==` false, which is exactly what php answers`` |
|         - | 2451 | ` * for an uncomparable pair.` |
|         - | 2452 | ` *` |
|         - | 2453 | ` * A REFUSAL (php throws DateException out of the DateTimeZone handler) is` |
|         - | 2454 | ` * carried back in zThrowClass/zThrowMsg rather than raised here, the way the` |
|         - | 2455 | `` * dimension hook's is: PH7_MemObjCmp runs under `sort()` and `in_array()` as`` |
|         - | 2456 | ` * well as under an operator, and none of those has a throw boundary of its own.` |
|         - | 2457 | ` * The comparator records it on the VM (PH7_CmpRefusalRaise) and the sites that` |
|         - | 2458 | ` * CAN route a throw -- the comparison opcodes, the switch arm and the host-call` |
|         - | 2459 | ` * boundary -- raise it.` |
|         - | 2460 | ` */` |
|         - | 2461 | `typedef struct PH7_NativeCmpCtx PH7_NativeCmpCtx;` |
|         - | 2462 | `struct PH7_NativeCmpCtx` |
|         - | 2463 | `{` |
|         - | 2464 | `	ph7_class_instance *pOther; /* The RIGHT operand as an INSTANCE, or 0 when it is a scalar */` |
|         - | 2465 | `	ph7_value *pOtherValue;     /* ...and the scalar itself, for the object-versus-value door` |
|         - | 2466 | ``	                             * (php asks the same handler for `$n == 2`). 0 when pOther is set. */`` |
|         - | 2467 | `	int bReversed;              /* The instance is the RIGHT operand: the hook owes the` |
|         - | 2468 | `	                             * answer already flipped, EXCEPT for the uncomparable 1,` |
|         - | 2469 | `	                             * which php answers from both directions alike. */` |
|         - | 2470 | `	int bAnswered;              /* Set by the hook when it RECOGNIZED the partner. The scalar` |
|         - | 2471 | `	                             * door falls back to php's cast rule when it did not; the` |
|         - | 2472 | `	                             * instance door keeps its older "always decided" contract. */` |
|         - | 2473 | `	sxi32 iResult;              /* -1 / 0 / 1; 1 is also php's ZEND_UNCOMPARABLE.` |
|         - | 2474 | `	                             * The caller inits it to 1, so a hook that` |
|         - | 2475 | `	                             * recognizes nothing may simply return. */` |
|         - | 2476 | `	const char *zThrowClass;    /* Set by the hook to refuse; 0 (the caller's init) means answered */` |
|         - | 2477 | `	char zThrowMsg[160];        /* ...and its message, formatted by the hook */` |
|         - | 2478 | `};` |
|         - | 2479 | `/*` |
|         - | 2480 | ` * One ARITHMETIC operator asked of a native class through ph7_class::xArith --` |
|         - | 2481 | `` * php's do_operation handler, which is what makes `$a + $b` mean something for`` |
|         - | 2482 | ` * an object.` |
|         - | 2483 | ` *` |
|         - | 2484 | ` * php asks the LEFT operand's handler first and the RIGHT one's when the left` |
|         - | 2485 | ` * has none, so the handler sees a pair it may be either half of and decides for` |
|         - | 2486 | ` * both: what the other operand is allowed to be, how it converts, and what the` |
|         - | 2487 | ` * answer is. Declining (leaving bHandled at 0) puts the pair back on the` |
|         - | 2488 | `` * ordinary numeric path, where an object is `Unsupported operand types`.`` |
|         - | 2489 | ` *` |
|         - | 2490 | ` * A REFUSAL is carried back rather than raised here, the way the dimension and` |
|         - | 2491 | ` * compare hooks' are: the opcode owns the operand stack and has to settle it` |
|         - | 2492 | ` * before any throw, and the exception CLASS varies -- BcMath\Number answers` |
|         - | 2493 | ` * ValueError for a string that is not a number and DivisionByZeroError for a` |
|         - | 2494 | ` * zero divisor, neither of which is the TypeError the ordinary path raises.` |
|         - | 2495 | ` */` |
|         - | 2496 | `typedef struct PH7_NativeArithCtx PH7_NativeArithCtx;` |
|         - | 2497 | `struct PH7_NativeArithCtx` |
|         - | 2498 | `{` |
|         - | 2499 | `	const char *zOp;         /* "+", "-", "*", "/", "%" or "**" */` |
|         - | 2500 | `	ph7_value *pLeft;        /* The two operands, in SOURCE order */` |
|         - | 2501 | `	ph7_value *pRight;` |
|         - | 2502 | `	ph7_value *pResult;      /* Where the handler writes the answer */` |
|         - | 2503 | `	int bHandled;            /* Set by the hook to claim the pair */` |
|         - | 2504 | `	const char *zThrowClass; /* ...or set this to refuse; 0 means no refusal */` |
|         - | 2505 | `	char zThrowMsg[160];     /* ...and word it here */` |
|         - | 2506 | `};` |
|         - | 2507 | `/*` |
|         - | 2508 | ` * Each class is parsed out and stored in an instance of the following structure.` |
|         - | 2509 | ` * PH7 introduced powerfull extensions to the PHP 5 OO subsystems.` |
|         - | 2510 | ` * Please refer to the official documentation for more information.` |
|         - | 2511 | ` */` |
|         - | 2512 | `struct ph7_class` |
|         - | 2513 | `{` |
|         - | 2514 | `	ph7_class *pBase;     /* Base class if any */` |
|         - | 2515 | `	SyHash hDerived;      /* Derived [child] classes */` |
|         - | 2516 | `	SyString sName;       /* Class full qualified name */` |
|         - | 2517 | `	SyString sDisp;       /* The name a MESSAGE shows, aliasing sName's buffer. The two differ` |
|         - | 2518 | `	                       * for an anonymous class alone: php names one` |
|         - | 2519 | ``	                       * `<parent-or-interface-or-"class">@anonymous` + a NUL byte +`` |
|         - | 2520 | ``	                       * `file:line$hex`, and gets the short form everywhere for free`` |
|         - | 2521 | ``	                       * because every diagnostic prints a class name with `%s`, which`` |
|         - | 2522 | ``	                       * stops at the NUL. PHL prints names with the length-counted `%z`,`` |
|         - | 2523 | ``	                       * so the truncation has to be a field: `sName` is the identity (the`` |
|         - | 2524 | `	                       * hash key, and what get_class()/::class/Reflection::getName() hand` |
|         - | 2525 | ``	                       * back), `sDisp` is what var_dump, print_r, get_debug_type and every`` |
|         - | 2526 | `	                       * diagnostic show. For every other class they are the same bytes and` |
|         - | 2527 | `	                       * the same length. */` |
|         - | 2528 | `	sxi32 iFlags;         /* Class configuration flags [i.e: final, interface, abstract, etc.]  */` |
|         - | 2529 | `	sxu64 nShadowName;    /* One bit per PLAIN name this class holds a MANGLED slot for` |
|         - | 2530 | `	                       * (OoShadowNameBit). PH7_CLASS_SHADOW_PROP says the class has at` |
|         - | 2531 | `	                       * least one; this says WHICH, cheaply enough to ask on every` |
|         - | 2532 | `	                       * property access. Zero when the flag is clear. */` |
|         - | 2533 | `	sxu64 nPrivName;      /* ...and one bit per plain name this class declares as a PRIVATE` |
|         - | 2534 | `	                       * instance property of its own (its trait-composed ones included).` |
|         - | 2535 | `	                       * The other half of the same screen: a scope can only mean a` |
|         - | 2536 | `	                       * mangled slot for a name it declares private itself. */` |
|         - | 2537 | `	SyHash hAttr;         /* Class PROPERTIES [static + instance]. Constants live in hConst */` |
|         - | 2538 | `	SyHash hConst;        /* Class CONSTANTS [incl. enum cases] — php keeps constants and` |
|         - | 2539 | `` 	                       * properties in SEPARATE namespaces, so `const C` and `public $C` `` |
|         - | 2540 | `	                       * coexist. Keyed by name, disjoint from hAttr. */` |
|         - | 2541 | `	SyHash hMethod;       /* Class methods */` |
|         - | 2542 | `	sxu32 nLine;          /* Line number on which this class was declared */` |
|         - | 2543 | `	SySet aInterface;     /* Implemented interface container */` |
|         - | 2544 | `	SySet aTrait;         /* Used trait container */` |
|         - | 2545 | `	ph7_class *pNextName; /* Next class [interface, abstract, etc.] with the same name */` |
|         - | 2546 | `	int bMounted;         /* TRUE if class has been mounted (internal VM state) */` |
|         - | 2547 | `	SyString sFile;       /* Path of the defining file (aliases the VM-lifetime dup in pVm->aFiles).` |
|         - | 2548 | `	                       * nByte == 0 when unknown: Reflection getFileName() reports false. */` |
|         - | 2549 | `	sxu32 nEndLine;       /* Line of the class body's closing brace (Reflection getEndLine) */` |
|         - | 2550 | `	SyString sDoc;        /* Doc-comment preceding the declaration (duplicated; empty = none) */` |
|         - | 2551 | `	SySet aAttrs;         /* Declared #[...] attributes (ph7_attribute records) */` |
|         - | 2552 | `	sxu32 nEnumBacking;   /* Enum backing type: 0 = pure/not an enum, MEMOBJ_INT or MEMOBJ_STRING */` |
|         - | 2553 | `	SySet aEnumCases;     /* Enum cases (ph7_class_attr *) in declaration order. Case singletons` |
|         - | 2554 | `	                       * materialize lazily and INDIVIDUALLY on first access (php 8.1: a broken` |
|         - | 2555 | `	                       * sibling case does not poison a valid one); an unmaterialized case has` |
|         - | 2556 | `	                       * nIdx == SXU32_HIGH. */` |
|         - | 2557 | `	void (*xNew)(ph7_vm *,ph7_class_instance *); /* php's create_object handler, run once the` |
|         - | 2558 | `	                       * instance frame exists and before any constructor. A native class whose` |
|         - | 2559 | `	                       * php counterpart answers its declared properties through a READ handler` |
|         - | 2560 | ``	                       * uses it to SEED those slots: php's ZipArchive declares `public int`` |
|         - | 2561 | ``	                       * $numFiles;` with no default and still shows 0 on a fresh object, because`` |
|         - | 2562 | `	                       * the handler answers rather than the slot. Seeding is the same fact from` |
|         - | 2563 | `	                       * the other side and keeps Reflection honest -- hasDefaultValue() stays` |
|         - | 2564 | `	                       * false, because there is no default, only a starting value. Resolved` |
|         - | 2565 | `	                       * through the ANCESTORS exactly as xRelease is. */` |
|         - | 2566 | `	void (*xRelease)(ph7_vm *,ph7_class_instance *); /* Native teardown for an instance of this class,` |
|         - | 2567 | `	                       * run by PH7_ClassInstanceRelease while the instance's slots are still` |
|         - | 2568 | `	                       * readable. This is NOT __destruct: php's WeakReference declares no` |
|         - | 2569 | `	                       * destructor, so a native class that must release a C-side resource` |
|         - | 2570 | `	                       * states it here instead of growing a method Reflection would report. */` |
|         - | 2571 | `	const PH7_NativeIterVtab *pIterVtab; /* How an InternalIterator walks an instance of this class` |
|         - | 2572 | `	                       * (php's get_iterator handler). Set on native IteratorAggregates whose` |
|         - | 2573 | `	                       * getIterator() answers PH7_NativeIteratorNew(); 0 everywhere else. */` |
|         - | 2574 | `	sxi32 (*xPresent)(ph7_vm *,ph7_class_instance *,ph7_value *,int); /* php's get_properties /` |
|         - | 2575 | `	                       * get_debug_info handlers, as one callback told which is asking:` |
|         - | 2576 | `	                       * the SHAPE a class SHOWS, which for several native classes is nothing` |
|         - | 2577 | `	                       * like the engine state it keeps. php presents a DateTime as` |
|         - | 2578 | `	                       * date/timezone_type/timezone and a WeakReference as ["object"], while` |
|         - | 2579 | `	                       * the slots underneath are a timestamp and a C-side cell — those slots` |
|         - | 2580 | `	                       * carry PH7_MOD_HIDDEN, and this fills an ARRAY with what php shows.` |
|         - | 2581 | `	                       * The last argument is 1 for the DEBUG surfaces (var_dump/print_r) and 0` |
|         - | 2582 | `	                       * for the property ones (var_export, the (array) cast), because php's` |
|         - | 2583 | `	                       * two handlers do not agree: a WeakReference shows ["object"] to` |
|         - | 2584 | `	                       * var_dump and NOTHING to (array), while a DateTime shows the same three` |
|         - | 2585 | `	                       * keys to both. Never consulted by get_object_vars()/foreach, which php` |
|         - | 2586 | `	                       * answers from the real (scoped) properties, nor yet by serialize(),` |
|         - | 2587 | `	                       * where php's answer is an __serialize/__unserialize pair. */` |
|         - | 2588 | `	const char *zNewRefusalClass; /* ...and the exception CLASS that refusal is, when it is` |
|         - | 2589 | ``	                       * not the usual `Error`: php's PDORow refuses `new` with a`` |
|         - | 2590 | `	                       * PDOException. 0 selects Error. */` |
|         - | 2591 | `	const char *zNewRefusal; /* php's create_object refusal TEXT for a PH7_CLASS_NOINSTANTIATE` |
|         - | 2592 | `	                       * class, when it is not the usual "Instantiation of class %s is not` |
|         - | 2593 | `	                       * allowed". php words Directory's as "Cannot directly construct` |
|         - | 2594 | `	                       * Directory, use dir() instead"; 0 selects the standard sentence. */` |
|         - | 2595 | `	void (*xClone)(ph7_vm *,ph7_class_instance *,ph7_class_instance *); /* php's clone_obj handler` |
|         - | 2596 | ``	                       * analogue: what `clone $o` DOES for an instance beyond the slot-by-slot`` |
|         - | 2597 | `	                       * copy, run on (clone, source) after the copy and before any __clone().` |
|         - | 2598 | `	                       * A DOM node's copy must be a copy of the NODE, not a second object over` |
|         - | 2599 | `	                       * the same one -- without this, a mutation through either object writes` |
|         - | 2600 | `	                       * the other. This is NOT __clone: php declares no such method on these` |
|         - | 2601 | `	                       * classes, so Reflection must not report one. Inherited by user` |
|         - | 2602 | `	                       * subclasses (the nearest ancestor's hook runs), which is php's handler` |
|         - | 2603 | `	                       * inheritance. 0 everywhere else. PH7_NativeClassSpec has no field for` |
|         - | 2604 | `	                       * it (a 14th field would touch every row of every spec table under` |
|         - | 2605 | `	                       * -Werror=missing-field-initializers); the owning installer assigns it` |
|         - | 2606 | `	                       * on the mounted class right after PH7_InstallNativeClasses. */` |
|         - | 2607 | `	void (*xDim)(ph7_vm *,ph7_class_instance *,PH7_NativeDimCtx *); /* php's read_dimension /` |
|         - | 2608 | `	                       * has_dimension handlers, as one callback told which is asking.` |
|         - | 2609 | ``	                       * A class states this when `$o[$k]` MEANS something and the class`` |
|         - | 2610 | `	                       * does not implement ArrayAccess -- php 8.3 gave DOMNodeList and` |
|         - | 2611 | `	                       * DOMNamedNodeMap dimension handlers WITHOUT declaring the` |
|         - | 2612 | ``	                       * interface, so `$list[0]` reads there while`` |
|         - | 2613 | ``	                       * `$list instanceof ArrayAccess` is false. No spec field can say`` |
|         - | 2614 | `	                       * that: the interface list is what a class DECLARES, and this is a` |
|         - | 2615 | `	                       * handler underneath it. Assigned on the mounted class by the` |
|         - | 2616 | `	                       * owning installer, like xClone, and inherited by user subclasses` |
|         - | 2617 | `	                       * (the nearest ancestor's hook runs) -- php's handler inheritance,` |
|         - | 2618 | `	                       * which is why a subclass's own offsetGet is NOT consulted for a` |
|         - | 2619 | `	                       * read even when it declares ArrayAccess. The WRITE half stays` |
|         - | 2620 | `	                       * php's: a store, an append and an unset are all` |
|         - | 2621 | ``	                       * `Cannot use object of type C as array` unless the class really`` |
|         - | 2622 | `	                       * implements ArrayAccess. 0 everywhere else. */` |
|         - | 2623 | `	void (*xSet)(ph7_vm *,ph7_class_instance *,PH7_NativeSetCtx *); /* php's write_property` |
|         - | 2624 | `	                       * handler: what a WRITE to one of this class's declared properties` |
|         - | 2625 | `	                       * converts to (or refuses), for a class whose properties are php's` |
|         - | 2626 | `	                       * own C struct. Reached from the store filter through the slot` |
|         - | 2627 | `	                       * table, so every write shape goes through it. Assigned on the` |
|         - | 2628 | `	                       * mounted class by the owning installer, like xClone and xDim,` |
|         - | 2629 | `	                       * which also flags the class's properties PH7_CLASS_ATTR_NATIVE_SET` |
|         - | 2630 | `	                       * so their slots get registered; inherited by user subclasses the` |
|         - | 2631 | `	                       * way php inherits a handler. 0 everywhere else. */` |
|         - | 2632 | `	void (*xProp)(ph7_vm *,ph7_class_instance *,PH7_NativePropCtx *); /* php's` |
|         - | 2633 | `	                       * read_property / has_property / write_property /` |
|         - | 2634 | `	                       * unset_property handlers, as one callback told which is` |
|         - | 2635 | `	                       * asking; see PH7_NativePropCtx. Consulted only where the` |
|         - | 2636 | `	                       * instance has no slot of that name. Assigned on the mounted` |
|         - | 2637 | `	                       * class by the owning installer, like xClone/xDim/xSet, and` |
|         - | 2638 | `	                       * inherited by user subclasses -- php's handler inheritance.` |
|         - | 2639 | `	                       * 0 everywhere else. */` |
|         - | 2640 | `	int (*xBool)(ph7_vm *,ph7_class_instance *); /* php's cast_object for _IS_BOOL: an` |
|         - | 2641 | `	                       * object is ALWAYS truthy unless its class says otherwise, and` |
|         - | 2642 | `	                       * BcMath\Number is the one that does -- a zero Number is falsy. */` |
|         - | 2643 | `	void (*xArith)(ph7_vm *,ph7_class_instance *,PH7_NativeArithCtx *); /* php's do_operation` |
|         - | 2644 | `	                       * handler; see PH7_NativeArithCtx. 0 for every class that has none,` |
|         - | 2645 | `	                       * which is all of them but BcMath\Number. */` |
|         - | 2646 | `	void (*xCmp)(ph7_vm *,ph7_class_instance *,PH7_NativeCmpCtx *); /* php's compare handler:` |
|         - | 2647 | ``	                       * what `==`, `<` and `<=>` MEAN for an instance of this class,`` |
|         - | 2648 | `	                       * asked instead of the property-by-property walk. php gives one` |
|         - | 2649 | `	                       * to the three date classes whose state is not their properties --` |
|         - | 2650 | `	                       * a DateTime compares as an INSTANT across DateTime and` |
|         - | 2651 | `	                       * DateTimeImmutable alike, two DateIntervals are never comparable,` |
|         - | 2652 | `	                       * two DateTimeZones of different kinds are a refusal. Asked of the` |
|         - | 2653 | `	                       * LEFT operand only, before the same-class screen and after the` |
|         - | 2654 | ``	                       * identity shortcut, and never for `===`. Assigned on the mounted`` |
|         - | 2655 | `	                       * class by the owning installer, like xClone/xDim/xSet, and` |
|         - | 2656 | `	                       * inherited by user subclasses (php's handler inheritance: a` |
|         - | 2657 | `	                       * subclass of DateTime still compares as an instant, extra` |
|         - | 2658 | `	                       * properties and all). 0 everywhere else. */` |
|         - | 2659 | `};` |
|         - | 2660 | `/* Class configuration flags */` |
|         - | 2661 | `#define PH7_CLASS_FINAL       0x001 /* Class is final [cannot be extended] */` |
|         - | 2662 | `#define PH7_CLASS_INTERFACE   0x002 /* Class is interface */` |
|         - | 2663 | `#define PH7_CLASS_ABSTRACT    0x004 /* Class is abstract */` |
|         - | 2664 | `#define PH7_CLASS_TRAIT       0x008 /* Class is a trait */` |
|         - | 2665 | `#define PH7_CLASS_TRAIT_VISITING 0x010 /* Trait is currently being applied (cycle detection) */` |
|         - | 2666 | `#define PH7_CLASS_READONLY    0x020 /* Class is readonly (PHP 8.2): every declared property is readonly */` |
|         - | 2667 | `#define PH7_CLASS_INTERNAL    0x040 /* Class was defined while compiling a builtin chunk (embedded PHP` |
|         - | 2668 | `                                     * library). Reflection reports it as internal: isInternal() true,` |
|         - | 2669 | `                                     * getFileName() false. */` |
|         - | 2670 | `#define PH7_CLASS_ENUM        0x080 /* Class is an enum (PHP 8.1). Also carries PH7_CLASS_FINAL. */` |
|         - | 2671 | `#define PH7_CLASS_STATIC_DEFER 0x200 /* This class's static table is not fully materialized: at least` |
|         - | 2672 | `                                      * one static property's default THREW when it was evaluated at` |
|         - | 2673 | `                                      * mount (PH7_CLASS_ATTR_STATIC_DEFER on the attribute) or failed` |
|         - | 2674 | `                                      * its type check (VM_CLASS_ATTR_TYPE_DEFER on the slot), or a` |
|         - | 2675 | `                                      * typed INSTANCE default has yet to be held to its type (php's` |
|         - | 2676 | `                                      * class resolution, which every reader shares). A hint` |
|         - | 2677 | `                                      * only: the access/instantiation sites call` |
|         - | 2678 | `                                      * PH7_VmMaterializeClassStatics, which re-scans the whole base` |
|         - | 2679 | `                                      * chain. Set on the class whose mount saw the failure; the gate` |
|         - | 2680 | `                                      * (VmClassStaticDeferPending) walks the bases, so mount ORDER` |
|         - | 2681 | `                                      * between a base and its subclass does not matter. */` |
|         - | 2682 | `#define PH7_CLASS_LINT_UNBOUND 0x400000 /* Syntax-check compile (phl -l) only: a parent, interface or` |
|         - | 2683 | `                                        * trait this declaration names could not be resolved, and the` |
|         - | 2684 | `                                        * mode carried on with the body rather than refusing. Every` |
|         - | 2685 | `                                        * check that needs the missing member's contents -- #[\Override],` |
|         - | 2686 | `                                        * the unimplemented-abstract count -- is then skipped, which is` |
|         - | 2687 | `                                        * what php does: it reports those only for a class it could` |
|         - | 2688 | `                                        * EARLY-BIND, and it binds nothing whose base it cannot see. */` |
|         - | 2689 | `#define PH7_CLASS_LATEBIND    0x1000000 /* Not early-bound: php declares this top-level class where its` |
|         - | 2690 | `                                     * statement RUNS (PH7_OP_CLASS_DECLARE). Set by the compile that` |
|         - | 2691 | `                                     * links it, so a subclass in the same file is not early-bound either. */` |
|         - | 2692 | `#define PH7_CLASS_HIDDEN      0x800000 /* ...and its unit has finished compiling while that statement has not` |
|         - | 2693 | `                                     * run: it keeps its hClass slot, but no lookup by name finds it` |
|         - | 2694 | `                                     * (PH7_VmClassEntry). Both flags clear when it is declared. */` |
|         - | 2695 | `#define PH7_CLASS_TOPLEVEL    0x200000 /* Declared UNCONDITIONALLY at file top level. php runs such a` |
|         - | 2696 | `                                     * declaration whatever else is in the file, so two of them under one` |
|         - | 2697 | `                                     * name is a redeclaration even when neither was early-bound -- which` |
|         - | 2698 | `                                     * is the difference between this flag and PH7_CLASS_BOUND below. */` |
|         - | 2699 | `#define PH7_CLASS_BOUND       0x100 /* Bound by an UNCONDITIONAL top-level declaration. PHP fatals on a` |
|         - | 2700 | `                                     * second such binding of the same name ("Cannot redeclare ..."); a` |
|         - | 2701 | `                                     * conditional (if/loop/func-nested) declaration is NOT marked, so the` |
|         - | 2702 | ``                                     * `if(false){class C{}}` / `if(!class_exists){..}` guard idioms hoist. */`` |
|         - | 2703 | ``#define PH7_CLASS_NOCLONE     0x400 /* `clone $o` is a catchable Error for this class. A native class whose`` |
|         - | 2704 | `                                     * instances own a C-side resource keyed by a private slot cannot be` |
|         - | 2705 | `                                     * copied slot-by-slot (WeakReference's shared cell would be dropped` |
|         - | 2706 | `                                     * twice), which is exactly why php makes those classes uncloneable.` |
|         - | 2707 | `                                     * Enum cases carry the same rule through PH7_CLASS_ENUM, and` |
|         - | 2708 | `                                     * Generator/Fiber are named directly at the OP_CLONE test. */` |
|         - | 2709 | `#define PH7_CLASS_NOSERIALIZE 0x800 /* serialize() of an instance is a catchable Exception naming the` |
|         - | 2710 | `                                     * class, php's answer for every class holding engine state.` |
|         - | 2711 | `                                     * Without it the default object path emits the private slots —` |
|         - | 2712 | `                                     * for these classes a raw POINTER, which unserialize() would` |
|         - | 2713 | `                                     * hand straight back to a method. php's ZEND_ACC_NOT_SERIALIZABLE:` |
|         - | 2714 | `                                     * tested FIRST and unconditionally, so a subclass declaring` |
|         - | 2715 | `                                     * __serialize() is refused too (DOMXPath is the case that shows` |
|         - | 2716 | `                                     * it). INHERITED — the serializer walks pBase, because php's flag` |
|         - | 2717 | `                                     * rides down to every user subclass. */` |
|         - | 2718 | ``#define PH7_CLASS_NOSERIALIZE_SUBOK 0x2000 /* The SOFT refusal: php's `ce->serialize` deny HANDLER,`` |
|         - | 2719 | `                                     * which the serializer consults only AFTER looking for` |
|         - | 2720 | `                                     * __serialize()/__sleep() — so a SUBCLASS that declares either` |
|         - | 2721 | `                                     * one serializes normally, and php says so in the sentence` |
|         - | 2722 | `                                     * ("…is not allowed, unless serialization methods are` |
|         - | 2723 | `                                     * implemented in a subclass"). The DOM node classes are the` |
|         - | 2724 | `                                     * users; __wakeup() alone does NOT rescue them. Inherited the` |
|         - | 2725 | `                                     * same way as the hard flag. */` |
|         - | 2726 | ``#define PH7_CLASS_DIM_WRITABLE 0x4000 /* A write through `$obj[k]` LANDS on this class's storage.`` |
|         - | 2727 | `                                       * php's split is the read_dimension handler: an internal` |
|         - | 2728 | `                                       * class whose own handler hands back the real element` |
|         - | 2729 | `                                       * (ArrayObject, ArrayIterator, WeakMap) supports indirect` |
|         - | 2730 | `                                       * modification, while everything routed through` |
|         - | 2731 | `                                       * zend_std_read_dimension — every userland ArrayAccess, and` |
|         - | 2732 | `                                       * the SPL classes that keep the standard handler — gets a` |
|         - | 2733 | `                                       * TEMPORARY, so php notices and drops the write. Inherited` |
|         - | 2734 | `                                       * by subclasses (the flag is looked up along pBase), but` |
|         - | 2735 | `                                       * only while the native offsetGet is still the one that` |
|         - | 2736 | `                                       * answers: an override takes the class off the fast handler` |
|         - | 2737 | `                                       * in php too. See PH7_VmDimFetchWritable. */` |
|         - | 2738 | `/*` |
|         - | 2739 | `` * ph7_class::iFlags bit: `(int)` on an instance of this class answers the OBJECT`` |
|         - | 2740 | `` * HANDLE, silently, instead of php's `Object of class X could not be converted to`` |
|         - | 2741 | `` * int` warning and its 1. php gives exactly two classes that cast_object -- the`` |
|         - | 2742 | ` * curl easy and multi handles -- and the reason is stated in its own source: both` |
|         - | 2743 | `` * used to be RESOURCES, whose `(int)` was the resource id, and a program that keyed`` |
|         - | 2744 | ` * a table by it had to keep working. Composer's CurlDownloader is that program.` |
|         - | 2745 | `` * `(float)`, `(string)` and every other cast stay php's refusal, and so does the`` |
|         - | 2746 | ` * COMPARISON, which is a different handler (see PH7_NativeCmpOpaqueHandle).` |
|         - | 2747 | ` */` |
|         - | 2748 | `#define PH7_CLASS_HANDLE_ID   0x40000` |
|         - | 2749 | `/*` |
|         - | 2750 | `` * ph7_class::iFlags bit: `get_object_vars()` on an instance of this class is`` |
|         - | 2751 | ` * answered by the class's ph7_class::xPresent table rather than by its real` |
|         - | 2752 | ` * slots.` |
|         - | 2753 | ` *` |
|         - | 2754 | ` * php's get_properties handler is asked for three PURPOSES, and its native` |
|         - | 2755 | ` * classes mostly disagree between them -- a DateTime shows its three keys to` |
|         - | 2756 | `` * var_dump and to `(array)` and NOTHING to get_object_vars, a DOM node shows a`` |
|         - | 2757 | ` * table to var_dump and nothing to either of the others. SimpleXMLElement is` |
|         - | 2758 | ` * the one that answers all three the same way, so the third purpose is a` |
|         - | 2759 | ` * per-class opt-in instead of a fourth argument every handler would have to` |
|         - | 2760 | ` * learn.` |
|         - | 2761 | ` */` |
|         - | 2762 | `#define PH7_CLASS_VARS_PRESENT 0x80000` |
|         - | 2763 | `/*` |
|         - | 2764 | `` * ph7_class::iFlags bit: `(int)`, `(float)` and every numeric COERCION of an`` |
|         - | 2765 | ` * instance of this class run through the class's string form, silently, instead` |
|         - | 2766 | `` * of php's `Object of class X could not be converted to int` warning and its 1.`` |
|         - | 2767 | ` *` |
|         - | 2768 | `` * php's SimpleXMLElement is the one that does it: `(int)$xml->count` is the`` |
|         - | 2769 | `` * number the element CONTAINS, and `$xml->n + 1` adds to it, because its`` |
|         - | 2770 | ``  * cast_object answers IS_LONG and IS_DOUBLE from the node's text. Its `(bool)` `` |
|         - | 2771 | `` * is a different question again (ph7_class::xBool), and `(string)` is the`` |
|         - | 2772 | ` * ordinary __toString().` |
|         - | 2773 | ` */` |
|         - | 2774 | `#define PH7_CLASS_NUM_AS_STRING 0x100000` |
|         - | 2775 | ``#define PH7_CLASS_ANON        0x20000 /* Declared by `new class {...}`. php has no NAME to put in a`` |
|         - | 2776 | `                                    * type text for it while its body compiles, which is why` |
|         - | 2777 | ``                                    * `self` inside one may not be part of an intersection`` |
|         - | 2778 | `                                    * (see the scope-keyword screen in the type parser). */` |
|         - | 2779 | `#define PH7_CLASS_SHADOW_PROP 0x10000 /* At least one property of this class is filed under php's` |
|         - | 2780 | `                                       * MANGLED storage name -- a base's PRIVATE instance property,` |
|         - | 2781 | `                                       * carried down so the subclass's objects still hold its slot` |
|         - | 2782 | `                                       * (PH7_ClassAttrStorageName). The only way two slots of one` |
|         - | 2783 | `                                       * object can share a plain NAME, which is what the by-name` |
|         - | 2784 | `                                       * presentation surfaces have to de-duplicate. */` |
|         - | 2785 | `#define PH7_CLASS_LAZY_ATTR    0x8000 /* This class declares at least one PH7_CLASS_ATTR_NATIVE_LAZY` |
|         - | 2786 | `                                       * property. The O(1) gate in front of the materialization walk:` |
|         - | 2787 | `                                       * every native class writes its slots through the same setters,` |
|         - | 2788 | `                                       * and only these two classes have anything to install. */` |
|         - | 2789 | ``#define PH7_CLASS_NOINSTANTIATE 0x1000 /* `new C` is refused by the OBJECT-CREATION step, before the`` |
|         - | 2790 | `                                     * constructor's visibility is ever consulted — php's` |
|         - | 2791 | `                                     * "Instantiation of class %s is not allowed", which its` |
|         - | 2792 | `                                     * create_object handler raises. The distinction is visible:` |
|         - | 2793 | `                                     * Closure's __construct is PRIVATE (Reflection prints it that` |
|         - | 2794 | ``                                     * way), so without this flag `new Closure` reports a visibility`` |
|         - | 2795 | `                                     * refusal ("Call to private Closure::__construct() from global` |
|         - | 2796 | `                                     * scope") where php reports the instantiation one. A class that` |
|         - | 2797 | `                                     * merely wants a private ctor does NOT want this bit. */` |
|         - | 2798 | `/* Class attribute/methods/constants protection levels */` |
|         - | 2799 | `#define PH7_CLASS_PROT_PUBLIC     1 /* public */` |
|         - | 2800 | `#define PH7_CLASS_PROT_PROTECTED  2 /* protected */` |
|         - | 2801 | `#define PH7_CLASS_PROT_PRIVATE    3 /* private */` |
|         - | 2802 | `/*` |
|         - | 2803 | ` * each class attribute (variable, constants) is parsed out and stored` |
|         - | 2804 | ` * in an instance of the following structure.` |
|         - | 2805 | ` */` |
|         - | 2806 | `struct ph7_class_attr` |
|         - | 2807 | `{` |
|         - | 2808 | `	SyString sName;      /* Atrribute name */` |
|         - | 2809 | `	SyString sStoreName; /* php's MANGLED storage name for a PRIVATE instance property --` |
|         - | 2810 | `	                      * "\0DeclaringClass\0name" -- materialized the first time this` |
|         - | 2811 | `	                      * attribute is filed in a class that did not declare it. Empty` |
|         - | 2812 | `	                      * (nByte == 0) until then, and for every other attribute, whose` |
|         - | 2813 | `	                      * storage name is sName. See PH7_ClassAttrStorageName. */` |
|         - | 2814 | `	sxi32 iFlags;        /* Attribute configuration [i.e: static, variable, constant, etc.] */` |
|         - | 2815 | `	sxi32 iProtection;   /* Protection level [i.e: public, private, protected] */` |
|         - | 2816 | `	SySet aByteCode;     /* Compiled attribute body */` |
|         - | 2817 | `	sxu32 nIdx;          /* Attribute index */` |
|         - | 2818 | `	sxu32 nLine;         /* Line number on which this attribute was defined */` |
|         - | 2819 | `	sxu32 nType;         /* Declared type: MEMOBJ_* bitmask, SXU32_HIGH for class, 0 = untyped */` |
|         - | 2820 | `	SyString sClass;     /* Class/interface name when nType == SXU32_HIGH */` |
|         - | 2821 | `	SyString sTypeName;  /* Original type text for error messages (e.g. "?int", "Foo", "string\|int") */` |
|         - | 2822 | `	SySet aUnionAlts;    /* Union alternatives (ph7_type_alt). Empty unless PH7_CLASS_ATTR_UNION is set. */` |
|         - | 2823 | `	ph7_class *pDeclClass; /* Class that originally declared this attribute */` |
|         - | 2824 | `	SyString sDoc;       /* Doc-comment preceding the declaration (duplicated; empty = none) */` |
|         - | 2825 | `	SySet aAttrs;        /* Declared #[...] attributes (ph7_attribute records) */` |
|         - | 2826 | `	const void *pNativeValue; /* A native class's literal initializer (PH7_NativeConstDef*), or 0.` |
|         - | 2827 | `	                      * A compiled declaration expresses its default as aByteCode evaluated at` |
|         - | 2828 | `	                      * mount; the C builder has no compiler to emit that, so it hands the` |
|         - | 2829 | `	                      * literal here and the mount writes it straight into the reserved slot.` |
|         - | 2830 | `	                      * Mutually exclusive with a non-empty aByteCode. */` |
|         - | 2831 | `};` |
|         - | 2832 | `/* Attribute configuration */` |
|         - | 2833 | `#define PH7_CLASS_ATTR_STATIC       0x001  /* Static attribute */` |
|         - | 2834 | `#define PH7_CLASS_ATTR_CONSTANT     0x002  /* Constant attribute */` |
|         - | 2835 | `#define PH7_CLASS_ATTR_ABSTRACT     0x004  /* Abstract method */` |
|         - | 2836 | `#define PH7_CLASS_ATTR_FINAL        0x008  /* Final method */` |
|         - | 2837 | `#define PH7_CLASS_ATTR_TYPED        0x010  /* Property has an explicit declared type */` |
|         - | 2838 | `#define PH7_CLASS_ATTR_NULLABLE     0x020  /* Type allows null (?type prefix or T\|null union) */` |
|         - | 2839 | `#define PH7_CLASS_ATTR_UNION        0x040  /* Property has a union type (use aUnionAlts) */` |
|         - | 2840 | `#define PH7_CLASS_ATTR_READONLY     0x080  /* readonly property (PHP 8.1) */` |
|         - | 2841 | `#define PH7_CLASS_ATTR_DYNAMIC      0x100  /* Runtime-added (dynamic) property: the ph7_class_attr is` |
|         - | 2842 | `                                            * instance-owned (synthesized, not class-declared) and must` |
|         - | 2843 | `                                            * be freed when the instance is released. */` |
|         - | 2844 | `#define PH7_CLASS_ATTR_ENUMCASE     0x200  /* Enum case: a class constant whose value is the lazily` |
|         - | 2845 | `                                            * materialized case singleton (aByteCode holds the BACKING` |
|         - | 2846 | `                                            * value expression for backed enums; empty when pure). */` |
|         - | 2847 | `#define PH7_CLASS_ATTR_EVALING      0x400  /* Transient: this constant's initializer is being evaluated` |
|         - | 2848 | `                                            * (on-demand, VmClassConstEvalOnDemand). Re-entry means a` |
|         - | 2849 | `                                            * self-referencing constant — php's catchable Error. */` |
|         - | 2850 | `#define PH7_CLASS_ATTR_PRIVATE_SET  0x800  /* private(set) asymmetric visibility (PHP 8.4): writes` |
|         - | 2851 | `                                            * only from the DECLARING class scope (subclasses excluded) */` |
|         - | 2852 | `#define PH7_CLASS_ATTR_PROTECTED_SET 0x1000 /* protected(set) asymmetric visibility (PHP 8.4): writes` |
|         - | 2853 | `                                            * from the declaring class or a subclass scope */` |
|         - | 2854 | `#define PH7_CLASS_ATTR_PUBLIC_SET   0x2000 /* explicit public(set): behaviorally the default, kept` |
|         - | 2855 | `                                            * for the weaker-than-set check and reflection output */` |
|         - | 2856 | ``#define PH7_CLASS_ATTR_HOOK_GET     0x4000 /* property has a `get` hook (PHP 8.4): reads dispatch`` |
|         - | 2857 | `                                            * __phl_hook_get_NAME (guard-bypassed inside hooks) */` |
|         - | 2858 | ``#define PH7_CLASS_ATTR_HOOK_SET     0x8000 /* property has a `set` hook (PHP 8.4): plain writes`` |
|         - | 2859 | `                                            * dispatch __phl_hook_set_NAME */` |
|         - | 2860 | `#define PH7_CLASS_ATTR_HOOK_VIRTUAL 0x10000 /* PHP 8.4 VIRTUAL hooked property: none of its own` |
|         - | 2861 | ``                                            * hook bodies references `$this->NAME`, so php gives it`` |
|         - | 2862 | `                                            * no backing store — excluded from the raw object` |
|         - | 2863 | `                                            * surfaces (var_dump/(array)/print_r/serialize/` |
|         - | 2864 | `                                            * get_class_vars and the get-dispatching walks when it` |
|         - | 2865 | `                                            * has no get hook), no default allowed, reads without a` |
|         - | 2866 | `                                            * get hook are php's "is write-only" Error. PHL still` |
|         - | 2867 | `                                            * allocates the (null) backing slot; this flag hides it. */` |
|         - | 2868 | `#define PH7_CLASS_ATTR_NATIVE_SET   0x100000 /* A NATIVE class's property whose WRITES run through` |
|         - | 2869 | `                                            * ph7_class::xSet (php's write_property). Set by the` |
|         - | 2870 | `                                            * installer that assigns the hook, and read by the two` |
|         - | 2871 | `                                            * places that care: instantiation, which registers the` |
|         - | 2872 | `                                            * slot so the store filter can find it, and the filter` |
|         - | 2873 | `                                            * itself. A slot carrying it is registered in` |
|         - | 2874 | `                                            * pVm->hTypedSlot exactly as a typed one is -- that table` |
|         - | 2875 | `                                            * is "slots a store must be filtered through", and the` |
|         - | 2876 | `                                            * two reasons compose (a native property may also be` |
|         - | 2877 | `                                            * typed). */` |
|         - | 2878 | `#define PH7_CLASS_ATTR_REFSRCPIN    0x8000000 /* STATIC property that is the SOURCE of a reference:` |
|         - | 2879 | `                                               * it holds one counted pin on its own slot, the` |
|         - | 2880 | `                                               * instance-side VM_CLASS_ATTR_REFSRCPIN's twin. A` |
|         - | 2881 | `                                               * class static lives as long as the VM, so the pin` |
|         - | 2882 | `                                               * is never given back -- which is the point: it` |
|         - | 2883 | `                                               * stops the other end's unpin from freeing it. */` |
|         - | 2884 | `` #define PH7_CLASS_ATTR_REFBOUND     0x80000 /* STATIC property currently bound to another slot by `=&` `` |
|         - | 2885 | ``                                             * (`C::$s =& $x`). The instance side records this per`` |
|         - | 2886 | `                                             * INSTANCE (VM_CLASS_ATTR_REFBOUND); a static has one slot` |
|         - | 2887 | `                                             * per declaration, so the bit lives here. It says the slot` |
|         - | 2888 | `                                             * this attribute points at is held by a COUNTED pin, and a` |
|         - | 2889 | `                                             * rebind must give that pin back rather than free a slot the` |
|         - | 2890 | `                                             * attribute never owned. */` |
|         - | 2891 | `#define PH7_CLASS_ATTR_HIDDEN       0x40000 /* A NATIVE class's engine slot: real storage that php keeps` |
|         - | 2892 | `                                            * in its own C struct and therefore never shows. Excluded` |
|         - | 2893 | `                                            * from every PRESENTATION surface — var_dump/print_r/` |
|         - | 2894 | `                                            * var_export, (array), get_object_vars, foreach, json_encode,` |
|         - | 2895 | `                                            * serialize, http_build_query and Reflection's property` |
|         - | 2896 | ``                                            * listing — while `new`, clone and the native bodies' own`` |
|         - | 2897 | `                                            * PH7_NativeAttr() reads still see it. Set from` |
|         - | 2898 | `                                            * PH7_MOD_HIDDEN on a PH7_NativePropDef. Use it for a slot` |
|         - | 2899 | `                                            * php shows NOTHING for (a handle, a cursor cache); a slot` |
|         - | 2900 | ``                                            * php shows under a DIFFERENT name (ArrayObject's `storage`,`` |
|         - | 2901 | ``                                            * DateTime's `date`) wants the recorded presentation hook`` |
|         - | 2902 | `                                            * still asks for, not this bit. */` |
|         - | 2903 | `#define PH7_CLASS_ATTR_STATIC_DEFER 0x20000 /* STATIC property whose default initializer THREW when it` |
|         - | 2904 | `                                            * was evaluated at class mount. php never evaluates a static` |
|         - | 2905 | `                                            * default at declaration time — it materializes the class's` |
|         - | 2906 | `                                            * static table at the FIRST static-property access — so the` |
|         - | 2907 | `                                            * mount-time throw is raised MUTED (VmEvalDefaultMuted: no` |
|         - | 2908 | `                                            * catch runs, nothing is reported) and rolled back whole,` |
|         - | 2909 | `                                            * and the initializer re-runs at that first access` |
|         - | 2910 | `                                            * (PH7_VmMaterializeClassStatics), where php raises it.` |
|         - | 2911 | `                                            * Cleared once the initializer completes without throwing:` |
|         - | 2912 | `                                            * a class whose bad default is never READ stays silent in` |
|         - | 2913 | `                                            * both engines, and a re-run that now succeeds (the constant` |
|         - | 2914 | `                                            * it names was define()d after the declaration) answers the` |
|         - | 2915 | `                                            * value, as php's does. */` |
|         - | 2916 | `#define PH7_CLASS_ATTR_NATIVE_VIRTUAL 0x200000 /* A NATIVE class's property that php FABRICATES on` |
|         - | 2917 | `                                            * demand (its get_properties handler) instead of keeping` |
|         - | 2918 | `                                            * in the object's real property table. DatePeriod's seven` |
|         - | 2919 | `                                            * are php's case: they read and write like ordinary` |
|         - | 2920 | `                                            * properties, so PHL declares real slots for them, but` |
|         - | 2921 | `                                            * php's COMPARISON walks the real table and finds nothing` |
|         - | 2922 | `                                            * there -- which is why any two DatePeriods are equal in` |
|         - | 2923 | `                                            * php whatever they contain, while a subclass's own` |
|         - | 2924 | `                                            * property still decides. Read only by the object` |
|         - | 2925 | `                                            * comparator; presentation is the HIDDEN bit's business,` |
|         - | 2926 | `                                            * and these are shown. */` |
|         - | 2927 | `#define PH7_CLASS_ATTR_NATIVE_LAZY  0x400000 /* A NATIVE class's property the OBJECT does not hold until` |
|         - | 2928 | `                                            * its constructor fills it. php's DateInterval and` |
|         - | 2929 | `                                            * DatePeriod are the case: the state lives in a C struct the` |
|         - | 2930 | `                                            * constructor allocates, and the property table is written` |
|         - | 2931 | `                                            * FROM that struct -- so an object nobody constructed has` |
|         - | 2932 | ``                                            * no such property at all, and `$i->y` there is an`` |
|         - | 2933 | ``                                            * `Undefined property` warning, `isset()` is false and`` |
|         - | 2934 | `                                            * get_object_vars()/foreach see nothing. The instance frame` |
|         - | 2935 | ``                                            * skips these at `new`; the whole set is installed, in`` |
|         - | 2936 | `                                            * declared order, the first time a C body writes one` |
|         - | 2937 | `                                            * (PH7_NativeMaterializeLazy), which is every constructor` |
|         - | 2938 | `                                            * and every C factory. */` |
|         - | 2939 | `#define PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT 0x800000 /* A LAZY property php really does DECLARE -- so` |
|         - | 2940 | `                                            * Reflection lists it whatever the object holds -- and whose` |
|         - | 2941 | `                                            * READ, while the slot is still absent, answers the declared` |
|         - | 2942 | `                                            * literal in SILENCE. php's split between its two handlers:` |
|         - | 2943 | `                                            * DatePeriod declares its seven and reads them through a` |
|         - | 2944 | `                                            * read_property that answers the ZEROED struct` |
|         - | 2945 | `                                            * (null/0/false), while DateInterval declares nothing at all` |
|         - | 2946 | `                                            * and its ten are undefined until the constructor runs. The` |
|         - | 2947 | `                                            * literal IS that zeroed field, which is why one bit says` |
|         - | 2948 | `                                            * both things. */` |
|         - | 2949 | `#define PH7_CLASS_ATTR_NATIVE_NOWRITE 0x1000000 /* A NATIVE class's property whose write_property` |
|         - | 2950 | `                                            * handler REFUSES every write. DatePeriod's seven are` |
|         - | 2951 | ``                                            * php's case: its handler answers `Cannot modify readonly`` |
|         - | 2952 | ``                                            * property C::$p` -- the readonly WORDING without the`` |
|         - | 2953 | `                                            * readonly flag, so Reflection still reports isReadOnly()` |
|         - | 2954 | ``                                            * false -- and its unset handler answers `Cannot unset`` |
|         - | 2955 | ``                                            * C::$p`. Every write form is refused, not just `=`:`` |
|         - | 2956 | ``                                            * `++`, a by-reference bind, a destructuring target, and`` |
|         - | 2957 | `                                            * a write to an object that was never constructed. */` |
|         - | 2958 | `#define PH7_CLASS_ATTR_NATIVE_ONDEMAND 0x2000000 /* A LAZY property the group materialization` |
|         - | 2959 | `                                            * SKIPS: it is installed only when a C body writes` |
|         - | 2960 | `                                            * it by name, so an object that never took one does` |
|         - | 2961 | `                                            * not carry the name at all. php's DateInterval` |
|         - | 2962 | ``                                            * `date_string` is the case -- it exists on an`` |
|         - | 2963 | `                                            * interval built from a STRING and on no other, and` |
|         - | 2964 | ``                                            * `isset()`/`property_exists()` answer false there. */`` |
|         - | 2965 | `#define PH7_CLASS_ATTR_NATIVE_LAZYSLOT 0x20000000 /* A NATIVE class's property php declares as a real` |
|         - | 2966 | `                                            * slot it fills on the FIRST read: the object carries` |
|         - | 2967 | ``                                            * the name from `new` in its declared position and`` |
|         - | 2968 | `                                            * uninitialized, so every presentation surface leaves` |
|         - | 2969 | `                                            * it out until something reads it, and the read routes` |
|         - | 2970 | `                                            * to the class's own handler rather than raising the` |
|         - | 2971 | ``                                            * uninitialized-typed Error. php's `Dom\Element::$children`,`` |
|         - | 2972 | ``                                            * `$classList` and `Dom\Document::$implementation` are the`` |
|         - | 2973 | `                                            * case -- Reflection reports them NOT virtual, and` |
|         - | 2974 | ``                                            * `get_object_vars()` answers them only after the read.`` |
|         - | 2975 | `                                            * Rides with NATIVE_NOWRITE: the handler owns the write. */` |
|         - | 2976 | `#define PH7_CLASS_ATTR_FABRICATED   0x10000000 /* METHOD php builds on demand instead of keeping in` |
|         - | 2977 | `                                 * the class's function table. See PH7_MOD_FABRICATED for the five` |
|         - | 2978 | `                                 * doors and how each of them answers. */` |
|         - | 2979 | `#define PH7_CLASS_ATTR_NATIVE_NOSLOT 0x4000000 /* A NATIVE class's VIRTUAL property: php DECLARES the` |
|         - | 2980 | `                                            * name (Reflection lists it, property_exists() answers` |
|         - | 2981 | `                                            * true, isVirtual() true and hasDefaultValue() false) and` |
|         - | 2982 | `                                            * keeps NO slot for it -- every value is a read_property /` |
|         - | 2983 | `                                            * write_property handler over the extension's own state.` |
|         - | 2984 | ``                                            * ext/dom is the case: all forty of `DOMDocument`'s are`` |
|         - | 2985 | ``                                            * handlers, which is why php's `(array)` cast,`` |
|         - | 2986 | ``                                            * `get_object_vars()`, `json_encode()`, `foreach`,`` |
|         - | 2987 | ``                                            * `var_export()` and `get_mangled_object_vars()` show`` |
|         - | 2988 | ``                                            * NOTHING for one while `print_r`/`var_dump` show the`` |
|         - | 2989 | `                                            * whole forty (the get_debug_info handler, ph7_class::` |
|         - | 2990 | ``                                            * xPresent). The instance frame skips these at `new`, so`` |
|         - | 2991 | `                                            * a read, a write and an isset() all take the miss path` |
|         - | 2992 | `                                            * and reach the class's __get/__set/__isset exactly as an` |
|         - | 2993 | ``                                            * undeclared name does; `unset()` is php's`` |
|         - | 2994 | ``                                            * `Cannot unset C::$p` rather than a silent no-op. */`` |
|         - | 2995 | `/* next free bit: 0x8000000 */` |
|         - | 2996 | `/*` |
|         - | 2997 | ` * Does a store into this property's slot have to be FILTERED? Two unrelated` |
|         - | 2998 | ` * reasons say yes -- a declared TYPE to enforce and a native class's own write` |
|         - | 2999 | ` * handler -- and both are answered by one lookup, since pVm->hTypedSlot keys` |
|         - | 3000 | ` * every filtered slot by its memobj index. Instantiation registers on this` |
|         - | 3001 | ` * predicate and the teardown paths deregister on it, so the two must never` |
|         - | 3002 | ` * disagree.` |
|         - | 3003 | ` */` |
|         - | 3004 | `#define PH7_ATTR_STORE_FILTERED(pAttr) \` |
|         - | 3005 | `	(((pAttr)->iFlags & (PH7_CLASS_ATTR_TYPED\|PH7_CLASS_ATTR_NATIVE_SET \` |
|         - | 3006 | `	                     \|PH7_CLASS_ATTR_NATIVE_NOWRITE)) != 0)` |
|         - | 3007 | `/*` |
|         - | 3008 | ` * Declaring a class from C (oo_native.c).` |
|         - | 3009 | ` *` |
|         - | 3010 | ` * A subsystem describes its classes as static tables and hands them to` |
|         - | 3011 | ` * PH7_InstallNativeClasses(), which drives the very builders the compiler drives` |
|         - | 3012 | `` * for `class Foo {}`. The point of the exercise is the METHOD table: a method's`` |
|         - | 3013 | ` * body may be a C routine (VM_FUNC_NATIVE), so the engine-access helpers that had` |
|         - | 3014 | `` * to be global `__prefix_verb()` thunks — because only a global function could be`` |
|         - | 3015 | ` * C — become methods of the class they always belonged to.` |
|         - | 3016 | ` */` |
|         - | 3017 | `/* Member modifiers. Visibility defaults to public when none is given. */` |
|         - | 3018 | `#define PH7_MOD_PUBLIC     0x00` |
|         - | 3019 | `#define PH7_MOD_PROTECTED  0x01` |
|         - | 3020 | `#define PH7_MOD_PRIVATE    0x02` |
|         - | 3021 | `#define PH7_MOD_STATIC     0x04` |
|         - | 3022 | `#define PH7_MOD_FINAL      0x08` |
|         - | 3023 | `#define PH7_MOD_ABSTRACT   0x10 /* No body: an interface's method, or an abstract declaration */` |
|         - | 3024 | `#define PH7_MOD_HIDDEN     0x20 /* PROPERTY only: an engine slot php keeps in its own struct and` |
|         - | 3025 | `                                 * never presents (PH7_CLASS_ATTR_HIDDEN). */` |
|         - | 3026 | ``#define PH7_MOD_READONLY   0x40 /* PROPERTY only: php's `readonly` (PH7_CLASS_ATTR_READONLY) */`` |
|         - | 3027 | ``#define PH7_MOD_PROT_SET   0x80 /* PROPERTY only: php's `protected(set)` asymmetric visibility */`` |
|         - | 3028 | ``#define PH7_MOD_PRIV_SET   0x100 /* PROPERTY only: php's `private(set)` asymmetric visibility */`` |
|         - | 3029 | `#define PH7_MOD_ONDEMAND   0x200 /* PROPERTY only: installed on the object only when a C body` |
|         - | 3030 | `                                  * writes it (PH7_CLASS_ATTR_NATIVE_ONDEMAND) */` |
|         - | 3031 | `#define PH7_MOD_VIRTUAL    0x400 /* PROPERTY only: php's VIRTUAL native property -- declared on the` |
|         - | 3032 | `                                  * class and answered by its own handlers, with NO slot on the` |
|         - | 3033 | `                                  * object (PH7_CLASS_ATTR_NATIVE_NOSLOT) */` |
|         - | 3034 | `#define PH7_MOD_LAZYSLOT   0x1000 /* PROPERTY only: php's lazily-filled REAL slot -- declared and` |
|         - | 3035 | ``                                  * carried uninitialized from `new`, filled by the class's own`` |
|         - | 3036 | `                                  * property handler on the first read` |
|         - | 3037 | `                                  * (PH7_CLASS_ATTR_NATIVE_LAZYSLOT). */` |
|         - | 3038 | `#define PH7_MOD_FABRICATED 0x800 /* METHOD only: php does not keep this one in the class's function` |
|         - | 3039 | `                                  * table -- it BUILDS it when something asks (Closure::__invoke is` |
|         - | 3040 | `                                  * the whole set). Present to method_exists(), hasMethod(),` |
|         - | 3041 | `                                  * getMethod() and getMethods(); absent from get_class_methods()` |
|         - | 3042 | `                                  * and from the class's own export, and refused by` |
|         - | 3043 | `                                  * ReflectionMethod::__construct, which is exactly the shape those` |
|         - | 3044 | `                                  * five doors have in php (PH7_CLASS_ATTR_FABRICATED). */` |
|         - | 3045 | `/* Literal kinds a native class constant may carry */` |
|         - | 3046 | `#define PH7_NATIVE_VAL_NULL   0` |
|         - | 3047 | `#define PH7_NATIVE_VAL_INT    1` |
|         - | 3048 | `#define PH7_NATIVE_VAL_STRING 2` |
|         - | 3049 | `#define PH7_NATIVE_VAL_BOOL   3` |
|         - | 3050 | `#define PH7_NATIVE_VAL_DOUBLE 4` |
|         - | 3051 | ``#define PH7_NATIVE_VAL_ARRAY  6 /* The EMPTY array, php's `private array $trace = [];`. The`` |
|         - | 3052 | `                                 * only array literal a stub default needs — anything with` |
|         - | 3053 | `                                 * elements would want the compiler's byte-code. */` |
|         - | 3054 | `#define PH7_NATIVE_VAL_NONE   5 /* On a PROPERTY row only: the slot has NO default at all,` |
|         - | 3055 | ``                                 * php's `public string $name;`. It needs a declared zType to`` |
|         - | 3056 | `                                 * mean anything (an untyped slot without a default is null),` |
|         - | 3057 | ``                                 * and it makes the property UNINITIALIZED at `new` — reading`` |
|         - | 3058 | `                                 * it before the class's own C body writes it is php's` |
|         - | 3059 | `                                 * "must not be accessed before initialization" Error, and` |
|         - | 3060 | `                                 * hasDefaultValue() answers false. PH7_NATIVE_VAL_NULL is the` |
|         - | 3061 | ``                                 * different thing it reads like: an explicit `= null`. */`` |
|         - | 3062 | `typedef struct PH7_NativeMethodDef PH7_NativeMethodDef;` |
|         - | 3063 | `typedef struct PH7_NativeConstDef  PH7_NativeConstDef;` |
|         - | 3064 | `typedef struct PH7_NativeClassSpec PH7_NativeClassSpec;` |
|         - | 3065 | `struct PH7_NativeMethodDef` |
|         - | 3066 | `{` |
|         - | 3067 | `	const char *zName;       /* php-visible method name */` |
|         - | 3068 | `	sxi32 iMods;             /* PH7_MOD_* */` |
|         - | 3069 | `	const char *zSig;        /* PHP-style parameter list ("string $name, int $flags = 0"),` |
|         - | 3070 | `	                          * or 0 for "unenforced". Static storage: never freed. Drives` |
|         - | 3071 | `	                          * arity enforcement, the by-ref mask AND Reflection, from the` |
|         - | 3072 | `	                          * one string — the same contract aBuiltinSig[] has. */` |
|         - | 3073 | `	const char *zRet;        /* Return-type text, or 0 */` |
|         - | 3074 | `	ProchHostFunction xFunc; /* The body */` |
|         - | 3075 | `};` |
|         - | 3076 | `struct PH7_NativeConstDef` |
|         - | 3077 | `{` |
|         - | 3078 | `	const char *zName;` |
|         - | 3079 | `	sxi32 iMods;` |
|         - | 3080 | `	sxi32 iType;             /* PH7_NATIVE_VAL_* */` |
|         - | 3081 | `	ph7_int64 iValue;        /* INT / BOOL */` |
|         - | 3082 | `	const char *zValue;      /* STRING */` |
|         - | 3083 | `	double rValue;           /* DOUBLE */` |
|         - | 3084 | `};` |
|         - | 3085 | `/*` |
|         - | 3086 | ` * A declared property. Its default is the same literal record a constant uses --` |
|         - | 3087 | ` * a compiled declaration would carry compiled byte-code here, which the builder` |
|         - | 3088 | ` * has no compiler to emit, so the value is stated directly and materialized at` |
|         - | 3089 | `` * `new` (instance) or at mount (static) by PH7_NativeLiteralValue.`` |
|         - | 3090 | ` */` |
|         - | 3091 | `typedef struct PH7_NativePropDef PH7_NativePropDef;` |
|         - | 3092 | `struct PH7_NativePropDef` |
|         - | 3093 | `{` |
|         - | 3094 | `	const char *zName;` |
|         - | 3095 | `	sxi32 iMods;             /* PH7_MOD_* (STATIC supported; FINAL ignored) */` |
|         - | 3096 | `	PH7_NativeConstDef sDefault; /* iType PH7_NATIVE_VAL_NULL = plain null default,` |
|         - | 3097 | `	                              * PH7_NATIVE_VAL_NONE = no default at all (typed slots) */` |
|         - | 3098 | `	const char *zType;       /* Declared type as php writes it ("?string", "int", "DateInterval"),` |
|         - | 3099 | `	                          * or 0 for an untyped slot. Enforced on every store and printed by` |
|         - | 3100 | ``	                          * Reflection exactly as a compiled `public ?string $p` would be —`` |
|         - | 3101 | `	                          * php declares a type on every property it presents, so a slot the` |
|         - | 3102 | ``	                          * class SHOWS wants one. Single atoms only (a leading `?` plus one`` |
|         - | 3103 | `	                          * scalar keyword or class name); a union needs the compiler's` |
|         - | 3104 | `	                          * alternative set and is not expressible here. */` |
|         - | 3105 | `};` |
|         - | 3106 | `struct PH7_NativeClassSpec` |
|         - | 3107 | `{` |
|         - | 3108 | `	const char *zName;` |
|         - | 3109 | `	const char *zParent;     /* or 0 */` |
|         - | 3110 | `	const char *zImplements; /* comma-separated list, or 0 */` |
|         - | 3111 | `	sxi32 iFlags;            /* PH7_CLASS_FINAL / ABSTRACT / INTERFACE / READONLY */` |
|         - | 3112 | `	const PH7_NativeMethodDef *aMethod; sxu32 nMethod;` |
|         - | 3113 | `	const PH7_NativeConstDef  *aConst;  sxu32 nConst;` |
|         - | 3114 | `	const PH7_NativePropDef   *aProp;   sxu32 nProp;` |
|         - | 3115 | `	void (*xRelease)(ph7_vm *,ph7_class_instance *); /* or 0; see ph7_class::xRelease */` |
|         - | 3116 | `	const PH7_NativeIterVtab *pIterVtab; /* or 0; see ph7_class::pIterVtab */` |
|         - | 3117 | `	/* xNew is not stated here: a spec that wants one installs it after mounting` |
|         - | 3118 | `	 * with PH7_NativeClassInstallNewHook(), the way the property, set and` |
|         - | 3119 | `	 * comparison hooks are installed. */` |
|         - | 3120 | `	sxi32 (*xPresent)(ph7_vm *,ph7_class_instance *,ph7_value *,int); /* or 0; see ph7_class::xPresent */` |
|         - | 3121 | `};` |
|         - | 3122 | `/*` |
|         - | 3123 | `` * One `case Name = <literal>;` of a native ENUM. The backing value is the same`` |
|         - | 3124 | ` * literal record a constant carries; iType PH7_NATIVE_VAL_NULL is a PURE enum's` |
|         - | 3125 | ` * case, which has no value at all.` |
|         - | 3126 | ` */` |
|         - | 3127 | `typedef struct PH7_NativeEnumCase PH7_NativeEnumCase;` |
|         - | 3128 | `struct PH7_NativeEnumCase` |
|         - | 3129 | `{` |
|         - | 3130 | `	const char *zName;` |
|         - | 3131 | `	PH7_NativeConstDef sValue;   /* the backing literal; zName/iMods unused */` |
|         - | 3132 | `};` |
|         - | 3133 | `PH7_PRIVATE sxi32 PH7_InstallNativeClasses(ph7_vm *pVm,const PH7_NativeClassSpec *aSpec,sxu32 nSpec);` |
|         - | 3134 | `PH7_PRIVATE int PH7_ClassInstancePresent(ph7_class_instance *pThis,ph7_value *pOut,int bDebug);` |
|         - | 3135 | `PH7_PRIVATE int PH7_ClassHasNativeDim(ph7_class *pClass);` |
|         - | 3136 | `PH7_PRIVATE int PH7_ClassNativeDim(ph7_class_instance *pThis,PH7_NativeDimCtx *pCtx);` |
|         - | 3137 | `/* Offer a dimension WRITE / APPEND / UNSET to the class's own handler, with the` |
|         - | 3138 | ` * offset and the incoming value. Answers 1 when the handler TOOK it (or refused` |
|         - | 3139 | ` * it in its own words, which zThrowClass then carries) and 0 when the caller` |
|         - | 3140 | ` * must raise the ordinary refusal. */` |
|         - | 3141 | `PH7_PRIVATE int PH7_ClassNativeDimStore(ph7_class_instance *pThis,int iMode,` |
|         - | 3142 | `	ph7_value *pOffset,ph7_value *pValue,PH7_NativeDimCtx *pCtx);` |
|         - | 3143 | `/* The refusal a native container gives a dimension WRITE/APPEND/UNSET: its own` |
|         - | 3144 | `` * sentence when its hook words one, and php's `Cannot use object of type C as`` |
|         - | 3145 | `` * array` for every class that does not. Answers the message length. */`` |
|         - | 3146 | `PH7_PRIVATE sxu32 PH7_ClassNativeDimRefusal(ph7_class_instance *pThis,int iMode,` |
|         - | 3147 | `	char *zMsg,sxu32 nMsg);` |
|         - | 3148 | `/* php's instantiation gate -- interface / trait / enum / abstract / a class` |
|         - | 3149 | `` * whose create_object handler refuses -- asked by every C-side `new`. */`` |
|         - | 3150 | `PH7_PRIVATE sxi32 PH7_VmCheckInstantiable(ph7_context *pCtx,ph7_class *pClass);` |
|         - | 3151 | `PH7_PRIVATE int PH7_ClassHasNativeProp(ph7_class *pClass);` |
|         - | 3152 | `PH7_PRIVATE int PH7_ClassNativeProp(ph7_class_instance *pThis,PH7_NativePropCtx *pCtx);` |
|         - | 3153 | `PH7_PRIVATE int PH7_ClassNativePropAsk(ph7_class_instance *pThis,PH7_NativePropCtx *pCtx,` |
|         - | 3154 | `	int iMode,const SyString *pName,ph7_value *pResult);` |
|         - | 3155 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallPropHook(ph7_vm *pVm,const char *zClass,` |
|         - | 3156 | `	void (*xProp)(ph7_vm *,ph7_class_instance *,PH7_NativePropCtx *));` |
|         - | 3157 | `PH7_PRIVATE int PH7_ClassNativePropOwns(ph7_class_instance *pThis,const SyString *pName);` |
|         - | 3158 | `PH7_PRIVATE int PH7_ClassNativeSet(ph7_class_instance *pThis,PH7_NativeSetCtx *pCtx);` |
|         - | 3159 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallSetHook(ph7_vm *pVm,const char *zClass,` |
|         - | 3160 | `	void (*xSet)(ph7_vm *,ph7_class_instance *,PH7_NativeSetCtx *));` |
|         - | 3161 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallNewHook(ph7_vm *pVm,const char *zClass,` |
|         - | 3162 | `	void (*xNew)(ph7_vm *,ph7_class_instance *));` |
|         - | 3163 | `PH7_PRIVATE int PH7_ClassNativeCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,sxi32 *pResult);` |
|         - | 3164 | `PH7_PRIVATE int PH7_ClassNativeCmpValue(ph7_class_instance *pLeft,ph7_value *pOther,` |
|         - | 3165 | `	int bReversed,sxi32 *pResult);` |
|         - | 3166 | `PH7_PRIVATE void PH7_NativeCmpOpaqueHandle(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeCmpCtx *pCtx);` |
|         - | 3167 | `PH7_PRIVATE int PH7_ClassCastsToHandleId(ph7_class *pClass);` |
|         - | 3168 | `PH7_PRIVATE int PH7_ClassNumberIsString(ph7_class *pClass);` |
|         - | 3169 | `PH7_PRIVATE int PH7_ClassVarsFromPresent(ph7_class *pClass);` |
|         - | 3170 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallCmpHook(ph7_vm *pVm,const char *zClass,` |
|         - | 3171 | `	void (*xCmp)(ph7_vm *,ph7_class_instance *,PH7_NativeCmpCtx *));` |
|         - | 3172 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallArithHook(ph7_vm *pVm,const char *zClass,` |
|         - | 3173 | `	void (*xArith)(ph7_vm *,ph7_class_instance *,PH7_NativeArithCtx *));` |
|         - | 3174 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallBoolHook(ph7_vm *pVm,const char *zClass,` |
|         - | 3175 | `	int (*xBool)(ph7_vm *,ph7_class_instance *));` |
|         - | 3176 | `PH7_PRIVATE int PH7_ClassNativeBool(ph7_class_instance *pThis,int *pOut);` |
|         - | 3177 | `/*` |
|         - | 3178 | ` * What VmArithOperandStep() decided about one operator's pair.` |
|         - | 3179 | ` */` |
|         - | 3180 | `#define PH7_ARITH_ORDINARY  0   /* no handler: run the numeric arithmetic */` |
|         - | 3181 | `#define PH7_ARITH_HANDLED   1   /* a handler answered; the destination already holds it */` |
|         - | 3182 | `#define PH7_ARITH_REFUSED  (-1) /* throw *pzClass with the message in pMsgOut */` |
|         - | 3183 | `PH7_PRIVATE int VmArithOperandStep(ph7_vm *pVm,ph7_value *pLeft,ph7_value *pRight,` |
|         - | 3184 | `	const char *zOp,ph7_value *pDest,const char **pzClass,SyBlob *pMsgOut);` |
|         - | 3185 | `PH7_PRIVATE int PH7_ValueHasArithHandler(ph7_value *pVal);` |
|         - | 3186 | `PH7_PRIVATE sxi32 PH7_NativeClassMarkVirtualProps(ph7_vm *pVm,const char *zClass);` |
|         - | 3187 | `PH7_PRIVATE sxi32 PH7_NativeClassMarkLazyProps(ph7_vm *pVm,const char *zClass,int bDefaultRead);` |
|         - | 3188 | `PH7_PRIVATE sxi32 PH7_NativeClassMarkNoWriteProps(ph7_vm *pVm,const char *zClass);` |
|         - | 3189 | `PH7_PRIVATE void PH7_NativeMaterializeLazy(ph7_vm *pVm,ph7_class_instance *pObj);` |
|         - | 3190 | `PH7_PRIVATE void PH7_NativeHideAttr(ph7_class_instance *pObj,const char *zName);` |
|         - | 3191 | `/*` |
|         - | 3192 | ` * The refusal a native compare handler carried back (ph7_vm::zCmpRefusalClass):` |
|         - | 3193 | ` * pending? raise it here, where a throw can be routed; raise it on a host CALL` |
|         - | 3194 | ` * CONTEXT, so a builtin that compared reports it the way its own throws are` |
|         - | 3195 | ` * reported; or drop it, for the two comparison doors that are not PHP execution` |
|         - | 3196 | ` * at all (the public ph7_value_compare, a VM reset).` |
|         - | 3197 | ` */` |
|         - | 3198 | `PH7_PRIVATE int PH7_CmpRefusalPending(ph7_vm *pVm);` |
|         - | 3199 | `/* Record php's comparison-recursion refusal ("Nesting level too deep - recursive` |
|         - | 3200 | ` * dependency?", an Error) through the same channel, for the two walks -- arrays and` |
|         - | 3201 | ` * objects -- that find themselves inside a container they are already inside. */` |
|         - | 3202 | `PH7_PRIVATE void PH7_CmpRefusalNesting(ph7_vm *pVm);` |
|         - | 3203 | `PH7_PRIVATE sxi32 PH7_CmpRefusalRaise(ph7_vm *pVm);` |
|         - | 3204 | `PH7_PRIVATE sxi32 PH7_CmpRefusalRaiseCtx(ph7_context *pCtx);` |
|         - | 3205 | `PH7_PRIVATE void PH7_CmpRefusalClear(ph7_vm *pVm);` |
|         - | 3206 | `PH7_PRIVATE sxi32 PH7_InstallEnumInterfaceMethods(ph7_vm *pVm,ph7_class *pClass);` |
|         - | 3207 | `PH7_PRIVATE sxi32 PH7_InstallNativeEnum(ph7_vm *pVm,const char *zName,sxu32 nBacking,` |
|         - | 3208 | `	const PH7_NativeEnumCase *aCase,sxu32 nCase,` |
|         - | 3209 | `	const PH7_NativeMethodDef *aMethod,sxu32 nMethod);` |
|         - | 3210 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallMethod(ph7_vm *pVm,ph7_class *pClass,` |
|         - | 3211 | `	const PH7_NativeMethodDef *pDef,void *pUserData);` |
|         - | 3212 | `/*` |
|         - | 3213 | `` * Attach one `#[Name(literal, ...)]` to a class declared from C. php puts an`` |
|         - | 3214 | ` * attribute on two of its own attribute classes, and the whole record — the FQN` |
|         - | 3215 | ` * plus its arguments — is what the engine reads to VALIDATE a target and what` |
|         - | 3216 | ` * ReflectionAttribute answers.` |
|         - | 3217 | ` */` |
|         - | 3218 | `typedef struct PH7_NativeAttrArg PH7_NativeAttrArg;` |
|         - | 3219 | `struct PH7_NativeAttrArg` |
|         - | 3220 | `{` |
|         - | 3221 | `	const char *zName;           /* named argument, or 0 for a positional one */` |
|         - | 3222 | `	PH7_NativeConstDef sValue;   /* the literal; zName/iMods unused */` |
|         - | 3223 | `};` |
|         - | 3224 | `PH7_PRIVATE sxi32 PH7_NativeClassAddAttribute(ph7_vm *pVm,ph7_class *pClass,` |
|         - | 3225 | `	const char *zAttr,const PH7_NativeAttrArg *aArg,sxu32 nArg);` |
|         - | 3226 | `PH7_PRIVATE sxi32 PH7_NativeMethodSetNoDiscard(ph7_vm *pVm,ph7_class *pClass,` |
|         - | 3227 | `	const char *zMethod,const PH7_NativeAttrArg *aArg,sxu32 nArg);` |
|         - | 3228 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallProperty(ph7_vm *pVm,ph7_class *pClass,` |
|         - | 3229 | `	const PH7_NativePropDef *pDef);` |
|         - | 3230 | `PH7_PRIVATE void PH7_NativeLiteralValue(ph7_vm *pVm,const void *pLiteral,ph7_value *pOut);` |
|         - | 3231 | `PH7_PRIVATE void PH7_NativeSetProp(ph7_vm *pVm,ph7_class_instance *pObj,` |
|         - | 3232 | `	const char *zProp,sxu32 nProp,ph7_value *pSrcVal);` |
|         - | 3233 | `/*` |
|         - | 3234 | ` * Reading and writing a native instance's own declared slots. Every native class` |
|         - | 3235 | ` * does this constantly (the date family had a private copy of the whole set), so` |
|         - | 3236 | ` * the accessors live with the builder that declares the slots.` |
|         - | 3237 | ` */` |
|         - | 3238 | `PH7_PRIVATE ph7_value * PH7_NativeAttr(ph7_class_instance *pObj,const char *zName);` |
|         - | 3239 | `PH7_PRIVATE int PH7_NativeAttrIsUninit(ph7_class_instance *pObj,const char *zName);` |
|         - | 3240 | `PH7_PRIVATE sxi64 PH7_NativeAttrInt(ph7_class_instance *pObj,const char *zName);` |
|         - | 3241 | `PH7_PRIVATE ph7_class_instance * PH7_NativeAttrObj(ph7_class_instance *pObj,const char *zName);` |
|         - | 3242 | `PH7_PRIVATE int PH7_NativeAttrTruthy(ph7_class_instance *pObj,const char *zName);` |
|         - | 3243 | `PH7_PRIVATE void PH7_NativeAttrStr(ph7_class_instance *pObj,const char *zName,` |
|         - | 3244 | `	const char **pzOut,int *pnOut);` |
|         - | 3245 | `PH7_PRIVATE void PH7_NativeSetAttrInt(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,sxi64 iVal);` |
|         - | 3246 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         - | 3247 | `PH7_PRIVATE void PH7_NativeSetAttrReal(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,ph7_real rVal);` |
|         - | 3248 | `#endif` |
|         - | 3249 | `PH7_PRIVATE void PH7_NativeSetAttrStr(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,` |
|         - | 3250 | `	const char *zVal,int nVal);` |
|         - | 3251 | `PH7_PRIVATE void PH7_NativeSetAttrBool(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,int bVal);` |
|         - | 3252 | `PH7_PRIVATE void PH7_NativeSetAttrObj(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,` |
|         - | 3253 | `	ph7_class_instance *pVal);` |
|         - | 3254 | `PH7_PRIVATE void PH7_NativeResultObject(ph7_context *pCtx,ph7_class_instance *pObj);` |
|         - | 3255 | `/*` |
|         - | 3256 | ` * php's InternalIterator: the Iterator a native IteratorAggregate answers when the` |
|         - | 3257 | ` * PHP it replaced was a GENERATOR -- the one body a C method cannot be. It is one` |
|         - | 3258 | ` * class in php too, wrapping whatever internal iterator the aggregate handed over,` |
|         - | 3259 | ` * so PHL gives it the same shape: fixed state slots on the iterator, and a vtable` |
|         - | 3260 | ` * on the AGGREGATE'S CLASS (ph7_class::pIterVtab, php's get_iterator handler) that` |
|         - | 3261 | ` * knows how to position and advance that aggregate's cursor. current()/key()/valid()` |
|         - | 3262 | ` * need no vtable entry -- they read the slots the two below leave behind.` |
|         - | 3263 | ` */` |
|         - | 3264 | `struct PH7_NativeIterVtab` |
|         - | 3265 | `{` |
|         - | 3266 | `	void (*xRewind)(ph7_vm *pVm,ph7_class_instance *pIt); /* settle on the first element */` |
|         - | 3267 | `	void (*xNext)(ph7_vm *pVm,ph7_class_instance *pIt);   /* settle on the one after */` |
|         - | 3268 | `	/* Optional: publish the cursor back onto the AGGREGATE, for a class that shows` |
|         - | 3269 | `	 * its walk as one of its own properties. DatePeriod is the case -- its` |
|         - | 3270 | ``	 * `current` is the cursor, and php writes it from every iterator method rather`` |
|         - | 3271 | ``	 * than from the walk itself, so `getIterator()` alone leaves it where the last`` |
|         - | 3272 | `	 * walk left it and the first valid()/current()/key()/rewind()/next() moves it.` |
|         - | 3273 | `	 * Called by the InternalIterator methods, never by the vtable's own halves. */` |
|         - | 3274 | `	void (*xPublish)(ph7_vm *pVm,ph7_class_instance *pIt);` |
|         - | 3275 | `	/* Optional: may this iterator be WALKED at all? Answered per call rather than` |
|         - | 3276 | `	 * once at creation because php refuses at the walk and not at the door --` |
|         - | 3277 | `	 * DatePeriod::getIterator() on an object nobody constructed hands back a real` |
|         - | 3278 | `	 * InternalIterator there, and the DateObjectError arrives at the first` |
|         - | 3279 | `	 * rewind(). Non-zero means the guard raised; the method then answers nothing` |
|         - | 3280 | `	 * and the host-call boundary reports the throw. */` |
|         - | 3281 | `	int (*xGuard)(ph7_context *pCtx,ph7_class_instance *pIt);` |
|         - | 3282 | `};` |
|         - | 3283 | `/* The state slots, private to InternalIterator and shared by every vtable:` |
|         - | 3284 | ` * the aggregate, the value and key at the cursor, an integer cursor and a spare` |
|         - | 3285 | ` * one for the vtable's own bookkeeping, and whether the walk is over. */` |
|         - | 3286 | `#define PH7_NATIVE_IT_SRC  "__src"` |
|         - | 3287 | `#define PH7_NATIVE_IT_CUR  "__cur"` |
|         - | 3288 | `#define PH7_NATIVE_IT_KEY  "__key"` |
|         - | 3289 | `#define PH7_NATIVE_IT_POS  "__pos"` |
|         - | 3290 | `#define PH7_NATIVE_IT_AUX  "__aux"` |
|         - | 3291 | `#define PH7_NATIVE_IT_DONE "__done"` |
|         - | 3292 | `PH7_PRIVATE sxi32 PH7_VmInstallNativeIterator(ph7_vm *pVm);` |
|         - | 3293 | `PH7_PRIVATE ph7_class_instance * PH7_NativeIteratorNew(ph7_vm *pVm,ph7_class_instance *pSrc);` |
|         - | 3294 | `/*` |
|         - | 3295 | ` * Each class method is parsed out and stored in an instance of the following` |
|         - | 3296 | ` * structure.` |
|         - | 3297 | ` * PH7 introduced some powerfull extensions to the PHP 5 programming` |
|         - | 3298 | ` * language like function overloading,type hinting,complex default` |
|         - | 3299 | ` * arguments and many more.` |
|         - | 3300 | ` * Please refer to the official documentation for more information.` |
|         - | 3301 | ` */` |
|         - | 3302 | `struct ph7_class_method` |
|         - | 3303 | `{` |
|         - | 3304 | `	ph7_vm_func sFunc;   /* Compiled method body */` |
|         - | 3305 | `	SyString sVmName;    /* Automatically generated name assigned to this method.` |
|         - | 3306 | `						  * Typically this is "[class_name__method_name@random_string]"` |
|         - | 3307 | `						  */` |
|         - | 3308 | `	sxi32 iProtection;   /* Protection level [i.e: public,private,protected] */` |
|         - | 3309 | `	sxi32 iFlags;        /* Methods configuration */` |
|         - | 3310 | `	sxi32 iCloneDepth;   /* Clone depth [Only used by the magic method __clone ] */` |
|         - | 3311 | `    sxu32 nLine;         /* Line on which this method was defined */` |
|         - | 3312 | `};` |
|         - | 3313 | `/*` |
|         - | 3314 | ` * Each active object (class instance) is represented by an instance of` |
|         - | 3315 | ` * the following structure.` |
|         - | 3316 | ` */` |
|         - | 3317 | `struct ph7_class_instance` |
|         - | 3318 | `{` |
|         - | 3319 | `	ph7_vm *pVm;        /* VM that own this instance */` |
|         - | 3320 | `	ph7_class *pClass;  /* Object is an instance of this class */` |
|         - | 3321 | `	SyHash hAttr;       /* Hashtable of active class members */` |
|         - | 3322 | `	sxi32 iRef;         /* Reference count */` |
|         - | 3323 | `	sxi32 iFlags;       /* Control flags */` |
|         - | 3324 | `	sxu32 nObjId;       /* Per-instance monotonic handle id (from pVm->nNextObjId,` |
|         - | 3325 | `	                     * never reused). Drives spl_object_id/hash + var_dump #N. */` |
|         - | 3326 | `	sxu32 nGcRoot;      /* 1-based row in the collector's root buffer, 0 while unbuffered */` |
|         - | 3327 | `	sxu8 iGcColor;      /* PH7_GC_* -- see vm_gc.c */` |
|         - | 3328 | `	PH7_AttrIter *pActiveIters; /* Walks of hAttr currently in flight over this object` |
|         - | 3329 | `	                     * (foreach, array_walk). A property removed under one of them` |
|         - | 3330 | `	                     * advances its cursor; one appended re-arms an exhausted one. */` |
|         - | 3331 | `};` |
|         - | 3332 | `/*` |
|         - | 3333 | ` * ph7_class_instance::iFlags bit set while the object's __clone() magic method` |
|         - | 3334 | ` * runs. PHP 8.3 lets __clone() re-initialize the cloned object's readonly` |
|         - | 3335 | ` * properties, so the readonly store guard consults this flag on the executing` |
|         - | 3336 | ` * $this. (Other iFlags bits are declared privately in their owning .c file:` |
|         - | 3337 | ` * 0x001 destroyed, 0x002 dumping, 0x004 fcc-bound.)` |
|         - | 3338 | ` */` |
|         - | 3339 | `#define CLASS_INSTANCE_DESTROYED 0x001 /* Instance is released (oo.c's teardown latch;` |
|         - | 3340 | `                                        * read by the cycle collector, which must not` |
|         - | 3341 | `                                        * walk a table being torn down) */` |
|         - | 3342 | `/* ph7_class_instance::iFlags bit: php's GC_PROTECT_RECURSION for an object -- the` |
|         - | 3343 | ` * instance counterpart of HASHMAP_DUMPING, and read through the same predicate. */` |
|         - | 3344 | `#define VM_INSTANCE_DUMPING 0x002` |
|         - | 3345 | `#define VM_INSTANCE_CLONING 0x008` |
|         - | 3346 | `/*` |
|         - | 3347 | ` * ph7_class_instance::iFlags bit: this object's __destruct has already been reached for` |
|         - | 3348 | ` * (or the refusal that stands in for it raised), so no later teardown may run it a second` |
|         - | 3349 | ` * time. php keeps the same bit (IS_OBJ_DESTRUCTOR_CALLED) for the same reason -- its` |
|         - | 3350 | ` * shutdown pass calls destructors on objects it does NOT free, and the free that follows` |
|         - | 3351 | ` * must not repeat them. Distinct from CLASS_INSTANCE_DESTROYED 0x001 (oo.c), which says` |
|         - | 3352 | ` * the whole instance is gone. 0x080 because 0x002..0x040 are claimed by unrelated readers` |
|         - | 3353 | ` * of this same word, each on its own kind of object.` |
|         - | 3354 | ` */` |
|         - | 3355 | `#define CLASS_INSTANCE_DTOR_CALLED 0x080` |
|         - | 3356 | `/*` |
|         - | 3357 | ` * ph7_class_instance::iFlags bit: php's GC_PROTECT_RECURSION for an object worn by the` |
|         - | 3358 | ` * COMPARISON walk -- the instance counterpart of HASHMAP_COMPARING, set on the LEFT` |
|         - | 3359 | ` * instance while PH7_ClassInstanceCmp walks its properties, exactly where` |
|         - | 3360 | ` * zend_std_compare_objects protects o1. An object that is its own descendant is refused` |
|         - | 3361 | ` * with php's "Nesting level too deep - recursive dependency?" instead of recursing` |
|         - | 3362 | ` * forever. 0x100 because 0x001..0x080 are claimed by unrelated readers of this word.` |
|         - | 3363 | ` */` |
|         - | 3364 | `#define VM_INSTANCE_COMPARING 0x100` |
|         - | 3365 | `/*` |
|         - | 3366 | ` * ph7_class_instance::iFlags bit set once this object's LAZY native properties` |
|         - | 3367 | ` * (PH7_CLASS_ATTR_NATIVE_LAZY) have been installed. It is the difference between` |
|         - | 3368 | ` * "the constructor has never run, so the name is not a property of this object at` |
|         - | 3369 | ` * all" and "the table exists and this one name was unset()" -- the first is php's` |
|         - | 3370 | ` * dynamic-property creation on a write and an undefined-property warning on a` |
|         - | 3371 | ` * read, the second re-creates the declared slot the ordinary way.` |
|         - | 3372 | ` */` |
|         - | 3373 | `#define VM_INSTANCE_LAZY_DONE 0x010` |
|         - | 3374 | `/*` |
|         - | 3375 | ` * Is this instance slot kept OUT of every surface that shows the object? Two` |
|         - | 3376 | ` * unrelated reasons say yes: the class calls it an engine slot` |
|         - | 3377 | ` * (PH7_CLASS_ATTR_HIDDEN) or this one OBJECT hides it (VM_CLASS_ATTR_UNSEEN).` |
|         - | 3378 | ` * Class-level members are not the object's either, so the one test covers all` |
|         - | 3379 | ` * three.` |
|         - | 3380 | ` */` |
|         - | 3381 | `#define PH7_ATTR_UNPRESENTED(pVmAttr) \` |
|         - | 3382 | `	(((pVmAttr)->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT \` |
|         - | 3383 | `	                              \|PH7_CLASS_ATTR_HIDDEN)) != 0 \` |
|         - | 3384 | `	 \|\| ((pVmAttr)->iState & VM_CLASS_ATTR_UNSEEN) != 0)` |
|         - | 3385 | `/*` |
|         - | 3386 | ` * Is this DECLARED attribute absent from the object because its class declares it` |
|         - | 3387 | ` * LAZILY and nothing has installed the set yet? The two miss paths -- a property` |
|         - | 3388 | ` * write and a by-reference bind -- ask before they re-create a declared slot.` |
|         - | 3389 | ` */` |
|         - | 3390 | `#define PH7_ATTR_LAZY_ABSENT(pAttr,pInst) \` |
|         - | 3391 | `	((((pAttr)->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY) != 0) \` |
|         - | 3392 | `	 && (((pInst)->iFlags & VM_INSTANCE_LAZY_DONE) == 0))` |
|         - | 3393 | `/*` |
|         - | 3394 | ` * A single instruction of the virtual machine has an opcode` |
|         - | 3395 | ` * and as many as three operands.` |
|         - | 3396 | ` * Each VM instruction resulting from compiling a PHP script` |
|         - | 3397 | ` * is stored in an instance of the following structure.` |
|         - | 3398 | ` */` |
|         - | 3399 | `struct VmInstr` |
|         - | 3400 | `{` |
|         - | 3401 | `	sxu8  iOp; /* Operation to preform */` |
|         - | 3402 | `	sxu8  bStrict; /* strict_types mode of the COMPILATION UNIT this instruction came from.` |
|         - | 3403 | `	                * Stamped by PH7_VmEmitInstr beside nLine, and published to` |
|         - | 3404 | `	                * pVm->bCurStrict under the same nLine != 0 gate, so an engine-dispatched` |
|         - | 3405 | `	                * call (a magic method, a property hook) can bind its arguments under the` |
|         - | 3406 | `	                * CALLING file's mode — php's rule — instead of always coercing. Sits in` |
|         - | 3407 | `	                * the padding after iOp: sizeof(VmInstr) is unchanged. */` |
|         - | 3408 | `	sxu8  bDiscard; /* PH7_OP_CALL only: php's !RETURN_VALUE_USED. The statement that owns` |
|         - | 3409 | ``	                * this call throws its answer away (`f();`, not `$x = f();` and not`` |
|         - | 3410 | ``	                * `f() + 1;`), which is the one thing a #[\NoDiscard] callee warns`` |
|         - | 3411 | `	                * about. Set by the codegen at the statement-discard site and cleared` |
|         - | 3412 | ``	                * by a `(void)` cast in front of it, which is php's way of saying the`` |
|         - | 3413 | `	                * drop is deliberate. Padding after bStrict, like bStrict itself.` |
|         - | 3414 | `	                * On a static PH7_OP_MEMBER, PH7_OP_NEW and PH7_OP_LOAD_FCC it means` |
|         - | 3415 | `	                * something else, since none of them is a call's result: the class` |
|         - | 3416 | `	                * operand is the WRITTEN keyword self/static/parent. php resolves` |
|         - | 3417 | `	                * the three only there -- the same string held in a variable is a` |
|         - | 3418 | ``	                * class name nothing can be called (`Class "self" not found`).`` |
|         - | 3419 | `	                * OP_IS_A says the same in its iP1. */` |
|         - | 3420 | `	sxu8  bRefSrc; /* PH7_OP_MEMBER only: this property fetch is a reference SOURCE --` |
|         - | 3421 | ``	                * php's `zend_compile_var(source, BP_VAR_W)`, the fetch a `=&` bind, a`` |
|         - | 3422 | ``	                * `[&$o->p]` element and a by-reference `foreach` make. It stays a`` |
|         - | 3423 | `	                * PH7_MEMBER_READ (a handler-backed property still hands back a COPY),` |
|         - | 3424 | `	                * but a MISSING name is CREATED rather than warned about, exactly as a` |
|         - | 3425 | `	                * write would create it. Padding after bDiscard, like bStrict itself.` |
|         - | 3426 | `	                * On a static METHOD member (iP1==1, iP2==PH7_MEMBER_METHOD) it means` |
|         - | 3427 | `	                * something else, since no property is fetched there: the name was the` |
|         - | 3428 | ``	                * literal `__construct`, which php resolves as the class's constructor`` |
|         - | 3429 | `	                * rather than as a method (vm_ops_oo.c). */` |
|         - | 3430 | `	sxi32 iP1; /* First operand */` |
|         - | 3431 | `	sxu32 iP2; /* Second operand (Often the jump destination) */` |
|         - | 3432 | `	sxu32 nAux; /* A per-instruction scratch word the RUNTIME owns, zero until it writes` |
|         - | 3433 | `	             * one. Two opcodes use it, each for an answer that cannot change under it:` |
|         - | 3434 | `	             *` |
|         - | 3435 | `	             *   PH7_OP_LOAD      the length of the variable NAME in p3. The name is a` |
|         - | 3436 | `	             *   PH7_OP_STORE     NUL-terminated compile-time buffer, and measuring it` |
|         - | 3437 | `	             *                    again on every execution was ~2% of a phpcs run.` |
|         - | 3438 | `	             *                    Written by VmNumberLocals for a body it walks, and` |
|         - | 3439 | `	             *                    lazily on first execution for one it does not.` |
|         - | 3440 | `	             *   PH7_OP_CALL_INIT the pVm->nCallableGen this call site was last screened` |
|         - | 3441 | `	             *                    at, written only when the callee is a compile-time` |
|         - | 3442 | `	             *                    constant (the push behind it is an OP_LOADC).` |
|         - | 3443 | `	             *   PH7_OP_LOADC     how many times this site has run, capped at two -- a` |
|         - | 3444 | `	             *                    site that runs once can never repay a cache record.` |
|         - | 3445 | `	             *   PH7_OP_CALL      the same count, for the same reason (VmCallSiteFor).` |
|         - | 3446 | `	             *` |
|         - | 3447 | `	             * Lives in the padding after iP2, so VmInstr is still 32 bytes and the` |
|         - | 3448 | `	             * bytecode costs nothing extra. */` |
|         - | 3449 | `	void *p3;  /* Third operand (Often Upper layer private data) */` |
|         - | 3450 | `	sxu32 nLine; /* Source line this instruction was compiled from (0 = unknown).` |
|         - | 3451 | `	              * Stamped by PH7_VmEmitInstr from the codegen's current token, so` |
|         - | 3452 | `	              * every one of its ~150 call sites keeps its signature. */` |
|         - | 3453 | `	sxu32 nSite; /* Two opcodes' worth of "this SITE already knows the answer", sharing one` |
|         - | 3454 | `	              * word because no instruction is ever both.` |
|         - | 3455 | `	              *` |
|         - | 3456 | `	              *   PH7_OP_CALL      this site's entry in pVm->aCallSite, plus one (0 = it` |
|         - | 3457 | `	              *   PH7_OP_LOADC     has never asked for one). A CALL remembers which` |
|         - | 3458 | `	              *                    function table entry its callee NAME resolved to; a` |
|         - | 3459 | `	              *                    LOADC which hConstant entry its constant name did.` |
|         - | 3460 | `	              *                    Both are a name hashed once instead of once per` |
|         - | 3461 | `	              *                    execution -- see VmCallSite.` |
|         - | 3462 | `	              *   PH7_OP_LOAD      the NUMBER this body gave the variable in p3, plus` |
|         - | 3463 | `	              *   PH7_OP_STORE     one (0 = the body was never numbered, or the name` |
|         - | 3464 | `	              *                    did not fit PH7_VAR_SLOT_MAX). It indexes the running` |
|         - | 3465 | `	              *                    frame's aLocalSlot -- see VmNumberLocals.` |
|         - | 3466 | `	              *` |
|         - | 3467 | `	              * Lives in the trailing padding after nLine, so VmInstr is still 32 bytes. */` |
|         - | 3468 | `};` |
|         - | 3469 | `/*` |
|         - | 3470 | ` * Named-argument metadata attached to PH7_OP_CALL instructions via p3.` |
|         - | 3471 | ` * Also carries the namespace-qualification flag formerly stored as p3=(void*)1.` |
|         - | 3472 | ` */` |
|         - | 3473 | `struct VmCallArgMap` |
|         - | 3474 | `{` |
|         - | 3475 | `	sxu8 bHasNamed;      /* 1 if any argument uses name: syntax */` |
|         - | 3476 | `	sxu8 bFromUnpack;    /* 1 when this is the EFFECTIVE map an argument UNPACK` |
|         - | 3477 | `	                      * produced (VmBuildEffectiveArgMap). php words its` |
|         - | 3478 | `	                      * positional-after-named refusal with a trailing` |
|         - | 3479 | ``	                      * ` during unpacking` only there; the same rule broken by`` |
|         - | 3480 | `	                      * call_user_func_array's array gets the bare sentence. */` |
|         - | 3481 | `	sxu8 bIsNamespaced;  /* 1 if compiler namespace-qualified the call */` |
|         - | 3482 | `	sxu8 bNotFrameless;  /* 1 for a namespaced call written in the ARGUMENTS of another` |
|         - | 3483 | ``	                      * namespaced call php makes frameless (`trim(implode(...))`):`` |
|         - | 3484 | `	                      * php's compiler never makes the inner one frameless, to keep` |
|         - | 3485 | `	                      * the two-branch code from nesting (in_jmp_frameless_branch) */` |
|         - | 3486 | `	sxu8 bStrict;        /* 1 if the call site's file declared strict_types=1 */` |
|         - | 3487 | `	sxu32 nOrigNameLit;  /* Original (unqualified) name-literal index + 1, stored` |
|         - | 3488 | `						  * when the CALL handler namespace-qualified the name so` |
|         - | 3489 | `						  * a following NEW can re-qualify with CLASS imports.` |
|         - | 3490 | `						  * 0 = unset. (Formerly abused OP_CALL's iP2, colliding` |
|         - | 3491 | ``						  * with the hasSpread flag: `new N\C(...$args)`.) */`` |
|         - | 3492 | `	sxu32 nNewClassInstr;/* Instruction index + 1 of the class-name push, for a call node` |
|         - | 3493 | ``	                      * that is a `new`'s operand. The reorder puts that push before`` |
|         - | 3494 | `	                      * the constructor arguments, so the NEW codegen can no longer` |
|         - | 3495 | `	                      * find it by peeking one instruction back. 0 = unset. */` |
|         - | 3496 | `	sxu8 bArgShapes;     /* 1 when the two masks below describe THIS call's argument` |
|         - | 3497 | `						  * positions. The compiler sets it for every call whose actual` |
|         - | 3498 | `						  * positions survive to the runtime stack unchanged — i.e. no` |
|         - | 3499 | `						  * spread and at most 31 arguments. 0 means "unknown shapes":` |
|         - | 3500 | `						  * the by-ref binders fall back to their runtime nIdx test. */` |
|         - | 3501 | `	sxu32 nNonLvalMask;  /* bit N: argument N is a HARD non-lvalue (a literal, an operator` |
|         - | 3502 | ``						  * result, a cast, a class constant, `@$x`, `$o?->p`, an assignment`` |
|         - | 3503 | ``						  * — php's `zend_is_variable` says no and it is not a call either).`` |
|         - | 3504 | `						  * Binding one to a by-ref parameter is php's catchable` |
|         - | 3505 | ``						  * `Argument #N ($p) could not be passed by reference` Error, raised`` |
|         - | 3506 | `						  * at the CALL before the callee's ZPP runs. */` |
|         - | 3507 | ``	sxu32 nTempCallMask; /* bit N: argument N is the RESULT of a call or a `new` — php's`` |
|         - | 3508 | `						  * SEND_VAR_NO_REF: an E_NOTICE ("Only variables should be passed` |
|         - | 3509 | `						  * by reference") and then it operates on the temporary. */` |
|         - | 3510 | `	sxu32 nConstStrMask; /* bit N: argument N is a string LITERAL with nothing interpolated` |
|         - | 3511 | `						  * -- php's ZEND_AST_ZVAL string, which its compiler can read. Its` |
|         - | 3512 | `						  * sprintf() fold keys on the format being one (see bFoldedCallee). */` |
|         - | 3513 | `	sxu32 nTotal;        /* Total number of compile-time arguments */` |
|         - | 3514 | `	SyString *aNames;    /* Array of nTotal names. nByte==0 means positional. */` |
|         - | 3515 | `	const sxu32 *aRun;   /* Effective (unpack) maps only, one per slot like aNames: which` |
|         - | 3516 | `						  * argument of the call as WRITTEN the slot came from. php forgets` |
|         - | 3517 | `						  * a named argument at the end of each unpack, so a positional one` |
|         - | 3518 | `						  * after it is refused only inside the SAME unpack; a later unpack` |
|         - | 3519 | `						  * binds it after the highest parameter bound so far. 0 = the list` |
|         - | 3520 | `						  * is one run (call_user_func_array's array, a compile map). */` |
|         - | 3521 | ``	SyString sNewAnon;   /* An anonymous class's `new class(...)` only: the synthesized`` |
|         - | 3522 | `						  * name of the class it instantiates. Its list is compiled from` |
|         - | 3523 | `						  * raw tokens with that name pushed AFTER the arguments, so a` |
|         - | 3524 | `						  * named argument's send-time screen (PH7_ROT_ANON) finds the` |
|         - | 3525 | `						  * class here instead of below the argument region. {0,0}` |
|         - | 3526 | `						  * everywhere else; bytes live in the map's own tail. */` |
|         - | 3527 | `	SyString sAssertSrc; /* Direct assert() calls only: the first argument's rendered` |
|         - | 3528 | ``						  * source text (php's zend_ast_export shape, e.g. `1 == 2`),`` |
|         - | 3529 | `						  * captured at compile time so a failing assertion reports` |
|         - | 3530 | ``						  * `assert(1 == 2)` like php instead of the evaluated value.`` |
|         - | 3531 | `						  * {0,0} for every other call site; bytes live in the VM` |
|         - | 3532 | `						  * allocator. See PH7_GenRenderAssertSpan (compile_literal.c). */` |
|         - | 3533 | `};` |
|         - | 3534 | `/*` |
|         - | 3535 | ` * A class declaration whose parent/interface/trait could not be resolved at` |
|         - | 3536 | ` * compile time (the enclosing file's statements — spl_autoload_register — had` |
|         - | 3537 | ` * not executed yet). The compiler captures the declaration's SOURCE plus a` |
|         - | 3538 | ` * reconstructed namespace/use-import prefix and defers the whole compile to` |
|         - | 3539 | ` * OP_CLASS_DEFER at the declaration's execution point (VmExecDeferredClass,` |
|         - | 3540 | ` * vm_include.c), where the autoloader is live. aRequired lists the names that` |
|         - | 3541 | ` * were missing; each still-missing one throws php's catchable` |
|         - | 3542 | `` * `Class/Interface/Trait "X" not found` Error before the re-compile runs.`` |
|         - | 3543 | ` */` |
|         - | 3544 | `typedef struct VmDeferredReq VmDeferredReq;` |
|         - | 3545 | `struct VmDeferredReq` |
|         - | 3546 | `{` |
|         - | 3547 | `	SyString sName;  /* Fully-qualified name (allocator-owned) */` |
|         - | 3548 | `	sxu8 cKind;      /* PH7_DEFER_KIND_* — picks the not-found noun */` |
|         - | 3549 | `};` |
|         - | 3550 | `#define PH7_DEFER_KIND_CLASS     0` |
|         - | 3551 | `#define PH7_DEFER_KIND_INTERFACE 1` |
|         - | 3552 | `#define PH7_DEFER_KIND_TRAIT     2` |
|         - | 3553 | `typedef struct VmDeferredClass VmDeferredClass;` |
|         - | 3554 | `struct VmDeferredClass` |
|         - | 3555 | `{` |
|         - | 3556 | `	SyString sText;     /* Re-compilable chunk: namespace + use-imports prefix +` |
|         - | 3557 | ``						 * the declaration source (anon: wrapped in `if (false) { new ... }`) */`` |
|         - | 3558 | `	SyString sSelfName; /* FQN the compile must install (anon: the synthesized name) —` |
|         - | 3559 | `						 * the post-eval existence check */` |
|         - | 3560 | `	SyString sAnonName; /* Synthesized anonymous-class name to inject via` |
|         - | 3561 | `						 * pVm->sDeferAnonName ({0,0} for a named declaration) */` |
|         - | 3562 | `	SySet aRequired;    /* VmDeferredReq — names unresolved at compile time */` |
|         - | 3563 | `	sxu32 nLine;        /* Declaration line (diagnostics) */` |
|         - | 3564 | `	sxu8 bDone;         /* 1 once the declaration executed successfully (idempotent site) */` |
|         - | 3565 | `	sxu8 bChecked;      /* 1 when the file's compile already ran the body's compile-time` |
|         - | 3566 | `						 * refusals (bDeclCheck), so the re-compile prints no warning again */` |
|         - | 3567 | `};` |
|         - | 3568 | `/*` |
|         - | 3569 | ` * PH7_OP_CLASS_OBLIGE's p3: the variance pairs a class's compile-time link could` |
|         - | 3570 | ` * not decide because a class they name was not loaded -- an interface's get-only` |
|         - | 3571 | ` * property, a method parameter or return. php leaves such a class undeclared until` |
|         - | 3572 | ` * its statement runs, autoloads every name the checks left unresolved, and checks` |
|         - | 3573 | ` * again; a pair still open then is refused (OoSettleObligations, oo.c).` |
|         - | 3574 | ` */` |
|         - | 3575 | `typedef struct VmClassOblige VmClassOblige;` |
|         - | 3576 | `struct VmClassOblige` |
|         - | 3577 | `{` |
|         - | 3578 | `	ph7_class *pBase;   /* the class whose member is the PARENT side, as the check got it */` |
|         - | 3579 | `	ph7_class *pSub;    /* ...and the class being linked */` |
|         - | 3580 | `	void *pParent;      /* ph7_class_method or ph7_class_attr */` |
|         - | 3581 | `	void *pChild;` |
|         - | 3582 | `	sxu8 bProp;         /* 1: a property pair (OoCheckPropRedeclare), 0: a method's */` |
|         - | 3583 | `	sxu8 bCtorExempt;   /* the method check's own flag */` |
|         - | 3584 | `};` |
|         - | 3585 | `typedef struct VmClassObligeSet VmClassObligeSet;` |
|         - | 3586 | `struct VmClassObligeSet` |
|         - | 3587 | `{` |
|         - | 3588 | `	SySet aOblige;      /* VmClassOblige, in link order */` |
|         - | 3589 | `	SySet aName;        /* SyString: the unresolved class names, in php's lookup order */` |
|         - | 3590 | `	sxu8 bDone;         /* settled once; the statement can run again (a loop, an include) */` |
|         - | 3591 | `};` |
|         - | 3592 | `/*` |
|         - | 3593 | `` * PH7_OP_CONST_DECL's p3: what a global `const` statement declares once its`` |
|         - | 3594 | ` * initializer -- compiled inline just before it -- has left the value on the stack.` |
|         - | 3595 | ` */` |
|         - | 3596 | `typedef struct VmConstDecl VmConstDecl;` |
|         - | 3597 | `struct VmConstDecl` |
|         - | 3598 | `{` |
|         - | 3599 | `	SyString sName;  /* Namespace-qualified name as written (VM-lifetime buffer) */` |
|         - | 3600 | `	SyString sFile;  /* Declaring file */` |
|         - | 3601 | ``	sxu32 nLine;     /* The `const` keyword's line: php blames every name in the list on it */`` |
|         - | 3602 | `	SySet aAttrs;    /* #[...] groups (ph7_attribute), handed to the constant record */` |
|         - | 3603 | `};` |
|         - | 3604 | `/* Each active class instance attribute is represented by an instance` |
|         - | 3605 | ` * of the following structure.` |
|         - | 3606 | ` */` |
|         - | 3607 | `typedef struct VmClassAttr VmClassAttr;` |
|         - | 3608 | `/*` |
|         - | 3609 | ` * One property SLOT on one object -- the engine's per-instance record, and the` |
|         - | 3610 | ` * engine's single most numerous heap object after the array node: 128,024 of them` |
|         - | 3611 | ` * live at the peak of the ecosystem gate's phpcs step.` |
|         - | 3612 | ` *` |
|         - | 3613 | ` * It carries ONE holder pointer, not two. It used to name both the instance the` |
|         - | 3614 | ` * slot belongs to and the class that owns the property, and the second was` |
|         - | 3615 | ` * derivable from the first at every site that ever set it: an instance record is` |
|         - | 3616 | `` * built by PH7_VmCreateClassInstanceFrame, whose `pClass` IS `pObj->pClass`, and`` |
|         - | 3617 | `` * the two dynamic-property doors write `pThis->pClass` beside `pThis`. The only`` |
|         - | 3618 | ` * records that are not an instance's are the class's own statics, which have no` |
|         - | 3619 | ` * instance at all -- so one pointer says both, and VM_CLASS_ATTR_CLASSHELD says` |
|         - | 3620 | ` * which kind it is.` |
|         - | 3621 | ` *` |
|         - | 3622 | ` * That is not a tidy-up, it is a bucket. The struct was 32 bytes, and a pool` |
|         - | 3623 | ` * request of 32 needs 40 with its header and so lands in the 64-byte bucket --` |
|         - | 3624 | ` * 7.81 MB at peak against 3.91 MB asked for, the census's only row of 100%` |
|         - | 3625 | ` * rounding waste. At 24 it needs 32 exactly and lands in the 32-byte bucket.` |
|         - | 3626 | ` * KEEP IT AT 24 BYTES: one more field of any size doubles this row again.` |
|         - | 3627 | ` *` |
|         - | 3628 | ` * Read it through PH7_VmAttrOwner / PH7_VmAttrInst below; nothing outside them` |
|         - | 3629 | ` * should touch pHolder.` |
|         - | 3630 | ` */` |
|         - | 3631 | `struct VmClassAttr` |
|         - | 3632 | `{` |
|         - | 3633 | `	ph7_class_attr *pAttr; /* Class attribute */` |
|         - | 3634 | `	void *pHolder;         /* The ph7_class_instance this slot belongs to, or -- when` |
|         - | 3635 | `	                        * VM_CLASS_ATTR_CLASSHELD is set -- the ph7_class whose own` |
|         - | 3636 | `	                        * static it is. The store filter reaches the instance for` |
|         - | 3637 | `	                        * ph7_class::xSet, which is a handler ON AN OBJECT (php's` |
|         - | 3638 | `	                        * write_property takes the object); DateInterval's writes its` |
|         - | 3639 | `	                        * own microsecond slot from there. The record lives in the` |
|         - | 3640 | `	                        * holder's hAttr and dies with it, so this never outlives` |
|         - | 3641 | `	                        * what it names. */` |
|         - | 3642 | `	sxu32 nIdx;            /* Memory object index */` |
|         - | 3643 | `	sxi32 iState;          /* Per-instance state: VM_CLASS_ATTR_UNINIT */` |
|         - | 3644 | `};` |
|         - | 3645 | `#define VM_CLASS_ATTR_UNINIT  0x01 /* Typed property never written (PHP 7.4+); also the` |
|         - | 3646 | `                                    * write-once latch for readonly properties (cleared on` |
|         - | 3647 | `                                    * the first successful write — see VmEnforcePropertyTypeOnStore) */` |
|         - | 3648 | `#define VM_CLASS_ATTR_RDONLY  0x08 /* php's read-only handler property, marked on the INSTANCE:` |
|         - | 3649 | ``                                    * a plain store and an unset() refuse with `Property p is`` |
|         - | 3650 | ``                                    * read only` while every path that takes a POINTER to it`` |
|         - | 3651 | ``                                    * goes through (a compound assign, `++`, `??=`, a`` |
|         - | 3652 | `                                    * reference bind). It is per-OBJECT rather than per-class` |
|         - | 3653 | `                                    * because php's own handler is: a PDOStatement nobody` |
|         - | 3654 | `                                    * built a cursor for takes the write, and only one` |
|         - | 3655 | `                                    * carrying a statement refuses. */` |
|         - | 3656 | `#define VM_CLASS_ATTR_UNSEEN  0x10 /* Per-INSTANCE presentation hide: the slot reads, writes and` |
|         - | 3657 | `                                    * answers isset() the way it always did, and every surface` |
|         - | 3658 | `                                    * that SHOWS an object -- var_dump/print_r/var_export, the` |
|         - | 3659 | `                                    * (array) cast, get_object_vars, foreach, json_encode,` |
|         - | 3660 | `                                    * serialize, http_build_query and Reflection's object dump` |
|         - | 3661 | `                                    * -- walks past it. php's from-string DateInterval is the` |
|         - | 3662 | ``                                    * case: it answers `$i->d` from the string it kept while`` |
|         - | 3663 | ``                                    * presenting `from_string` and `date_string` alone. */`` |
|         - | 3664 | `#define VM_CLASS_ATTR_TYPE_DEFER 0x04 /* Typed STATIC property whose eagerly-evaluated DEFAULT failed` |
|         - | 3665 | `                                       * its type check at class mount. php evaluates static defaults` |
|         - | 3666 | `                                       * lazily, so the failure is deferred: any static-property access` |
|         - | 3667 | `                                       * on the class (read/write/isset, any property) and any` |
|         - | 3668 | `                                       * instantiation throws the catchable "Cannot assign <kind> to` |
|         - | 3669 | `                                       * property C::$s of type T" TypeError; a never-touched class` |
|         - | 3670 | `                                       * stays silent. Raised by PH7_VmMaterializeClassStatics, which` |
|         - | 3671 | `                                       * also sets the flag when a DEFERRED default (the sibling` |
|         - | 3672 | `                                       * PH7_CLASS_ATTR_STATIC_DEFER) evaluates at first access and` |
|         - | 3673 | `                                       * only then fails its type check. */` |
|         - | 3674 | ``#define VM_CLASS_ATTR_REFBOUND 0x02 /* Property is bound to a reference (`$o->p =& $x`): its nIdx`` |
|         - | 3675 | `                                    * slot is SHARED with (and pinned by) the source variable, so` |
|         - | 3676 | `                                    * PH7_VmReleaseInstanceAttr must NOT release/recycle it — the` |
|         - | 3677 | `                                    * surviving alias would dangle. Mirrors the use(&$x) pin. */` |
|         - | 3678 | `#define VM_CLASS_ATTR_CLASSHELD 0x40 /* pHolder is the ph7_class whose STATIC this slot is,` |
|         - | 3679 | `                                     * not a ph7_class_instance. A static belongs to the class` |
|         - | 3680 | `                                     * and is filed in every instance's hAttr as well, so the` |
|         - | 3681 | `                                     * record has no instance behind it -- which is exactly` |
|         - | 3682 | `                                     * what "no instance" used to be spelled as a NULL pInst` |
|         - | 3683 | `                                     * beside a non-NULL pOwner. */` |
|         - | 3684 | ``#define VM_CLASS_ATTR_REFSRCPIN 0x20 /* Property is the SOURCE of a reference (`$r =& $o->p`,`` |
|         - | 3685 | ``                                     * `$q->p =& $o->p`, `foreach ($o->p as &$v)`): php makes both`` |
|         - | 3686 | `                                     * ends references, and the other end pins the slot. A property` |
|         - | 3687 | `                                     * is not a holder the reference table can NAME, so the slot's` |
|         - | 3688 | `                                     * only recorded holder was that pin -- and when the other end` |
|         - | 3689 | `                                     * died, the unpin freed the value out from under THIS property,` |
|         - | 3690 | `                                     * which then read NULL. The bit says this property holds one` |
|         - | 3691 | `                                     * counted pin of its own, given back when it is released. */` |
|         - | 3692 | ` /*` |
|         - | 3693 | ` * The two questions the one holder pointer answers.` |
|         - | 3694 | ` *` |
|         - | 3695 | ` * PH7_VmAttrInst -- the object this slot sits on, or 0 for a class static, which is` |
|         - | 3696 | ` * the NULL pInst every caller used to test for.` |
|         - | 3697 | ` * PH7_VmAttrOwner -- the class that owns the property, for the diagnostics in` |
|         - | 3698 | ` * vm_error.c. For an instance record that is the instance's own class, which is what` |
|         - | 3699 | `` * the frame builder wrote there by hand (its `pClass` is `pObj->pClass`); for a`` |
|         - | 3700 | ` * static it is the holder itself.` |
|         - | 3701 | ` */` |
|   2408340 | 3702 | `SX_STATIC_INLINE ph7_class_instance * PH7_VmAttrInst(const VmClassAttr *pVmAttr)` |
|         5 | 3703 | `{` |
|   2408345 | 3704 | `	if( pVmAttr->iState & VM_CLASS_ATTR_CLASSHELD ){` |
|       151 | 3705 | `		return 0;` |
|         - | 3706 | `	}` |
|   2408195 | 3707 | `	return (ph7_class_instance *)pVmAttr->pHolder;` |
|   1204177 | 3708 | `}` |
|    201606 | 3709 | `SX_STATIC_INLINE ph7_class * PH7_VmAttrOwner(const VmClassAttr *pVmAttr)` |
|         5 | 3710 | `{` |
|    201611 | 3711 | `	if( pVmAttr->iState & VM_CLASS_ATTR_CLASSHELD ){` |
|        69 | 3712 | `		return (ph7_class *)pVmAttr->pHolder;` |
|         - | 3713 | `	}` |
|    201547 | 3714 | `	return pVmAttr->pHolder ? ((ph7_class_instance *)pVmAttr->pHolder)->pClass : 0;` |
|    100808 | 3715 | `}` |
|         - | 3716 | `/* The two ways a record is filed. Both leave iState's other bits alone, so a caller` |
|         - | 3717 | ` * may set its state before or after saying who holds the slot. */` |
|  10826451 | 3718 | `SX_STATIC_INLINE void PH7_VmAttrSetInst(VmClassAttr *pVmAttr,ph7_class_instance *pInst)` |
|         5 | 3719 | `{` |
|  10826456 | 3720 | `	pVmAttr->pHolder = (void *)pInst;` |
|  10826456 | 3721 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_CLASSHELD;` |
|  10826456 | 3722 | `}` |
|      1486 | 3723 | `SX_STATIC_INLINE void PH7_VmAttrSetClass(VmClassAttr *pVmAttr,ph7_class *pClass)` |
|         5 | 3724 | `{` |
|      1491 | 3725 | `	pVmAttr->pHolder = (void *)pClass;` |
|      1491 | 3726 | `	pVmAttr->iState \|= VM_CLASS_ATTR_CLASSHELD;` |
|      1491 | 3727 | `}` |
|         - | 3728 | `/* Forward reference */` |
|         - | 3729 | `typedef struct VmSlot VmSlot;` |
|         - | 3730 | `struct VmSlot` |
|         - | 3731 | `{` |
|         - | 3732 | `	sxu32 nIdx;      /* Index in pVm->aMemObj[] */` |
|         - | 3733 | `	void *pUserData; /* Upper-layer private data */` |
|         - | 3734 | `};` |
|         - | 3735 | `/*` |
|         - | 3736 | ` * The segmented memory-object table.` |
|         - | 3737 | ` *` |
|         - | 3738 | ` * aMemObj used to be one doubling SySet: every value pointer died on any growth,` |
|         - | 3739 | ` * the doubling realloc moved a whole 33 MB block, and the buffer never shrank.` |
|         - | 3740 | ` * Here the value slots live in FIXED-SIZE segments (VM_MEMPOOL_SEG_SLOTS each,` |
|         - | 3741 | ` * one pool allocation per segment), addressed by the same flat nIdx the engine` |
|         - | 3742 | ` * has always carried -- the split is a shift and a mask here, in the accessor,` |
|         - | 3743 | ` * so every caller's index means what it always meant. A value's address never` |
|         - | 3744 | ` * moves, growth appends a segment instead of copying the table, and a fully-free` |
|         - | 3745 | ` * trailing segment is handed back on truncate.` |
|         - | 3746 | ` *` |
|         - | 3747 | ` * SEGMENT SIZE is a floor, not just a granularity: the first segment is` |
|         - | 3748 | ` * allocated when the VM is, so every VM pays for one whether it holds three` |
|         - | 3749 | ` * values or three hundred thousand. 256 slots is 16 KB, which is what the` |
|         - | 3750 | ` * SySetAlloc(&pVm->aMemObj,0xFF) this replaced opened with -- deliberately, so` |
|         - | 3751 | ` * that segmenting the table did not raise the per-VM floor. It matters in two` |
|         - | 3752 | ` * places that are not this box: the -S server caches PHL_VM_CACHE_SIZE (16) VMs,` |
|         - | 3753 | ` * so the floor is paid sixteen times, and on ESP32-S3 internal RAM dips to 32 KB` |
|         - | 3754 | ` * free, where a 256 KB opening allocation is not a cost but a failure.` |
|         - | 3755 | ` * The price of a small segment is one direct block and one segment-table entry` |
|         - | 3756 | ` * per 256 slots: at the phpcs peak of record (~356K slots) that is ~1,400` |
|         - | 3757 | ` * segments, ~33 KB of allocator headers and a 2,048-entry pointer table -- under` |
|         - | 3758 | ` * 0.04% of the peak. Override with -DPH7_VM_MEMPOOL_SEG_SHIFT=n for a target` |
|         - | 3759 | ` * that wants a different trade; nothing but the two constants below depends on it.` |
|         - | 3760 | ` *` |
|         - | 3761 | ` * Freed slots form a single INTRUSIVE free list threaded through the slots'` |
|         - | 3762 | ` * own (dead) nIdx word -- VmMemPoolFreeSlot writes the link into the slot, so` |
|         - | 3763 | ` * slot reuse is O(1) and costs zero extra memory. nFreeHead is the head, or` |
|         - | 3764 | ` * SXU32_HIGH when the list is empty. This replaced the aFreeObj SySet, which` |
|         - | 3765 | ` * grew one 16-byte VmSlot per free index to describe exactly what the link now` |
|         - | 3766 | ` * describes for free. Because the link lives INSIDE the slot, a slot on the` |
|         - | 3767 | ` * list carries MEMOBJ_POOLFREE: a second free of the same index would otherwise` |
|         - | 3768 | ` * write the head into the slot the head already points at, and every later` |
|         - | 3769 | ` * reserve would hand out that one slot forever. The old stack merely handed the` |
|         - | 3770 | ` * index out twice and drained; this one would not.` |
|         - | 3771 | ` */` |
|         - | 3772 | `#ifndef PH7_VM_MEMPOOL_SEG_SHIFT` |
|         - | 3773 | `#define PH7_VM_MEMPOOL_SEG_SHIFT 8` |
|         - | 3774 | `#endif` |
|         - | 3775 | `#define VM_MEMPOOL_SEG_SHIFT  PH7_VM_MEMPOOL_SEG_SHIFT` |
|         - | 3776 | `#define VM_MEMPOOL_SEG_SLOTS  (1u << VM_MEMPOOL_SEG_SHIFT)` |
|         - | 3777 | `#define VM_MEMPOOL_SEG_MASK   (VM_MEMPOOL_SEG_SLOTS - 1u)` |
|         - | 3778 | `typedef struct VmMemPool VmMemPool;` |
|         - | 3779 | `struct VmMemPool` |
|         - | 3780 | `{` |
|         - | 3781 | `	SyMemBackend *pAllocator; /* Memory backend the segments come from */` |
|         - | 3782 | `	ph7_value   **apSeg;      /* Segment pointer table (VM_MEMPOOL_SEG_SLOTS slots each) */` |
|         - | 3783 | `	sxu32         nSeg;       /* Segments currently allocated */` |
|         - | 3784 | `	sxu32         nCap;       /* Capacity of apSeg */` |
|         - | 3785 | `	sxu32         nUsed;      /* Logical slots in use -- SySetUsed(aMemObj) semantics */` |
|         - | 3786 | `	sxu32         nFreeHead;  /* First free slot, or SXU32_HIGH; freed slots chain through their nIdx */` |
|         - | 3787 | `};` |
|         - | 3788 | `/*` |
|         - | 3789 | ` * The nIdx'th slot of the pool, or NULL when the index is past the end -- the` |
|         - | 3790 | ` * same bounds contract SySetAt kept on the set this replaces, so a call site` |
|         - | 3791 | ` * that leaned on NULL for "that index has not been allocated" still works.` |
|         - | 3792 | ` * INLINE because this is the engine's hottest read: every array element,` |
|         - | 3793 | ` * property and variable is reached through it -- which is also why the shift and` |
|         - | 3794 | ` * the mask are the COMPILE-TIME constants and not fields of the pool. They can` |
|         - | 3795 | ` * only ever hold these two values, and reading them out of the struct would put` |
|         - | 3796 | ` * two loads and a variable shift on every value access to say what an immediate` |
|         - | 3797 | ` * already says.` |
|         - | 3798 | ` */` |
| 250622418 | 3799 | `SX_STATIC_INLINE ph7_value * PH7_MemObjAt(VmMemPool *pPool,sxu32 nIdx)` |
|         5 | 3800 | `{` |
| 250622423 | 3801 | `	if( nIdx >= pPool->nUsed ){` |
|       169 | 3802 | `		return 0;   /* Out of range */` |
|         - | 3803 | `	}` |
| 250622259 | 3804 | `	return &pPool->apSeg[nIdx >> VM_MEMPOOL_SEG_SHIFT][nIdx & VM_MEMPOOL_SEG_MASK];` |
| 125317929 | 3805 | `}` |
|         - | 3806 | `/*` |
|         - | 3807 | ` * Cycle-collector colours (vm_gc.c). php's, and Bacon & Rajan's before it.` |
|         - | 3808 | ` * BLACK is "in use", GREY "being trial-deleted", WHITE "counted zero",` |
|         - | 3809 | ` * PURPLE "buffered as a possible root", DEAD "proved garbage, being freed".` |
|         - | 3810 | ` * A container is born BLACK because its struct is zeroed.` |
|         - | 3811 | ` */` |
|         - | 3812 | `#define PH7_GC_BLACK   0` |
|         - | 3813 | `#define PH7_GC_GREY    1` |
|         - | 3814 | `#define PH7_GC_WHITE   2` |
|         - | 3815 | `#define PH7_GC_PURPLE  3` |
|         - | 3816 | `#define PH7_GC_DEAD    4` |
|         - | 3817 | `typedef struct VmGcRef VmGcRef;` |
|         - | 3818 | `/* One container, in the root buffer or in a traversal worklist. */` |
|         - | 3819 | `struct VmGcRef` |
|         - | 3820 | `{` |
|         - | 3821 | `	void *pPtr;  /* ph7_hashmap * or ph7_class_instance *; 0 once the row is spent */` |
|         - | 3822 | `	sxu8 bMap;   /* which of the two it is */` |
|         - | 3823 | `};` |
|         - | 3824 | `typedef struct VmRefObj VmRefObj;` |
|         - | 3825 | `typedef struct VmRefSpill VmRefSpill;` |
|         - | 3826 | `/*` |
|         - | 3827 | ` * The SECOND and later holder of each kind. Allocated only for a slot that really` |
|         - | 3828 | `` * has two names on it, or two array nodes -- which is what a PHP `&` reference is,`` |
|         - | 3829 | ` * and which almost no slot is: an ordinary variable is named once and an ordinary` |
|         - | 3830 | ` * array element is pointed at by one node. Every slot used to carry both of these` |
|         - | 3831 | ` * sets inline (80 bytes) plus the 32-byte buffer each grew on its first row, so the` |
|         - | 3832 | ` * engine paid a reference's price for every variable and every element it created.` |
|         - | 3833 | ` */` |
|         - | 3834 | `struct VmRefSpill` |
|         - | 3835 | `{` |
|         - | 3836 | `	SySet aReference;  /* Holders beyond pEntry0 */` |
|         - | 3837 | `	SySet aArrEntries; /* Holders beyond pNode0 */` |
|         - | 3838 | `};` |
|         - | 3839 | `/* Reference-object body (vm.c reference machinery; shared with vm_builtin_var.c's unset).` |
|         - | 3840 | ` *` |
|         - | 3841 | ` * A record is the FALLBACK shape, not the ordinary one: apRefObj[nIdx] is a tagged` |
|         - | 3842 | ` * WORD (see VM_REF_TAG_* below) and only a slot whose answer will not fit in one` |
|         - | 3843 | ` * ever allocates this. */` |
|         - | 3844 | `struct VmRefObj` |
|         - | 3845 | `{` |
|         - | 3846 | `	SyHashEntry *pEntry0;     /* The one name bound to this slot; 0 once it is gone */` |
|         - | 3847 | `	ph7_hashmap_node *pNode0; /* The one array node pointing here; 0 once it is gone */` |
|         - | 3848 | `	VmRefSpill *pSpill;       /* The 2nd..nth holder of either kind; 0 while there is none */` |
|         - | 3849 | `	sxu32 nIdx;        /* Referenced object index -- also this record's cell in apRefObj[] */` |
|         - | 3850 | `	sxu32 nPin;        /* Holders the table cannot name, COUNTED so the last one to go` |
|         - | 3851 | `	                    * can release the slot: reference-bound properties (one per` |
|         - | 3852 | `	                    * binding, dropped when the property is released or re-bound).` |
|         - | 3853 | `	                    * A slot pinned by a site that never unpins (a use(&$x) capture,` |
|         - | 3854 | `	                    * a static, an enum case) leaves this 0 and relies on the` |
|         - | 3855 | `	                    * VM_REF_IDX_KEEP flag alone, which is a permanent pin. */` |
|         - | 3856 | `	sxi32 iFlags;      /* Configuration flags */` |
|         - | 3857 | `};` |
|         - | 3858 | `#define VM_REF_IDX_KEEP  0x001 /* Do not restore the memory object to the free list */` |
|         - | 3859 | `/*` |
|         - | 3860 | ` * apRefObj[nIdx] is ONE TAGGED WORD, not a pointer to a record.` |
|         - | 3861 | ` *` |
|         - | 3862 | ` * What the reference table has to say about the ordinary slot is one sentence long --` |
|         - | 3863 | ` * "this name holds it", "this array node points at it", "the object that declares it` |
|         - | 3864 | ` * holds it" -- and a whole heap record to say it is the engine's single largest` |
|         - | 3865 | ` * per-value cost. The word says the sentence itself; a record is allocated only when` |
|         - | 3866 | ` * the answer needs more than one holder, which a census of the ecosystem gate's phpcs` |
|         - | 3867 | ` * step puts at 14 of the 360264 records live at peak.` |
|         - | 3868 | ` *` |
|         - | 3869 | ` *   0                          nothing has ever been registered against the slot` |
|         - | 3870 | ` *   pEntry \| VM_REF_TAG_NAME   exactly ONE holder: the name bound to the slot` |
|         - | 3871 | ` *   pNode  \| VM_REF_TAG_NODE   exactly ONE holder: the array node pointing at it` |
|         - | 3872 | ` *   bits   \| VM_REF_TAG_MARK   registered, NO NAMED holder; the rest of the word is` |
|         - | 3873 | ` *                              the pin count and the flags (this is both the spent` |
|         - | 3874 | ` *                              record a dropped holder leaves behind -- which is what` |
|         - | 3875 | ` *                              still returns the slot to the free pool -- and the` |
|         - | 3876 | ` *                              declared property's own VM_REF_IDX_KEEP)` |
|         - | 3877 | ` *   pRef   (tag 0, non-zero)   a VmRefObj *: two or more holders, or a pin beside one` |
|         - | 3878 | ` *` |
|         - | 3879 | ` * The two pointer tags ride in the low bits of a pool-allocated address; the allocator` |
|         - | 3880 | ` * keeps every chunk 8-aligned (see the alignment note on sxmem.c's OS methods), and a` |
|         - | 3881 | ` * pointer that is not 4-aligned falls back to a record rather than being tagged.` |
|         - | 3882 | ` */` |
|         - | 3883 | `#define VM_REF_TAG_MASK    3` |
|         - | 3884 | `#define VM_REF_TAG_FULL    0  /* a VmRefObj * */` |
|         - | 3885 | `#define VM_REF_TAG_NAME    1  /* a SyHashEntry * */` |
|         - | 3886 | `#define VM_REF_TAG_NODE    2  /* a ph7_hashmap_node * */` |
|         - | 3887 | `#define VM_REF_TAG_MARK    3  /* no pointer: pin count and flags in the upper bits */` |
|         - | 3888 | `#define VM_REF_MARK_KEEP   0x4        /* bit 2 of a MARK word: VM_REF_IDX_KEEP */` |
|         - | 3889 | `#define VM_REF_MARK_PIN    0x8        /* bit 3 and up: the counted pin */` |
|         - | 3890 | `#define VM_REF_MARK_PINMAX 0x0FFFFFFF /* a pin count past this promotes to a record */` |
|         - | 3891 | `/* The tag of a word. Only the low two bits are read, so the cast may narrow. */` |
|         - | 3892 | `#define VM_REF_TAGOF(W)    (SX_PTR_TO_INT(W) & VM_REF_TAG_MASK)` |
|         - | 3893 | `/* VmObEntry struct moved to ph7int.h */` |
|         - | 3894 |  |
|         - | 3895 | `/*` |
|         - | 3896 | ` * Each catch [i.e catch(Exception $e){ } ] block is parsed out and stored` |
|         - | 3897 | ` * in an instance of the following structure.` |
|         - | 3898 | ` */` |
|         - | 3899 | `typedef struct ph7_exception_block ph7_exception_block;` |
|         - | 3900 | `typedef struct ph7_exception ph7_exception;` |
|         - | 3901 | `struct ph7_exception_block` |
|         - | 3902 | `{` |
|         - | 3903 | `	SySet aClasses;  /* Exception class names (SyString instances) for multi-catch */` |
|         - | 3904 | `	SyString sThis;  /* Instance name [i.e: $e..] */` |
|         - | 3905 | `	SySet *pByteCode;/* Compiled instructions of a DETACHED catch body (the path every` |
|         - | 3906 | `	                  * non-generator try takes; NULL for a ROOT C inline catch, which compiles` |
|         - | 3907 | `	                  * into the function's own array). Heap-allocated so its ADDRESS is stable:` |
|         - | 3908 | ``	                  * a `break`/`continue` inside the body records this container in its`` |
|         - | 3909 | `	                  * JumpFixup, resolved long after PH7_CompileCatch returned and after` |
|         - | 3910 | `	                  * sEntry grew (both would move an embedded SySet). */` |
|         - | 3911 | `	sxu32 iHandlerPc;/* ROOT C: inline PC where this catch body begins (0 = not inlined) */` |
|         - | 3912 | `};` |
|         - | 3913 | `/*` |
|         - | 3914 | ` * Context for the exception mechanism.` |
|         - | 3915 | ` */` |
|         - | 3916 | `struct ph7_exception` |
|         - | 3917 | `{` |
|         - | 3918 | `	ph7_vm *pVm;    /* VM that own this exception */` |
|         - | 3919 | `	SySet sEntry;   /* Compiled 'catch' blocks (ph7_exception_block instance)` |
|         - | 3920 | `				     * container.` |
|         - | 3921 | `					 */` |
|         - | 3922 | `	SySet sFinally; /* Compiled 'finally' block bytecode (legacy; unused once ROOT C inlining lands) */` |
|         - | 3923 | `	int iHasFinally;/* TRUE if a finally block was compiled */` |
|         - | 3924 | `	int iFinallyDone;/* TRUE if the finally block was already executed (legacy VmLocalExec path) */` |
|         - | 3925 | `	int iInlined;   /* ROOT C: TRUE when this try's catch/finally are inlined into the function` |
|         - | 3926 | `					 * bytecode (generator body). FALSE = legacy detached-mini-program path. */` |
|         - | 3927 | `	sxu32 iFinallyPc;/* ROOT C: inline PC where the finally body begins (0 = no finally) */` |
|         - | 3928 | `	sxu32 iEndCatchPc;/* ROOT C: inline PC just after the whole try/catch/finally (normal exit) */` |
|         - | 3929 | `	sxu32 iNextFinallyPc;/* ROOT C: iFinallyPc of the lexically-enclosing try-with-finally in the` |
|         - | 3930 | `					   * same function, or 0 — threads a return/break out through nested finallys */` |
|         - | 3931 | `	int iInCatch;   /* ROOT C: TRUE while a catch body of this try is running (finally still owed) */` |
|         - | 3932 | `	ph7_class_instance *pInflight;/* ROOT C: exception to bind at OP_CATCH / re-raise at END_FINALLY */` |
|         - | 3933 | `	VmFrame *pFrame; /* Frame that trigger the exception */` |
|         - | 3934 | `	sxu32 iLandingPc;/* Post-try landing pad (= OP_LOAD_EXCEPTION's iP2). Mirrors the` |
|         - | 3935 | `					  * exception frame's iExceptionJump but survives that frame's` |
|         - | 3936 | `					  * teardown, so an in-place catch can record where to resume. */` |
|         - | 3937 | `	void *pOwnerInstr;/* Bytecode array (VmInstr*) this try was compiled into. iLandingPc` |
|         - | 3938 | `					   * indexes THIS array; the resume only fires in the exec running it` |
|         - | 3939 | `					   * (distinguishes a mini-program from the body that shares its frame). */` |
|         - | 3940 | `` 	sxi32 iErrSuppress;/* '@' suppression depth at try entry. A throw from inside `@expr` `` |
|         - | 3941 | `	                    * unwinds past the ERR_CTRL that would have closed the window, so` |
|         - | 3942 | `	                    * the catch restores this snapshot instead of leaking the depth —` |
|         - | 3943 | ``	                    * and a try/catch nested INSIDE an `@` still stays suppressed. */`` |
|         - | 3944 | `	sxu32 nSelfDepth;/* pVm->aSelf depth when this try opened. php runs a catch and a` |
|         - | 3945 | `					   * finally in the scope of the body that DECLARED the try; this` |
|         - | 3946 | `					   * engine runs them at the THROW SITE, which can be several calls` |
|         - | 3947 | `					   * deeper, so the late-static-binding stack still carries the class` |
|         - | 3948 | `					   * of every call still open above the try. The handler parks that` |
|         - | 3949 | ``					   * slice back to this depth, and `static::` / `new static` /`` |
|         - | 3950 | `					   * get_called_class() inside it answer the try owner's called class` |
|         - | 3951 | `					   * the way php's do. */` |
|         - | 3952 | `	sxi32 iStackDepth;/* Operand-stack base (0-based TOS index = pTos-pStack, -1 when empty)` |
|         - | 3953 | `					   * captured when this try opened at OP_LOAD_EXCEPTION. Used only by` |
|         - | 3954 | `					   * Generator::throw() inject-at-yield to drain the abandoned` |
|         - | 3955 | `					   * (mid-expression) operand slots back to the try's base before` |
|         - | 3956 | `					   * landing at iLandingPc. */` |
|         - | 3957 | `	ph7_exception *pCompiled;/* BYTECODE stage 2b: NULL on the compiler-owned object; on a` |
|         - | 3958 | `					   * runtime ACTIVATION (clone pushed by OP_LOAD_EXCEPTION) this points` |
|         - | 3959 | `					   * at the compiled origin. Every activation of a lexical try carries` |
|         - | 3960 | `					   * its OWN mutable state (pFrame/iFinallyDone/iInCatch/pInflight/` |
|         - | 3961 | `					   * iStackDepth) — recursion levels no longer share one object, which` |
|         - | 3962 | `					   * ran every level's catch/finally against the deepest frame. */` |
|         - | 3963 | `};` |
|         - | 3964 | `/*` |
|         - | 3965 | `` * ROOT C: a pending non-local exit for an inline `finally` body. When control`` |
|         - | 3966 | ` * enters a finally (normal fall-through, a caught/unmatched throw, or a return/` |
|         - | 3967 | ` * break/continue crossing the try), one of these is pushed onto pVm->aFinallyAction;` |
|         - | 3968 | ` * the finally's terminating OP_END_FINALLY pops it and dispatches accordingly. A` |
|         - | 3969 | ` * return/break/continue crossing several nested finallys keeps ONE record on the` |
|         - | 3970 | ` * stack and re-drives it through each finally via ph7_exception.iNextFinallyPc.` |
|         - | 3971 | ` */` |
|         - | 3972 | `#define PH7_FA_FALLTHROUGH 0  /* Resume at iNextPc (post-construct landing) */` |
|         - | 3973 | `#define PH7_FA_RETHROW     1  /* Re-raise pExc after the finally runs */` |
|         - | 3974 | `#define PH7_FA_RETURN      2  /* Return sRet from pTargetBody after the finally chain */` |
|         - | 3975 | `#define PH7_FA_JMP         3  /* Break/continue: resume at iNextPc after the finally chain */` |
|         - | 3976 | `typedef struct VmFinallyAction VmFinallyAction;` |
|         - | 3977 | `struct VmFinallyAction` |
|         - | 3978 | `{` |
|         - | 3979 | `	int eKind;                    /* One of PH7_FA_* */` |
|         - | 3980 | `	sxu32 iNextPc;                /* FALLTHROUGH/JMP: pc (0-based) to resume at in this array */` |
|         - | 3981 | `	ph7_class_instance *pExc;     /* RETHROW: exception to re-raise (holds a ref) */` |
|         - | 3982 | `	ph7_value sRet;               /* RETURN: the value to return (owned) */` |
|         - | 3983 | ``	int bHasRetVal;               /* RETURN: TRUE if sRet holds a real value (vs bare `return;`) */`` |
|         - | 3984 | `	void *pTargetBody;            /* RETURN: VmFrame* the return materializes on */` |
|         - | 3985 | `	int nCross;                   /* trys still to cross through their finallys (-1 = unbounded,` |
|         - | 3986 | `	                               * for RETURN; a positive count bounds a break/continue to the` |
|         - | 3987 | `	                               * trys between it and its target loop) */` |
|         - | 3988 | `};` |
|         - | 3989 | `/* Forward reference */` |
|         - | 3990 | `typedef struct ph7_case_expr ph7_case_expr;` |
|         - | 3991 | `typedef struct ph7_switch ph7_switch;` |
|         - | 3992 | `/*` |
|         - | 3993 | ` * Each compiled case block in a swicth statement is compiled` |
|         - | 3994 | ` * and stored in an instance of the following structure.` |
|         - | 3995 | ` */` |
|         - | 3996 | `struct ph7_case_expr` |
|         - | 3997 | `{` |
|         - | 3998 | `	SySet aByteCode;   /* Compiled body of the case block */` |
|         - | 3999 | `	sxu32 nStart;      /* First instruction to execute */` |
|         - | 4000 | `};` |
|         - | 4001 | `/*` |
|         - | 4002 | ` * Each compiled switch statement is parsed out and stored` |
|         - | 4003 | ` * in an instance of the following structure.` |
|         - | 4004 | ` */` |
|         - | 4005 | `struct ph7_switch` |
|         - | 4006 | `{` |
|         - | 4007 | `	SySet aCaseExpr;  /* Compile case block */` |
|         - | 4008 | `	sxu32 nOut;       /* First instruction to execute after this statement */` |
|         - | 4009 | `	sxu32 nDefault;   /* First instruction to execute in the default block */` |
|         - | 4010 | `};` |
|         - | 4011 | `/*` |
|         - | 4012 | ` * Each arm of a PHP 8.0 match expression is compiled into` |
|         - | 4013 | ` * an instance of the following structure.` |
|         - | 4014 | ` */` |
|         - | 4015 | `typedef struct ph7_match_arm ph7_match_arm;` |
|         - | 4016 | `typedef struct ph7_match     ph7_match;` |
|         - | 4017 | `struct ph7_match_arm` |
|         - | 4018 | `{` |
|         - | 4019 | `	SySet aConds;   /* SySet of SySet (VmInstr) — one compiled bytecode block per condition value */` |
|         - | 4020 | `	SySet aResult;  /* Compiled bytecode of the arm's result expression */` |
|         - | 4021 | `	int   bDefault; /* 1 if this is the 'default' arm */` |
|         - | 4022 | `};` |
|         - | 4023 | `struct ph7_match` |
|         - | 4024 | `{` |
|         - | 4025 | `	SySet aArms;    /* SySet of ph7_match_arm */` |
|         - | 4026 | `};` |
|         - | 4027 | `/* Assertion flags */` |
|         - | 4028 | `#define PH7_ASSERT_DISABLE    0x01  /* Disable assertion */` |
|         - | 4029 | `#define PH7_ASSERT_WARNING    0x02  /* Deprecated in PHP 8: kept for constant compatibility only */` |
|         - | 4030 | `#define PH7_ASSERT_BAIL       0x04  /* Terminate execution on failed assertions */` |
|         - | 4031 | `#define PH7_ASSERT_QUIET_EVAL 0x08  /* Not used */` |
|         - | 4032 | `#define PH7_ASSERT_CALLBACK   0x10  /* Callback to call on failed assertions */` |
|         - | 4033 | `#define PH7_ASSERT_ZEND_OFF   0x20  /* zend.assertions < 1: assert() compiled out (php CLI default -1) */` |
|         - | 4034 | `/*` |
|         - | 4035 | ` * error_log() consumer function signature.` |
|         - | 4036 | ` * Refer to the [PH7_VM_CONFIG_ERR_LOG_HANDLER] configuration directive` |
|         - | 4037 | ` * for more information on how to register an error_log consumer().` |
|         - | 4038 | ` */` |
|         - | 4039 | `typedef void (*ProcErrLog)(const char *,int,const char *,const char *);` |
|         - | 4040 | `/*` |
|         - | 4041 | ` * An instance of the following structure hold the bytecode instructions` |
|         - | 4042 | ` * resulting from compiling a PHP script.` |
|         - | 4043 | ` * This structure contains the complete state of the virtual machine.` |
|         - | 4044 | ` */` |
|         - | 4045 | `/* In-flight magic-accessor guard entry (band A #3a; see vm.c helpers). */` |
|         - | 4046 | `typedef struct VmMagicGuard VmMagicGuard;` |
|         - | 4047 | `struct VmMagicGuard` |
|         - | 4048 | `{` |
|         - | 4049 | `	void *pThis;      /* instance identity */` |
|         - | 4050 | `	sxu32 nNameHash;  /* property-name hash (SyBinHash) */` |
|         - | 4051 | `	sxu8 cKind;       /* accessor kind: 'g' = __get */` |
|         - | 4052 | `};` |
|         - | 4053 | `/* Pending property write-back entry (PHP 8.4 hooks + magic ??=): a LIFO of` |
|         - | 4054 | ` * these (ph7_vm.aHookRmw) carries every write whose dispatch is deferred past` |
|         - | 4055 | ` * OP_MEMBER to a later opcode:` |
|         - | 4056 | ` *   VM_HOOK_PEND_RMW        — read-modify-write on a hooked property: OP_MEMBER` |
|         - | 4057 | ` *                             dispatched the get hook (or read the raw backing` |
|         - | 4058 | ` *                             store when set-only) into a fresh SCRATCH memobj` |
|         - | 4059 | ` *                             slot; the modify op (++/--/compound-assign)` |
|         - | 4060 | ` *                             mutates the scratch and its tail consumes the` |
|         - | 4061 | ` *                             entry (matched by kind + scratch index) to` |
|         - | 4062 | ` *                             dispatch the set hook with the computed value.` |
|         - | 4063 | `` *   VM_HOOK_PEND_COAL_HOOK  — `$o->p ??= v` on a hooked property: the entry is`` |
|         - | 4064 | ` *                             consumed by the OP_NULLC_STORE at nPc (matched by` |
|         - | 4065 | ` *                             owner + pc) to dispatch the set hook.` |
|         - | 4066 | `` *   VM_HOOK_PEND_COAL_MAGIC — `$o->p ??= v` on a missing property whose class`` |
|         - | 4067 | ` *                             declares __set: consumed the same way, dispatching` |
|         - | 4068 | ` *                             __set(sName, value).` |
|         - | 4069 | ` *   VM_HOOK_PEND_RMW_MAGIC  — read-modify-write on an OVERLOADED property` |
|         - | 4070 | `` *                             (`$o->n++`, `$o->n .= 'x'`): the same scratch-slot`` |
|         - | 4071 | ` *                             rail as VM_HOOK_PEND_RMW, with __get having` |
|         - | 4072 | ` *                             provided the current value and __set(sName, value)` |
|         - | 4073 | ` *                             taking the computed one.` |
|         - | 4074 | `` *   VM_HOOK_PEND_RMW_DIM    — `$o[$k] op= v` on an ArrayAccess element (php's`` |
|         - | 4075 | ` *                             ASSIGN_DIM_OP): offsetGet($k) provided the current` |
|         - | 4076 | ` *                             value and offsetSet($k, value) takes the computed` |
|         - | 4077 | ` *                             one. The KEY lives in its own reserved memobj,` |
|         - | 4078 | ` *                             whose index this kind keeps in nBackIdx.` |
|         - | 4079 | ` * The armed window is [nJmpPc, nPc]: an owner fetch outside it means the` |
|         - | 4080 | ` * statement was abandoned (a routed throw) or the ??= short-circuit jump was` |
|         - | 4081 | ` * taken — the entry is dropped, no set dispatch (php: the throw/skip discards` |
|         - | 4082 | ` * the write). LIFO order makes nested arms (a ??= RHS containing further` |
|         - | 4083 | ` * hooked stores or coalesce-assigns, a recursive re-entry through a cast` |
|         - | 4084 | ` * inside a modify op) nest correctly. Each entry owns one instance reference;` |
|         - | 4085 | ` * MAGIC entries own their name blob. */` |
|         - | 4086 | `#define VM_HOOK_PEND_RMW         0` |
|         - | 4087 | `#define VM_HOOK_PEND_COAL_HOOK   1` |
|         - | 4088 | `#define VM_HOOK_PEND_COAL_MAGIC  2` |
|         - | 4089 | `#define VM_HOOK_PEND_RMW_MAGIC   3` |
|         - | 4090 | `#define VM_HOOK_PEND_RMW_DIM     4` |
|         - | 4091 | `/* The SCRATCH-slot kinds: armed by the fetch (OP_MEMBER / OP_LOAD_IDX) and` |
|         - | 4092 | ` * consumed by the modify op's tail through VmHookRmwConsume, matched by the` |
|         - | 4093 | ` * scratch index the fetch left in pTos->nIdx. */` |
|         - | 4094 | `#define VM_HOOK_PEND_IS_RMW(iKind) \` |
|         - | 4095 | `	((iKind) == VM_HOOK_PEND_RMW \|\| (iKind) == VM_HOOK_PEND_RMW_MAGIC \` |
|         - | 4096 | `	 \|\| (iKind) == VM_HOOK_PEND_RMW_DIM)` |
|         - | 4097 | `typedef struct VmHookRmw VmHookRmw;` |
|         - | 4098 | `struct VmHookRmw` |
|         - | 4099 | `{` |
|         - | 4100 | `	sxu8 iKind;                 /* VM_HOOK_PEND_* */` |
|         - | 4101 | `	ph7_class_instance *pThis;  /* receiver (owns one reference while pending) */` |
|         - | 4102 | `	ph7_class_attr *pAttr;      /* hooked property (hook kinds; 0 for MAGIC) */` |
|         - | 4103 | ``	sxu32 nBackIdx;             /* BACKING slot index (for `set => expr` stores) */`` |
|         - | 4104 | `	sxu32 nScratchIdx;          /* RMW: scratch slot the modify op operates on;` |
|         - | 4105 | `	                             * SXU32_HIGH for the coalesce kinds */` |
|         - | 4106 | `	SyBlob sName;               /* COAL_MAGIC: property name copy (entry-owned) */` |
|         - | 4107 | `	void *pOwnerStack;          /* arming activation's operand-stack base (identity;` |
|         - | 4108 | `	                             * a nested exec — even a recursive one over the same` |
|         - | 4109 | `	                             * bytecode — has a different base, so it never drops` |
|         - | 4110 | `	                             * an enclosing activation's pending entry) */` |
|         - | 4111 | `	void *pInstrs;              /* arming activation's bytecode array */` |
|         - | 4112 | `	sxu32 nJmpPc;               /* first pc of the armed window (RMW: == nPc;` |
|         - | 4113 | `	                             * coalesce: the OP_NULLC_JMP right after the arm) */` |
|         - | 4114 | `	sxu32 nPc;                  /* pc of the consuming op (RMW: the modify op;` |
|         - | 4115 | `	                             * coalesce: the OP_NULLC_STORE) */` |
|         - | 4116 | `};` |
|         - | 4117 |  |
|         - | 4118 | `/* A shared weak cell: one per weakly-referenced target instance. pObj nulls` |
|         - | 4119 | ` * when the target is released (the PH7_ClassInstanceRelease hook); nRef` |
|         - | 4120 | ` * counts the PHP-side handles (WeakReference objects, WeakMap entries). */` |
|         - | 4121 | `typedef struct VmWeakCell VmWeakCell;` |
|         - | 4122 | `struct VmWeakCell` |
|         - | 4123 | `{` |
|         - | 4124 | `	ph7_class_instance *pObj; /* target instance; 0 once dead */` |
|         - | 4125 | `	ph7_class_instance *pRef; /* the ONE WeakReference handed out for pObj, so` |
|         - | 4126 | `	                           * WeakReference::create($o) answers the same object` |
|         - | 4127 | `	                           * twice as php's does. NOT owned: the WeakReference's` |
|         - | 4128 | `	                           * own release nulls it. */` |
|         - | 4129 | `	sxu32 nRef;               /* PHP-side handle count */` |
|         - | 4130 | `};` |
|         - | 4131 | `/* The OPEN directory handle behind one DirectoryIterator (php's u.dir.dirp).` |
|         - | 4132 | ` * Registered per instance rather than in a property slot: a slot holding a C` |
|         - | 4133 | `` * pointer would be compared by `==` (php's two equal-positioned iterators are`` |
|         - | 4134 | ` * equal) and COPIED by clone, and php's clone opens the directory again. The` |
|         - | 4135 | ` * class's xRelease closes it. */` |
|         - | 4136 | `typedef struct VmDirHandle VmDirHandle;` |
|         - | 4137 | `struct VmDirHandle` |
|         - | 4138 | `{` |
|         - | 4139 | `	const ph7_io_stream *pStream; /* device that opened it */` |
|         - | 4140 | `	void *pHandle;                /* its handle */` |
|         - | 4141 | `	ph7_class_instance *pThis;    /* the owning instance -- and the hash KEY's bytes,` |
|         - | 4142 | `	                               * which SyHashInsert borrows rather than copies */` |
|         - | 4143 | `};` |
|         - | 4144 | `/* One -d/-c php.ini directive queued for the INI chunk (name/value are` |
|         - | 4145 | ` * allocator-owned copies; see PH7_VM_CONFIG_INI_ENTRY). sFile/nLine are the` |
|         - | 4146 | ` * host's source for the refusal warning ("Unknown" plus a virtual line for` |
|         - | 4147 | ` * -d, the real path and line for a -c file) -- empty when the host never` |
|         - | 4148 | ` * supplied one, which silences the warning rather than misattributing it.` |
|         - | 4149 | ` * iStop is the scanner's stop condition, which is what dates that warning and` |
|         - | 4150 | ` * -- for the PH7_INI_STOP_SECTION* codes -- is the whole entry, an unclosed` |
|         - | 4151 | `` * `[` standing in the queue where a directive would be. */`` |
|         - | 4152 | `typedef struct VmIniEntry VmIniEntry;` |
|         - | 4153 | `struct VmIniEntry` |
|         - | 4154 | `{` |
|         - | 4155 | `	SyString sName;` |
|         - | 4156 | `	SyString sValue;` |
|         - | 4157 | `	SyString sFile;` |
|         - | 4158 | `	sxu32 nLine;` |
|         - | 4159 | `	int iStop;           /* PH7_INI_STOP_*: how the host's scanner stopped on this one */` |
|         - | 4160 | `};` |
|         - | 4161 | `/*` |
|         - | 4162 | ` * One live php.ini directive. The table was an embedded-PHP array on a private` |
|         - | 4163 | `` * `__IniS` class; it is C now, seeded lazily on the first INI call from the static`` |
|         - | 4164 | ` * defaults merged with the CLI's -d/-c queue (aIniCli).` |
|         - | 4165 | ` *` |
|         - | 4166 | ` * php exposes both a global_value and a local_value per directive: ini_set() moves` |
|         - | 4167 | ` * the local one, ini_restore() puts the global one back, and get_cfg_var() answers` |
|         - | 4168 | ` * the global one. Both blobs live on the VM allocator, so they are freed with it --` |
|         - | 4169 | ` * no release hook, exactly as aIniCli needs none.` |
|         - | 4170 | ` */` |
|         - | 4171 | `/*` |
|         - | 4172 | ` * The diagnostics of the last date parse — DateTime::getLastErrors()'s whole answer.` |
|         - | 4173 | ` *` |
|         - | 4174 | ` * php keeps one such record per request and both date classes read it, so this is a` |
|         - | 4175 | `` * VM field rather than the PHL-only `public static DateTime::$__dtLastErr` it used to`` |
|         - | 4176 | ` * be. Every message is a static literal owned by the parser, so nothing here owns` |
|         - | 4177 | ` * memory and the record needs no release hook. The kept-vs-total split is php's:` |
|         - | 4178 | `` * `error_count` counts every error the scan raised, while the `errors` map holds one`` |
|         - | 4179 | ` * entry per POSITION (a later error at a position php has already reported replaces` |
|         - | 4180 | ` * the message rather than adding a row).` |
|         - | 4181 | ` */` |
|         - | 4182 | `#define PH7_DT_MAX_WARN 3` |
|         - | 4183 | `#define PH7_DT_MAX_ERR  8` |
|         - | 4184 | `/* One diagnostic row. The message is always a static literal, so a row keeps` |
|         - | 4185 | ` * the pointer rather than the bytes. */` |
|         - | 4186 | `typedef struct phl_dt_diag_row phl_dt_diag_row;` |
|         - | 4187 | `struct phl_dt_diag_row` |
|         - | 4188 | `{` |
|         - | 4189 | `	int iPos;` |
|         - | 4190 | `	const char *zMsg;` |
|         - | 4191 | `};` |
|         - | 4192 | `typedef struct phl_dt_lasterr phl_dt_lasterr;` |
|         - | 4193 | `struct phl_dt_lasterr` |
|         - | 4194 | `{` |
|         - | 4195 | ``	sxu8 bSet;                        /* 0 -> getLastErrors() answers php's `false` */`` |
|         - | 4196 | `	int nWarn;                        /* warning_count (total) */` |
|         - | 4197 | `	int nWarnKept;                    /* rows in the warnings map */` |
|         - | 4198 | `	int aWarnPos[PH7_DT_MAX_WARN];` |
|         - | 4199 | `	const char *azWarn[PH7_DT_MAX_WARN];` |
|         - | 4200 | `	int nErr;                         /* error_count (total) */` |
|         - | 4201 | `	int nErrKept;                     /* rows in the errors map */` |
|         - | 4202 | `	/* php's scanner records an error and READS ON, so a string may carry one per` |
|         - | 4203 | `	 * byte of itself: the rows grow rather than fitting a fixed array. The blob` |
|         - | 4204 | `	 * holds nErrKept phl_dt_diag_row, allocated from the VM's own backend and` |
|         - | 4205 | `	 * released wholesale with it. */` |
|         - | 4206 | `	SyBlob sErr;` |
|         - | 4207 | `};` |
|         - | 4208 | `typedef struct VmIniSlot VmIniSlot;` |
|         - | 4209 | `struct VmIniSlot` |
|         - | 4210 | `{` |
|         - | 4211 | `	SyString sName;   /* static default name, or a VM-lifetime dup of a CLI name */` |
|         - | 4212 | `	sxi32 iAccess;    /* INI_USER\|INI_PERDIR\|INI_SYSTEM bitmask php reports */` |
|         - | 4213 | `	SyBlob sGlobal;   /* php's global_value */` |
|         - | 4214 | `	SyBlob sLocal;    /* php's local_value (what ini_get answers, modulo live wiring) */` |
|         - | 4215 | `	/* php's third state for a value: UNSET. A directive php declares with no` |
|         - | 4216 | `	 * default at all reports NULL rather than the empty string from every` |
|         - | 4217 | `	 * surface that shows the raw value, and the empty string IS a different` |
|         - | 4218 | `	 * value -- one a script can write. An empty blob cannot tell them apart, so` |
|         - | 4219 | `	 * the two flags do. */` |
|         - | 4220 | `	sxu8 bGlobalNull; /* the directive was declared with no value */` |
|         - | 4221 | `	sxu8 bLocalNull;  /* and nothing has written one since */` |
|         - | 4222 | `};` |
|         - | 4223 | ``/* php's default spl_autoload_extensions() list: the `.inc` is tried FIRST,`` |
|         - | 4224 | ` * which is what decides the answer when two files with the same base name` |
|         - | 4225 | ` * both declare the class. */` |
|         - | 4226 | `#define PH7_SPL_AUTOLOAD_EXT ".inc,.php"` |
|         - | 4227 | `struct ph7_vm` |
|         - | 4228 | `{` |
|         - | 4229 | `	SyMemBackend sAllocator;	/* Memory backend */` |
|         - | 4230 | `#if defined(PH7_ENABLE_THREADS)` |
|         - | 4231 | `	SyMutex *pMutex;           /* Recursive mutex associated with VM. */` |
|         - | 4232 | `#endif` |
|         - | 4233 | `	ph7 *pEngine;               /* Interpreter that own this VM */` |
|         - | 4234 | `	SySet aByteCode;            /* Default bytecode container */` |
|         - | 4235 | `	SySet *pByteContainer;      /* Current bytecode container */` |
|         - | 4236 | `	VmFrame *pFrame;            /* Stack of active frames */` |
|         - | 4237 | `	SyPRNGCtx sPrng;            /* PRNG context (engine-internal, OS-seeded entropy) */` |
|         - | 4238 | `	SyMT19937Ctx sMt;           /* MT19937 backing rand()/mt_rand(); reset by srand()/mt_srand() */` |
|         - | 4239 | `	sxi32 mtSeeded;             /* TRUE once sMt holds a seed (lazy: first draw seeds from the OS CSPRNG, like PHP) */` |
|         - | 4240 | `	VmMemPool aMemObj;          /* Object allocation table (segmented) */` |
|         - | 4241 | `	SySet aLitObj;              /* Literals allocation table */` |
|         - | 4242 | `	ph7_value *aOps;            /* Operand stack */` |
|         - | 4243 | `	SyHash hClass;              /* Compiled classes container */` |
|         - | 4244 | `	SyHash hConstant;           /* Host-application and user defined constants container */` |
|         - | 4245 | `	SyHash hHostFunction;       /* Host-application installable functions */` |
|         - | 4246 | `	SyHash hFunction;           /* Compiled functions */` |
|         - | 4247 | `	SyHash hSuper;              /* Superglobals hashtable */` |
|         - | 4248 | `	sxu32 aSuperFirst[8];       /* Which FIRST BYTES any superglobal name starts with, as a` |
|         - | 4249 | `	                             * 256-bit set. Every variable access asks hSuper before the` |
|         - | 4250 | `	                             * frame -- php's rule, and the order cannot change -- and` |
|         - | 4251 | `	                             * that question hashed the whole name to answer "no" for the` |
|         - | 4252 | `	                             * ~9 names that are superglobals ($GLOBALS and the $_* set).` |
|         - | 4253 | `	                             * Two thirds of the engine's hash lookups on a real workload` |
|         - | 4254 | `	                             * were that miss. One bit test now settles it for a name that` |
|         - | 4255 | `	                             * cannot be one. */` |
|         - | 4256 | `	SyHash hPDO;                /* PDO installed drivers */` |
|         - | 4257 | `	SyBlob sConsumer;           /* Default VM consumer [i.e Redirect all VM output to this blob] */` |
|         - | 4258 | `	SyBlob sWorker;             /* General purpose working buffer */` |
|         - | 4259 | `	SySet aFiles;               /* Stack of processed files */` |
|         - | 4260 | ``	SyBlob sReflectConstName;   /* Scratch for the `Class::MEMBER` name ReflectionParameter::`` |
|         - | 4261 | `	                             * getDefaultValueConstantName() answers for a class-constant` |
|         - | 4262 | `	                             * default: the two halves live in separate literals, so the` |
|         - | 4263 | `	                             * joined text needs somewhere to live past the return. */` |
|         - | 4264 | `	SySet aIncFrame;            /* Stack of ACTIVE include/require/eval activations (VmIncFrame).` |
|         - | 4265 | `	                             * php shows each of them as a trace frame of its own -- the` |
|         - | 4266 | ``	                             * `#N main.php(4): require()` between the included file's frames`` |
|         - | 4267 | `	                             * and the caller's -- and nothing else in this engine records` |
|         - | 4268 | `	                             * one: an include shares its caller's variable scope, so it` |
|         - | 4269 | `	                             * pushes no VmFrame to be found later. */` |
|         - | 4270 | `	SySet aPaths;               /* Set of import paths */` |
|         - | 4271 | `	SySet aIncluded;            /* Set of included files */` |
|         - | 4272 | ``	SySet aEvalFile;            /* Interned `<file>(<line>) : eval()'d code` unit names, one per`` |
|         - | 4273 | `	                             * eval() SITE. A compiled function or class copies the name it` |
|         - | 4274 | `	                             * was declared in, so the text has to outlive the eval that` |
|         - | 4275 | `	                             * made it -- and an eval in a loop must not mint a fresh copy` |
|         - | 4276 | `	                             * every turn, since the site's file and line never change. */` |
|         - | 4277 | `	SySet aOB;                  /* Stackable output buffers */` |
|         - | 4278 | `	SySet aResponseHeaders;     /* HTTP response headers (VmResponseHeader entries) */` |
|         - | 4279 | `	int iResponseStatus;        /* HTTP response status code (default 200) */` |
|         - | 4280 | `	int bHeadersSent;           /* TRUE once non-OB output has been emitted */` |
|         - | 4281 | `	SyBlob sOutStartFile;       /* WHERE that first output went out: php names the file and the` |
|         - | 4282 | `	                             * line in four diagnostics ("output started at %s:%u", and the` |
|         - | 4283 | `	                             * session pair's "sent from %s on line %u") and hands them to` |
|         - | 4284 | `	                             * headers_sent()'s two by-ref out-params. Empty until output. */` |
|         - | 4285 | `	sxu32 nOutStartLine;        /* ... its line (0 while nothing has been emitted) */` |
|         - | 4286 | `	SyBlob sSessStartFile;      /* WHERE the active session was started: php's session-locked ini` |
|         - | 4287 | `	                             * diagnostic names it ("started from %s on line %u"). */` |
|         - | 4288 | `	sxu32 nSessStartLine;       /* ... its line */` |
|         - | 4289 | `	int bHttpContext;           /* TRUE when an HTTP request has been fed (server/CGI mode) */` |
|         - | 4290 | `	int bInlineTryCatch;        /* ROOT C: TRUE once the inline try/catch/finally VM handlers exist,` |
|         - | 4291 | `	                             * enabling the compiler to inline generator-body try/catch (so a` |
|         - | 4292 | ``	                             * `yield` in a catch/finally suspends). Default 0 = legacy path. */`` |
|         - | 4293 | `	int bRenderingUncaught;     /* TRUE while the uncaught-exception report is being rendered. That` |
|         - | 4294 | `	                             * report asks the exception for its own trace (getTraceAsString,` |
|         - | 4295 | `	                             * userland code), so anything that throws in there would re-enter` |
|         - | 4296 | `	                             * the renderer and recurse until the process dies — which is exactly` |
|         - | 4297 | `	                             * what a bad max-arity stamp did (18 GB before the OOM killer, 19 Jul` |
|         - | 4298 | `	                             * 2026). The guard makes the second entry fall back to the` |
|         - | 4299 | `	                             * synthesized trace instead of looping. */` |
|         - | 4300 | `	int bCompilingBuiltin;      /* TRUE while the embedded builtin PHP library chunks compile at VM` |
|         - | 4301 | `	                             * init: classes/functions defined then are stamped INTERNAL so` |
|         - | 4302 | `	                             * Reflection reports isInternal() like Zend does for C-level code. */` |
|         - | 4303 | ``	int bSyntaxCheck;           /* TRUE for a `phl -l` compile (PH7_SYNTAX_CHECK): the unit is only`` |
|         - | 4304 | `	                             * ever PARSED, never run. A class declaration is then never` |
|         - | 4305 | `	                             * DEFERRED -- nothing autoloads here, so deferring would leave its` |
|         - | 4306 | `	                             * whole body unparsed and lint an unparsable file clean -- and the` |
|         - | 4307 | `	                             * refusals that only a resolved parent/interface/trait can answer` |
|         - | 4308 | `	                             * are not raised, because php binds inheritance at run time and` |
|         - | 4309 | ``	                             * `php -l` does not report those either. */`` |
|         - | 4310 | `	int bReflectBypass;         /* Consume-once: the next method OP_CALL skips the visibility` |
|         - | 4311 | `	                             * check (ReflectionMethod::invoke bypasses protection like PHP` |
|         - | 4312 | `	                             * 8.1+). Cleared by the check site; never survives past one call. */` |
|         - | 4313 | `	VmNativeCall *pNativeCall;  /* The INTERNAL functions and methods running right now,` |
|         - | 4314 | `	                             * newest first -- the frames php's trace carries for a` |
|         - | 4315 | `	                             * throw raised inside a C body. See VmNativeCall. */` |
|         - | 4316 | `	int bElideNativeCall;       /* Consume-once: the next internal call links its VmNativeCall` |
|         - | 4317 | `	                             * already elided. Armed only by PH7_VmCallIteratorMethod for` |
|         - | 4318 | `	                             * a Generator, whose methods the engine's own iteration drives` |
|         - | 4319 | `	                             * where php calls the generator's iterator handlers -- no call,` |
|         - | 4320 | `	                             * so no frame in php's trace. Saved and restored around that` |
|         - | 4321 | `	                             * one dispatch. */` |
|         - | 4322 | `	SyString *pNativeTraceName; /* Consume-once: the next OP_CALL's frame is VM_FRAME_NATIVE_TRACE,` |
|         - | 4323 | `	                             * reached for by this running INTERNAL function. Armed only by` |
|         - | 4324 | `	                             * the autoload loop, around each loader it calls. */` |
|         - | 4325 | `	SyString *pNativeFrameName; /* Consume-once: the next OP_CALL's frame was entered by this` |
|         - | 4326 | `	                             * INTERNAL function and so has no userland call site, even` |
|         - | 4327 | `	                             * though its argument BINDING still follows the caller. Armed` |
|         - | 4328 | `	                             * only by a call_user_func/_array php's compiler could not` |
|         - | 4329 | `	                             * elide (an unqualified one inside a namespace). */` |
|         - | 4330 | `	int bCallbackWeak;          /* Consume-once: the next OP_CALL is an INTERNAL function invoking a` |
|         - | 4331 | `	                             * userland callback (array_map, usort, an autoloader, a shutdown` |
|         - | 4332 | `	                             * function, Reflection's invoke, Closure::call), which php runs in` |
|         - | 4333 | `	                             * WEAK mode however strict the file that reached the builtin is —` |
|         - | 4334 | `	                             * there is no "calling file" at such a boundary. The two php` |
|         - | 4335 | `	                             * FORWARDS, call_user_func and call_user_func_array, do not set it:` |
|         - | 4336 | `	                             * they pass the caller's own mode on an argument map. Cleared at` |
|         - | 4337 | `	                             * the head of OP_CALL like the latches below. */` |
|         - | 4338 | `	int bHostDiscard;           /* The HOST (C) function now running was called from a statement` |
|         - | 4339 | `	                             * that throws its answer away. Set around the foreign-function` |
|         - | 4340 | `	                             * dispatch in OP_CALL from the instruction's bDiscard, and read` |
|         - | 4341 | `	                             * by exactly two builtins: php's two callback FORWARDS. */` |
|         - | 4342 | `	int bDiscardCallback;       /* Consume-once: the next call dispatched through` |
|         - | 4343 | `	                             * PH7_VmCallUserFunction inherits that drop, which is how` |
|         - | 4344 | ``	                             * `call_user_func('f');` warns for a #[\NoDiscard] `f` and`` |
|         - | 4345 | ``	                             * `array_map('f', $a);` does not (php special-cases the same two`` |
|         - | 4346 | `	                             * names at compile time). Consumed at the head of OP_CALL. */` |
|         - | 4347 | `	int bDirectCallable;        /* Consume-once: the next PH7_VmCallUserFunctionWithMap is a` |
|         - | 4348 | ``	                             * DIRECT `$cb()` of a "C::m" string or ['C','m'] pair, not a`` |
|         - | 4349 | `	                             * callback. php binds a class-name callback to the caller's` |
|         - | 4350 | ``	                             * compatible `$this`; the direct spelling calls through C. */`` |
|         - | 4351 | `	int bDynamicForward;        /* Consume-once, armed by the same two FORWARDS: the callback they` |
|         - | 4352 | `	                             * are about to dispatch is a call php's compiler could NOT fold` |
|         - | 4353 | `	                             * into a direct one, because the callable was not a literal` |
|         - | 4354 | ``	                             * string (`call_user_func($n)`), so php runs it as a DYNAMIC call`` |
|         - | 4355 | `	                             * (ZEND_INIT_USER_CALL). The folded shape,` |
|         - | 4356 | ``	                             * `call_user_func('compact', 'a')`, is a plain direct call there`` |
|         - | 4357 | `	                             * and leaves this clear. Read into PH7_CTX_CALL_DYNAMIC at the` |
|         - | 4358 | `	                             * head of OP_CALL and cleared with the latches above. */` |
|         - | 4359 | `	int bMagicDispatch;         /* Consume-once: the next method OP_CALL is the ENGINE reaching for` |
|         - | 4360 | `	                             * a magic method (PH7_VmCallMagicMethod), so the visibility check` |
|         - | 4361 | `	                             * lets a non-public one through — php only WARNS at such a` |
|         - | 4362 | `	                             * declaration and dispatches anyway. Set at the engine's own` |
|         - | 4363 | `	                             * dispatch sites only, so a call the USER wrote (including a` |
|         - | 4364 | ``	                             * first-class `$o->__get(...)`, which reaches the same C`` |
|         - | 4365 | `	                             * dispatcher) is still denied. Cleared at the head of OP_CALL. */` |
|         - | 4366 | `	int bClosureScreened;       /* Consume-once: the method call now being dispatched comes out of a` |
|         - | 4367 | `	                             * Closure whose callee was RESOLVED and screened when the closure was` |
|         - | 4368 | ``	                             * BUILT (`$this->p(...)`, `Closure::fromCallable([$this,'p'])`,`` |
|         - | 4369 | `	                             * ReflectionMethod::getClosure), so no site may re-decide its` |
|         - | 4370 | `	                             * visibility against the CALLER. php stores a resolved function +` |
|         - | 4371 | `	                             * scope in the Closure and never looks the name up again; PHL keeps a` |
|         - | 4372 | `	                             * name, so without this latch an escaped closure over a private method` |
|         - | 4373 | `	                             * died at the invocation php runs. Armed by VmClosureUnwrap, read by` |
|         - | 4374 | `	                             * the array-callable dispatch sites, cleared at the head of OP_CALL. */` |
|         - | 4375 | `	int bClosureStaticTramp;    /* Consume-once: the pair now being dispatched comes out of an UNBOUND` |
|         - | 4376 | `	                             * method Closure over a name no method answered -- php's __callStatic` |
|         - | 4377 | `	                             * TRAMPOLINE, decided where the closure was built. The pair's own` |
|         - | 4378 | ``	                             * catch-all rule would re-ask the CALLER's `$this` and pick __call (or`` |
|         - | 4379 | ``	                             * refuse a direct `$c()` as non-static) inside an instance of the`` |
|         - | 4380 | `	                             * class. Armed by VmClosureUnwrap, read by the pair dispatch` |
|         - | 4381 | `	                             * (PH7_VmCallUserFunctionWithMap), cleared with bClosureScreened. */` |
|         - | 4382 | `	int bClosureNoNamed;        /* Consume-once, same lifetime: the pair comes out of a` |
|         - | 4383 | `	                             * Closure::fromCallable() TRAMPOLINE over __call/__callStatic. php` |
|         - | 4384 | `	                             * builds that one as an internal function with no parameters and` |
|         - | 4385 | `	                             * no variadic, so a named argument is "Unknown named parameter"` |
|         - | 4386 | ``	                             * before the catch-all runs; the `(...)` syntax's trampoline has a`` |
|         - | 4387 | ``	                             * `...$arguments` and packs it into $args instead. */`` |
|         - | 4388 | `	char zDefTz[68];           /* date_default_timezone_set() identifier, stored verbatim like php` |
|         - | 4389 | `	                             * (default "UTC"; only UTC/GMT are accepted — no tz database) */` |
|         - | 4390 | `	sxu32 nDefTz;               /* zDefTz length in bytes */` |
|         - | 4391 | `	sxu8 bDefTzExplicit;        /* a script called date_default_timezone_set(). php latches on` |
|         - | 4392 | `	                             * that: a later ini_set('date.timezone') records the DIRECTIVE` |
|         - | 4393 | `	                             * and no longer moves the default. */` |
|         - | 4394 | `	phl_dt_lasterr sDtLastErr;  /* DateTime::getLastErrors()'s answer. Was a PHL-only` |
|         - | 4395 | ``	                             * `public static $__dtLastErr` on DateTime, a property php has`` |
|         - | 4396 | `	                             * no equivalent of; both date classes read this field now. */` |
|         - | 4397 | `	SySet aShutdown;            /* Stack of shutdown user callbacks */` |
|         - | 4398 | `	SySet aIniCli;              /* php.ini directives from the CLI (-d/-c): VmIniEntry copies,` |
|         - | 4399 | `	                             * merged into aIniTab when the directive table is seeded */` |
|         - | 4400 | `	SySet aIniTab;              /* The live directive table (VmIniSlot), sorted by name so` |
|         - | 4401 | `	                             * ini_get_all() needs no sort of its own */` |
|         - | 4402 | `	SySet aPersistSock;         /* PERSISTENT socket handles (VmPersistSock), keyed by the` |
|         - | 4403 | `	                             * address as the opener spelled it: php hands the SAME` |
|         - | 4404 | `	                             * resource back for a second pfsockopen() of one address */` |
|         - | 4405 | `	sxu8 bIniSeeded;            /* aIniTab has been built (lazily, on the first INI call) */` |
|         - | 4406 | `	int iPosixErr;              /* ext/posix's remembered errno: what` |
|         - | 4407 | `	                             * posix_get_last_error()/posix_errno() answer.` |
|         - | 4408 | `	                             * php keeps one per MODULE; per VM is the same` |
|         - | 4409 | `	                             * lifetime for a program and keeps two embedded` |
|         - | 4410 | `	                             * VMs apart. Nothing ever clears it -- a later` |
|         - | 4411 | `	                             * SUCCESS leaves the last failure standing,` |
|         - | 4412 | `	                             * which is php's own contract. */` |
|         - | 4413 | `	void *pSyslog;              /* ext/standard's syslog state (builtin_syslog.c owns the` |
|         - | 4414 | `	                             * shape): the prefix openlog() was given -- POSIX keeps the` |
|         - | 4415 | `	                             * POINTER, so it has to outlive the call -- and, on Windows,` |
|         - | 4416 | `	                             * the event-source handle a record is reported through.` |
|         - | 4417 | `	                             * Allocated on the first call, freed by PH7_SyslogVmRelease. */` |
|         - | 4418 | `	void *pPcntl;               /* ext/pcntl's per-VM state (builtin_pcntl.c owns the` |
|         - | 4419 | `	                             * shape): the handler each signal was last given, the` |
|         - | 4420 | `	                             * remembered errno and the async-dispatch flag. Allocated` |
|         - | 4421 | `	                             * lazily on the first call and freed by PH7_PcntlVmRelease,` |
|         - | 4422 | `	                             * which also puts every disposition this VM took over back` |
|         - | 4423 | `	                             * to SIG_DFL. */` |
|         - | 4424 | `	void *pGettext;             /* ext/gettext's per-VM state (builtin_gettext.c owns the` |
|         - | 4425 | `	                             * shape): the domain bindings, the current textdomain and` |
|         - | 4426 | `	                             * the catalog each domain last resolved. Allocated lazily` |
|         - | 4427 | `	                             * from sAllocator on the first call, and freed with it. */` |
|         - | 4428 | ``	/* Session state. Was a private `__SessS` class with five static properties, which`` |
|         - | 4429 | `	 * the INI subsystem had to reach into to live-wire session.name/session.save_path;` |
|         - | 4430 | `	 * both subsystems read these fields now, so neither depends on the other's shape. */` |
|         - | 4431 | `	sxi32 iSessStatus;          /* PHP_SESSION_NONE / _ACTIVE */` |
|         - | 4432 | `	SyBlob sSessId;             /* current session id ("" = none yet) */` |
|         - | 4433 | `	SyBlob sSessName;           /* cookie/session name (default "PHPSESSID") */` |
|         - | 4434 | `	SyBlob sSessPath;           /* save path ("" = not resolved yet -> sys_get_temp_dir()) */` |
|         - | 4435 | `	ph7_value sSessHandler;     /* session_set_save_handler(): the handler OBJECT, or the array` |
|         - | 4436 | `	                             * of callables the procedural form passes. NULL = the built-in` |
|         - | 4437 | ``	                             * `files` store. */`` |
|         - | 4438 | `	sxu8 bSessOpened;           /* a userland handler's open() has run for this session */` |
|         - | 4439 | `	SyBlob sSessData;           /* the payload the store last handed back or was handed: what` |
|         - | 4440 | `	                             * session.lazy_write compares the next write against */` |
|         - | 4441 | `	SyHash hWeakCell;           /* instance pointer bytes -> VmWeakCell* (weak-reference registry;` |
|         - | 4442 | `	                             * PH7_ClassInstanceRelease kills matching cells on free) */` |
|         - | 4443 | `	SyHash hDirHandle;          /* instance pointer bytes -> VmDirHandle* (the open DIR* behind a` |
|         - | 4444 | `	                             * DirectoryIterator; the class's xRelease closes and unregisters) */` |
|         - | 4445 | `	SySet aAutoload;            /* Stack of spl_autoload callbacks */` |
|         - | 4446 | `	SyBlob sAutoloadExt;        /* spl_autoload_extensions(): the comma-separated list` |
|         - | 4447 | `	                             * spl_autoload() tries when it is handed none.` |
|         - | 4448 | `	                             * php's own default is ".inc,.php" and the ORDER is` |
|         - | 4449 | `	                             * observable -- it is what decides which of two files` |
|         - | 4450 | `	                             * with the same base name defines the class. */` |
|         - | 4451 | `	SyHash hAutoloadActive;     /* Classes currently being autoloaded (reentrancy guard) */` |
|         - | 4452 | `	SyHash hTypedSlot;          /* memobj nIdx -> VmClassAttr* for every slot a store must be` |
|         - | 4453 | `	                             * FILTERED through: a declared TYPE to enforce, a native` |
|         - | 4454 | `	                             * class's write handler, or both (PH7_ATTR_STORE_FILTERED).` |
|         - | 4455 | `	                             * Registered and dropped through the two helpers below, which` |
|         - | 4456 | `	                             * are the only writers -- the predicate must not be spelled` |
|         - | 4457 | `	                             * out at a call site again. */` |
|         - | 4458 | `	unsigned char *pFilterBits; /* One BIT per memobj slot: is it in hTypedSlot? A store to a` |
|         - | 4459 | `	                             * property asks that on every write, and it is a hash of a` |
|         - | 4460 | `	                             * dense small INTEGER to hear "no" -- 9M of the engine's 225M` |
|         - | 4461 | `	                             * lookups on the ecosystem gate's phpcs step. The slot index` |
|         - | 4462 | `	                             * indexes this directly instead. Kept by the same two helpers` |
|         - | 4463 | `	                             * that own the table, so it cannot drift from it; a slot past` |
|         - | 4464 | `	                             * nFilterBits was never registered, which is the same answer. */` |
|         - | 4465 | `	sxu32 nFilterBits;          /* How many slots pFilterBits covers (0 = never allocated) */` |
|         - | 4466 | `	sxu8 bFilterBitsOff;        /* The bitmap could not be grown to cover a slot that IS` |
|         - | 4467 | `	                             * registered, so it can no longer answer for anything and` |
|         - | 4468 | `	                             * every question goes back to the table. Sticky, because a` |
|         - | 4469 | `	                             * fresh bitmap would be missing the bits of everything` |
|         - | 4470 | `	                             * registered before it. An allocation CAN fail here without` |
|         - | 4471 | `	                             * the box being out of memory -- a script's memory_limit is a` |
|         - | 4472 | `	                             * real ceiling since the 137th session -- and answering "not` |
|         - | 4473 | `	                             * filtered" there would silently skip a typed property's` |
|         - | 4474 | `	                             * type check, its readonly screen and a native write` |
|         - | 4475 | `	                             * handler. */` |
|         - | 4476 | `	sxu32 nNativeSetSlot;       /* How many of those slots carry a native WRITE HANDLER. Kept` |
|         - | 4477 | `	                             * by the same two helpers, and read by the in-place mutation` |
|         - | 4478 | ``	                             * opcodes: `$i++` on an ordinary variable must not pay for a`` |
|         - | 4479 | `	                             * hash lookup just because some class in the script declares a` |
|         - | 4480 | `	                             * typed property, and with no handler-backed slot alive there` |
|         - | 4481 | `	                             * is nothing for one to find. */` |
|         - | 4482 | `	SySet aException;           /* Stack of loaded exception */` |
|         - | 4483 | `	SySet aFinallyAction;       /* ROOT C: stack of VmFinallyAction — pending action (fallthrough /` |
|         - | 4484 | `	                             * rethrow / return / break-continue) for each inline finally in flight */` |
|         - | 4485 | `	ph7_class_instance *pPendingException; /* Exception deferred past a finally block */` |
|         - | 4486 | `	ph7_class_instance *pInflightException; /* Exception being unwound while a finally runs; a throw from` |
|         - | 4487 | `	                                         * that finally that escapes the finally chains it as $previous` |
|         - | 4488 | `	                                         * (PHP finally-supersede) */` |
|         - | 4489 | `	sxu32 nInflightExcBase;                 /* Exception-stack depth when the in-flight finally started; a throw` |
|         - | 4490 | `	                                         * is "leaving the finally" once the stack unwinds to/below this */` |
|         - | 4491 | `	/* The in-place-catch resume target (ROOT B). The four fields are ONE record and` |
|         - | 4492 | `	 * only mean anything together: a frame paired with another try's landing pad` |
|         - | 4493 | `	 * drains the operand stack to a foreign base and lands mid-statement. They are` |
|         - | 4494 | `	 * written, cleared, saved and restored only through VmSetResumeTarget /` |
|         - | 4495 | `	 * VmClearResumeTarget / VmSaveResumeTarget / VmRestoreResumeTarget — never one` |
|         - | 4496 | `	 * at a time. */` |
|         - | 4497 | `	VmFrame *pResumeFrame;      /* Body frame whose in-place catch consumed the live throw */` |
|         - | 4498 | `	sxu32 iResumePc;            /* Its post-try landing pad (1-based, as iExceptionJump) */` |
|         - | 4499 | `	void *pResumeInstr;         /* Bytecode array the catching try lives in; resume only in that exec */` |
|         - | 4500 | `	sxi32 iResumeStackDepth;    /* Operand-stack base (0-based TOS index) of the catching try, recorded` |
|         - | 4501 | `	                             * with the resume target. Used only by Generator::throw() inject-at-yield` |
|         - | 4502 | `	                             * to drain abandoned mid-expression operands before landing at iResumePc. */` |
|         - | 4503 | `	/* ROOT C inline redirect: set by VmThrowException when a throw is caught by an INLINE` |
|         - | 4504 | `	 * try (generator body). The throw site checks the pair (pInlineInstr, pInlineFrame)` |
|         - | 4505 | `	 * against its own (aInstr, pEntryFrame), drains the operand stack to iInlineDrain, and` |
|         - | 4506 | `	 * jumps to iInlinePc; a mismatch means another activation owns it, so the throw` |
|         - | 4507 | `	 * propagates. The bytecode array alone is NOT identity: two live activations of the` |
|         - | 4508 | `	 * same function share it, so a generator whose sibling activation owned the try` |
|         - | 4509 | `	 * consumed the redirect and ran that try's finally against its OWN variables (twig's` |
|         - | 4510 | ``	 * `Template::yieldBlock`, whose recursive delegation runs three activations of one`` |
|         - | 4511 | ``	 * method at once, read an unset `$level` there). The frame pins the activation, the`` |
|         - | 4512 | `	 * same pairing VmRecordedResume and OP_LOAD_EXCEPTION's activation match already use.` |
|         - | 4513 | `	 * Separate from the ROOT B fields above (legacy path). */` |
|         - | 4514 | `	void *pInlineInstr;         /* Owner bytecode array of the catching inline try (0 = none) */` |
|         - | 4515 | `	void *pInlineFrame;         /* Body frame that owns that try (the activation's identity) */` |
|         - | 4516 | `	sxu32 iInlinePc;            /* 0-based target pc (iHandlerPc or iFinallyPc) */` |
|         - | 4517 | `	sxi32 iInlineDrain;         /* Operand-stack base to drain to before landing (0-based TOS idx) */` |
|         - | 4518 | `	SySet aMagicGuard;          /* In-flight magic-accessor guard (php's property guard):` |
|         - | 4519 | `	                             * {instance, property-name hash, kind} entries pushed around a` |
|         - | 4520 | `	                             * __get dispatch so a self-recursive read of the same property` |
|         - | 4521 | `	                             * falls back to the undefined-property path instead of looping. */` |
|         - | 4522 | `	ph7_class_instance *pMagicSetThis; /* Pending __set receiver (band A #3b): OP_MEMBER detected a` |
|         - | 4523 | `	                             * plain store to a missing/inaccessible property whose class` |
|         - | 4524 | `	                             * declares __set; the VALUE only exists at the immediately-` |
|         - | 4525 | `	                             * following OP_STORE, which consumes this (with sMagicSetName)` |
|         - | 4526 | `	                             * and dispatches __set($name,$value). Holds a reference;` |
|         - | 4527 | `	                             * one-instruction lifetime by construction. */` |
|         - | 4528 | `	SyBlob sMagicSetName;       /* Pending __set property name (stable copy) */` |
|         - | 4529 | `	ph7_class_instance *pHookSetThis; /* Pending property-hook set receiver (PHP 8.4): OP_MEMBER` |
|         - | 4530 | `	                             * detected a plain store to a hooked property; the following` |
|         - | 4531 | `	                             * OP_STORE consumes this (with pHookSetAttr/nHookSetIdx) and` |
|         - | 4532 | `	                             * dispatches __phl_hook_set_NAME — or throws the read-only` |
|         - | 4533 | `	                             * Error when the property has no set hook. Owns one instance` |
|         - | 4534 | `	                             * reference while armed. */` |
|         - | 4535 | `	ph7_class_attr *pHookSetAttr; /* Pending hook-set property (declared attr; name + flags) */` |
|         - | 4536 | ``	sxu32 nHookSetIdx;          /* Pending hook-set BACKING slot index (for `set => expr`) */`` |
|         - | 4537 | ``	VmClassAttr *pRefTargetAttr; /* Pending reference-store target (`$o->p =& $x`): OP_MEMBER tagged`` |
|         - | 4538 | `	                             * PH7_MEMBER_REF_TARGET resolved the instance property slot and` |
|         - | 4539 | `	                             * stashed it here; the immediately-following member-marked` |
|         - | 4540 | `	                             * OP_STORE_REF rebinds it to alias the source variable's slot.` |
|         - | 4541 | `	                             * One-instruction lifetime by construction. */` |
|         - | 4542 | ``	ph7_class_attr *pRefTargetStaticAttr; /* Same, for a static-property target (`self::$s =& $x`). */`` |
|         - | 4543 | `	ph7_class_instance *pRefTargetThis;   /* Instance owning pRefTargetAttr; retained (iRef++) by` |
|         - | 4544 | `	                             * OP_MEMBER, released by the consuming OP_STORE_REF. */` |
|         - | 4545 | `	SySet aHookRmw;             /* Pending property-hook read-modify-write write-backs (LIFO;` |
|         - | 4546 | `	                             * VmHookRmw entries — see the struct above ph7_vm). */` |
|         - | 4547 | `	ph7_class_instance *pMagicCallThis; /* Pending __call receiver (band A #3b): OP_MEMBER hit a` |
|         - | 4548 | `	                             * missing (or inaccessible) method on a class declaring` |
|         - | 4549 | `	                             * __call/__callStatic and marked the callee slot` |
|         - | 4550 | `	                             * MEMOBJ_AUX_MAGICCALL; the packing body OP_CALL then runs` |
|         - | 4551 | `	                             * (VmMagicCallDispatch) consumes this + the class + the original` |
|         - | 4552 | `	                             * name. Holds a reference; NULL for __callStatic. */` |
|         - | 4553 | `	ph7_class *pMagicCallClass; /* Pending __call/__callStatic declaring class */` |
|         - | 4554 | `	ph7_class *pMagicCallLsb;   /* ...and its forwarded called class, or 0 (VmMagicCall.pLsb) */` |
|         - | 4555 | `	ph7_class *pConstEvalClass; /* Transient: class whose constant/property initializer bytecode is` |
|         - | 4556 | `	                             * being evaluated (VmLocalExec has no method frame, so self::/parent::` |
|         - | 4557 | `	                             * inside an initializer resolve through this fallback — consulted by` |
|         - | 4558 | `	                             * PH7_VmPeekDeclaringClass/PH7_VmPeekTopClass when no frame matches). */` |
|         - | 4559 | `	void *pConstEvalFrame;      /* The VmFrame that was current when an ON-DEMAND const initializer` |
|         - | 4560 | `	                             * eval began (VmLocalExec pushes no frame). While the current frame` |
|         - | 4561 | `	                             * still equals it, self::/parent:: resolve to pConstEvalClass even` |
|         - | 4562 | `	                             * though an outer method frame exists (e.g. Base::CONST accessed from` |
|         - | 4563 | `	                             * Sub::method() must NOT resolve self to Sub). A method call inside the` |
|         - | 4564 | `	                             * initializer pushes a new frame, so the marker no longer matches and` |
|         - | 4565 | `	                             * that method's own declaring class wins. NULL outside on-demand eval. */` |
|         - | 4566 | `	sxi32 nConstEvalDepth;      /* Nesting depth of constant/enum-case initializer evaluations. A` |
|         - | 4567 | `	                             * cycle detected at an inner level (pConstCycleAttr) is thrown only` |
|         - | 4568 | `	                             * when depth returns to 0 — a throw INSIDE an initializer mini-exec` |
|         - | 4569 | `	                             * cannot be routed to a user catch (pre-existing engine restriction),` |
|         - | 4570 | `	                             * so the outermost, opcode-level evaluation raises it instead. */` |
|         - | 4571 | `	ph7_class_attr *pConstCycleAttr;  /* Self-referencing constant detected during evaluation */` |
|         - | 4572 | `	ph7_class *pConstCycleClass;      /* ...and the class it belongs to (for the Error message) */` |
|         - | 4573 | `	SyBlob sMagicCallName;      /* Pending original method name (stable copy) */` |
|         - | 4574 | `	ph7_user_func *pMagicCallFunc; /* The __call/__callStatic packing body's function record, built on` |
|         - | 4575 | `	                             * first use (PH7_VmMagicCallFunc) and NOT registered in` |
|         - | 4576 | `	                             * hHostFunction: OP_CALL points straight at it, so the dispatch has` |
|         - | 4577 | `	                             * no PHP-visible name to reach it by. */` |
|         - | 4578 | `	sxi32 nBoundaryRc;          /* C-boundary parked throw status (0 / PH7_EXCEPTION / PH7_ABORT).` |
|         - | 4579 | `	                             * Set by VmBoundaryPark when a PHP callee invoked from a C site` |
|         - | 4580 | `	                             * (magic method, cast hook, __destruct, user callback) raised and` |
|         - | 4581 | `	                             * that C site has no status channel to route it. Consumed once per` |
|         - | 4582 | `	                             * dispatch at the executor's fetch point (and cleared wherever the` |
|         - | 4583 | `	                             * same in-flight throw is landed via VmRecordedResume or the inline` |
|         - | 4584 | `	                             * redirect), so a swallowed throw outlives at most the C remainder` |
|         - | 4585 | `	                             * of one opcode instead of silently resuming execution. */` |
|         - | 4586 | `	sxu32 nThrowFence;          /* 0, or 1 + the exception-stack depth whose handlers a throw may` |
|         - | 4587 | `	                             * not reach: set around a user callback whose exception php` |
|         - | 4588 | `	                             * leaves PENDING for the C door that called it (which wraps it as` |
|         - | 4589 | `	                             * a TypeError's previous). A throw finding no handler above the` |
|         - | 4590 | `	                             * fence stops there instead of running an outer catch in place,` |
|         - | 4591 | `	                             * parking its instance in pFencedExc (VmThrowException). */` |
|         - | 4592 | `	ph7_class_instance *pFencedExc; /* The instance that stopped at nThrowFence (one reference) */` |
|         - | 4593 | `	int bReturnTypeFence;       /* A return value is being checked against its declared type: a` |
|         - | 4594 | ``	                             * `callable` arm raises its scope deprecation behind the fence */`` |
|         - | 4595 | `	ph7_class_instance *pReturnTypeExc; /* ...and the exception the handler threw there, the` |
|         - | 4596 | `	                             * $previous of the return TypeError (one reference) */` |
|         - | 4597 | `	SySet aIOstream;            /* Installed IO stream container */` |
|         - | 4598 | `	/* Devices a script has taken OUT of service with stream_wrapper_unregister().` |
|         - | 4599 | `	 * Held as DEVICE pointers rather than names, so a userland wrapper registered` |
|         - | 4600 | `	 * over an unregistered built-in coexists with it in the list above and is the` |
|         - | 4601 | `	 * one the lookup finds. */` |
|         - | 4602 | `	SySet aSuppressedIo;` |
|         - | 4603 | `	const ph7_io_stream *pDefStream; /* Default IO stream [i.e: typically this is the 'file://' stream] */` |
|         - | 4604 | `	ph7_value sExec;           /* Compiled script return value [Can be extracted via the PH7_VM_CONFIG_EXEC_VALUE directive]*/` |
|         - | 4605 | `	ph7_value sExceptionCB;    /* ACTIVE set_exception_handler() handler */` |
|         - | 4606 | `	ph7_value sErrCB;          /* ACTIVE set_error_handler() handler */` |
|         - | 4607 | `	sxi64 iErrCBLevels;        /* sErrCB's $error_levels mask: a handler is only called for the` |
|         - | 4608 | `	                            * levels it was REGISTERED for, and every other one falls` |
|         - | 4609 | `	                            * through to the engine's own reporting. Read at full width --` |
|         - | 4610 | `	                            * php ANDs a zend_long, so 2^32+1024 still selects` |
|         - | 4611 | `	                            * E_USER_NOTICE. */` |
|         - | 4612 | `	SySet aExceptionCBSaved;   /* VmHandlerSlot stack underneath sExceptionCB */` |
|         - | 4613 | `	SySet aErrCBSaved;         /* VmHandlerSlot stack underneath sErrCB */` |
|         - | 4614 | `	void *pStdin;              /* STDIN IO stream */` |
|         - | 4615 | `	void *pStdout;             /* STDOUT IO stream */` |
|         - | 4616 | `	void *pStderr;             /* STDERR IO stream */` |
|         - | 4617 | `	int bErrReport;            /* TRUE to report all runtime Error/Warning/Notice */` |
|         - | 4618 | `	int iDisplayErrors;        /* display_errors ini DESTINATION, not a gate: OFF emits no` |
|         - | 4619 | `` 	                            * DISPLAY copy, STDOUT emits `\nWarning: msg in F on line N` `` |
|         - | 4620 | `	                            * to the program output stream, STDERR emits the same sentence` |
|         - | 4621 | `	                            * WITHOUT the leading blank line to the error stream, outside` |
|         - | 4622 | `	                            * the output layer. php CLI default: off.` |
|         - | 4623 | `	                            * See PH7_VmDisplayErrorsMode() for the value table. */` |
|         - | 4624 | `	int bLogErrors;            /* log_errors ini gate: TRUE emits the LOG copy of a runtime` |
|         - | 4625 | ``	                            * diagnostic (`PHP Warning:  msg in F on line N`) to the error`` |
|         - | 4626 | `	                            * stream (stderr via sVmErrConsumer). php CLI default: on. */` |
|         - | 4627 | ``	SyBlob sErrLogPath;        /* `error_log` ini destination: the file every LOG copy and`` |
|         - | 4628 | `	                            * error_log()'s configured-logger types are appended to,` |
|         - | 4629 | `	                            * timestamped. EMPTY means unset, which is php's SAPI logger` |
|         - | 4630 | `	                            * -- the error stream. A path that will not open falls back` |
|         - | 4631 | `	                            * to that stream too, exactly as php's logger does. */` |
|         - | 4632 | `	int bGcEnabled;            /* gc_enable()/gc_disable(): whether the cycle collector may` |
|         - | 4633 | `	                            * buffer a possible root at all. Off means PHL frees by` |
|         - | 4634 | `	                            * reference count alone, which strands every cycle. */` |
|         - | 4635 | `	SySet aGcRoot;             /* Possible cycle roots: a container whose refcount dropped` |
|         - | 4636 | `	                            * without reaching zero. See vm_gc.c */` |
|         - | 4637 | `	SySet aGcWork;             /* Traversal worklist (VM-owned so a collection allocates` |
|         - | 4638 | `	                            * nothing per run) */` |
|         - | 4639 | `	SySet aGcAux;              /* ...and the one scan_black runs on, since it is entered` |
|         - | 4640 | `	                            * mid-drain of the primary */` |
|         - | 4641 | `	SySet aGcDead;             /* What the collect phase proved garbage */` |
|         - | 4642 | `	sxu8 bGcWanted;            /* The root buffer filled: collect at the next fetch point */` |
|         - | 4643 | `	sxu8 bGcRunning;           /* A collection is in flight; nothing may buffer or re-enter */` |
|         - | 4644 | `	SySet aDeadClosure;        /* Run-time closures whose last holder went: freed at the VM's` |
|         - | 4645 | `	                            * next fetch point rather than on the spot, because the drop` |
|         - | 4646 | `	                            * happens in the middle of a dispatch that is still about to` |
|         - | 4647 | `	                            * look the function up. See PH7_VmPurgeDeadClosures. */` |
|         - | 4648 | `	sxu8 bClosurePurge;        /* ...and whether that list has anything on it */` |
|         - | 4649 | `	sxu32 nGcThreshold;        /* Buffered roots that trigger a collection; adaptive (vm_gc.c) */` |
|         - | 4650 | `	sxu32 nGcRuns;             /* Collections run, for gc_status() */` |
|         - | 4651 | `	sxu32 nGcCollected;        /* Containers freed by them, for gc_status() */` |
|         - | 4652 | `	sxi32 iErrMask;      /* error_reporting() level. PH7 collapsed it to the bErrReport` |
|         - | 4653 | `	                      * boolean, so E_ALL & ~E_DEPRECATED still printed every` |
|         - | 4654 | `	                      * deprecation — any non-zero level meant "report all". */` |
|         - | 4655 | `	int bErrMaskSet;     /* Has anybody SAID what the level is? The main script's own` |
|         - | 4656 | `	                      * compile runs inside ph7_compile_file, which is what CREATES` |
|         - | 4657 | `	                      * the VM, so a diagnostic raised there is older than the host's` |
|         - | 4658 | `	                      * first ph7_vm_config() call and iErrMask is still zero — which` |
|         - | 4659 | ``	                      * a compile diagnostic must not read as `error_reporting(0)`.`` |
|         - | 4660 | `	                      * Set by every door that writes iErrMask, never cleared. */` |
|         - | 4661 | `	int nRecursionDepth;       /* Current PHP call depth (OP_CALL frames only) */` |
|         - | 4662 | `	int nErrSuppress;          /* '@' error-control depth: >0 means the diagnostics raised` |
|         - | 4663 | `	                            * while evaluating the suppressed expression are not printed` |
|         - | 4664 | `	                            * (a user error handler is still invoked, as in php). Nests. */` |
|         - | 4665 | `	int nMaxDepth;             /* Maximum PHP call depth; 0 == unbounded (the host` |
|         - | 4666 | `	                            * default: PHP frames are heap-bound since the` |
|         - | 4667 | `	                            * iterative executor, so recursion is limited by` |
|         - | 4668 | `	                            * memory like the main PHP engine). Embedders opt in` |
|         - | 4669 | `	                            * via PH7_VM_CONFIG_RECURSION_DEPTH. */` |
|         - | 4670 | `	int nVmExecDepth;          /* Live native VmByteCodeExec activations (C-stack guard;` |
|         - | 4671 | `	                            * see the VmByteCodeExec wrapper in vm.c) */` |
|         - | 4672 | `	int nMaxNativeDepth;       /* Maximum native VmByteCodeExec nesting (mini-programs,` |
|         - | 4673 | `	                            * C->PHP callbacks, ctx start/resume, eval/include) —` |
|         - | 4674 | `	                            * what actually protects the C stack now that PHP` |
|         - | 4675 | `	                            * recursion is iterative. Platform-sized default,` |
|         - | 4676 | `	                            * PH7_VM_CONFIG_NATIVE_DEPTH overrides. */` |
|         - | 4677 | `	void *pIdleCallFrames;     /* Freelist of VmCallFrame nodes (BYTECODE stage 2):` |
|         - | 4678 | `	                            * fixed-size, strictly LIFO per invocation — reusing` |
|         - | 4679 | `	                            * them skips a pool alloc/free round-trip per PHP` |
|         - | 4680 | `	                            * call (the measured trampoline overhead). Backing` |
|         - | 4681 | `	                            * memory is allocator-owned; freed wholesale. */` |
|         - | 4682 | `	/* Freelists of recycled operand-stack buffers (BYTECODE stage 7): a returning PHP` |
|         - | 4683 | `	 * call recycles its (tight-sized) operand stack here instead of freeing it, so a` |
|         - | 4684 | `	 * same-size call reuses it -- skipping the buffer alloc AND the per-slot init.` |
|         - | 4685 | `	 * Bounded by an entry count AND a total-slot budget; buffers are plain allocator` |
|         - | 4686 | `	 * blocks so cold/suspend/abort paths can still raw-free them.` |
|         - | 4687 | `	 *` |
|         - | 4688 | `	 * KEYED BY SIZE, because only an EXACT size is reusable. One list held every` |
|         - | 4689 | `	 * parked buffer and every call walked it looking for its own size: with the cap` |
|         - | 4690 | `	 * at 256 buffers that walk was 2.2% of a phpcs run, spent almost entirely on` |
|         - | 4691 | `	 * sizes the caller was never going to take. The size picks the chain now, so a` |
|         - | 4692 | `	 * call compares against the handful of buffers whose size ends in the same six` |
|         - | 4693 | `	 * bits instead of against all of them. */` |
|         - | 4694 | `	void *apIdleOperandStack[PH7_STACK_POOL_BUCKETS];` |
|         - | 4695 | `	int nIdleOperandStacks;    /* Buffers parked across every chain (cap: VM_STACK_POOL_MAX) */` |
|         - | 4696 | `	sxu32 nIdleOperandSlots;   /* Slots parked across those buffers. The pool's real cost is` |
|         - | 4697 | `	                            * memory, not entries, so this -- not the entry count alone --` |
|         - | 4698 | `	                            * is what bounds it (VM_STACK_POOL_SLOTS). */` |
|         - | 4699 | `	void *pIdleStackNodes;     /* Freelist of spare VmIdleStack nodes (BYTECODE stage 7b):` |
|         - | 4700 | `	                            * reused across recycle/reuse cycles so a parked buffer's` |
|         - | 4701 | `	                            * wrapper node isn't pool-alloc/freed per call (mirrors` |
|         - | 4702 | `	                            * pIdleCallFrames). Allocator-owned; freed wholesale. */` |
|         - | 4703 | `	int nObDepth;              /* Output handlers currently running (0 outside one) */` |
|         - | 4704 | `	sxu32 nObActive;           /* 1-based index of the buffer whose handler is running` |
|         - | 4705 | `	                            * (0 outside one). php truncates the ob stack at that` |
|         - | 4706 | `	                            * buffer for the duration: ob_get_level()/contents()/` |
|         - | 4707 | `	                            * length()/list_handlers() answer for IT, not for` |
|         - | 4708 | `	                            * whatever is stacked above it. */` |
|         - | 4709 | `	int bConstEnum;            /* Expanding constants to DESCRIBE them` |
|         - | 4710 | `	                            * (get_defined_constants): php reports a deprecated` |
|         - | 4711 | `	                            * constant when it is READ, and listing the table is` |
|         - | 4712 | `	                            * not a read. */` |
|         - | 4713 | `	int bObRefused;            /* An ob call refused from inside a handler ended the` |
|         - | 4714 | `	                            * request: that operation delivers nothing more. */` |
|         - | 4715 | `	int bObRaising;            /* The "producing output" deprecation is being raised: its` |
|         - | 4716 | `	                            * handler is still running in php's eyes, so whatever the` |
|         - | 4717 | `	                            * display or a user error handler prints marks the buffer` |
|         - | 4718 | `	                            * it lands in (VmObSink). */` |
|         - | 4719 | `	VmFrame *pObFrame;         /* Frame that CALLED the running output handler. The` |
|         - | 4720 | `	                            * handler's own body runs in a deeper frame, so` |
|         - | 4721 | ``	                            * `nObDepth > 0 && pFrame != pObFrame` is "we are`` |
|         - | 4722 | `	                            * inside the handler" — and it stays false for the` |
|         - | 4723 | `	                            * in-place catch PHL runs, in the caller's frame,` |
|         - | 4724 | `	                            * when the handler throws. */` |
|         - | 4725 | `	int nExceptDepth;          /* Exception depth */` |
|         - | 4726 | `	int nExcCtorDepth;         /* Engine-raised throws whose exception __construct is running` |
|         - | 4727 | `	                            * (VmExcCtorEnter): caps the self-feeding case where building` |
|         - | 4728 | `	                            * an exception throws again. */` |
|         - | 4729 | `	sxu32 nLazyInitLine;       /* While a LAZY class initializer runs (a static property's` |
|         - | 4730 | `	                            * deferred default, a class constant's on-demand evaluation):` |
|         - | 4731 | `	                            * the line of the ACCESS that triggered it. A Throwable born` |
|         - | 4732 | `	                            * in the initializer's OWN bytecode is stamped with THIS line` |
|         - | 4733 | `	                            * rather than the initializer's, because that is where php` |
|         - | 4734 | `	                            * evaluates the expression. 0 = not in one, or the access site` |
|         - | 4735 | `	                            * was internal (prelude) code whose line means nothing in the` |
|         - | 4736 | `	                            * file the stamp names. See PH7_VmStampThrowableSite. */` |
|         - | 4737 | `	sxu32 nArgSiteLine;        /* ONE-SHOT site for the next Throwable stamped: an argument` |
|         - | 4738 | `	                            * refusal is raised inside the callee, on the refused` |
|         - | 4739 | `	                            * parameter's declaration line and in the callee's file, even` |
|         - | 4740 | `	                            * where the binder runs before the callee's frame exists (a` |
|         - | 4741 | `	                            * fiber body, a generator). Set by VmArgSiteArm, cleared by` |
|         - | 4742 | `	                            * the stamp that consumes it and by the thrower after. */` |
|         - | 4743 | `	const SyString *pArgSiteFile; /* ...and that file; 0 or empty = the stamp's own answer */` |
|         - | 4744 | `	sxi32 nLazyInitDepth;      /* nVmExecDepth of that initializer's own activation. The` |
|         - | 4745 | `	                            * override applies at THIS depth only: anything the` |
|         - | 4746 | `	                            * initializer manages to call — an autoloader, a nested` |
|         - | 4747 | `	                            * constant's evaluation — runs its own lines and keeps them. */` |
|         - | 4748 | `	int nMuteThrow;            /* > 0 while an initializer runs MUTED (VmEvalDefaultMuted): an` |
|         - | 4749 | `	                            * uncaught throw runs no exception handler, prints no report and` |
|         - | 4750 | `	                            * leaves iExitStatus alone, because php has not reached that code` |
|         - | 4751 | `	                            * yet. Depth-counted (an initializer can mount another class). */` |
|         - | 4752 | `	int nSpeculative;          /* > 0 while a program is run only to LOOK at the value it would` |
|         - | 4753 | `	                            * produce (PH7_VmEvalConstExpr, which renders a parameter default` |
|         - | 4754 | `	                            * for a declaration message). php's own compiler folds such an` |
|         - | 4755 | `	                            * expression and gives up the moment evaluating it raises` |
|         - | 4756 | `	                            * ANYTHING, so nothing raised here may be observable: no user` |
|         - | 4757 | `	                            * error handler runs, no error_get_last() record is written and` |
|         - | 4758 | `	                            * nothing is printed. Depth-counted like nMuteThrow, which mutes` |
|         - | 4759 | `	                            * the THROW half of the same window. */` |
|         - | 4760 | `	sxu32 nSpecDiag;           /* Diagnostics dropped by nSpeculative, monotonic. A speculative` |
|         - | 4761 | `	                            * evaluation that moved this counter is one php would not have` |
|         - | 4762 | ``	                            * folded, so its caller renders php's `<expression>` instead. */`` |
|         - | 4763 | `	int closure_cnt;           /* Loaded closures counter */` |
|         - | 4764 | `	int json_rc;               /* JSON return status [refer to json_encode()/json_decode()]*/` |
|         - | 4765 | `	sxi32 iLcgS1;              /* php's combined LCG, the generator behind uniqid()'s $more_entropy` |
|         - | 4766 | `	                            * tail (and php's own lcg_value()). Two L'Ecuyer streams whose` |
|         - | 4767 | `	                            * DIFFERENCE is the answer; seeded lazily from the clock and the` |
|         - | 4768 | `	                            * engine's own entropy, once per VM, the way php seeds its pair` |
|         - | 4769 | `	                            * once per process. */` |
|         - | 4770 | `	sxi32 iLcgS2;` |
|         - | 4771 | `	int bLcgSeeded;            /* ...and whether that has happened yet */` |
|         - | 4772 | `	sxu32 nNextObjId;          /* Next object handle id to hand out (monotonic; reset to 1 per exec` |
|         - | 4773 | `	                            * so a reused VM looks like a fresh process). See ph7_class_instance.nObjId */` |
|         - | 4774 | `	ProcErrLog xErrLog;        /* error_log() consumer [refer to PH7_VM_CONFIG_ERR_LOG_HANDLER] */` |
|         - | 4775 | `	sxu32 nOutputLen;          /* Total number of generated output */` |
|         - | 4776 | `	ph7_output_consumer sVmConsumer; /* Registered output consumer callback */` |
|         - | 4777 | `	ph7_output_consumer sVmErrConsumer; /* Diagnostics (stderr) consumer [PH7_VM_CONFIG_ERR_STREAM].` |
|         - | 4778 | `	                            * When xConsumer is 0 the log copy falls back to sVmConsumer so` |
|         - | 4779 | `	                            * embedders that never wire a stderr stream still see diagnostics. */` |
|         - | 4780 | `	int iAssertFlags;          /* Assertion flags */` |
|         - | 4781 | `	ph7_value sAssertCallback; /* Callback to call on failed assertions */` |
|         - | 4782 | `	void **apRefObj;           /* Reference WORD per memory-object slot, INDEXED BY SLOT:` |
|         - | 4783 | `	                            * apRefObj[nIdx] describes the holders of aMemObj[nIdx], or` |
|         - | 4784 | `	                            * is 0 when nothing has ever been registered against it. A` |
|         - | 4785 | `	                            * slot index is already a dense small integer, so hashing it` |
|         - | 4786 | `	                            * bought nothing and cost a rehash of every record each time` |
|         - | 4787 | `	                            * the table doubled. See VM_REF_TAG_* for what a word says --` |
|         - | 4788 | `	                            * nearly every slot's answer fits in the word itself and` |
|         - | 4789 | `	                            * allocates no record at all. */` |
|         - | 4790 | `	sxu32 nRefSize;            /* apRefObj[] length, in slots */` |
|         - | 4791 | `	sxu32 nRefUsed;            /* Cells currently filled (a word or a record) */` |
|         - | 4792 | `	SySet aSelf;               /* 'self' stack used for static member access [i.e: self::MyConstant] */` |
|         - | 4793 | `	ph7_hashmap *pGlobal;      /* $GLOBALS hashmap */` |
|         - | 4794 | `	sxu32 nGlobalIdx;          /* $GLOBALS index */` |
|         - | 4795 | `	SySet aCallSite;           /* VmCallSite -- one per PH7_OP_CALL site that has run, holding` |
|         - | 4796 | `	                            * the function-table entry its callee name resolved to. Indexed` |
|         - | 4797 | `	                            * by VmInstr.nSite - 1, and claimed only by a site that actually` |
|         - | 4798 | `	                            * executes. */` |
|         - | 4799 | `	SySet aUnitDecl;           /* VmUnitDecl -- what the eval()/include compiles in flight have` |
|         - | 4800 | `	                            * installed, so a unit that fails to compile can take its own` |
|         - | 4801 | `	                            * declarations back out (VmEvalChunk). Logged only while` |
|         - | 4802 | `	                            * bUnitDecl is set: never the main script, never at run time. */` |
|         - | 4803 | `	int bUnitDecl;             /* 1 while VmEvalChunk is compiling a unit */` |
|         - | 4804 | `	SySet aHiddenClass;        /* ph7_class* -- the PH7_CLASS_LATEBIND classes the compiles in flight` |
|         - | 4805 | `	                            * have linked, hidden when their unit finishes (PH7_VmHideClasses). */` |
|         - | 4806 | `	SyHash hCallName;          /* The callee names aCallSite records point at, interned. 43,375` |
|         - | 4807 | `	                            * call sites execute on the ecosystem gate's phpcs step and they` |
|         - | 4808 | `	                            * spell only a few thousand distinct names between them, so a` |
|         - | 4809 | `	                            * copy per SITE was 2.8 MB where a copy per NAME is a fifth of` |
|         - | 4810 | `	                            * one -- and the shared copy is the one already in cache when` |
|         - | 4811 | `	                            * the next site checks its own record. Keyed by the name BYTES` |
|         - | 4812 | `	                            * (case-sensitively: a site spells its callee the same way every` |
|         - | 4813 | `	                            * time), and the entry's key IS the interned copy. */` |
|         - | 4814 | `	sxu32 nFreeCallSite;       /* Head of aCallSite's free list (index + 1, 0 = empty). An` |
|         - | 4815 | `	                            * eval()/include compiles into a bytecode container that is` |
|         - | 4816 | `	                            * RELEASED when the chunk finishes, so the records its call` |
|         - | 4817 | `` 	                            * sites claimed go back here -- without it, `while(1) eval(...)` `` |
|         - | 4818 | `	                            * would grow aCallSite for ever. */` |
|         - | 4819 | `	sxu32 nCallableGen;        /* Bumped whenever the set of things a NAME can call changes --` |
|         - | 4820 | `	                            * a function, a class or a host function installed or removed.` |
|         - | 4821 | `	                            * PH7_OP_CALL_INIT stamps a call site it has screened with the` |
|         - | 4822 | `	                            * generation it screened at, so a site whose callee is a` |
|         - | 4823 | `	                            * compile-time constant asks the question once per generation` |
|         - | 4824 | `	                            * instead of once per call. Starts at 1: 0 is 'never screened'. */` |
|         - | 4825 | `	sxu32 nRunGen;             /* Bumped by every PH7_VmReset: which run of a reused VM` |
|         - | 4826 | `	                            * installed a constant (ph7_constant.nRunGen) */` |
|         - | 4827 | `	sxu32 nConstGen;           /* The same idea for the CONSTANT table: bumped whenever a name` |
|         - | 4828 | `	                            * is installed in or removed from hConstant. A PH7_OP_LOADC` |
|         - | 4829 | `	                            * site's answer can only change then -- both the constant it` |
|         - | 4830 | `	                            * resolved to and, for a namespaced site, WHICH of its two` |
|         - | 4831 | `	                            * candidate names won -- so a site stamped with this generation` |
|         - | 4832 | `	                            * skips the lookups. Starts at 1: 0 is 'never resolved'. */` |
|         - | 4833 | `	sxu32 nCurLine;            /* Line of the instruction currently executing (0 outside the` |
|         - | 4834 | `	                            * dispatch loop). Every runtime diagnostic, debug_backtrace()` |
|         - | 4835 | `	                            * and Throwable reads its line from here. */` |
|         - | 4836 | `	sxu8 bCurStrict;           /* strict_types mode of the unit that instruction came from,` |
|         - | 4837 | `	                            * published beside nCurLine. Read by the argument binder when` |
|         - | 4838 | `	                            * an OP_CALL carries no compiled call map — which is every` |
|         - | 4839 | `	                            * ENGINE-dispatched call (magic method, property hook), where` |
|         - | 4840 | `	                            * php still applies the calling file's mode. */` |
|         - | 4841 | `	sxi32 nLastErrType;        /* error_get_last(): severity of the last UNHANDLED diagnostic` |
|         - | 4842 | `	                            * (0 = none yet). php records one even when '@' or` |
|         - | 4843 | `	                            * error_reporting() hides it, but NOT when a user handler` |
|         - | 4844 | `	                            * claimed it by returning true. */` |
|         - | 4845 | `	sxu32 nLastErrLine;        /* ... its line */` |
|         - | 4846 | `	SyBlob sLastErrMsg;        /* ... its message */` |
|         - | 4847 | `	SyBlob sLastErrFile;       /* ... its file */` |
|         - | 4848 | `	char zDisplayName[256];    /* Scratch for PH7_VmFuncDisplayName: a closure's INTERNAL name is a` |
|         - | 4849 | `	                            * synthesized unique key ("[closure_3]"), but php shows` |
|         - | 4850 | `	                            * "{closure:file:line}". Valid until the next call. */` |
|         - | 4851 | `	sxu32 nSuperBaseline;      /* SySetUsed(aMemObj) snapshot taken in PH7_VmMakeReady` |
|         - | 4852 | `								* right before the superglobals are created. ph7_vm_reset()` |
|         - | 4853 | `								* releases and truncates aMemObj back to this watermark then` |
|         - | 4854 | `								* rebuilds the per-exec object graph, so a compiled VM can be` |
|         - | 4855 | `								* re-executed (compile-once / execute-many) without state` |
|         - | 4856 | `								* bleed or unbounded heap growth. */` |
|         - | 4857 | `	/* Index of the shared empty-string literal reserved at VM init */` |
|         - | 4858 | `	sxu32 nEmptyStringIdx;` |
|         - | 4859 | `	/* Argument-unpacking capture (PHP 8.1 named-parameter semantics for spreads).` |
|         - | 4860 | `	 * Populated by OP_SPREAD; CALL/NEW derive each call's own arg-count growth from` |
|         - | 4861 | `	 * these runs (VmSpreadOwnExtra) and replay the keys (VmBuildEffectiveArgMap),` |
|         - | 4862 | `	 * then consume this call's runs. See the VmSpreadRun/VmSpreadKey machinery in vm.c. */` |
|         - | 4863 | `	SySet aSpreadRun;          /* VmSpreadRun: one entry per expansion in the current arg list */` |
|         - | 4864 | `	sxu32 nSpreadCallBase;     /* Index into aSpreadRun of the first run owned by the CALL/NEW` |
|         - | 4865 | `	                            * currently dispatching (VmSpreadOwnExtra records it; the replay` |
|         - | 4866 | `	                            * and consume use it instead of an ambiguous pStart scan, which a` |
|         - | 4867 | ``	                            * zero-width `...[]` run sharing a nested call's base slot fooled) */`` |
|         - | 4868 | `	SySet aSpreadKey;          /* VmSpreadKey: one (off,len) per expanded element, in order */` |
|         - | 4869 | `	SyBlob sSpreadKeyBlob;     /* Backing bytes for the string keys referenced by aSpreadKey */` |
|         - | 4870 | `	SySet aEffArgName;         /* SyString: effective per-actual-slot arg names built at CALL */` |
|         - | 4871 | `	SySet aEffArgRun;          /* sxu32: the written argument each effective slot came from */` |
|         - | 4872 | `	const char *zCmpRefusalClass; /* A native compare handler (ph7_class::xCmp) REFUSED the pair,` |
|         - | 4873 | `	                            * and this is the exception class it named -- php throws` |
|         - | 4874 | `	                            * DateException out of the DateTimeZone handler. Recorded rather` |
|         - | 4875 | `	                            * than raised because PH7_MemObjCmp has no throw boundary: it runs` |
|         - | 4876 | `	                            * under sort(), in_array() and max() as often as under an operator.` |
|         - | 4877 | `	                            * The sites that DO have one (the comparison opcodes, the switch` |
|         - | 4878 | `	                            * arm, the host-call boundary) raise it through` |
|         - | 4879 | `	                            * PH7_CmpRefusalRaise. FIRST refusal wins, like nBoundaryRc: a` |
|         - | 4880 | `	                            * driver that keeps comparing after one must not overwrite the` |
|         - | 4881 | `	                            * message the script will see. 0 when none is pending. */` |
|         - | 4882 | `	char zCmpRefusalMsg[160];  /* ...and its wording, copied out of the hook's context */` |
|         - | 4883 | `	sxi32 iCmpCallbackExc;     /* The dispatch STATUS a comparison callback did not return with` |
|         - | 4884 | `								* (PH7_EXCEPTION, or PH7_ABORT for an UNCAUGHT throw), so the` |
|         - | 4885 | `								* driver (usort/uasort/uksort and the array_udiff/` |
|         - | 4886 | `								* array_uintersect families) can abort and propagate exactly` |
|         - | 4887 | `								* it. Zero when no comparison raised; a comparator has no` |
|         - | 4888 | `								* status channel, so this latch is the only way out. */` |
|         - | 4889 | `	int bCmpBoolRaised;        /* php's compare_deprecation_thrown: the "Returning bool from` |
|         - | 4890 | `								* comparison function" deprecation was raised by the running` |
|         - | 4891 | `								* sort or diff/intersect merge. Cleared at the ENTRY of every` |
|         - | 4892 | `								* such builtin and never restored, so a comparator that runs` |
|         - | 4893 | `								* another usort() re-arms the outer one, as php's does. */` |
|         - | 4894 | `	int iMbEncoding;           /* mbstring's internal encoding, an MB_ENC_* id from` |
|         - | 4895 | `								* builtin_mb.c; 0 is UTF-8, which is why zeroing the` |
|         - | 4896 | `								* VM leaves php's default in place. */` |
|         - | 4897 | `	sxu8 aMbDetectOrder[8];    /* mbstring's DETECT ORDER, as builtin_mb.c detect ids. It is` |
|         - | 4898 | ``	                            * what `mb_detect_encoding($s)` walks with no list of its`` |
|         - | 4899 | ``	                            * own, and what `mb_detect_order()` reads and writes. NOT`` |
|         - | 4900 | ``	                            * what the name `auto` means: that one is the LANGUAGE's`` |
|         - | 4901 | `	                            * default order (ASCII, UTF-8) whatever this holds --` |
|         - | 4902 | `	                            * probed, because the two read alike in the default state` |
|         - | 4903 | `	                            * and only diverge once a script has set an order. */` |
|         - | 4904 | `	sxu8 nMbDetectOrder;       /* how many of them; 0 at VM init means the default pair. */` |
|         - | 4905 | `	sxi32 iMbSubstitute;       /* mbstring's substitute code point ('?' at VM init;` |
|         - | 4906 | `								* 0 is a code point a script may really ask for). */` |
|         - | 4907 | `	sxu8 iMbSubstMode;         /* how it is written: builtin_mb.c's MB_SUBST_* — the` |
|         - | 4908 | `								* code point itself, nothing at all, or the U+/entity` |
|         - | 4909 | `								* spelling of what could not be represented. php keeps` |
|         - | 4910 | `								* the two apart, so setting "long" does not forget the` |
|         - | 4911 | `								* code point an error character still takes. */` |
|         - | 4912 | `	sxi32 iExitStatus;         /* Script exit status */` |
|         - | 4913 | `	sxu8 bHaltRequested;       /* Set by exit/die (OP_HALT or the builtin) so the halt` |
|         - | 4914 | `								* cascades out of nested execution units (include/require/` |
|         - | 4915 | `								* eval chunks) instead of hard-exiting the process; the` |
|         - | 4916 | `								* top-level executor then runs shutdown callbacks normally. */` |
|         - | 4917 | `	sxu8 bInReset;             /* Set while ph7_vm_reset() bulk-releases the per-exec` |
|         - | 4918 | `								* object pool. Suppresses user __destruct invocation during` |
|         - | 4919 | `								* that teardown: destructors would run arbitrary PHP against a` |
|         - | 4920 | `								* half-reset VM (reference table already gone, $GLOBALS` |
|         - | 4921 | `								* nulled). PH7 never ran` |
|         - | 4922 | `								* global-scope destructors before (release nuked the arena),` |
|         - | 4923 | `								* so this preserves prior semantics while staying crash-safe.` |
|         - | 4924 | `								* Engine-level instance memory is still reclaimed. */` |
|         - | 4925 | `	sxu8 bNoFrameLoc;          /* Set around a diagnostic raised with NO php frame under it.` |
|         - | 4926 | `								* php then has no file and no line to name and reports the` |
|         - | 4927 | ``								* location as `in Unknown on line 0` (its`` |
|         - | 4928 | `								* EG(current_execute_data) == NULL branch). See` |
|         - | 4929 | `								* VmDiagnosticWhere. */` |
|         - | 4930 | `	sxu8 bShutdownAborted;     /* Set when a destructor in the shutdown pass left an uncaught` |
|         - | 4931 | `								* throwable. php's phase runs under one zend_try, so the first` |
|         - | 4932 | `								* bailout abandons every destructor still owed -- the flag is` |
|         - | 4933 | `								* what carries that decision across the two passes. */` |
|         - | 4934 | `	sxu8 bInShutdownDtor;      /* Set while the shutdown destructor pass runs (php's` |
|         - | 4935 | `								* zend_call_destructors, between the shutdown callbacks and` |
|         - | 4936 | `								* the output-buffer flush). php reads this state as` |
|         - | 4937 | ``								* `EG(current_execute_data) == NULL`: a non-public __destruct`` |
|         - | 4938 | `								* reached with no PHP frame on the stack is not the Error a` |
|         - | 4939 | `								* running program gets but an E_WARNING that says the call was` |
|         - | 4940 | `								* ignored, and the object is left undestructed. */` |
|         - | 4941 | `	ph7_gen_state sCodeGen;    /* Code generator module */` |
|         - | 4942 | `	sxu32 nLastEvalErr;        /* Compile-error count of the most recent VmEvalChunk unit. Unlike` |
|         - | 4943 | `								* sCodeGen.nErr it survives the nested-compile state save/restore,` |
|         - | 4944 | `								* so VmExecDeferredClass can tell whether ITS chunk failed even` |
|         - | 4945 | `								* when the deferred declaration executes inside an outer compile` |
|         - | 4946 | `								* (an autoload-during-compile require). */` |
|         - | 4947 | `	sxu32 nAnonSeq;            /* Anonymous-class sequence number, appended to the synthesized` |
|         - | 4948 | ``								* name as `$%x`. php's CG(rtd_key_counter): one counter for the`` |
|         - | 4949 | `								* whole request, bumped in COMPILE order, which is what makes` |
|         - | 4950 | `								* two anonymous classes written on the same line distinguishable. */` |
|         - | 4951 | `	int bDeclQuietNext;        /* One-shot: the next nested compile unit is a checked deferred` |
|         - | 4952 | `								* declaration's re-compile (see ph7_gen_state.bDeclQuiet). */` |
|         - | 4953 | `	SyString sDeferAnonName;   /* One-shot synthesized-name override for the next anonymous-class` |
|         - | 4954 | `								* compile: set by VmExecDeferredClass before re-compiling a` |
|         - | 4955 | ``								* deferred `new class ... {}` chunk so the runtime-installed`` |
|         - | 4956 | `								* class carries the SAME name the site's OP_NEW loads; consumed` |
|         - | 4957 | `								* (cleared) by PH7_CompileAnnonClass. {0,0} otherwise. */` |
|         - | 4958 | `	int nReflectFactory;       /* > 0 while a ReflectionClass door is BUILDING a member reflector` |
|         - | 4959 | `	                            * rather than a user calling its constructor. php's own doors do` |
|         - | 4960 | `	                            * not go through ReflectionMethod::__construct at all -- they are` |
|         - | 4961 | `	                            * handed the function directly -- which is why` |
|         - | 4962 | ``	                            * `(new ReflectionClass('Closure'))->getMethod('__invoke')` answers`` |
|         - | 4963 | ``	                            * and `new ReflectionMethod('Closure','__invoke')` refuses. Here`` |
|         - | 4964 | `	                            * both spellings reach the same constructor, so this is what tells` |
|         - | 4965 | `	                            * them apart. Depth-counted; nothing user-visible runs inside the` |
|         - | 4966 | `	                            * window (the constructor it brackets is native). */` |
|         - | 4967 | `	ph7_exec_ctx *pActiveCtx;  /* Currently executing fiber/generator context (NULL in normal code) */` |
|         - | 4968 | `#ifdef PH7_CORO_STACK` |
|         - | 4969 | `	ph7_exec_ctx *pCoroCtx;    /* The fiber whose own native STACK is the one executing, or NULL.` |
|         - | 4970 | `	                            * Unlike pActiveCtx this does not change when the fiber's body` |
|         - | 4971 | `	                            * drives a generator, so it answers the one question the throw` |
|         - | 4972 | `	                            * path asks: is there a fiber to leave? Travels with the` |
|         - | 4973 | `	                            * VM-state swap, so nesting one fiber inside another restores` |
|         - | 4974 | `	                            * the outer one by construction. */` |
|         - | 4975 | `#endif` |
|         - | 4976 | `	ph7_class_instance *pCurFiber; /* The Fiber whose body the running code is inside, or NULL --` |
|         - | 4977 | `	                            * php's EG(active_fiber), which is what Fiber::getCurrent()` |
|         - | 4978 | `	                            * answers. Distinct from pActiveCtx: that one is whatever` |
|         - | 4979 | `	                            * coroutine is executing (a GENERATOR started inside a fiber` |
|         - | 4980 | `	                            * is the active ctx while the fiber is still the current one),` |
|         - | 4981 | `	                            * and it names no object. Saved and restored around a fiber's` |
|         - | 4982 | `	                            * start/resume, so nesting is the call structure itself. */` |
|         - | 4983 | `	ph7_class *pFiberClass;    /* Cached Fiber class pointer for fast dispatch */` |
|         - | 4984 | `	ph7_class *pGeneratorClass; /* Cached Generator class pointer */` |
|         - | 4985 | `	ph7_class *pClosureClass;  /* Cached Closure class pointer (closures are instances of it) */` |
|         - | 4986 | `	ph7_class_instance *pClosureThis; /* Transient: bound $this for a bound PLAIN closure about to be` |
|         - | 4987 | `	                                   * invoked, set by VmClosureUnwrap, consumed (ref transferred) at` |
|         - | 4988 | `	                                   * the OP_CALL user-function frame setup. Owns one reference. */` |
|         - | 4989 | `	ph7_class *pClosureScope; /* Transient: bound $__scope class for the same bound PLAIN closure` |
|         - | 4990 | `	                           * (private/protected visibility override); consumed alongside pClosureThis. */` |
|         - | 4991 | `	int bClosureUnbound;      /* Transient, same lifetime as pClosureScope: the PLAIN closure about to be` |
|         - | 4992 | ``	                           * invoked is a rebind's clone with no `$this`, so the receiver its`` |
|         - | 4993 | `	                           * function captured where it was CREATED is not installed. php drops it` |
|         - | 4994 | ``	                           * with the unbind (`bindTo(null)`); the capture lives on the shared`` |
|         - | 4995 | `	                           * per-instantiation function, which the original closure still runs on.` |
|         - | 4996 | `	                           * PH7_CLOSURE_UNSCOPED says the rebind dropped its class scope as well. */` |
|         - | 4997 | `	ph7_class *pClosureMethodCls; /* Consume-once: the class a METHOD closure's callee is looked up in` |
|         - | 4998 | ``	                               * when that is not its receiver's own -- `parent::m(...)`,`` |
|         - | 4999 | ``	                               * `A::m(...)`, ReflectionMethod::getClosure(). php keeps the resolved`` |
|         - | 5000 | `	                               * function, so the call must not find the receiver's override. Armed` |
|         - | 5001 | `	                               * by VmClosureUnwrap, read by the pair dispatch` |
|         - | 5002 | `	                               * (PH7_VmCallUserFunctionWithMap) and the fiber body-finder, cleared` |
|         - | 5003 | `	                               * with bClosureScreened. Owns no reference. */` |
|         - | 5004 | `	ph7_class *pStdClass;      /* Cached stdClass pointer (target of (object) cast + dynamic props) */` |
|         - | 5005 | `	ph7_class *pIncClass;      /* Cached __PHP_Incomplete_Class pointer: unserialize()'s carrier for a` |
|         - | 5006 | `	                            * disallowed or unknown class. Every script-level property access or` |
|         - | 5007 | `	                            * method call on an instance is php's incomplete-object diagnostic` |
|         - | 5008 | `	                            * (PH7_VmIncompleteMsg); the engine itself reads hAttr freely. */` |
|         - | 5009 | `	ph7_class *pArrayAccessClass; /* Cached ArrayAccess interface pointer */` |
|         - | 5010 | `	ph7_class *pCountableClass;   /* Cached Countable interface pointer */` |
|         - | 5011 | `	ph7_class *pStringableClass;  /* Cached Stringable interface pointer */` |
|         - | 5012 | `	ph7_class *pJsonSerializableClass; /* Cached JsonSerializable interface pointer */` |
|         - | 5013 | `	ph7_class *pTraversableClass; /* Cached Traversable interface pointer (iterable type check) */` |
|         - | 5014 | `	/* Pending null-coalesce-assign target on an ArrayAccess subscript.` |
|         - | 5015 | `	 * Set by LOAD_IDX iP2=3 when the key is missing on an ArrayAccess` |
|         - | 5016 | `	 * object; consumed by NULLC_STORE so it can dispatch to offsetSet` |
|         - | 5017 | `	 * instead of writing through the (synthetic) pNos->nIdx. NULLC_STORE` |
|         - | 5018 | `	 * always clears it, matched or not. */` |
|         - | 5019 | `	ph7_class_instance *pCoalesceObj;` |
|         - | 5020 | `	ph7_value sCoalesceKey;` |
|         - | 5021 | `	int bCoalesceArmed;` |
|         - | 5022 | `#ifdef PH7_ENABLE_PCRE` |
|         - | 5023 | `	int iPcreLastError;        /* preg_last_error() return value */` |
|         - | 5024 | `	/* mbstring's regex family (vm_pcre.c). Every field reads as php's default` |
|         - | 5025 | `	 * when it is ZERO, so a freshly zeroed VM already answers "UTF-8" and "pr"` |
|         - | 5026 | `	 * and needs no init hook of its own: iMbReOpt carries MBRE_OPT_SET once a` |
|         - | 5027 | `	 * script has set options, iMbReSyntax holds the syntax letter and 0 means` |
|         - | 5028 | `	 * 'r', and iMbReEnc is an index into builtin_mb.c's encoding table whose` |
|         - | 5029 | `	 * entry 0 is UTF-8. The search state is allocated out of the VM allocator,` |
|         - | 5030 | `	 * so it goes away with the VM. */` |
|         - | 5031 | `	sxu32 iMbReOpt;            /* mb_regex_set_options() bits, 0 = untouched */` |
|         - | 5032 | `	sxu8 iMbReSyntax;          /* its syntax letter, 0 = 'r' */` |
|         - | 5033 | `	int iMbReEnc;              /* mb_regex_encoding(), an encoding-table index */` |
|         - | 5034 | `	char *zMbReStr;            /* mb_ereg_search_init()'s subject, 0 = none set */` |
|         - | 5035 | `	sxu32 nMbReStr;` |
|         - | 5036 | `	char *zMbRePat;            /* ...and its pattern, 0 = none set */` |
|         - | 5037 | `	sxu32 nMbRePat;` |
|         - | 5038 | `	sxu32 iMbReOptCur;         /* the options that pattern was set with */` |
|         - | 5039 | `	sxu8 iMbReSynCur;` |
|         - | 5040 | `	sxu32 iMbRePos;            /* the search cursor, a BYTE offset */` |
|         - | 5041 | `	sxu32 *aMbReOv;            /* the last match's offsets, 2 per group */` |
|         - | 5042 | `	int nMbReOv;               /* how many groups are in there; 0 = no match yet */` |
|         - | 5043 | `#endif` |
|         - | 5044 | `#ifdef PH7_ENABLE_LIBXML` |
|         - | 5045 | `	SySet aLibxmlErr;          /* Queued phl_libxml_err entries (libxml_get_errors) */` |
|         - | 5046 | `	SyBlob sLibxmlPend;        /* libxml message text held back because it has no trailing` |
|         - | 5047 | `	                            * newline: php buffers such a fragment and prints it JOINED` |
|         - | 5048 | `	                            * to the next diagnostic, whenever that arrives (see` |
|         - | 5049 | `	                            * PH7_LibxmlCaptureEnd). Reset per request. */` |
|         - | 5050 | `	int bLibxmlInternalErr;    /* libxml_use_internal_errors(true) is active */` |
|         - | 5051 | `	void *pLibxmlLastErr;      /* phl_libxml_err* slot backing libxml_get_last_error */` |
|         - | 5052 | `	void *pXmlDocs;            /* phl_xmldoc registry chain; freed on reset/release */` |
|         - | 5053 | `	void *pXmlLimbo;           /* The OWNERLESS shell (phl_xmldoc with no xmlDoc): every` |
|         - | 5054 | ``	                            * constructed-but-never-adopted DOM node -- php's `new`` |
|         - | 5055 | ``	                            * DOMText('t')`, whose node has NO document until the first`` |
|         - | 5056 | `	                            * insertion adopts it -- parks on its orphan set, freed with` |
|         - | 5057 | `	                            * the registry chain it sits on. Lazily created by the DOM's` |
|         - | 5058 | `	                            * constructors; reset to 0 whenever the chain is freed. */` |
|         - | 5059 | `	void *pXmlWriters;         /* XMLWriter registry chain; freed on reset/release */` |
|         - | 5060 | `	void *pXmlParsers;         /* phl_xmlparser registry chain (ext/xml); freed on reset/release */` |
|         - | 5061 | `	void *pPdoConns;           /* phl_pdo registry chain (ext/pdo); freed on reset/release --` |
|         - | 5062 | `	                            * a sqlite3 handle lives outside SyMemBackend, so the` |
|         - | 5063 | `	                            * wholesale release would leak both it and the file lock */` |
|         - | 5064 | `	void *pSq3Conns;           /* phl_sq3 registry chain (ext/sqlite3); freed on reset/release.` |
|         - | 5065 | `	                            * A SEPARATE chain from pPdoConns: the two extensions share` |
|         - | 5066 | `	                            * libsqlite3 and nothing else -- different error model, different` |
|         - | 5067 | `	                            * open flags, different object -- so they own their handles apart */` |
|         - | 5068 | `	void *pCurlHandles;        /* phl_curl registry chain (ext/curl); freed on reset/release --` |
|         - | 5069 | `	                            * a CURL* lives outside SyMemBackend too, and holds a socket` |
|         - | 5070 | `	                            * and a connection cache with it */` |
|         - | 5071 | `	void *pCurlMultis;         /* phl_curlm registry chain (ext/curl); swept BEFORE` |
|         - | 5072 | `	                            * pCurlHandles, since a multi still holds the easy handles` |
|         - | 5073 | `	                            * that were added to it */` |
|         - | 5074 | `	void *pCurlShares;         /* phl_curlsh registry chain (ext/curl); swept AFTER` |
|         - | 5075 | `	                            * pCurlHandles, since a CURLSH an easy handle still names` |
|         - | 5076 | `	                            * refuses to be cleaned up */` |
|         - | 5077 | `	ph7_value sXmlEntLoader;   /* libxml_set_external_entity_loader()'s callable; NULL = default.` |
|         - | 5078 | `	                            * Stored and answered, never invoked: no PHL parse path loads an` |
|         - | 5079 | `	                            * external entity (php's sanitized defaults keep it off too) —` |
|         - | 5080 | `	                            * a recorded divergence. */` |
|         - | 5081 | `	ph7_value sXmlStreamsCtx;  /* libxml_set_streams_context()'s stream-context resource; read by` |
|         - | 5082 | `	                            * nothing until an http:// wrapper exists. */` |
|         - | 5083 | `#endif` |
|         - | 5084 | `	void *pPhars;              /* phl_phar registry chain (ext/phar): every archive this run` |
|         - | 5085 | `	                            * opened, freed on reset/release. php's own cache is` |
|         - | 5086 | `	                            * per-request and behaves the same way. */` |
|         - | 5087 | `	void *pZips;               /* phl_zip registry chain (ext/zip): every archive a ZipArchive` |
|         - | 5088 | ``	                            * or a `zip://` open is holding, freed on reset/release */`` |
|         - | 5089 | `	void *pLastDir;            /* php's "last opened directory stream": the io_private the` |
|         - | 5090 | `	                            * most recent opendir() handed out, which readdir(),` |
|         - | 5091 | `	                            * rewinddir() and closedir() fall back to when they are` |
|         - | 5092 | `	                            * given null (deprecated since 8.1). Cleared when THAT` |
|         - | 5093 | `	                            * handle is closed and at reset; never owns anything. */` |
|         - | 5094 | `	SyBlob sPharRunning;       /* The archive the running script came from, as Phar::running()` |
|         - | 5095 | `	                            * answers it: set by Phar::mapPhar(), empty outside one. */` |
|         - | 5096 | `#ifdef PH7_ENABLE_NET` |
|         - | 5097 | `	void *pSockets;            /* phl_socket registry chain (ext/sockets); freed on reset/release` |
|         - | 5098 | `	                            * -- a DESCRIPTOR is not the allocator's, so the wholesale` |
|         - | 5099 | `	                            * release would leak the file handle and its port */` |
|         - | 5100 | `	void *pAddrInfos;          /* phl_addrinfo registry chain (ext/sockets), same rule: each` |
|         - | 5101 | `	                            * record holds a copied ai_canonname of its own */` |
|         - | 5102 | `	int iSocketLastErr;        /* php's SOCKETS_G(last_error): the per-REQUEST errno` |
|         - | 5103 | `	                            * socket_last_error() answers with no argument, beside the` |
|         - | 5104 | `	                            * per-socket one every record carries */` |
|         - | 5105 | `#endif` |
|         - | 5106 | `	SyBlob sPharErr;           /* The phar wrapper's open-failure sentence. It has to outlive the` |
|         - | 5107 | `	                            * xOpen that formatted it -- the engine keeps the POINTER and the` |
|         - | 5108 | `	                            * caller prints it after the open returned -- so it cannot be a` |
|         - | 5109 | `	                            * stack buffer (ASan caught exactly that). */` |
|         - | 5110 | `#ifdef PH7_ENABLE_ZLIB` |
|         - | 5111 | `	void *pZlibCtx;            /* phl_zctx registry chain (ext/zlib); freed on reset/release --` |
|         - | 5112 | `	                            * a z_stream's window is libz's own allocation, outside` |
|         - | 5113 | `	                            * SyMemBackend, so the wholesale release would leak it */` |
|         - | 5114 | `	int iZlibLevel;            /* the compression level the NEXT compress.zlib open uses, and` |
|         - | 5115 | `	                            * the strategy with it: gzopen()'s mode string carries both` |
|         - | 5116 | `	                            * ("wb9f") and an xOpen is handed flags rather than the string,` |
|         - | 5117 | `	                            * so the door that parsed them arms them here. Reset to libz's` |
|         - | 5118 | `	                            * defaults by the open that reads them. */` |
|         - | 5119 | `	int iZlibStrategy;` |
|         - | 5120 | `	int bZlibDirect;           /* 1 while a gzopen()-family open is in flight. The two doors` |
|         - | 5121 | `	                            * onto this device report a failure differently: gzopen() reads` |
|         - | 5122 | `	                            * as the FILE open it is ("No such file or directory"), while` |
|         - | 5123 | `	                            * compress.zlib:// is a wrapper and php gives every one of its` |
|         - | 5124 | `	                            * failures the same flat "operation failed". */` |
|         - | 5125 | `#endif` |
|         - | 5126 | `#ifdef PH7_ENABLE_OPENSSL` |
|         - | 5127 | `	void *pSslObjs;            /* phl_ssl_obj registry chain (ext/openssl); freed on reset/release` |
|         - | 5128 | `	                            * -- an X509/EVP_PKEY/X509_REQ is OpenSSL's own allocation, outside` |
|         - | 5129 | `	                            * SyMemBackend, so the wholesale release would leak it */` |
|         - | 5130 | `	void *pSslErrors;          /* phl_ssl_errors: php's 16-slot ring, drained from OpenSSL's own` |
|         - | 5131 | `	                            * error queue after a failure and read one entry at a time by` |
|         - | 5132 | `	                            * openssl_error_string() */` |
|         - | 5133 | `#endif` |
|         - | 5134 | `	/* php numbers every resource with a small sequential id that (int) casts and` |
|         - | 5135 | `	 * "Resource id #N" render, and that distinguishes two live resources from one` |
|         - | 5136 | `	 * another. PHL's resource value is a bare void*, so the id lives in this` |
|         - | 5137 | `	 * per-VM registry: pointer -> phl_res_id, assigned on first observation.` |
|         - | 5138 | `	 * Freed with the VM (ids are never recycled, as php's may be). */` |
|         - | 5139 | `	SyHash hResourceId;        /* void* -> phl_res_id* */` |
|         - | 5140 | `	sxu32 nResourceIdNext;     /* Next id to hand out (php's start at 1) */` |
|         - | 5141 | `	/* Stream contexts (stream_context_create). The chain owns every context the` |
|         - | 5142 | `	 * script made; pDefaultCtx is the one stream_context_get_default() hands` |
|         - | 5143 | `	 * back and every opener falls back to. */` |
|         - | 5144 | `	void *pStreamCtx;          /* phl_stream_ctx registry chain; freed on reset */` |
|         - | 5145 | `	void *pDefaultCtx;         /* phl_stream_ctx* — the default context, or 0 */` |
|         - | 5146 | `	void *pOpenCtx;            /* the context the open in flight runs under */` |
|         - | 5147 | `	char zOpenMode[16];        /* the mode string the open in flight was ASKED with, when a` |
|         - | 5148 | `	                            * caller had one: php hands a userland wrapper's stream_open()` |
|         - | 5149 | `	                            * the caller's own spelling ('rb', 'w+', 'x'), and PHL could` |
|         - | 5150 | `	                            * only rebuild an approximation from the flag bits -- so` |
|         - | 5151 | `	                            * file_put_contents() told a wrapper it was opening for` |
|         - | 5152 | `	                            * READING. Empty when the opener has no string of its own` |
|         - | 5153 | `	                            * (the C-level readers), and cleared after every open. */` |
|         - | 5154 | `	/* What a FAILED open says. php names the URI the script wrote -- scheme and` |
|         - | 5155 | `	 * all -- and gives the WRAPPER's reason for it, where only the plain-file` |
|         - | 5156 | `	 * wrapper's reason is an errno. PH7_VmGetStreamDevice() advances past the` |
|         - | 5157 | `	 * scheme, so the two halves of the name are remembered here as it does:` |
|         - | 5158 | `	 * zOpenUriTail is the pointer it handed back, and a warning printing THAT` |
|         - | 5159 | `	 * pointer is reporting THIS open and may name the whole thing instead. */` |
|         - | 5160 | `	const char *zOpenUri;      /* the URI as written, or 0 */` |
|         - | 5161 | `	int nOpenUri;              /* its length */` |
|         - | 5162 | `	const char *zOpenUriTail;  /* the scheme-stripped remainder handed to the wrapper */` |
|         - | 5163 | `	const char *zOpenCaller;   /* the FUNCTION reporting this open, for a wrapper that` |
|         - | 5164 | `	                            * raises a diagnostic of its own before the caller's` |
|         - | 5165 | `	                            * (php's resolver failure is two warnings, not one) */` |
|         - | 5166 | `	const char *zOpenErr;      /* the wrapper's own reason for the open in flight, or 0` |
|         - | 5167 | `	                            * for the plain-file wrapper's errno */` |
|         - | 5168 | `	char zOpenErrBuf[512];     /* storage for a reason that has to be BUILT -- a userland` |
|         - | 5169 | `	                            * wrapper names its own class and method, and the http` |
|         - | 5170 | `	                            * wrapper interpolates a host name or a whole status` |
|         - | 5171 | `	                            * line -- since the caller's buffer does not outlive the` |
|         - | 5172 | `	                            * call */` |
|         - | 5173 | `	int nOpenDepth;            /* opens in flight. The three fields above belong to the` |
|         - | 5174 | `	                            * OUTERMOST one: php://filter opens its own resource from` |
|         - | 5175 | `	                            * inside its xOpen, and that inner open would otherwise` |
|         - | 5176 | `	                            * report the RESOURCE's errno under the filter's name --` |
|         - | 5177 | `	                            * and leave zOpenUriTail pointing into a blob it frees on` |
|         - | 5178 | `	                            * the way out. */` |
|         - | 5179 | `	/* The response headers of the last http:// exchange, one line per '\n'. Two` |
|         - | 5180 | ``	 * consumers outlive the handle that produced them: `$http_response_header`,`` |
|         - | 5181 | `	 * which the stream layer writes into the frame that called the opener, and` |
|         - | 5182 | `	 * php 8.4's http_get_last_response_headers(), which answers them until` |
|         - | 5183 | `	 * http_clear_last_response_headers() drops the store. */` |
|         - | 5184 | `	SyBlob sHttpRespHdrs;      /* the lines, '\n'-separated */` |
|         - | 5185 | `	sxu8 bHttpRespHdrs;        /* something has been recorded (the getter's NULL/array split) */` |
|         - | 5186 | `	sxu8 bHttpRespFresh;       /* recorded by the open in flight and not yet published */` |
|         - | 5187 | `	sxu8 bHttpGetHeaders;      /* the open in flight is get_headers()', which php makes` |
|         - | 5188 | ``	                            * two things at once: a context with `ignore_errors` on,`` |
|         - | 5189 | `	                            * so a refused status is an ordinary set of headers, and` |
|         - | 5190 | `	                            * STREAM_ONLY_GET_HEADERS, which skips the dechunk filter` |
|         - | 5191 | `	                            * and so KEEPS the Transfer-Encoding header the ordinary` |
|         - | 5192 | `	                            * read consumes */` |
|         - | 5193 | `	/* Stream filters (stream_filter_append and the php://filter wrapper). The` |
|         - | 5194 | `	 * chain owns every filter INSTANCE the script created, so one that is never` |
|         - | 5195 | `	 * removed still goes back at reset. */` |
|         - | 5196 | `	void *pStreamFilter;       /* phl_stream_filter registry chain; freed on reset */` |
|         - | 5197 | `	void *pUserFilters;        /* stream_filter_register() name => class chain */` |
|         - | 5198 | ``	void *pFilterCall;         /* phl_brigade_res* — the `$out` of the filter() call`` |
|         - | 5199 | `	                            * in flight, which is what stream_bucket_new()` |
|         - | 5200 | `	                            * hangs its token on */` |
|         - | 5201 | `	SyString *pCalleeName;     /* the builtin currently running, for diagnostics` |
|         - | 5202 | `	                            * raised where no ph7_context reaches (see vm_exec.c) */` |
|         - | 5203 | `	ph7_vm *pNext,*pPrev;      /* List of active VM's */` |
|         - | 5204 | `	sxu32 nMagic;              /* Sanity check against misuse */` |
|         - | 5205 | `};` |
|         - | 5206 | `/*` |
|         - | 5207 | ` * Allowed value for ph7_vm.nMagic` |
|         - | 5208 | ` */` |
|         - | 5209 | `#define PH7_VM_INIT   0xFADE9512  /* VM correctly initialized */` |
|         - | 5210 | `#define PH7_VM_RUN    0xEA271285  /* VM ready to execute PH7 bytecode */` |
|         - | 5211 | `#define PH7_VM_EXEC   0xCAFE2DAD  /* VM executing PH7 bytecode */` |
|         - | 5212 | `#define PH7_VM_STALE  0xBAD1DEAD  /* Stale VM */` |
|         - | 5213 | `/*` |
|         - | 5214 | ` * Error codes according to the PHP language reference manual.` |
|         - | 5215 | ` */` |
|         - | 5216 | `enum iErrCode` |
|         - | 5217 | `{` |
|         - | 5218 | `	E_ERROR             = 1,   /* Fatal run-time errors. These indicate errors that can not be recovered` |
|         - | 5219 | `							    * from, such as a memory allocation problem. Execution of the script is` |
|         - | 5220 | `							    * halted.` |
|         - | 5221 | `								* The only fatal error under PH7 is an out-of-memory. All others erros` |
|         - | 5222 | `								* even a call to undefined function will not halt script execution.` |
|         - | 5223 | `							    */` |
|         - | 5224 | `	E_WARNING           = 2,   /* Run-time warnings (non-fatal errors). Execution of the script is not halted.  */` |
|         - | 5225 | `	E_PARSE             = 4,   /* Compile-time parse errors. Parse errors should only be generated by the parser.*/` |
|         - | 5226 | `	E_NOTICE            = 8,   /* Run-time notices. Indicate that the script encountered something that could` |
|         - | 5227 | `							    * indicate an error, but could also happen in the normal course of running a script.` |
|         - | 5228 | `							    */` |
|         - | 5229 | `	E_CORE_WARNING      = 16,  /* Fatal errors that occur during PHP's initial startup. This is like an E_ERROR` |
|         - | 5230 | `							    * except it is generated by the core of PHP.` |
|         - | 5231 | `							    */` |
|         - | 5232 | `	E_USER_ERROR        = 256,  /* User-generated error message.*/` |
|         - | 5233 | `	E_USER_WARNING      = 512,  /* User-generated warning message.*/` |
|         - | 5234 | `	E_USER_NOTICE       = 1024, /* User-generated notice message.*/` |
|         - | 5235 | `	E_STRICT            = 2048, /* Enable to have PHP suggest changes to your code which will ensure the best interoperability` |
|         - | 5236 | `								 * and forward compatibility of your code.` |
|         - | 5237 | `								 */` |
|         - | 5238 | `	E_RECOVERABLE_ERROR = 4096, /* Catchable fatal error. It indicates that a probably dangerous error occured, but did not` |
|         - | 5239 | `								 * leave the Engine in an unstable state. If the error is not caught by a user defined handle` |
|         - | 5240 | `								 * the application aborts as it was an E_ERROR.` |
|         - | 5241 | `								 */` |
|         - | 5242 | `	E_DEPRECATED        = 8192, /* Run-time notices. Enable this to receive warnings about code that will not` |
|         - | 5243 | `								 * work in future versions.` |
|         - | 5244 | `								 */` |
|         - | 5245 | `	E_USER_DEPRECATED   = 16384, /* User-generated warning message. */` |
|         - | 5246 | `	E_ALL               = 32767  /* All errors and warnings */` |
|         - | 5247 | `};` |
|         - | 5248 | `/*` |
|         - | 5249 | ` * Each VM instruction resulting from compiling a PHP script is represented` |
|         - | 5250 | ` * by one of the following OP codes.` |
|         - | 5251 | ` * The program consists of a linear sequence of operations. Each operation` |
|         - | 5252 | ` * has an opcode and 3 operands.Operands P1 is an integer.` |
|         - | 5253 | ` * Operand P2 is an unsigned integer and operand P3 is a memory address.` |
|         - | 5254 | ` * Few opcodes use all 3 operands.` |
|         - | 5255 | ` */` |
|         - | 5256 | `enum ph7_vm_op {` |
|         - | 5257 | `  PH7_OP_DONE =   1,   /* Done */` |
|         - | 5258 | `  PH7_OP_HALT,         /* Halt */` |
|         - | 5259 | `  PH7_OP_LOAD,         /* Load memory object */` |
|         - | 5260 | `  PH7_OP_LOADC,        /* Load constant */` |
|         - | 5261 | `  PH7_OP_LOAD_IDX,     /* Load array entry */` |
|         - | 5262 | `  PH7_OP_LOAD_MAP,     /* Load hashmap('array') */` |
|         - | 5263 | `  PH7_OP_LOAD_LIST,    /* Load list */` |
|         - | 5264 | `  PH7_OP_LOAD_CLOSURE, /* Load closure */` |
|         - | 5265 | `  PH7_OP_LOAD_FCC,     /* Load first-class callable: wrap a function/method as a Closure */` |
|         - | 5266 | `  PH7_OP_NOOP,         /* NOOP */` |
|         - | 5267 | `  PH7_OP_JMP,          /* Unconditional jump */` |
|         - | 5268 | `  PH7_OP_JZ,           /* Jump on zero (FALSE jump) */` |
|         - | 5269 | `  PH7_OP_JNZ,          /* Jump on non-zero (TRUE jump) */` |
|         - | 5270 | `  PH7_OP_POP,          /* Stack POP */` |
|         - | 5271 | `  PH7_OP_CAT,          /* Concatenation */` |
|         - | 5272 | `  PH7_OP_CVT_INT,      /* Integer cast */` |
|         - | 5273 | `  PH7_OP_CVT_STR,      /* String cast */` |
|         - | 5274 | `  PH7_OP_CVT_REAL,     /* Float cast */` |
|         - | 5275 | `  PH7_OP_CALL,         /* Function call */` |
|         - | 5276 | `  PH7_OP_UMINUS,       /* Unary minus '-'*/` |
|         - | 5277 | `  PH7_OP_UPLUS,        /* Unary plus '+'*/` |
|         - | 5278 | `  PH7_OP_BITNOT,       /* Bitwise not '~' */` |
|         - | 5279 | `  PH7_OP_LNOT,         /* Logical not '!' */` |
|         - | 5280 | `  PH7_OP_MUL,          /* Multiplication '*' */` |
|         - | 5281 | `  PH7_OP_DIV,          /* Division '/' */` |
|         - | 5282 | `  PH7_OP_MOD,          /* Modulus '%' */` |
|         - | 5283 | `  PH7_OP_POW,          /* Exponentiation '**' */` |
|         - | 5284 | `  PH7_OP_ADD,          /* Add '+' */` |
|         - | 5285 | `  PH7_OP_SUB,          /* Sub '-' */` |
|         - | 5286 | `  PH7_OP_SHL,          /* Left shift '<<' */` |
|         - | 5287 | `  PH7_OP_SHR,          /* Right shift '>>' */` |
|         - | 5288 | `  PH7_OP_LT,           /* Less than '<' */` |
|         - | 5289 | `  PH7_OP_LE,           /* Less or equal '<=' */` |
|         - | 5290 | `  PH7_OP_GT,           /* Greater than '>' */` |
|         - | 5291 | `  PH7_OP_GE,           /* Greater or equal '>=' */` |
|         - | 5292 | `  PH7_OP_SPACESHIP,    /* Spaceship '<=>' */` |
|         - | 5293 | `  PH7_OP_EQ,           /* Equal '==' */` |
|         - | 5294 | `  PH7_OP_NEQ,          /* Not equal '!=' */` |
|         - | 5295 | `  PH7_OP_TEQ,          /* Type equal '===' */` |
|         - | 5296 | `  PH7_OP_TNE,          /* Type not equal '!==' */` |
|         - | 5297 | `  PH7_OP_BAND,         /* Bitwise and '&' */` |
|         - | 5298 | `  PH7_OP_BXOR,         /* Bitwise xor '^' */` |
|         - | 5299 | `  PH7_OP_BOR,          /* Bitwise or '\|' */` |
|         - | 5300 | `  PH7_OP_LAND,         /* Logical and '&&','and' */` |
|         - | 5301 | `  PH7_OP_LOR,          /* Logical or  '\|\|','or' */` |
|         - | 5302 | `  PH7_OP_LXOR,         /* Logical xor 'xor' */` |
|         - | 5303 | `  PH7_OP_STORE,        /* Store Object */` |
|         - | 5304 | `  PH7_OP_STORE_IDX,    /* Store indexed object */` |
|         - | 5305 | `  PH7_OP_STORE_IDX_REF,/* Store indexed object by reference */` |
|         - | 5306 | `  PH7_OP_PULL,         /* Stack pull */` |
|         - | 5307 | `  PH7_OP_SWAP,         /* Stack swap */` |
|         - | 5308 | `  PH7_OP_YIELD,        /* Stack yield */` |
|         - | 5309 | `  PH7_OP_YIELD_FROM,   /* Generator delegation (yield from <iterable>) */` |
|         - | 5310 | `  PH7_OP_CVT_BOOL,     /* Boolean cast */` |
|         - | 5311 | `  PH7_OP_CVT_NUMC,     /* Numeric (integer,real or both) type cast */` |
|         - | 5312 | `  PH7_OP_INCR,         /* Increment ++ */` |
|         - | 5313 | `  PH7_OP_DECR,         /* Decrement -- */` |
|         - | 5314 | `  PH7_OP_NEW,          /* new */` |
|         - | 5315 | `  PH7_OP_CLONE,        /* clone */` |
|         - | 5316 | `  PH7_OP_ADD_STORE,    /* Add and store '+=' */` |
|         - | 5317 | `  PH7_OP_SUB_STORE,    /* Sub and store '-=' */` |
|         - | 5318 | `  PH7_OP_MUL_STORE,    /* Mul and store '*=' */` |
|         - | 5319 | `  PH7_OP_DIV_STORE,    /* Div and store '/=' */` |
|         - | 5320 | `  PH7_OP_MOD_STORE,    /* Mod and store '%=' */` |
|         - | 5321 | `  PH7_OP_POW_STORE,    /* Pow and store '**=' */` |
|         - | 5322 | `  PH7_OP_CAT_STORE,    /* Cat and store '.=' */` |
|         - | 5323 | `  PH7_OP_SHL_STORE,    /* Shift left and store '>>=' */` |
|         - | 5324 | `  PH7_OP_SHR_STORE,    /* Shift right and store '<<=' */` |
|         - | 5325 | `  PH7_OP_BAND_STORE,   /* Bitand and store '&=' */` |
|         - | 5326 | `  PH7_OP_BOR_STORE,    /* Bitor and store '\|=' */` |
|         - | 5327 | `  PH7_OP_BXOR_STORE,   /* Bitxor and store '^=' */` |
|         - | 5328 | `  PH7_OP_CONSUME,      /* Consume VM output */` |
|         - | 5329 | `  PH7_OP_LOAD_REF,     /* Load reference */` |
|         - | 5330 | `  PH7_OP_STORE_REF,    /* Store a reference to a variable*/` |
|         - | 5331 | `  PH7_OP_MEMBER,       /* Class member run-time access */` |
|         - | 5332 | `  PH7_OP_UPLINK,       /* Run-Time frame link */` |
|         - | 5333 | `  PH7_OP_CVT_NULL,     /* NULL cast */` |
|         - | 5334 | `  PH7_OP_CVT_ARRAY,    /* Array cast */` |
|         - | 5335 | `  PH7_OP_CVT_OBJ,      /* Object cast */` |
|         - | 5336 | `  PH7_OP_FOREACH_INIT, /* For each init */` |
|         - | 5337 | `  PH7_OP_FOREACH_STEP, /* For each step */` |
|         - | 5338 | `  PH7_OP_IS_A,         /* Instanceof */` |
|         - | 5339 | `  PH7_OP_LOAD_EXCEPTION,/* Load an exception */` |
|         - | 5340 | `  PH7_OP_POP_EXCEPTION, /* POP an exception */` |
|         - | 5341 | `  PH7_OP_THROW,         /* Throw exception */` |
|         - | 5342 | `  PH7_OP_SWITCH,        /* Switch operation */` |
|         - | 5343 | `  PH7_OP_MATCH,         /* Match expression (PHP 8.0) */` |
|         - | 5344 | `  PH7_OP_ERR_CTRL,     /* Error control */` |
|         - | 5345 | `  PH7_OP_DUP,          /* Duplicate top of stack */` |
|         - | 5346 | `  PH7_OP_NULLC,         /* Null coalescing ?? */` |
|         - | 5347 | `  PH7_OP_NULLC_JMP,     /* Null coalescing assign short-circuit jump */` |
|         - | 5348 | `  PH7_OP_NULLC_STORE,   /* Null coalescing assign store */` |
|         - | 5349 | `  PH7_OP_NULLSAFE_JMP,  /* Nullsafe (?->) short-circuit jump */` |
|         - | 5350 | `  PH7_OP_SPREAD,        /* Mark TOS for argument unpacking (...$arr) */` |
|         - | 5351 | `  PH7_OP_FLAG_SPREAD,   /* Flag TOS as a spread source for the next LOAD_MAP */` |
|         - | 5352 | `  PH7_OP_CATCH,         /* Bind the in-flight exception into a catch variable (ROOT C inline catch) */` |
|         - | 5353 | `  PH7_OP_END_FINALLY,   /* Terminate an inline finally: dispatch the pending action (ROOT C) */` |
|         - | 5354 | `  PH7_OP_SET_FINALLY_RET,/* Seed a pending RETURN and enter the innermost enclosing finally (ROOT C) */` |
|         - | 5355 | `  PH7_OP_SET_FINALLY_JMP,/* Seed a pending BREAK/CONTINUE (jump target) and enter a finally (ROOT C) */` |
|         - | 5356 | `  PH7_OP_CATCH_JMP,     /* Jump to iP2, leaving the try/catch structures iP1 describes (see` |
|         - | 5357 | `                         * PH7_CATCH_JMP_P1): LEVELS detached catch/finally mini-programs and` |
|         - | 5358 | `                         * CROSS enclosing trys whose OP_POP_EXCEPTION the jump skips. Two` |
|         - | 5359 | `                         * regimes. LEVELS > 0: iP2 is a pc in the OWNING body's bytecode, which` |
|         - | 5360 | `                         * this mini-program cannot address — park it on that body's frame and` |
|         - | 5361 | `                         * end the mini-program; each try's OP_POP_EXCEPTION landing pad on the` |
|         - | 5362 | `                         * way out decrements, and the last one drains CROSS and takes the jump,` |
|         - | 5363 | ``                         * exactly as it materializes a catch's parked `return`. LEVELS == 0:`` |
|         - | 5364 | ``                         * iP2 is in THIS array (a `goto` out of a try body) — just drain CROSS`` |
|         - | 5365 | `                         * and jump. Emitted for break/continue/goto alike. */` |
|         - | 5366 | `  PH7_OP_UNSET_VAR,     /* unset($name): drop ONE name binding (p3 = name), never the shared slot */` |
|         - | 5367 | `  PH7_OP_CALL_INIT,     /* Screen a call's callee where it is WRITTEN, before its arguments run:` |
|         - | 5368 | `                         * php resolves one at INIT_FCALL / INIT_DYNAMIC_CALL and raises the` |
|         - | 5369 | `                         * direct dispatch's own Error there. Emitted only for a callee the` |
|         - | 5370 | `                         * following OP_CALL would be the first to look at — a member callee` |
|         - | 5371 | `                         * was already screened by its OP_MEMBER. iP2 = 1 when the compiler` |
|         - | 5372 | `                         * namespace-qualified the name, which the global fallback needs. */` |
|         - | 5373 | `  PH7_OP_ROT_CALLEE,    /* Rotate this call's CALLEE — which the codegen pushed BEFORE the` |
|         - | 5374 | `                         * arguments, because php resolves a callee where it is written — up` |
|         - | 5375 | `                         * to the top of the stack, so OP_CALL sees the [args…][callee] layout` |
|         - | 5376 | `                         * its whole dispatch is written against. iP1 = compile-time argument` |
|         - | 5377 | `                         * count, iP2 = PH7_ROT_* flags (SPREAD: re-derive the runtime count` |
|         - | 5378 | `                         * from this call's unpack runs; TWOSLOT: the callee is an OP_MEMBER` |
|         - | 5379 | `                         * method pair [receiver][name], not a single value). */` |
|         - | 5380 | `  PH7_OP_NAMED_SEND,    /* Screen a NAMED argument where it is sent: php resolves the name at` |
|         - | 5381 | `                         * the SEND of that argument, so an unknown name (or one a positional` |
|         - | 5382 | `                         * argument already filled) throws before a later argument runs and` |
|         - | 5383 | ``                         * before a plain `$var` operand is read. Emitted after the argument's`` |
|         - | 5384 | `                         * own expression; the callee is still BELOW the argument region.` |
|         - | 5385 | `                         * iP1 = the argument's compile-time position, iP2 = PH7_ROT_SPREAD when` |
|         - | 5386 | ``                         * an unpack precedes it (\| PH7_ROT_NEW for a `new`'s list), p3 = the`` |
|         - | 5387 | `                         * call's VmCallArgMap. */` |
|         - | 5388 | `  PH7_OP_FUNC_DECL,     /* Bind a CONDITIONAL function declaration: p3 = ph7_vm_func. php binds` |
|         - | 5389 | `                         * a function written at a unit's top level when the unit compiles and` |
|         - | 5390 | ``                         * one written anywhere else (inside an `if`, a loop, another function's`` |
|         - | 5391 | `                         * body) when execution REACHES it -- which is what makes` |
|         - | 5392 | ``                          * `if (!function_exists('f')) { function f(){} }` a no-op when `f` `` |
|         - | 5393 | `                         * exists, and every symfony/polyfill-* package harmless beside a real` |
|         - | 5394 | `                         * mbstring. Redeclaring is php's runtime fatal, raised here. */` |
|         - | 5395 | `  PH7_OP_CLASS_DEFER,   /* Deferred class declaration: p3 = VmDeferredClass. Compile-time` |
|         - | 5396 | `                         * resolution of a parent/interface/trait failed (autoloader not yet` |
|         - | 5397 | `                         * REGISTERED — the declaring file's own statements had not run), so the` |
|         - | 5398 | `                         * whole declaration re-compiles here, at its execution point, where` |
|         - | 5399 | `                         * spl_autoload_register has taken effect. php's own model: classes with` |
|         - | 5400 | `                         * unresolved parents are declared in execution order, not hoisted. */` |
|         - | 5401 | `  PH7_OP_CLASS_OBLIGE,  /* Settle a class's unresolved variance pairs where its declaration` |
|         - | 5402 | `                         * RUNS: p3 = VmClassObligeSet. Autoloads what they name, then checks` |
|         - | 5403 | `                         * them again; one still open is php's compile fatal. */` |
|         - | 5404 | `  PH7_OP_CLASS_DECLARE, /* Declare a class php does not early-bind where its statement RUNS:` |
|         - | 5405 | `                         * p3 = the PH7_CLASS_HIDDEN class, put into hClass here. */` |
|         - | 5406 | `  PH7_OP_SNAPSHOT       /* Give the top P1 stack slots their own copy of the string bytes they` |
|         - | 5407 | `                         * were loaded from (P1 = 0 means the top slot alone). A value copy only` |
|         - | 5408 | `                         * BORROWS the source's bytes (PH7_MemObjLoad), which is right while the` |
|         - | 5409 | `                         * source cannot change and wrong the moment it can: a value already` |
|         - | 5410 | `                         * pushed then reads a LATER write to that source through the alias, with` |
|         - | 5411 | `                         * the length it captured at the push. Emitted where something that can` |
|         - | 5412 | `                         * RUN still sits between a push and the instruction that consumes it --` |
|         - | 5413 | `                         * a by-value argument before a later argument, an array literal's` |
|         - | 5414 | `                         * entries before a later entry, a binary operator's left operand before` |
|         - | 5415 | `                         * its right -- and nowhere else, so ordinary code pays nothing for it. */` |
|         - | 5416 | `  ,PH7_OP_PICK          /* Push a copy of the stack slot P1 below the top (P1 = 0 is DUP).` |
|         - | 5417 | `                         * php evaluates an assignment target's dynamic subscript and property` |
|         - | 5418 | `                         * NAMES before the assigned value and performs the FETCHES after it, so` |
|         - | 5419 | ``                         * `$a[k()] = v()` runs k() first and only then vivifies. A stack machine`` |
|         - | 5420 | `                         * cannot emit that in one pass: the names are pushed first, the value` |
|         - | 5421 | `                         * lands on top of them, and the access chain -- emitted last, so nothing` |
|         - | 5422 | `                         * it creates is visible to the value -- reads each name back from where` |
|         - | 5423 | `                         * it was parked. That read is this. */` |
|         - | 5424 | ``  ,PH7_OP_CONST_DECL    /* Declare a global `const` here, where its statement RUNS: p3 =`` |
|         - | 5425 | `                         * VmConstDecl, the value on top of the stack (popped). php binds` |
|         - | 5426 | `                         * one at ZEND_DECLARE_CONST, so a name read before the statement` |
|         - | 5427 | `                         * is undefined and a second declaration of a taken name warns` |
|         - | 5428 | `                         * and keeps the first value, exactly as define() does. */` |
|         - | 5429 | `};` |
|         - | 5430 | `/*` |
|         - | 5431 | ` * PH7_OP_CATCH_JMP.iP1 payload. Both halves are nesting depths of the source, never` |
|         - | 5432 | ` * large: LEVELS = detached catch/finally boundaries the jump leaves (0 = none, it` |
|         - | 5433 | ` * stays in this bytecode array), CROSS = enclosing try activations whose` |
|         - | 5434 | ` * OP_POP_EXCEPTION the jump skips, and whose finally it must therefore drain itself.` |
|         - | 5435 | ` */` |
|         - | 5436 | `#define PH7_CATCH_JMP_P1(LEVELS,CROSS) \` |
|         - | 5437 | `	((sxi32)((((sxu32)(CROSS)) << 16) \| ((sxu32)(LEVELS) & 0xFFFFu)))` |
|         - | 5438 | `#define PH7_CATCH_JMP_LEVELS(P1) ((sxu16)((sxu32)(P1) & 0xFFFFu))` |
|         - | 5439 | `#define PH7_CATCH_JMP_CROSS(P1)  ((sxu16)(((sxu32)(P1) >> 16) & 0xFFFFu))` |
|         - | 5440 | `/* LOADC.iP1 bit flags */` |
|         - | 5441 | `#define PH7_LOADC_EXPAND   0x01 /* Candidate for constant/function/class expansion */` |
|         - | 5442 | `#define PH7_LOADC_NOKEY    0x04 /* The nil this pushes is an ABSENT array-literal key (auto-index),` |
|         - | 5443 | ``                                 * not an explicit `null =>` one. The two are both MEMOBJ_NULL on the`` |
|         - | 5444 | `                                 * stack, and LOAD_MAP must tell them apart: an absent key auto-indexes` |
|         - | 5445 | `                                 * silently, an explicit null key deprecates and stores under "". */` |
|         - | 5446 | `#define PH7_LOADC_ABSOLUTE 0x02 /* Fully-qualified — skip namespace prefixing */` |
|         - | 5447 | ``#define PH7_LOADC_NOGLOBAL 0x08 /* The p3 candidate came from a `use const` import, which php`` |
|         - | 5448 | `                                 * resolves WITHOUT a global fallback: if that exact name is` |
|         - | 5449 | `                                 * undefined the read is an Error, even when a global constant` |
|         - | 5450 | `                                 * of the imported alias's short name exists. */` |
|         - | 5451 | `/* ROT_CALLEE.iP2 — what the rotation has to know about the region it is turning over. */` |
|         - | 5452 | `#define PH7_ROT_SPREAD  0x1 /* This call unpacks: the runtime argument count is iP1 plus the net` |
|         - | 5453 | `                             * growth of its OWN captured spread runs (VmSpreadOwnExtra). */` |
|         - | 5454 | `#define PH7_ROT_TWOSLOT 0x2 /* The callee is an OP_MEMBER method pair — [receiver][method name] —` |
|         - | 5455 | `                             * which OP_CALL reads as two slots ($this / the late-static-binding` |
|         - | 5456 | `                             * class from the receiver, the name from the top). A __call routing` |
|         - | 5457 | `                             * collapses that pair to ONE marked carrier slot at run time, which` |
|         - | 5458 | `                             * the handler detects rather than guessing. */` |
|         - | 5459 | ``#define PH7_ROT_NEW     0x4 /* NAMED_SEND.iP2 only: the slot below the arguments is a `new`'s`` |
|         - | 5460 | `                             * CLASS operand (left by its screen pass), so the name is asked` |
|         - | 5461 | `                             * of that class's constructor, not looked up as a function. */` |
|         - | 5462 | `#define PH7_ROT_ANON    0x8 /* NAMED_SEND.iP2 only, with PH7_ROT_NEW: an anonymous class's list.` |
|         - | 5463 | `                             * Nothing stands below its arguments; the class is the one the` |
|         - | 5464 | `                             * map's sNewAnon names. */` |
|         - | 5465 | `#define PH7_ROT_READ    0x10 /* NAMED_SEND.iP2 only: a later argument can run code, so the deferred` |
|         - | 5466 | `                             * operand just pushed is read (warned for, or created for a` |
|         - | 5467 | `                             * by-reference formal) here, at its own SEND, as php reads it. */` |
|         - | 5468 | `#define PH7_ROT_POSITIONAL 0x20 /* NAMED_SEND.iP2 only, with PH7_ROT_READ: the argument has no` |
|         - | 5469 | `                             * name -- the instruction is that read alone. */` |
|         - | 5470 | ``#define PH7_ROT_FRAMELESS 0x40 /* NAMED_SEND.iP2 only, with PH7_ROT_POSITIONAL: a plain `$var` in a`` |
|         - | 5471 | `                             * call php compiles frameless, which a builtin callee reads at the` |
|         - | 5472 | `                             * call; only a function of the namespace's own reads it here. */` |
|         - | 5473 | `/* CALL.iP2 — a bit SET, not the plain hasSpread boolean it started as. */` |
|         - | 5474 | `#define PH7_CALL_SPREAD    0x1 /* This call unpacks (the ROT_SPREAD of the call itself). */` |
|         - | 5475 | `#define PH7_CALL_CONSTRUCT 0x2 /* This call was emitted by a language CONSTRUCT's codegen --` |
|         - | 5476 | ``                                * `isset`, `empty`, `unset`, `eval`, `print`, `include`,`` |
|         - | 5477 | ``                                * `include_once`, `require`, `require_once`. Each is dispatched`` |
|         - | 5478 | `                                * here as a host function of the same name, and php has no such` |
|         - | 5479 | ``                                * function: `function_exists('empty')` is false there and every`` |
|         - | 5480 | `                                * other name door agrees. The implementations stay registered and` |
|         - | 5481 | `                                * stay hidden (ph7_user_func::bConstruct), and this bit is what` |
|         - | 5482 | `                                * says the ENGINE spelled the name -- the exact counterpart of` |
|         - | 5483 | `                                * MEMOBJ_AUX_ENGINEFN for the compiled-function table. A script` |
|         - | 5484 | `                                * cannot produce a call site carrying it: every one of those` |
|         - | 5485 | `                                * names is a lexer KEYWORD, so the only OP_CALL that can ever` |
|         - | 5486 | `                                * name one comes from the construct codegen. */` |
|         - | 5487 | `/* CALL_INIT.iP2 — the same two compile-time questions, asked one instruction earlier.` |
|         - | 5488 | ` * (Its own bit names: the value is not a CALL's iP2 and the two never mix.) */` |
|         - | 5489 | `#define PH7_CALLINIT_NAMESPACED 0x1 /* The callee name was written unqualified inside a namespace,` |
|         - | 5490 | `                                     * so php's global fallback may still resolve it. */` |
|         - | 5491 | `#define PH7_CALLINIT_CONSTRUCT  0x2 /* PH7_CALL_CONSTRUCT's twin: the call this screens is a` |
|         - | 5492 | `                                     * language construct's, whose callee is a host function no` |
|         - | 5493 | `                                     * script can name. The screen asks PH7_VmIsCallable, which` |
|         - | 5494 | `                                     * (rightly) answers NO for those nine names, so it has to` |
|         - | 5495 | `                                     * stand down here exactly as the OP_CALL lookup does. */` |
|         - | 5496 | `/* MEMBER.iP2 — member-access context. 0=read is the default; the unset/isset/empty modes mirror the` |
|         - | 5497 | ` * array LOAD_IDX context modes so unset()/isset()/empty() on a property behave like on an array elem. */` |
|         - | 5498 | `/* Two of PH7_OP_LOAD_IDX's own iP2 context codes (they do NOT line up with the` |
|         - | 5499 | ` * PH7_MEMBER_* set below). Shared because the PROPERTY opcode has to recognize the` |
|         - | 5500 | `` * base of an unset-subscript: `unset($o->p[$k])` reaches into what $p holds, which`` |
|         - | 5501 | ` * is an indirect modification of $p. */` |
|         - | 5502 | `#define VM_IDX_CTX_UNSET 5` |
|         - | 5503 | ``/* An INTERMEDIATE subscript of an unset chain (`unset($a['k']['n'])`): every unset`` |
|         - | 5504 | ` * rule applies to it — COW-separate the parent, never vivify a missing key, unset's` |
|         - | 5505 | ` * own wording for a bad base — except the removal itself, which belongs to the` |
|         - | 5506 | ` * OUTERMOST subscript alone. */` |
|         - | 5507 | `#define VM_IDX_CTX_UNSET_BASE 10` |
|         - | 5508 | ``/* A READ-MODIFY-WRITE subscript (`$a[k] += v`, `$a[k]++`, `$a[k] .= v`) — php's`` |
|         - | 5509 | ` * BP_VAR_RW fetch. It needs a writable slot exactly as the plain write context` |
|         - | 5510 | ` * (1) does, and everything downstream treats it as one; the single thing that` |
|         - | 5511 | ` * separates them is that php READS the element first, so a missing key WARNS` |
|         - | 5512 | ``  * before it is created. Every level of a chain carries it (`$a['x']['y'] += 1` `` |
|         - | 5513 | ` * warns for both), which is why it is a compile-time context and not a peek at` |
|         - | 5514 | ` * the instruction that follows. */` |
|         - | 5515 | `#define VM_IDX_CTX_RMW 11` |
|         - | 5516 | `#define VM_IDX_IS_UNSET(iP2) ((iP2) == VM_IDX_CTX_UNSET \|\| (iP2) == VM_IDX_CTX_UNSET_BASE)` |
|         - | 5517 | `#define PH7_MEMBER_READ   0 /* attribute read */` |
|         - | 5518 | `#define PH7_MEMBER_METHOD 1 /* method-call preparation */` |
|         - | 5519 | `#define PH7_MEMBER_UNSET  2 /* unset($o->p): remove the property */` |
|         - | 5520 | `#define PH7_MEMBER_ISSET  3 /* isset($o->p): silent on a read-miss */` |
|         - | 5521 | `#define PH7_MEMBER_EMPTY  4 /* empty($o->p): silent on a read-miss */` |
|         - | 5522 | `#define PH7_MEMBER_WRITE  5 /* write-lvalue base ($o->arr[..]=, $o->p??=): auto-create a missing prop */` |
|         - | 5523 | `#define PH7_MEMBER_REF_TARGET 6 /* reference-store target ($o->p =& $x, C::$s =& $x): resolve the` |
|         - | 5524 | `                                 * property slot and stash it for the following OP_STORE_REF; skip` |
|         - | 5525 | `                                 * the read/hook/magic machinery (a ref bind neither reads nor coerces) */` |
|         - | 5526 | `#define PH7_MEMBER_DEFPATH 7    /* D1 commit 2: deferred by-ref/by-value property call arg ($o->p). Reads a` |
|         - | 5527 | `                                 * present property (like READ); on a miss/magic, records the lvalue path` |
|         - | 5528 | `                                 * (MEMOBJ_AUX_DEFPATH) that OP_CALL re-walks in vivify or read+warn mode */` |
|         - | 5529 | ``#define PH7_MEMBER_COALESCE 9 /* `$o->p ?? d`: php's THIRD accessor level, between a read and an`` |
|         - | 5530 | `                               * isset(). Silent on a read-miss like isset()/empty(), but the` |
|         - | 5531 | `                               * expression takes the property's VALUE, not a truth: __isset()` |
|         - | 5532 | `                               * GATES the access and __get() (or a get HOOK) ANSWERS it, and` |
|         - | 5533 | `                               * with no __isset declared the accessor answers on its own.` |
|         - | 5534 | ``                               * `??` used to compile as PH7_MEMBER_ISSET, so every accessor`` |
|         - | 5535 | `                               * path handed the coalesce a BOOLEAN. */` |
|         - | 5536 | `#define PH7_MEMBER_LIST_TARGET 8 /* positional list-destructuring store target ([$o->p] = [...]): a pure` |
|         - | 5537 | `                                  * write whose value arrives only at the following OP_LOAD_LIST, which` |
|         - | 5538 | `                                  * writes the slot directly (with typed-slot enforcement). Skip the` |
|         - | 5539 | `                                  * uninitialized-typed read Error and the __get consult, and vivify a` |
|         - | 5540 | `                                  * missing property like a write base */` |
|         - | 5541 | `/* -- END-OF INSTRUCTIONS -- */` |
|         - | 5542 | `/*` |
|         - | 5543 | ` * Expression Operators ID.` |
|         - | 5544 | ` */` |
|         - | 5545 | `enum ph7_expr_id {` |
|         - | 5546 | `	EXPR_OP_NEW = 1,   /* new */` |
|         - | 5547 | `	EXPR_OP_CLONE,     /* clone */` |
|         - | 5548 | `	EXPR_OP_ARROW,     /* -> */` |
|         - | 5549 | `	EXPR_OP_NULLSAFE_ARROW, /* ?-> (PHP 8.0 nullsafe) */` |
|         - | 5550 | `	EXPR_OP_DC,        /* :: */` |
|         - | 5551 | `	EXPR_OP_SUBSCRIPT, /* []: Subscripting */` |
|         - | 5552 | `	EXPR_OP_FUNC_CALL, /* func_call() */` |
|         - | 5553 | `	EXPR_OP_INCR,      /* ++ */` |
|         - | 5554 | `	EXPR_OP_DECR,      /* -- */` |
|         - | 5555 | `	EXPR_OP_BITNOT,    /* ~ */` |
|         - | 5556 | `	EXPR_OP_UMINUS,    /* Unary minus  */` |
|         - | 5557 | `	EXPR_OP_UPLUS,     /* Unary plus */` |
|         - | 5558 | `	EXPR_OP_TYPECAST,  /* Type cast [i.e: (int),(float),(string)...] */` |
|         - | 5559 | `	EXPR_OP_ALT,       /* @ */` |
|         - | 5560 | `	EXPR_OP_INSTOF,    /* instanceof */` |
|         - | 5561 | `	EXPR_OP_LOGNOT,    /* logical not ! */` |
|         - | 5562 | `	EXPR_OP_MUL,       /* Multiplication */` |
|         - | 5563 | `	EXPR_OP_DIV,       /* division */` |
|         - | 5564 | `	EXPR_OP_MOD,       /* Modulus */` |
|         - | 5565 | `	EXPR_OP_POW,       /* Exponentiation ** */` |
|         - | 5566 | `	EXPR_OP_ADD,       /* Addition */` |
|         - | 5567 | `	EXPR_OP_SUB,       /* Substraction */` |
|         - | 5568 | `	EXPR_OP_DOT,       /* Concatenation */` |
|         - | 5569 | `	EXPR_OP_SHL,       /* Left shift */` |
|         - | 5570 | `	EXPR_OP_SHR,       /* Right shift */` |
|         - | 5571 | `	EXPR_OP_LT,        /* Less than */` |
|         - | 5572 | `	EXPR_OP_LE,        /* Less equal */` |
|         - | 5573 | `	EXPR_OP_GT,        /* Greater than */` |
|         - | 5574 | `	EXPR_OP_GE,        /* Greater equal */` |
|         - | 5575 | `	EXPR_OP_SPACESHIP, /* Spaceship <=> */` |
|         - | 5576 | `	EXPR_OP_EQ,        /* Equal == */` |
|         - | 5577 | `	EXPR_OP_NE,        /* Not equal != <> */` |
|         - | 5578 | `	EXPR_OP_TEQ,       /* Type equal === */` |
|         - | 5579 | `	EXPR_OP_TNE,       /* Type not equal !== */` |
|         - | 5580 | `	EXPR_OP_BAND,      /* Biwise and '&' */` |
|         - | 5581 | `	EXPR_OP_REF,       /* Reference operator '&' */` |
|         - | 5582 | `	EXPR_OP_XOR,       /* bitwise xor '^' */` |
|         - | 5583 | `	EXPR_OP_BOR,       /* bitwise or '\|' */` |
|         - | 5584 | `	EXPR_OP_LAND,      /* Logical and '&&','and' */` |
|         - | 5585 | `	EXPR_OP_LOR,       /* Logical or  '\|\|','or'*/` |
|         - | 5586 | `	EXPR_OP_LXOR,      /* Logical xor 'xor' */` |
|         - | 5587 | `	EXPR_OP_QUESTY,    /* Ternary operator '?' */` |
|         - | 5588 | `	EXPR_OP_NULLC,     /* Null coalescing '??' */` |
|         - | 5589 | `	EXPR_OP_ASSIGN,    /* Assignment '=' */` |
|         - | 5590 | `	EXPR_OP_ADD_ASSIGN, /* Combined operator: += */` |
|         - | 5591 | `	EXPR_OP_SUB_ASSIGN, /* Combined operator: -= */` |
|         - | 5592 | `	EXPR_OP_MUL_ASSIGN, /* Combined operator: *= */` |
|         - | 5593 | `	EXPR_OP_DIV_ASSIGN, /* Combined operator: /= */` |
|         - | 5594 | `	EXPR_OP_MOD_ASSIGN, /* Combined operator: %= */` |
|         - | 5595 | `	EXPR_OP_POW_ASSIGN, /* Combined operator: **= */` |
|         - | 5596 | `	EXPR_OP_DOT_ASSIGN, /* Combined operator: .= */` |
|         - | 5597 | `	EXPR_OP_AND_ASSIGN, /* Combined operator: &= */` |
|         - | 5598 | `	EXPR_OP_OR_ASSIGN,  /* Combined operator: \|= */` |
|         - | 5599 | `	EXPR_OP_XOR_ASSIGN, /* Combined operator: ^= */` |
|         - | 5600 | `	EXPR_OP_SHL_ASSIGN, /* Combined operator: <<= */` |
|         - | 5601 | `	EXPR_OP_SHR_ASSIGN, /* Combined operator: >>= */` |
|         - | 5602 | `	EXPR_OP_NULLC_ASSIGN, /* Combined operator: null coalescing assign */` |
|         - | 5603 | `	EXPR_OP_PIPE,       /* PHP 8.5 pipe operator: \|> */` |
|         - | 5604 | `	EXPR_OP_COMMA       /* Comma expression */` |
|         - | 5605 | `};` |
|         - | 5606 | `/*` |
|         - | 5607 | ` * Very high level tokens.` |
|         - | 5608 | ` */` |
|         - | 5609 | `#define PH7_TOKEN_RAW 0x001 /* Raw text [i.e: HTML,XML...] */` |
|         - | 5610 | `#define PH7_TOKEN_PHP 0x002 /* PHP chunk */` |
|         - | 5611 | `/*` |
|         - | 5612 | ` * Lexer token codes` |
|         - | 5613 | ` * The following set of constants are the tokens recognized` |
|         - | 5614 | ` * by the lexer when processing PHP input.` |
|         - | 5615 | ` * Important: Token values MUST BE A POWER OF TWO.` |
|         - | 5616 | ` */` |
|         - | 5617 | `#define PH7_TK_INTEGER   0x0000001  /* Integer */` |
|         - | 5618 | `#define PH7_TK_REAL      0x0000002  /* Real number */` |
|         - | 5619 | `#define PH7_TK_NUM       (PH7_TK_INTEGER\|PH7_TK_REAL) /* Numeric token,either integer or real */` |
|         - | 5620 | `#define PH7_TK_KEYWORD   0x0000004 /* Keyword [i.e: while,for,if,foreach...] */` |
|         - | 5621 | `#define PH7_TK_ID        0x0000008 /* Alphanumeric or UTF-8 stream */` |
|         - | 5622 | `#define PH7_TK_DOLLAR    0x0000010 /* '$' Dollar sign */` |
|         - | 5623 | `#define PH7_TK_OP        0x0000020 /* Operator [i.e: +,*,/...] */` |
|         - | 5624 | `#define PH7_TK_OCB       0x0000040 /* Open curly brace'{' */` |
|         - | 5625 | `#define PH7_TK_CCB       0x0000080 /* Closing curly brace'}' */` |
|         - | 5626 | `#define PH7_TK_NSSEP     0x0000100 /* Namespace separator '\' */` |
|         - | 5627 | `#define PH7_TK_LPAREN    0x0000200 /* Left parenthesis '(' */` |
|         - | 5628 | `#define PH7_TK_RPAREN    0x0000400 /* Right parenthesis ')' */` |
|         - | 5629 | `#define PH7_TK_OSB       0x0000800 /* Open square bracket '[' */` |
|         - | 5630 | `#define PH7_TK_CSB       0x0001000 /* Closing square bracket ']' */` |
|         - | 5631 | `#define PH7_TK_DSTR      0x0002000 /* Double quoted string "$str" */` |
|         - | 5632 | `#define PH7_TK_SSTR      0x0004000 /* Single quoted string 'str' */` |
|         - | 5633 | `#define PH7_TK_HEREDOC   0x0008000 /* Heredoc <<< */` |
|         - | 5634 | `#define PH7_TK_NOWDOC    0x0010000 /* Nowdoc <<< */` |
|         - | 5635 | `#define PH7_TK_COMMA     0x0020000 /* Comma ',' */` |
|         - | 5636 | `#define PH7_TK_SEMI      0x0040000 /* Semi-colon ";" */` |
|         - | 5637 | ``#define PH7_TK_BSTR      0x0080000 /* Backtick quoted string [i.e: Shell command `date`] */`` |
|         - | 5638 | `#define PH7_TK_COLON     0x0100000 /* single Colon ':' */` |
|         - | 5639 | `#define PH7_TK_AMPER     0x0200000 /* Ampersand '&' */` |
|         - | 5640 | `#define PH7_TK_EQUAL     0x0400000 /* Equal '=' */` |
|         - | 5641 | `#define PH7_TK_ARRAY_OP  0x0800000 /* Array operator '=>' */` |
|         - | 5642 | `#define PH7_TK_ELLIPSIS  0x1000000 /* Ellipsis '...' */` |
|         - | 5643 | `#define PH7_TK_OTHER     0x2000000 /* Other symbols */` |
|         - | 5644 | ``#define PH7_TK_VOID_CAST 0x8000000 /* php 8.5's `(void)` cast, assembled by the lexer from the three`` |
|         - | 5645 | `                                      * tokens the way every other cast operator is. It is NOT an` |
|         - | 5646 | `                                      * expression operator: php's grammar takes it only at the head` |
|         - | 5647 | ``                                      * of an expression STATEMENT or of a `for` clause, so anywhere`` |
|         - | 5648 | `                                      * else it stays an unrecognized token and the parser reports` |
|         - | 5649 | ``                                      * php's `unexpected token "(void)"`. */`` |
|         - | 5650 | `#define PH7_TK_UNTERM    0x10000000 /* The lexeme ran into the END OF THE INPUT without its closing` |
|         - | 5651 | `                                     * delimiter: an unterminated quote, heredoc or block comment.` |
|         - | 5652 | `                                     * php refuses each of those; this engine used to consume them` |
|         - | 5653 | `                                     * up to EOF and run the program. */` |
|         - | 5654 | ``#define PH7_TK_FQNAME    0x20000000 /* A PH7_TK_OTHER whose text is a whole `\A\B` name standing where`` |
|         - | 5655 | ``                                    * php's grammar has no place for one (`A \B`): php's scanner made it`` |
|         - | 5656 | `                                    * ONE T_NAME_FULLY_QUALIFIED token and its parse error names it so.` |
|         - | 5657 | `                                    * The lexer takes the whole name into the token for that message. */` |
|         - | 5658 | `#define PH7_TK_ALIAS_CAST 0x40000000 /* A cast written with one of php's four NON-CANONICAL spellings --` |
|         - | 5659 | `                                      * (integer), (boolean), (double), (binary). The lexer hands the` |
|         - | 5660 | `                                      * parser the canonical token text, so the alias is gone by the` |
|         - | 5661 | `                                      * time anything downstream could report it; this bit is what` |
|         - | 5662 | `                                      * remembers that the source said the other word, and the alias` |
|         - | 5663 | `                                      * is recoverable from the canonical name because each of the` |
|         - | 5664 | `                                      * four is the only alias of its target. */` |
|         - | 5665 | `#define PH7_TK_MEMBER_NAME 0x4000000 /* Reserved word used as a member NAME right after -> / ?-> / ::` |
|         - | 5666 | `                                      * (Enum::Null, C::Array, $o->list()): a plain identifier, never` |
|         - | 5667 | `                                      * the literal value — GenStateLoadLiteral skips its value conversion. */` |
|         - | 5668 | `/*` |
|         - | 5669 | ` * PHP keyword.` |
|         - | 5670 | ` * These words have special meaning in PHP. Some of them represent things which look like` |
|         - | 5671 | ` * functions, some look like constants, and so on, but they're not, really: they are language constructs.` |
|         - | 5672 | ` * You cannot use any of the following words as constants, class names, function or method names.` |
|         - | 5673 | ` * Using them as variable names is generally OK, but could lead to confusion.` |
|         - | 5674 | ` */` |
|         - | 5675 | `#define PH7_TKWRD_EXTENDS      1 /* extends */` |
|         - | 5676 | `#define PH7_TKWRD_ENDSWITCH    2 /* endswitch */` |
|         - | 5677 | `#define PH7_TKWRD_SWITCH       3 /* switch */` |
|         - | 5678 | `#define PH7_TKWRD_PRINT        4 /* print */` |
|         - | 5679 | `#define PH7_TKWRD_INTERFACE    5 /* interface */` |
|         - | 5680 | `#define PH7_TKWRD_ENDDEC       6 /* enddeclare */` |
|         - | 5681 | `#define PH7_TKWRD_DECLARE      7 /* declare */` |
|         - | 5682 | `/* The number '8' is reserved for PH7_TK_ID */` |
|         - | 5683 | `#define PH7_TKWRD_REQONCE      9 /* require_once */` |
|         - | 5684 | `#define PH7_TKWRD_REQUIRE      10 /* require */` |
|         - | 5685 | `#define PH7_TKWRD_ELIF         0x4000000 /* elseif: MUST BE A POWER OF TWO */` |
|         - | 5686 | `#define PH7_TKWRD_ELSE         0x8000000 /* else:  MUST BE A POWER OF TWO */` |
|         - | 5687 | `#define PH7_TKWRD_IF           13 /* if */` |
|         - | 5688 | `#define PH7_TKWRD_FINAL        14 /* final */` |
|         - | 5689 | `#define PH7_TKWRD_LIST         15 /* list */` |
|         - | 5690 | `#define PH7_TKWRD_STATIC       16 /* static */` |
|         - | 5691 | `#define PH7_TKWRD_CASE         17 /* case */` |
|         - | 5692 | `#define PH7_TKWRD_SELF         18 /* self */` |
|         - | 5693 | `#define PH7_TKWRD_FUNCTION     19 /* function */` |
|         - | 5694 | `#define PH7_TKWRD_NAMESPACE    20 /* namespace */` |
|         - | 5695 | `#define PH7_TKWRD_ENDIF        0x400000 /* endif: MUST BE A POWER OF TWO */` |
|         - | 5696 | `#define PH7_TKWRD_CLONE        0x80 /* clone: MUST BE A POWER OF TWO  */` |
|         - | 5697 | `#define PH7_TKWRD_NEW          0x100 /* new: MUST BE A POWER OF TWO  */` |
|         - | 5698 | `#define PH7_TKWRD_CONST        22 /* const */` |
|         - | 5699 | `#define PH7_TKWRD_THROW        23 /* throw */` |
|         - | 5700 | `#define PH7_TKWRD_USE          24 /* use */` |
|         - | 5701 | `#define PH7_TKWRD_ENDWHILE     0x800000 /* endwhile: MUST BE A POWER OF TWO */` |
|         - | 5702 | `#define PH7_TKWRD_WHILE        26 /* while */` |
|         - | 5703 | `#define PH7_TKWRD_EVAL         27 /* eval */` |
|         - | 5704 | `#define PH7_TKWRD_VAR          28 /* var */` |
|         - | 5705 | `#define PH7_TKWRD_ARRAY        0x200 /* array: MUST BE A POWER OF TWO */` |
|         - | 5706 | `#define PH7_TKWRD_ABSTRACT     29 /* abstract */` |
|         - | 5707 | `#define PH7_TKWRD_TRY          30 /* try */` |
|         - | 5708 | `#define PH7_TKWRD_AND          0x400 /* and: MUST BE A POWER OF TWO  */` |
|         - | 5709 | `#define PH7_TKWRD_DEFAULT      31 /* default */` |
|         - | 5710 | `#define PH7_TKWRD_CLASS        32 /* class */` |
|         - | 5711 | `#define PH7_TKWRD_AS           33 /* as */` |
|         - | 5712 | `#define PH7_TKWRD_CONTINUE     34 /* continue */` |
|         - | 5713 | `#define PH7_TKWRD_EXIT         35 /* exit */` |
|         - | 5714 | `#define PH7_TKWRD_DIE          36 /* die */` |
|         - | 5715 | `#define PH7_TKWRD_ECHO         37 /* echo */` |
|         - | 5716 | `#define PH7_TKWRD_GLOBAL       38 /* global */` |
|         - | 5717 | `#define PH7_TKWRD_IMPLEMENTS   39 /* implements */` |
|         - | 5718 | `#define PH7_TKWRD_INCONCE      40 /* include_once */` |
|         - | 5719 | `#define PH7_TKWRD_INCLUDE      41 /* include */` |
|         - | 5720 | `#define PH7_TKWRD_EMPTY        42 /* empty */` |
|         - | 5721 | `#define PH7_TKWRD_INSTANCEOF   0x800 /* instanceof: MUST BE A POWER OF TWO  */` |
|         - | 5722 | `#define PH7_TKWRD_ISSET        43 /* isset */` |
|         - | 5723 | `#define PH7_TKWRD_PARENT       44 /* parent */` |
|         - | 5724 | `#define PH7_TKWRD_PRIVATE      45 /* private */` |
|         - | 5725 | `#define PH7_TKWRD_ENDFOR       0x1000000 /* endfor: MUST BE A POWER OF TWO */` |
|         - | 5726 | `#define PH7_TKWRD_END4EACH     0x2000000 /* endforeach: MUST BE A POWER OF TWO */` |
|         - | 5727 | `#define PH7_TKWRD_FOR          48 /* for */` |
|         - | 5728 | `#define PH7_TKWRD_FOREACH      49 /* foreach */` |
|         - | 5729 | `#define PH7_TKWRD_OR           0x1000 /* or: MUST BE A POWER OF TWO  */` |
|         - | 5730 | `#define PH7_TKWRD_PROTECTED    50 /* protected */` |
|         - | 5731 | `#define PH7_TKWRD_DO           51 /* do */` |
|         - | 5732 | `#define PH7_TKWRD_PUBLIC       52 /* public */` |
|         - | 5733 | `#define PH7_TKWRD_CATCH        53 /* catch */` |
|         - | 5734 | `#define PH7_TKWRD_RETURN       54 /* return */` |
|         - | 5735 | `#define PH7_TKWRD_UNSET        0x2000 /* unset: MUST BE A POWER OF TWO  */` |
|         - | 5736 | `#define PH7_TKWRD_XOR          0x4000 /* xor: MUST BE A POWER OF TWO  */` |
|         - | 5737 | `#define PH7_TKWRD_BREAK        55 /* break */` |
|         - | 5738 | `#define PH7_TKWRD_GOTO         56 /* goto */` |
|         - | 5739 | `#define PH7_TKWRD_TRAIT        57 /* trait */` |
|         - | 5740 | `#define PH7_TKWRD_INSTEADOF    58 /* insteadof */` |
|         - | 5741 | `#define PH7_TKWRD_FINALLY      59 /* finally */` |
|         - | 5742 | `#define PH7_TKWRD_YIELD        60 /* yield */` |
|         - | 5743 | `#define PH7_TKWRD_FN           61 /* fn (PHP 7.4 arrow function) */` |
|         - | 5744 | `#define PH7_TKWRD_MATCH        62 /* match (PHP 8.0 match expression) */` |
|         - | 5745 | `#define PH7_TKWRD_BOOL         0x8000  /* bool:  MUST BE A POWER OF TWO */` |
|         - | 5746 | `#define PH7_TKWRD_INT          0x10000  /* int:   MUST BE A POWER OF TWO */` |
|         - | 5747 | `#define PH7_TKWRD_FLOAT        0x20000  /* float:  MUST BE A POWER OF TWO */` |
|         - | 5748 | `#define PH7_TKWRD_STRING       0x40000  /* string: MUST BE A POWER OF TWO */` |
|         - | 5749 | `#define PH7_TKWRD_OBJECT       0x80000 /* object: MUST BE A POWER OF TWO */` |
|         - | 5750 | `/* 0x100000 and 0x200000 are free: they were the PH7-ism 'eq'/'ne' string` |
|         - | 5751 | ` * comparison operators, removed so both stay usable as plain identifiers. */` |
|         - | 5752 | `/*` |
|         - | 5753 | ` * PHP-exact ENT_* flag values for the html-entity family. Single source of` |
|         - | 5754 | ` * truth: constant.c declares the PHP-visible ENT_* constants from these and` |
|         - | 5755 | ` * builtin.c implements the semantics against them. The low two bits are the` |
|         - | 5756 | ` * quote bits (ENT_QUOTES = both, ENT_COMPAT = double only, ENT_NOQUOTES = 0)` |
|         - | 5757 | ` * and bits 16\|32 select the doctype — composites, not independent flags.` |
|         - | 5758 | ` */` |
|         - | 5759 | `#define PH7_ENT_QUOTE_SINGLE 0x01 /* encode/decode ' */` |
|         - | 5760 | `#define PH7_ENT_QUOTE_DOUBLE 0x02 /* encode/decode " (== ENT_COMPAT) */` |
|         - | 5761 | `#define PH7_ENT_QUOTES       (PH7_ENT_QUOTE_DOUBLE\|PH7_ENT_QUOTE_SINGLE)` |
|         - | 5762 | `#define PH7_ENT_IGNORE       0x04 /* drop invalid UTF-8 units */` |
|         - | 5763 | `#define PH7_ENT_SUBSTITUTE   0x08 /* invalid UTF-8 unit -> U+FFFD */` |
|         - | 5764 | `#define PH7_ENT_DOC_MASK     0x30 /* doctype selector */` |
|         - | 5765 | `#define PH7_ENT_DOC_HTML401  0x00` |
|         - | 5766 | `#define PH7_ENT_DOC_XML1     0x10` |
|         - | 5767 | `#define PH7_ENT_DOC_XHTML    0x20` |
|         - | 5768 | `#define PH7_ENT_DOC_HTML5    0x30` |
|         - | 5769 | `#define PH7_ENT_DISALLOWED   0x80 /* substitute doctype-disallowed codepoints */` |
|         - | 5770 | `/* The shared default for all five builtins (php 8.1+): ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401. */` |
|         - | 5771 | `#define PH7_ENT_DEFAULT      (PH7_ENT_QUOTES\|PH7_ENT_SUBSTITUTE\|PH7_ENT_DOC_HTML401)` |
|         - | 5772 | `/* JSON encoding/decoding related definition */` |
|         - | 5773 | `enum json_err_code{` |
|         - | 5774 | `	JSON_ERROR_NONE = 0,  /* No error has occurred. */` |
|         - | 5775 | `	JSON_ERROR_DEPTH,     /* The maximum stack depth has been exceeded.  */` |
|         - | 5776 | `	JSON_ERROR_STATE_MISMATCH, /* Occurs with underflow or with the modes mismatch.  */` |
|         - | 5777 | `	JSON_ERROR_CTRL_CHAR, /* Control character error, possibly incorrectly encoded.  */` |
|         - | 5778 | `	JSON_ERROR_SYNTAX,    /* Syntax error. */` |
|         - | 5779 | `	JSON_ERROR_UTF8,      /* Malformed UTF-8 characters */` |
|         - | 5780 | `	JSON_ERROR_RECURSION, /* A container already being encoded shows up inside itself (php value 6) */` |
|         - | 5781 | `	JSON_ERROR_INF_OR_NAN = 7, /* Inf or NaN given to json_encode (php value) */` |
|         - | 5782 | `	JSON_ERROR_UNSUPPORTED_TYPE = 8, /* A resource given to json_encode (php value) */` |
|         - | 5783 | `	JSON_ERROR_INVALID_PROPERTY_NAME = 9, /* Object-mode decode of a property name with a` |
|         - | 5784 | `	                                       * LEADING NUL byte — php reserves that prefix for` |
|         - | 5785 | `	                                       * mangled private/protected names (php value) */` |
|         - | 5786 | `	JSON_ERROR_UTF16 = 10, /* Unpaired UTF-16 surrogate in a \uXXXX escape (php value) */` |
|         - | 5787 | `	JSON_ERROR_NON_BACKED_ENUM = 11 /* Non-backed enum given to json_encode (php 8.1 value) */` |
|         - | 5788 | `};` |
|         - | 5789 | `/* The following constants can be combined to form options for json_encode(). */` |
|         - | 5790 | `#define	JSON_HEX_TAG           0x01  /* All < and > are converted to \u003C and \u003E. */` |
|         - | 5791 | `#define JSON_HEX_AMP           0x02  /* All &s are converted to \u0026. */` |
|         - | 5792 | `#define JSON_HEX_APOS          0x04  /* All ' are converted to \u0027. */` |
|         - | 5793 | `#define JSON_HEX_QUOT          0x08  /* All " are converted to \u0022. */` |
|         - | 5794 | `#define JSON_FORCE_OBJECT      0x10  /* Outputs an object rather than an array */` |
|         - | 5795 | `#define JSON_NUMERIC_CHECK     0x20  /* Encodes numeric strings as numbers. */` |
|         - | 5796 | `#define JSON_PRETTY_PRINT      0x80  /* Use whitespace in returned data to format it.*/` |
|         - | 5797 | `#define JSON_UNESCAPED_SLASHES 0x40  /* Don't escape '/' */` |
|         - | 5798 | `#define JSON_UNESCAPED_UNICODE 0x100 /* Emit multibyte UTF-8 raw instead of \uXXXX */` |
|         - | 5799 | `#define JSON_PARTIAL_OUTPUT_ON_ERROR 0x200 /* Substitute (0 / null / "") for an unencodable` |
|         - | 5800 | `                                            * piece and record the error instead of failing */` |
|         - | 5801 | `#define JSON_PRESERVE_ZERO_FRACTION  0x400 /* A float with no fractional digits prints ".0"` |
|         - | 5802 | `                                            * (1.0 encodes as "1.0", not "1") */` |
|         - | 5803 | `#define JSON_UNESCAPED_LINE_TERMINATORS 0x800 /* ...U+2028/U+2029 included */` |
|         - | 5804 | `#define JSON_INVALID_UTF8_IGNORE     0x100000 /* Drop ill-formed UTF-8 instead of failing */` |
|         - | 5805 | `#define JSON_INVALID_UTF8_SUBSTITUTE 0x200000 /* ...replace it with U+FFFD */` |
|         - | 5806 | `#define JSON_THROW_ON_ERROR    0x400000 /* Throw JsonException on encode/decode error */` |
|         - | 5807 | `/* DECODE flags: php numbers the decode options in their own space, so each shares a` |
|         - | 5808 | ` * bit with an encode flag exactly as php's JSON_BIGINT_AS_STRING shares 2 with` |
|         - | 5809 | ` * JSON_HEX_AMP (and JSON_OBJECT_AS_ARRAY shares 1 with JSON_HEX_TAG). */` |
|         - | 5810 | `#define JSON_OBJECT_AS_ARRAY   0x01` |
|         - | 5811 | `#define JSON_BIGINT_AS_STRING  0x02` |
|         - | 5812 | `/*` |
|         - | 5813 | ` * extract() $flags — php's ENUM (ext/standard/php_array.h), not a bitmask.` |
|         - | 5814 | ` * PH7 exposed a legacy power-of-two bitmask here (1/2/4/8/16/32/64), which` |
|         - | 5815 | ` * changed the meaning of valid php source: extract($a,1) is EXTR_SKIP in php` |
|         - | 5816 | ` * but was EXTR_OVERWRITE in PHL, and EXTR_PREFIX_ALL printed 8 instead of 3.` |
|         - | 5817 | ` * The values below ARE php's, and vm_builtin_extract() dispatches on` |
|         - | 5818 | ` * (flags & 0xff) exactly like php does.` |
|         - | 5819 | ` */` |
|         - | 5820 | `#define PH7_EXTR_OVERWRITE        0` |
|         - | 5821 | `#define PH7_EXTR_SKIP             1` |
|         - | 5822 | `#define PH7_EXTR_PREFIX_SAME      2` |
|         - | 5823 | `#define PH7_EXTR_PREFIX_ALL       3` |
|         - | 5824 | `#define PH7_EXTR_PREFIX_INVALID   4` |
|         - | 5825 | `#define PH7_EXTR_PREFIX_IF_EXISTS 5` |
|         - | 5826 | `#define PH7_EXTR_IF_EXISTS        6` |
|         - | 5827 | `#define PH7_EXTR_REFS             0x100 /* php's by-reference extraction (rides above the mode) */` |
|         - | 5828 | `/*` |
|         - | 5829 | ` * pathinfo() $flags, glob() $flags and parse_ini_*() $scanner_mode — php's VALUES.` |
|         - | 5830 | ` *` |
|         - | 5831 | ` * Each of these was a PH7 invention (pathinfo counted 1/2/3/4 where php's are POWERS` |
|         - | 5832 | ` * OF TWO, glob used its own 1..64 ladder, and the ini scanner started at 1), which` |
|         - | 5833 | `` * changes the meaning of valid php source: `PATHINFO_DIRNAME\|PATHINFO_BASENAME` is 3`` |
|         - | 5834 | ` * in both engines but PHL read 3 as PATHINFO_EXTENSION, a script passing php's literal` |
|         - | 5835 | ` * 64 to json_encode() got JSON_BIGINT_AS_STRING instead of JSON_UNESCAPED_SLASHES, and` |
|         - | 5836 | ` * INI_SCANNER_RAW (php 1) selected nothing.` |
|         - | 5837 | ` *` |
|         - | 5838 | ` * The GLOB_* values are php 8.5's OWN portable set (main/php_glob.h, new in 8.5), which` |
|         - | 5839 | ` * a default build uses on every platform including MSVC — the bundled branch is taken` |
|         - | 5840 | ` * unless the POSIX build is configured with --enable-system-glob (off by default), and` |
|         - | 5841 | ` * the win32 build has no such option. Do NOT read them off the host <glob.h>: glibc's` |
|         - | 5842 | ` * ladder is different (MARK 2, NOSORT 4, BRACE 1024, ONLYDIR 8192), and so was php's` |
|         - | 5843 | ` * own pre-8.5 win32/glob.h (NOESCAPE 0x2000). PATHINFO_*, INI_SCANNER_* and JSON_* are` |
|         - | 5844 | ` * plain #defines in ext/standard and ext/json, never platform-conditional.` |
|         - | 5845 | ` */` |
|         - | 5846 | `#define PH7_PATHINFO_DIRNAME    1` |
|         - | 5847 | `#define PH7_PATHINFO_BASENAME   2` |
|         - | 5848 | `#define PH7_PATHINFO_EXTENSION  4` |
|         - | 5849 | `#define PH7_PATHINFO_FILENAME   8` |
|         - | 5850 | `#define PH7_PATHINFO_ALL        (PH7_PATHINFO_DIRNAME\|PH7_PATHINFO_BASENAME\|\` |
|         - | 5851 | `                                 PH7_PATHINFO_EXTENSION\|PH7_PATHINFO_FILENAME)` |
|         - | 5852 | `#define PH7_GLOB_ERR            0x0004` |
|         - | 5853 | `#define PH7_GLOB_MARK           0x0008` |
|         - | 5854 | `#define PH7_GLOB_NOCHECK        0x0010` |
|         - | 5855 | `#define PH7_GLOB_NOSORT         0x0020` |
|         - | 5856 | `#define PH7_GLOB_BRACE          0x0080` |
|         - | 5857 | `#define PH7_GLOB_NOESCAPE       0x1000` |
|         - | 5858 | `#define PH7_GLOB_ONLYDIR        0x40000000` |
|         - | 5859 | `#define PH7_INI_SCANNER_NORMAL  0` |
|         - | 5860 | `#define PH7_INI_SCANNER_RAW     1` |
|         - | 5861 | `#define PH7_INI_SCANNER_TYPED   2` |
|         - | 5862 | `/* php's INI_SCANNER_TYPED is 2 — not defined here because PHL does not register the` |
|         - | 5863 | ` * constant (nor honour any scanner mode yet); it is tracked with the missing JSON_*. */` |
|         - | 5864 | `/*` |
|         - | 5865 | ` * Each parsed URI is recorded and stored in an instance of the following structure.` |
|         - | 5866 | ` */` |
|         - | 5867 | `typedef struct SyhttpUri SyhttpUri;` |
|         - | 5868 | `struct SyhttpUri` |
|         - | 5869 | `{` |
|         - | 5870 | `	SyString sHost;     /* Hostname or IP address */` |
|         - | 5871 | `	SyString sPort;     /* Port number */` |
|         - | 5872 | `	SyString sPath;     /* Mandatory resource path passed verbatim (Not decoded) */` |
|         - | 5873 | `	SyString sQuery;    /* Query part */` |
|         - | 5874 | `	SyString sFragment; /* Fragment part */` |
|         - | 5875 | `	SyString sScheme;   /* Scheme */` |
|         - | 5876 | `	SyString sUser;     /* Username */` |
|         - | 5877 | `	SyString sPass;     /* Password */` |
|         - | 5878 | `	SyString sRaw;      /* Raw URI */` |
|         - | 5879 | `};` |
|         - | 5880 | `/*` |
|         - | 5881 | ` * An instance of the following structure is used to record all MIME headers seen` |
|         - | 5882 | ` * during a HTTP interaction.` |
|         - | 5883 | ` */` |
|         - | 5884 | `typedef struct SyhttpHeader SyhttpHeader;` |
|         - | 5885 | `struct SyhttpHeader` |
|         - | 5886 | `{` |
|         - | 5887 | `	SyString sName;    /* Header name [i.e:"Content-Type","Host","User-Agent"]. NOT NUL TERMINATED */` |
|         - | 5888 | `	SyString sValue;   /* Header values [i.e: "text/html"]. NOT NUL TERMINATED */` |
|         - | 5889 | `};` |
|         - | 5890 | `/*` |
|         - | 5891 | ` * Supported HTTP methods.` |
|         - | 5892 | ` */` |
|         - | 5893 | `#define HTTP_METHOD_GET  1 /* GET */` |
|         - | 5894 | `#define HTTP_METHOD_HEAD 2 /* HEAD */` |
|         - | 5895 | `#define HTTP_METHOD_POST 3 /* POST */` |
|         - | 5896 | `#define HTTP_METHOD_PUT  4 /* PUT */` |
|         - | 5897 | `#define HTTP_METHOD_OTHR 5 /* Other HTTP methods [i.e: DELETE,TRACE,OPTIONS...]*/` |
|         - | 5898 | `/*` |
|         - | 5899 | ` * Supported HTTP protocol version.` |
|         - | 5900 | ` */` |
|         - | 5901 | `#define HTTP_PROTO_10 1 /* HTTP/1.0 */` |
|         - | 5902 | `#define HTTP_PROTO_11 2 /* HTTP/1.1 */` |
|         - | 5903 | `/* memobj.c function prototypes */` |
|         - | 5904 | `/*` |
|         - | 5905 | ` * Bound on how deep var_dump()/print_r()/var_export() will walk. php has NO limit:` |
|         - | 5906 | ` * it recurses until the process runs out of memory (a 10,000-level array dumps 300 MB` |
|         - | 5907 | ` * and a 100,000-level one dies on the allocator, not on a depth). PHL walks the same` |
|         - | 5908 | ` * tree on the same C stack, so it needs one — this is a backstop for pathological` |
|         - | 5909 | ` * FINITE nesting, not a cycle guard: a container that is its own descendant is caught` |
|         - | 5910 | ` * by PH7_MemObjDumpIsRecursive() and rendered as php's *RECURSION*. Matches` |
|         - | 5911 | ` * SERIALIZE_MAX_DEPTH, the bound the serializer walk has always used, and the one` |
|         - | 5912 | ` * var_export has carried since it was written. Costs ~250 bytes of C stack per level,` |
|         - | 5913 | ` * so a host that hands the engine less than ~2 MB of stack wants a lower one.` |
|         - | 5914 | ` */` |
|         - | 5915 | `#define PH7_DUMP_MAX_DEPTH 4096` |
|         - | 5916 | `PH7_PRIVATE sxi32 PH7_MemObjDump(SyBlob *pOut,ph7_value *pObj,int ShowType,int nTab,int nDepth,int isRef);` |
|         - | 5917 | `PH7_PRIVATE int PH7_MemObjDumpIsRecursive(ph7_value *pObj);` |
|         - | 5918 | `PH7_PRIVATE const char * PH7_MemObjTypeDump(ph7_value *pVal);` |
|         - | 5919 | `PH7_PRIVATE sxi32 PH7_MemObjAdd(ph7_value *pObj1,ph7_value *pObj2,int bAddStore);` |
|         - | 5920 | `/*` |
|         - | 5921 | ` * Bound on how deep PH7_MemObjCmp() will walk. Like PH7_DUMP_MAX_DEPTH this is a` |
|         - | 5922 | ` * backstop for pathological FINITE nesting, NOT the cycle guard: a container that is` |
|         - | 5923 | ` * its own descendant is caught by HASHMAP_COMPARING / VM_INSTANCE_COMPARING, php's own` |
|         - | 5924 | ` * mechanism. php has no depth limit here either -- it compares a 200-level graph and` |
|         - | 5925 | ` * dies on the C stack, not on a count -- so this only has to sit above anything real.` |
|         - | 5926 | ` * Each array level spends TWO counts (PH7_HashmapCmp, then HashmapNodeCmp) and roughly` |
|         - | 5927 | ` * 400 bytes of C stack, so 4096 needs under 1 MB.` |
|         - | 5928 | ` */` |
|         - | 5929 | `#define PH7_CMP_MAX_DEPTH 4096` |
|         - | 5930 | `PH7_PRIVATE sxi32 PH7_MemObjCmp(ph7_value *pObj1,ph7_value *pObj2,int bStrict,int iNest);` |
|         - | 5931 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromString(ph7_vm *pVm,ph7_value *pObj,const SyString *pVal);` |
|         - | 5932 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromArray(ph7_vm *pVm,ph7_value *pObj,ph7_hashmap *pArray);` |
|         - | 5933 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromReal(ph7_vm *pVm,ph7_value *pObj,ph7_real rVal);` |
|         - | 5934 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromInt(ph7_vm *pVm,ph7_value *pObj,sxi64 iVal);` |
|         - | 5935 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromBool(ph7_vm *pVm,ph7_value *pObj,sxi32 iVal);` |
|         - | 5936 | `PH7_PRIVATE sxi32 PH7_MemObjInit(ph7_vm *pVm,ph7_value *pObj);` |
|         - | 5937 | `PH7_PRIVATE sxi32 PH7_MemObjStringAppend(ph7_value *pObj,const char *zData,sxu32 nLen);` |
|         - | 5938 | `#if 0` |
|         - | 5939 | `/* Not used in the current release of the PH7 engine */` |
|         - | 5940 | `PH7_PRIVATE sxi32 PH7_MemObjStringFormat(ph7_value *pObj,const char *zFormat,va_list ap);` |
|         - | 5941 | `#endif` |
|         - | 5942 | `PH7_PRIVATE sxi32 PH7_MemObjStore(ph7_value *pSrc,ph7_value *pDest);` |
|         - | 5943 | `/*` |
|         - | 5944 | ` * The components php's parse_url() answers, and the split that produces them.` |
|         - | 5945 | ` * Shared with filter_var()'s FILTER_VALIDATE_URL, which php builds on the same` |
|         - | 5946 | ` * parse (a component that is ABSENT is what its b* flag reports; an empty one` |
|         - | 5947 | ` * is present-and-empty).` |
|         - | 5948 | ` */` |
|         - | 5949 | `typedef struct VmUrlParts VmUrlParts;` |
|         - | 5950 | `struct VmUrlParts` |
|         - | 5951 | `{` |
|         - | 5952 | `	SyString sScheme,sUser,sPass,sHost,sPath,sQuery,sFragment;` |
|         - | 5953 | `	int iPort;     /* Resolved port, meaningful only when bPort is set */` |
|         - | 5954 | `	sxu8 bScheme,bUser,bPass,bHost,bPort,bPath,bQuery,bFragment;` |
|         - | 5955 | `};` |
|         - | 5956 | `PH7_PRIVATE int PH7_VmUrlSplit(const char *z,int n,VmUrlParts *pOut);` |
|         - | 5957 | `/*` |
|         - | 5958 | ` * PHL_VALUE_CENSUS -- the value-primitive census (memobj.c has the` |
|         - | 5959 | ` * instrument and build-aux/valuecensus.sh drives it). Off in every shipping build.` |
|         - | 5960 | ` *` |
|         - | 5961 | ` * PHL_VC_DOOR is what makes a row a call SITE: the two hot doors below are inlined` |
|         - | 5962 | ` * in the build that ships, and __builtin_return_address(0) inside an INLINED function` |
|         - | 5963 | ` * names the caller's caller. Under the census they are compiled out of line, so a row` |
|         - | 5964 | `` * is the line that called. `unused` is on it because a translation unit that never`` |
|         - | 5965 | `` * calls the door would otherwise warn -- gcc refuses `inline` and `noinline` together,`` |
|         - | 5966 | ` * so the usual static-inline exemption is not available here.` |
|         - | 5967 | ` *` |
|         - | 5968 | ` * PHL_VCENSUS_CALLER lifts every row one frame -- the hashcensus -c convention, and` |
|         - | 5969 | ` * the same warning applies: it needs -fno-omit-frame-pointer, and it is how a row that` |
|         - | 5970 | ` * is a whole subsystem funnelled through one line gets decomposed.` |
|         - | 5971 | ` */` |
|         - | 5972 | `#if defined(PHL_VALUE_CENSUS)` |
|         - | 5973 | `#define PHL_VC_RELEASE 0` |
|         - | 5974 | `#define PHL_VC_LOAD    1` |
|         - | 5975 | `#define PHL_VC_STORE   2` |
|         - | 5976 | `#define PHL_VC_INIT    3` |
|         - | 5977 | `#define PHL_VC_KINDS   4` |
|         - | 5978 | `PH7_PRIVATE void PH7_ValueCensusNote(void *pSite,sxu32 iKind,int bWork);` |
|         - | 5979 | `#if defined(PHL_VCENSUS_CALLER)` |
|         - | 5980 | `#define PHL_VCENSUS_SITE() __builtin_return_address(1)` |
|         - | 5981 | `#else` |
|         - | 5982 | `#define PHL_VCENSUS_SITE() __builtin_return_address(0)` |
|         - | 5983 | `#endif` |
|         - | 5984 | `#define PHL_VC_NOTE(K,W) PH7_ValueCensusNote(PHL_VCENSUS_SITE(),(K),(W))` |
|         - | 5985 | `#define PHL_VC_DOOR static __attribute__((noinline,unused))` |
|         - | 5986 | `#else` |
|         - | 5987 | `#define PHL_VC_NOTE(K,W) ((void)0)` |
|         - | 5988 | `#define PHL_VC_DOOR SX_STATIC_INLINE` |
|         - | 5989 | `#endif` |
|         - | 5990 | `/* Take/drop the reference a ph7_value holds on a stream handle. Both accept ANY` |
|         - | 5991 | ` * resource pointer and do nothing unless it is a live io_private -- see nValRef. */` |
|         - | 5992 | `PH7_PRIVATE int PH7_StreamValueRef(void *pResource);` |
|         - | 5993 | `PH7_PRIVATE void PH7_StreamValueUnref(void *pResource);` |
|         - | 5994 | `/*` |
|         - | 5995 | ` * Load an ALIASING copy of a value: the destination gets the scalar half verbatim, one` |
|         - | 5996 | ` * more reference on a container, and a READ-ONLY view of the source's string bytes. It is` |
|         - | 5997 | ` * how a variable, an element and a property all reach the operand stack, and it is the` |
|         - | 5998 | ` * engine's second-most-called function -- 1.34 BILLION times on the ecosystem gate's phpcs` |
|         - | 5999 | ` * step (counted), from only 62 call sites.` |
|         - | 6000 | ` *` |
|         - | 6001 | ` * Inline for the same reason SySetAt, PH7_MemObjAt and PH7_MemObjRelease are: the body is` |
|         - | 6002 | ` * a dozen instructions and it lived in memobj.c while every hot caller lived elsewhere, so` |
|         - | 6003 | ` * with no LTO every one of those 1.34 billion was a real call. Sixty-two sites is a cheap` |
|         - | 6004 | ` * place to spend that.` |
|         - | 6005 | ` *` |
|         - | 6006 | ` * The destination's blob is released first because a Load OVERWRITES it. On the workload of` |
|         - | 6007 | ` * record that branch was taken **0 times in 1.34 billion** -- the destination is nearly` |
|         - | 6008 | ` * always a fresh operand slot -- so it stays a call rather than more inline bytes.` |
|         - | 6009 | ` */` |
| 167148637 | 6010 | `PHL_VC_DOOR sxi32 PH7_MemObjLoad(ph7_value *pSrc,ph7_value *pDest)` |
|         5 | 6011 | `{` |
|         - | 6012 | `	PHL_VC_NOTE(PHL_VC_LOAD,(pSrc->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) != 0);` |
| 167148642 | 6013 | `	PH7_MEMOBJ_COPY_SCALAR(pDest,pSrc);` |
|         - | 6014 | `	/* D1 commit 2: a MEMOBJ_AUX_DEFPATH carrier OWNS its heap descriptor via x.pOther, and` |
|         - | 6015 | `	 * PH7_MemObjRelease frees it exactly once. An aliasing Load copies iFlags+x.pOther` |
|         - | 6016 | `	 * verbatim, so a Load-duplicated carrier would let two slots free the same descriptor.` |
|         - | 6017 | `	 * Carriers are transient (produced by LOAD_IDX/MEMBER, consumed at OP_CALL) and are never` |
|         - | 6018 | `	 * Load-copied today; strip the flag defensively so the invariant can't be violated. */` |
| 167148642 | 6019 | `	pDest->iFlags &= ~MEMOBJ_AUX_DEFPATH;` |
| 167148642 | 6020 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|         - | 6021 | `		/* Increment reference count */` |
|   1316356 | 6022 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
| 166490136 | 6023 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|         - | 6024 | `		/* Increment reference count */` |
|    645042 | 6025 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
| 165509550 | 6026 | `	}else if( pSrc->iFlags & MEMOBJ_STREAMRES ){` |
|         - | 6027 | `		/* One more holder of the stream handle: php closes a stream when its` |
|         - | 6028 | `		 * last value goes, and this door is how a variable reaches the stack. */` |
|    195482 | 6029 | `		PH7_StreamValueRef(pSrc->x.pOther);` |
|     97055 | 6030 | `	}` |
| 167148642 | 6031 | `	if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|       259 | 6032 | `		SyBlobRelease(&pDest->sBlob);` |
|       127 | 6033 | `	}` |
| 167148642 | 6034 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  64158094 | 6035 | `		SyBlobReadOnly(&pDest->sBlob,SyBlobData(&pSrc->sBlob),SyBlobLength(&pSrc->sBlob));` |
|  32084969 | 6036 | `	}` |
| 167148642 | 6037 | `	return SXRET_OK;` |
|         5 | 6038 | `}` |
|         - | 6039 | `PH7_PRIVATE ph7_value * PH7_ValuePeek(ph7_value *pVal,ph7_value *pScratch);` |
|         - | 6040 | `PH7_PRIVATE sxi64 PH7_ValuePeekInt64(ph7_value *pVal);` |
|         - | 6041 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         - | 6042 | `PH7_PRIVATE ph7_real PH7_ValuePeekReal(ph7_value *pVal);` |
|         - | 6043 | `#endif` |
|         - | 6044 | `PH7_PRIVATE int PH7_ValuePeekBool(ph7_value *pVal);` |
|         - | 6045 | `PH7_PRIVATE sxi32 PH7_MemObjReleaseSlow(ph7_value *pObj);` |
|         - | 6046 | `/*` |
|         - | 6047 | ` * Drop whatever a value owns. THE most-called function in the engine: 2.72 billion` |
|         - | 6048 | ` * times on the ecosystem gate's phpcs step, out of ~4.5 billion calls into the four` |
|         - | 6049 | ` * value primitives together (counted).` |
|         - | 6050 | ` *` |
|         - | 6051 | ` * 43.1% of those calls -- 1.17 billion of them -- had NOTHING TO DO, and this test is` |
|         - | 6052 | ` * why they no longer make the call. A value already typed MEMOBJ_NULL owns no hashmap,` |
|         - | 6053 | `` * no instance, and no string (the slow path's own `(iFlags & MEMOBJ_NULL) == 0` guard is`` |
|         - | 6054 | ` * what skips SyBlobRelease, so a NULL value's blob is not released today either). What` |
|         - | 6055 | ` * IS still owned by a NULL-typed value is one of the three AUX carriers, each of which` |
|         - | 6056 | ` * holds a heap descriptor this is the universal free site for -- so they are the mask,` |
|         - | 6057 | `` * and they must stay in it: a `??=` peek, a __call carrier and a deferred-path lvalue`` |
|         - | 6058 | ` * are all NULL-typed by construction.` |
|         - | 6059 | ` *` |
|         - | 6060 | ` * Inline because the body it guards is three flag tests and a return for nearly half of` |
|         - | 6061 | ` * those 2.72 billion calls, and the call and return around them cost more than they do.` |
|         - | 6062 | ` * The same reason SySetAt and PH7_MemObjAt are inline.` |
|         - | 6063 | ` */` |
|         - | 6064 | `#define MEMOBJ_AUX_OWNED (MEMOBJ_AUX_COALSTROFF\|MEMOBJ_AUX_MAGICCALL\|MEMOBJ_AUX_DEFPATH)` |
|         - | 6065 | `/*` |
|         - | 6066 | ` * The two markers that describe a value's place in ONE array literal under construction --` |
|         - | 6067 | ` * "this key is ABSENT, auto-index it" and "this value is a SPREAD source" -- and that mean` |
|         - | 6068 | ` * nothing once the LOAD_MAP they were written for has read them.` |
|         - | 6069 | ` *` |
|         - | 6070 | ` * They are the only flags whose lifetime is shorter than their slot's, and MemObjSetType` |
|         - | 6071 | ` * keeps every AUX bit, so without this they SURVIVED the pop: a constant pushed onto a slot` |
|         - | 6072 | `` * that had last held an absent-key nil came out marked absent, and `[E_USER_ERROR => 'x',`` |
|         - | 6073 | `` * E_USER_WARNING => 'y']` built `[256 => 'x', 257 => 'y']` -- a wrong array, silently.`` |
|         - | 6074 | ` * Cleared at the release every pop routes through, which is why they belong in the fast` |
|         - | 6075 | ` * path's mask: a marked slot must not take its "owns nothing" return.` |
|         - | 6076 | ` *` |
|         - | 6077 | ` * A deferred plain-variable argument's marker is the third: it BORROWS the name, and dies` |
|         - | 6078 | `` * with the call it was loaded for. A call whose argument list threw after it -- `f($u, t())`,`` |
|         - | 6079 | ` * or a named argument refused at its send -- never reached the resolver that clears it, so` |
|         - | 6080 | `` * the slot kept it, and the next array pushed there (`new ArrayObject([])`) was read back`` |
|         - | 6081 | `` * as a variable NAME: a garbage `Undefined variable` and a NULL hashmap.`` |
|         - | 6082 | ` */` |
|         - | 6083 | `#define MEMOBJ_AUX_STACKMARK (MEMOBJ_AUX_NOKEY\|MEMOBJ_AUX_SPREAD\|MEMOBJ_AUX_DEFERRED)` |
| 313558421 | 6084 | `PHL_VC_DOOR sxi32 PH7_MemObjRelease(ph7_value *pObj)` |
|         5 | 6085 | `{` |
|         - | 6086 | `	PHL_VC_NOTE(PHL_VC_RELEASE,` |
|         - | 6087 | `		(pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_AUX_OWNED\|MEMOBJ_AUX_STACKMARK)) != MEMOBJ_NULL);` |
| 313558426 | 6088 | `	if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_AUX_OWNED\|MEMOBJ_AUX_STACKMARK)) == MEMOBJ_NULL ){` |
|  39252000 | 6089 | `		return SXRET_OK;   /* Owns nothing -- 43.1% of every release the engine makes */` |
|         - | 6090 | `	}` |
| 274306431 | 6091 | `	return PH7_MemObjReleaseSlow(pObj);` |
| 156784831 | 6092 | `}` |
|         - | 6093 | `PH7_PRIVATE sxi32 PH7_MemObjToNumeric(ph7_value *pObj);` |
|         - | 6094 | `PH7_PRIVATE sxi32 PH7_MemObjStringIncrement(ph7_value *pObj);` |
|         - | 6095 | `PH7_PRIVATE sxi32 PH7_MemObjTryInteger(ph7_value *pObj);` |
|         - | 6096 | `PH7_PRIVATE ProcMemObjCast PH7_MemObjCastMethod(sxi32 iFlags);` |
|         - | 6097 | `PH7_PRIVATE int PH7_MemObjStringIsNumeric(ph7_value *pValue);` |
|         - | 6098 | `PH7_PRIVATE int PH7_MemObjStringNumericPrefix(ph7_value *pValue,const char **pzTail);` |
|         - | 6099 | `PH7_PRIVATE sxi32 PH7_MemObjIsNumeric(ph7_value *pObj);` |
|         - | 6100 | `PH7_PRIVATE sxi32 PH7_MemObjIsEmpty(ph7_value *pObj);` |
|         - | 6101 | `PH7_PRIVATE sxi32 PH7_MemObjToHashmap(ph7_value *pObj);` |
|         - | 6102 | `PH7_PRIVATE sxi32 PH7_MemObjToObject(ph7_value *pObj);` |
|         - | 6103 | `PH7_PRIVATE sxi32 PH7_MemObjToString(ph7_value *pObj);` |
|         - | 6104 | `PH7_PRIVATE sxi32 PH7_MemObjToStringUV(ph7_value *pObj);` |
|         - | 6105 | `PH7_PRIVATE int PH7_MemObjIsNotStringable(ph7_value *pObj);` |
|         - | 6106 | `PH7_PRIVATE sxi32 PH7_MemObjToNull(ph7_value *pObj);` |
|         - | 6107 | `PH7_PRIVATE sxi32 PH7_MemObjToReal(ph7_value *pObj);` |
|         - | 6108 | `PH7_PRIVATE sxi32 PH7_MemObjToInteger(ph7_value *pObj);` |
|         - | 6109 | `PH7_PRIVATE sxi32 PH7_MemObjToBool(ph7_value *pObj);` |
|         - | 6110 | `PH7_PRIVATE int PH7_RealFitsInt64(double r);` |
|         - | 6111 | `PH7_PRIVATE sxi64 PH7_RealToInt64(double r);` |
|         - | 6112 | `PH7_PRIVATE void PH7_RealWarnIntCast(ph7_vm *pVm,double r);` |
|         - | 6113 | `PH7_PRIVATE void PH7_MemObjWarnIntCast(ph7_value *pObj);` |
|         - | 6114 | `PH7_PRIVATE sxi64 PH7_TokenValueToInt64(SyString *pData);` |
|         - | 6115 | `/* lex.c function prototypes */` |
|         - | 6116 | `PH7_PRIVATE sxi32 PH7_TokenizeRawText(const char *zInput,sxu32 nLen,SySet *pOut,sxu32 nBaseLine);` |
|         - | 6117 | `PH7_PRIVATE sxi32 PH7_TokenizePHP(const char *zInput,sxu32 nLen,sxu32 nLineStart,SySet *pOut,SySet *pTrivia);` |
|         - | 6118 | `/* vm.c function prototypes */` |
|         - | 6119 | `PH7_PRIVATE void PH7_VmReleaseContextValue(ph7_context *pCtx,ph7_value *pValue);` |
|         - | 6120 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(ph7_vm *pVm,ph7_vm_func *pFunc,const char *zName,sxu32 nByte,` |
|         - | 6121 | `	sxi32 iFlags,void *pUserData);` |
|         - | 6122 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(ph7_vm *pVm,ph7_vm_func *pFunc,SyString *pName);` |
|         - | 6123 | `PH7_PRIVATE SyHashEntry * PH7_VmGetUserFunction(ph7_vm *pVm,const void *pName,sxu32 nByte,int bEngineName);` |
|         - | 6124 | `PH7_PRIVATE SyHashEntry * PH7_VmGetHostFunction(ph7_vm *pVm,const void *pName,sxu32 nByte,int bEngineName);` |
|         - | 6125 | `PH7_PRIVATE void PH7_VmMarkLanguageConstructs(ph7_vm *pVm);` |
|         - | 6126 | `PH7_PRIVATE int PH7_VmNameIsInternalFunc(ph7_vm *pVm,const char *zName,sxu32 nByte);` |
|         - | 6127 | `PH7_PRIVATE SyHashEntry * PH7_VmCallSiteAnswer(ph7_vm *pVm,VmInstr *pInstr,const SyString *pName,int bEngineName,int *pbHost);` |
|         - | 6128 | `PH7_PRIVATE void PH7_VmCallSiteRecord(ph7_vm *pVm,VmInstr *pInstr,const SyString *pName,int bEngineName,int bHost,SyHashEntry *pEntry);` |
|         - | 6129 | `PH7_PRIVATE void PH7_VmCallSiteReleaseChunk(ph7_vm *pVm,SySet *pByteCode);` |
|         - | 6130 | `PH7_PRIVATE SyHashEntry * PH7_VmConstSiteAnswer(ph7_vm *pVm,VmInstr *pInstr);` |
|         - | 6131 | `PH7_PRIVATE void PH7_VmConstSiteRecord(ph7_vm *pVm,VmInstr *pInstr,SyHashEntry *pEntry);` |
|         - | 6132 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(ph7_vm *pVm,ph7_class_instance *pObj);` |
|         - | 6133 | `PH7_PRIVATE ph7_value * PH7_VmCreateDynamicAttr(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nName,VmClassAttr **ppAttr);` |
|         - | 6134 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(ph7_vm *pVm,sxu32 nIdx,SyHashEntry *pEntry,ph7_hashmap_node *pMapEntry);` |
|         - | 6135 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(ph7_vm *pVm,sxu32 nIdx,SyHashEntry *pEntry,ph7_hashmap_node *pMapEntry,sxi32 iFlags);` |
|         - | 6136 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew);` |
|         - | 6137 | `PH7_PRIVATE int PH7_VmIncludePathSep(void);` |
|         - | 6138 | `PH7_PRIVATE void PH7_VmSetIncludePath(ph7_vm *pVm,const char *zPath,sxu32 nByte);` |
|         - | 6139 | `PH7_PRIVATE void PH7_VmApplyEngineIni(ph7_vm *pVm);` |
|         - | 6140 | `PH7_PRIVATE void PH7_VmGetIncludePath(ph7_vm *pVm,SyBlob *pOut);` |
|         - | 6141 | `PH7_PRIVATE int PH7_VmExecutingDir(ph7_vm *pVm,SyString *pOut);` |
|         - | 6142 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable,sxi32 iNest);` |
|         - | 6143 | `PH7_PRIVATE void PH7_VmHideClasses(ph7_vm *pVm,sxu32 nMark);` |
|         - | 6144 | `PH7_PRIVATE sxi32 PH7_VmDeclareHiddenClass(ph7_vm *pVm,ph7_class *pClass);` |
|         - | 6145 | `PH7_PRIVATE SyHashEntry * PH7_VmClassEntry(ph7_vm *pVm,const char *zName,sxu32 nByte);` |
|         - | 6146 | `PH7_PRIVATE void PH7_VmClassNameAnchor(const char **pzName,sxu32 *pnByte);` |
|         - | 6147 | `PH7_PRIVATE sxi32 PH7_VmMaterializeClassConst(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr);` |
|         - | 6148 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable);` |
|         - | 6149 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(ph7_vm *pVm,const SyString *pName,ProcConstant xExpand,void *pUserData);` |
|         - | 6150 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstantEx(ph7_vm *pVm,const SyString *pName,ProcConstant xExpand,` |
|         - | 6151 | `	void *pUserData,const SyString *pFile,sxu32 nLine,int bUser);` |
|         - | 6152 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(ph7_vm *pVm,const SyString *pName,ProchHostFunction xFunc,void *pUserData);` |
|         - | 6153 | `/* Builds a ph7_user_func WITHOUT registering it as a global name. The native-class` |
|         - | 6154 | ` * builder uses it for method bodies, which are reachable only through their class. */` |
|         - | 6155 | `PH7_PRIVATE sxi32 PH7_NewForeignFunction(ph7_vm *pVm,const SyString *pName,ProchHostFunction xFunc,` |
|         - | 6156 | `	void *pUserData,ph7_user_func **ppOut);` |
|         - | 6157 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(ph7_vm *pVm,ph7_class *pClass);` |
|         - | 6158 | `PH7_PRIVATE void PH7_VmUnitDeclEnd(ph7_vm *pVm,sxu32 nMark,int bFailed);` |
|         - | 6159 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(const void *pSrc,unsigned int nLen,void *pUserData);` |
|         - | 6160 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm);` |
|         - | 6161 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex);` |
|         - | 6162 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(ph7_vm *pVm,SyString *pString);` |
|         - | 6163 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(ph7_vm *pVm,const char *zFormat,va_list ap);` |
|         - | 6164 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap);` |
|         - | 6165 | `PH7_PRIVATE sxi32 PH7_VmThrowError(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zMessage);` |
|         - | 6166 | `PH7_PRIVATE sxi32 PH7_VmMemoryError(ph7_vm *pVm);` |
|         - | 6167 | `PH7_PRIVATE sxi32 PH7_ContextMemoryError(ph7_context *pCtx);` |
|         - | 6168 | `PH7_PRIVATE sxu32 PH7_VmResourceId(ph7_vm *pVm,void *pRes);` |
|         - | 6169 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...);` |
|         - | 6170 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionCode(ph7_context *pCtx,const char *zClass,sxi32 iCode,const char *zFormat,...);` |
|         - | 6171 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionPrev(ph7_context *pCtx,ph7_class_instance *pPrev,const char *zClass,const char *zFormat,...);` |
|         - | 6172 | `PH7_PRIVATE sxi32 VmHostFuncThrowRc(ph7_context *pCtx,sxi32 rc);` |
|         - | 6173 | `PH7_PRIVATE sxi32 PH7_VmThrowArrayNextIndexError(ph7_vm *pVm);` |
|         - | 6174 | `PH7_PRIVATE sxi32 PH7_VmThrowGlobalsAppendError(ph7_vm *pVm);` |
|         - | 6175 | `PH7_PRIVATE sxi32 PH7_VmInstallGlobalVar(ph7_vm *pVm,const char *zName,sxu32 nByte,ph7_value *pValue,sxu32 nRefIdx);` |
|         - | 6176 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...);` |
|         - | 6177 | `PH7_PRIVATE sxi32 PH7_VmDump(ph7_vm *pVm,ProcConsumer xConsumer,void *pUserData);` |
|         - | 6178 | `PH7_PRIVATE sxi32 PH7_VmInstallDateTime(ph7_vm *pVm);` |
|         - | 6179 | `PH7_PRIVATE int PH7_VmErrorLogToFile(ph7_vm *pVm,const char *zMsg,sxu32 nMsg);` |
|         - | 6180 | ``/* `display_errors` destinations -- php's PHP_DISPLAY_ERRORS_* numbering, which is`` |
|         - | 6181 | `` * observable: the directive accepts the numbers as well as the words, so `2` is`` |
|         - | 6182 | `` * the error stream and `1` the program output. */`` |
|         - | 6183 | `#define PH7_DISPLAY_ERRORS_OFF    0` |
|         - | 6184 | `#define PH7_DISPLAY_ERRORS_STDOUT 1` |
|         - | 6185 | `#define PH7_DISPLAY_ERRORS_STDERR 2` |
|         - | 6186 | `PH7_PRIVATE int PH7_VmDisplayErrorsMode(const char *zVal,sxu32 nVal);` |
|         - | 6187 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 6188 | `/* Shared between builtin_date.c (procedural date functions) and` |
|         - | 6189 | ` * builtin_date_parse.c (the DateTime family) */` |
|         - | 6190 | `PH7_PRIVATE sxi32 DateFormat(ph7_context *pCtx,const char *zIn,int nLen,Sytm *pTm,int uSec);` |
|         - | 6191 | `PH7_PRIVATE void PH7_VmLogTimestamp(ph7_vm *pVm,SyBlob *pOut);` |
|         - | 6192 | `PH7_PRIVATE sxi64 DtDaysFromCivil(sxi64 y,int m,int d);` |
|         - | 6193 | `PH7_PRIVATE void DtCivilFromDays(sxi64 z,sxi64 *py,int *pm,int *pd);` |
|         - | 6194 | `PH7_PRIVATE sxi64 DtFloorDiv(sxi64 a,sxi64 b);` |
|         - | 6195 | `PH7_PRIVATE void DtFillSytm(sxi64 iTs,sxi32 iOff,char *zZone,Sytm *pTm);` |
|         - | 6196 | `PH7_PRIVATE void DtNowUs(ph7_vm *pVm,sxi64 *piSec,int *puSec);` |
|         - | 6197 | `/* The script default zone as the tz database sees it: an index or -1, and the` |
|         - | 6198 | ` * offset/abbreviation/is-DST that index is on at an instant. Shared with` |
|         - | 6199 | ` * builtin_date.c so the procedural doors ask the same question the DateTime` |
|         - | 6200 | ` * family does. Both answer "no database zone" harmlessly with the flag off. */` |
|         - | 6201 | `PH7_PRIVATE int DtDefaultTzIndex(ph7_vm *pVm);` |
|         - | 6202 | `PH7_PRIVATE sxi32 DtTzOffsetOf(int iTz,sxi32 iFixed,sxi64 iTs,int *pbDst,` |
|         - | 6203 | `	const char **pzAbbr,int *pnAbbr);` |
|         - | 6204 | `#ifdef PH7_ENABLE_TZDB` |
|         - | 6205 | `/* The embedded IANA database (builtin_date_tzdb.c). Absent from the tiny build` |
|         - | 6206 | ` * on purpose -- the payload is ~296 KB -- so every caller asks PH7_TzFind()` |
|         - | 6207 | ` * first and keeps its fixed-offset path for the answer -1. */` |
|         - | 6208 | `PH7_PRIVATE int PH7_TzFind(const char *zName,int nName);` |
|         - | 6209 | `PH7_PRIVATE int PH7_TzAbbrFind(const char *zName,int nName,sxi32 *piOff,int *pbDst,` |
|         - | 6210 | `	const char **pzCanon,int *pnCanon);` |
|         - | 6211 | `PH7_PRIVATE int PH7_TzAbbrCount(void);` |
|         - | 6212 | `PH7_PRIVATE const char * PH7_TzAbbrAt(int i,int *pnName,int *pnRow);` |
|         - | 6213 | `PH7_PRIVATE int PH7_TzAbbrRowAt(int i,int j,sxi32 *piOff,int *pbDst,int *piZone);` |
|         - | 6214 | `PH7_PRIVATE const char * PH7_TzAbbrZoneFind(const char *zName,int nName,sxi64 iOff,` |
|         - | 6215 | `	sxi64 iDst,int *pnZone);` |
|         - | 6216 | `PH7_PRIVATE int PH7_TzCount(void);` |
|         - | 6217 | `PH7_PRIVATE int PH7_TzAt(int i);` |
|         - | 6218 | `PH7_PRIVATE const char * PH7_TzName(int iZone,int *pnName,int *pbBackward);` |
|         - | 6219 | `PH7_PRIVATE int PH7_TzGroup(int iZone);` |
|         - | 6220 | `PH7_PRIVATE const char * PH7_TzCountry(int iZone);` |
|         - | 6221 | `PH7_PRIVATE void PH7_TzLocation(int iZone,double *prLat,double *prLong,` |
|         - | 6222 | `	const char **pzComment,int *pnComment);` |
|         - | 6223 | `PH7_PRIVATE const char * PH7_TzVersion(void);` |
|         - | 6224 | `PH7_PRIVATE int PH7_TzOffsetAt(int iZone,sxi64 iTs,sxi32 *piOff,int *pbDst,` |
|         - | 6225 | `	const char **pzAbbr,int *pnAbbr);` |
|         - | 6226 | `PH7_PRIVATE int PH7_TzTransCount(int iZone);` |
|         - | 6227 | `PH7_PRIVATE int PH7_TzTransAt(int iZone,int i,sxi64 *piTs,sxi32 *piOff,int *pbDst,` |
|         - | 6228 | `	const char **pzAbbr,int *pnAbbr);` |
|         - | 6229 | `PH7_PRIVATE int PH7_TzTransNextPosix(int iZone,sxi64 iTs,sxi64 *piTs,sxi32 *piOff,` |
|         - | 6230 | `	int *pbDst,const char **pzAbbr,int *pnAbbr);` |
|         - | 6231 | `PH7_PRIVATE int PH7_TzLocalToUtc(int iZone,sxi64 iLocal,sxi64 *piTs,sxi32 *piOff);` |
|         - | 6232 | `PH7_PRIVATE int PH7_TzLocalToUtcFirst(int iZone,sxi64 iLocal,sxi64 *piTs,sxi32 *piOff);` |
|         - | 6233 | `PH7_PRIVATE int PH7_TzLocalToUtcSeed(int iZone,sxi64 iLocal,sxi32 iOffNow,int bDstNow,` |
|         - | 6234 | `	sxi64 *piTs,sxi32 *piOff);` |
|         - | 6235 | `#endif /* PH7_ENABLE_TZDB */` |
|         - | 6236 | `#ifdef PH7_ENABLE_JIS` |
|         - | 6237 | `/* The Japanese legacy character sets (builtin_jis.c). Absent from the tiny` |
|         - | 6238 | ` * build on purpose -- the table is ~31 KB -- so a framing that needs it is not` |
|         - | 6239 | ` * a known encoding there at all, rather than a known one that answers wrong. */` |
|         - | 6240 | `PH7_PRIVATE sxu32 PH7_JisX0208ToUni(int iRow,int iCell);` |
|         - | 6241 | `PH7_PRIVATE int PH7_JisX0208FromUni(sxu32 cp,int *piRow,int *piCell);` |
|         - | 6242 | `PH7_PRIVATE sxu32 PH7_JisX0201RomanToUni(int c);` |
|         - | 6243 | `PH7_PRIVATE int PH7_JisX0201RomanFromUni(sxu32 cp,int *piByte);` |
|         - | 6244 | `PH7_PRIVATE sxu32 PH7_JisX0201KanaToUni(int c);` |
|         - | 6245 | `PH7_PRIVATE int PH7_JisX0201KanaFromUni(sxu32 cp,int *piByte);` |
|         - | 6246 | `#endif /* PH7_ENABLE_JIS */` |
|         - | 6247 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 6248 | `PH7_PRIVATE const char * PH7_VmBuiltinSigLookup(const char *zName,sxu32 nLen,const char **pzRet);` |
|         - | 6249 | `PH7_PRIVATE void PH7_VmStoreArgByRef(ph7_vm *pVm,ph7_value *pArg,ph7_value *pNewVal);` |
|         - | 6250 | `PH7_PRIVATE sxi32 PH7_ValueToStringUV(ph7_context *pCtx,ph7_value *pValue,const char **pzData,int *pnLen);` |
|         - | 6251 | `PH7_PRIVATE void PH7_ValueToStringUVOnce(ph7_context *pCtx,ph7_value *pValue,const char **pzData,int *pnLen);` |
|         - | 6252 | `PH7_PRIVATE ph7_class *PH7_ValueToStringUVDefer(ph7_context *pCtx,ph7_value *pValue,const char **pzData,int *pnLen);` |
|         - | 6253 | `PH7_PRIVATE void PH7_VmThrowWarningFmt(ph7_vm *pVm,const char *zFmt,...);` |
|         - | 6254 | `PH7_PRIVATE sxi32 PH7_CheckCallbackArg(ph7_context *pCtx,ph7_value *pCb,int iArg,const char *zParam,int bNullable);` |
|         - | 6255 | `PH7_PRIVATE sxi32 PH7_CheckCallbackReason(ph7_context *pCtx,ph7_value *pCb,int iArg,const char *zParam,int bNullable);` |
|         - | 6256 | `PH7_PRIVATE sxi32 PH7_VmInstallReflection(ph7_vm *pVm);` |
|         - | 6257 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionTypes(ph7_vm *pVm); /* vm_builtin_reflection.c */` |
|         - | 6258 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionSmall(ph7_vm *pVm); /* vm_builtin_reflection.c */` |
|         - | 6259 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionAttribute(ph7_vm *pVm); /* vm_builtin_reflection.c */` |
|         - | 6260 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionEnum(ph7_vm *pVm); /* vm_builtin_reflection.c */` |
|         - | 6261 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionClass(ph7_vm *pVm); /* vm_builtin_reflection.c */` |
|         - | 6262 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionFunc(ph7_vm *pVm);  /* vm_builtin_reflection.c */` |
|         - | 6263 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionHookType(ph7_vm *pVm); /* vm_builtin_reflection.c */` |
|         - | 6264 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionMember(ph7_vm *pVm); /* vm_builtin_reflection.c */` |
|         - | 6265 | `PH7_PRIVATE sxi32 PH7_ClosurePresent(ph7_vm *pVm,ph7_class_instance *pThis,` |
|         - | 6266 | `	ph7_value *pOut,int bDebug); /* vm_builtin_reflection.c: Closure's ph7_class::xPresent */` |
|         - | 6267 | `PH7_PRIVATE sxi32 PH7_VmInstallBuiltinLib(ph7_vm *pVm); /* vm_builtin_lib.c */` |
|         - | 6268 | `PH7_PRIVATE ph7_class_instance * PH7_VmNewClosure(ph7_vm *pVm,const SyString *pName,` |
|         - | 6269 | `	ph7_class_instance *pBoundThis,const SyString *pScope);` |
|         - | 6270 | `PH7_PRIVATE sxi32 PH7_VmEnforcePropStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue);` |
|         - | 6271 | `PH7_PRIVATE int PH7_VmSlotRefCount(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 6272 | `PH7_PRIVATE sxi32 PH7_VmInit(ph7_vm *pVm,ph7 *pEngine);` |
|         - | 6273 | `PH7_PRIVATE sxi32 PH7_VmConfigure(ph7_vm *pVm,sxi32 nOp,va_list ap);` |
|         - | 6274 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm);` |
|         - | 6275 | `PH7_PRIVATE int PH7_VmFramelessArity(const SyString *pName,int nArgs);` |
|         - | 6276 | `/* Fiber API helpers (used by api.c) */` |
|         - | 6277 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal);` |
|         - | 6278 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult);` |
|         - | 6279 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult);` |
|         - | 6280 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber);` |
|         - | 6281 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber);` |
|         - | 6282 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber);` |
|         - | 6283 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm);` |
|         - | 6284 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm);` |
|         - | 6285 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(ph7_vm *pVm);` |
|         - | 6286 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm);` |
|         - | 6287 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm);` |
|         - | 6288 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm);` |
|         - | 6289 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm);` |
|         - | 6290 | `PH7_PRIVATE VmInstr *PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex);` |
|         - | 6291 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm);` |
|         - | 6292 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer);` |
|         - | 6293 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(ph7_vm *pVm,sxi32 iOp,sxi32 iP1,sxu32 iP2,void *p3,sxu32 *pIndex);` |
|         - | 6294 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm);` |
|         - | 6295 | `/* The wall clock as php reads it: epoch seconds and the sub-second microseconds,` |
|         - | 6296 | ` * through whatever source this build/embedder has (see DateNow). uniqid() is the` |
|         - | 6297 | ` * second caller after the date surface itself. */` |
|         - | 6298 | `PH7_PRIVATE void PH7_VmClockNow(ph7_vm *pVm,ph7_int64 *pSec,ph7_int64 *pUsec);` |
|         - | 6299 | ``/* php's `php_combined_lcg()`: a double in [0,1) from the two L'Ecuyer streams on`` |
|         - | 6300 | ` * the VM. Seeds them on first use. */` |
|         - | 6301 | `PH7_PRIVATE double PH7_VmCombinedLcg(ph7_vm *pVm);` |
|         - | 6302 | `PH7_PRIVATE void PH7_VmMtSrand(ph7_vm *pVm,sxu32 nSeed,int bLegacyTwist);` |
|         - | 6303 | `PH7_PRIVATE sxu32 PH7_VmMtRand(ph7_vm *pVm);` |
|         - | 6304 | `PH7_PRIVATE sxi64 PH7_VmMtRandRange(ph7_vm *pVm,sxi64 iMin,sxi64 iMax);` |
|         - | 6305 | `PH7_PRIVATE void PH7_VmReleaseInstanceAttr(ph7_vm *pVm,VmClassAttr *pVmAttr);` |
|         - | 6306 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_method *pMethod,` |
|         - | 6307 | `	ph7_value *pResult,int nArg,ph7_value **apArg);` |
|         - | 6308 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethodMap(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_method *pMethod,` |
|         - | 6309 | `	ph7_value *pResult,int nArg,ph7_value **apArg,VmCallArgMap *pMap);` |
|         - | 6310 | `PH7_PRIVATE sxi32 PH7_VmCallIteratorMethod(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_method *pMethod,` |
|         - | 6311 | `	ph7_value *pResult);` |
|         - | 6312 | `PH7_PRIVATE sxi32 PH7_VmCallMethodSwallow(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_method *pMethod,` |
|         - | 6313 | `	ph7_value *pResult,int nArg,ph7_value **apArg,int *pbThrew);` |
|         - | 6314 | `PH7_PRIVATE sxi32 PH7_VmCallMethodUnchecked(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_method *pMethod,` |
|         - | 6315 | `	ph7_value *pResult,int nArg,ph7_value **apArg);` |
|         - | 6316 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(ph7_vm *pVm,ph7_value *pFunc,int nArg,ph7_value **apArg,ph7_value *pResult);` |
|         - | 6317 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionWithMap(ph7_vm *pVm,ph7_value *pFunc,int nArg,ph7_value **apArg,ph7_value *pResult,VmCallArgMap *pArgMap);` |
|         - | 6318 | `PH7_PRIVATE ph7_class_instance * PH7_VmCallerThisFor(ph7_vm *pVm,ph7_class *pClass);` |
|         - | 6319 | `PH7_PRIVATE ph7_class_instance * PH7_VmCallerThis(ph7_vm *pVm);` |
|         - | 6320 | `PH7_PRIVATE ph7_class_instance * PH7_VmFrameThis(ph7_vm *pVm,VmFrame *pFrame);` |
|         - | 6321 | `PH7_PRIVATE ph7_class * PH7_VmClosureFuncScope(ph7_vm *pVm,ph7_vm_func *pFunc,ph7_class *pBound,` |
|         - | 6322 | `	int bThis,ph7_class *pFrom);` |
|         - | 6323 | `PH7_PRIVATE ph7_class_instance * PH7_VmStaticFallbackThis(ph7_vm *pVm,ph7_class *pClass);` |
|         - | 6324 | `PH7_PRIVATE sxi32 PH7_VmDispatchMagicCall(ph7_vm *pVm,ph7_class *pClass,ph7_class *pLsb,` |
|         - | 6325 | `	ph7_class_instance *pThis,` |
|         - | 6326 | `	const char *zName,sxu32 nName,ph7_value *pResult,int nArg,ph7_value **apArg,VmCallArgMap *pArgMap);` |
|         - | 6327 | `/* Per-element callback for PH7_VmIteratorWalk: return SXRET_OK to continue,` |
|         - | 6328 | ` * SXERR_EOF to stop early (not an error), or PH7_EXCEPTION/PH7_ABORT to propagate. */` |
|         - | 6329 | `typedef sxi32 (*ProcIterStep)(ph7_vm *pVm,ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|         - | 6330 | `PH7_PRIVATE sxi32 PH7_VmIteratorWalk(ph7_vm *pVm,ph7_value *pObj,ProcIterStep xStep,void *pUserData);` |
|         - | 6331 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(ph7_vm *pVm,ph7_value *pFunc,ph7_value *pResult,...);` |
|         - | 6332 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce);` |
|         - | 6333 | `PH7_PRIVATE void PH7_VmWarnByRefArgsGivenValue(ph7_vm *pVm,ph7_value *pCallable,int nArg,ph7_hashmap_node **apNode,SyString *aNames);` |
|         - | 6334 | `PH7_PRIVATE sxi32 PH7_VmByRefArgsGivenValue(ph7_vm *pVm,ph7_class *pOwner,ph7_vm_func *pFunc,ph7_value *pCallable,int nArg,ph7_value **apArg,ph7_hashmap_node **apNode,SyString *aNames);` |
|         - | 6335 | `PH7_PRIVATE sxi32 PH7_VmCufDropByRefArgs(ph7_context *pCtx,ph7_value *pCallable,int nArg,ph7_value **apArg,` |
|         - | 6336 | `	SyString *aNames);` |
|         - | 6337 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen);` |
|         - | 6338 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm);` |
|         - | 6339 | `PH7_PRIVATE ph7_class * PH7_VmPeekSelfClass(ph7_vm *pVm);` |
|         - | 6340 | `PH7_PRIVATE ph7_class * PH7_VmTraitUsingClass(ph7_vm *pVm,ph7_class *pTrait,ph7_class *pFrom);` |
|         - | 6341 | `PH7_PRIVATE ph7_class * PH7_VmMemberOwnerClass(ph7_class *pDeclClass,ph7_class *pFrom);` |
|         - | 6342 | `PH7_PRIVATE int PH7_VmEvalConstExpr(ph7_vm *pVm,SySet *pByteCode,ph7_value *pOut);` |
|         - | 6343 | `PH7_PRIVATE void PH7_ClassRenderDecl(ph7_vm *pVm,ph7_class *pScope,ph7_vm_func *pFunc,SyBlob *pOut);` |
|         - | 6344 | `PH7_PRIVATE int PH7_ClassFoldDefault(ph7_vm *pVm,SySet *pByteCode,ph7_value *pOut);` |
|         - | 6345 | `PH7_PRIVATE sxi32 PH7_ClassCheckOverrideCompat(ph7_gen_state *pGen,ph7_class *pBase,ph7_class *pSub,` |
|         - | 6346 | `	ph7_class_method *pParent,ph7_class_method *pChild,int bCtorExempt);` |
|         - | 6347 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm);` |
|         - | 6348 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke);` |
|         - | 6349 | `PH7_PRIVATE int PH7_VmArrayCallableParts(ph7_vm *pVm,ph7_hashmap *pMap,ph7_value **ppTarget,ph7_value **ppMethod);` |
|         - | 6350 | `PH7_PRIVATE int PH7_VmCallableMethodAccessible(ph7_vm *pVm,ph7_class *pClass,ph7_class_method *pMethod);` |
|         - | 6351 | `PH7_PRIVATE int PH7_VmCallableStringParts(const char *zName,sxu32 nName,` |
|         - | 6352 | `	const char **pzCls,sxu32 *pnCls,const char **pzMeth,sxu32 *pnMeth);` |
|         - | 6353 | `PH7_PRIVATE int PH7_VmQualifiedCallableMethod(ph7_vm *pVm,ph7_class *pOrg,const char *zName,sxu32 nName,` |
|         - | 6354 | `	ph7_class **ppClass,const char **pzCls,sxu32 *pnCls,const char **pzMeth,sxu32 *pnMeth,` |
|         - | 6355 | `	char *zBuf,int nBuf,const char **pzWhy);` |
|         - | 6356 | `PH7_PRIVATE int PH7_VmClassLookupRaised(ph7_vm *pVm,sxi32 nBrcBefore,const void *pResumeBefore);` |
|         - | 6357 | `PH7_PRIVATE const char * PH7_VmCallableReason(ph7_vm *pVm,ph7_value *pValue,char *zBuf,int nBuf);` |
|         - | 6358 | `PH7_PRIVATE sxi32 PH7_VmCallableDeprecation(ph7_vm *pVm,ph7_value *pValue);` |
|         - | 6359 | `PH7_PRIVATE sxi32 PH7_VmCallableDeprecationFenced(ph7_vm *pVm,ph7_value *pValue,ph7_class_instance **ppExc);` |
|         - | 6360 | `PH7_PRIVATE void PH7_VmCallableName(ph7_vm *pVm,ph7_value *pValue,SyBlob *pOut);` |
|         - | 6361 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(ph7_vm *pVm,const char *zName,sxu32 nByte);` |
|         - | 6362 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(ph7_hashmap *pMap,const char *zKey,int nKeylen,const char *zData,int nLen);` |
|         - | 6363 | `/* The file:// strip is pure string work and the VFS layer needs it in every` |
|         - | 6364 | ` * build, disk IO enabled or not. */` |
|         - | 6365 | `PH7_PRIVATE const char * PH7_VmFileUrlLocalPath(const char *zPath);` |
|         - | 6366 | `PH7_PRIVATE int PH7_VmUrlSchemeLen(const char *zIn,int nByte);` |
|         - | 6367 | `#ifndef PH7_DISABLE_DISK_IO` |
|         - | 6368 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(ph7_vm *pVm,const char **pzDevice,int nByte);` |
|         - | 6369 | `PH7_PRIVATE int PH7_VmStreamDeviceSuppressed(ph7_vm *pVm,const ph7_io_stream *pStream);` |
|         - | 6370 | `PH7_PRIVATE ph7_io_stream * PH7_VmFindStreamDevice(ph7_vm *pVm,const char *zName,int nName);` |
|         - | 6371 | `PH7_PRIVATE int PH7_VmStreamSchemeDisabled(ph7_vm *pVm,const char *zName,int nName);` |
|         - | 6372 | `PH7_PRIVATE int PH7_VmStreamDeviceIsRemoteHost(const char *zUri,int nByte,int *pnScheme);` |
|         - | 6373 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|         - | 6374 | `/* vm_http.c function prototypes */` |
|         - | 6375 | `PH7_PRIVATE sxi32 PH7_VmHttpSplitURI(SyhttpUri *pOut,const char *zUri,sxu32 nLen);` |
|         - | 6376 | `PH7_PRIVATE sxi32 PH7_VmHttpProcessRequest(ph7_vm *pVm,const char *zRequest,int nByte);` |
|         - | 6377 | `/* vm_http_response.c function prototypes */` |
|         - | 6378 | `PH7_PRIVATE void PH7_RegisterHttpResponseFunctions(ph7_vm *pVm);` |
|         - | 6379 | `PH7_PRIVATE void PH7_VmReleaseResponseHeaders(ph7_vm *pVm);` |
|         - | 6380 | `PH7_PRIVATE int PH7_VmHttpDate(sxi64 iWhen,char *zBuf,int nBuf);` |
|         - | 6381 | `PH7_PRIVATE void PH7_VmSetResponseHeader(ph7_vm *pVm,const char *zName,const char *zValue,sxu32 nValue);` |
|         - | 6382 | `PH7_PRIVATE void PH7_VmRemoveCookieByName(ph7_vm *pVm,const char *zName,sxu32 nName);` |
|         - | 6383 | `PH7_PRIVATE void PH7_VmEmitCookie(ph7_vm *pVm,const char *zName,sxu32 nName,` |
|         - | 6384 | `	const char *zValue,sxu32 nValue,int bEncode,sxi64 iExpires,` |
|         - | 6385 | `	const char *zPath,sxu32 nPath,const char *zDomain,sxu32 nDomain,` |
|         - | 6386 | `	int bSecure,int bHttpOnly,const char *zSame,sxu32 nSame,int bPartitioned);` |
|         - | 6387 | `/* vm_pcre.c function prototypes */` |
|         - | 6388 | `#ifdef PH7_ENABLE_PCRE` |
|         - | 6389 | `PH7_PRIVATE void PH7_RegisterPcreFunctions(ph7_vm *pVm);` |
|         - | 6390 | `PH7_PRIVATE int PH7_MbEncodingLookup(const char *z,int n);` |
|         - | 6391 | `PH7_PRIVATE const char * PH7_MbEncodingCanonical(int iNameId);` |
|         - | 6392 | `PH7_PRIVATE int PH7_MbEncodingIsUtf8(int iNameId);` |
|         - | 6393 | `PH7_PRIVATE void PH7_RegisterPcreConstants(ph7_vm *pVm);` |
|         - | 6394 | `PH7_PRIVATE sxi32 PH7_PcreMatchQuiet(ph7_context *pCtx,const char *zPat,int nPat,` |
|         - | 6395 | `	const char *zSub,int nSub,int *pMatched);` |
|         - | 6396 | `PH7_PRIVATE int PH7_PcrePatternCheck(ph7_vm *pVm,const char *zPattern,int nLen,` |
|         - | 6397 | `	char *zErr,sxu32 nErr);` |
|         - | 6398 | `/* RegexIterator's five operation modes, php's REGIT_MODE_* values — they are the` |
|         - | 6399 | ` * class constants, so the numbers are php-visible and fixed. */` |
|         - | 6400 | `#define PH7_REGIT_MATCH        0` |
|         - | 6401 | `#define PH7_REGIT_GET_MATCH    1` |
|         - | 6402 | `#define PH7_REGIT_ALL_MATCHES  2` |
|         - | 6403 | `#define PH7_REGIT_SPLIT        3` |
|         - | 6404 | `#define PH7_REGIT_REPLACE      4` |
|         - | 6405 | `PH7_PRIVATE sxi32 PH7_PcreRegitApply(ph7_context *pCtx,int iMode,ph7_value *pPattern,` |
|         - | 6406 | `	ph7_value *pSubject,int iPregFlags,ph7_value *pRepl,ph7_value *pOut,int *pbOk);` |
|         - | 6407 | `#endif /* PH7_ENABLE_PCRE */` |
|         - | 6408 | `/* One resource pointer's php-visible id. Allocated per distinct resource and` |
|         - | 6409 | `` * owned by ph7_vm.hResourceId, whose key is the `pRes` field itself (SyHash`` |
|         - | 6410 | ` * stores the key POINTER, so it must outlive the entry). Core (used by` |
|         - | 6411 | ` * PH7_VmResourceId) — must NOT sit under PH7_ENABLE_LIBXML or the tiny build,` |
|         - | 6412 | ` * which omits libxml, fails to compile it. */` |
|         - | 6413 | `typedef struct phl_res_id phl_res_id;` |
|         - | 6414 | `struct phl_res_id {` |
|         - | 6415 | `	void *pRes;   /* The resource pointer, and the hash key */` |
|         - | 6416 | `	sxu32 nId;    /* php-visible id */` |
|         - | 6417 | `};` |
|         - | 6418 | `#ifdef PH7_ENABLE_LIBXML` |
|         - | 6419 | `/* One entry in the per-VM libxml error queue (mirrors php's LibXMLError:` |
|         - | 6420 | ` * level/code/column/message/file/line).  Strings are SyMemBackend copies` |
|         - | 6421 | ` * owned by the queue and released by PH7_LibxmlClearErrors(). */` |
|         - | 6422 | `typedef struct phl_libxml_err phl_libxml_err;` |
|         - | 6423 | `struct phl_libxml_err {` |
|         - | 6424 | `	int iLevel;      /* LIBXML_ERR_WARNING/ERROR/FATAL */` |
|         - | 6425 | `	int iCode;       /* raw libxml2 error code */` |
|         - | 6426 | `	int iLine;` |
|         - | 6427 | `	int iColumn;` |
|         - | 6428 | `	SyString sMsg;   /* message text, trailing newline preserved (php parity) */` |
|         - | 6429 | `	SyString sFile;  /* source file/URI, empty for in-memory strings */` |
|         - | 6430 | `};` |
|         - | 6431 | `/* Per-VM owner of one libxml document tree.  See the lifetime notes at the` |
|         - | 6432 | ` * top of vm_libxml.c: docs are only freed at VM reset/release, never while` |
|         - | 6433 | ` * PHP code could still hold a wrapper into them. */` |
|         - | 6434 | `typedef struct phl_xmldoc phl_xmldoc;` |
|         - | 6435 | `struct phl_xmldoc {` |
|         - | 6436 | `	void *pDoc;         /* xmlDocPtr (void* keeps libxml headers out of ph7int.h) */` |
|         - | 6437 | `	SySet aOrphans;     /* xmlNodePtr's unlinked from the tree but still owned */` |
|         - | 6438 | `	/* The stand-in NODES a DTD's NOTATION declarations are answered through:` |
|         - | 6439 | `	 * an xmlNotation is {name, PublicID, SystemID} and nothing else -- it has` |
|         - | 6440 | `	 * no type field, so it cannot be walked as a node -- and php builds an` |
|         - | 6441 | `	 * entity-shaped node per lookup. One per declaration is built here and` |
|         - | 6442 | `	 * kept, so the wrapper identity every other node has holds for these too;` |
|         - | 6443 | `	 * they need their own free (vm_libxml.c), since xmlFreeNode would read` |
|         - | 6444 | `	 * an xmlEntity's length/etype pair as a node's property list. */` |
|         - | 6445 | `	SySet aNotations;   /* synthesized XML_NOTATION_NODE xmlNodePtr's */` |
|         - | 6446 | `	/* The stand-in ATTRIBUTES php 8.4's tree answers a namespace DECLARATION` |
|         - | 6447 | ``	 * through: that tree lists `xmlns:p` in the attribute map as an attribute`` |
|         - | 6448 | `	 * of its own, and libxml keeps a declaration in an xmlNs, which is not a` |
|         - | 6449 | `	 * node.  One stand-in is built per declaration and kept here so the` |
|         - | 6450 | `	 * wrapper identity every other node has holds for these too; each owns` |
|         - | 6451 | `	 * the xmlns-namespace handle it points at, and neither is linked into the` |
|         - | 6452 | `	 * element, so both need their own free (vm_libxml.c). */` |
|         - | 6453 | `	SySet aNsAttrs;     /* synthesized XML_ATTRIBUTE_NODE xmlAttrPtr's */` |
|         - | 6454 | ``	/* A `<template>` element's CONTENT, which is not its children. php's HTML`` |
|         - | 6455 | `	 * parser puts what it parsed between the tags into a DOCUMENT FRAGMENT of` |
|         - | 6456 | `	 * the template's own and leaves the element childless, and both halves of` |
|         - | 6457 | ``	 * that are observable: the content is invisible to `childNodes`,`` |
|         - | 6458 | ``	 * `firstChild`, `hasChildNodes()` and `textContent` and VISIBLE to the two`` |
|         - | 6459 | ``	 * writers and to `innerHTML`, while a child a program APPENDS is the mirror`` |
|         - | 6460 | `	 * image. No rule that hides or shows ONE list answers both, so the storage` |
|         - | 6461 | `	 * is the answer: the fragment lives here, keyed by the element it belongs` |
|         - | 6462 | `	 * to, never linked into the tree. Each is freed with the document` |
|         - | 6463 | `	 * (vm_libxml.c); nesting is flat, a template inside a fragment holding its` |
|         - | 6464 | `	 * own entry here. */` |
|         - | 6465 | `	SySet aTemplates;   /* phl_domtpl pairs: element -> its content fragment */` |
|         - | 6466 | `	ph7_vm *pVm;        /* Owning VM (error routing from libxml callbacks) */` |
|         - | 6467 | `	void *pDocObj;      /* The DOMDocument wrapper for this tree, BORROWED, or 0.` |
|         - | 6468 | `	                     * ext/dom keys its per-node wrapper cache on the document` |
|         - | 6469 | ``	                     * OBJECT, so `dom_import_simplexml()` needs the one this`` |
|         - | 6470 | `	                     * tree already has -- that is what makes two imports of the` |
|         - | 6471 | `	                     * same node the same DOMElement, and what makes an import` |
|         - | 6472 | `	                     * back out of a SimpleXML that came FROM a DOMDocument` |
|         - | 6473 | `	                     * answer that document's own nodes. Cleared by` |
|         - | 6474 | `	                     * DOMDocument's xRelease when the object goes, so the` |
|         - | 6475 | `	                     * pointer is never stale. */` |
|         - | 6476 | `	/* Which of the two DOM class trees this document has been imported into by` |
|         - | 6477 | `	 * ext/simplexml, and 0 while neither door has run. php latches it on the` |
|         - | 6478 | ``	 * TREE, not on a node: whichever of `dom_import_simplexml()` and`` |
|         - | 6479 | ``	 * `Dom\\import_simplexml()` runs first decides, and the other then refuses`` |
|         - | 6480 | ``	 * the whole document with `must not be already imported as a ...`. A modern`` |
|         - | 6481 | `	 * document object standing on the tree latches it the same way (its own` |
|         - | 6482 | `	 * producer is the only door into that family), which is why a plain` |
|         - | 6483 | ``	 * `new DOMDocument` does NOT -- the 2004 tree is not a choice, it is what`` |
|         - | 6484 | `	 * you get when nobody chose. */` |
|         - | 6485 | `	int iSxFamily;      /* 0 = neither, 1 = the 2004 tree, 2 = php 8.4's */` |
|         - | 6486 | `	int bPreserveWS;    /* DOMDocument->preserveWhiteSpace */` |
|         - | 6487 | `	int bFormatOutput;  /* DOMDocument->formatOutput */` |
|         - | 6488 | `	phl_xmldoc *pNext;  /* Registry chain (pVm->pXmlDocs) */` |
|         - | 6489 | `};` |
|         - | 6490 | ``/* One `<template>` element and the fragment holding what was parsed inside it.`` |
|         - | 6491 | ` * Both are xmlNodePtr; the fragment is never linked into the tree. */` |
|         - | 6492 | `typedef struct phl_domtpl phl_domtpl;` |
|         - | 6493 | `struct phl_domtpl {` |
|         - | 6494 | ``	void *pTpl;         /* the XML_ELEMENT_NODE named `template` */`` |
|         - | 6495 | `	void *pFrag;        /* its XML_DOCUMENT_FRAG_NODE, owned */` |
|         - | 6496 | `};` |
|         - | 6497 | `/* One PHP-visible DOM node handle: the MEMOBJ_RES payload behind every DOM` |
|         - | 6498 | ` * wrapper object.  pNode points into pShell's tree (or IS the xmlDoc); the` |
|         - | 6499 | ` * shell outlives every handle (docs are only freed at VM reset/release). */` |
|         - | 6500 | `typedef struct phl_domnode phl_domnode;` |
|         - | 6501 | `struct phl_domnode {` |
|         - | 6502 | `	phl_xmldoc *pShell; /* Owning document registry entry */` |
|         - | 6503 | `	void *pNode;        /* xmlNodePtr / xmlDocPtr / xmlAttrPtr */` |
|         - | 6504 | `};` |
|         - | 6505 | `/* vm_libxml.c */` |
|         - | 6506 | `PH7_PRIVATE void PH7_RegisterLibxmlConstants(ph7_vm *pVm);` |
|         - | 6507 | `PH7_PRIVATE sxi32 PH7_VmInstallLibxml(ph7_vm *pVm);` |
|         - | 6508 | `PH7_PRIVATE void PH7_LibxmlVmReset(ph7_vm *pVm);` |
|         - | 6509 | `PH7_PRIVATE void PH7_LibxmlVmRelease(ph7_vm *pVm);` |
|         - | 6510 | `PH7_PRIVATE void PH7_LibxmlClearErrors(ph7_vm *pVm);` |
|         - | 6511 | `PH7_PRIVATE phl_xmldoc * PH7_LibxmlNewDoc(ph7_vm *pVm,void *pXmlDocPtr);` |
|         - | 6512 | `PH7_PRIVATE sxu32 PH7_LibxmlCaptureBegin(ph7_vm *pVm);` |
|         - | 6513 | `PH7_PRIVATE sxu32 PH7_LibxmlCaptureBeginRaw(ph7_vm *pVm);` |
|         - | 6514 | `PH7_PRIVATE void PH7_LibxmlCaptureEnd(ph7_vm *pVm,sxu32 nMark,const char *zFnName);` |
|         - | 6515 | `PH7_PRIVATE void PH7_LibxmlDropErrors(ph7_vm *pVm,sxu32 nMark);` |
|         - | 6516 | `/* Push one error onto the per-VM queue + last-error slot (strings copied).` |
|         - | 6517 | ` * The shared structured-error callback and the DOM schema error hooks both` |
|         - | 6518 | ` * funnel through this so ph7int.h needs no libxml types. */` |
|         - | 6519 | `PH7_PRIVATE void PH7_LibxmlCaptureEndOpts(ph7_vm *pVm,sxu32 nMark,const char *zFnName,int iOpts);` |
|         - | 6520 | `PH7_PRIVATE void PH7_LibxmlRaiseGeneric(ph7_vm *pVm,const char *zFnName,const char *zMsg);` |
|         - | 6521 | `PH7_PRIVATE void PH7_LibxmlQueueError(ph7_vm *pVm,int iLevel,int iCode,int iLine,int iColumn,` |
|         - | 6522 | `	const char *zMsg,const char *zFile);` |
|         - | 6523 | `/* vm_dom.c */` |
|         - | 6524 | `PH7_PRIVATE sxi32 PH7_VmInstallDom(ph7_vm *pVm);` |
|         - | 6525 | `/* Read a document's bytes for a loader, through php's own stream layer, with` |
|         - | 6526 | `` * libxml's `failed to load external entity` warning already raised for a file`` |
|         - | 6527 | ` * that is not there. ext/simplexml's two file doors want exactly what` |
|         - | 6528 | ` * DOMDocument::load() wants. */` |
|         - | 6529 | `PH7_PRIVATE int PH7_DomReadFile(ph7_context *pCtx,const char *zFile,int nFile,const char *zFn,` |
|         - | 6530 | `	SyBlob *pBody,SyBlob *pPath);` |
|         - | 6531 | `/* The document-cached wrapper for one node of pShell's tree, creating the` |
|         - | 6532 | ` * document object if this tree has none yet. ext/simplexml's two import doors` |
|         - | 6533 | ` * are the only callers: every other wrap already has a document object in hand.` |
|         - | 6534 | ` *` |
|         - | 6535 | ` * bModern is the DOOR's family, and it decides only what is MINTED -- the class` |
|         - | 6536 | ` * of a node this tree has not wrapped before, and the class of the document` |
|         - | 6537 | ` * object when there is none. A node the cache already holds comes back as it` |
|         - | 6538 | `` * was, so `Dom\\import_simplexml()` over a DOMDocument's tree answers a`` |
|         - | 6539 | `` * `Dom\\Element` for a fresh node and that document's own DOMElement for one it`` |
|         - | 6540 | ` * has already handed out. */` |
|         - | 6541 | `PH7_PRIVATE ph7_class_instance * PH7_DomWrapForeign(ph7_vm *pVm,phl_xmldoc *pShell,void *pNode,` |
|         - | 6542 | `	int bModern);` |
|         - | 6543 | `/* vm_simplexml.c */` |
|         - | 6544 | `PH7_PRIVATE sxi32 PH7_VmInstallSimpleXml(ph7_vm *pVm);` |
|         - | 6545 | `/* vm_xmlwriter.c */` |
|         - | 6546 | `PH7_PRIVATE sxi32 PH7_VmInstallXmlWriter(ph7_vm *pVm);` |
|         - | 6547 | `PH7_PRIVATE void PH7_XmlWriterVmSweep(ph7_vm *pVm);` |
|         - | 6548 | `/* vm_xml.c (php's ext/xml: the expat-style push-parser surface over libxml2) */` |
|         - | 6549 | `PH7_PRIVATE sxi32 PH7_VmInstallXml(ph7_vm *pVm);` |
|         - | 6550 | `PH7_PRIVATE void PH7_XmlParserVmSweep(ph7_vm *pVm);` |
|         - | 6551 | `#endif /* PH7_ENABLE_LIBXML */` |
|         - | 6552 | `#ifdef PH7_ENABLE_SQLITE` |
|         - | 6553 | `/* vm_pdo.c (ext/pdo: the driver-independent class library) */` |
|         - | 6554 | `PH7_PRIVATE sxi32 PH7_VmInstallPdo(ph7_vm *pVm);` |
|         - | 6555 | `PH7_PRIVATE void PH7_PdoVmReset(ph7_vm *pVm);` |
|         - | 6556 | `PH7_PRIVATE void PH7_PdoVmRelease(ph7_vm *pVm);` |
|         - | 6557 | `/* vm_pdo_sqlite.c (ext/pdo_sqlite: the driver and its Pdo\Sqlite subclass) */` |
|         - | 6558 | `PH7_PRIVATE sxi32 PH7_VmInstallPdoSqlite(ph7_vm *pVm);` |
|         - | 6559 | `/* vm_sqlite3.c (ext/sqlite3: php's other sqlite surface, the SQLite3 class family) */` |
|         - | 6560 | `PH7_PRIVATE sxi32 PH7_VmInstallSqlite3(ph7_vm *pVm);` |
|         - | 6561 | `PH7_PRIVATE void PH7_RegisterSqlite3Constants(ph7_vm *pVm);` |
|         - | 6562 | `PH7_PRIVATE void PH7_Sqlite3VmReset(ph7_vm *pVm);` |
|         - | 6563 | `PH7_PRIVATE void PH7_Sqlite3VmRelease(ph7_vm *pVm);` |
|         - | 6564 | `#endif /* PH7_ENABLE_SQLITE */` |
|         - | 6565 | `#ifdef PH7_ENABLE_CURL` |
|         - | 6566 | `/* vm_curl.c (ext/curl: php's libcurl binding) */` |
|         - | 6567 | `PH7_PRIVATE sxi32 PH7_VmInstallCurl(ph7_vm *pVm);` |
|         - | 6568 | `PH7_PRIVATE void PH7_RegisterCurlConstants(ph7_vm *pVm);` |
|         - | 6569 | `PH7_PRIVATE void PH7_CurlVmReset(ph7_vm *pVm);` |
|         - | 6570 | `PH7_PRIVATE void PH7_CurlVmRelease(ph7_vm *pVm);` |
|         - | 6571 | `#endif /* PH7_ENABLE_CURL */` |
|         - | 6572 | `/* net.c types and function prototypes */` |
|         - | 6573 | `#ifdef PH7_ENABLE_NET` |
|         - | 6574 | `#ifdef __WINNT__` |
|         - | 6575 | `#include <winsock2.h>` |
|         - | 6576 | `typedef SOCKET ph7_socket;` |
|         - | 6577 | `typedef int ph7_socklen;` |
|         - | 6578 | `#define PH7_NET_INVALID_SOCKET INVALID_SOCKET` |
|         - | 6579 | `#else` |
|         - | 6580 | `typedef int ph7_socket;` |
|         - | 6581 | `typedef unsigned int ph7_socklen;` |
|         - | 6582 | `#define PH7_NET_INVALID_SOCKET (-1)` |
|         - | 6583 | `#endif` |
|         - | 6584 | `struct sockaddr; /* Forward declaration */` |
|         - | 6585 | `/* The one failure whose MESSAGE only the caller can word: php answers` |
|         - | 6586 | `` * `php_network_getaddresses: getaddrinfo for <host> failed: ...` with the host`` |
|         - | 6587 | ` * in it, reports no OS code beside it, and raises it TWICE — once from the` |
|         - | 6588 | ` * transport and once from the opener that asked. */` |
|         - | 6589 | `#define PH7_NET_ERR_RESOLVE (-3)` |
|         - | 6590 | `PH7_PRIVATE int PH7_NetInit(void);` |
|         - | 6591 | `PH7_PRIVATE int PH7_NetEnsureInit(void);` |
|         - | 6592 | `PH7_PRIVATE void PH7_NetCleanup(void);` |
|         - | 6593 | `PH7_PRIVATE ph7_socket PH7_NetListen(const char *zHost,int iPort,int iBacklog);` |
|         - | 6594 | `/*` |
|         - | 6595 | `` * The `socket` context options net.c can apply, php's own option names. A NULL`` |
|         - | 6596 | ` * pointer means "none of them", which is what every internal opener passes.` |
|         - | 6597 | ` * so_broadcast and ipv6_v6only describe a datagram socket and an address family` |
|         - | 6598 | ` * this build has not got (recorded), so they are stored on the context` |
|         - | 6599 | ` * and never reach a socket.` |
|         - | 6600 | ` */` |
|         - | 6601 | `typedef struct ph7_sockopts ph7_sockopts;` |
|         - | 6602 | `struct ph7_sockopts` |
|         - | 6603 | `{` |
|         - | 6604 | ``	const char *zBindHost; /* `bindto`'s host half, already parsed (0 = no bind) */`` |
|         - | 6605 | ``	int iBindPort;         /* `bindto`'s port half */`` |
|         - | 6606 | ``	int bReusePort;        /* `so_reuseport` */`` |
|         - | 6607 | ``	int bNoDelay;          /* `tcp_nodelay` */`` |
|         - | 6608 | ``	int bBroadcast;        /* `so_broadcast`: what a DATAGRAM socket needs before`` |
|         - | 6609 | `	                        * it may address 255.255.255.255 at all */` |
|         - | 6610 | ``	int bV6Only;           /* `ipv6_v6only`, applied to an AF_INET6 listener */`` |
|         - | 6611 | ``	int iBacklog;          /* `backlog`; <= 0 keeps the transport's default */`` |
|         - | 6612 | `	/* OUT: how the local bind failed on the socket that was USED, which php` |
|         - | 6613 | `	 * warns about in two different wordings and never treats as fatal. */` |
|         - | 6614 | `	int iBindErr;          /* PH7_SOCKOPT_BIND_* (0 = it worked, or none asked) */` |
|         - | 6615 | `	int iBindErrno;        /* the OS code behind PH7_SOCKOPT_BIND_REFUSED */` |
|         - | 6616 | `};` |
|         - | 6617 | `#define PH7_SOCKOPT_BIND_RESOLVE 1 /* not a numeric address (php never resolves one) */` |
|         - | 6618 | `#define PH7_SOCKOPT_BIND_REFUSED 2 /* bind() itself said no */` |
|         - | 6619 | `PH7_PRIVATE ph7_socket PH7_NetBind(const char *zHost,int iPort,int bDgram,int bListen,` |
|         - | 6620 | `	int iBacklog,const ph7_sockopts *pOpt,int *pErrno,const char **pzErr);` |
|         - | 6621 | `PH7_PRIVATE ph7_socket PH7_NetConnect(const char *zHost,int iPort,int iTimeoutMs,` |
|         - | 6622 | `	int bDgram,int bAsync,ph7_sockopts *pOpt,int *pErrno,const char **pzErr);` |
|         - | 6623 | `PH7_PRIVATE ph7_socket PH7_NetAccept(ph7_socket listenSock,struct sockaddr *pAddr,ph7_socklen *pAddrLen);` |
|         - | 6624 | `PH7_PRIVATE ph7_socket PH7_NetAcceptTimed(ph7_socket listenSock,int iTimeoutMs,int *pbTimedOut,` |
|         - | 6625 | `	char *zPeer,int nPeer);` |
|         - | 6626 | `PH7_PRIVATE int PH7_NetSockName(ph7_socket sock,int bPeer,char *zBuf,int nBuf);` |
|         - | 6627 | `PH7_PRIVATE int PH7_NetHostName(char *zBuf,int nBuf,int *pErrno);` |
|         - | 6628 | `PH7_PRIVATE int PH7_NetWait(ph7_socket sock,int bWrite,int iTimeoutMs);` |
|         - | 6629 | `/* The platform-numbered socket constants, asked for by id because their VALUES` |
|         - | 6630 | ` * differ per OS (AF_INET6 is 10, 23 and 30 on three of them). */` |
|         - | 6631 | `#define PH7_NETC_PF_INET        1` |
|         - | 6632 | `#define PH7_NETC_PF_INET6       2` |
|         - | 6633 | `#define PH7_NETC_PF_UNIX        3` |
|         - | 6634 | `#define PH7_NETC_SOCK_STREAM    4` |
|         - | 6635 | `#define PH7_NETC_SOCK_DGRAM     5` |
|         - | 6636 | `#define PH7_NETC_SOCK_RAW       6` |
|         - | 6637 | `#define PH7_NETC_SOCK_SEQPACKET 7` |
|         - | 6638 | `#define PH7_NETC_SOCK_RDM       8` |
|         - | 6639 | `#define PH7_NETC_IPPROTO_IP     9` |
|         - | 6640 | `#define PH7_NETC_IPPROTO_TCP   10` |
|         - | 6641 | `#define PH7_NETC_IPPROTO_UDP   11` |
|         - | 6642 | `#define PH7_NETC_IPPROTO_ICMP  12` |
|         - | 6643 | `#define PH7_NETC_IPPROTO_RAW   13` |
|         - | 6644 | `PH7_PRIVATE ph7_int64 PH7_NetSocketConst(int iWhich);` |
|         - | 6645 | `PH7_PRIVATE int PH7_NetShutdown(ph7_socket sock,int iHow);` |
|         - | 6646 | `PH7_PRIVATE int PH7_NetAtEnd(ph7_socket sock);` |
|         - | 6647 | `PH7_PRIVATE int PH7_NetIsAlive(ph7_socket sock);` |
|         - | 6648 | `PH7_PRIVATE int PH7_NetRecvFrom(ph7_socket sock,void *pBuf,int nLen,int iFlags,char *zAddr,int nAddr);` |
|         - | 6649 | `PH7_PRIVATE int PH7_NetSendTo(ph7_socket sock,const void *pBuf,int nLen,int iFlags,` |
|         - | 6650 | `	const char *zHost,int iPort,int *pErrno);` |
|         - | 6651 | `PH7_PRIVATE int PH7_NetSocketPair(int iDomain,int iType,int iProtocol,ph7_socket *aOut,int *pErrno);` |
|         - | 6652 | `PH7_PRIVATE int PH7_NetLastError(void);` |
|         - | 6653 | `PH7_PRIVATE int PH7_NetWouldBlock(void);` |
|         - | 6654 | `PH7_PRIVATE const char * PH7_NetStrError(int iErr);` |
|         - | 6655 | `PH7_PRIVATE int PH7_NetRecv(ph7_socket sock,void *pBuf,int nLen,int flags);` |
|         - | 6656 | `PH7_PRIVATE int PH7_NetSend(ph7_socket sock,const void *pBuf,int nLen,int flags);` |
|         - | 6657 | `PH7_PRIVATE int PH7_NetSendAll(ph7_socket sock,const void *pBuf,int nLen);` |
|         - | 6658 | `PH7_PRIVATE void PH7_NetClose(ph7_socket sock);` |
|         - | 6659 | `PH7_PRIVATE void PH7_NetSetTimeout(ph7_socket sock,int iMilliseconds);` |
|         - | 6660 | `PH7_PRIVATE void PH7_NetSetRwTimeout(ph7_socket sock,ph7_int64 iSeconds,ph7_int64 iMicroseconds);` |
|         - | 6661 | `PH7_PRIVATE void PH7_NetSetBlocking(ph7_socket sock,int bBlocking);` |
|         - | 6662 | `PH7_PRIVATE void PH7_NetAddrToString(const struct sockaddr *pAddr,char *zBuf,int nBufLen);` |
|         - | 6663 | `PH7_PRIVATE int PH7_NetAddrPort(const struct sockaddr *pAddr);` |
|         - | 6664 | `/* ext/sockets (builtin_sockets.c): php's BSD socket API, which is the other` |
|         - | 6665 | ` * face of the descriptors net.c drives for the stream wrappers. */` |
|         - | 6666 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 6667 | `PH7_PRIVATE const ph7_builtin_func * PH7_SocketsFuncTable(sxu32 *pnEntry);` |
|         - | 6668 | `PH7_PRIVATE sxi32 PH7_VmInstallSockets(ph7_vm *pVm);` |
|         - | 6669 | `PH7_PRIVATE void PH7_RegisterSocketsConstants(ph7_vm *pVm);` |
|         - | 6670 | `PH7_PRIVATE void PH7_SocketsVmReset(ph7_vm *pVm);` |
|         - | 6671 | `PH7_PRIVATE void PH7_SocketsVmRelease(ph7_vm *pVm);` |
|         - | 6672 | `/* socket_strerror()'s table: the platform's own, plus php's -10000 host-lookup` |
|         - | 6673 | ` * range, which no errno occupies. */` |
|         - | 6674 | `PH7_PRIVATE const char * PH7_SocketStrError(int iErr);` |
|         - | 6675 | `#endif` |
|         - | 6676 | `/* The three doors ext/sockets uses onto the stream device stack (vfs_stream.c). */` |
|         - | 6677 | `PH7_PRIVATE int PH7_StreamSocketHandle(io_private *pDev,ph7_socket *pOut);` |
|         - | 6678 | `PH7_PRIVATE io_private * PH7_StreamWrapSocket(ph7_context *pCtx,ph7_socket sock,int bDgram,` |
|         - | 6679 | `	const char *zLabel,const char *zUri);` |
|         - | 6680 | `PH7_PRIVATE void PH7_StreamCloseExported(io_private *pDev);` |
|         - | 6681 | `#endif /* PH7_ENABLE_NET */` |
|         - | 6682 | `/* vm_json.c function prototypes */` |
|         - | 6683 | `PH7_PRIVATE int vm_builtin_json_encode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6684 | `PH7_PRIVATE int vm_builtin_json_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6685 | `PH7_PRIVATE int vm_builtin_json_last_error_msg(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6686 | `PH7_PRIVATE int vm_builtin_json_decode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6687 | `PH7_PRIVATE int vm_builtin_json_validate(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6688 | `/* vm_serialize.c function prototypes */` |
|         - | 6689 | `PH7_PRIVATE int vm_builtin_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6690 | `PH7_PRIVATE int vm_builtin_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6691 | `PH7_PRIVATE sxi32 PH7_VmUnserializeOne(ph7_context *pCtx,const char *zIn,int nByte,int *pnRead,ph7_value *pOut);` |
|         - | 6692 | `PH7_PRIVATE void PH7_AppendShortestReal(SyBlob *pOut,double d);` |
|         - | 6693 | `/* memobj.c float-shape helper (php_gcvt/smart_str_append_double semantics);` |
|         - | 6694 | ` * shared by the float->string cast and builtin.c's printf float conversions */` |
|         - | 6695 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         - | 6696 | `PH7_PRIVATE sxi32 PH7_PhpFloatShape(char *zBuf,sxi32 nLen,int bGeneric);` |
|         - | 6697 | `#endif` |
|         - | 6698 | `/* builtin.c utf8 function prototypes */` |
|         - | 6699 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult,int bReturnPropagates);` |
|         - | 6700 | `PH7_PRIVATE int VmLocalExecThrew(ph7_vm *pVm,sxi32 rc,const void *pResumeBefore,const void *pInlineBefore);` |
|         - | 6701 | `PH7_PRIVATE int PH7_VmIsAutoGlobal(const char *zName,sxu32 nByte);` |
|         - | 6702 | `/* The engine's two NON-refusing offset rules, shared with the native ArrayAccess` |
|         - | 6703 | ` * classes: a RESOURCE key warns and is rewritten in place to its integer id, and a` |
|         - | 6704 | ` * NULL key deprecates and then folds to the "" key (the caller falls through). */` |
|         - | 6705 | `PH7_PRIVATE void PH7_VmOffsetResourceWarn(ph7_vm *pVm,ph7_value *pKey);` |
|         - | 6706 | `PH7_PRIVATE int PH7_VmNullOffsetDeprecate(ph7_vm *pVm,ph7_value *pKey);` |
|         - | 6707 | `/* Wording modes for PH7_VmArrayKeyArg(): the RULES are the engine's subscript` |
|         - | 6708 | ` * rules in all three, only the two sentences differ. */` |
|         - | 6709 | ``#define PH7_ARRAYKEY_OFFSET 0 /* the engine's own offset wording, `$a[$k]`'s */`` |
|         - | 6710 | `#define PH7_ARRAYKEY_AKE    1 /* folded array_key_exists(): engine type wording, its own null clause */` |
|         - | 6711 | `#define PH7_ARRAYKEY_ZPP    2 /* any real call: php's ZPP type wording, the same null clause */` |
|         - | 6712 | `PH7_PRIVATE sxi32 PH7_VmArrayKeyArg(ph7_context *pCtx,ph7_value *pKey,int iWording);` |
|         - | 6713 | `PH7_PRIVATE sxi32 PH7_VmExecAttrArg(ph7_vm *pVm,SySet *pByteCode,ph7_class *pDeclCls,ph7_value *pResult);` |
|         - | 6714 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...);` |
|         - | 6715 | `PH7_PRIVATE sxi32 PH7_VmFatalError(ph7_vm *pVm,const char *zFormat,...);` |
|         - | 6716 | `PH7_PRIVATE sxi32 PH7_VmEmitCompileDiagnostic(ph7_vm *pVm,sxi32 iErr,const char *zLabel,const char *zBody,sxu32 nBody,const char *zBare,sxu32 nBare,sxu32 nLine);` |
|         - | 6717 | `PH7_PRIVATE sxu32 PH7_ClassAbstractGap(ph7_vm *pVm,ph7_class *pClass,SyBlob *pMsg);` |
|         - | 6718 | `/* vm_builtin_class.c function prototypes */` |
|         - | 6719 | `PH7_PRIVATE int PH7_VmInstanceOf(ph7_class *pThis,ph7_class *pClass);` |
|         - | 6720 | `PH7_PRIVATE int PH7_SplOuterForward(ph7_vm *pVm,ph7_class_instance *pThis,SyString *pName,ph7_class_instance **ppInner,ph7_class_method **ppMeth);` |
|         - | 6721 | `PH7_PRIVATE void PH7_VmStampThrowableSite(ph7_vm *pVm,ph7_class_instance *pThis);` |
|         - | 6722 | `PH7_PRIVATE int PH7_VmClassMemberAccess(ph7_vm *pVm,ph7_class *pClass,const SyString *pAttrName,sxi32 iProtection,int bLog);` |
|         - | 6723 | `PH7_PRIVATE int PH7_VmClassAttrAccess(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,int bLog);` |
|         - | 6724 | `PH7_PRIVATE ph7_class * PH7_VmCallerScope(ph7_vm *pVm);` |
|         - | 6725 | `PH7_PRIVATE ph7_class * PH7_VmMethodScopeName(ph7_vm *pVm,ph7_class *pClass,ph7_class_method *pMeth);` |
|         - | 6726 | `PH7_PRIVATE int PH7_VmFccMethodIsDirect(ph7_vm *pVm,ph7_class *pClass,const char *zName,sxu32 nName);` |
|         - | 6727 | `PH7_PRIVATE ph7_class_instance * PH7_VmFccClassReceiver(ph7_vm *pVm,ph7_class *pClass,const char *zMeth,sxu32 nMeth);` |
|         - | 6728 | `PH7_PRIVATE ph7_class * PH7_VmClosureScopeClass(ph7_vm *pVm,ph7_class_instance *pClosure);` |
|         - | 6729 | `PH7_PRIVATE int PH7_VmClosureIsStatic(ph7_vm *pVm,ph7_class_instance *pClosure);` |
|         - | 6730 | `PH7_PRIVATE ph7_class * PH7_VmComposingClass(ph7_class *pClass,ph7_class *pDecl);` |
|         - | 6731 | `PH7_PRIVATE void PH7_ClassMethodRegisteredName(ph7_class *pClass,const char *zName,sxu32 nByte,SyString *pOut);` |
|         - | 6732 | `PH7_PRIVATE ph7_class * PH7_VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg);` |
|         - | 6733 | `PH7_PRIVATE int vm_builtin_get_class(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6734 | `PH7_PRIVATE int vm_builtin_get_parent_class(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6735 | `PH7_PRIVATE int vm_builtin_get_called_class(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6736 | `PH7_PRIVATE int vm_builtin_property_exists(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6737 | `PH7_PRIVATE int vm_builtin_method_exists(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6738 | `PH7_PRIVATE int vm_builtin_class_exists(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6739 | `PH7_PRIVATE int vm_builtin_interface_exists(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6740 | `PH7_PRIVATE int vm_builtin_trait_exists(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6741 | `PH7_PRIVATE int vm_builtin_class_alias(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6742 | `PH7_PRIVATE int vm_builtin_get_declared_classes(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6743 | `PH7_PRIVATE int vm_builtin_get_declared_interfaces(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6744 | `PH7_PRIVATE int vm_builtin_get_declared_traits(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6745 | `PH7_PRIVATE int vm_builtin_get_class_methods(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6746 | `PH7_PRIVATE int vm_builtin_class_parents(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6747 | `PH7_PRIVATE int vm_builtin_class_implements(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6748 | `PH7_PRIVATE int vm_builtin_class_uses(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6749 | `PH7_PRIVATE int vm_builtin_get_class_vars(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6750 | `PH7_PRIVATE int vm_builtin_get_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6751 | `PH7_PRIVATE int vm_builtin_get_mangled_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6752 | `PH7_PRIVATE int vm_builtin_is_a(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6753 | `PH7_PRIVATE int vm_builtin_clone(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6754 | `PH7_PRIVATE int vm_builtin_spl_object_id(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6755 | `PH7_PRIVATE int vm_builtin_spl_object_hash(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6756 | `PH7_PRIVATE int vm_builtin_is_subclass_of(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6757 | `PH7_PRIVATE int vm_builtin_call_user_func(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6758 | `PH7_PRIVATE int vm_builtin_call_user_func_array(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6759 | `/* vm_builtin_ob.c function prototypes */` |
|         - | 6760 | `PH7_PRIVATE int VmObConsumer(const void *pData,unsigned int nDataLen,void *pUserData);` |
|         - | 6761 | `PH7_PRIVATE void PH7_VmObFlushAll(ph7_vm *pVm);` |
|         - | 6762 | `PH7_PRIVATE int vm_builtin_ob_clean(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6763 | `PH7_PRIVATE int vm_builtin_ob_end_clean(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6764 | `PH7_PRIVATE int vm_builtin_ob_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6765 | `PH7_PRIVATE int vm_builtin_ob_get_clean(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6766 | `PH7_PRIVATE int vm_builtin_flush(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6767 | `PH7_PRIVATE int vm_builtin_ob_get_flush(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6768 | `PH7_PRIVATE int vm_builtin_ob_get_status(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6769 | `PH7_PRIVATE int vm_builtin_ob_get_length(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6770 | `PH7_PRIVATE int vm_builtin_ob_get_level(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6771 | `PH7_PRIVATE int vm_builtin_ob_start(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6772 | `PH7_PRIVATE int vm_builtin_ob_flush(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6773 | `PH7_PRIVATE int vm_builtin_ob_end_flush(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6774 | `PH7_PRIVATE int vm_builtin_ob_implicit_flush(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6775 | `PH7_PRIVATE int vm_builtin_ob_list_handlers(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6776 | `/* vm_builtin_getopt.c function prototypes */` |
|         - | 6777 | `PH7_PRIVATE int vm_builtin_getopt(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6778 | `/* vm_random.c function prototypes */` |
|         - | 6779 | `/* php's ext/random object surface: the Random\Engine contract, its errors,` |
|         - | 6780 | ` * the seeded engines and the Randomizer that consumes them. */` |
|         - | 6781 | `PH7_PRIVATE sxi32 PH7_VmInstallRandom(ph7_vm *pVm);` |
|         - | 6782 | `/* builtin_math.c function prototypes */` |
|         - | 6783 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|         - | 6784 | `PH7_PRIVATE int PH7_builtin_sqrt(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6785 | `PH7_PRIVATE int PH7_builtin_acosh(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6786 | `PH7_PRIVATE int PH7_builtin_asinh(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6787 | `PH7_PRIVATE int PH7_builtin_atanh(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6788 | `PH7_PRIVATE int PH7_builtin_expm1(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6789 | `PH7_PRIVATE int PH7_builtin_log1p(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6790 | `PH7_PRIVATE int PH7_builtin_deg2rad(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6791 | `PH7_PRIVATE int PH7_builtin_rad2deg(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6792 | `PH7_PRIVATE int PH7_builtin_fpow(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6793 | `PH7_PRIVATE int PH7_builtin_exp(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6794 | `PH7_PRIVATE int PH7_builtin_floor(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6795 | `PH7_PRIVATE int PH7_builtin_cos(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6796 | `PH7_PRIVATE int PH7_builtin_acos(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6797 | `PH7_PRIVATE int PH7_builtin_cosh(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6798 | `PH7_PRIVATE int PH7_builtin_sin(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6799 | `PH7_PRIVATE int PH7_builtin_asin(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6800 | `PH7_PRIVATE int PH7_builtin_sinh(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6801 | `PH7_PRIVATE int PH7_builtin_ceil(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6802 | `PH7_PRIVATE int PH7_builtin_tan(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6803 | `PH7_PRIVATE int PH7_builtin_atan(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6804 | `PH7_PRIVATE int PH7_builtin_tanh(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6805 | `PH7_PRIVATE int PH7_builtin_atan2(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6806 | `PH7_PRIVATE int PH7_builtin_abs(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6807 | `PH7_PRIVATE int PH7_builtin_log(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6808 | `PH7_PRIVATE int PH7_builtin_log10(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6809 | `PH7_PRIVATE int PH7_builtin_pow(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6810 | `PH7_PRIVATE int PH7_builtin_pi(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6811 | `PH7_PRIVATE int PH7_builtin_fmod(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6812 | `PH7_PRIVATE int PH7_builtin_hypot(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6813 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|         - | 6814 | `PH7_PRIVATE int PH7_builtin_round(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6815 | `/*` |
|         - | 6816 | ` * PHP's rounding modes (mirror ext/standard/php_math_round_mode.h). Only the` |
|         - | 6817 | ` * four HALF_* integers are exposed to userland (PHP_ROUND_HALF_UP..HALF_ODD,` |
|         - | 6818 | ` * see constant.c); the CEILING/FLOOR/TOWARD_ZERO/AWAY_FROM_ZERO modes (5..8)` |
|         - | 6819 | ` * have no userland constant but are reachable by passing the raw integer to` |
|         - | 6820 | ` * round()'s 3rd argument, which PHP 8.5 still accepts, so all eight are` |
|         - | 6821 | `` * honored. `enum RoundingMode` names all eight and numbers them DIFFERENTLY --`` |
|         - | 6822 | ` * PH7_RoundingModeCase() is the translation.` |
|         - | 6823 | ` */` |
|         - | 6824 | `#define PH7_ROUND_HALF_UP        1` |
|         - | 6825 | `#define PH7_ROUND_HALF_DOWN      2` |
|         - | 6826 | `#define PH7_ROUND_HALF_EVEN      3` |
|         - | 6827 | `#define PH7_ROUND_HALF_ODD       4` |
|         - | 6828 | `#define PH7_ROUND_CEILING        5` |
|         - | 6829 | `#define PH7_ROUND_FLOOR          6` |
|         - | 6830 | `#define PH7_ROUND_TOWARD_ZERO    7` |
|         - | 6831 | `#define PH7_ROUND_AWAY_FROM_ZERO 8` |
|         - | 6832 | ``/* php 8.4's `enum RoundingMode`, declared beside round() -- round() and`` |
|         - | 6833 | ` * bcround() are its two consumers. */` |
|         - | 6834 | `PH7_PRIVATE sxi32 PH7_VmInstallRoundingMode(ph7_vm *pVm);` |
|         - | 6835 | `PH7_PRIVATE int PH7_RoundingModeCase(ph7_value *pVal,int *pMode);` |
|         - | 6836 | `PH7_PRIVATE int PH7_builtin_intdiv(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6837 | `PH7_PRIVATE int PH7_builtin_number_format(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6838 | `PH7_PRIVATE int PH7_builtin_dechex(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6839 | `PH7_PRIVATE int PH7_builtin_decoct(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6840 | `PH7_PRIVATE int PH7_builtin_decbin(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6841 | `PH7_PRIVATE int PH7_builtin_hexdec(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6842 | `PH7_PRIVATE int PH7_builtin_bindec(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6843 | `PH7_PRIVATE int PH7_builtin_octdec(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6844 | `PH7_PRIVATE int PH7_builtin_srand(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6845 | `PH7_PRIVATE int PH7_builtin_base_convert(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6846 | `/* builtin_date.c function prototypes */` |
|         - | 6847 | `PH7_PRIVATE int PH7_builtin_time(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6848 | `PH7_PRIVATE int PH7_builtin_microtime(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6849 | `PH7_PRIVATE int PH7_builtin_hrtime(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6850 | `PH7_PRIVATE int PH7_builtin_getrusage(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6851 | `#ifdef PH7_ENABLE_PCRE` |
|         - | 6852 | `PH7_PRIVATE void PH7_PcreVersionInfo(char *zBuf,int nBuf,int *pMajor,int *pMinor,int *pJit);` |
|         - | 6853 | `#endif` |
|         - | 6854 | `PH7_PRIVATE int PH7_builtin_getdate(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6855 | `PH7_PRIVATE int PH7_builtin_gettimeofday(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6856 | `PH7_PRIVATE int PH7_builtin_date(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6857 | `PH7_PRIVATE int PH7_builtin_gmdate(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6858 | `PH7_PRIVATE int PH7_builtin_localtime(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6859 | `PH7_PRIVATE int PH7_builtin_idate(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6860 | `PH7_PRIVATE int PH7_builtin_mktime(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6861 | `PH7_PRIVATE int PH7_builtin_date_default_timezone_get(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6862 | `PH7_PRIVATE int PH7_builtin_date_default_timezone_set(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6863 | `PH7_PRIVATE int PH7_builtin_date_sun_info(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6864 | `PH7_PRIVATE int PH7_builtin_date_sunrise(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6865 | `PH7_PRIVATE int PH7_builtin_date_sunset(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6866 | `/* builtin_mb.c (UTF-8-only mb_* family) */` |
|         - | 6867 | ``/* Shared ZPP helper: resolve an `int`-typed parameter with php's full contract`` |
|         - | 6868 | ` * (null deprecation, lossy float / float-string deprecations, TypeErrors for` |
|         - | 6869 | ` * NAN/INF/non-numeric). Builtins with int params should use it instead of a` |
|         - | 6870 | ` * bare ph7_value_to_int64(), which coerces silently. */` |
|         - | 6871 | `PH7_PRIVATE sxi32 PH7_IntArgResolve(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,` |
|         - | 6872 | `	int iArgNum,const char *zParamName,const char *zTypeStr,sxi64 *pOut);` |
|         - | 6873 | `PH7_PRIVATE int PH7_builtin_mb_strlen_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6874 | `PH7_PRIVATE int PH7_builtin_mb_substr_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6875 | `PH7_PRIVATE int PH7_builtin_mb_case_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6876 | `PH7_PRIVATE int PH7_builtin_mb_convert_case_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6877 | `PH7_PRIVATE int PH7_builtin_mb_strpos_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6878 | `PH7_PRIVATE int PH7_builtin_mb_str_split_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6879 | `PH7_PRIVATE int PH7_builtin_mb_trim_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6880 | `PH7_PRIVATE int PH7_builtin_mb_internal_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6881 | `PH7_PRIVATE int PH7_builtin_mb_substitute_character_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6882 | `PH7_PRIVATE int PH7_builtin_mb_scrub_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6883 | `PH7_PRIVATE int PH7_builtin_mb_check_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6884 | `PH7_PRIVATE int PH7_builtin_mb_strwidth_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6885 | `PH7_PRIVATE int PH7_builtin_mb_chr_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6886 | `PH7_PRIVATE int PH7_builtin_mb_ord_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6887 | `PH7_PRIVATE int PH7_builtin_mb_ucfirst_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6888 | `PH7_PRIVATE int PH7_builtin_mb_strstr_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6889 | `PH7_PRIVATE int PH7_builtin_mb_substr_count_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6890 | `PH7_PRIVATE int PH7_builtin_mb_str_pad_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6891 | `PH7_PRIVATE int PH7_builtin_mb_strcut_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6892 | `PH7_PRIVATE int PH7_builtin_mb_strimwidth_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6893 | `PH7_PRIVATE int PH7_builtin_mb_detect_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6894 | `PH7_PRIVATE int PH7_builtin_mb_detect_order_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6895 | `PH7_PRIVATE int PH7_builtin_mb_list_encodings_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6896 | `PH7_PRIVATE int PH7_builtin_mb_encoding_aliases_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6897 | `PH7_PRIVATE int PH7_builtin_mb_convert_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6898 | `/* iconv (builtin_iconv.c) */` |
|         - | 6899 | `PH7_PRIVATE int PH7_builtin_iconv_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6900 | `PH7_PRIVATE int PH7_IconvTranslate(SyBlob *pOut,const char *zIn,int nIn,` |
|         - | 6901 | `	const char *zFrom,int nFrom,const char *zTo,int nTo);` |
|         - | 6902 | `PH7_PRIVATE int PH7_builtin_iconv_strlen_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6903 | `PH7_PRIVATE int PH7_builtin_iconv_substr_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6904 | `PH7_PRIVATE int PH7_builtin_iconv_strpos_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6905 | `PH7_PRIVATE int PH7_builtin_iconv_strrpos_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6906 | `PH7_PRIVATE int PH7_builtin_iconv_get_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6907 | `PH7_PRIVATE int PH7_builtin_iconv_mime_encode_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6908 | `PH7_PRIVATE int PH7_builtin_iconv_mime_decode_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6909 | `PH7_PRIVATE int PH7_builtin_iconv_mime_decode_headers_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6910 | `/* vm_builtin_spl.c */` |
|         - | 6911 | `PH7_PRIVATE sxi32 PH7_VmInstallSpl(ph7_vm *pVm);` |
|         - | 6912 | `/* vm_builtin_tokenizer.c */` |
|         - | 6913 | `PH7_PRIVATE sxi32 PH7_VmInstallTokenizer(ph7_vm *pVm);` |
|         - | 6914 | `PH7_PRIVATE void PH7_RegisterTokenizerConstants(ph7_vm *pVm);` |
|         - | 6915 | `/* vm_builtin_session.c */` |
|         - | 6916 | `PH7_PRIVATE sxi32 PH7_VmInstallSession(ph7_vm *pVm);` |
|         - | 6917 | `PH7_PRIVATE void PH7_VmSessionShutdown(ph7_vm *pVm);` |
|         - | 6918 | `/* vm_builtin_ini.c */` |
|         - | 6919 | `PH7_PRIVATE sxi32 PH7_VmInstallIni(ph7_vm *pVm);` |
|         - | 6920 | `PH7_PRIVATE sxi64 PH7_VmIniGetInt(ph7_vm *pVm,const char *zName,sxi64 iDefault);` |
|         - | 6921 | `PH7_PRIVATE int PH7_VmApplyMemoryLimit(ph7_vm *pVm,const char *zVal,sxu32 nVal);` |
|         - | 6922 | `PH7_PRIVATE void PH7_VmIniGetStr(ph7_vm *pVm,const char *zName,SyBlob *pOut);` |
|         - | 6923 | `PH7_PRIVATE int PH7_VmIniIsUnset(ph7_vm *pVm,const char *zName);` |
|         - | 6924 | `PH7_PRIVATE int PH7_VmIniDescribe(ph7_vm *pVm,const char *zName,sxu32 nName,` |
|         - | 6925 | `	sxi32 *piAccess,SyBlob *pOut,SyBlob *pDef);` |
|         - | 6926 | `PH7_PRIVATE int PH7_VmIniGetBool(ph7_vm *pVm,const char *zName,int bDefault);` |
|         - | 6927 | `PH7_PRIVATE int PH7_VmIniSet(ph7_vm *pVm,const char *zName,sxu32 nName,const char *zVal,sxu32 nVal,const char *zWho);` |
|         - | 6928 | `/* vfs_win.c / vfs_unix.c exported structs */` |
|         - | 6929 | `#ifdef __WINNT__` |
|         - | 6930 | `extern const ph7_vfs sWinVfs;` |
|         - | 6931 | `extern const ph7_io_stream sWinFileStream;` |
|         - | 6932 | `/* Would php's MapViewOfFile() copy of this plain file map a view of zero` |
|         - | 6933 | ` * requested bytes? stream_copy_to_stream() answers false then. */` |
|         - | 6934 | `PH7_PRIVATE int PH7_WinFileMapsEmptyView(void *pHandle,ph7_int64 nAhead);` |
|         - | 6935 | `/* The Win32 code (0: none) and php's text for the last failed opendir(). */` |
|         - | 6936 | `PH7_PRIVATE unsigned long PH7_WinOpenDirReason(char *zBuf,int nBuf);` |
|         - | 6937 | `#elif defined(__UNIXES__)` |
|         - | 6938 | `extern const ph7_vfs sUnixVfs;` |
|         - | 6939 | `extern const ph7_io_stream sUnixFileStream;` |
|         - | 6940 | `#endif` |
|         - | 6941 | `/* Built-in IO stream drivers: tcp:// lives in vfs_stream.c; php://, data://` |
|         - | 6942 | ` * and the pipe (popen) stream live in vfs_io_driver.c. Registration (vfs.c)` |
|         - | 6943 | ` * and the standard-stream exporters reference them across those files. */` |
|         - | 6944 | `extern const ph7_io_stream sTCP_Stream;` |
|         - | 6945 | `/* vfs_http.c -- what a script reads BACK from an http:// exchange. The store is` |
|         - | 6946 | ` * in every build: the two php 8.4 getters over it are ordinary builtins, and a` |
|         - | 6947 | ` * build with no network simply never records anything into it. */` |
|         - | 6948 | `PH7_PRIVATE void PH7_HttpPublishHeaders(ph7_vm *pVm,SyBlob *pLines);` |
|         - | 6949 | `PH7_PRIVATE ph7_value * PH7_HttpHeaderArray(ph7_vm *pVm,SyBlob *pLines);` |
|         - | 6950 | `PH7_PRIVATE void PH7_HttpFlushResponseHeaders(ph7_vm *pVm);` |
|         - | 6951 | `PH7_PRIVATE void PH7_HttpClearResponseHeaders(ph7_vm *pVm);` |
|         - | 6952 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 6953 | `PH7_PRIVATE void PH7_HttpInstallFuncs(ph7_vm *pVm);` |
|         - | 6954 | `#endif` |
|         - | 6955 | `#if !defined(PH7_DISABLE_DISK_IO) && defined(PH7_ENABLE_NET)` |
|         - | 6956 | `/* ... and the wrapper itself, which is where the store is filled. */` |
|         - | 6957 | `extern const ph7_io_stream sHTTP_Stream;` |
|         - | 6958 | `#ifdef PH7_ENABLE_OPENSSL` |
|         - | 6959 | `/* The same wrapper over the ssl:// transport, which is all php's https:// is:` |
|         - | 6960 | ` * a device of its own so the SCHEME a URL was opened under is known without` |
|         - | 6961 | ` * re-reading it, since only the name it was found under says so. */` |
|         - | 6962 | `extern const ph7_io_stream sHTTPS_Stream;` |
|         - | 6963 | `/* The ssl:// transport's client handshake, for a socket that is not a stream` |
|         - | 6964 | ` * handle: the http wrapper dials its own. */` |
|         - | 6965 | `struct phl_stream_ctx;` |
|         - | 6966 | `PH7_PRIVATE int PH7_SslClientHandshake(ph7_vm *pVm,ph7_socket sock,` |
|         - | 6967 | `	struct phl_stream_ctx *pCtxRes,const char *zPeerName,void **ppSsl,void **ppSslCtx,` |
|         - | 6968 | `	char *zErr,int nErr);` |
|         - | 6969 | `PH7_PRIVATE void PH7_SslDropSession(void **ppSsl,void **ppSslCtx);` |
|         - | 6970 | `#endif` |
|         - | 6971 | `PH7_PRIVATE int PH7_HttpStreamIs(const ph7_io_stream *pStream);` |
|         - | 6972 | `PH7_PRIVATE ph7_value * PH7_HttpStreamHeaderArray(ph7_vm *pVm,void *pHandle);` |
|         - | 6973 | `PH7_PRIVATE ph7_socket * PH7_HttpStreamSocket(void *pHandle);` |
|         - | 6974 | `PH7_PRIVATE sxu32 PH7_HttpStreamUnread(void *pHandle);` |
|         - | 6975 | `PH7_PRIVATE int PH7_HttpStreamAtEof(void *pHandle);` |
|         - | 6976 | `#endif /* !PH7_DISABLE_DISK_IO && PH7_ENABLE_NET */` |
|         - | 6977 | `extern const ph7_io_stream sDATA_Stream;` |
|         - | 6978 | `extern const ph7_io_stream sPHP_Stream;` |
|         - | 6979 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 6980 | `/* glob:// (vfs.c): a directory whose entries are a pattern's matches. It has no` |
|         - | 6981 | ` * xOpen at all, which is php's wrapper too. The three accessors are what SPL` |
|         - | 6982 | ` * asks of a directory handle that turns out to be this one: php's` |
|         - | 6983 | ` * php_stream_is(), php_glob_stream_get_path() and php_glob_stream_get_count(). */` |
|         - | 6984 | `extern const ph7_io_stream sGLOB_Stream;` |
|         - | 6985 | `PH7_PRIVATE int PH7_GlobStreamIs(const ph7_io_stream *pStream);` |
|         - | 6986 | `PH7_PRIVATE const char * PH7_GlobStreamPath(void *pHandle,int *pnLen);` |
|         - | 6987 | `PH7_PRIVATE sxi64 PH7_GlobStreamCount(void *pHandle);` |
|         - | 6988 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 6989 | `/* IO private state carried by every open stream handle (fopen/opendir/popen` |
|         - | 6990 | ` * resources and the exported std streams). Shared between vfs.c,` |
|         - | 6991 | ` * vfs_stream.c and vfs_io_driver.c. */` |
|         - | 6992 | `struct io_private` |
|         - | 6993 | `{` |
|         - | 6994 | `	/* THE FIRST WORD, and the only field a probe may read on a resource pointer` |
|         - | 6995 | ``	 * it has not identified yet. `iMagic` below sits ~240 bytes in, and a`` |
|         - | 6996 | `	 * resource that is not a stream can be much smaller than that -- a` |
|         - | 6997 | `	 * Generator's context is 184 bytes -- so reading the tail magic to ask "is` |
|         - | 6998 | `	 * this a stream?" reads past the end of the object. Offset zero is in bounds` |
|         - | 6999 | `	 * for every allocation there is. Written by InitIOPrivate and by nothing` |
|         - | 7000 | `	 * else, so it also says the layout below is real: the handles that are NOT` |
|         - | 7001 | `	 * streams but open with an io_private header (a context, a filter, a bucket` |
|         - | 7002 | `	 * brigade) build theirs by hand and leave this zero. */` |
|         - | 7003 | `	sxu32 iHead;` |
|         - | 7004 | `	const ph7_io_stream *pStream; /* Underlying IO device */` |
|         - | 7005 | `	void *pHandle; /* IO handle */` |
|         - | 7006 | `	/* Unbuffered IO */` |
|         - | 7007 | `	SyBlob sBuffer; /* Working buffer */` |
|         - | 7008 | `	sxu32 nOfft;    /* Current read offset */` |
|         - | 7009 | `	/* What the opener was ASKED for. php reports both back from` |
|         - | 7010 | ``	 * stream_get_meta_data() and neither was retained here, so the `uri` and`` |
|         - | 7011 | ``	 * `mode` keys of that array simply did not exist. sUri stays EMPTY for a`` |
|         - | 7012 | `	 * stream php opens without a wrapper (a popen() pipe), which is exactly` |
|         - | 7013 | `	 * when php omits the key. */` |
|         - | 7014 | `	SyBlob sUri;     /* the path/URI as the opener received it */` |
|         - | 7015 | `	char zMode[16];  /* the mode string, php's own field width */` |
|         - | 7016 | `	/* Per-handle settings the stream_set_* family writes. */` |
|         - | 7017 | `	sxu32 nChunk;    /* stream_set_chunk_size(), php's 8192 by default */` |
|         - | 7018 | `	sxu8 bNonBlock;  /* stream_set_blocking(false) took effect at the descriptor */` |
|         - | 7019 | `	sxu8 bHasTimeout;/* stream_set_timeout() armed one, so an EAGAIN read EXPIRED */` |
|         - | 7020 | ``	sxu8 bTimedOut;  /* the last read expired; php's meta `timed_out`, cleared by the next */`` |
|         - | 7021 | `	sxu8 bEof;       /* a read on this handle has already come back empty */` |
|         - | 7022 | `	int iLastReadErr;/* errno of the last device read that FAILED, latched for the` |
|         - | 7023 | `	                  * reader to announce (php's "Read of N bytes failed with` |
|         - | 7024 | `	                  * errno=..." notice) and cleared once it has. 0 = nothing` |
|         - | 7025 | `	                  * to report; a read that merely found EOF never sets it. */` |
|         - | 7026 | `	sxu8 bDir;       /* opendir()/dir() handle rather than a byte stream */` |
|         - | 7027 | `	/* Where the SCRIPT is on a device that cannot say it itself -- a popen()` |
|         - | 7028 | `	 * pipe, a socket, a directory handle. php's stream layer tracks a position` |
|         - | 7029 | `	 * for EVERY stream and only asks the device when it seeks, so ftell() on a` |
|         - | 7030 | `	 * pipe answers the bytes that have gone past rather than failing; this is` |
|         - | 7031 | `	 * that counter, and it is read only when the device has no xTell. A` |
|         - | 7032 | `	 * directory handle steps it by one php_stream_dirent per entry read` |
|         - | 7033 | `	 * (PHL_DIR_RECORD), which is the number php's own ftell() reports.` |
|         - | 7034 | `	 *` |
|         - | 7035 | `	 * It is the ONLY answer ftell() gives: php never asks the device again, and` |
|         - | 7036 | `	 * on a handle opened for APPEND the two numbers part company on the first` |
|         - | 7037 | `	 * write -- the descriptor jumps to the end of the file, php's counter moves` |
|         - | 7038 | `	 * by the bytes written. bPosSeeded records that the device has been asked` |
|         - | 7039 | `	 * once, which is php's own single lseek() at open time: it is 0 for a file` |
|         - | 7040 | `	 * and -1 for a descriptor that cannot say (a proc_open() pipe), and ftell()` |
|         - | 7041 | `	 * on the latter answers false until enough bytes have gone past. */` |
|         - | 7042 | `	ph7_int64 iPos;` |
|         - | 7043 | `	sxu8 bPosSeeded; /* the device was asked where it started */` |
|         - | 7044 | `	sxu8 bPersist;   /* opened PERSISTENTLY: get_resource_type() names it apart */` |
|         - | 7045 | `	/* The stream CONTEXT this handle carries (phl_stream_ctx*), owned by the VM` |
|         - | 7046 | `	 * registry. php attaches the opener's context to a TRANSPORT stream and to` |
|         - | 7047 | `	 * nothing else, and creates one on demand for a` |
|         - | 7048 | `	 * stream_context_set_option($stream,…). */` |
|         - | 7049 | `	void *pCtxRes;` |
|         - | 7050 | `	/* The two FILTER chains this handle carries (phl_stream_filter*, head first).` |
|         - | 7051 | `	 * php runs the read chain on what came off the device before the script sees` |
|         - | 7052 | `	 * it and the write chain on what the script wrote before the device does, so` |
|         - | 7053 | `	 * a filtered read cannot be served straight into the caller's buffer: a` |
|         - | 7054 | `	 * filter changes the byte COUNT. sFilt is where the chain's output waits. */` |
|         - | 7055 | `	void *pReadFilters;   /* phl_stream_filter* — read chain head, or 0 */` |
|         - | 7056 | `	void *pWriteFilters;  /* phl_stream_filter* — write chain head, or 0 */` |
|         - | 7057 | `	SyBlob sFilt;         /* filtered bytes not yet handed to a reader */` |
|         - | 7058 | `	sxu32 nFiltOfft;      /* read offset inside sFilt */` |
|         - | 7059 | `	sxu8 bFiltDone;       /* the read chain already had its CLOSING call */` |
|         - | 7060 | `	sxu8 bFiltErr;        /* a filter REFUSED: the next read answers false, once */` |
|         - | 7061 | `	ph7_int64 iFiltPos;   /* bytes the read CHAIN has delivered: php's position` |
|         - | 7062 | `	                       * for a filtered stream counts what came OUT, which` |
|         - | 7063 | `	                       * has nothing to do with the device's own offset */` |
|         - | 7064 | `	sxu32 iMagic;   /* Sanity check to avoid misuse */` |
|         - | 7065 | `	/* How many ph7_values name this handle. php's stream is refcounted and its` |
|         - | 7066 | ``	 * last holder closes it: `$h = fopen(...); $h = null;` releases the`` |
|         - | 7067 | `	 * descriptor there and used to leak it here, because a handle was a bare` |
|         - | 7068 | `	 * pointer nobody owned. Counted at the three value doors (Load, Store,` |
|         - | 7069 | `	 * Release) exactly the way a hashmap and an instance already are; a holder` |
|         - | 7070 | `	 * that is NOT a ph7_value -- the VM's own STDIN/STDOUT/STDERR, the` |
|         - | 7071 | `	 * persistent-socket registry, a native object's handle slot -- takes its own` |
|         - | 7072 | `	 * count so a script dropping its copy cannot close the handle underneath it.` |
|         - | 7073 | `	 * Only a live io_private (iMagic == IO_PRIVATE_MAGIC) carries one: every` |
|         - | 7074 | `	 * other resource kind shares this header's magic word and nothing else. */` |
|         - | 7075 | `	sxi32 nValRef;` |
|         - | 7076 | `};` |
|         - | 7077 | `#define IO_PRIVATE_MAGIC 0xFEAC14` |
|         - | 7078 | `/* io_private.iHead: "the bytes at this pointer really are an io_private". */` |
|         - | 7079 | `#define IO_PRIVATE_HEAD_MAGIC 0x10DEA5` |
|         - | 7080 | `/* proc_open()'s handle is an io_private with this magic in the same field, which` |
|         - | 7081 | `` * is what lets one probe tell the two apart — and what php names `process`. */`` |
|         - | 7082 | `#define PROC_PRIVATE_MAGIC 0x9C0DE5` |
|         - | 7083 | `/* A user-facing handle (fopen/opendir/popen) that has been fclose()'d/closedir()'d/` |
|         - | 7084 | ` * pclose()'d keeps its io_private alive but stamped with this magic, so every` |
|         - | 7085 | ` * ph7_value that still references it observes a closed resource` |
|         - | 7086 | ` * (gettype()=='resource (closed)', is_resource()==false), matching php. */` |
|         - | 7087 | `#define IO_PRIVATE_CLOSED_MAGIC 0xC105ED` |
|         - | 7088 | `/* stream_context_create()'s handle carries this magic in the same field, for the` |
|         - | 7089 | ` * same reason proc_open()'s does: php makes a context a RESOURCE, and the only` |
|         - | 7090 | ` * thing a resource probe can look at here is that word. */` |
|         - | 7091 | `#define STREAM_CTX_MAGIC 0xC07E47` |
|         - | 7092 | `/* Make sure we are dealing with a valid io_private instance */` |
|         - | 7093 | `#define IO_PRIVATE_INVALID(IO) ( IO == 0 \|\| IO->iMagic != IO_PRIVATE_MAGIC )` |
|         - | 7094 | `/*` |
|         - | 7095 | ` * One php stream CONTEXT: the wrapper => option => value map a script hands an` |
|         - | 7096 | ``  * opener, plus the `notification` parameter. php's is a `stream-context` `` |
|         - | 7097 | ` * resource, and a PHL resource is a bare void*, so the struct opens with an` |
|         - | 7098 | ` * io_private-compatible header (proc_open()'s handle does the same) — every` |
|         - | 7099 | ` * resource probe reads that magic and stays in bounds. The VM owns the chain` |
|         - | 7100 | ` * and drops it at reset, so a reused VM does not carry one request's default` |
|         - | 7101 | ` * context into the next.` |
|         - | 7102 | ` */` |
|         - | 7103 | `/*` |
|         - | 7104 | `` * php's STREAM_NOTIFY_* -- WHICH event a context's `notification` callback is`` |
|         - | 7105 | ` * being told about -- and the three severities beside them. RESOLVE, the two` |
|         - | 7106 | ` * AUTH codes and AUTH_RESULT are php's own numbers for events no wrapper here` |
|         - | 7107 | ` * raises; they are defined because a script SWITCHES on them, and a name that` |
|         - | 7108 | ` * is missing is a fatal where an unreachable one is simply never matched.` |
|         - | 7109 | ` */` |
|         - | 7110 | `#define PHL_STREAM_NOTIFY_RESOLVE       1` |
|         - | 7111 | `#define PHL_STREAM_NOTIFY_CONNECT       2` |
|         - | 7112 | `#define PHL_STREAM_NOTIFY_AUTH_REQUIRED 3` |
|         - | 7113 | `#define PHL_STREAM_NOTIFY_MIME_TYPE_IS  4` |
|         - | 7114 | `#define PHL_STREAM_NOTIFY_FILE_SIZE_IS  5` |
|         - | 7115 | `#define PHL_STREAM_NOTIFY_REDIRECTED    6` |
|         - | 7116 | `#define PHL_STREAM_NOTIFY_PROGRESS      7` |
|         - | 7117 | `#define PHL_STREAM_NOTIFY_COMPLETED     8` |
|         - | 7118 | `#define PHL_STREAM_NOTIFY_FAILURE       9` |
|         - | 7119 | `#define PHL_STREAM_NOTIFY_AUTH_RESULT   10` |
|         - | 7120 | `#define PHL_STREAM_NOTIFY_SEVERITY_INFO 0` |
|         - | 7121 | `#define PHL_STREAM_NOTIFY_SEVERITY_WARN 1` |
|         - | 7122 | `#define PHL_STREAM_NOTIFY_SEVERITY_ERR  2` |
|         - | 7123 | `typedef struct phl_stream_ctx phl_stream_ctx;` |
|         - | 7124 | `struct phl_stream_ctx` |
|         - | 7125 | `{` |
|         - | 7126 | `	io_private base;        /* io_private-compatible header (base.iMagic == STREAM_CTX_MAGIC) */` |
|         - | 7127 | `	ph7_vm *pVm;            /* owning VM */` |
|         - | 7128 | `	ph7_value *pOptions;    /* the wrapper => (option => value) map; never 0 */` |
|         - | 7129 | ``	ph7_value *pNotify;     /* the `notification` param, or 0 when none was set */`` |
|         - | 7130 | `	/* php's notifier carries the PROGRESS counter on the CONTEXT rather than on` |
|         - | 7131 | `	 * the stream, and never disarms it: a context reused for a second exchange` |
|         - | 7132 | `	 * reports that exchange's request write and header read under the FIRST` |
|         - | 7133 | `	 * one's running total, until the wrapper's own progress_init resets it.` |
|         - | 7134 | `	 * That is visible from a script, so it is modelled rather than approximated. */` |
|         - | 7135 | `	sxi64 iProgress;        /* bytes counted since the last init */` |
|         - | 7136 | `	sxi64 iProgressMax;     /* what the wrapper announced, or 0 for "unknown" */` |
|         - | 7137 | `	int bProgress;          /* has an init armed the counter yet? */` |
|         - | 7138 | `	int bNotifyDead;        /* the callback threw: php stops calling it */` |
|         - | 7139 | `	/* Registry chain (pVm->pStreamCtx), DOUBLY linked: a context whose last` |
|         - | 7140 | `	 * holder goes away is freed there and then, the way php's refcounted` |
|         - | 7141 | ``	 * `stream-context` resource is, so unlinking one may not walk the chain.`` |
|         - | 7142 | `	 * base.nValRef counts the holders -- every ph7_value naming it through the` |
|         - | 7143 | `	 * three value doors, plus one for each NON-value holder (the VM's default` |
|         - | 7144 | `	 * context, a stream that carries one in io_private.pCtxRes). */` |
|         - | 7145 | `	phl_stream_ctx *pNext;` |
|         - | 7146 | `	phl_stream_ctx *pPrev;` |
|         - | 7147 | `};` |
|         - | 7148 | `/* The context behind a ph7_value, or 0 when the value is not one. */` |
|         - | 7149 | `PH7_PRIVATE phl_stream_ctx * PH7_StreamCtxFromValue(ph7_value *pVal);` |
|         - | 7150 | `/* The per-VM DEFAULT context (stream_context_get_default), created on demand. */` |
|         - | 7151 | `PH7_PRIVATE phl_stream_ctx * PH7_StreamCtxDefault(ph7_vm *pVm);` |
|         - | 7152 | `/* One wrapper option, or 0 when the context does not carry it. */` |
|         - | 7153 | `PH7_PRIVATE ph7_value * PH7_StreamCtxOption(phl_stream_ctx *pCtxRes,const char *zWrapper,const char *zOption);` |
|         - | 7154 | `/* Drop every context this VM created (called from PH7_VmReset). */` |
|         - | 7155 | `PH7_PRIVATE void PH7_StreamCtxVmReset(ph7_vm *pVm);` |
|         - | 7156 | ``/* The `$context` argument of an opener, php's rules applied (see the body). */`` |
|         - | 7157 | `PH7_PRIVATE phl_stream_ctx * PH7_StreamCtxFromArg(ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|         - | 7158 | `	int iArg,const char *zArgName,int bNoDefault,int *pbThrew);` |
|         - | 7159 | `/* Arm the context PH7_StreamOpenHandle's next open runs under. */` |
|         - | 7160 | `PH7_PRIVATE void PH7_StreamCtxArm(ph7_vm *pVm,phl_stream_ctx *pRes);` |
|         - | 7161 | ``/* Tell the context's `notification` callback about one event. A NULL zMsg is`` |
|         - | 7162 | ` * php's null third argument; nMsg < 0 means "NUL-terminated". */` |
|         - | 7163 | `PH7_PRIVATE void PH7_StreamCtxNotify(phl_stream_ctx *pCtxRes,int iCode,int iSeverity,` |
|         - | 7164 | `	const char *zMsg,int nMsg,int iMsgCode,sxi64 iBytes,sxi64 iBytesMax);` |
|         - | 7165 | `/* php's progress notifier: arm the counter at 0 with a known maximum (and say` |
|         - | 7166 | ` * so), add to it, or report the end of the transfer. */` |
|         - | 7167 | `PH7_PRIVATE void PH7_StreamCtxProgressInit(phl_stream_ctx *pCtxRes,sxi64 iMax);` |
|         - | 7168 | `PH7_PRIVATE void PH7_StreamCtxProgressAdd(phl_stream_ctx *pCtxRes,sxi64 nDelta);` |
|         - | 7169 | `PH7_PRIVATE void PH7_StreamCtxCompleted(phl_stream_ctx *pCtxRes);` |
|         - | 7170 | `/*` |
|         - | 7171 | ` * ---------------------------------------------------------------------------` |
|         - | 7172 | ` * Stream filters.` |
|         - | 7173 | ` *` |
|         - | 7174 | ` * php runs a stream's bytes through a CHAIN on the way in and another on the` |
|         - | 7175 | ` * way out. A filter is handed a BRIGADE — the buckets that came off the device,` |
|         - | 7176 | ` * or that the script wrote — and appends what it made to a second one; what it` |
|         - | 7177 | ` * ANSWERS says whether that output may go on (PASS_ON), whether it needs more` |
|         - | 7178 | ` * input before it can produce any (FEED_ME), or whether the stream is finished` |
|         - | 7179 | ` * (ERR_FATAL). The brigade rather than one string is what lets a filter split` |
|         - | 7180 | ` * or merge its input, and what a userland filter walks with` |
|         - | 7181 | ` * stream_bucket_make_writeable().` |
|         - | 7182 | ` * ---------------------------------------------------------------------------` |
|         - | 7183 | ` */` |
|         - | 7184 | `/* php's PSFS_* filter results. */` |
|         - | 7185 | `#define PHL_PSFS_ERR_FATAL 0` |
|         - | 7186 | `#define PHL_PSFS_FEED_ME   1` |
|         - | 7187 | `#define PHL_PSFS_PASS_ON   2` |
|         - | 7188 | `/* php's PSFS_FLAG_* — which kind of call this is. FLUSH_CLOSE is the last one a` |
|         - | 7189 | ` * filter ever gets and the only chance a buffering filter has to emit its tail. */` |
|         - | 7190 | `#define PHL_PSFS_FLAG_NORMAL      0` |
|         - | 7191 | `#define PHL_PSFS_FLAG_FLUSH_INC   1` |
|         - | 7192 | `#define PHL_PSFS_FLAG_FLUSH_CLOSE 2` |
|         - | 7193 | `/* php's STREAM_FILTER_* chain selectors. */` |
|         - | 7194 | `#define PHL_STREAM_FILTER_READ  1` |
|         - | 7195 | `#define PHL_STREAM_FILTER_WRITE 2` |
|         - | 7196 | `#define PHL_STREAM_FILTER_ALL   3` |
|         - | 7197 | `/* stream_filter_append()'s handle carries this magic in the io_private-compatible` |
|         - | 7198 | ` * header every PHL resource opens with; php names the resource "stream filter". */` |
|         - | 7199 | `#define STREAM_FILTER_MAGIC 0xF117E4` |
|         - | 7200 | `/* A filter that has been removed from its chain. A stream keeps its own closed` |
|         - | 7201 | ` * magic because the struct outlives the close; a filter needs one of its own so` |
|         - | 7202 | ` * the header can still say WHICH kind it is after the fact -- that is what lets` |
|         - | 7203 | ` * the last ph7_value naming it hand the memory back instead of parking it on` |
|         - | 7204 | ` * the VM registry until reset. Every probe that reports a closed resource` |
|         - | 7205 | ` * treats it exactly like IO_PRIVATE_CLOSED_MAGIC. */` |
|         - | 7206 | `#define STREAM_FILTER_CLOSED_MAGIC 0xF11DEA` |
|         - | 7207 | `typedef struct phl_bucket phl_bucket;` |
|         - | 7208 | `typedef struct phl_brigade phl_brigade;` |
|         - | 7209 | `typedef struct phl_stream_filter phl_stream_filter;` |
|         - | 7210 | `typedef struct phl_filter_ops phl_filter_ops;` |
|         - | 7211 | `/* stream_filter_register()'s two script-visible handles. php names them` |
|         - | 7212 | `` * `userfilter.bucket brigade` and `userfilter.bucket`. */`` |
|         - | 7213 | `#define STREAM_BRIGADE_MAGIC 0xB817AD` |
|         - | 7214 | `#define STREAM_BUCKET_MAGIC  0xB0C4E7` |
|         - | 7215 | `/* One bucket: a run of bytes travelling through a chain. */` |
|         - | 7216 | `struct phl_bucket` |
|         - | 7217 | `{` |
|         - | 7218 | `	SyBlob sData;      /* the bytes */` |
|         - | 7219 | `	phl_bucket *pNext; /* next bucket in the brigade */` |
|         - | 7220 | `};` |
|         - | 7221 | `struct phl_brigade` |
|         - | 7222 | `{` |
|         - | 7223 | `	phl_bucket *pHead,*pTail;` |
|         - | 7224 | `};` |
|         - | 7225 | ``/* The brigade a userland filter() is handed. Both `$in` and `$out` are one of`` |
|         - | 7226 | ` * these. They belong to the FILTER rather than to the call, so a script that` |
|         - | 7227 | ` * held one past the call it was handed in still has something in bounds to look` |
|         - | 7228 | ` * at — what it loses is the brigade behind it, which is cleared on the way out` |
|         - | 7229 | ` * and makes a stale handle answer "empty". */` |
|         - | 7230 | `typedef struct phl_brigade_res phl_brigade_res;` |
|         - | 7231 | `struct phl_brigade_res` |
|         - | 7232 | `{` |
|         - | 7233 | `	io_private base;      /* resource header (base.iMagic == STREAM_BRIGADE_MAGIC) */` |
|         - | 7234 | `	ph7_vm *pVm;` |
|         - | 7235 | `	phl_brigade *pBrig;   /* the brigade it stands for, 0 between calls */` |
|         - | 7236 | `	phl_stream_filter *pOwner; /* the filter these bytes live inside */` |
|         - | 7237 | `};` |
|         - | 7238 | `/* What a built-in filter IS. A userland filter has no ops and runs its class. */` |
|         - | 7239 | `struct phl_filter_ops` |
|         - | 7240 | `{` |
|         - | 7241 | `	const char *zName;  /* php's own registered name */` |
|         - | 7242 | `	/* Read the $params argument, once, when the filter is created. A non-zero` |
|         - | 7243 | `	 * answer is php's "filter refused to be created". */` |
|         - | 7244 | `	int (*xCreate)(phl_stream_filter *pFilter,ph7_value *pParams);` |
|         - | 7245 | `	int (*xFilter)(phl_stream_filter *pFilter,phl_brigade *pIn,phl_brigade *pOut,int iFlags);` |
|         - | 7246 | `	void (*xClose)(phl_stream_filter *pFilter);` |
|         - | 7247 | `};` |
|         - | 7248 | `struct phl_stream_filter` |
|         - | 7249 | `{` |
|         - | 7250 | `	io_private base;            /* resource header (base.iMagic == STREAM_FILTER_MAGIC) */` |
|         - | 7251 | `	ph7_vm *pVm;                /* owning VM */` |
|         - | 7252 | `	const phl_filter_ops *pOps; /* built-in behaviour, or 0 for a userland filter */` |
|         - | 7253 | `	SyBlob sName;               /* the name it was CREATED under (a wildcard match keeps the request) */` |
|         - | 7254 | `	SyBlob sCarry;              /* bytes the filter could not encode yet (base64/qp/dechunk) */` |
|         - | 7255 | `	int iState;                 /* per-filter scalar state */` |
|         - | 7256 | `	sxu8 bClosed;               /* the FLUSH_CLOSE call has already been made */` |
|         - | 7257 | `	sxu8 bDead;                 /* it answered ERR_FATAL: the chain is finished */` |
|         - | 7258 | `	int iChain;                 /* PHL_STREAM_FILTER_READ or _WRITE */` |
|         - | 7259 | `	io_private *pDev;           /* the handle it is attached to; 0 once removed */` |
|         - | 7260 | `	phl_stream_filter *pNext;   /* next filter in that chain */` |
|         - | 7261 | `	phl_stream_filter *pRegNext;/* VM registry chain (pVm->pStreamFilter) */` |
|         - | 7262 | `	void *pPriv;                /* per-filter private state, freed by xClose */` |
|         - | 7263 | `	phl_brigade_res sIn,sOut;   /* the two handles filter() is given */` |
|         - | 7264 | `	void *pObj;                 /* userland filter instance (ph7_class_instance*) */` |
|         - | 7265 | `	ph7_value *pStreamRes;      /* the $stream the userland filter's property answers */` |
|         - | 7266 | `};` |
|         - | 7267 | `/* Brigade plumbing, shared with the userland-filter half. */` |
|         - | 7268 | `PH7_PRIVATE phl_bucket * PH7_FilterBucketNew(ph7_vm *pVm,const void *pData,sxu32 nLen);` |
|         - | 7269 | `PH7_PRIVATE phl_bucket * PH7_FilterBucketPop(phl_brigade *pBrig);` |
|         - | 7270 | `PH7_PRIVATE void PH7_FilterBucketAppend(phl_brigade *pBrig,phl_bucket *pBucket);` |
|         - | 7271 | `PH7_PRIVATE void PH7_FilterBucketFree(ph7_vm *pVm,phl_bucket *pBucket);` |
|         - | 7272 | `PH7_PRIVATE void PH7_FilterBrigadeRelease(ph7_vm *pVm,phl_brigade *pBrig);` |
|         - | 7273 | `/* Run one chain over nLen bytes, appending what came out to pOut. Answers a` |
|         - | 7274 | ` * PHL_PSFS_* code; ERR_FATAL means the stream is finished. */` |
|         - | 7275 | `PH7_PRIVATE int PH7_FilterChainProcess(phl_stream_filter *pHead,` |
|         - | 7276 | `	const void *pData,sxu32 nLen,int iFlags,int iRestFlags,SyBlob *pOut,int *pbUnread);` |
|         - | 7277 | `/* A seek moved the device: a chain that had already been CLOSED at the old end` |
|         - | 7278 | ` * of file has to be able to run again. */` |
|         - | 7279 | `PH7_PRIVATE void PH7_StreamFilterRewound(io_private *pDev);` |
|         - | 7280 | `/* Drop both chains of a handle, flushing the write one while the device is` |
|         - | 7281 | ` * still open (every close path and the io_private reset paths). */` |
|         - | 7282 | `PH7_PRIVATE void PH7_StreamFilterReleaseChains(io_private *pDev);` |
|         - | 7283 | `/* Attach a filter by NAME, php's own failure diagnostics raised from pCtx.` |
|         - | 7284 | ` * Answers the filter, or 0 when there is no such name. */` |
|         - | 7285 | `PH7_PRIVATE phl_stream_filter * PH7_StreamFilterAttach(ph7_vm *pVm,io_private *pDev,` |
|         - | 7286 | `	const char *zName,int nName,int iChain,int bPrepend,ph7_value *pParams,` |
|         - | 7287 | `	ph7_value *pStreamVal);` |
|         - | 7288 | `/* The filter behind a ph7_value, or 0 when the value is not a live one. */` |
|         - | 7289 | `PH7_PRIVATE phl_stream_filter * PH7_StreamFilterFromValue(ph7_value *pVal);` |
|         - | 7290 | `/* Drop every filter this VM created (called from PH7_VmReset). */` |
|         - | 7291 | `PH7_PRIVATE void PH7_StreamFilterVmReset(ph7_vm *pVm);` |
|         - | 7292 | `/* The last ph7_value naming a filter handle -- or one of the two brigade handles` |
|         - | 7293 | ` * that live inside it -- has gone away. Hands the memory back when the chain has` |
|         - | 7294 | ` * let go of it too. */` |
|         - | 7295 | `PH7_PRIVATE void PH7_StreamFilterValueGone(void *pResource);` |
|         - | 7296 | `/* The stream_filter_register()/php_user_filter half. */` |
|         - | 7297 | `PH7_PRIVATE int PH7_builtin_stream_filter_register(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7298 | `PH7_PRIVATE int PH7_builtin_stream_bucket_make_writeable(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7299 | `PH7_PRIVATE int PH7_builtin_stream_bucket_append(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7300 | `PH7_PRIVATE int PH7_builtin_stream_bucket_prepend(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7301 | `PH7_PRIVATE int PH7_builtin_stream_bucket_new(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7302 | `PH7_PRIVATE sxi32 PH7_VmInstallStreamFilter(ph7_vm *pVm);` |
|         - | 7303 | `/* Attach the filters a php://filter URL names to the handle it wrapped. */` |
|         - | 7304 | `PH7_PRIVATE int PH7_StreamFilterParseUrl(ph7_vm *pVm,const char *zSpec,int nSpec,` |
|         - | 7305 | `	io_private *pDev,int iChains);` |
|         - | 7306 | ``/* php's `$stream` screen: a TypeError for a non-resource and for a closed one. */`` |
|         - | 7307 | `PH7_PRIVATE io_private * PH7_StreamHandleArg(ph7_context *pCtx,ph7_value *pArg,int iPos,` |
|         - | 7308 | `	const char *zName,int *pRc);` |
|         - | 7309 | `PH7_PRIVATE void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut);` |
|         - | 7310 | `PH7_PRIVATE void SetIOPrivateOpenedAs(io_private *pDev,const char *zUri,int nUriLen,const char *zMode,int nModeLen);` |
|         - | 7311 | `PH7_PRIVATE const char * PH7_StreamWrapperLabel(ph7_vm *pVm,const ph7_io_stream *pStream);` |
|         - | 7312 | `PH7_PRIVATE void MarkIOPrivateClosed(io_private *pDev);` |
|         - | 7313 | `PH7_PRIVATE void PH7_StreamReleaseUnopened(ph7_context *pCtx,io_private *pDev);` |
|         - | 7314 | `/* One buffered line off a handle, php's php_stream_get_line: the newline is` |
|         - | 7315 | ` * INCLUDED, nMaxLen (0 = no cap) bounds the bytes handed back and the remainder` |
|         - | 7316 | ` * stays buffered. The pointer is into the handle's own working buffer and the` |
|         - | 7317 | ` * next read invalidates it. */` |
|         - | 7318 | `PH7_PRIVATE ph7_int64 StreamReadLine(io_private *pDev,const char **pzData,ph7_int64 nMaxLen);` |
|         - | 7319 | `/* php's feof(): the end flag, but only once the line readers' look-ahead buffer` |
|         - | 7320 | ` * has been drained -- buffered bytes are not an end, and a userland wrapper is` |
|         - | 7321 | ` * asked the question itself. */` |
|         - | 7322 | `PH7_PRIVATE int PH7_StreamAtEof(io_private *pDev);` |
|         - | 7323 | ``/* php's `Read of N bytes failed with errno=...` notice, raised from whichever`` |
|         - | 7324 | ` * builtin or METHOD is asking. A no-op unless the last device read failed. */` |
|         - | 7325 | `PH7_PRIVATE void StreamReportReadFailure(ph7_context *pCtx,io_private *pDev);` |
|         - | 7326 | `/* Why PH7_StreamOpenPath() answered 0. It reports nothing itself: fopen() warns` |
|         - | 7327 | ` * and SplFileObject's constructor throws, which is php's own split. */` |
|         - | 7328 | `#define PH7_STREAM_OPEN_OK       0` |
|         - | 7329 | `#define PH7_STREAM_OPEN_NODEVICE 1 /* no wrapper is registered for the scheme */` |
|         - | 7330 | `#define PH7_STREAM_OPEN_FAILED   2 /* the wrapper refused the name (errno is set) */` |
|         - | 7331 | `#define PH7_STREAM_OPEN_NOMEM    3` |
|         - | 7332 | `#define PH7_STREAM_OPEN_BADMODE  4 /* the plain-file wrapper refused the MODE */` |
|         - | 7333 | `PH7_PRIVATE io_private * PH7_StreamOpenPath(ph7_context *pCtx,ph7_value *pPath,` |
|         - | 7334 | `	const char *zMode,int nMode,int bUseInclude,phl_stream_ctx *pCtxRes,` |
|         - | 7335 | `	ph7_value *pCtxArg,int *piErr,const char **pzErrUri);` |
|         - | 7336 | `/* "Failed to open stream" warning helper (vfs.c, errno-based); used by the` |
|         - | 7337 | ` * fopen/opendir/file_* family in vfs_stream.c. */` |
|         - | 7338 | `PH7_PRIVATE void VfsThrowOpenWarning(ph7_context *pCtx,const char *zFile);` |
|         - | 7339 | `/* strerror() for a failed stream OPEN: php's own substitution of ENOENT for` |
|         - | 7340 | ` * ENOTDIR, which belongs to the open and to no other file operation. */` |
|         - | 7341 | `PH7_PRIVATE const char * PH7_VfsOpenStrerror(int iErr);` |
|         - | 7342 | `PH7_PRIVATE void VfsThrowUnknownWrapperWarning(ph7_context *pCtx,const char *zUri);` |
|         - | 7343 | `PH7_PRIVATE const ph7_io_stream * PH7_VfsStreamDeviceOrFile(ph7_context *pCtx,const char **pzUri,int nByte);` |
|         - | 7344 | `PH7_PRIVATE int PH7_VfsEmptyPathRefused(ph7_context *pCtx,int nPath);` |
|         - | 7345 | `/* The separator php's own path expansion writes on this platform. */` |
|         - | 7346 | `#ifdef __WINNT__` |
|         - | 7347 | `#define PH7_PATH_SEP      '\\'` |
|         - | 7348 | `#define PH7_PATH_SEP_STR  "\\"` |
|         - | 7349 | `#else` |
|         - | 7350 | `#define PH7_PATH_SEP      '/'` |
|         - | 7351 | `#define PH7_PATH_SEP_STR  "/"` |
|         - | 7352 | `#endif` |
|         - | 7353 | `PH7_PRIVATE int PH7_VfsPathIsAbsolute(const char *zPath,int nPath);` |
|         - | 7354 | `PH7_PRIVATE void PH7_VfsExpandPath(ph7_context *pCtx,const char *zPath,int nPath,SyBlob *pOut);` |
|         - | 7355 | `/*` |
|         - | 7356 | ` * One name a directory walk produced, as an OFFSET into the blob that holds` |
|         - | 7357 | ` * them back to back -- the blob grows as the walk does, so a pointer would not` |
|         - | 7358 | ` * survive the next append. It is the shape glob:// keeps its matches in and the` |
|         - | 7359 | ` * shape ext/zip's addGlob()/addPattern() read them back out of.` |
|         - | 7360 | ` */` |
|         - | 7361 | `typedef struct PH7_GlobHit PH7_GlobHit;` |
|         - | 7362 | `struct PH7_GlobHit` |
|         - | 7363 | `{` |
|         - | 7364 | `	sxu32 nOfs;` |
|         - | 7365 | `	sxu32 nLen;` |
|         - | 7366 | `};` |
|         - | 7367 | ``/* Every entry of ONE directory, `.` and `..` included and sorted by bytes --`` |
|         - | 7368 | `` * php's `php_stream_scandir` with its alphasort comparator, which is what`` |
|         - | 7369 | ` * ZipArchive::addPattern() walks. */` |
|         - | 7370 | `PH7_PRIVATE sxi32 PH7_VfsListDir(ph7_vm *pVm,const char *zDir,int nDir,SyBlob *pHit,SySet *pSet);` |
|         - | 7371 | `/* A wrapper's own reason for refusing the open in flight, which is what php` |
|         - | 7372 | ` * prints after "Failed to open stream:" instead of an errno. Set from an xOpen` |
|         - | 7373 | ` * body; PH7_StreamOpenHandle() re-arms the default before every open. */` |
|         - | 7374 | `PH7_PRIVATE void PH7_StreamSetOpenError(ph7_vm *pVm,const char *zReason);` |
|         - | 7375 | ``/* The same, for php's `"Cls::method" call failed`: a userland wrapper's refusal`` |
|         - | 7376 | ` * names the call that made it, so the sentence is built and copied. */` |
|         - | 7377 | `PH7_PRIVATE void PH7_StreamSetOpenErrorCall(ph7_vm *pVm,const char *zClass,const char *zMethod);` |
|         - | 7378 | `PH7_PRIVATE void VfsThrowNoDeviceWarning(ph7_context *pCtx,const char *zUri,int bDir);` |
|         - | 7379 | `PH7_PRIVATE int PH7_VfsStatDoubleUp(ph7_value *pIn,ph7_value *pOut);` |
|         - | 7380 | `PH7_PRIVATE int PH7_VfsStatFill(ph7_value *pArray,ph7_value *pWorker,const ph7_int64 *aVal);` |
|         - | 7381 | `PH7_PRIVATE int PH7_VfsStatFromFd(int iFd,ph7_value *pArray,ph7_value *pWorker);` |
|         - | 7382 | `PH7_PRIVATE const char * VfsStrerror(int iErr);` |
|         - | 7383 | `/* Stream-device predicates (vfs_io_driver.c) */` |
|         - | 7384 | `PH7_PRIVATE int is_php_stream(const ph7_io_stream *pStream);` |
|         - | 7385 | `PH7_PRIVATE int is_data_stream(const ph7_io_stream *pStream);` |
|         - | 7386 | `/* Which php:// sub-stream a handle opened (PH7_IO_STREAM_*, 0 when unknown).` |
|         - | 7387 | ` * stream_get_meta_data() names MEMORY, TEMP and STDIO apart, and the device` |
|         - | 7388 | ` * itself is the only place that knows. */` |
|         - | 7389 | `PH7_PRIVATE int PH7_PhpStreamKind(void *pHandle);` |
|         - | 7390 | `/* Has a php://temp handle handed out its last byte? php's temp stream copies its` |
|         - | 7391 | ` * inner memory stream's eof, so it reports the end one read EARLIER than a bare` |
|         - | 7392 | ` * php://memory; 0 for every other device. */` |
|         - | 7393 | `PH7_PRIVATE int PH7_PhpStreamTempDrained(void *pHandle);` |
|         - | 7394 | `/* The handle a php://filter proxy wraps, or 0 for any other php:// stream. */` |
|         - | 7395 | `PH7_PRIVATE io_private * PH7_PhpStreamInner(void *pHandle);` |
|         - | 7396 | `/* That handle, or pDev itself when it is not a proxy. */` |
|         - | 7397 | `PH7_PRIVATE io_private * PH7_StreamUnwrap(io_private *pDev);` |
|         - | 7398 | `/* Where the SCRIPT is on a handle: the device position less what was read ahead. */` |
|         - | 7399 | `PH7_PRIVATE ph7_int64 PH7_StreamLogicalTell(io_private *pDev);` |
|         - | 7400 | `/* Seek the stream a php://filter proxy wraps, in the same model. */` |
|         - | 7401 | `PH7_PRIVATE int PH7_StreamSeekWrapped(io_private *pDev,ph7_int64 iOfft,int whence);` |
|         - | 7402 | `/* The POSIX descriptor behind an open handle, or -1 for a device that has none` |
|         - | 7403 | ` * (a memory buffer, a data:// payload, a userland wrapper) and on Windows,` |
|         - | 7404 | ` * where the file devices carry a HANDLE instead. Only the settings php applies` |
|         - | 7405 | ` * AT the descriptor need it. */` |
|         - | 7406 | `PH7_PRIVATE int PH7_StreamPosixFd(io_private *pDev);` |
|         - | 7407 | `/* Is this a handle php's plain-files device would own: a file, a pipe, a` |
|         - | 7408 | ` * standard stream? */` |
|         - | 7409 | `PH7_PRIVATE int PH7_StreamIsPlainDevice(io_private *pDev);` |
|         - | 7410 | ``/* The `stream_type` label a handle reports (STDIO / MEMORY / TEMP / Input /`` |
|         - | 7411 | ` * RFC2397 / dir / user-space / a transport's), which is what ext/posix names in` |
|         - | 7412 | ` * php's "Could not use stream of type '%s'". */` |
|         - | 7413 | `PH7_PRIVATE const char * PH7_StreamTypeLabel(io_private *pDev);` |
|         - | 7414 | `/* 1 / 0 / -1 ("ask the device instead"): can this handle report a position?` |
|         - | 7415 | `` * php's stream_get_meta_data() `seekable` is exactly this question. */`` |
|         - | 7416 | `PH7_PRIVATE int PH7_StreamHandleCanSeek(io_private *pDev);` |
|         - | 7417 | `/* The php:// sub-streams, as PH7_PhpStreamKind() reports them. */` |
|         - | 7418 | `#define PH7_IO_STREAM_STDIN  1 /* php://stdin */` |
|         - | 7419 | `#define PH7_IO_STREAM_STDOUT 2 /* php://stdout */` |
|         - | 7420 | `#define PH7_IO_STREAM_STDERR 3 /* php://stderr */` |
|         - | 7421 | `#define PH7_IO_STREAM_OUTPUT 4 /* php://output */` |
|         - | 7422 | `#define PH7_IO_STREAM_MEMORY 5 /* php://memory, php://temp, and data:// payloads */` |
|         - | 7423 | `#define PH7_IO_STREAM_FILTER 6 /* php://filter/…/resource=… — a stream wrapped around another */` |
|         - | 7424 | `/* php://input — the REQUEST BODY. There is none under a command line, and php's` |
|         - | 7425 | ` * CLI answers an empty stream for it rather than reading standard input (the` |
|         - | 7426 | `` * body a `php x.php < file` supplies arrives through STDIN, and php://input`` |
|         - | 7427 | ` * stays ""). It runs on the memory machinery, so it is seekable and can be read` |
|         - | 7428 | ` * twice, and it differs from php://memory in exactly four answers: fflush() is` |
|         - | 7429 | ` * FALSE, fstat() is FALSE, ftruncate() is unsupported, and its metadata names` |
|         - | 7430 | `` * it `Input`. */`` |
|         - | 7431 | `#define PH7_IO_STREAM_INPUT  7` |
|         - | 7432 | `/*` |
|         - | 7433 | ` * How far php's directory stream advances per entry read: one` |
|         - | 7434 | ``  * `php_stream_dirent`, which is `char d_name[MAXPATHLEN]` plus the `d_type` `` |
|         - | 7435 | ` * byte. php's MAXPATHLEN is PATH_MAX where the platform has one and a flat 2048` |
|         - | 7436 | ` * on Windows; both numbers were read back from the two php builds rather than` |
|         - | 7437 | `` * assumed (`readdir($d); ftell($d)` answers 4097 here and 2049 there). It is the`` |
|         - | 7438 | ` * only thing ftell() on a directory handle reports, and fseek()/rewind() rewind` |
|         - | 7439 | ` * the directory WITHOUT putting it back.` |
|         - | 7440 | ` */` |
|         - | 7441 | `#ifndef __WINNT__` |
|         - | 7442 | `#include <limits.h>    /* PATH_MAX, which is where php's MAXPATHLEN comes from */` |
|         - | 7443 | `#endif` |
|         - | 7444 | `#ifdef __WINNT__` |
|         - | 7445 | `#define PHL_DIR_RECORD 2049            /* php's win32 MAXPATHLEN is 2048 */` |
|         - | 7446 | `#elif defined(PATH_MAX)` |
|         - | 7447 | `#define PHL_DIR_RECORD (PATH_MAX + 1)  /* php takes MAXPATHLEN from PATH_MAX */` |
|         - | 7448 | `#else` |
|         - | 7449 | `#define PHL_DIR_RECORD 4097` |
|         - | 7450 | `#endif` |
|         - | 7451 | `PH7_PRIVATE int PH7_Utf8Read(` |
|         - | 7452 | `  const unsigned char *z,         /* First byte of UTF-8 character */` |
|         - | 7453 | `  const unsigned char *zTerm,     /* Pretend this byte is 0x00 */` |
|         - | 7454 | `  const unsigned char **pzNext    /* Write first byte past UTF-8 char here */` |
|         - | 7455 | `);` |
|         - | 7456 | `PH7_PRIVATE sxi32 PH7_Utf8ReadStrict(const unsigned char *z,sxu32 n,sxu32 *pLen);` |
|         - | 7457 | `/* parse.c function prototypes */` |
|         - | 7458 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc);` |
|         - | 7459 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot);` |
|         - | 7460 | `PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode);` |
|         - | 7461 | `PH7_PRIVATE int PH7_ExprNodeIsThis(ph7_expr_node *pNode);` |
|         - | 7462 | `PH7_PRIVATE int PH7_ExprNodeIsClassConst(ph7_expr_node *pNode);` |
|         - | 7463 | `PH7_PRIVATE int PH7_ExprIsModifiableValue(ph7_expr_node *pNode);` |
|         - | 7464 | `PH7_PRIVATE SyToken * PH7_ExprTokenInStream(ph7_gen_state *pGen,SyToken *pTok);` |
|         - | 7465 | `PH7_PRIVATE sxi32 PH7_ExprOperandNotAVariable(ph7_gen_state *pGen,ph7_expr_node *pOperand);` |
|         - | 7466 | `/* OP_STORE_REF / OP_STORE_IDX_REF iP1 bit 1: the reference SOURCE was written as a` |
|         - | 7467 | ` * CALL. php's compiler records the same thing (ZEND_RETURNS_FUNCTION) so the bind` |
|         - | 7468 | `` * can raise `Only variables should be assigned by reference` when the callee did`` |
|         - | 7469 | ` * not return by reference. Bit 0 stays STORE_IDX_REF's "a key is on the stack". */` |
|         - | 7470 | `#define PH7_STOREREF_CALLSRC 0x02` |
|         - | 7471 | `/* Context bits for GenStateWriteTargetCheck — php's write-target rules are the` |
|         - | 7472 | ` * same everywhere except for these two distinctions. */` |
|         - | 7473 | ``#define PH7_WTC_UNSET   0x01 /* `unset()`: the $this refusal takes its own wording */`` |
|         - | 7474 | ``#define PH7_WTC_REFSRC  0x02 /* the SOURCE of `=&`: php compiles it in write context`` |
|         - | 7475 | `                              * (so a temporary base is still refused) but never runs` |
|         - | 7476 | `                              * zend_ensure_writable_variable over it, which is why` |
|         - | 7477 | ``                              * `$r =& f()` is legal where `f() =& $x` is not */`` |
|         - | 7478 | ``#define PH7_WTC_RMW     0x04 /* a READ-MODIFY-WRITE target -- `+=`, `.=`, `++`, `--`.`` |
|         - | 7479 | ``                              * php's `$this` rule belongs to the ASSIGNMENT compiler`` |
|         - | 7480 | ``                              * (zend_compile_assign / assign_ref), so `$this += 1` and`` |
|         - | 7481 | ``                              * `$this++` compile and fail at RUN time on the operand`` |
|         - | 7482 | `                              * types instead. The temporary and call rules still apply:` |
|         - | 7483 | ``                              * `(new A)->p++` is refused exactly as `= 1` is. */`` |
|         - | 7484 | ``#define PH7_WTC_THISSRC 0x08 /* an ARRAY LITERAL's `&$x` element. php compiles it in`` |
|         - | 7485 | ``                              * write context -- `[&f()]` is its "Can't use function`` |
|         - | 7486 | `                              * return value in write context" -- but the element only` |
|         - | 7487 | `                              * takes a REFERENCE to the slot, it never re-points it, so` |
|         - | 7488 | ``                              * `$this` is legal there and writing through the element`` |
|         - | 7489 | ``                              * leaves the receiver alone. `[&$this, 'cmp']` is how the`` |
|         - | 7490 | `                              * pre-5.4 callable idiom is spelled and phpseclib's SFTP` |
|         - | 7491 | `                              * still writes it. */` |
|         - | 7492 | `PH7_PRIVATE sxi32 GenStateWriteTargetCheck(ph7_gen_state *pGen,ph7_expr_node *pTarget,int iCtx);` |
|         - | 7493 | `PH7_PRIVATE void PH7_ExprSubtreeSpan(ph7_expr_node *pNode,SyToken **ppMin,SyToken **ppMax);` |
|         - | 7494 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext);` |
|         - | 7495 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd);` |
|         - | 7496 | ``/* TRUE when a KEYWORD token opens `[static] fn[&](…) =>` rather than naming a`` |
|         - | 7497 | ` * variable/member/label ($fn, $o->fn, C::fn, \A\fn, f(fn: 1)). Every raw-token` |
|         - | 7498 | ` * lookahead that has to step over an arrow function must ask this first; the` |
|         - | 7499 | `` * test is positional, so a MALFORMED `fn` still reaches the arrow parser and`` |
|         - | 7500 | `` * keeps php's `expecting "("`. */`` |
|         - | 7501 | `PH7_PRIVATE int PH7_TokenOpensArrowFunc(SyToken *pStart,SyToken *pTok,SyToken *pEnd);` |
|         - | 7502 | `PH7_PRIVATE const ph7_expr_op * PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast);` |
|         - | 7503 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet);` |
|         - | 7504 | `/* compile.c function prototypes */` |
|         - | 7505 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType);` |
|         - | 7506 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 7507 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 7508 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 7509 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 7510 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 7511 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 7512 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 7513 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 7514 | `PH7_PRIVATE sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 7515 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 7516 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 7517 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 7518 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 7519 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 7520 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 7521 | `PH7_PRIVATE int GenStateIsReadonly(SyToken *pTok);` |
|         - | 7522 | `PH7_PRIVATE int GenStateStartsReadonlyAnonClass(SyToken *pIn,SyToken *pEnd);` |
|         - | 7523 | `PH7_PRIVATE int GenStateStartsClosureExpr(SyToken *pIn,SyToken *pEnd);` |
|         - | 7524 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 7525 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 7526 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 7527 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(ph7_vm *pVm,ProcConsumer xErr,void *pErrData);` |
|         - | 7528 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(ph7_vm *pVm,ProcConsumer xErr,void *pErrData);` |
|         - | 7529 | `PH7_PRIVATE void PH7_CompilerSaveState(ph7_vm *pVm,ph7_gen_state *pSaved,ProcConsumer xErr,void *pErrData);` |
|         - | 7530 | `PH7_PRIVATE void PH7_CompilerRestoreState(ph7_vm *pVm,ph7_gen_state *pSaved);` |
|         - | 7531 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...);` |
|         - | 7532 | `PH7_PRIVATE sxi32 PH7_GenSyntaxError(ph7_gen_state *pGen,SyToken *pTok,const char *zExpecting);` |
|         - | 7533 | `PH7_PRIVATE sxi32 PH7_GenUnmatchedCloser(ph7_gen_state *pGen,SyToken *pTok);` |
|         - | 7534 | `PH7_PRIVATE const char *PH7_GenStrayStatementTail(ph7_gen_state *pGen);` |
|         - | 7535 | `PH7_PRIVATE void PH7_GenCarryBraces(ph7_gen_state *pGen);` |
|         - | 7536 | `PH7_PRIVATE sxi32 PH7_CompileScript(ph7_vm *pVm,SyString *pScript,sxi32 iFlags);` |
|         - | 7537 | `/* constant.c function prototypes */` |
|         - | 7538 | `PH7_PRIVATE void PH7_RegisterBuiltInConstant(ph7_vm *pVm);` |
|         - | 7539 | `PH7_PRIVATE void PH7_MarkDeprecatedConstants(ph7_vm *pVm);` |
|         - | 7540 | `/* vm.c reference/frame internals shared with vm_builtin_var.c */` |
|         - | 7541 | `/* vm_gc.c -- the cycle collector */` |
|         - | 7542 | `PH7_PRIVATE void PH7_GcInit(ph7_vm *pVm);` |
|         - | 7543 | `PH7_PRIVATE void PH7_GcResetBuffer(ph7_vm *pVm);` |
|         - | 7544 | `PH7_PRIVATE void PH7_GcRelease(ph7_vm *pVm);` |
|         - | 7545 | `PH7_PRIVATE void PH7_GcPossibleRoot(ph7_vm *pVm,void *pCont,int bMap);` |
|         - | 7546 | `PH7_PRIVATE void PH7_GcForget(ph7_vm *pVm,void *pCont,int bMap);` |
|         - | 7547 | `PH7_PRIVATE sxu32 PH7_GcCollect(ph7_vm *pVm);` |
|         - | 7548 | `/* vm.c -- lifetime of a run-time closure's per-instantiation ph7_vm_func */` |
|         - | 7549 | `PH7_PRIVATE ph7_vm_func * PH7_VmRuntimeClosure(ph7_vm *pVm,const char *zName,sxu32 nByte);` |
|         - | 7550 | `PH7_PRIVATE void PH7_VmClosureFuncRef(ph7_vm_func *pFunc);` |
|         - | 7551 | `PH7_PRIVATE void PH7_VmClosureFuncUnref(ph7_vm *pVm,ph7_vm_func *pFunc);` |
|         - | 7552 | `PH7_PRIVATE void PH7_VmClosureInstanceRef(ph7_vm *pVm,ph7_class_instance *pObj,int iDelta);` |
|         - | 7553 | `PH7_PRIVATE void PH7_VmPurgeDeadClosures(ph7_vm *pVm);` |
|         - | 7554 | `PH7_PRIVATE SyHashEntry * PH7_VmSuperGet(ph7_vm *pVm,const char *zName,sxu32 nByte);` |
|         - | 7555 | `PH7_PRIVATE void PH7_VmSuperNote(ph7_vm *pVm,const char *zName,sxu32 nByte);` |
|         - | 7556 | `/* The reference table asks its questions BY SLOT: a slot that answers from its word` |
|         - | 7557 | ` * has no record for a caller to hold, so there is no VmRefObjExtract any more. */` |
|         - | 7558 | `PH7_PRIVATE int PH7_VmSlotDropIfBare(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 7559 | `PH7_PRIVATE sxu32 PH7_VmSlotEntryCount(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 7560 | `PH7_PRIVATE sxu32 PH7_VmSlotNodeCount(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 7561 | `PH7_PRIVATE sxu32 PH7_VmSlotPinCount(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 7562 | `PH7_PRIVATE int PH7_VmSlotKeepPinned(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 7563 | `PH7_PRIVATE int PH7_VmSlotRegistered(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 7564 | `PH7_PRIVATE int PH7_VmSlotSoleNodeIs(ph7_vm *pVm,sxu32 nIdx,ph7_hashmap_node *pNode);` |
|         - | 7565 | `PH7_PRIVATE void PH7_VmSlotUnlink(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 7566 | `PH7_PRIVATE int VmDropFrameLocalSlot(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 7567 | `PH7_PRIVATE void VmDropFrameRefEntry(ph7_vm *pVm,sxu32 nIdx,SyHashEntry *pEntry);` |
|         - | 7568 | `PH7_PRIVATE sxu32 PH7_VmSlotHolderCount(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 7569 | `PH7_PRIVATE int PH7_VmSlotSelfPinned(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 7570 | `PH7_PRIVATE int PH7_VmSlotDropOwnerHold(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 7571 | `PH7_PRIVATE void PH7_VmReleaseUnheldSlot(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 7572 | `PH7_PRIVATE void PH7_VmRebindVarSlot(ph7_vm *pVm,VmFrame *pFrame,SyHashEntry *pEntry,` |
|         - | 7573 | `	const char *zName,sxu32 nByte,sxu32 nIdx);` |
|         - | 7574 | `PH7_PRIVATE void PH7_VmBindVarSlot(ph7_vm *pVm,VmFrame *pFrame,const char *zName,sxu32 nByte,` |
|         - | 7575 | `	sxu32 nIdx);` |
|         - | 7576 | `PH7_PRIVATE int PH7_VmVarNameIsInternal(const char *zName,sxu32 nByte);` |
|         - | 7577 | `/* vm_builtin_var.c function prototypes (rows stay in vm.c's aVmFunc[]) */` |
|         - | 7578 | `PH7_PRIVATE sxi32 VmUnsetVarByName(ph7_vm *pVm,VmFrame *pFrame,const char *zName,sxu32 nByte);` |
|         - | 7579 | `PH7_PRIVATE sxi32 VmUnsetVarByNameEx(ph7_vm *pVm,VmFrame *pFrame,const char *zName,sxu32 nByte,int bNameGuard);` |
|         - | 7580 | `PH7_PRIVATE int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7581 | `PH7_PRIVATE int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7582 | `PH7_PRIVATE int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7583 | `PH7_PRIVATE int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7584 | `PH7_PRIVATE int vm_builtin_get_debug_type(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7585 | `PH7_PRIVATE int vm_builtin_settype(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7586 | `PH7_PRIVATE int vm_builtin_get_resource_id(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7587 | `PH7_PRIVATE int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7588 | `PH7_PRIVATE int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7589 | `PH7_PRIVATE int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7590 | `PH7_PRIVATE int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7591 | `/* vm_builtin_lang.c function prototypes (rows stay in vm.c's aVmFunc[]) */` |
|         - | 7592 | `PH7_PRIVATE sxi32 VmClassConstEvalOnDemand(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr);` |
|         - | 7593 | `PH7_PRIVATE void VmDeprecatedConstNotice(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pMember);` |
|         - | 7594 | `PH7_PRIVATE void VmNoDiscardWarn(ph7_vm *pVm,ph7_vm_func *pFunc,ph7_class *pDeclClass);` |
|         - | 7595 | `PH7_PRIVATE ph7_value * VmEnumCaseBackingValue(ph7_vm *pVm,ph7_class_attr *pCase);` |
|         - | 7596 | `PH7_PRIVATE sxi32 VmEnumMaterialize(ph7_vm *pVm,ph7_class *pClass);` |
|         - | 7597 | `PH7_PRIVATE void VmExpandConstantWithNotice(ph7_vm *pVm,ph7_constant *pCons,ph7_value *pOut);` |
|         - | 7598 | `PH7_PRIVATE sxi32 VmExpandConstantOnce(ph7_vm *pVm,ph7_constant *pCons,ph7_value *pOut);` |
|         - | 7599 | `PH7_PRIVATE SyHashEntry * PH7_VmConstantFetch(ph7_vm *pVm,const char *zName,sxu32 nName,int bStripLead);` |
|         - | 7600 | `PH7_PRIVATE void VmTrackOutput(ph7_vm *pVm, sxu32 nLen);` |
|         - | 7601 | `PH7_PRIVATE int PH7_VmOutputOrigin(ph7_vm *pVm,SyString *pFile,sxu32 *pnLine);` |
|         - | 7602 | `PH7_PRIVATE int PH7_VmSessionOrigin(ph7_vm *pVm,SyString *pFile,sxu32 *pnLine);` |
|         - | 7603 | `PH7_PRIVATE void PH7_VmSetSessionOrigin(ph7_vm *pVm);` |
|         - | 7604 | `PH7_PRIVATE void PH7_VmAppendWhere(ph7_vm *pVm,SyBlob *pMsg,int bSessionActive);` |
|         - | 7605 | `PH7_PRIVATE ph7_value * VmExtractMemObj(ph7_vm *pVm,const SyString *pName,int bDup,int bCreate);` |
|         - | 7606 | `PH7_PRIVATE ph7_value * PH7_VmExtractVarSlot(ph7_vm *pVm,const SyString *pName,int bCreate,sxu32 nSlot,const VmInstr *aCode);` |
|         - | 7607 | `/* Is a store to this slot filtered at all? The screen in front of every hTypedSlot` |
|         - | 7608 | ` * lookup on a hot path; a false answer is final, a true one still asks the table.` |
|         - | 7609 | ` * With the bitmap disabled it degrades to the emptiness test every one of those` |
|         - | 7610 | ` * call sites used before it existed, which over-answers and never under-answers. */` |
|         - | 7611 | `#define PH7_VM_STORE_FILTERED(pVm,nIdx) \` |
|         - | 7612 | `	((pVm)->bFilterBitsOff \` |
|         - | 7613 | `	 ? SyHashTotalEntry(&(pVm)->hTypedSlot) > 0 \` |
|         - | 7614 | `	 : ((nIdx) < (pVm)->nFilterBits \` |
|         - | 7615 | `	    && ((pVm)->pFilterBits[(nIdx) >> 3] & (1 << ((nIdx) & 7))) != 0))` |
|         - | 7616 | `PH7_PRIVATE void VmVarMemoFlush(VmFrame *pFrame);` |
|         - | 7617 | `/* D1 commit 2: deferred-lvalue-path capture (built by the LOAD_IDX/MEMBER record modes) */` |
|         - | 7618 | `PH7_PRIVATE VmDeferredPath * VmDeferPathNew(ph7_vm *pVm,int eRoot,sxu32 nRootIdx,const SyString *pName);` |
|         - | 7619 | `PH7_PRIVATE VmDeferredPath * VmDeferPathNewPrefetch(ph7_vm *pVm,int nKind,ph7_class *pClass,` |
|         - | 7620 | `	const SyString *pName,ph7_value *pVal);` |
|         - | 7621 | `PH7_PRIVATE sxi32 VmDeferPathPushElem(VmDeferredPath *pPath,ph7_value *pKey);` |
|         - | 7622 | `PH7_PRIVATE sxi32 VmDeferPathPushAppend(VmDeferredPath *pPath);` |
|         - | 7623 | `PH7_PRIVATE sxi32 VmDeferPathPushProp(VmDeferredPath *pPath,const SyString *pName);` |
|         - | 7624 | `PH7_PRIVATE void VmFreeDeferredPath(VmDeferredPath *pPath);` |
|         - | 7625 | `PH7_PRIVATE VmCoalStrOff * VmCoalStrOffNew(ph7_vm *pVm,ph7_value *pKey);` |
|         - | 7626 | `PH7_PRIVATE void VmFreeCoalStrOff(VmCoalStrOff *pCoal);` |
|         - | 7627 | `PH7_PRIVATE VmMagicCall * VmMagicCallNew(ph7_vm *pVm,ph7_class_instance *pRecv,` |
|         - | 7628 | `	ph7_class *pClass,const SyString *pName);` |
|         - | 7629 | `PH7_PRIVATE void VmFreeMagicCall(VmMagicCall *pPend);` |
|         - | 7630 | `PH7_PRIVATE void VmExpandUserConstant(ph7_value *pVal,void *pUserData);` |
|         - | 7631 | `PH7_PRIVATE int PH7_VmConstantNameTaken(ph7_vm *pVm,const char *zName,sxu32 nLen);` |
|         - | 7632 | `PH7_PRIVATE ph7_class * VmExtractEnumClass(ph7_vm *pVm,ph7_value *pName);` |
|         - | 7633 | `PH7_PRIVATE int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7634 | `PH7_PRIVATE int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7635 | `PH7_PRIVATE int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7636 | `PH7_PRIVATE int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7637 | `PH7_PRIVATE int vm_builtin_enum_cases(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7638 | `PH7_PRIVATE int vm_builtin_enum_exists(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7639 | `PH7_PRIVATE int vm_builtin_enum_from(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7640 | `PH7_PRIVATE int vm_builtin_enum_tryfrom(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7641 | `PH7_PRIVATE int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7642 | `PH7_PRIVATE int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7643 | `PH7_PRIVATE int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7644 | `PH7_PRIVATE int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7645 | `PH7_PRIVATE int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7646 | `PH7_PRIVATE int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7647 | `PH7_PRIVATE int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7648 | `PH7_PRIVATE int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7649 | `PH7_PRIVATE int vm_builtin_php_sapi_name(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7650 | `PH7_PRIVATE int vm_builtin_php_ini_loaded_file(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7651 | `PH7_PRIVATE int vm_builtin_php_ini_scanned_files(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7652 | `PH7_PRIVATE int vm_builtin_phpversion(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7653 | `PH7_PRIVATE int vm_builtin_extension_loaded(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7654 | `PH7_PRIVATE int vm_builtin_get_loaded_extensions(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7655 | `/* vm_extension.c -- the extension partition every internal name is placed in */` |
|         - | 7656 | `PH7_PRIVATE const char * PH7_VmExtensionName(int iExt);` |
|         - | 7657 | `PH7_PRIVATE int PH7_VmExtensionLookup(const char *zName,int nName);` |
|         - | 7658 | `PH7_PRIVATE int PH7_VmExtensionIsLoaded(ph7_vm *pVm,const char *zName,int nName);` |
|         - | 7659 | `PH7_PRIVATE int PH7_VmExtOfFunc(const char *zName,int nName);` |
|         - | 7660 | `PH7_PRIVATE int PH7_VmExtOfClass(const char *zName,int nName);` |
|         - | 7661 | `PH7_PRIVATE int PH7_VmExtOfConstant(const char *zName,int nName);` |
|         - | 7662 | `PH7_PRIVATE int PH7_VmExtOfIni(const char *zName,int nName);` |
|         - | 7663 | `#define PH7_EXT_KIND_FUNC   0` |
|         - | 7664 | `#define PH7_EXT_KIND_CLASS  1` |
|         - | 7665 | `#define PH7_EXT_KIND_CONST  2` |
|         - | 7666 | `#define PH7_EXT_KIND_INI    3` |
|         - | 7667 | `PH7_PRIVATE int PH7_VmExtWalk(int iExt,int iKind,int (*xVisit)(const char *,int,void *),void *pData);` |
|         - | 7668 | `PH7_PRIVATE int PH7_VmInternalNameExists(ph7_vm *pVm,int iKind,const char *zName,int nName);` |
|         - | 7669 | `PH7_PRIVATE int PH7_VmExtWalkDep(int iExt,int (*xVisit)(const char *,const char *,void *),void *pData);` |
|         - | 7670 | `#define PH7_EXT_CORE 0        /* the engine itself; every other id is vm_extension_names.h's */` |
|         - | 7671 | `#define PH7_EXT_MAX  64        /* a caller's per-extension scratch bound; the table is far under it */` |
|         - | 7672 | `PH7_PRIVATE int PH7_VmExtHasName(int iKind,const char *zName,int nName);` |
|         - | 7673 | `PH7_PRIVATE int PH7_VmExtensionCount(void);` |
|         - | 7674 | `PH7_PRIVATE int PH7_VmExtensionAvailable(int iExt);` |
|         - | 7675 | `PH7_PRIVATE int vm_builtin_get_extension_funcs(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7676 | `PH7_PRIVATE int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7677 | `PH7_PRIVATE int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7678 | `PH7_PRIVATE int vm_builtin_random_bytes(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7679 | `PH7_PRIVATE int vm_builtin_random_int(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7680 | `PH7_PRIVATE int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7681 | `PH7_PRIVATE int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7682 | `/* vm.c frame/backtrace internals shared with vm_builtin_error.c */` |
|         - | 7683 | `PH7_PRIVATE VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|         - | 7684 | `PH7_PRIVATE void VmBuildBacktrace(ph7_vm *pVm,sxi32 iOptions,sxi32 iLimit,ph7_value *pList);` |
|         - | 7685 | `PH7_PRIVATE void PH7_GenAppendFatalTrace(ph7_vm *pVm,SyBlob *pOut,int iTraceKind);` |
|         - | 7686 | `PH7_PRIVATE void PH7_VmTraceToString(ph7_vm *pVm,ph7_value *pTrace,int bMainMarker,SyBlob *pOut);` |
|         - | 7687 | `PH7_PRIVATE int PH7_VmAppendTraceScalar(ph7_vm *pVm,SyBlob *pOut,ph7_value *pArg,sxi64 nMax);` |
|         - | 7688 | `PH7_PRIVATE void PH7_VmFrameActualArgs(ph7_vm *pVm,VmFrame *pFrame,ph7_value *pArray,int bNamedExtras);` |
|         - | 7689 | `/* vm_builtin_error.c function prototypes (rows stay in vm.c's aVmFunc[]) */` |
|         - | 7690 | `PH7_PRIVATE int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7691 | `PH7_PRIVATE int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7692 | `PH7_PRIVATE int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7693 | `PH7_PRIVATE int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7694 | `PH7_PRIVATE int vm_builtin_error_clear_last(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7695 | `PH7_PRIVATE int vm_builtin_error_get_last(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7696 | `PH7_PRIVATE int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7697 | `PH7_PRIVATE int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7698 | `PH7_PRIVATE int vm_builtin_get_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7699 | `PH7_PRIVATE int vm_builtin_get_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7700 | `PH7_PRIVATE int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7701 | `PH7_PRIVATE int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7702 | `PH7_PRIVATE int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7703 | `PH7_PRIVATE int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7704 | `PH7_PRIVATE int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7705 | `/* Autoload-callback record (spl_autoload_register in vm_include.c; walked by` |
|         - | 7706 | ` * VmTriggerAutoload in vm.c) */` |
|         - | 7707 | `typedef struct VmAutoloadCB VmAutoloadCB;` |
|         - | 7708 | `struct VmAutoloadCB` |
|         - | 7709 | `{` |
|         - | 7710 | `	ph7_value sCallback; /* Autoload callback (string or [obj,method] array) */` |
|         - | 7711 | `	ph7_value sInvoke;   /* Its registration-scope closure, or NULL (PH7_VmBindCallbackScope) */` |
|         - | 7712 | `};` |
|         - | 7713 | `/* Shutdown-callback record (register_shutdown_function in vm_builtin_call.c;` |
|         - | 7714 | ` * invoked by VmInvokeShutdownCallbacks in vm.c) */` |
|         - | 7715 | `typedef struct VmShutdownCB VmShutdownCB;` |
|         - | 7716 | `struct VmShutdownCB` |
|         - | 7717 | `{` |
|         - | 7718 | `	ph7_value sCallback; /* Shutdown callback */` |
|         - | 7719 | `	ph7_value sInvoke;   /* Its registration-scope closure, or NULL (PH7_VmBindCallbackScope) */` |
|         - | 7720 | `	ph7_value aArg[10];   /* Callback arguments (10 maximum arguments) */` |
|         - | 7721 | `	int nArg;             /* Total number of given arguments */` |
|         - | 7722 | `};` |
|         - | 7723 | `/* Operand-stack guard slack (vm.c allocator; checked by the call machinery) */` |
|         - | 7724 | `#define VM_STACK_GUARD 16` |
|         - | 7725 | `/* vm.c closure/exception internals shared with vm_builtin_call.c */` |
|         - | 7726 | `PH7_PRIVATE int VmValueIsClosure(ph7_vm *pVm,ph7_value *pVal);` |
|         - | 7727 | `PH7_PRIVATE int PH7_VmFuncDisplayName(ph7_vm *pVm,ph7_vm_func *pFunc,const char **pzOut);` |
|         - | 7728 | `PH7_PRIVATE sxi32 VmClosureUnwrap(ph7_vm *pVm,ph7_value *pVal,ph7_value *pOut);` |
|         - | 7729 | `PH7_PRIVATE sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|         - | 7730 | `PH7_PRIVATE sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|         - | 7731 | `PH7_PRIVATE ph7_value * VmNewOperandStack(ph7_vm *pVm,sxu32 nInstr);` |
|         - | 7732 | `/* Fiber/generator trampoline state (BYTECODE stages 2-4); shared between` |
|         - | 7733 | ` * vm.c's interpreter and vm_exec_ctx.c's park/resume machinery. */` |
|         - | 7734 | `/*` |
|         - | 7735 | ` * Boundary state of one VmByteCodeExec activation:` |
|         - | 7736 | ` * everything the executor must restore to continue an activation after a` |
|         - | 7737 | ` * nested call returns. pc/pTos are authoritative here only at activation` |
|         - | 7738 | ` * boundaries — the dispatch loop keeps them in locals for the hot path and` |
|         - | 7739 | ` * syncs around the call epilogue and the terminal labels. Stage 2 stacks` |
|         - | 7740 | ` * these records to replace the native recursion.` |
|         - | 7741 | ` */` |
|         - | 7742 | `typedef struct VmExecState VmExecState;` |
|         - | 7743 | `struct VmExecState` |
|         - | 7744 | `{` |
|         - | 7745 | `	VmInstr *aInstr;        /* Bytecode of this activation */` |
|         - | 7746 | `	ph7_value *pStack;      /* Operand-stack base (owned by this activation) */` |
|         - | 7747 | `	ph7_value *pTos;        /* Top-of-stack (synced at boundaries) */` |
|         - | 7748 | `	ph7_value *pHigh;       /* WATERMARK: the deepest pTos this activation ever reached,` |
|         - | 7749 | `	                         * sampled at each instruction fetch and synced with pTos at` |
|         - | 7750 | `	                         * the same boundaries. Nothing above it was ever written, so` |
|         - | 7751 | `	                         * it is what the operand stack's teardown sweep walks to --` |
|         - | 7752 | `	                         * see VmOperandStackRecycle. An OP_SPREAD that reallocs the` |
|         - | 7753 | `	                         * buffer resets it to the whole (grown) capacity rather than` |
|         - | 7754 | `	                         * carrying a pointer into the freed one. */` |
|         - | 7755 | `	sxu32 nStackCap;        /* pStack's allocated slot count; grows when an OP_SPREAD in` |
|         - | 7756 | `	                         * this activation reallocs the operand stack (see` |
|         - | 7757 | `	                         * VmGrowOperandStack). Saved/restored with the activation. */` |
|         - | 7758 | `	sxu32 nStackOrig;       /* The activation's ORIGINAL (ungrown) capacity — nMaxStack+guard,` |
|         - | 7759 | `	                         * fixed at entry. VmGrowOperandStack sizes headroom relative to` |
|         - | 7760 | `	                         * THIS (not the grown nStackCap) so capacity can't ratchet up` |
|         - | 7761 | `	                         * across statements that share one operand stack. */` |
|         - | 7762 | `	sxi32 pc;               /* Program counter (synced at boundaries) */` |
|         - | 7763 | `	sxu32 nExceptionBase;   /* Exception-stack depth at entry (finally-drain floor) */` |
|         - | 7764 | `	sxu32 nFinallyActBase;  /* aFinallyAction depth at entry: actions above it belong to` |
|         - | 7765 | `	                         * this activation and are DISCARDED (refs released) when the` |
|         - | 7766 | ``	                         * activation ends — a `return` inside a redirect-entered`` |
|         - | 7767 | `	                         * finally short-circuits OP_END_FINALLY, orphaning its` |
|         - | 7768 | `	                         * pending action (an FA_RETHROW holding the swallowed` |
|         - | 7769 | `	                         * exception), which would otherwise be mis-popped by an` |
|         - | 7770 | `	                         * enclosing function's next END_FINALLY. */` |
|         - | 7771 | `	VmFrame *pEntryFrame;   /* Active frame at entry (exec identity for VmRecordedResume) */` |
|         - | 7772 | `	ph7_value *pResult;     /* Where the terminal OP_DONE stores the result (or NULL) */` |
|         - | 7773 | `	sxu32 *pLastRef;        /* By-ref return out-param (or NULL) */` |
|         - | 7774 | `	ph7_vm_func *pEnforceRetFunc; /* Return-type enforcement target (user-fn bodies only) */` |
|         - | 7775 | `	sxu8 is_callback;       /* TRUE only for a C->PHP callback trampoline activation */` |
|         - | 7776 | `	sxu8 bReturnPropagates; /* TRUE only for a catch/finally mini-program */` |
|         - | 7777 | `};` |
|         - | 7778 | `/*` |
|         - | 7779 | ` * One in-flight user-function call: what the caller's OP_CALL set up and the` |
|         - | 7780 | ` * pop boundary (VmCallFinish) must tear down.` |
|         - | 7781 | ` */` |
|         - | 7782 | `typedef struct VmCallRecord VmCallRecord;` |
|         - | 7783 | `struct VmCallRecord` |
|         - | 7784 | `{` |
|         - | 7785 | `	ph7_vm_func *pVmFunc;   /* Callee */` |
|         - | 7786 | `	VmFrame *pFrame;        /* Callee's VM frame (entered by the OP_CALL setup) */` |
|         - | 7787 | `	ph7_value *pFrameStack; /* Callee's operand stack (owned; freed here). NULL when the body was skipped */` |
|         - | 7788 | `	sxu32 nStackCap;        /* pFrameStack's allocated slot count — nMaxStack+VM_STACK_GUARD` |
|         - | 7789 | `	                         * at setup, updated if an OP_SPREAD in the callee grew it; the` |
|         - | 7790 | `	                         * pop-time recycle releases exactly this many slots */` |
|         - | 7791 | `	sxu32 nLiveTos;         /* How many of pFrameStack's slots this activation ever` |
|         - | 7792 | `	                         * touched: its operand-stack watermark + 1. The pop-time` |
|         - | 7793 | `	                         * recycle releases exactly this many and leaves the rest` |
|         - | 7794 | `	                         * alone -- see VmOperandStackRecycle */` |
|         - | 7795 | `	sxu32 nLastRef;         /* Callee body's last-referenced slot (by-ref return) */` |
|         - | 7796 | `	sxu8 bSelfPushed;       /* TRUE when the setup pushed onto pVm->aSelf */` |
|         - | 7797 | `};` |
|         - | 7798 | `/*` |
|         - | 7799 | ` * One node of the in-loop call-record stack (BYTECODE stage 2): the caller's` |
|         - | 7800 | ` * activation to restore plus the in-flight call to finish, linked to the` |
|         - | 7801 | ` * next-outer record. Nodes are pool-allocated individually so pointers into` |
|         - | 7802 | ` * them (sState.pLastRef aims at sCall.nLastRef while the callee runs) stay` |
|         - | 7803 | ` * stable — a growable array would invalidate them on realloc. The stack is a` |
|         - | 7804 | ` * LOCAL of each native VmByteCodeExec invocation: an inner native entry` |
|         - | 7805 | ` * (mini-program, C->PHP callback, ctx resume) can never unwind records that` |
|         - | 7806 | ` * belong to an outer invocation, preserving the old nesting isolation by` |
|         - | 7807 | ` * construction.` |
|         - | 7808 | ` */` |
|         - | 7809 | `typedef struct VmCallFrame VmCallFrame;` |
|         - | 7810 | `struct VmCallFrame` |
|         - | 7811 | `{` |
|         - | 7812 | `	VmExecState sCaller;   /* Caller activation, restored on pop */` |
|         - | 7813 | `	VmCallRecord sCall;    /* The in-flight call, finished (VmCallFinish) on pop */` |
|         - | 7814 | `	VmCallFrame *pPrev;    /* Next-outer record, or NULL at this invocation's base */` |
|         - | 7815 | `};` |
|         - | 7816 | `typedef struct VmParkedSegment VmParkedSegment;` |
|         - | 7817 | `/*` |
|         - | 7818 | ` * BYTECODE stage 4: a Fiber::suspend() from inside a nested PHP call parks the` |
|         - | 7819 | ` * whole trampoline record segment here instead of unwinding it. The records,` |
|         - | 7820 | ` * their VmFrames and operand stacks all stay alive on the heap (that IS what a` |
|         - | 7821 | ` * suspended fiber is); only the dispatch loop's pointers move into the ctx.` |
|         - | 7822 | ` * Resume re-pushes the chain and continues INSIDE the innermost callee.` |
|         - | 7823 | ` */` |
|         - | 7824 | `struct VmParkedSegment` |
|         - | 7825 | `{` |
|         - | 7826 | `	VmExecState sState;    /* Innermost activation — resume re-enters here (pTos synced) */` |
|         - | 7827 | `	VmCallFrame *pCallTop; /* Parked record chain (caller activations toward the body) */` |
|         - | 7828 | `	VmFrame *pTopFrame;    /* pVm->pFrame at suspend (innermost callee / open-try frame) */` |
|         - | 7829 | `	sxu32 nOldExcBase;     /* pCtx->nExceptionBase at park — resume rebases the segment's` |
|         - | 7830 | `	                        * absolute nExceptionBase floors by (newBase - nOldExcBase) */` |
|         - | 7831 | `	sxu32 nOldFinBase;     /* pCtx->nFinallyBase at park — resume rebases the segment's` |
|         - | 7832 | `	                        * absolute nFinallyActBase floors by its OWN delta (the two` |
|         - | 7833 | `	                        * stacks move independently) */` |
|         - | 7834 | `	int nRecords;          /* Chain length: each record contributed one nRecursionDepth++` |
|         - | 7835 | `	                        * (and, if bSelfPushed, one aSelf push) that VmCallFinish never` |
|         - | 7836 | `	                        * ran. Deactivate that accounting while parked, reactivate on` |
|         - | 7837 | `	                        * resume; an abandoned segment stays deactivated. */` |
|         - | 7838 | `};` |
|         - | 7839 |  |
|         - | 7840 | `PH7_PRIVATE sxi32 VmByteCodeExec(ph7_vm *pVm,VmInstr *aInstr,ph7_value *pStack,int nTos,` |
|         - | 7841 | `	ph7_value *pResult,sxu32 *pLastRef,int is_callback,sxi32 nPc,` |
|         - | 7842 | `	ph7_vm_func *pEnforceRetFunc,int bReturnPropagates,` |
|         - | 7843 | `	VmParkedSegment *pAdoptSegment,ph7_value **ppBaseOwner,` |
|         - | 7844 | `	sxu32 *pnBaseCap,sxu32 nStackOrig);` |
|         - | 7845 | `/* vm.c frame/type-enforcement internals shared with vm_exec_ctx.c (and the` |
|         - | 7846 | ` * upcoming vm_error.c) */` |
|         - | 7847 | `PH7_PRIVATE int VmCheckPseudoType(ph7_vm *pVm, ph7_value *pValue, const SyString *pClass);` |
|         - | 7848 | `PH7_PRIVATE sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable, int bStrict,` |
|         - | 7849 | `	ph7_class *pSelf);` |
|         - | 7850 | `PH7_PRIVATE void VmMaterializeIntTyped(ph7_value *pVal, sxu32 nType);` |
|         - | 7851 | `PH7_PRIVATE void VmDropResumeTarget(ph7_vm *pVm, VmFrame *pFrame);` |
|         - | 7852 | `PH7_PRIVATE void VmReleaseFrameForeachSteps(ph7_vm *pVm, VmFrame *pFrame);` |
|         - | 7853 | `/* The in-place-catch resume record, moved as a whole. See its four fields in ph7_vm. */` |
|         - | 7854 | `typedef struct VmResumeTarget {` |
|         - | 7855 | `	VmFrame *pFrame;` |
|         - | 7856 | `	sxu32 iPc;` |
|         - | 7857 | `	void *pInstr;` |
|         - | 7858 | `	sxi32 iStackDepth;` |
|         - | 7859 | `} VmResumeTarget;` |
|         - | 7860 | `PH7_PRIVATE void VmSetResumeTarget(ph7_vm *pVm,VmFrame *pFrame,sxu32 iPc,void *pInstr,sxi32 iStackDepth);` |
|         - | 7861 | `PH7_PRIVATE void VmClearResumeTarget(ph7_vm *pVm);` |
|         - | 7862 | `PH7_PRIVATE void VmSaveResumeTarget(ph7_vm *pVm,VmResumeTarget *pSave);` |
|         - | 7863 | `PH7_PRIVATE void VmRestoreResumeTarget(ph7_vm *pVm,const VmResumeTarget *pSave);` |
|         - | 7864 | `/* Flags for VmEnforcePropertyTypeOnStore(). CLONE_INIT is php 8.5's` |
|         - | 7865 | ` * clone-with re-initialization of a readonly property; VIA_REF says the write` |
|         - | 7866 | ` * arrived through a REFERENCE to the slot rather than through the property` |
|         - | 7867 | ` * itself, which is a sentence of its own in php. */` |
|         - | 7868 | `#define VM_TYPED_STORE_CLONE_INIT 0x01` |
|         - | 7869 | `#define VM_TYPED_STORE_VIA_REF    0x02` |
|         - | 7870 | `PH7_PRIVATE sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue,int iStoreFlags);` |
|         - | 7871 | `PH7_PRIVATE sxi32 PH7_VmNativeSetSlot(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue);` |
|         - | 7872 | `PH7_PRIVATE sxi32 PH7_VmStoreFilterRegister(ph7_vm *pVm,VmClassAttr *pVmAttr);` |
|         - | 7873 | `PH7_PRIVATE void PH7_VmStoreFilterDrop(ph7_vm *pVm,ph7_class_attr *pAttr,sxu32 nIdx);` |
|         - | 7874 | `PH7_PRIVATE sxi32 VmEnforceScalarType(ph7_value *pVal, sxu32 nType, int bStrict);` |
|         - | 7875 | `PH7_PRIVATE void VmExcReleaseAll(ph7_vm *pVm,SySet *pSet);` |
|         - | 7876 | `PH7_PRIVATE const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf);` |
|         - | 7877 | `PH7_PRIVATE int VmFuncHasReturnType(ph7_vm_func *pFunc);` |
|         - | 7878 | `PH7_PRIVATE int PH7_VmHookSplitName(SyString *pName,SyString *pProp,const char **pzKind);` |
|         - | 7879 | `PH7_PRIVATE int PH7_VmHookFuncName(ph7_class *pClass,ph7_vm_func *pFunc,SyBlob *pOut);` |
|         - | 7880 | `PH7_PRIVATE sxu32 VmFuncRequiredArgCount(ph7_vm_func *pFunc,sxu32 *pnNonVariadic);` |
|         - | 7881 | `PH7_PRIVATE void VmLeaveFrame(ph7_vm *pVm);` |
|         - | 7882 | `PH7_PRIVATE int VmNativeNestingExceeded(ph7_vm *pVm);` |
|         - | 7883 | `PH7_PRIVATE sxi32 VmNativeNestingFatal(ph7_vm *pVm);` |
|         - | 7884 | `PH7_PRIVATE VmFrame * VmNewFrame(ph7_vm *pVm, void *pUserData, ph7_class_instance *pThis);` |
|         - | 7885 | `PH7_PRIVATE ph7_class *VmHintScopeClass(ph7_vm *pVm, ph7_class *pDecl, ph7_class *pUsing);` |
|         - | 7886 | `PH7_PRIVATE int VmClassHintMatches(ph7_vm *pVm,const SyString *pName,ph7_class *pScope,` |
|         - | 7887 | `	ph7_value *pVal,ph7_class **ppResolved);` |
|         - | 7888 | `PH7_PRIVATE const char *VmHintTextResolved(ph7_vm *pVm,const SyString *pDeclared,ph7_class *pScope,` |
|         - | 7889 | `	char *zBuf,sxu32 nBuf);` |
|         - | 7890 | ``#define PH7_HINT_TEXT_ITERABLE 0x1 /* expand a standalone `iterable` to Traversable\|array */`` |
|         - | 7891 | ``#define PH7_HINT_TEXT_STATIC   0x2 /* resolve `static` beside `self`/`parent` */`` |
|         - | 7892 | `PH7_PRIVATE const char *VmHintTextResolvedEx(ph7_vm *pVm,const SyString *pDeclared,ph7_class *pScope,` |
|         - | 7893 | `	int iFlags,char *zBuf,sxu32 nBuf);` |
|         - | 7894 | `PH7_PRIVATE ph7_class *VmHintScopeDeclared(ph7_class *pDecl);` |
|         - | 7895 | `PH7_PRIVATE ph7_class *VmResolveTypeClass(ph7_vm *pVm, const SyString *pCN, ph7_class *pSelf);` |
|         - | 7896 | `PH7_PRIVATE const char *VmScalarTypeName(sxu32 nType, SyString *pDeclared, char *zBuf, sxu32 nBuf);` |
|         - | 7897 | `PH7_PRIVATE const char *VmClassHintTypeName(const SyString *pAsWritten,ph7_class *pResolved,` |
|         - | 7898 | `	int bNullable,char *zBuf,sxu32 nBuf);` |
|         - | 7899 | `PH7_PRIVATE sxi32 VmThrowBuiltinTooFewArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName, sxu32 nPassed,sxu32 nRequired,sxu32 nMaxDeclared);` |
|         - | 7900 | `PH7_PRIVATE sxi32 VmThrowBuiltinTooManyArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName, sxu32 nPassed,sxu32 nMax,sxu32 nRequired);` |
|         - | 7901 | `PH7_PRIVATE sxi32 VmThrowBuiltinExtraNamed(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName);` |
|         - | 7902 | `PH7_PRIVATE sxi32 VmThrowTooFewArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName, ph7_vm_func *pCallee,sxu32 nPassed,sxu32 nRequired,sxu32 nNonVariadic,int bCallSite);` |
|         - | 7903 | `PH7_PRIVATE void PH7_VmWarnByRefValueGiven(ph7_vm *pVm,ph7_class *pOwnerClass,ph7_vm_func *pCallee,sxu32 nArgPos,SyString *pArgName);` |
|         - | 7904 | `PH7_PRIVATE sxi32 VmThrowByRefRefusal(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,ph7_vm_func *pCallee,sxu32 nArgPos,SyString *pArgName);` |
|         - | 7905 | `PH7_PRIVATE sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,ph7_class *pOwnerClass,ph7_vm_func *pCallee,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven);` |
|         - | 7906 | `PH7_PRIVATE sxi32 VmVariadicElementTypeCheck(ph7_vm *pVm,ph7_class *pSelfHint,ph7_vm_func *pCallee,ph7_vm_func_arg *pFormal,ph7_value *pVal,sxu32 nArgPos,int bCallIsStrict);` |
|         - | 7907 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex);` |
|         - | 7908 | `PH7_PRIVATE sxi32 VmMemPoolInit(VmMemPool *pPool,SyMemBackend *pAllocator);` |
|         - | 7909 | `PH7_PRIVATE ph7_value * VmMemPoolReserve(VmMemPool *pPool,sxu32 *pIndex);` |
|         - | 7910 | `PH7_PRIVATE sxi32 VmMemPoolTruncate(VmMemPool *pPool,sxu32 nNewSize);` |
|         - | 7911 | `PH7_PRIVATE void VmMemPoolFreeSlot(VmMemPool *pPool,sxu32 nIdx);` |
|         - | 7912 | `/* Argument-unpacking key capture (PHP 8.1 named-parameter semantics for spreads).` |
|         - | 7913 | `` * `pMap->aNames` is COMPILE-TIME metadata indexed by compile-time argument`` |
|         - | 7914 | ` * position, but a runtime spread expands its slot to a variable element count,` |
|         - | 7915 | ` * so any spread that expands to !=1 element shifts the following actual stack` |
|         - | 7916 | ` * positions out of alignment with aNames — and the element keys (which PHP 8.1` |
|         - | 7917 | ` * treats as named arguments) are otherwise discarded. OP_SPREAD records one` |
|         - | 7918 | ` * VmSpreadRun per expansion plus one VmSpreadKey per element (in order) on the` |
|         - | 7919 | ` * VM; CALL/NEW replay them (VmBuildEffectiveArgMap) into an effective map with` |
|         - | 7920 | ` * one name entry per ACTUAL slot, then let the existing named-argument resolver` |
|         - | 7921 | ` * run unchanged. The same runs give each call its own argument-count growth` |
|         - | 7922 | ` * (VmSpreadOwnExtra). This call's runs are consumed (truncated) at the CALL. */` |
|         - | 7923 | `typedef struct VmSpreadRun VmSpreadRun;` |
|         - | 7924 | `struct VmSpreadRun {` |
|         - | 7925 | `	ph7_value *pStart;   /* First stack slot the expansion wrote (the source slot) */` |
|         - | 7926 | `	sxu32 nCount;        /* Elements produced (0 for an empty array) */` |
|         - | 7927 | `	sxu32 nKeyStart;     /* aSpreadKey index of this run's first element key */` |
|         - | 7928 | `	sxu32 nBlobStart;    /* sSpreadKeyBlob length before this run's keys were appended */` |
|         - | 7929 | `};` |
|         - | 7930 | `typedef struct VmSpreadKey VmSpreadKey;` |
|         - | 7931 | `struct VmSpreadKey {` |
|         - | 7932 | `	sxu32 nOff;          /* Byte offset into pVm->sSpreadKeyBlob (valid iff nLen>0) */` |
|         - | 7933 | `	sxu32 nLen;          /* Key length; 0 == integer key == positional element */` |
|         - | 7934 | `};` |
|         - | 7935 | `#define VM_STACK_UNMODELED SXU32_HIGH /* shared by vm.c (stack modeling) and vm_exec.c */` |
|         - | 7936 | `/* vm_ops_*.c — opcode handlers extracted from VmByteCodeExecBody. The loop` |
|         - | 7937 | ` * syncs pTos/pc into its VmExecState, calls the handler, reloads them and` |
|         - | 7938 | ` * maps the returned code onto its labels. */` |
|         - | 7939 | `typedef enum VmOpRc {` |
|         - | 7940 | `	VM_OP_NEXT = 0,   /* arm done: fall to the loop's trailing pc++ */` |
|         - | 7941 | `	VM_OP_ABORT,      /* -> the loop's Abort label */` |
|         - | 7942 | `	VM_OP_EXCEPTION   /* -> the loop's Exception label */` |
|         - | 7943 | `} VmOpRc;` |
|         - | 7944 | `PH7_PRIVATE int VmRecordedResume(ph7_vm *pVm,sxi32 *pResumePc,VmFrame *pEntryFrame,VmInstr *aInstr);` |
|         - | 7945 | `/*` |
|         - | 7946 | ` * Does the pending ROOT C inline redirect belong to the activation running` |
|         - | 7947 | ` * (aInstrArg, pEntryArg)? Both halves are needed: the bytecode array alone is` |
|         - | 7948 | ` * shared by every live activation of one function (see pInlineFrame). A redirect` |
|         - | 7949 | ` * whose owning frame was invalidated (VmDrainFinally retires a handler by zeroing` |
|         - | 7950 | ` * it) names no activation, so it falls back to the bytecode array alone — losing` |
|         - | 7951 | ` * the catch entirely would be worse than landing it one activation over.` |
|         - | 7952 | ` */` |
|         - | 7953 | `#define VmInlineOwnedBy(pVm,aInstrArg,pEntryArg) \` |
|         - | 7954 | `	((pVm)->pInlineInstr == (void *)(aInstrArg) \` |
|         - | 7955 | `	 && ((pVm)->pInlineFrame == 0 \|\| (pVm)->pInlineFrame == (void *)(pEntryArg)))` |
|         - | 7956 | `PH7_PRIVATE void VmPopOperand(ph7_value **ppTos, sxi32 nPop);` |
|         - | 7957 | `PH7_PRIVATE void VmForeachStepUnlink(ph7_foreach_info *pInfo,ph7_foreach_step *pStep);` |
|         - | 7958 | `PH7_PRIVATE int VmHookGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName);` |
|         - | 7959 | `PH7_PRIVATE void VmForeachHashmapStepRelease(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,int bPop);` |
|         - | 7960 | `PH7_PRIVATE void VmForeachStepRelease(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep);` |
|         - | 7961 | `PH7_PRIVATE void VmForeachStepAbandon(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,ph7_class_instance *pThis);` |
|         - | 7962 | `PH7_PRIVATE int VmClassAllowsDynamicProps(ph7_vm *pVm,ph7_class *pClass);` |
|         - | 7963 | `PH7_PRIVATE int VmClassHasAttributeNamed(ph7_class *pClass,const char *zName,sxu32 nName);` |
|         - | 7964 | `/* __PHP_Incomplete_Class: unserialize()'s carrier object (vm.c helpers).` |
|         - | 7965 | ` * PH7_INCOMPLETE_MAGIC_MEMBER is php's MAGIC_MEMBER — the dynamic property that` |
|         - | 7966 | ` * remembers the original class name; the serializer strips it back out. */` |
|         - | 7967 | `#define PH7_INCOMPLETE_MAGIC_MEMBER "__PHP_Incomplete_Class_Name"` |
|         - | 7968 | `PH7_PRIVATE int PH7_VmIsIncompleteClass(ph7_vm *pVm,ph7_class *pClass);` |
|         - | 7969 | `PH7_PRIVATE void PH7_VmIncompleteMsg(ph7_vm *pVm,ph7_class_instance *pThis,const char *zWhat,SyBlob *pOut);` |
|         - | 7970 | `PH7_PRIVATE void PH7_VmIncompleteAccessWarn(ph7_vm *pVm,ph7_class_instance *pThis,SyString *pFuncName);` |
|         - | 7971 | `PH7_PRIVATE void VmRecreateDeclaredAttr(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_attr *pAttr,VmClassAttr **ppAttr);` |
|         - | 7972 | `PH7_PRIVATE int VmMemberCtxIsLookup(sxi32 iP2);` |
|         - | 7973 | `PH7_PRIVATE int VmMemberCtxWantsValue(sxi32 iP2);` |
|         - | 7974 | `PH7_PRIVATE int VmMemberNextIsWrite(const VmInstr *pNext);` |
|         - | 7975 | `PH7_PRIVATE int VmMemberNextIsRmw(const VmInstr *pNext);` |
|         - | 7976 | `PH7_PRIVATE int VmNextIsCompoundAssign(const VmInstr *pNext);` |
|         - | 7977 | `PH7_PRIVATE sxi32 VmSpreadOwnExtra(ph7_vm *pVm, sxi32 iP1, ph7_value *pTos);` |
|         - | 7978 | `PH7_PRIVATE VmCallArgMap *VmEffCallArgMap(ph7_vm *pVm, VmInstr *pInstr, ph7_value *pArg, sxu32 nActual, VmCallArgMap *pStorage);` |
|         - | 7979 | `PH7_PRIVATE int VmMagicGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind);` |
|         - | 7980 | `PH7_PRIVATE void VmMagicGuardPush(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind);` |
|         - | 7981 | `PH7_PRIVATE void VmMagicGuardPop(ph7_vm *pVm);` |
|         - | 7982 | `PH7_PRIVATE void VmPinMemObjSlot(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 7983 | `PH7_PRIVATE void PH7_VmBindAttrRef(ph7_vm *pVm,VmClassAttr *pVmAttr,sxu32 nSrcIdx);` |
|         - | 7984 | `PH7_PRIVATE void VmPinMemObjSlotCounted(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 7985 | `PH7_PRIVATE void VmUnpinMemObjSlot(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 7986 | `PH7_PRIVATE sxi32 VmSpreadMergeStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData);` |
|         - | 7987 | `PH7_PRIVATE int VmValueIsTraversable(ph7_vm *pVm, ph7_value *pVal);` |
|         - | 7988 | `PH7_PRIVATE sxi32 VmHashmapRefInsert(ph7_hashmap *pMap, const char *zKey, sxu32 nByte, sxu32 nRefIdx);` |
|         - | 7989 | `PH7_PRIVATE void VmWarnCannotUseAsArray(ph7_vm *pVm,sxi32 iFlags);` |
|         - | 7990 | `PH7_PRIVATE sxi32 VmHookRmwConsume(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 7991 | `PH7_PRIVATE void VmHookRmwFreeScratch(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 7992 | `PH7_PRIVATE sxi32 VmHookSetDispatch(ph7_vm *pVm,ph7_class_instance *pHThis,ph7_class_attr *pHAttr,sxu32 nBackIdx,ph7_value *pValue);` |
|         - | 7993 | `PH7_PRIVATE void VmMagicSetDispatch(ph7_vm *pVm,ph7_class_instance *pSetThis,const SyString *pName,ph7_value *pValue);` |
|         - | 7994 | `PH7_PRIVATE ph7_exception * VmExcActivate(ph7_vm *pVm,ph7_exception *pCompiled);` |
|         - | 7995 | `PH7_PRIVATE ph7_exception * VmExcLive(ph7_vm *pVm,ph7_exception *pCompiled);` |
|         - | 7996 | `PH7_PRIVATE sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight);` |
|         - | 7997 | `PH7_PRIVATE sxu32 VmComputeMaxStack(ph7_vm *pVm, VmInstr *aInstr, sxu32 nInstr);` |
|         - | 7998 | `PH7_PRIVATE sxi32 VmDrainFinally(ph7_vm *pVm, sxu32 nExceptionBase);` |
|         - | 7999 | `PH7_PRIVATE sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName);` |
|         - | 8000 | `PH7_PRIVATE void VmHookRmwDropTop(ph7_vm *pVm);` |
|         - | 8001 | `PH7_PRIVATE sxi32 VmInitCallContext(ph7_context *pOut, ph7_vm *pVm, ph7_user_func *pFunc, ph7_value *pRet, sxi32 iFlags);` |
|         - | 8002 | `PH7_PRIVATE void VmMaterializeCatchReturn(ph7_vm *pVm, ph7_value *pResult, VmFrame *pEntryFrame);` |
|         - | 8003 | `PH7_PRIVATE ph7_value * VmOperandStackAlloc(ph7_vm *pVm, sxu32 nSlots);` |
|         - | 8004 | `PH7_PRIVATE void VmOperandStackRecycle(ph7_vm *pVm, ph7_value *pStack, sxu32 nCap, sxu32 nLive);` |
|         - | 8005 | `PH7_PRIVATE ph7_vm_func * VmOverload(ph7_vm *pVm, ph7_vm_func *pList, ph7_value *aArg, int nArg);` |
|         - | 8006 | `PH7_PRIVATE void VmReleaseCallContext(ph7_context *pCtx);` |
|         - | 8007 | `PH7_PRIVATE sxi32 VmResolveNamedArgs(ph7_vm *pVm, VmCallArgMap *pMap, ph7_vm_func_arg *aFormalArg, sxu32 nNonVariadic, sxi32 iVariadicIdx, sxu32 nActual, sxi32 *aSlot, sxu8 *aUsed);` |
|         - | 8008 | `PH7_PRIVATE void VmSpreadExpandMap(ph7_vm *pVm, ph7_value **ppTos, ph7_hashmap *pMap, int bVarSource);` |
|         - | 8009 | `PH7_PRIVATE sxi32 VmSpreadValuesStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData);` |
|         - | 8010 | `PH7_PRIVATE int VmStringWantsPerlIncr(ph7_value *pVal);` |
|         - | 8011 | `PH7_PRIVATE sxi32 VmSuspendCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, sxi32 pc, sxi32 nTos);` |
|         - | 8012 | `PH7_PRIVATE int VmExcMatches(ph7_exception *pExc,ph7_exception *pCompiled);` |
|         - | 8013 | `PH7_PRIVATE void VmSpreadConsume(ph7_vm *pVm);` |
|         - | 8014 | `PH7_PRIVATE VmOpRc VmExecOpSwitch(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 8015 | `PH7_PRIVATE VmOpRc VmExecOpCatch(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 8016 | `PH7_PRIVATE VmOpRc VmExecOpLoadException(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 8017 | `PH7_PRIVATE VmOpRc VmExecOpSpaceship(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 8018 | `PH7_PRIVATE VmOpRc VmExecOpGe(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 8019 | `PH7_PRIVATE VmOpRc VmExecOpLe(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 8020 | `PH7_PRIVATE VmOpRc VmExecOpTne(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 8021 | `PH7_PRIVATE VmOpRc VmExecOpTeq(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 8022 | `PH7_PRIVATE VmOpRc VmExecOpNeq(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 8023 | `PH7_PRIVATE VmOpRc VmExecOpLor(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 8024 | `PH7_PRIVATE VmOpRc VmExecOpShrStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 8025 | `PH7_PRIVATE VmOpRc VmExecOpShr(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 8026 | `PH7_PRIVATE VmOpRc VmExecOpMulStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 8027 | `PH7_PRIVATE VmOpRc VmExecOpLoadList(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 8028 | `PH7_PRIVATE VmOpRc VmExecOpUnsetVar(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 8029 | `PH7_PRIVATE VmOpRc VmExecOpConsume(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 8030 | `PH7_PRIVATE VmOpRc VmExecOpMatch(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 8031 | `PH7_PRIVATE VmOpRc VmExecOpClone(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 8032 | `PH7_PRIVATE VmOpRc VmExecOpThrow(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 8033 | `PH7_PRIVATE VmOpRc VmExecOpNullcStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 8034 | `PH7_PRIVATE VmOpRc VmExecOpDiv(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 8035 | `PH7_PRIVATE VmOpRc VmExecOpModStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 8036 | `PH7_PRIVATE VmOpRc VmExecOpMod(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 8037 | `PH7_PRIVATE VmOpRc VmExecOpSubStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 8038 | `PH7_PRIVATE VmOpRc VmExecOpSub(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 8039 | `PH7_PRIVATE VmOpRc VmExecOpPowStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 8040 | `PH7_PRIVATE void PH7_MemObjPow(ph7_value *pBase,ph7_value *pExp,ph7_value *pOut);` |
|         - | 8041 | `PH7_PRIVATE VmOpRc VmExecOpLoadMap(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 8042 | `PH7_PRIVATE VmOpRc VmExecOpLoadIdx(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 8043 | `PH7_PRIVATE VmOpRc VmExecOpLoadClosure(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 8044 | `PH7_PRIVATE VmOpRc VmExecOpStoreIdxRef(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 8045 | `PH7_PRIVATE VmOpRc VmExecOpStoreRef(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 8046 | `PH7_PRIVATE VmOpRc VmExecOpMember(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 8047 | `PH7_PRIVATE VmOpRc VmExecOpNew(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 8048 | `PH7_PRIVATE VmOpRc VmExecOpForeachInit(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 8049 | `PH7_PRIVATE VmOpRc VmExecOpForeachStep(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 8050 | `/* vm_error.c — error/diagnostics/type-enforcement machinery shared with vm.c */` |
|         - | 8051 | `PH7_PRIVATE sxi32 PH7_VmErrPhpBit(sxi32 iErr);` |
|         - | 8052 | `PH7_PRIVATE void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|         - | 8053 | `PH7_PRIVATE void PH7_VmActiveFuncName(ph7_vm *pVm,SyBlob *pOut);` |
|         - | 8054 | `PH7_PRIVATE sxi32 VmEnterFrame(ph7_vm *pVm,void *pUserData,ph7_class_instance *pThis,VmFrame **ppFrame);` |
|         - | 8055 | `PH7_PRIVATE void VmExcRelease(ph7_vm *pVm,ph7_exception *pExc);` |
|         - | 8056 | `PH7_PRIVATE void VmClearFramePending(VmFrame *pFrame);` |
|         - | 8057 | `PH7_PRIVATE sxi32 VmDrainCrossedTrys(ph7_vm *pVm,sxu32 nCross,sxu32 nFloor);` |
|         - | 8058 | `PH7_PRIVATE sxi32 VmLocalExecIntoObj(ph7_vm *pVm,SySet *pByteCode,ph7_value **ppMemObj,int bReturnPropagates);` |
|         - | 8059 |  |
|         - | 8060 | `PH7_PRIVATE sxi32 VmArithOperandCheck(ph7_vm *pVm,ph7_value *pLeft,ph7_value *pRight,const char *zOp,SyBlob *pMsgOut);` |
|         - | 8061 | `PH7_PRIVATE const char * VmArithTypeName(ph7_value *pVal);` |
|         - | 8062 | `PH7_PRIVATE const char * VmArithValueName(ph7_value *pVal);` |
|         - | 8063 | `PH7_PRIVATE void VmIncDecTypeErrorMsg(ph7_value *pVal,int bIncr,SyBlob *pMsgOut);` |
|         - | 8064 | `PH7_PRIVATE sxi32 VmUnaryArithOperandCheck(ph7_vm *pVm,ph7_value *pVal,SyBlob *pMsgOut);` |
|         - | 8065 | `PH7_PRIVATE void VmBitNotTypeErrorMsg(ph7_value *pVal,SyBlob *pMsgOut);` |
|         - | 8066 | `PH7_PRIVATE void VmStringBitwise(ph7_value *pLeft,ph7_value *pRight,int cOp,SyBlob *pOut);` |
|         - | 8067 | `PH7_PRIVATE sxi32 VmCheckReadonlyMutate(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 8068 | `PH7_PRIVATE sxi32 VmCheckSetVisibility(ph7_vm *pVm,ph7_class *pOwner,ph7_class_attr *pAttr);` |
|         - | 8069 | `PH7_PRIVATE sxi32 VmThrowNativeNoWrite(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr);` |
|         - | 8070 | `PH7_PRIVATE sxi32 VmThrowNativeNoUnset(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr);` |
|         - | 8071 | `PH7_PRIVATE sxi32 VmThrowNativeReadOnly(ph7_vm *pVm,ph7_class_attr *pAttr);` |
|         - | 8072 | `PH7_PRIVATE void PH7_NativeMarkAttrReadOnly(ph7_class_instance *pThis,const char *zProp);` |
|         - | 8073 | `PH7_PRIVATE sxi32 VmCheckReadonlyUnset(ph7_vm *pVm,ph7_class *pClass,VmClassAttr *pVmAttr);` |
|         - | 8074 | `PH7_PRIVATE sxi32 PH7_VmCheckIndirectModify(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 8075 | `PH7_PRIVATE sxi32 VmCloneApplyUpdate(ph7_vm *pVm,ph7_class_instance *pClone, const char *zName,sxu32 nName,ph7_value *pValue);` |
|         - | 8076 | `PH7_PRIVATE void VmCoalesceDisarm(ph7_vm *pVm);` |
|         - | 8077 | `PH7_PRIVATE sxi32 VmConstCycleThrow(ph7_vm *pVm);` |
|         - | 8078 | `PH7_PRIVATE ph7_class * VmCurrentSelf(ph7_vm *pVm);` |
|         - | 8079 | `PH7_PRIVATE sxi32 VmEnforceConstantType(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue,int bLazy);` |
|         - | 8080 | `PH7_PRIVATE sxi32 VmEnforceTypedDefault(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue);` |
|         - | 8081 | `PH7_PRIVATE sxi32 VmCheckTypedDefault(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue);` |
|         - | 8082 | `PH7_PRIVATE int VmTypedDefaultRefusal(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue,SyBlob *pMsg);` |
|         - | 8083 | `PH7_PRIVATE int VmArgDefaultRefusal(ph7_vm *pVm,ph7_class *pScope,ph7_vm_func_arg *pArg,ph7_value *pValue,SyBlob *pMsg);` |
|         - | 8084 | `PH7_PRIVATE int VmArgDefaultWidens(ph7_vm_func_arg *pArg,ph7_value *pValue);` |
|         - | 8085 | `PH7_PRIVATE sxi32 VmEnforceArgType(ph7_vm *pVm,ph7_vm_func *pFunc,ph7_vm_func_arg *pFormal,` |
|         - | 8086 | `	sxu32 nArgPos,ph7_value *pVal,int bStrict,ph7_class *pSelfHint);` |
|         - | 8087 | `PH7_PRIVATE int VmClassStaticDeferPending(ph7_class *pClass);` |
|         - | 8088 | `PH7_PRIVATE sxi32 PH7_VmMaterializeClassStatics(ph7_vm *pVm,ph7_class *pClass);` |
|         - | 8089 | `PH7_PRIVATE void PH7_VmResolvedDefault(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue);` |
|         - | 8090 | `PH7_PRIVATE sxi32 VmEnforceReturnType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue);` |
|         - | 8091 | `PH7_PRIVATE sxi32 VmEnumMaterializeCase(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pCase);` |
|         - | 8092 | `PH7_PRIVATE int VmFinallyAdvance(ph7_vm *pVm, VmInstr *aInstr, int *pnCross, sxu32 *pPc);` |
|         - | 8093 | `PH7_PRIVATE int VmRecursionExceeded(ph7_vm *pVm);` |
|         - | 8094 | `PH7_PRIVATE sxi32 VmRecursionFatal(ph7_vm *pVm);` |
|         - | 8095 | `PH7_PRIVATE int VmValueIsLossyToInt(ph7_value *pVal);` |
|         - | 8096 | `PH7_PRIVATE sxi32 VmRejectFloatOperand(ph7_vm *pVm,ph7_value *pVal);` |
|         - | 8097 | `PH7_PRIVATE sxi32 VmThrowArgNotPassed(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName, ph7_vm_func *pCallee,sxu32 nArg,SyString *pArgName);` |
|         - | 8098 | `PH7_PRIVATE sxi32 VmThrowBuiltinError(ph7_vm *pVm,const char *zClass,sxu32 nClass,SyBlob *pMsg);` |
|         - | 8099 | `PH7_PRIVATE sxi32 VmThrowBuiltinErrorCode(ph7_vm *pVm,const char *zClass,sxu32 nClass,` |
|         - | 8100 | `	SyBlob *pMsg,sxi32 iCode);` |
|         - | 8101 | `PH7_PRIVATE sxi32 VmThrowFixedError(ph7_vm *pVm, const char *zClass, const char *zMsg);` |
|         - | 8102 | `PH7_PRIVATE sxi32 VmThrowFixedErrorCode(ph7_vm *pVm,const char *zClass,sxi32 iCode,` |
|         - | 8103 | `	const char *zMsg);` |
|         - | 8104 | `PH7_PRIVATE sxi32 VmThrowFromVm(ph7_vm *pVm, const char *zClass, const char *zMsg, sxu32 nMsg);` |
|         - | 8105 | `PH7_PRIVATE sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg);` |
|         - | 8106 | `PH7_PRIVATE sxi32 VmThrowSpreadError(ph7_vm *pVm,ph7_value *pBad,int bArgUnpack);` |
|         - | 8107 | `PH7_PRIVATE sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr);` |
|         - | 8108 | `PH7_PRIVATE sxi32 VmThrowAutoInitArrayError(ph7_vm *pVm,VmClassAttr *pVmAttr);` |
|         - | 8109 | `PH7_PRIVATE sxi32 VmAutoInitArrayProperty(ph7_vm *pVm,VmClassAttr *pVmAttr,ph7_value *pSlot);` |
|         - | 8110 | `PH7_PRIVATE sxi32 VmRefUninitTypedProperty(ph7_vm *pVm,ph7_class *pClass,VmClassAttr *pVmAttr,ph7_value *pSlot);` |
|         - | 8111 | `PH7_PRIVATE sxi32 VmUncaughtException(ph7_vm *pVm, ph7_class_instance *pThis);` |
|         - | 8112 | `/* vm_arg_check.c — builtin arity/signature enforcement, called from vm.c */` |
|         - | 8113 | `PH7_PRIVATE void VmSetBuiltinArity(ph7_vm *pVm);` |
|         - | 8114 | `PH7_PRIVATE void VmSetBuiltinSignatures(ph7_vm *pVm);` |
|         - | 8115 | `/* Signature-string derivations, shared with the native-class builder: one` |
|         - | 8116 | ` * PHP-style parameter list ("string $s, int $o = 0") is the single source of a` |
|         - | 8117 | ` * callee's arity bounds and by-ref positions, for a builtin and a native method` |
|         - | 8118 | ` * alike — which is how a native method gets the too-few/too-many ArgumentCountError` |
|         - | 8119 | ` * that a prelude-declared method never had. */` |
|         - | 8120 | `PH7_PRIVATE void VmDeriveArityFromSig(const char *zSig,sxi16 *pnMin,sxu8 *pbAtLeast,sxi16 *pnMax,sxu8 *pbHasMax);` |
|         - | 8121 | `PH7_PRIVATE sxu32 VmDeriveByRefMaskFromSig(const char *zSig);` |
|         - | 8122 | `PH7_PRIVATE int VmBuiltinPrefersRef(SyString *pName);` |
|         - | 8123 | `PH7_PRIVATE sxi32 PH7_VmBindNamedArgsToSig(ph7_context *pCtx,ph7_user_func *pFunc,VmCallArgMap *pMap,SySet *pArgSet,int *pnArg,int *pnExtra,VmCallArgMap *pTail);` |
|         - | 8124 | `/* PH7_VmBuiltinExtraNamedRule: what a variadic builtin does with unknown named extras */` |
|         - | 8125 | `#define VM_XNAMED_TAKE          0` |
|         - | 8126 | `#define VM_XNAMED_REFUSE        1` |
|         - | 8127 | `#define VM_XNAMED_BEFORE_TYPES  2` |
|         - | 8128 | `#define VM_XNAMED_BEFORE_ARITY  3` |
|         - | 8129 | `PH7_PRIVATE int PH7_VmBuiltinExtraNamedRule(const SyString *pName);` |
|         - | 8130 | `PH7_PRIVATE sxi32 PH7_VmRefuseExtraNamed(ph7_context *pCtx,ph7_user_func *pFunc);` |
|         - | 8131 | `PH7_PRIVATE sxi32 VmEnforceBuiltinArgTypes(ph7_context *pCtx,ph7_user_func *pFunc,int nGiven,ph7_value **apArg);` |
|         - | 8132 | `PH7_PRIVATE sxi32 PH7_VmScreenByRefArgShapes(ph7_context *pCtx,ph7_user_func *pFunc,VmCallArgMap *pMap,int nGiven,ph7_value **apArg);` |
|         - | 8133 | `PH7_PRIVATE int PH7_VmSigParamName(const char *zSig,int nPos,SyString *pOut);` |
|         - | 8134 | `PH7_PRIVATE int PH7_VmSigNamedParam(const char *zSig,const SyString *pName,int *pbVariadic);` |
|         - | 8135 | `PH7_PRIVATE int PH7_VmSigDefaultToValue(ph7_context *pCtx,const char *z,int n,ph7_value *pOut); /* vm_builtin_reflection.c */` |
|         - | 8136 | `PH7_PRIVATE int PH7_VmArgRefusedByRef(VmCallArgMap *pMap,sxu32 nPos,ph7_value *pVal);` |
|         - | 8137 | `PH7_PRIVATE sxi32 PH7_VmResolveDeferredArgs(ph7_vm *pVm,ph7_value *pArg,ph7_value *pTos,` |
|         - | 8138 | `	ph7_vm_func_arg *pFormal,sxu32 nFormal,sxu32 nByRefMask,int bAllByRef,int bAllByValue,` |
|         - | 8139 | `	VmCallArgMap *pCallMap);` |
|         - | 8140 | `PH7_PRIVATE void PH7_VmArgTempCallNotice(ph7_vm *pVm,VmCallArgMap *pMap,sxu32 nPos,ph7_value *pVal);` |
|         - | 8141 | `PH7_PRIVATE int PH7_ArgSatisfiesString(ph7_value *pArg);` |
|         - | 8142 | `PH7_PRIVATE int PH7_ArgIsUnstringableObject(ph7_value *pArg);` |
|         - | 8143 | `PH7_PRIVATE void VmDeprecatedAttrNotice(ph7_vm *pVm,ph7_vm_func *pFunc,ph7_class *pDeclClass);` |
|         - | 8144 | `PH7_PRIVATE void PH7_MarkDeprecatedFunctions(ph7_vm *pVm);` |
|         - | 8145 | `PH7_PRIVATE void PH7_VmDeprecatedCallNotice(ph7_vm *pVm,const ph7_deprecated_name *pDep);` |
|         - | 8146 | `/* vm_exec_ctx.c — Fiber/Generator/Closure engine shared with vm.c */` |
|         - | 8147 | `PH7_PRIVATE sxi32 PH7_VmInstallClosureNative(ph7_vm *pVm);` |
|         - | 8148 | `PH7_PRIVATE sxi32 PH7_VmInstallFiberNative(ph7_vm *pVm);` |
|         - | 8149 | `PH7_PRIVATE sxi32 PH7_VmInstallGeneratorNative(ph7_vm *pVm);` |
|         - | 8150 | `PH7_PRIVATE void VmStampCoroutineCallSite(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|         - | 8151 | `PH7_PRIVATE int vm_builtin_Closure_bindTo(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 8152 | `PH7_PRIVATE int vm_builtin_Closure_call(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 8153 | `PH7_PRIVATE int vm_builtin_Closure_construct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 8154 | `PH7_PRIVATE int vm_builtin_Closure_invoke(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 8155 | `PH7_PRIVATE int vm_builtin_Closure_fromCallable(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 8156 | `PH7_PRIVATE int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 8157 | `PH7_PRIVATE int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 8158 | `PH7_PRIVATE int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 8159 | `PH7_PRIVATE int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 8160 | `PH7_PRIVATE int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 8161 | `PH7_PRIVATE int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 8162 | `PH7_PRIVATE int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 8163 | `PH7_PRIVATE int vm_builtin_Fiber_getCurrent(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 8164 | `PH7_PRIVATE int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 8165 | `PH7_PRIVATE int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 8166 | `PH7_PRIVATE int vm_builtin_Fiber_throw(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 8167 | `PH7_PRIVATE int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 8168 | `PH7_PRIVATE int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 8169 | `PH7_PRIVATE int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 8170 | `PH7_PRIVATE int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 8171 | `PH7_PRIVATE int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 8172 | `PH7_PRIVATE int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 8173 | `PH7_PRIVATE int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 8174 | `PH7_PRIVATE int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 8175 | `PH7_PRIVATE int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 8176 | `PH7_PRIVATE int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 8177 | `PH7_PRIVATE ph7_class_instance * VmCreateClosure(ph7_vm *pVm, const SyString *pName, ph7_class_instance *pBoundThis, const SyString *pScope);` |
|         - | 8178 | `PH7_PRIVATE ph7_class * VmFccResolveScope(ph7_vm *pVm, ph7_value *pTarget);` |
|         - | 8179 | `PH7_PRIVATE ph7_class * PH7_VmResolveScopeName(ph7_vm *pVm, const char *zCls, sxu32 nCls);` |
|         - | 8180 | `PH7_PRIVATE int PH7_VmIsScopeKeyword(const char *zName,sxu32 nName);` |
|         - | 8181 | `PH7_PRIVATE ph7_class * PH7_VmResolveCallableScope(ph7_vm *pVm,const char *zCls,sxu32 nCls);` |
|         - | 8182 | `PH7_PRIVATE ph7_class_instance * VmFccWrapValue(ph7_vm *pVm, ph7_value *pValue, int bBindCaller);` |
|         - | 8183 | `PH7_PRIVATE void PH7_VmBindCallbackScope(ph7_vm *pVm, ph7_value *pCallback, ph7_value *pOut);` |
|         - | 8184 | `PH7_PRIVATE void PH7_VmByRefArgWriteBack(ph7_vm *pVm,ph7_value *pArg,sxi32 iPreFlags);` |
|         - | 8185 | `PH7_PRIVATE sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx, ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg, const SyString *aArgName, int bStrict, ph7_class *pSelfHint, int bCallSiteInMsg, int bAliasByRef);` |
|         - | 8186 | `PH7_PRIVATE sxi32 VmCtxBindNamedArgs(ph7_vm *pVm, ph7_vm_func *pFunc, VmCallArgMap *pMap, sxu32 nActual, ph7_value **apIn, ph7_value **apOut, SyString *aOutName, int *pnOut, sxi32 *piHole);` |
|         - | 8187 | `PH7_PRIVATE sxi32 VmCtxThrowNamedHole(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_class *pSelfHint, sxi32 iHole);` |
|         - | 8188 | `PH7_PRIVATE ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj);` |
|         - | 8189 | `PH7_PRIVATE int PH7_VmGeneratorIsClosed(ph7_vm *pVm, ph7_class_instance *pThis);` |
|         - | 8190 | `PH7_PRIVATE sxi32 PH7_VmGeneratorPrime(ph7_vm *pVm, ph7_class_instance *pThis);` |
|         - | 8191 | `PH7_PRIVATE ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc);` |
|         - | 8192 | `PH7_PRIVATE ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|         - | 8193 | `PH7_PRIVATE void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|         - | 8194 | `PH7_PRIVATE void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen);` |
|         - | 8195 | `PH7_PRIVATE sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult);` |
|         - | 8196 | `/* vm_include.c function prototypes (rows stay in vm.c's aVmFunc[]) */` |
|         - | 8197 | `PH7_PRIVATE sxi32 VmMountUserClass(ph7_vm *pVm,ph7_class *pClass);` |
|         - | 8198 | `PH7_PRIVATE sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);` |
|         - | 8199 | `PH7_PRIVATE SyString * PH7_VmExecutingUnitFile(ph7_vm *pVm);` |
|         - | 8200 | `PH7_PRIVATE ph7_vm_func * PH7_VmPreludeBuiltinFrame(ph7_vm *pVm,SyString **ppFile,sxu32 *pnLine);` |
|         - | 8201 | `PH7_PRIVATE const char * PH7_CtxDiagFuncName(ph7_context *pCtx,char *zBuf,int nBuf);` |
|         - | 8202 | `PH7_PRIVATE void PH7_VmIncFramePush(ph7_vm *pVm,const char *zName,SyString *pPath);` |
|         - | 8203 | `PH7_PRIVATE void PH7_VmIncFramePop(ph7_vm *pVm);` |
|         - | 8204 | `PH7_PRIVATE sxi32 VmExecDeferredClass(ph7_vm *pVm,VmDeferredClass *pDefer,VmDeferredReq **ppMissing);` |
|         - | 8205 | `PH7_PRIVATE ph7_user_func * PH7_VmMagicCallFunc(ph7_vm *pVm);` |
|         - | 8206 | `PH7_PRIVATE int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8207 | `PH7_PRIVATE int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8208 | `PH7_PRIVATE int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8209 | `PH7_PRIVATE int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8210 | `PH7_PRIVATE int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8211 | `PH7_PRIVATE int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8212 | `PH7_PRIVATE int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8213 | `PH7_PRIVATE int vm_builtin_set_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8214 | `PH7_PRIVATE int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8215 | `PH7_PRIVATE int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8216 | `PH7_PRIVATE int vm_builtin_spl_autoload_extensions(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8217 | `PH7_PRIVATE int vm_builtin_spl_autoload_call(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8218 | `PH7_PRIVATE int vm_builtin_spl_classes(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8219 | `PH7_PRIVATE int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8220 | `PH7_PRIVATE int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8221 | `/* vm_builtin_call.c — callable machinery shared with vm.c's interpreter */` |
|         - | 8222 | `PH7_PRIVATE ph7_class * PH7_VmResolveParentClass(ph7_vm *pVm);` |
|         - | 8223 | `PH7_PRIVATE const char * PH7_VmScopeKeywordRefusal(ph7_vm *pVm,const char *zCls,sxu32 nCls,` |
|         - | 8224 | `	int bClassName,char *zBuf,int nBuf);` |
|         - | 8225 | `PH7_PRIVATE void VmBoundaryPark(ph7_vm *pVm,sxi32 rc);` |
|         - | 8226 | `/*` |
|         - | 8227 | ` * The status a C->PHP dispatch answers when the callee did NOT return: the` |
|         - | 8228 | ` * builtin driving the loop must abandon it and hand the status straight out.` |
|         - | 8229 | ` * Both members matter and testing only the first is a silent wrong answer —` |
|         - | 8230 | ` * php stops an internal function the moment its callback throws, and an` |
|         - | 8231 | ` * UNCAUGHT throw comes back as PH7_ABORT (VmUncaughtException reports the` |
|         - | 8232 | ` * fatal and answers SXERR_ABORT), not as PH7_EXCEPTION. A loop that tested` |
|         - | 8233 | ` * only PH7_EXCEPTION therefore ran the callback again for every remaining` |
|         - | 8234 | ` * element — repeating its side effects and re-reporting the fatal once per` |
|         - | 8235 | ` * element. Same set VmBoundaryPark parks; see its comment.` |
|         - | 8236 | ` */` |
|         - | 8237 | `#define PH7_CALLBACK_UNWOUND(rc) ((rc) == PH7_EXCEPTION \|\| (rc) == PH7_ABORT)` |
|         - | 8238 | `PH7_PRIVATE sxi32 PH7_VmCallCallbackByValue(ph7_vm *pVm,ph7_value *pFunc,int nArg,` |
|         - | 8239 | `	ph7_value **apArg,ph7_value *pResult,sxu32 nRefOkMask);` |
|         - | 8240 | `PH7_PRIVATE sxi32 VmIterCallMethod(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nLen,ph7_value *pResult);` |
|         - | 8241 | `PH7_PRIVATE sxi32 VmCallClassMethodLsb(ph7_vm *pVm,ph7_class *pCalled,ph7_class_instance *pThis,` |
|         - | 8242 | `	ph7_class_method *pMethod,ph7_value *pResult,int nArg,ph7_value **apArg,VmCallArgMap *pMap);` |
|         - | 8243 | `PH7_PRIVATE sxi32 VmCallClassMethodWithMap(ph7_vm *pVm,ph7_class_instance *pThis,` |
|         - | 8244 | `	ph7_class_method *pMethod,ph7_value *pResult,int nArg,` |
|         - | 8245 | `	ph7_value **apArg,VmCallArgMap *pMap);` |
|         - | 8246 | `PH7_PRIVATE sxi32 VmCallObjectInvoke(ph7_vm *pVm,ph7_class_instance *pThis,` |
|         - | 8247 | `	int nArg,ph7_value **apArg,ph7_value *pResult,VmCallArgMap *pMap);` |
|         - | 8248 | `PH7_PRIVATE sxi32 VmRaiseNotCallable(ph7_vm *pVm,ph7_class_instance *pThis);` |
|         - | 8249 | `PH7_PRIVATE sxi32 PH7_VmForbidDynamicCall(ph7_context *pCtx);` |
|         - | 8250 | `PH7_PRIVATE int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8251 | `PH7_PRIVATE int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8252 | `PH7_PRIVATE int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8253 | `PH7_PRIVATE int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8254 | `PH7_PRIVATE int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8255 | `PH7_PRIVATE int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8256 | `PH7_PRIVATE int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8257 | `PH7_PRIVATE int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8258 | `/* builtin.c function prototypes */` |
|         - | 8259 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm);` |
|         - | 8260 | `/* builtin_hash.c function prototypes */` |
|         - | 8261 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 8262 | `/* Binary-to-hex consumer shared by bin2hex() (builtin.c) and the hash` |
|         - | 8263 | ` * builtins (builtin_hash.c). */` |
|         - | 8264 | `PH7_PRIVATE int HashConsumer(const void *pData,unsigned int nLen,void *pUserData);` |
|         - | 8265 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|         - | 8266 | `PH7_PRIVATE int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8267 | `PH7_PRIVATE int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8268 | `PH7_PRIVATE int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8269 | `PH7_PRIVATE int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8270 | `PH7_PRIVATE int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8271 | `PH7_PRIVATE int PH7_builtin_hash_hmac_algos(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8272 | `PH7_PRIVATE int PH7_builtin_hash_init(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8273 | `PH7_PRIVATE int PH7_builtin_hash_update(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8274 | `PH7_PRIVATE int PH7_builtin_hash_final(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8275 | `PH7_PRIVATE int PH7_builtin_hash_copy(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8276 | `PH7_PRIVATE int PH7_builtin_hash_pbkdf2(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8277 | `PH7_PRIVATE int PH7_builtin_hash_hkdf(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8278 | `PH7_PRIVATE int PH7_builtin_hash_file(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8279 | `PH7_PRIVATE int PH7_builtin_hash_hmac_file(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8280 | `PH7_PRIVATE int PH7_builtin_hash_update_file(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8281 | `PH7_PRIVATE int PH7_builtin_hash_update_stream(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8282 | `PH7_PRIVATE sxi32 PH7_VmInstallHashContext(ph7_vm *pVm);` |
|         - | 8283 | `/* php's one hash_init() flag: request an HMAC rather than a plain digest. */` |
|         - | 8284 | `#define PH7_HASH_HMAC 1` |
|         - | 8285 | `PH7_PRIVATE int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8286 | `PH7_PRIVATE int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8287 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|         - | 8288 | `PH7_PRIVATE int PH7_builtin_crypt(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8289 | `PH7_PRIVATE int PH7_builtin_password_algos(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8290 | `PH7_PRIVATE int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8291 | `PH7_PRIVATE int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8292 | `PH7_PRIVATE int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8293 | `PH7_PRIVATE int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8294 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 8295 | `/* builtin_fmt.c function prototypes (PH7_NEED_FMT_AND_INI: compiled whenever` |
|         - | 8296 | ` * disk I/O is enabled, independently of PH7_DISABLE_BUILTIN_FUNC) */` |
|         - | 8297 | `#ifndef PH7_DISABLE_DISK_IO` |
|         - | 8298 | `PH7_PRIVATE int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8299 | `PH7_PRIVATE int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8300 | `PH7_PRIVATE int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8301 | `PH7_PRIVATE int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8302 | `#endif /* PH7_DISABLE_DISK_IO */` |
|         - | 8303 | `#include <signal.h>   /* sig_atomic_t: PH7_PcntlAsyncPending, read at the fetch point */` |
|         - | 8304 | `/*` |
|         - | 8305 | ` * ext/pcntl (builtin_pcntl.c). These four are OUTSIDE every guard because the` |
|         - | 8306 | ` * engine links against them in every build: vm_exec.c reads the flag at its` |
|         - | 8307 | ` * fetch point, vm.c registers the constants and releases the state. Where the` |
|         - | 8308 | ` * extension is not compiled in -- Windows, or a build with no builtins -- the` |
|         - | 8309 | ` * flag is simply always zero and the three functions are no-ops.` |
|         - | 8310 | ` */` |
|         - | 8311 | `extern volatile sig_atomic_t PH7_PcntlAsyncPending;` |
|         - | 8312 | `PH7_PRIVATE void PH7_PcntlDrainAsync(ph7_vm *pVm);` |
|         - | 8313 | `PH7_PRIVATE void PH7_PcntlVmRelease(ph7_vm *pVm);` |
|         - | 8314 | `PH7_PRIVATE void PH7_RegisterPcntlConstants(ph7_vm *pVm);` |
|         - | 8315 | `PH7_PRIVATE sxi32 PH7_VmInstallPcntl(ph7_vm *pVm);` |
|         - | 8316 | ``/* Its uncatchable `Error installing signal handler for %d`, which lives with the`` |
|         - | 8317 | ` * engine's other clean-halt fatals in vm_error.c rather than with the extension. */` |
|         - | 8318 | `PH7_PRIVATE sxi32 PH7_VmSignalInstallFatal(ph7_vm *pVm,int signo);` |
|         - | 8319 | `#if defined(PH7_ENABLE_THREADS)` |
|         - | 8320 | `/* Drop this process to single-threaded mode in the CHILD of a fork() (api.c).` |
|         - | 8321 | ` * Declared beside pcntl's names because pcntl_fork() is its only caller, but it` |
|         - | 8322 | ` * belongs to the library core and is built wherever threads are. */` |
|         - | 8323 | `PH7_PRIVATE void PH7_LibForkChild(void);` |
|         - | 8324 | `#endif` |
|         - | 8325 | `/* php's syslog trio (builtin_syslog.c). Not an extension -- ext/standard, and` |
|         - | 8326 | ` * therefore present on every platform php is. */` |
|         - | 8327 | `PH7_PRIVATE void PH7_RegisterSyslogConstants(ph7_vm *pVm);` |
|         - | 8328 | `PH7_PRIVATE void PH7_SyslogVmRelease(ph7_vm *pVm);` |
|         - | 8329 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 8330 | `PH7_PRIVATE int PH7_builtin_openlog(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8331 | `PH7_PRIVATE int PH7_builtin_syslog(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8332 | `PH7_PRIVATE int PH7_builtin_closelog(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8333 | `#endif` |
|         - | 8334 | `/* builtin_parse.c function prototypes */` |
|         - | 8335 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 8336 | `/* HTML entity escape engine: shared by the htmlspecialchars/htmlentities` |
|         - | 8337 | ` * family (builtin.c) and filter_var's SANITIZE filters (builtin_parse.c). */` |
|         - | 8338 | `/* The charsets the HTML entity family models: php's own UTF-8 and ISO-8859-1` |
|         - | 8339 | ` * (one byte per character, its VALUE the code point). Everything else keeps` |
|         - | 8340 | ` * php's unsupported-charset warning. */` |
|         - | 8341 | `#define PH7_HTML_CS_UTF8   0` |
|         - | 8342 | `#define PH7_HTML_CS_LATIN1 1` |
|         - | 8343 | `PH7_PRIVATE void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,int iFlags,int bAll,int bDoubleEncode,int iCs);` |
|         - | 8344 | `PH7_PRIVATE void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,int iFlags,int bFull,int iCs);` |
|         - | 8345 | `PH7_PRIVATE int HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx);` |
|         - | 8346 | `PH7_PRIVATE void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags,int iCs);` |
|         - | 8347 | `PH7_PRIVATE int PH7_builtin_filter_var(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8348 | `PH7_PRIVATE int PH7_builtin_filter_input(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8349 | `PH7_PRIVATE int PH7_builtin_filter_list(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8350 | `PH7_PRIVATE int PH7_builtin_filter_id(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8351 | `PH7_PRIVATE int PH7_builtin_filter_has_var(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8352 | `PH7_PRIVATE int PH7_builtin_filter_var_array(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8353 | `PH7_PRIVATE int PH7_builtin_filter_input_array(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8354 | `PH7_PRIVATE int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8355 | `PH7_PRIVATE int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8356 | `PH7_PRIVATE int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8357 | `PH7_PRIVATE int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8358 | `PH7_PRIVATE int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8359 | `PH7_PRIVATE int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8360 | `PH7_PRIVATE int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8361 | `PH7_PRIVATE int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8362 | `PH7_PRIVATE int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8363 | `PH7_PRIVATE int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8364 | `PH7_PRIVATE int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8365 | `PH7_PRIVATE int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8366 | `PH7_PRIVATE int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8367 | `PH7_PRIVATE int PH7_builtin_convert_uuencode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8368 | `PH7_PRIVATE int PH7_builtin_convert_uudecode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8369 | `PH7_PRIVATE int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8370 | `PH7_PRIVATE int PH7_builtin_rawurlencode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8371 | `PH7_PRIVATE int PH7_builtin_http_build_query(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8372 | `PH7_PRIVATE int PH7_builtin_parse_str(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8373 | `PH7_PRIVATE int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8374 | `PH7_PRIVATE int PH7_builtin_rawurldecode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8375 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 8376 | `#ifndef PH7_DISABLE_DISK_IO` |
|         - | 8377 | `PH7_PRIVATE int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8378 | `PH7_PRIVATE int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8379 | `PH7_PRIVATE int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8380 | `#endif /* PH7_DISABLE_DISK_IO */` |
|         - | 8381 | `/* builtin_string.c function prototypes */` |
|         - | 8382 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 8383 | `PH7_PRIVATE int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8384 | `PH7_PRIVATE int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8385 | `PH7_PRIVATE int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8386 | `PH7_PRIVATE int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8387 | `PH7_PRIVATE int PH7_builtin_chunk_split(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8388 | `PH7_PRIVATE int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8389 | `PH7_PRIVATE int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8390 | `PH7_PRIVATE int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8391 | `PH7_PRIVATE int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8392 | `PH7_PRIVATE int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8393 | `PH7_PRIVATE int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8394 | `PH7_PRIVATE int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8395 | `PH7_PRIVATE int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8396 | `PH7_PRIVATE int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8397 | `PH7_PRIVATE int PH7_builtin_levenshtein(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8398 | `PH7_PRIVATE int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8399 | `PH7_PRIVATE int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8400 | `PH7_PRIVATE int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8401 | `PH7_PRIVATE int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8402 | `PH7_PRIVATE int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8403 | `PH7_PRIVATE int PH7_builtin_similar_text(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8404 | `PH7_PRIVATE int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8405 | `PH7_PRIVATE int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8406 | `PH7_PRIVATE int PH7_builtin_str_rot13(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8407 | `PH7_PRIVATE int PH7_builtin_metaphone(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8408 | `/* builtin_pack.c — the binary-string pair */` |
|         - | 8409 | `PH7_PRIVATE int PH7_builtin_pack(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8410 | `PH7_PRIVATE int PH7_builtin_unpack(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8411 | `/* builtin_bcmath.c -- ext/bcmath: arbitrary-precision decimal arithmetic */` |
|         - | 8412 | `PH7_PRIVATE int PH7_builtin_bcadd(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8413 | `PH7_PRIVATE int PH7_builtin_bcsub(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8414 | `PH7_PRIVATE int PH7_builtin_bcmul(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8415 | `PH7_PRIVATE int PH7_builtin_bccomp(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8416 | `PH7_PRIVATE int PH7_builtin_bcdiv(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8417 | `PH7_PRIVATE int PH7_builtin_bcmod(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8418 | `PH7_PRIVATE int PH7_builtin_bcdivmod(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8419 | `PH7_PRIVATE int PH7_builtin_bcpow(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8420 | `PH7_PRIVATE int PH7_builtin_bcpowmod(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8421 | `PH7_PRIVATE int PH7_builtin_bcsqrt(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8422 | `PH7_PRIVATE int PH7_builtin_bcround(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8423 | `PH7_PRIVATE int PH7_builtin_bcfloor(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8424 | `PH7_PRIVATE int PH7_builtin_bcceil(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8425 | `PH7_PRIVATE sxi32 PH7_VmInstallBcMath(ph7_vm *pVm);` |
|         - | 8426 | `PH7_PRIVATE int PH7_builtin_bcscale(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8427 | `/* builtin_calendar.c -- ext/calendar: the serial day number and its calendars */` |
|         - | 8428 | `PH7_PRIVATE int PH7_builtin_gregoriantojd(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8429 | `PH7_PRIVATE int PH7_builtin_jdtogregorian(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8430 | `PH7_PRIVATE int PH7_builtin_juliantojd(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8431 | `PH7_PRIVATE int PH7_builtin_jdtojulian(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8432 | `PH7_PRIVATE int PH7_builtin_frenchtojd(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8433 | `PH7_PRIVATE int PH7_builtin_jdtofrench(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8434 | `PH7_PRIVATE int PH7_builtin_jewishtojd(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8435 | `PH7_PRIVATE int PH7_builtin_jdtojewish(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8436 | `PH7_PRIVATE int PH7_builtin_cal_info(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8437 | `/* posix (builtin_posix.c) -- php builds no ext/posix on Windows, and neither` |
|         - | 8438 | ` * does this, so every name below is absent there. */` |
|         - | 8439 | `#ifndef __WINNT__` |
|         - | 8440 | `/* ext/pcntl's own builtins (builtin_pcntl.c); the names the engine links against` |
|         - | 8441 | ` * in EVERY build are declared above, outside both guards. */` |
|         - | 8442 | `PH7_PRIVATE int PH7_builtin_pcntl_fork(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8443 | `PH7_PRIVATE int PH7_builtin_pcntl_waitpid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8444 | `PH7_PRIVATE int PH7_builtin_pcntl_waitid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8445 | `PH7_PRIVATE int PH7_builtin_pcntl_wait(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8446 | `PH7_PRIVATE int PH7_builtin_pcntl_signal(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8447 | `PH7_PRIVATE int PH7_builtin_pcntl_signal_get_handler(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8448 | `PH7_PRIVATE int PH7_builtin_pcntl_signal_dispatch(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8449 | `PH7_PRIVATE int PH7_builtin_pcntl_sigprocmask(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8450 | `PH7_PRIVATE int PH7_builtin_pcntl_sigwaitinfo(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8451 | `PH7_PRIVATE int PH7_builtin_pcntl_sigtimedwait(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8452 | `PH7_PRIVATE int PH7_builtin_pcntl_wifexited(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8453 | `PH7_PRIVATE int PH7_builtin_pcntl_wifstopped(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8454 | `PH7_PRIVATE int PH7_builtin_pcntl_wifcontinued(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8455 | `PH7_PRIVATE int PH7_builtin_pcntl_wifsignaled(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8456 | `PH7_PRIVATE int PH7_builtin_pcntl_wexitstatus(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8457 | `PH7_PRIVATE int PH7_builtin_pcntl_wtermsig(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8458 | `PH7_PRIVATE int PH7_builtin_pcntl_wstopsig(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8459 | `PH7_PRIVATE int PH7_builtin_pcntl_exec(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8460 | `PH7_PRIVATE int PH7_builtin_pcntl_alarm(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8461 | `PH7_PRIVATE int PH7_builtin_pcntl_get_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8462 | `PH7_PRIVATE int PH7_builtin_pcntl_getpriority(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8463 | `PH7_PRIVATE int PH7_builtin_pcntl_setpriority(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8464 | `PH7_PRIVATE int PH7_builtin_pcntl_strerror(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8465 | `PH7_PRIVATE int PH7_builtin_pcntl_async_signals(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8466 | `#ifdef __linux__` |
|         - | 8467 | `PH7_PRIVATE int PH7_builtin_pcntl_unshare(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8468 | `PH7_PRIVATE int PH7_builtin_pcntl_getcpuaffinity(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8469 | `PH7_PRIVATE int PH7_builtin_pcntl_setcpuaffinity(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8470 | `PH7_PRIVATE int PH7_builtin_pcntl_getcpu(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8471 | `#endif` |
|         - | 8472 | `PH7_PRIVATE int PH7_builtin_posix_kill(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8473 | `PH7_PRIVATE int PH7_builtin_posix_getpid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8474 | `PH7_PRIVATE int PH7_builtin_posix_getppid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8475 | `PH7_PRIVATE int PH7_builtin_posix_getuid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8476 | `PH7_PRIVATE int PH7_builtin_posix_setuid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8477 | `PH7_PRIVATE int PH7_builtin_posix_geteuid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8478 | `PH7_PRIVATE int PH7_builtin_posix_seteuid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8479 | `PH7_PRIVATE int PH7_builtin_posix_getgid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8480 | `PH7_PRIVATE int PH7_builtin_posix_setgid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8481 | `PH7_PRIVATE int PH7_builtin_posix_getegid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8482 | `PH7_PRIVATE int PH7_builtin_posix_setegid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8483 | `PH7_PRIVATE int PH7_builtin_posix_getgroups(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8484 | `PH7_PRIVATE int PH7_builtin_posix_getlogin(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8485 | `PH7_PRIVATE int PH7_builtin_posix_getpgrp(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8486 | `PH7_PRIVATE int PH7_builtin_posix_setsid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8487 | `PH7_PRIVATE int PH7_builtin_posix_setpgid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8488 | `PH7_PRIVATE int PH7_builtin_posix_getpgid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8489 | `PH7_PRIVATE int PH7_builtin_posix_getsid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8490 | `PH7_PRIVATE int PH7_builtin_posix_uname(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8491 | `PH7_PRIVATE int PH7_builtin_posix_times(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8492 | `PH7_PRIVATE int PH7_builtin_posix_ctermid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8493 | `PH7_PRIVATE int PH7_builtin_posix_ttyname(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8494 | `PH7_PRIVATE int PH7_builtin_posix_isatty(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8495 | `PH7_PRIVATE int PH7_builtin_posix_getcwd(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8496 | `PH7_PRIVATE int PH7_builtin_posix_mkfifo(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8497 | `PH7_PRIVATE int PH7_builtin_posix_mknod(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8498 | `PH7_PRIVATE int PH7_builtin_posix_access(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8499 | `PH7_PRIVATE int PH7_builtin_posix_eaccess(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8500 | `PH7_PRIVATE int PH7_builtin_posix_getgrnam(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8501 | `PH7_PRIVATE int PH7_builtin_posix_getgrgid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8502 | `PH7_PRIVATE int PH7_builtin_posix_getpwnam(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8503 | `PH7_PRIVATE int PH7_builtin_posix_getpwuid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8504 | `PH7_PRIVATE int PH7_builtin_posix_getrlimit(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8505 | `PH7_PRIVATE int PH7_builtin_posix_setrlimit(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8506 | `PH7_PRIVATE int PH7_builtin_posix_get_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8507 | `PH7_PRIVATE int PH7_builtin_posix_strerror(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8508 | `PH7_PRIVATE int PH7_builtin_posix_initgroups(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8509 | `PH7_PRIVATE int PH7_builtin_posix_sysconf(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8510 | `PH7_PRIVATE int PH7_builtin_posix_pathconf(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8511 | `PH7_PRIVATE int PH7_builtin_posix_fpathconf(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8512 | `#endif /* __WINNT__ */` |
|         - | 8513 | `/* fileinfo (builtin_fileinfo.c) -- php's ext/fileinfo, over PHL's own signature` |
|         - | 8514 | ` * table rather than a magic database file. */` |
|         - | 8515 | `#ifdef PH7_ENABLE_ZLIB` |
|         - | 8516 | `/* vm_openssl.c / vm_openssl_x509.c */` |
|         - | 8517 | `PH7_PRIVATE sxi32 PH7_VmInstallOpenSsl(ph7_vm *pVm);` |
|         - | 8518 | `PH7_PRIVATE const ph7_builtin_func * PH7_OpenSslFuncTable(sxu32 *pnEntry);` |
|         - | 8519 | `PH7_PRIVATE sxi32 PH7_VmInstallOpenSslX509(ph7_vm *pVm);` |
|         - | 8520 | `PH7_PRIVATE const ph7_builtin_func * PH7_OpenSslX509FuncTable(sxu32 *pnEntry);` |
|         - | 8521 | `PH7_PRIVATE void PH7_SslVmReset(ph7_vm *pVm);` |
|         - | 8522 | `PH7_PRIVATE void PH7_SslVmRelease(ph7_vm *pVm);` |
|         - | 8523 | `/* ext/zip: the ZipArchive class, its ten deprecated procedural verbs and the` |
|         - | 8524 | `` * read-only `zip://` wrapper. It rides ext/zlib's build guard because php's own`` |
|         - | 8525 | ` * requires zlib -- a deflated member is the format's normal case. */` |
|         - | 8526 | `#ifdef PH7_ENABLE_ZLIB` |
|         - | 8527 | `PH7_PRIVATE sxi32 PH7_VmInstallZip(ph7_vm *pVm);` |
|         - | 8528 | `PH7_PRIVATE const ph7_builtin_func * PH7_ZipFuncTable(sxu32 *pnEntry);` |
|         - | 8529 | `PH7_PRIVATE int PH7_ZipStreamIs(const ph7_io_stream *pStream);` |
|         - | 8530 | `PH7_PRIVATE int PH7_ZipStreamViaWrapper(void *pHandle);` |
|         - | 8531 | `#endif` |
|         - | 8532 | `PH7_PRIVATE void PH7_ZipVmReset(ph7_vm *pVm);` |
|         - | 8533 | `PH7_PRIVATE void PH7_ZipVmRelease(ph7_vm *pVm);` |
|         - | 8534 | `/* The name get_resource_type() gives one of ext/zip's two procedural handles,` |
|         - | 8535 | ` * or 0 when the resource is not one of them. */` |
|         - | 8536 | `PH7_PRIVATE const char * PH7_ZipResourceType(void *pResource);` |
|         - | 8537 | `PH7_PRIVATE sxi32 PH7_VmInstallZlib(ph7_vm *pVm);` |
|         - | 8538 | `PH7_PRIVATE const ph7_builtin_func * PH7_ZlibFuncTable(sxu32 *pnEntry);` |
|         - | 8539 | `PH7_PRIVATE void PH7_ZlibVmReset(ph7_vm *pVm);` |
|         - | 8540 | `PH7_PRIVATE void PH7_ZlibVmRelease(ph7_vm *pVm);` |
|         - | 8541 | `PH7_PRIVATE void PH7_ZlibArmOpen(ph7_vm *pVm,int iLevel,int iStrategy);` |
|         - | 8542 | `PH7_PRIVATE void PH7_ZlibArmDirect(ph7_vm *pVm,int bDirect);` |
|         - | 8543 | `PH7_PRIVATE int PH7_ZlibStreamIs(const ph7_io_stream *pStream);` |
|         - | 8544 | `PH7_PRIVATE int PH7_ZlibFilterCreate(phl_stream_filter *pFilter,ph7_value *pParams);` |
|         - | 8545 | `PH7_PRIVATE int PH7_ZlibFilterRun(phl_stream_filter *pFilter,phl_brigade *pIn,` |
|         - | 8546 | `	phl_brigade *pOut,int iFlags);` |
|         - | 8547 | `PH7_PRIVATE void PH7_ZlibFilterClose(phl_stream_filter *pFilter);` |
|         - | 8548 | `extern const ph7_io_stream sZLIB_Stream;` |
|         - | 8549 | `extern const ph7_io_stream sZIP_Stream;` |
|         - | 8550 | `#endif` |
|         - | 8551 | `PH7_PRIVATE void PH7_VmAddResponseHeader(ph7_vm *pVm,const char *zName,const char *zValue);` |
|         - | 8552 | `PH7_PRIVATE sxi32 PH7_VmInstallPhar(ph7_vm *pVm);` |
|         - | 8553 | `PH7_PRIVATE int PH7_PharStreamIs(const ph7_io_stream *pStream);` |
|         - | 8554 | `PH7_PRIVATE int PH7_PharUrlStat(ph7_vm *pVm,const char *zPath,ph7_int64 *aVal);` |
|         - | 8555 | `PH7_PRIVATE int PH7_PharCanonicalUrl(ph7_vm *pVm,const char *zPath,int nPath,SyBlob *pOut);` |
|         - | 8556 | `/* What PH7_PharPathOp() was asked to do. Mirrors vfs.c's VFS_POP_* codes, which` |
|         - | 8557 | ` * are file-local. */` |
|         - | 8558 | `#define PHAR_PATHOP_UNLINK 0` |
|         - | 8559 | `#define PHAR_PATHOP_RENAME 1` |
|         - | 8560 | `#define PHAR_PATHOP_MKDIR  2` |
|         - | 8561 | `#define PHAR_PATHOP_CHMOD  3` |
|         - | 8562 | `#define PHAR_PATHOP_RMDIR  4` |
|         - | 8563 | `PH7_PRIVATE int PH7_PharPathOp(ph7_context *pCtx,const char *zPath,const char *zDest,int eOp);` |
|         - | 8564 | `PH7_PRIVATE int PH7_VmSerializeValue(ph7_context *pCtx,ph7_value *pIn,SyBlob *pOut);` |
|         - | 8565 | `extern const ph7_io_stream sPHAR_Stream;` |
|         - | 8566 | `PH7_PRIVATE sxi32 PH7_VmInstallFileinfo(ph7_vm *pVm);` |
|         - | 8567 | `PH7_PRIVATE int PH7_builtin_finfo_open(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8568 | `PH7_PRIVATE int PH7_builtin_finfo_close(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8569 | `PH7_PRIVATE int PH7_builtin_finfo_set_flags(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8570 | `PH7_PRIVATE int PH7_builtin_finfo_file(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8571 | `PH7_PRIVATE int PH7_builtin_finfo_buffer(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8572 | `PH7_PRIVATE int PH7_builtin_mime_content_type(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8573 | `/* gettext (builtin_gettext.c) */` |
|         - | 8574 | `PH7_PRIVATE int PH7_builtin_textdomain(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8575 | `PH7_PRIVATE int PH7_builtin_bindtextdomain(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8576 | `PH7_PRIVATE int PH7_builtin_bind_textdomain_codeset(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8577 | `PH7_PRIVATE int PH7_builtin_gettext(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8578 | `PH7_PRIVATE int PH7_builtin_dgettext(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8579 | `PH7_PRIVATE int PH7_builtin_dcgettext(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8580 | `PH7_PRIVATE int PH7_builtin_ngettext(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8581 | `PH7_PRIVATE int PH7_builtin_dngettext(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8582 | `PH7_PRIVATE int PH7_builtin_dcngettext(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8583 | `PH7_PRIVATE int PH7_builtin_cal_days_in_month(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8584 | `PH7_PRIVATE int PH7_builtin_cal_to_jd(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8585 | `PH7_PRIVATE int PH7_builtin_cal_from_jd(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8586 | `PH7_PRIVATE int PH7_builtin_jddayofweek(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8587 | `PH7_PRIVATE int PH7_builtin_jdmonthname(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8588 | `PH7_PRIVATE int PH7_builtin_unixtojd(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8589 | `PH7_PRIVATE int PH7_builtin_jdtounix(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8590 | `PH7_PRIVATE int PH7_builtin_easter_days(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8591 | `PH7_PRIVATE int PH7_builtin_easter_date(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8592 | `/* builtin_image.c -- ext/standard's image surface: the IMAGETYPE_* space and` |
|         - | 8593 | ` * the container readers behind getimagesize(). */` |
|         - | 8594 | `PH7_PRIVATE const char * PH7_ImageTypeMime(ph7_int64 iType);` |
|         - | 8595 | `PH7_PRIVATE int PH7_builtin_image_type_to_mime_type(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8596 | `PH7_PRIVATE int PH7_builtin_image_type_to_extension(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8597 | `PH7_PRIVATE int PH7_builtin_getimagesize(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8598 | `PH7_PRIVATE int PH7_builtin_getimagesizefromstring(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8599 | `/* builtin_scanf.c -- sscanf() and the scanner fscanf() (vfs_stream.c) shares. */` |
|         - | 8600 | `PH7_PRIVATE sxi32 PH7_ScanfRun(ph7_context *pCtx,const char *zStr,int nStr,` |
|         - | 8601 | `	const char *zFmt,int nFmt,ph7_value **apVar,int nVar);` |
|         - | 8602 | `PH7_PRIVATE int PH7_builtin_sscanf(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8603 | `PH7_PRIVATE int PH7_builtin_fscanf(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8604 | `PH7_PRIVATE int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8605 | `PH7_PRIVATE int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8606 | `PH7_PRIVATE int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8607 | `PH7_PRIVATE int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8608 | `PH7_PRIVATE int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8609 | `PH7_PRIVATE int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8610 | `PH7_PRIVATE int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8611 | `PH7_PRIVATE int PH7_builtin_stripcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8612 | `PH7_PRIVATE int PH7_builtin_quoted_printable_encode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8613 | `PH7_PRIVATE int PH7_builtin_quoted_printable_decode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8614 | `PH7_PRIVATE int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8615 | `PH7_PRIVATE int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8616 | `PH7_PRIVATE int PH7_builtin_strnatcmp(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8617 | `PH7_PRIVATE int PH7_builtin_version_compare(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8618 | `PH7_PRIVATE int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8619 | `PH7_PRIVATE int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8620 | `PH7_PRIVATE int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8621 | `PH7_PRIVATE int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8622 | `PH7_PRIVATE int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8623 | `PH7_PRIVATE int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8624 | `PH7_PRIVATE int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8625 | `PH7_PRIVATE int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8626 | `PH7_PRIVATE int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8627 | `PH7_PRIVATE int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8628 | `PH7_PRIVATE int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8629 | `PH7_PRIVATE int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8630 | `PH7_PRIVATE int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8631 | `PH7_PRIVATE int PH7_builtin_count_chars(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8632 | `PH7_PRIVATE int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8633 | `PH7_PRIVATE int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8634 | `PH7_PRIVATE int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8635 | `PH7_PRIVATE int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8636 | `PH7_PRIVATE int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8637 | `PH7_PRIVATE int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8638 | `PH7_PRIVATE int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8639 | `PH7_PRIVATE int PH7_builtin_str_word_count(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8640 | `PH7_PRIVATE int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8641 | `PH7_PRIVATE int PH7_builtin_substr_compare(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8642 | `PH7_PRIVATE int PH7_builtin_substr_count(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8643 | `PH7_PRIVATE int PH7_builtin_substr_replace(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8644 | `PH7_PRIVATE int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8645 | `PH7_PRIVATE int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8646 | `PH7_PRIVATE int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8647 | `PH7_PRIVATE int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8648 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 8649 | `/* hashmap.c function prototypes */` |
|         - | 8650 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(ph7_vm *pVm,sxu32 (*xIntHash)(sxi64),sxu32 (*xBlobHash)(const void *,sxu32));` |
|         - | 8651 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm);` |
|         - | 8652 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS);` |
|         - | 8653 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap);` |
|         - | 8654 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(ph7_hashmap *pMap,ph7_value *pKey,ph7_hashmap_node **ppNode);` |
|         - | 8655 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(ph7_hashmap *pMap,ph7_value *pKey,ph7_value *pVal);` |
|         - | 8656 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(ph7_hashmap *pMap,ph7_value *pKey,sxu32 nRefIdx);` |
|         - | 8657 | `PH7_PRIVATE sxi32 PH7_HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest);` |
|         - | 8658 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight);` |
|         - | 8659 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore);` |
|         - | 8660 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest);` |
|         - | 8661 | `PH7_PRIVATE sxi32 PH7_HashmapDupMaterialized(ph7_hashmap *pSrc,ph7_hashmap *pDest);` |
|         - | 8662 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue);` |
|         - | 8663 | `PH7_PRIVATE sxi32 PH7_HashmapCmp(ph7_hashmap *pLeft,ph7_hashmap *pRight,int bStrict,int iNest);` |
|         - | 8664 | `PH7_PRIVATE void PH7_HashmapRegisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep);` |
|         - | 8665 | `PH7_PRIVATE void PH7_HashmapUnregisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep);` |
|         - | 8666 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap);` |
|         - | 8667 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore);` |
|         - | 8668 | `PH7_PRIVATE int PH7_HashmapNodeIsRef(ph7_hashmap_node *pNode);` |
|         - | 8669 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey);` |
|         - | 8670 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm);` |
|         - | 8671 | `/* hashmap.c engine helpers shared with hashmap_sort.c / hashmap_builtin.c */` |
|         - | 8672 | `PH7_PRIVATE ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode);` |
|         - | 8673 | `PH7_PRIVATE sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict,int iNest);` |
|         - | 8674 | `PH7_PRIVATE void HashmapRehashIntNode(ph7_hashmap_node *pEntry);` |
|         - | 8675 | `PH7_PRIVATE sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected);` |
|         - | 8676 | `PH7_PRIVATE sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve);` |
|         - | 8677 | `PH7_PRIVATE int HashmapFindValue(ph7_hashmap *pMap,ph7_value *pNeedle,ph7_hashmap_node **ppNode,int bStrict);` |
|         - | 8678 | `PH7_PRIVATE int HashmapValueStrEq(ph7_value *pA,ph7_value *pB,int bUserVisible,sxi32 *pRc);` |
|         - | 8679 | `PH7_PRIVATE sxi32 HashmapStringifyElems(ph7_hashmap *pMap);` |
|         - | 8680 | `PH7_PRIVATE int HashmapFindStringValue(ph7_hashmap *pMap,ph7_value *pNeedle,ph7_hashmap_node **ppNode,sxi32 *pRc);` |
|         - | 8681 | `PH7_PRIVATE sxi32 HashmapOverwrite(ph7_hashmap *pSrc,ph7_hashmap *pDest);` |
|         - | 8682 | `PH7_PRIVATE sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest);` |
|         - | 8683 | `PH7_PRIVATE sxi32 HashmapInsert(ph7_hashmap *pMap,ph7_value *pKey,ph7_value *pVal);` |
|         - | 8684 | `PH7_PRIVATE sxi32 HashmapLookupIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_hashmap_node **ppNode);` |
|         - | 8685 | `PH7_PRIVATE sxi32 HashmapLookupBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_hashmap_node **ppNode);` |
|         - | 8686 | `PH7_PRIVATE sxi32 PH7_HashmapInsertRawKey(ph7_hashmap *pMap,const char *zKey,sxu32 nKey,ph7_value *pVal);` |
|         - | 8687 | `/* hashmap_sort.c: the node ordering primitive and the sort builtin family.` |
|         - | 8688 | ` * Shared with hashmap.c (shuffle/array_unique/array_rand) and referenced from` |
|         - | 8689 | ` * the aHashmapFunc[] registration table; compiled in every mode. */` |
|         - | 8690 | `typedef sxi32 (*ProcNodeCmp)(ph7_hashmap_node *,ph7_hashmap_node *,void *);` |
|         - | 8691 | `/* One entry as an ordering sees it: the node, plus where it stood before the` |
|         - | 8692 | ` * sort ran. php stamps every bucket the same way (Z_EXTRA), and the stamp is` |
|         - | 8693 | ` * what makes a quicksort stable for the sort builtins and what lets the` |
|         - | 8694 | ` * diff/intersect family answer in the source array's order. */` |
|         - | 8695 | `typedef struct HashmapSortEnt HashmapSortEnt;` |
|         - | 8696 | `struct HashmapSortEnt {` |
|         - | 8697 | `	ph7_hashmap_node *pNode;` |
|         - | 8698 | `	sxu32 nOrd;` |
|         - | 8699 | `};` |
|         - | 8700 | `PH7_PRIVATE void PH7_HashmapSortEntVector(HashmapSortEnt *aEnt,sxu32 n,ProcNodeCmp xCmp,void *pCmpData);` |
|         - | 8701 | `PH7_PRIVATE sxi32 PH7_HashmapUserCmp(ph7_context *pCtx,ph7_value *pCallback,ph7_value *pA,ph7_value *pB,` |
|         - | 8702 | `	int bBoolRetry,int *pCmp);` |
|         - | 8703 | `PH7_PRIVATE sxi32 PH7_HashmapShuffle(ph7_hashmap *pMap);` |
|         - | 8704 | `PH7_PRIVATE sxi32 HashmapNodeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData);` |
|         - | 8705 | `PH7_PRIVATE void HashmapSortRehash(ph7_hashmap *pMap);` |
|         - | 8706 | `PH7_PRIVATE int HashmapValueFlagEqual(ph7_vm *pVm,ph7_value *pA,ph7_value *pB,int base,int bFold);` |
|         - | 8707 | `PH7_PRIVATE int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8708 | `PH7_PRIVATE int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8709 | `PH7_PRIVATE int ph7_hashmap_natsort(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8710 | `PH7_PRIVATE int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8711 | `PH7_PRIVATE int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8712 | `PH7_PRIVATE int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8713 | `PH7_PRIVATE int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8714 | `PH7_PRIVATE int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8715 | `PH7_PRIVATE int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8716 | `PH7_PRIVATE int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8717 | `/* hashmap_builtin.c function prototypes (the array_* family; referenced from` |
|         - | 8718 | ` * the aHashmapFunc[] registration table in hashmap.c; compiled in every mode) */` |
|         - | 8719 | `PH7_PRIVATE int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8720 | `PH7_PRIVATE int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8721 | `PH7_PRIVATE int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8722 | `PH7_PRIVATE int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8723 | `PH7_PRIVATE int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8724 | `PH7_PRIVATE int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8725 | `PH7_PRIVATE int ph7_hashmap_unshift(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8726 | `PH7_PRIVATE int ph7_hashmap_merge_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8727 | `PH7_PRIVATE int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8728 | `PH7_PRIVATE int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8729 | `PH7_PRIVATE int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8730 | `PH7_PRIVATE int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8731 | `PH7_PRIVATE int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8732 | `PH7_PRIVATE int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8733 | `PH7_PRIVATE int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8734 | `PH7_PRIVATE int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8735 | `PH7_PRIVATE int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8736 | `PH7_PRIVATE int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8737 | `PH7_PRIVATE int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8738 | `PH7_PRIVATE int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8739 | `PH7_PRIVATE int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8740 | `PH7_PRIVATE int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8741 | `PH7_PRIVATE int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8742 | `PH7_PRIVATE int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8743 | `PH7_PRIVATE int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8744 | `PH7_PRIVATE int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8745 | `PH7_PRIVATE int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8746 | `PH7_PRIVATE int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8747 | `PH7_PRIVATE int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8748 | `PH7_PRIVATE int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8749 | `PH7_PRIVATE int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8750 | `PH7_PRIVATE int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8751 | `PH7_PRIVATE int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8752 | `PH7_PRIVATE int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8753 | `PH7_PRIVATE int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8754 | `PH7_PRIVATE int ph7_hashmap_diff_ukey(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8755 | `PH7_PRIVATE int ph7_hashmap_intersect_ukey(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8756 | `PH7_PRIVATE int ph7_hashmap_udiff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8757 | `PH7_PRIVATE int ph7_hashmap_uintersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8758 | `PH7_PRIVATE int ph7_hashmap_udiff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8759 | `PH7_PRIVATE int ph7_hashmap_uintersect_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8760 | `PH7_PRIVATE int ph7_hashmap_intersect_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8761 | `PH7_PRIVATE int ph7_hashmap_multisort(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8762 | `PH7_PRIVATE int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8763 | `PH7_PRIVATE int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8764 | `PH7_PRIVATE int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8765 | `PH7_PRIVATE int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8766 | `PH7_PRIVATE int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8767 | `PH7_PRIVATE int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8768 | `PH7_PRIVATE int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8769 | `PH7_PRIVATE int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8770 | `PH7_PRIVATE int ph7_hashmap_max(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8771 | `PH7_PRIVATE int ph7_hashmap_min(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8772 | `PH7_PRIVATE int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8773 | `PH7_PRIVATE int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8774 | `PH7_PRIVATE int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8775 | `PH7_PRIVATE int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8776 | `PH7_PRIVATE int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8777 | `PH7_PRIVATE int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8778 | `PH7_PRIVATE int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8779 | `PH7_PRIVATE int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8780 | `PH7_PRIVATE int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8781 | `PH7_PRIVATE int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8782 | `PH7_PRIVATE int ph7_hashmap_first(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8783 | `PH7_PRIVATE int ph7_hashmap_last(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8784 | `PH7_PRIVATE int ph7_hashmap_key_first(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8785 | `PH7_PRIVATE int ph7_hashmap_key_last(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8786 | `PH7_PRIVATE int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8787 | `PH7_PRIVATE int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8788 | `PH7_PRIVATE int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8789 | `PH7_PRIVATE int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8790 | `PH7_PRIVATE int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8791 | `PH7_PRIVATE int ph7_iterator_to_array(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8792 | `PH7_PRIVATE int ph7_iterator_count(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8793 | `PH7_PRIVATE int ph7_iterator_apply(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8794 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth);` |
|         - | 8795 | `PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth,int bProp);` |
|         - | 8796 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(ph7_hashmap *pMap,int (*xWalk)(ph7_value *,ph7_value *,void *),void *pUserData);` |
|         - | 8797 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap);` |
|         - | 8798 | `/* php's key fold, shared with the diagnostics that must print a key the way the` |
|         - | 8799 | ` * LOOKUP saw it. Leaves a non-integer key as a printable string value. */` |
|         - | 8800 | `PH7_PRIVATE int PH7_HashmapKeyIsInt(ph7_value *pKey);` |
|         - | 8801 | `/* php value-name helper (true/false/class-name/null); used by the range()/` |
|         - | 8802 | ` * array_rand() domain-error messages in hashmap.c, which are compiled in every` |
|         - | 8803 | ` * mode, so it must stay outside the PH7_DISABLE_DISK_IO guard. */` |
|         - | 8804 | `PH7_PRIVATE const char *VmValueGivenName(ph7_value *pVal,char *zBuf,sxu32 nBuf);` |
|         - | 8805 | `PH7_PRIVATE void PH7_ReflectInterfacesOf(ph7_vm *pVm,ph7_class *pClass,SySet *pOut);` |
|         - | 8806 | `/* Outcomes of php's STRING-container offset rules (VmStringOffsetResolve). */` |
|         - | 8807 | `#define VM_STROFF_OK      0  /* *piOfft holds php's offset */` |
|         - | 8808 | `#define VM_STROFF_REJECT  1  /* php's TypeError; pMsg carries its message */` |
|         - | 8809 | `#define VM_STROFF_MISS    2  /* lenient context: answer "not set", say nothing */` |
|         - | 8810 | `/* ...and its DIAGNOSTIC LEVEL, of which php has three, not two:` |
|         - | 8811 | ` *   VM_STROFF_LOUD      a real read or write: every warning, and an offset TYPE` |
|         - | 8812 | ` *                       php refuses is the TypeError.` |
|         - | 8813 | `` *   VM_STROFF_COALESCE  a `??` / `??=` fetch: the NOT-SET diagnostics are`` |
|         - | 8814 | `` *                       suppressed (no `Uninitialized string offset`, and a`` |
|         - | 8815 | ` *                       refused offset TYPE answers "not set"), and so is the` |
|         - | 8816 | ` *                       null/bool/float CAST notice — but the offset SHAPE` |
|         - | 8817 | ` *                       warning still fires and the offset is still read:` |
|         - | 8818 | ``  *                       `$s["1x"] ?? "d"` warns `Illegal string offset "1x"` `` |
|         - | 8819 | `` *                       and answers `$s[1]`.`` |
|         - | 8820 | ` *   VM_STROFF_ISSET     isset()/empty()/unset(): fully quiet, every shape.` |
|         - | 8821 | ` *   VM_STROFF_UNSETBASE an INTERMEDIATE subscript of an unset chain` |
|         - | 8822 | `` *                       (`unset($s[k][0])`, `unset($s[k]->p)`): php reads the`` |
|         - | 8823 | ` *                       offset to hand it on, so the CAST notice fires as in a` |
|         - | 8824 | ` *                       real write, but the int-then-garbage warning does not` |
|         - | 8825 | `` *                       (`unset($s["1x"][0])` says nothing about "1x") and an`` |
|         - | 8826 | ` *                       offset TYPE php refuses is not the read's TypeError —` |
|         - | 8827 | `` *                       it is the unset's own `Cannot unset string offsets`,`` |
|         - | 8828 | ` *                       which the caller raises on REJECT at this level.` |
|         - | 8829 | ` */` |
|         - | 8830 | `#define VM_STROFF_LOUD      0` |
|         - | 8831 | `#define VM_STROFF_COALESCE  1` |
|         - | 8832 | `#define VM_STROFF_ISSET     2` |
|         - | 8833 | `#define VM_STROFF_UNSETBASE 3` |
|         - | 8834 | `PH7_PRIVATE int VmStringOffsetResolve(ph7_vm *pVm,ph7_value *pIdx,int iLevel,sxi64 *piOfft,SyBlob *pMsg);` |
|         - | 8835 | `PH7_PRIVATE sxi32 VmStringOffsetWrite(ph7_vm *pVm,ph7_value *pStr,sxi64 iRawOfft,ph7_value *pVal);` |
|         - | 8836 | `/* Numeric-string classifier — php's is_numeric_string() grammar — shared from` |
|         - | 8837 | ` * hashmap.c (range/array_rand) for the stage-2 ZPP domain-error sweep` |
|         - | 8838 | ` * RangeStrToNumber only ever returns ERROR/LONG/DOUBLE; the` |
|         - | 8839 | ` * STRING/DIGIT codes are range()-internal endpoint tags. range() and array_rand()` |
|         - | 8840 | ` * are core builtins compiled in every mode, so these must stay outside the` |
|         - | 8841 | ` * PH7_DISABLE_DISK_IO guard. */` |
|         - | 8842 | `#define RANGE_IN_ERROR   0` |
|         - | 8843 | `#define RANGE_IN_LONG    1` |
|         - | 8844 | `#define RANGE_IN_DOUBLE  2` |
|         - | 8845 | `#define RANGE_IN_STRING  3` |
|         - | 8846 | `#define RANGE_IN_DIGIT   4` |
|         - | 8847 | `PH7_PRIVATE sxu8 RangeStrToNumber(const char *zIn,sxu32 nLen,sxi64 *pLong,double *pDouble);` |
|         - | 8848 | `#ifndef PH7_DISABLE_DISK_IO` |
|         - | 8849 | `PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut);` |
|         - | 8850 | `/* builtin.c function prototypes */` |
|         - | 8851 | `PH7_PRIVATE sxi32 PH7_InputFormat(int (*xConsumer)(ph7_context *,const char *,int,void *),` |
|         - | 8852 | `	ph7_context *pCtx,const char *zIn,int nByte,int nArg,ph7_value **apArg,void *pUserData,int vf);` |
|         - | 8853 | `PH7_PRIVATE sxi32 PH7_FormatValidate(ph7_context *pCtx,const char *zFormat,int nByte);` |
|         - | 8854 | `PH7_PRIVATE sxi32 PH7_FormatCheckArgCount(ph7_context *pCtx,const char *zFormat,int nByte,int nValues,int nFixed,int bVararg);` |
|         - | 8855 | `PH7_PRIVATE sxi32 PH7_FormatCheckFormatArg(ph7_context *pCtx,ph7_value *pArg,int iArg);` |
|         - | 8856 | `PH7_PRIVATE sxi32 PH7_CheckStreamArg(ph7_context *pCtx,ph7_value *pArg,int iArg,const char *zName);` |
|         - | 8857 | `/* $escape = "" disables escape processing: a sentinel outside 0..255 so no byte` |
|         - | 8858 | ` * of a field can ever compare equal to it. */` |
|         - | 8859 | `#define PH7_CSV_NO_ESCAPE 256` |
|         - | 8860 | `/* Cursor of the incremental "is this record still open?" scan (see` |
|         - | 8861 | ` * PH7_CsvScanOpen); fgetcsv() keeps one per record it is assembling. */` |
|         - | 8862 | `typedef struct PH7_CsvScan PH7_CsvScan;` |
|         - | 8863 | `struct PH7_CsvScan {` |
|         - | 8864 | `	int iState;   /* 0 field start, 1 unquoted, 2 inside the enclosure, 3 past it */` |
|         - | 8865 | `	sxu32 nPos;   /* how much of the record has been scanned */` |
|         - | 8866 | `};` |
|         - | 8867 | `PH7_PRIVATE void PH7_CsvScanInit(PH7_CsvScan *pScan);` |
|         - | 8868 | `PH7_PRIVATE int PH7_CsvScanOpen(PH7_CsvScan *pScan,const char *zIn,sxu32 nByte,` |
|         - | 8869 | `	int delim,int encl,int escape);` |
|         - | 8870 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(ph7_value *pArray,const char *zInput,int nByte,` |
|         - | 8871 | `	int delim,int encl,int escape,int *pbOpen);` |
|         - | 8872 | `PH7_PRIVATE sxi32 PH7_CsvCharArg(ph7_context *pCtx,ph7_value *pArg,int iArg,` |
|         - | 8873 | `	const char *zName,int bAllowEmpty,int *pChar);` |
|         - | 8874 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen,int bTagSpaces);` |
|         - | 8875 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection,int iScannerMode,const char *zFile);` |
|         - | 8876 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|         - | 8877 | `/* Constant lookup by name: unguarded because the php.ini value grammar (vm.c,` |
|         - | 8878 | ` * VmIniExprOperand) substitutes a constant for a bare identifier, and that runs` |
|         - | 8879 | ` * in every build. [[tiny-build-disk-io-guard-fragility]] */` |
|         - | 8880 | `PH7_PRIVATE int PH7_VmQueryConstant(ph7_vm *pVm,const char *zName,sxu32 nName,ph7_value *pOut);` |
|         - | 8881 | `PH7_PRIVATE int PH7_ExpandBuiltinConstant(ph7_vm *pVm,const char *zName,sxu32 nName,ph7_value *pOut);` |
|         - | 8882 | `/* Natural-order compare: unguarded because hashmap.c's SORT_NATURAL path (always` |
|         - | 8883 | ` * compiled) uses it, even in the tiny build. [[tiny-build-disk-io-guard-fragility]] */` |
|         - | 8884 | `PH7_PRIVATE int PH7_StrNatCmp(const char *zA,int nA,const char *zB,int nB,int bFold);` |
|         - | 8885 | `/* oo.c function prototypes */` |
|         - | 8886 | `PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine);` |
|         - | 8887 | `PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags);` |
|         - | 8888 | `PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,` |
|         - | 8889 | `	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags);` |
|         - | 8890 | `PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte);` |
|         - | 8891 | `PH7_PRIVATE ph7_class_attr   * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte);` |
|         - | 8892 | `PH7_PRIVATE ph7_class_attr   * PH7_ClassExtractConstant(ph7_class *pClass,const char *zName,sxu32 nByte);` |
|         - | 8893 | `PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr);` |
|         - | 8894 | `PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth);` |
|         - | 8895 | `PH7_PRIVATE int PH7_MagicMethodMustBePublic(const SyString *pName);` |
|         - | 8896 | `PH7_PRIVATE sxi32 PH7_VmCallMagicMethod(ph7_vm *pVm,ph7_class_instance *pThis,` |
|         - | 8897 | `	ph7_class_method *pMethod,ph7_value *pResult,int nArg,ph7_value **apArg);` |
|         - | 8898 | `PH7_PRIVATE SyString * PH7_VmImplicitCallerArm(ph7_vm *pVm);` |
|         - | 8899 | `PH7_PRIVATE SyString * PH7_VmReachingNativeName(ph7_vm *pVm);` |
|         - | 8900 | `PH7_PRIVATE sxi32 PH7_VmCallMagicMethodLsb(ph7_vm *pVm,ph7_class *pCalled,ph7_class_instance *pThis,` |
|         - | 8901 | `	ph7_class_method *pMethod,ph7_value *pResult,int nArg,ph7_value **apArg);` |
|         - | 8902 | `PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase);` |
|         - | 8903 | `PH7_PRIVATE sxi32 PH7_ClassCheckInterfaceProps(ph7_gen_state *pGen,ph7_class *pSub);` |
|         - | 8904 | `PH7_PRIVATE sxi32 PH7_ClassSettleObligations(ph7_vm *pVm,VmClassObligeSet *pSet);` |
|         - | 8905 | `PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait);` |
|         - | 8906 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase);` |
|         - | 8907 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceCheckRedeclare(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pParent);` |
|         - | 8908 | `PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface);` |
|         - | 8909 | `PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass);` |
|         - | 8910 | `PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc);` |
|         - | 8911 | `/* The two clone questions that follow php's HANDLER inheritance rather than the` |
|         - | 8912 | ` * class's own row: a user subclass of an uncloneable class is uncloneable` |
|         - | 8913 | `` * (`class M extends IteratorIterator {}` refuses `clone $m` with M's name), and`` |
|         - | 8914 | ` * a subclass of a class with a native clone hook clones through that hook. */` |
|         - | 8915 | `PH7_PRIVATE int PH7_ClassIsUncloneable(ph7_class *pClass);` |
|         - | 8916 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest);` |
|         - | 8917 | `PH7_PRIVATE void  PH7_ClassInstanceUnref(ph7_class_instance *pThis);` |
|         - | 8918 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallDestructor(ph7_class_instance *pThis);` |
|         - | 8919 | `PH7_PRIVATE void PH7_ClassInstanceCtorFailed(ph7_class_instance *pThis);` |
|         - | 8920 | `PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth);` |
|         - | 8921 | `PH7_PRIVATE int PH7_UnmangleAttrName(const char *zKey,sxu32 nKey,SyString *pClass,SyString *pName);` |
|         - | 8922 | `PH7_PRIVATE SyHashEntry * PH7_ClassInstanceAttrEntry(ph7_class_instance *pThis,const char *zName,sxu32 nName);` |
|         - | 8923 | `PH7_PRIVATE const SyString * PH7_ClassAttrStorageName(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr);` |
|         - | 8924 | `PH7_PRIVATE void PH7_ClassNotePrivateName(ph7_class *pClass,ph7_class_attr *pAttr);` |
|         - | 8925 | `PH7_PRIVATE ph7_class_attr * PH7_ClassScopedAttribute(ph7_vm *pVm,ph7_class *pClass,const char *zName,sxu32 nName);` |
|         - | 8926 | `PH7_PRIVATE SyHashEntry * PH7_ClassInstanceScopedAttrEntry(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nName,sxu32 nHash);` |
|         - | 8927 | `PH7_PRIVATE int PH7_ClassInstanceAttrShadowed(ph7_vm *pVm,ph7_class_instance *pThis,SyHashEntry *pEntry);` |
|         - | 8928 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(ph7_vm *pVm,ph7_class *pClass,ph7_class_instance *pThis,const char *zMethod,` |
|         - | 8929 | `	sxu32 nByte,const SyString *pAttrName,ph7_value *pResult);` |
|         - | 8930 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr);` |
|         - | 8931 | `PH7_PRIVATE void PH7_ClassInstanceAttrKey(ph7_class_instance *pThis,VmClassAttr *pAttr,ph7_value *pKey);` |
|         - | 8932 | `PH7_PRIVATE int PH7_ClassInstanceAttrPresented(VmClassAttr *pAttr);` |
|         - | 8933 | `PH7_PRIVATE void PH7_ClassInstanceIterOpen(ph7_class_instance *pThis,PH7_AttrIter *pIter);` |
|         - | 8934 | `PH7_PRIVATE SyHashEntry * PH7_ClassInstanceIterNext(PH7_AttrIter *pIter);` |
|         - | 8935 | `PH7_PRIVATE void PH7_ClassInstanceIterClose(ph7_class_instance *pThis,PH7_AttrIter *pIter);` |
|         - | 8936 | `PH7_PRIVATE void PH7_ClassInstanceDeleteAttrEntry(ph7_class_instance *pThis,SyHashEntry *pEntry);` |
|         - | 8937 | `PH7_PRIVATE void PH7_ClassInstanceAttrAppended(ph7_class_instance *pThis,SyHashEntry *pEntry);` |
|         - | 8938 | `PH7_PRIVATE sxi32 PH7_VmHookGetAttrValue(ph7_class_instance *pThis,VmClassAttr *pVmAttr,ph7_value *pOut);` |
|         - | 8939 | `PH7_PRIVATE void PH7_MemObjPrintRInline(SyBlob *pOut,ph7_value *pObj);` |
|         - | 8940 | `PH7_PRIVATE ph7_value * PH7_EnumCaseNameValue(ph7_class_instance *pThis);` |
|         - | 8941 | `PH7_PRIVATE ph7_value * PH7_EnumCaseBackingValueOf(ph7_class_instance *pThis);` |
|         - | 8942 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap);` |
|         - | 8943 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmapRaw(ph7_class_instance *pThis,ph7_hashmap *pMap);` |
|         - | 8944 | `PH7_PRIVATE int PH7_ClassAttrIsRef(ph7_class_instance *pThis,VmClassAttr *pVmAttr);` |
|         - | 8945 | `PH7_PRIVATE sxi32 PH7_ClassInstanceOwnPropsToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap);` |
|         - | 8946 | `PH7_PRIVATE int PH7_ClassAttrUninitialized(VmClassAttr *pVmAttr);` |
|         - | 8947 | `PH7_PRIVATE int PH7_ClassAttrUninitializedForRead(VmClassAttr *pVmAttr);` |
|         - | 8948 | `PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(ph7_class_instance *pThis,` |
|         - | 8949 | `	int (*xWalk)(const char *,ph7_value *,void *),void *pUserData);` |
|         - | 8950 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName);` |
|         - | 8951 | `PH7_PRIVATE int PH7_VmDimFetchWritable(ph7_class *pClass);` |
|         - | 8952 | `PH7_PRIVATE sxu32 PH7_SplDimElemSlot(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pKey,int bCreate);` |
|         - | 8953 | `PH7_PRIVATE void PH7_SplDirVmRelease(ph7_vm *pVm);` |
|         - | 8954 | `/* Called from the VM's reset and release paths, which every build has: the` |
|         - | 8955 | ` * tiny one answers them with the stubs at the tail of vm_phar.c. */` |
|         - | 8956 | `PH7_PRIVATE void PH7_PharVmReset(ph7_vm *pVm);` |
|         - | 8957 | `PH7_PRIVATE void PH7_PharVmRelease(ph7_vm *pVm);` |
|         - | 8958 | `PH7_PRIVATE void PH7_VmOverloadedElemNotice(ph7_vm *pVm,ph7_class *pClass,ph7_value *pVal);` |
|         - | 8959 | `PH7_PRIVATE void PH7_VmOverloadedPropNotice(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,ph7_value *pVal);` |
|         - | 8960 | `PH7_PRIVATE ph7_class_instance * PH7_ContextThis(ph7_context *pCtx);` |
|         - | 8961 | `PH7_PRIVATE ph7_class * PH7_ContextCalledClass(ph7_context *pCtx);` |
|         - | 8962 | `PH7_PRIVATE ph7_value * PH7_ContextThisValue(ph7_context *pCtx);` |
|         - | 8963 | `/* vfs.c */` |
|         - | 8964 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 8965 | `PH7_PRIVATE int PH7_StreamIsUrlWrapper(const ph7_io_stream *pStream);` |
|         - | 8966 | `PH7_PRIVATE void * PH7_StreamOpenHandle(ph7_vm *pVm,const ph7_io_stream *pStream,const char *zFile,` |
|         - | 8967 | `	int iFlags,int use_include,ph7_value *pResource,int bPushInclude,int *pNew,const char *zCaller);` |
|         - | 8968 | `PH7_PRIVATE sxi32 PH7_StreamReadWholeFile(void *pHandle,const ph7_io_stream *pStream,SyBlob *pOut);` |
|         - | 8969 | `PH7_PRIVATE int PH7_VfsAppendFile(ph7_context *pCtx,const char *zFile,const void *pData,int nLen);` |
|         - | 8970 | `PH7_PRIVATE void PH7_StreamCloseHandle(const ph7_io_stream *pStream,void *pHandle);` |
|         - | 8971 | `PH7_PRIVATE ph7_int64 PH7_StreamRead(io_private *pDev,void *pBuf,ph7_int64 nLen);` |
|         - | 8972 | `PH7_PRIVATE ph7_int64 PH7_StreamWrite(io_private *pDev,const void *pData,ph7_int64 nLen);` |
|         - | 8973 | `PH7_PRIVATE void PH7_StreamFlushWriteChain(io_private *pDev);` |
|         - | 8974 | `PH7_PRIVATE int PH7_StreamModeIsValid(const char *zMode,int nLen,int *piFlags);` |
|         - | 8975 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 8976 | `PH7_PRIVATE const char * PH7_ExtractDirName(const char *zPath,int nByte,int *pLen);` |
|         - | 8977 | `PH7_PRIVATE const char * PH7_ExtractBaseName(const char *zPath,int nByte,int *pLen);` |
|         - | 8978 | `PH7_PRIVATE sxi32 PH7_RegisterIORoutine(ph7_vm *pVm);` |
|         - | 8979 | `/* vfs_stream.c / vfs_io_driver.c function prototypes (referenced from the` |
|         - | 8980 | ` * registration tables in vfs.c's PH7_RegisterIORoutine) */` |
|         - | 8981 | `#ifndef PH7_DISABLE_DISK_IO` |
|         - | 8982 | `PH7_PRIVATE int PH7_builtin_closedir(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8983 | `PH7_PRIVATE int PH7_builtin_copy(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8984 | `PH7_PRIVATE int PH7_builtin_escapeshellarg(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8985 | `PH7_PRIVATE int PH7_builtin_exec(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8986 | `PH7_PRIVATE int PH7_builtin_escapeshellcmd(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8987 | `PH7_PRIVATE int PH7_builtin_fclose(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8988 | `PH7_PRIVATE int PH7_builtin_feof(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8989 | `PH7_PRIVATE int PH7_builtin_fflush(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8990 | `PH7_PRIVATE int PH7_builtin_fgetc(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8991 | `PH7_PRIVATE int PH7_builtin_fgetcsv(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8992 | `PH7_PRIVATE int PH7_builtin_fgets(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8993 | `PH7_PRIVATE int PH7_builtin_stream_get_line(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8994 | `PH7_PRIVATE int PH7_builtin_fgetss(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8995 | `PH7_PRIVATE int PH7_builtin_file(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8996 | `PH7_PRIVATE int PH7_builtin_file_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8997 | `PH7_PRIVATE int PH7_builtin_file_put_contents(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8998 | `PH7_PRIVATE int PH7_builtin_flock(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8999 | `PH7_PRIVATE int PH7_builtin_fopen(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9000 | `PH7_PRIVATE int PH7_builtin_fpassthru(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9001 | `PH7_PRIVATE int PH7_builtin_fprintf(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9002 | `PH7_PRIVATE int PH7_builtin_fputcsv(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9003 | `PH7_PRIVATE int PH7_builtin_fread(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9004 | `PH7_PRIVATE int PH7_builtin_fseek(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9005 | `PH7_PRIVATE int PH7_builtin_fsockopen(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9006 | `PH7_PRIVATE int PH7_builtin_stream_socket_server(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9007 | `PH7_PRIVATE int PH7_builtin_stream_socket_accept(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9008 | `PH7_PRIVATE int PH7_builtin_stream_socket_get_name(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9009 | `PH7_PRIVATE int PH7_builtin_stream_select(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9010 | `PH7_PRIVATE int PH7_builtin_stream_socket_pair(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9011 | `PH7_PRIVATE int PH7_builtin_inet_pton(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9012 | `PH7_PRIVATE int PH7_builtin_inet_ntop(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9013 | `PH7_PRIVATE int PH7_builtin_gethostname(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9014 | `PH7_PRIVATE int PH7_builtin_stream_socket_shutdown(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9015 | `PH7_PRIVATE int PH7_builtin_stream_socket_recvfrom(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9016 | `PH7_PRIVATE int PH7_builtin_stream_socket_sendto(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9017 | `PH7_PRIVATE int PH7_builtin_fstat(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9018 | `PH7_PRIVATE int PH7_builtin_ftell(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9019 | `PH7_PRIVATE int PH7_builtin_ftruncate(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9020 | `PH7_PRIVATE int PH7_builtin_fwrite(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9021 | `PH7_PRIVATE int PH7_builtin_md5_file(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9022 | `PH7_PRIVATE int PH7_builtin_opendir(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9023 | `PH7_PRIVATE int PH7_builtin_dir(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9024 | `PH7_PRIVATE int PH7_builtin_parse_ini_file(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9025 | `PH7_PRIVATE int PH7_builtin_passthru(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9026 | `PH7_PRIVATE int PH7_builtin_pclose(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9027 | `PH7_PRIVATE int PH7_builtin_popen(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9028 | `PH7_PRIVATE int PH7_builtin_proc_close(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9029 | `PH7_PRIVATE int PH7_builtin_proc_get_status(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9030 | `PH7_PRIVATE int PH7_builtin_proc_nice(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9031 | `PH7_PRIVATE int PH7_builtin_proc_open(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9032 | `PH7_PRIVATE int PH7_builtin_proc_terminate(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9033 | `PH7_PRIVATE int PH7_builtin_readdir(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9034 | `PH7_PRIVATE int PH7_builtin_readfile(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9035 | `PH7_PRIVATE int PH7_builtin_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9036 | `PH7_PRIVATE int PH7_builtin_rewinddir(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9037 | `PH7_PRIVATE int PH7_builtin_sha1_file(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9038 | `PH7_PRIVATE int PH7_builtin_shell_exec(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9039 | `PH7_PRIVATE int PH7_builtin_system(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9040 | `PH7_PRIVATE int PH7_builtin_stream_context_create(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9041 | `PH7_PRIVATE int PH7_builtin_stream_context_get_options(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9042 | `PH7_PRIVATE int PH7_builtin_stream_context_set_option(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9043 | `PH7_PRIVATE int PH7_builtin_stream_context_set_options(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9044 | `PH7_PRIVATE int PH7_builtin_stream_context_get_params(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9045 | `PH7_PRIVATE int PH7_builtin_stream_context_set_params(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9046 | `PH7_PRIVATE int PH7_builtin_stream_context_get_default(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9047 | `PH7_PRIVATE int PH7_builtin_stream_context_set_default(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9048 | `PH7_PRIVATE int PH7_builtin_stream_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9049 | `PH7_PRIVATE int PH7_builtin_stream_get_meta_data(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9050 | `PH7_PRIVATE int PH7_builtin_stream_set_blocking(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9051 | `PH7_PRIVATE int PH7_builtin_stream_set_timeout(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9052 | `PH7_PRIVATE int PH7_builtin_stream_set_chunk_size(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9053 | `PH7_PRIVATE int PH7_builtin_stream_set_read_buffer(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9054 | `PH7_PRIVATE int PH7_builtin_stream_set_write_buffer(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9055 | `PH7_PRIVATE int PH7_builtin_stream_supports_lock(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9056 | `PH7_PRIVATE int PH7_builtin_stream_is_local(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9057 | `PH7_PRIVATE int PH7_builtin_stream_copy_to_stream(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9058 | `PH7_PRIVATE int PH7_builtin_stream_socket_enable_crypto(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9059 | `PH7_PRIVATE int PH7_builtin_stream_get_transports(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9060 | `PH7_PRIVATE int PH7_builtin_stream_get_wrappers(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9061 | `PH7_PRIVATE int PH7_builtin_stream_isatty(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9062 | `PH7_PRIVATE int PH7_builtin_stream_wrapper_register(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9063 | `PH7_PRIVATE int PH7_StreamUserUrlStat(ph7_context *pCtx,const char *zPath,int iFlags,ph7_int64 *aVal);` |
|         - | 9064 | `PH7_PRIVATE void PH7_StreamUserCast(ph7_context *pCtx,const ph7_io_stream *pStream,void *pHandle);` |
|         - | 9065 | `PH7_PRIVATE int PH7_StreamUserPathOp(ph7_context *pCtx,const char *zPath,const char *zMethod,void *pStreamCtx,ph7_value **apExtra,int nExtra,int *pbAnswer);` |
|         - | 9066 | `PH7_PRIVATE void PH7_StreamArmOpenMode(ph7_vm *pVm,const char *zMode,int nMode);` |
|         - | 9067 | `/* php's whole-string, case-sensitive question about a php://memory or php://temp` |
|         - | 9068 | `` * mode: does it hold a `w`, an `a` or a `+` anywhere (writable), and does it hold`` |
|         - | 9069 | `` * an `a` (append)? Both the device's write permission and the mode it REPORTS`` |
|         - | 9070 | ` * come from this one answer. */` |
|         - | 9071 | `PH7_PRIVATE void PH7_PhpMemoryMode(const char *zMode,int nMode,int *pbWrite,int *pbAppend);` |
|         - | 9072 | `PH7_PRIVATE int PH7_StreamUserDirReason(ph7_vm *pVm,const ph7_io_stream *pStream,char *zBuf,int nBuf);` |
|         - | 9073 | `PH7_PRIVATE int PH7_VfsUserStatFields(ph7_context *pCtx,const char *zPath,int eAsk,ph7_int64 *aVal);` |
|         - | 9074 | `PH7_PRIVATE void PH7_VfsUserStatResult(ph7_context *pCtx,int eAsk,const ph7_int64 *aVal);` |
|         - | 9075 | `PH7_PRIVATE void PH7_VfsStatAccessMasks(ph7_context *pCtx,ph7_int64 nUid,ph7_int64 nGid,int *pR,int *pW,int *pX);` |
|         - | 9076 | `PH7_PRIVATE int PH7_builtin_stream_wrapper_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9077 | `PH7_PRIVATE int PH7_builtin_stream_wrapper_restore(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9078 | `PH7_PRIVATE int PH7_builtin_stream_filter_append(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9079 | `PH7_PRIVATE int PH7_builtin_stream_filter_prepend(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9080 | `PH7_PRIVATE int PH7_builtin_stream_filter_remove(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9081 | `PH7_PRIVATE int PH7_builtin_stream_get_filters(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9082 | `PH7_PRIVATE int PH7_builtin_vfprintf(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 9083 | `#endif /* PH7_DISABLE_DISK_IO */` |
|         - | 9084 | `PH7_PRIVATE const ph7_vfs * PH7_ExportBuiltinVfs(void);` |
|         - | 9085 | `PH7_PRIVATE const char * PH7_VfsResourceType(void *pResource);` |
|         - | 9086 | `PH7_PRIVATE int PH7_VfsResourceIsClosed(void *pResource);` |
|         - | 9087 | `PH7_PRIVATE void * PH7_ExportStdin(ph7_vm *pVm);` |
|         - | 9088 | `PH7_PRIVATE void * PH7_ExportStdout(ph7_vm *pVm);` |
|         - | 9089 | `PH7_PRIVATE void * PH7_ExportStderr(ph7_vm *pVm);` |
|         - | 9090 | `/* lib.c function prototypes */` |
|         - | 9091 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 9092 | `PH7_PRIVATE sxi32 SyBinToHexConsumer(const void *pIn,sxu32 nLen,ProcConsumer xConsumer,void *pConsumerData);` |
|         - | 9093 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 9094 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 9095 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|         - | 9096 | `PH7_PRIVATE sxu32 SyCrc32(const void *pSrc,sxu32 nLen);` |
|         - | 9097 | `PH7_PRIVATE void MD5Update(MD5Context *ctx, const unsigned char *buf, unsigned int len);` |
|         - | 9098 | `PH7_PRIVATE void MD5Final(unsigned char digest[16], MD5Context *ctx);` |
|         - | 9099 | `PH7_PRIVATE sxi32 MD5Init(MD5Context *pCtx);` |
|         - | 9100 | `PH7_PRIVATE sxi32 SyMD5Compute(const void *pIn,sxu32 nLen,unsigned char zDigest[16]);` |
|         - | 9101 | `PH7_PRIVATE void SHA1Init(SHA1Context *context);` |
|         - | 9102 | `PH7_PRIVATE void SHA1Update(SHA1Context *context,const unsigned char *data,unsigned int len);` |
|         - | 9103 | `PH7_PRIVATE void SHA1Final(SHA1Context *context, unsigned char digest[20]);` |
|         - | 9104 | `PH7_PRIVATE sxi32 SySha1Compute(const void *pIn,sxu32 nLen,unsigned char zDigest[20]);` |
|         - | 9105 | `#endif` |
|         - | 9106 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 9107 | `PH7_PRIVATE sxi32 SyRandomness(SyPRNGCtx *pCtx,void *pBuf,sxu32 nLen);` |
|         - | 9108 | `PH7_PRIVATE sxi32 SyRandomnessInit(SyPRNGCtx *pCtx,ProcRandomSeed xSeed,void *pUserData);` |
|         - | 9109 | `PH7_PRIVATE sxu32 SyBufferFormat(char *zBuf,sxu32 nLen,const char *zFormat,...);` |
|         - | 9110 | `PH7_PRIVATE sxu32 SyBlobFormatAp(SyBlob *pBlob,const char *zFormat,va_list ap);` |
|         - | 9111 | `PH7_PRIVATE sxu32 SyBlobFormat(SyBlob *pBlob,const char *zFormat,...);` |
|         - | 9112 | `PH7_PRIVATE sxi32 SyProcFormat(ProcConsumer xConsumer,void *pData,const char *zFormat,...);` |
|         - | 9113 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 9114 | `PH7_PRIVATE const char *SyTimeGetMonth(sxi32 iMonth);` |
|         - | 9115 | `PH7_PRIVATE const char *SyTimeGetDay(sxi32 iDay);` |
|         - | 9116 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 9117 | `PH7_PRIVATE sxi32 SyUriDecode(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData,int bPlus);` |
|         - | 9118 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 9119 | `PH7_PRIVATE sxi32 SyUriEncode(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData);` |
|         - | 9120 | `PH7_PRIVATE sxi32 SyUriEncodeRaw(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData);` |
|         - | 9121 | `#endif` |
|         - | 9122 | `PH7_PRIVATE sxi32 SyLexRelease(SyLex *pLex);` |
|         - | 9123 | `PH7_PRIVATE sxi32 SyLexTokenizeInput(SyLex *pLex,const char *zInput,sxu32 nLen,void *pCtxData,ProcSort xSort,ProcCmp xCmp);` |
|         - | 9124 | `PH7_PRIVATE sxi32 SyLexInit(SyLex *pLex,SySet *pSet,ProcTokenizer xTokenizer,void *pUserData);` |
|         - | 9125 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 9126 | `PH7_PRIVATE sxi32 SyBase64Decode(const char *zB64,sxu32 nLen,ProcConsumer xConsumer,void *pUserData);` |
|         - | 9127 | `PH7_PRIVATE sxi32 SyBase64Encode(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData);` |
|         - | 9128 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 9129 | `PH7_PRIVATE sxi32 SyStrToReal(const char *zSrc,sxu32 nLen,void *pOutVal,const char **zRest);` |
|         - | 9130 | `PH7_PRIVATE sxi32 SyBinaryStrToInt64(const char *zSrc,sxu32 nLen,void *pOutVal,const char **zRest);` |
|         - | 9131 | `PH7_PRIVATE sxi32 SyOctalStrToInt64(const char *zSrc,sxu32 nLen,void *pOutVal,const char **zRest);` |
|         - | 9132 | `PH7_PRIVATE sxi32 SyHexStrToInt64(const char *zSrc,sxu32 nLen,void *pOutVal,const char **zRest);` |
|         - | 9133 | `PH7_PRIVATE sxi32 SyHexToint(sxi32 c);` |
|         - | 9134 | `PH7_PRIVATE sxi32 SyStrToInt64(const char *zSrc,sxu32 nLen,void *pOutVal,const char **zRest);` |
|         - | 9135 | `PH7_PRIVATE sxi32 SyStrToInt32(const char *zSrc,sxu32 nLen,void *pOutVal,const char **zRest);` |
|         - | 9136 | `PH7_PRIVATE sxi32 SyStrIsNumeric(const char *zSrc,sxu32 nLen,sxu8 *pReal,const char **pzTail);` |
|         - | 9137 | `PH7_PRIVATE SyHashEntry *SyHashLastEntry(SyHash *pHash);` |
|         - | 9138 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData);` |
|         - | 9139 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData);` |
|         - | 9140 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32(*xStep)(SyHashEntry *,void *),void *pUserData);` |
|         - | 9141 | `PH7_PRIVATE SyHashEntry *SyHashGetNextEntry(SyHash *pHash);` |
|         - | 9142 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash);` |
|         - | 9143 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry);` |
|         - | 9144 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData);` |
|         - | 9145 | `PH7_PRIVATE SyHashEntry *SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen);` |
|         - | 9146 | `PH7_PRIVATE sxu32 SyHashKey(SyHash *pHash,const void *pKey,sxu32 nKeyLen);` |
|         - | 9147 | `PH7_PRIVATE SyHashEntry *SyHashGetHashed(SyHash *pHash,const void *pKey,sxu32 nKeyLen,sxu32 nHash);` |
|         - | 9148 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash);` |
|         - | 9149 | `PH7_PRIVATE sxi32 SyHashInitSized(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp,sxu32 nBucket);` |
|         - | 9150 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp);` |
|         - | 9151 | `PH7_PRIVATE sxu32 SyStrHash(const void *pSrc,sxu32 nLen);` |
|         - | 9152 | `PH7_PRIVATE sxu32 SyBinHash(const void *pSrc,sxu32 nLen);` |
|         - | 9153 | `PH7_PRIVATE sxu32 Systrcpy(char *zDest,sxu32 nDestLen,const char *zSrc,sxu32 nLen);` |
|         - | 9154 | `PH7_PRIVATE void *SySetAt(SySet *pSet,sxu32 nIdx);` |
|         - | 9155 | `PH7_PRIVATE void *SySetPop(SySet *pSet);` |
|         - | 9156 | `PH7_PRIVATE void *SySetPeek(SySet *pSet);` |
|         - | 9157 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet);` |
|         - | 9158 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet);` |
|         - | 9159 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet);` |
|         - | 9160 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry);` |
|         - | 9161 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 9162 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet);` |
|         - | 9163 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 9164 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize);` |
|         - | 9165 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem);` |
|         - | 9166 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem);` |
|         - | 9167 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize);` |
|         - | 9168 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 9169 | `PH7_PRIVATE sxi32 SyBlobSearch(const void *pBlob,sxu32 nLen,const void *pPattern,sxu32 pLen,sxu32 *pOfft);` |
|         - | 9170 | `#endif` |
|         - | 9171 | `PH7_PRIVATE sxi32 SyBlobRelease(SyBlob *pBlob);` |
|         - | 9172 | `PH7_PRIVATE sxi32 SyBlobReset(SyBlob *pBlob);` |
|         - | 9173 | `PH7_PRIVATE sxi32 SyBlobCmp(SyBlob *pLeft,SyBlob *pRight);` |
|         - | 9174 | `PH7_PRIVATE sxi32 SyBlobDup(SyBlob *pSrc,SyBlob *pDest);` |
|         - | 9175 | `PH7_PRIVATE sxi32 SyBlobNullAppend(SyBlob *pBlob);` |
|         - | 9176 | `PH7_PRIVATE sxi32 SyBlobAppend(SyBlob *pBlob,const void *pData,sxu32 nSize);` |
|         - | 9177 | `PH7_PRIVATE sxi32 SyBlobReadOnly(SyBlob *pBlob,const void *pData,sxu32 nByte);` |
|         - | 9178 | `PH7_PRIVATE sxi32 SyBlobInit(SyBlob *pBlob,SyMemBackend *pAllocator);` |
|         - | 9179 | `PH7_PRIVATE sxi32 SyBlobInitFromBuf(SyBlob *pBlob,void *pBuffer,sxu32 nSize);` |
|         - | 9180 | `PH7_PRIVATE char *SyMemBackendStrDup(SyMemBackend *pBackend,const char *zSrc,sxu32 nSize);` |
|         - | 9181 | `PH7_PRIVATE void *SyMemBackendDup(SyMemBackend *pBackend,const void *pSrc,sxu32 nSize);` |
|         - | 9182 | `PH7_PRIVATE sxi32 SyMemBackendRelease(SyMemBackend *pBackend);` |
|         - | 9183 | `PH7_PRIVATE sxi32 SyMemBackendInitFromOthers(SyMemBackend *pBackend,const SyMemMethods *pMethods,ProcMemError xMemErr,void *pUserData);` |
|         - | 9184 | `PH7_PRIVATE sxi32 SyMemBackendInit(SyMemBackend *pBackend,ProcMemError xMemErr,void *pUserData);` |
|         - | 9185 | `PH7_PRIVATE sxi32 SyMemBackendInitFromParent(SyMemBackend *pBackend,SyMemBackend *pParent);` |
|         - | 9186 | `#if 0` |
|         - | 9187 | `/* Not used in the current release of the PH7 engine */` |
|         - | 9188 | `PH7_PRIVATE void *SyMemBackendPoolRealloc(SyMemBackend *pBackend,void *pOld,sxu32 nByte);` |
|         - | 9189 | `#endif` |
|         - | 9190 | `PH7_PRIVATE sxi32 SyMemBackendPoolFree(SyMemBackend *pBackend,void *pChunk);` |
|         - | 9191 | `PH7_PRIVATE void *SyMemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte);` |
|         - | 9192 | `PH7_PRIVATE sxi32 SyMemBackendFree(SyMemBackend *pBackend,void *pChunk);` |
|         - | 9193 | `PH7_PRIVATE void *SyMemBackendRealloc(SyMemBackend *pBackend,void *pOld,sxu32 nByte);` |
|         - | 9194 | `PH7_PRIVATE void *SyMemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte);` |
|         - | 9195 | `#if defined(PH7_ENABLE_THREADS)` |
|         - | 9196 | `PH7_PRIVATE sxi32 SyMemBackendMakeThreadSafe(SyMemBackend *pBackend,const SyMutexMethods *pMethods);` |
|         - | 9197 | `PH7_PRIVATE sxi32 SyMemBackendDisbaleMutexing(SyMemBackend *pBackend);` |
|         - | 9198 | `#endif` |
|         - | 9199 | `PH7_PRIVATE sxu32 SyMemcpy(const void *pSrc,void *pDest,sxu32 nLen);` |
|         - | 9200 | `PH7_PRIVATE sxi32 SyMemcmp(const void *pB1,const void *pB2,sxu32 nSize);` |
|         - | 9201 | `PH7_PRIVATE void SyZero(void *pSrc,sxu32 nSize);` |
|         - | 9202 | `PH7_PRIVATE sxi32 SyStrnicmp(const char *zLeft,const char *zRight,sxu32 SLen);` |
|         - | 9203 | `PH7_PRIVATE sxi32 SyStrnmicmp(const void *pLeft, const void *pRight,sxu32 SLen);` |
|         - | 9204 | `/* used by hashmap.c's key sorting — must stay visible in the tiny build */` |
|         - | 9205 | `PH7_PRIVATE sxi32 SyStrncmp(const char *zLeft,const char *zRight,sxu32 nLen);` |
|         - | 9206 | `PH7_PRIVATE sxi32 SyByteListFind(const char *zSrc,sxu32 nLen,const char *zList,sxu32 *pFirstPos);` |
|         - | 9207 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 9208 | `PH7_PRIVATE sxi32 SyByteFind2(const char *zStr,sxu32 nLen,sxi32 c,sxu32 *pPos);` |
|         - | 9209 | `#endif` |
|         - | 9210 | `PH7_PRIVATE sxi32 SyByteFind(const char *zStr,sxu32 nLen,sxi32 c,sxu32 *pPos);` |
|         - | 9211 | `PH7_PRIVATE sxu32 SyStrlen(const char *zSrc);` |
|         - | 9212 | `#if defined(PH7_ENABLE_THREADS)` |
|         - | 9213 | `PH7_PRIVATE const SyMutexMethods *SyMutexExportMethods(void);` |
|         - | 9214 | `PH7_PRIVATE sxi32 SyMemBackendMakeThreadSafe(SyMemBackend *pBackend,const SyMutexMethods *pMethods);` |
|         - | 9215 | `PH7_PRIVATE sxi32 SyMemBackendDisbaleMutexing(SyMemBackend *pBackend);` |
|         - | 9216 | `#endif` |
|         - | 9217 | `#endif /* __PH7INT_H__ */` |
|         - | 9218 |  |
