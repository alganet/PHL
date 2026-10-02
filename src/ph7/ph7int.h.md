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
|         - |  158 | ``/* The pending offset of a `$s[k] ??= v`, owned by its MEMOBJ_AUX_COALSTROFF peek result.`` |
|         - |  159 | ` * Carries its own allocator so PH7_MemObjRelease can free it without a VM pointer, exactly` |
|         - |  160 | ` * like VmDeferredPath. */` |
|         - |  161 | `typedef struct VmCoalStrOff VmCoalStrOff;` |
|         - |  162 | `struct VmCoalStrOff` |
|         - |  163 | `{` |
|         - |  164 | `	SyMemBackend *pAlloc;` |
|         - |  165 | `	ph7_value sKey;      /* the RAW offset, unresolved: the store re-resolves it LOUDLY */` |
|         - |  166 | `};` |
|         - |  167 | `/* The pending __call / __callStatic routing, owned by its MEMOBJ_AUX_MAGICCALL carrier` |
|         - |  168 | ` * slot. Carries its own allocator so PH7_MemObjRelease can free it without a VM pointer,` |
|         - |  169 | ` * exactly like VmCoalStrOff.` |
|         - |  170 | ` *` |
|         - |  171 | ` * It used to be three fields on the VM, set by OP_MEMBER and read by the OP_CALL that` |
|         - |  172 | ` * followed. That only held while the arguments were evaluated BEFORE the member op; once` |
|         - |  173 | ` * the callee is resolved first — php's order — an argument that is itself a routed call` |
|         - |  174 | `` * (`$o->outer($o->inner(1))`) runs in between and would overwrite the outer routing. The`` |
|         - |  175 | ` * record rides the slot instead, so it nests, and an abandoned call cannot leak the` |
|         - |  176 | ` * receiver reference. */` |
|         - |  177 | `typedef struct VmMagicCall VmMagicCall;` |
|         - |  178 | `struct VmMagicCall` |
|         - |  179 | `{` |
|         - |  180 | `	SyMemBackend *pAlloc;` |
|         - |  181 | `	ph7_class_instance *pRecv; /* OWNED receiver reference; 0 for a static routing */` |
|         - |  182 | `	ph7_class *pClass;         /* the class whose handler answers */` |
|         - |  183 | `	SyBlob sName;              /* the method name as the CALL SITE spelled it */` |
|         - |  184 | `};` |
|         - |  185 | `/*` |
|         - |  186 | ` * D1 commit 2: a captured lvalue path for a deferred by-ref/by-value call argument` |
|         - |  187 | ` * ($a["k"], $o->p, and nested/undefined-base forms). Built on a lookup MISS by the` |
|         - |  188 | ` * LOAD_IDX/MEMBER record modes and re-walked by VmResolveDeferredArgs at OP_CALL once the` |
|         - |  189 | ` * callee's by-ref shape is known. Owned by a MEMOBJ_AUX_DEFPATH stack slot's x.pOther.` |
|         - |  190 | ` */` |
|         - |  191 | `typedef struct VmDeferStep VmDeferStep;` |
|         - |  192 | `typedef struct VmDeferredPath VmDeferredPath;` |
|         - |  193 | `struct VmDeferStep {` |
|         - |  194 | `	int       isProp;    /* 0 = subscript element, 1 = object property */` |
|         - |  195 | ``	int       bAppend;   /* element step with NO key: `f($a[])`, php's append. Only a by-REF`` |
|         - |  196 | `	                      * parameter may take one — a by-value binding is php's runtime` |
|         - |  197 | ``	                      * `Cannot use [] for reading` Error. */`` |
|         - |  198 | `	ph7_value sKey;      /* element: deep-copied index value (copied before pIdx is released) */` |
|         - |  199 | `	SyString  sProp;     /* property: name, into pName below (owned by the path allocation) */` |
|         - |  200 | `	char     *zProp;     /* property: owned copy of the name bytes (freed with the path) */` |
|         - |  201 | `};` |
|         - |  202 | `struct VmDeferredPath {` |
|         - |  203 | `	SyMemBackend *pAlloc;    /* allocator, so PH7_MemObjRelease can self-free without a pVm */` |
|         - |  204 | `	int           eRoot;     /* 0 = real container nIdx, 1 = undefined-var name, 2 = string base,` |
|         - |  205 | `	                          * 3 = a value ALREADY FETCHED (VM_DEFER_ROOT_PREFETCH below) */` |
|         - |  206 | `	sxu32         nRootIdx;  /* eRoot==0: aMemObj slot of the root container ($a/$o) */` |
|         - |  207 | `	SyString      sRootName; /* eRoot==1: variable name (VM-lifetime bytecode string, borrowed) */` |
|         - |  208 | `	sxu8          nOverKind; /* eRoot==3: which verdict a by-REFERENCE binding gets (VM_OVER_*) */` |
|         - |  209 | `	ph7_class    *pOverClass;/* eRoot==3: the class that answered — php's message names it */` |
|         - |  210 | `	SyString      sOverName; /* eRoot==3: the PROPERTY name (empty for an element) */` |
|         - |  211 | `	char         *zOverName; /* eRoot==3: owned bytes behind sOverName */` |
|         - |  212 | `	ph7_value     sPrefetch; /* eRoot==3: what the accessor answered */` |
|         - |  213 | `	sxu32         nStep;     /* number of captured steps (outer-to-inner) */` |
|         - |  214 | `	sxu32         nAlloc;    /* capacity of aStep */` |
|         - |  215 | `	VmDeferStep  *aStep;     /* captured steps */` |
|         - |  216 | `};` |
|         - |  217 | `/*` |
|         - |  218 | `` * eRoot == 3. Some fetches cannot be DEFERRED at all: a userland `offsetGet` (or a`` |
|         - |  219 | ` * subclass override of a native one) is a method call, and php runs it where the` |
|         - |  220 | ` * subscript is WRITTEN, whatever the parameter turns out to be. So the accessor runs` |
|         - |  221 | ` * at the fetch and the carrier holds its RESULT — the by-ref decision still arrives at` |
|         - |  222 | ``  * OP_CALL, and all it decides is php's `Indirect modification of overloaded element` `` |
|         - |  223 | ` * notice, since a value the container copied has no slot to alias either way.` |
|         - |  224 | ` */` |
|         - |  225 | `#define VM_DEFER_ROOT_PREFETCH 3` |
|         - |  226 | `/* What a by-REFERENCE binding of a prefetched value is, in php's words. All three are` |
|         - |  227 | ` * decided at the CALL because none of them is about the fetch: the same fetch feeding a` |
|         - |  228 | ` * by-VALUE parameter is silent. */` |
|         - |  229 | `#define VM_OVER_ELEM 0 /* Notice: Indirect modification of overloaded element of C has no effect */` |
|         - |  230 | `#define VM_OVER_PROP 1 /* Notice: Indirect modification of overloaded property C::$p has no effect */` |
|         - |  231 | `#define VM_OVER_HOOK 2 /* Error:  Indirect modification of C::$p is not allowed */` |
|         - |  232 | `/* Allowed value types.` |
|         - |  233 | ` */` |
|         - |  234 | `#define MEMOBJ_STRING    0x001  /* Memory value is a UTF-8 string */` |
|         - |  235 | `#define MEMOBJ_INT       0x002  /* Memory value is an integer */` |
|         - |  236 | `#define MEMOBJ_REAL      0x004  /* Memory value is a real number */` |
|         - |  237 | `#define MEMOBJ_BOOL      0x008  /* Memory value is a boolean */` |
|         - |  238 | `#define MEMOBJ_NULL      0x020  /* Memory value is NULL */` |
|         - |  239 | `#define MEMOBJ_HASHMAP   0x040  /* Memory value is a hashmap aka 'array' in the PHP jargon */` |
|         - |  240 | `#define MEMOBJ_OBJ       0x080  /* Memory value is an object [i.e: class instance] */` |
|         - |  241 | `#define MEMOBJ_RES       0x100  /* Memory value is a resource [User private data] */` |
|         - |  242 | `#define MEMOBJ_VOID      0x200  /* Pseudo-type: function must not return a value */` |
|         - |  243 | `#define MEMOBJ_REFERENCE 0x400  /* Memory value hold a reference (64-bit index) of another ph7_value */` |
|         - |  244 | `#define MEMOBJ_AUX_SPREAD 0x800 /* Stack-only marker: this value is a spread source for the next LOAD_MAP */` |
|         - |  245 | `#define MEMOBJ_NEVER     0x1000 /* Pseudo-type (return-only): never-returning function must not return at all */` |
|         - |  246 | `#define MEMOBJ_AUX_NOKEY 0x2000 /* Stack-only marker: absent array-literal key (see PH7_LOADC_NOKEY) */` |
|         - |  247 | `#define MEMOBJ_AUX_CUFVAL 0x4000 /* Stack-only marker: the ENGINE deliberately handed this by-ref` |
|         - |  248 | `                                  * argument a by-value copy, so the by-ref binder must NOT raise its` |
|         - |  249 | `                                  * "could not be passed by reference" Error for it. Two producers:` |
|         - |  250 | `                                  * call_user_func(), which php warns about and copies; and an` |
|         - |  251 | ``                                  * argument UNPACKED out of a temporary array (`f(...[1])`), whose`` |
|         - |  252 | `                                  * element php binds into a temporary nothing can observe. */` |
|         - |  253 | `#define MEMOBJ_AUX_DEFERRED 0x8000 /* Stack-only marker (D1): a deferred call argument whose target did` |
|         - |  254 | `                                    * not exist at load time. The value is NULL; x.pOther carries the` |
|         - |  255 | `                                    * lazy-lvalue descriptor (a plain-variable name pointer, or an` |
|         - |  256 | `                                    * element/property descriptor). OP_CALL's VmResolveDeferredArgs` |
|         - |  257 | `                                    * materializes it for a by-ref parameter or warns+passes NULL for a` |
|         - |  258 | `                                    * by-value one, clearing this flag. Never survives into a stored` |
|         - |  259 | `                                    * value: it is part of MEMOBJ_AUX, so MemObjStore strips it. */` |
|         - |  260 | `#define MEMOBJ_AUX_DEFPATH 0x10000 /* Stack-only marker (D1 commit 2): a deferred call argument that is an` |
|         - |  261 | `                                    * array-element ($a["k"]) or property ($o->p) lvalue whose target was` |
|         - |  262 | `                                    * ABSENT at load time. The value is NULL; x.pOther owns a heap` |
|         - |  263 | `                                    * VmDeferredPath (captured lvalue chain). VmResolveDeferredArgs re-walks` |
|         - |  264 | `                                    * it in vivify-mode (by-ref) or read+warn-mode (by-value). Unlike` |
|         - |  265 | `                                    * MEMOBJ_AUX_DEFERRED (a borrowed name pointer), this OWNS heap memory:` |
|         - |  266 | `                                    * PH7_MemObjRelease frees it at the TOP, before its MEMOBJ_NULL` |
|         - |  267 | `                                    * short-circuit, so every pop/abort/exception path releases it. Part of` |
|         - |  268 | `                                    * MEMOBJ_AUX, so MemObjStore strips the flag on copy. */` |
|         - |  269 | `#define MEMOBJ_AUX_COALSTROFF 0x40000 /* Stack-only marker: this NULL is the peek result of a` |
|         - |  270 | ``                                       * `$s[k] ??= v` over a STRING, and it OWNS a heap`` |
|         - |  271 | `                                       * VmCoalStrOff holding the RAW offset (x.pOther) for the` |
|         - |  272 | `                                       * OP_NULLC_STORE that follows — which has to write into` |
|         - |  273 | `                                       * the string OFFSET, and by then the offset value is gone.` |
|         - |  274 | `                                       * Same ownership contract as MEMOBJ_AUX_DEFPATH:` |
|         - |  275 | `                                       * PH7_MemObjRelease frees it, so an abandoned statement` |
|         - |  276 | `                                       * cannot leak it, and it nests (one carrier per pending` |
|         - |  277 | `                                       * ??= on the operand stack) where a single VM-wide slot` |
|         - |  278 | `                                       * could not. */` |
|         - |  279 | `#define MEMOBJ_AUX_MAGICCALL 0x80000 /* Stack-only marker: this callee slot is the engine's own` |
|         - |  280 | `                                      * __call/__callStatic dispatch, not a callable at all. OP_MEMBER` |
|         - |  281 | `                                      * sets it (with the receiver/class/original name latched on the` |
|         - |  282 | `                                      * VM) where a missing or inaccessible method must route through` |
|         - |  283 | `                                      * the magic handler; OP_CALL sees the mark BEFORE any callable` |
|         - |  284 | `                                      * decode and runs the packing body directly. It is what replaced` |
|         - |  285 | `                                      * the "__phl_magic_call" NAME the four OP_MEMBER sites used to` |
|         - |  286 | `                                      * write into this slot -- a hidden global function that` |
|         - |  287 | `                                      * function_exists() and get_defined_functions() both reported.` |
|         - |  288 | `                                      * The slot itself stays NULL-typed. Part of MEMOBJ_AUX, so a` |
|         - |  289 | `                                      * copy can never carry it. */` |
|         - |  290 | `#define MEMOBJ_AUX_MEMBERCALL 0x100000 /* Stack-only marker: this callee slot came out of an OP_MEMBER` |
|         - |  291 | `                                       * method resolution, which already DECIDED the call's` |
|         - |  292 | `                                       * visibility against the entry it actually chose. OP_CALL's` |
|         - |  293 | `                                       * own screen must then stand down: it re-derives the method` |
|         - |  294 | `                                       * from the FUNCTION's name against its declaring class, and` |
|         - |  295 | ``                                       * a trait adaptation splits those apart — `pub as private`` |
|         - |  296 | ``                                       * pHi` and `prot as public opened` share one struct name and`` |
|         - |  297 | `                                       * one sVmName with the method they were made from, so the` |
|         - |  298 | `                                       * re-derivation answered for the ORIGINAL and got the rule` |
|         - |  299 | `                                       * backwards in both directions. The mark rides the exact` |
|         - |  300 | `                                       * stack slot the call consumes (like MEMOBJ_AUX_MAGICCALL),` |
|         - |  301 | `                                       * so it cannot leak to another call the way a VM-wide latch` |
|         - |  302 | `                                       * could. Part of MEMOBJ_AUX, so a copy can never carry it. */` |
|         - |  303 | `#define MEMOBJ_AUX_ENGINEFN 0x200000 /* Stack-only marker: this callee STRING is one of the engine's` |
|         - |  304 | ``                                      * own function-table names (`[closure_N]`, and the`` |
|         - |  305 | ``                                      * `[__Class@meth_xxxxxxxxxx]` a mounted method is keyed under),`` |
|         - |  306 | `                                      * put there by the ENGINE rather than written by the program.` |
|         - |  307 | `                                      * It is what lets PH7_VmGetUserFunction refuse those names to a` |
|         - |  308 | `                                      * script -- function_exists() reported them, and calling a` |
|         - |  309 | `                                      * method's one underflowed the operand stack -- while the` |
|         - |  310 | `                                      * engine's own by-name dispatch of the same entries resolves.` |
|         - |  311 | `                                      * Set by the two SYNTHETIC call builders, which have no OP_MEMBER` |
|         - |  312 | `                                      * ahead of them to leave a mark: VmCallClassMethodLsb (a method's` |
|         - |  313 | `                                      * sVmName) and PH7_VmCallUserFunctionWithMap (a Closure unwrapped` |
|         - |  314 | ``                                      * to its `[closure_N]`). The in-line OP_CALL cases carry the same`` |
|         - |  315 | `                                      * verdict in a local instead -- MEMOBJ_AUX_MEMBERCALL for a` |
|         - |  316 | `                                      * resolved method, the unwrap branch for a closure -- because the` |
|         - |  317 | `                                      * member mark is consumed before the lookup. Part of MEMOBJ_AUX,` |
|         - |  318 | `                                      * so a copy can never carry it. */` |
|         - |  319 | `#define MEMOBJ_AUX_STROFFSET 0x20000 /* Stack-only marker: this value was READ OUT of a string by a` |
|         - |  320 | `                                      * subscript ($s[1]). It carries the BASE's slot index like any` |
|         - |  321 | `                                      * other element read, but a string offset is not a slot: php` |
|         - |  322 | `                                      * refuses to make a reference to one` |
|         - |  323 | `                                      * ("Cannot create references to/from string offsets"), and` |
|         - |  324 | `                                      * binding the index anyway aliased the WHOLE STRING — writing` |
|         - |  325 | `                                      * through the reference replaced it. The reference-binding` |
|         - |  326 | `                                      * sites test this. Part of MEMOBJ_AUX, so MemObjStore strips` |
|         - |  327 | ``                                      * it: a plain `$c = $s[1]` copy carries nothing. */`` |
|         - |  328 | `#define MEMOBJ_AUX_NATIVEPROP 0x400000 /* Stack-only marker: this value was read out of a NATIVE` |
|         - |  329 | `                                      * class's handler-backed property (PH7_CLASS_ATTR_NATIVE_SET),` |
|         - |  330 | `                                      * which is a field of php's own C struct rather than storage a` |
|         - |  331 | `                                      * script may alias. It says the value carries no slot on` |
|         - |  332 | `                                      * purpose, so the reference-binding site makes a silent COPY` |
|         - |  333 | `                                      * instead of raising the "require a variable not a constant"` |
|         - |  334 | ``                                      * diagnostic — php binds `$r = &$i->f` to a temporary and says`` |
|         - |  335 | `                                      * nothing. Part of MEMOBJ_AUX, so a copy can never carry it. */` |
|         - |  336 | `#define MEMOBJ_STREAMRES 0x1000000 /* This MEMOBJ_RES names a STREAM HANDLE (an io_private),` |
|         - |  337 | `                                    * and the value holds one of that handle's counted` |
|         - |  338 | `                                    * references. It marks the VALUE rather than the object` |
|         - |  339 | `                                    * because the object is the one thing a release cannot` |
|         - |  340 | `                                    * look at: a Generator's context resource is FREED by its` |
|         - |  341 | `                                    * own destructor before the slot naming it is released, so` |
|         - |  342 | `                                    * a probe of the pointer -- at any offset -- is a` |
|         - |  343 | `                                    * use-after-free. Not part of MEMOBJ_AUX, so it survives a` |
|         - |  344 | `                                    * store the way the type bits do; MemObjSetType clears it` |
|         - |  345 | `                                    * with them, so a slot retyped to another resource kind` |
|         - |  346 | `                                    * cannot inherit it. */` |
|         - |  347 | `/* Mask of all known types */` |
|         - |  348 | `#define MEMOBJ_ALL (MEMOBJ_STRING\|MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)` |
|         - |  349 | `/* Scalar variables` |
|         - |  350 | ` * According to the PHP language reference manual` |
|         - |  351 | ` *  Scalar variables are those containing an integer, float, string or boolean.` |
|         - |  352 | ` *  Types array, object and resource are not scalar.` |
|         - |  353 | ` */` |
|         - |  354 | `#define MEMOBJ_AUX_REFRET 0x800000 /* Stack-only marker: this value is the result of a call to a` |
|         - |  355 | `                                    * function DECLARED to return by reference. php raises` |
|         - |  356 | ``                                     * `Only variable references should be returned by reference` `` |
|         - |  357 | `                                    * at the RETURN when such a function has no variable to` |
|         - |  358 | `                                    * bind, and says nothing more at the call site -- so the` |
|         - |  359 | ``                                    * `Only variables should be assigned by reference` notice,`` |
|         - |  360 | `                                    * which is about a callee that never promised a reference,` |
|         - |  361 | `                                    * stands down for a value carrying this. */` |
|         - |  362 | `#define MEMOBJ_POOLFREE 0x1000000  /* NOT a type or a stack marker: pool bookkeeping. This slot is` |
|         - |  363 | `                                    * ON the value pool's intrusive free list (see VmMemPool), so` |
|         - |  364 | `                                    * its nIdx word is the link to the next free slot and NOT its` |
|         - |  365 | `                                    * own index. Set by VmMemPoolFreeSlot, cleared by the` |
|         - |  366 | `                                    * PH7_MemObjInit every acquire runs and by VmMemPoolTruncate` |
|         - |  367 | `                                    * when it abandons the chain. It exists to make a double free` |
|         - |  368 | `                                    * a no-op: the link lives inside the slot, so freeing the same` |
|         - |  369 | `                                    * index twice would point the head at itself and hand that one` |
|         - |  370 | `                                    * slot out for the rest of the run. Deliberately survives` |
|         - |  371 | `                                    * PH7_MemObjRelease, which leaves iFlags alone once a value is` |
|         - |  372 | `                                    * already MEMOBJ_NULL -- and a slot on the list always is. */` |
|         - |  373 | `#define MEMOBJ_SCALAR (MEMOBJ_STRING\|MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_NULL)` |
|         - |  374 | `#define MEMOBJ_AUX (MEMOBJ_REFERENCE\|MEMOBJ_AUX_SPREAD\|MEMOBJ_AUX_NOKEY\|MEMOBJ_AUX_CUFVAL\|MEMOBJ_AUX_DEFERRED\|MEMOBJ_AUX_DEFPATH\|MEMOBJ_AUX_STROFFSET\|MEMOBJ_AUX_COALSTROFF\|MEMOBJ_AUX_MAGICCALL\|MEMOBJ_AUX_MEMBERCALL\|MEMOBJ_AUX_ENGINEFN\|MEMOBJ_AUX_NATIVEPROP\|MEMOBJ_AUX_REFRET)` |
|         - |  375 | `/* Closure-instance flags (ph7_class_instance.iFlags), shared by vm_exec.c's OP_LOAD_FCC` |
|         - |  376 | ` * and vm_exec_ctx.c's closure machinery. Distinct from CLASS_INSTANCE_DESTROYED 0x001` |
|         - |  377 | ` * (oo.c) and VM_INSTANCE_DUMPING 0x002, which share the same word. */` |
|         - |  378 | `/* ph7_class_instance.iFlags bit: this Closure is a bound/static first-class callable and` |
|         - |  379 | ` * carries $__this/$__scope. Lets the hot plain-closure unwrap skip those attribute lookups.` |
|         - |  380 | ` * (Distinct from CLASS_INSTANCE_DESTROYED 0x001 and VM_INSTANCE_DUMPING 0x002.) */` |
|         - |  381 | `#define VM_INSTANCE_FCC_BOUND 0x004` |
|         - |  382 | `/* ph7_class_instance.iFlags bit: this Closure wraps an __invoke OBJECT, and the engine —` |
|         - |  383 | `` * not the source — is what named `__invoke` (Closure::fromCallable($obj)). php resolves it`` |
|         - |  384 | `` * the way it resolves `$obj()`, so a non-public __invoke is dispatched rather than denied;`` |
|         - |  385 | `` * `$obj->__invoke(...)` and `[$obj,'__invoke']`, which the SOURCE names, stay denied and`` |
|         - |  386 | ` * never carry this bit. Read by VmClosureUnwrap, which arms the engine's magic latch. */` |
|         - |  387 | `#define VM_INSTANCE_FCC_INVOKE_OBJ 0x010` |
|         - |  388 | `/* ph7_class_instance.iFlags bit: this Closure's $__fn names a METHOD of $__this's class (or` |
|         - |  389 | `` * of $__scope) — `$o->m(...)`, `C::m(...)`, `Closure::fromCallable([$o,'m'])`. Without it the`` |
|         - |  390 | ` * unwrap had to GUESS, by asking whether the class declares a method of that name, and it` |
|         - |  391 | ` * guessed wrong in both directions: a name the class answers only through __call fell through` |
|         - |  392 | ` * to the plain-function route and failed with "Call to undefined function m()", and a name` |
|         - |  393 | ` * that happened to match a global function ran the FUNCTION. A bound plain closure` |
|         - |  394 | `` * (`function(){…}->bindTo($o)`) never carries this bit, which is what the guess was really`` |
|         - |  395 | ` * trying to detect. */` |
|         - |  396 | `#define VM_INSTANCE_FCC_METHOD 0x020` |
|         - |  397 | `/* ph7_class_instance.iFlags bit: this Closure's callee was RESOLVED to a real, directly` |
|         - |  398 | ` * callable method when the closure was BUILT — the way php resolves one, keeping the` |
|         - |  399 | ` * function itself rather than a name. No dispatch site may re-decide its visibility against` |
|         - |  400 | `` * the CALLER: that is what killed an escaped `$this->priv(...)` php runs anywhere. A closure`` |
|         - |  401 | ` * whose creation resolved to the class's __call/__callStatic TRAMPOLINE instead (a missing or` |
|         - |  402 | ` * inaccessible name on a class that declares one) deliberately does NOT carry the bit — its` |
|         - |  403 | ` * dispatch has to reach the catch-all, as php's does. */` |
|         - |  404 | `#define VM_INSTANCE_FCC_SCREENED 0x040` |
|         - |  405 | `/*` |
|         - |  406 | ` * The following macro clear the current ph7_value type and replace` |
|         - |  407 | ` * it with the given one.` |
|         - |  408 | ` */` |
|         - |  409 | `#define MemObjSetType(OBJ,TYPE) ((OBJ)->iFlags = ((OBJ)->iFlags&~(MEMOBJ_ALL\|MEMOBJ_STREAMRES))\|TYPE)` |
|         - |  410 | `/*` |
|         - |  411 | ` * Signed 64-bit arithmetic with overflow detection. PHP promotes an integer` |
|         - |  412 | ` * operation that overflows sxi64 to a floating-point result, so the executor` |
|         - |  413 | ` * checks for overflow on every +,-,* (and ++/--) and re-runs the operation in` |
|         - |  414 | ` * double precision when it trips. GCC/Clang expose the __builtin_*_overflow` |
|         - |  415 | ` * intrinsics (zero cost, no UB); MSVC lacks them, so we fall back to portable` |
|         - |  416 | ` * implementations defined in memobj.c. Each macro sets *pR to the wrapped` |
|         - |  417 | ` * result and evaluates to non-zero on overflow.` |
|         - |  418 | ` */` |
|         - |  419 | `#if defined(__GNUC__) \|\| defined(__clang__)` |
|         - |  420 | `#define PH7_ADD_OVERFLOW64(a,b,pR) __builtin_add_overflow((a),(b),(pR))` |
|         - |  421 | `#define PH7_SUB_OVERFLOW64(a,b,pR) __builtin_sub_overflow((a),(b),(pR))` |
|         - |  422 | `#define PH7_MUL_OVERFLOW64(a,b,pR) __builtin_mul_overflow((a),(b),(pR))` |
|         - |  423 | `#else` |
|         - |  424 | `PH7_PRIVATE int PH7_AddOverflow64(sxi64 a,sxi64 b,sxi64 *pR);` |
|         - |  425 | `PH7_PRIVATE int PH7_SubOverflow64(sxi64 a,sxi64 b,sxi64 *pR);` |
|         - |  426 | `PH7_PRIVATE int PH7_MulOverflow64(sxi64 a,sxi64 b,sxi64 *pR);` |
|         - |  427 | `#define PH7_ADD_OVERFLOW64(a,b,pR) PH7_AddOverflow64((a),(b),(pR))` |
|         - |  428 | `#define PH7_SUB_OVERFLOW64(a,b,pR) PH7_SubOverflow64((a),(b),(pR))` |
|         - |  429 | `#define PH7_MUL_OVERFLOW64(a,b,pR) PH7_MulOverflow64((a),(b),(pR))` |
|         - |  430 | `#endif` |
|         - |  431 | `/* ph7_value cast method signature */` |
|         - |  432 | `typedef sxi32 (*ProcMemObjCast)(ph7_value *);` |
|         - |  433 | `/* Forward reference */` |
|         - |  434 | `typedef struct ph7_output_consumer ph7_output_consumer;` |
|         - |  435 | `/*` |
|         - |  436 | ` * One parameter of a builtin's declared signature, as the shared argument screen` |
|         - |  437 | ` * (VmEnforceBuiltinArgTypes) needs to see it: the type TEXT, the parameter name, and` |
|         - |  438 | `` * the two markers the text carries -- `&` for by-reference and a leading `~` for`` |
|         - |  439 | ` * php's stub-versus-body disagreement. Every pointer is INTO zSig, which is static` |
|         - |  440 | ` * storage; nothing here is copied and nothing here is freed.` |
|         - |  441 | ` *` |
|         - |  442 | ` * Getting this out of zSig is a walk -- skip spaces, find the comma that ends the` |
|         - |  443 | ` * parameter (honouring a quoted default, which can contain one), find the '$', check` |
|         - |  444 | ` * for a variadic '...', trim the type's trailing spaces and '&'. The screen did that` |
|         - |  445 | ` * walk for every argument of every builtin call: 18,901,261 parameters on the` |
|         - |  446 | ` * ecosystem gate's phpcs step, for an answer that is a property of the DECLARATION` |
|         - |  447 | ` * and cannot change between two calls.` |
|         - |  448 | ` */` |
|         - |  449 | `typedef struct VmArgScreenParam VmArgScreenParam;` |
|         - |  450 | `struct VmArgScreenParam` |
|         - |  451 | `{` |
|         - |  452 | `	const char *zType;   /* into zSig; nType 0 means untyped and unscreened */` |
|         - |  453 | `	const char *zName;   /* into zSig, past the '$' */` |
|         - |  454 | `	sxu16 nType;` |
|         - |  455 | `	sxu16 nName;` |
|         - |  456 | `	sxu8 bByRef;         /* "array &$array" */` |
|         - |  457 | `	sxu8 bStub;          /* "~Type $p": the builtin raises its own TypeError */` |
|         - |  458 | `	sxu32 nMask;         /* VMSIG_* -- which arms this declared type has. The screen asks` |
|         - |  459 | `	                      * that question up to forty-four times per argument, and every` |
|         - |  460 | `	                      * ask was a split-on-'\|' walk over the same text. The bits are` |
|         - |  461 | `	                      * SET by calling the very functions they replace (see` |
|         - |  462 | `	                      * VmArgScreenNext), so a bit cannot mean something the walk did` |
|         - |  463 | `	                      * not say. */` |
|         - |  464 | `};` |
|         - |  465 | `/* The arms VmArgScreenParam::nMask records. The first thirteen are VmSigTypeHas() tokens;` |
|         - |  466 | ` * the last three are the three composite questions the screen asks about a whole type. */` |
|         - |  467 | `#define VMSIG_MIXED      0x00000001` |
|         - |  468 | `#define VMSIG_ARRAY      0x00000002` |
|         - |  469 | `#define VMSIG_ITERABLE   0x00000004` |
|         - |  470 | `#define VMSIG_CALLABLE   0x00000008` |
|         - |  471 | `#define VMSIG_OBJECT     0x00000010` |
|         - |  472 | `#define VMSIG_STRING     0x00000020` |
|         - |  473 | `#define VMSIG_NULL       0x00000040` |
|         - |  474 | `#define VMSIG_INT        0x00000080` |
|         - |  475 | `#define VMSIG_FLOAT      0x00000100` |
|         - |  476 | `#define VMSIG_BOOL       0x00000200` |
|         - |  477 | `#define VMSIG_TRUE       0x00000400` |
|         - |  478 | `#define VMSIG_FALSE      0x00000800` |
|         - |  479 | `#define VMSIG_RESOURCE   0x00001000` |
|         - |  480 | `#define VMSIG_CLASS      0x00002000   /* VmSigTypeHasClass: an arm that is not a builtin type */` |
|         - |  481 | `#define VMSIG_INTONLY    0x00004000   /* VmSigTypeIsIntOnly */` |
|         - |  482 | `#define VMSIG_ARRAYONLY  0x00008000   /* VmSigTypeIsArrayOnly */` |
|         - |  483 | `typedef struct ph7_user_func ph7_user_func;` |
|         - |  484 | `typedef struct ph7_conf ph7_conf;` |
|         - |  485 | `/*` |
|         - |  486 | ` * An instance of the following structure store the default VM output` |
|         - |  487 | ` * consumer and it's private data.` |
|         - |  488 | ` * Client-programs can register their own output consumer callback` |
|         - |  489 | ` * via the [PH7_VM_CONFIG_OUTPUT] configuration directive.` |
|         - |  490 | ` * Please refer to the official documentation for more information` |
|         - |  491 | ` * on how to register an output consumer callback.` |
|         - |  492 | ` */` |
|         - |  493 | `struct ph7_output_consumer` |
|         - |  494 | `{` |
|         - |  495 | `	ProcConsumer xConsumer; /* VM output consumer routine */` |
|         - |  496 | `	void *pUserData;        /* Third argument to xConsumer() */` |
|         - |  497 | `	ProcConsumer xDef;      /* Default output consumer routine */` |
|         - |  498 | `	void *pDefData;         /* Third argument to xDef() */` |
|         - |  499 | `};` |
|         - |  500 | `/*` |
|         - |  501 | ` * PH7 engine [i.e: ph7 instance] configuration is stored in` |
|         - |  502 | ` * an instance of the following structure.` |
|         - |  503 | ` * Please refer to the official documentation for more information` |
|         - |  504 | ` * on how to configure your ph7 engine instance.` |
|         - |  505 | ` */` |
|         - |  506 | `struct ph7_conf` |
|         - |  507 | `{` |
|         - |  508 | `	ProcConsumer xErr;   /* Compile-time error consumer callback (the LOG channel) */` |
|         - |  509 | `	void *pErrData;      /* Third argument to xErr() */` |
|         - |  510 | `	ProcConsumer xOut;   /* Program-output consumer for a compile diagnostic's DISPLAY copy` |
|         - |  511 | `	                      * [PH7_CONFIG_OUTPUT]. The VM's own output consumer does not exist` |
|         - |  512 | `	                      * yet while the main script compiles, and the display copy is` |
|         - |  513 | `	                      * program output — falling back to xErr put it on the log stream. */` |
|         - |  514 | `	void *pOutData;      /* Third argument to xOut() */` |
|         - |  515 | `	SyBlob sErrConsumer; /* Default error consumer */` |
|         - |  516 | `	ph7_clock xClock;    /* Optional embedder clock [PH7_CONFIG_CLOCK]; NULL => platform default */` |
|         - |  517 | `	void *pClockData;    /* Third argument to xClock() */` |
|         - |  518 | `	sxu32 nMaxInput;     /* Per-compile input byte cap [PH7_CONFIG_MAX_INPUT]; 0 = PH7_MAX_INPUT_SIZE */` |
|         - |  519 | `	SySet aIniEntry;     /* php.ini directives applied to every VM at birth [PH7_CONFIG_INI_ENTRY]:` |
|         - |  520 | `	                      * VmIniEntry copies on the ENGINE allocator, replayed by ph7VmInit` |
|         - |  521 | `	                      * before the unit is compiled. A directive that only ever reached a` |
|         - |  522 | `	                      * finished VM could not gate that unit's own compile diagnostics:` |
|         - |  523 | `	                      * ph7_compile_file is what CREATES the VM. */` |
|         - |  524 | `	int bErrReport;      /* PH7_CONFIG_ERR_REPORT: seed a new VM's reporting mask to E_ALL,` |
|         - |  525 | ``	                      * before the aIniEntry replay so `-d error_reporting=` still wins */`` |
|         - |  526 | `	SyString sIniFile;   /* PH7_CONFIG_INI_FILE: the php.ini file that was read, canonical, on` |
|         - |  527 | `	                      * the ENGINE allocator. Empty when none was; php_ini_loaded_file()` |
|         - |  528 | ``	                      * answers FALSE then, which is php's own answer to `php -n`. */`` |
|         - |  529 | `};` |
|         - |  530 | `/*` |
|         - |  531 | ` * Signature of the C function responsible of expanding constant values.` |
|         - |  532 | ` */` |
|         - |  533 | `typedef void (*ProcConstant)(ph7_value *,void *);` |
|         - |  534 | `/*` |
|         - |  535 | ` * Each registered constant [i.e: __TIME__, __DATE__, PHP_OS, INT_MAX, etc.] is stored` |
|         - |  536 | ` * in an instance of the following structure.` |
|         - |  537 | ` * Please refer to the official documentation for more information` |
|         - |  538 | ` * on how to create/install foreign constants.` |
|         - |  539 | ` */` |
|         - |  540 | `typedef struct ph7_constant ph7_constant;` |
|         - |  541 | `struct ph7_constant` |
|         - |  542 | `{` |
|         - |  543 | `	SyString sName;        /* Constant name */` |
|         - |  544 | `	ProcConstant xExpand;  /* Function responsible of expanding constant value */` |
|         - |  545 | `	void *pUserData;       /* Last argument to xExpand() */` |
|         - |  546 | `	SyString sFile;        /* Defining file (aliases the VM-lifetime dup in pVm->aFiles);` |
|         - |  547 | `	                        * nByte == 0 = unknown/engine constant */` |
|         - |  548 | ``	sxu32 nLine;           /* Declaration line for `const`; 0 for define()/engine */`` |
|         - |  549 | `	sxu8 bUserDefined;     /* 1 when created by user code (const / define()):` |
|         - |  550 | `	                        * Reflection isInternal()/getFileName() input */` |
|         - |  551 | `	const char *zDeprecated; /* php's reason clause when the SYMBOL is deprecated` |
|         - |  552 | `	                        * ("8.1, as the constant has no effect"), else NULL.` |
|         - |  553 | `	                        * Static storage. Naming the constant raises php's` |
|         - |  554 | `	                        * E_DEPRECATED; LISTING the table does not. */` |
|         - |  555 | `	SySet aAttrs;          /* Declared #[...] attributes (ph7_attribute records) —` |
|         - |  556 | ``	                        * php 8.5 attributes on `const` statements */`` |
|         - |  557 | `};` |
|         - |  558 | `typedef struct ph7_aux_data ph7_aux_data;` |
|         - |  559 | `/*` |
|         - |  560 | ` * Auxiliary data associated with each foreign function is stored` |
|         - |  561 | ` * in a stack of the following structure.` |
|         - |  562 | ` * Note that automatic tracked chunks are also stored in an instance` |
|         - |  563 | ` * of this structure.` |
|         - |  564 | ` */` |
|         - |  565 | `struct ph7_aux_data` |
|         - |  566 | `{` |
|         - |  567 | `	void *pAuxData; /* Aux data */` |
|         - |  568 | `};` |
|         - |  569 | `/* Foreign functions signature */` |
|         - |  570 | `typedef int (*ProchHostFunction)(ph7_context *,int,ph7_value **);` |
|         - |  571 | `/*` |
|         - |  572 | ` * Each installed foreign function is recored in an instance of the following` |
|         - |  573 | ` * structure.` |
|         - |  574 | ` * Please refer to the official documentation for more information on how` |
|         - |  575 | ` * to create/install foreign functions.` |
|         - |  576 | ` */` |
|         - |  577 | `/*` |
|         - |  578 | ` * One name php 8.x deprecated, and the clause it ends the notice with.` |
|         - |  579 | ` *` |
|         - |  580 | `` * The SUBJECT is php's own spelling -- `curl_close` for a function,`` |
|         - |  581 | `` * `SplObjectStorage::attach` for a method, where php always names the`` |
|         - |  582 | ` * DECLARING class even for a call through a subclass -- so the raise site` |
|         - |  583 | ` * needs no class lookup of its own. Static storage, shared by the E_DEPRECATED` |
|         - |  584 | `` * notice and the export format's `<internal, deprecated:EXT>` tag.`` |
|         - |  585 | ` */` |
|         - |  586 | `typedef struct ph7_deprecated_name ph7_deprecated_name;` |
|         - |  587 | `struct ph7_deprecated_name` |
|         - |  588 | `{` |
|         - |  589 | `	const char *zName;  /* php's subject, without the trailing "()" */` |
|         - |  590 | `	const char *zWhy;   /* what follows "is deprecated since ": "8.2",` |
|         - |  591 | `	                     * "8.5, use method SplObjectStorage::offsetSet() instead" */` |
|         - |  592 | `};` |
|         - |  593 | `struct ph7_user_func` |
|         - |  594 | `{` |
|         - |  595 | `	ph7_vm *pVm;              /* VM that own this instance */` |
|         - |  596 | `	SyString sName;           /* Foreign function name */` |
|         - |  597 | `	ProchHostFunction xFunc;  /* Implementation of the foreign function */` |
|         - |  598 | `	void *pUserData;          /* User private data [Refer to the official documentation for more information]*/` |
|         - |  599 | `	SySet aAux;               /* Stack of auxiliary data [Refer to the official documentation for more information]*/` |
|         - |  600 | `	sxi16 nMinArg;            /* Minimum required arguments for the PHP-8 ArgumentCountError` |
|         - |  601 | `	                           * check at the OP_CALL choke point; 0 = no central enforcement` |
|         - |  602 | `	                           * (the builtin self-validates, or genuinely accepts zero args). */` |
|         - |  603 | `	sxu8 bAtLeast;            /* 0 -> "expects exactly N", 1 -> "expects at least N" (the` |
|         - |  604 | `	                           * wording depends on whether the builtin has optional params). */` |
|         - |  605 | `	sxu8 bHasMaxArg;          /* 0 -> no central too-many-arguments check (the SAFE default: this` |
|         - |  606 | `	                           * struct is SyZero'd on creation, so an unstamped builtin must mean` |
|         - |  607 | `	                           * "unenforced", never "accepts at most zero"). 1 -> nMaxArg applies. */` |
|         - |  608 | `	sxi16 nMaxArg;            /* Maximum accepted arguments when bHasMaxArg; derived from the` |
|         - |  609 | `	                           * signature. A variadic parameter leaves bHasMaxArg at 0. */` |
|         - |  610 | `	const char *zSig;         /* PHP-style parameter list ("string $s, int $o = 0") from the` |
|         - |  611 | `	                           * static signature table, or NULL: ReflectionFunction input for` |
|         - |  612 | `	                           * internal functions. Points at static storage — never freed. */` |
|         - |  613 | `	const char *zRet;         /* Return-type text from the same table, or NULL */` |
|         - |  614 | `	const ph7_deprecated_name *pDeprecated; /* php's deprecation for this name, or NULL. A` |
|         - |  615 | `	                           * NATIVE method reaches it through ph7_vm_func::pNative, so one` |
|         - |  616 | `	                           * field covers both a C builtin and a native class method --` |
|         - |  617 | `	                           * they share this struct and the same OP_CALL block. */` |
|         - |  618 | ``	sxu32 nByRefMask;         /* D1: bit N set => positional parameter N is by-reference (`&$p` in zSig),`` |
|         - |  619 | `	                           * derived once in VmSetBuiltinSignatures. Lets OP_CALL materialize a` |
|         - |  620 | `	                           * deferred by-ref out-param regardless of how the builtin was reached` |
|         - |  621 | ``	                           * (bare name, dynamic `$f=...`, or callable) — the compile-time`` |
|         - |  622 | `	                           * GenStateByRefBuiltinMask only sees the bare-name case. 0 when unstamped. */` |
|         - |  623 | `	sxu32 nPathMask;          /* Which of this builtin's arguments php reads with Z_PARAM_PATH,` |
|         - |  624 | `	                           * so a NUL inside one is a catchable ValueError rather than a` |
|         - |  625 | `	                           * truncated read. Derived from a ~70-name table, and the two` |
|         - |  626 | `	                           * questions the shared argument screen used to ask by SCANNING` |
|         - |  627 | `	                           * that table (and a second one) on every single builtin call.` |
|         - |  628 | `	                           * Both answers depend on the NAME alone, so they are worked out` |
|         - |  629 | `	                           * the first time this function is called and kept here. */` |
|         - |  630 | `	sxu8 bConstruct;          /* This name is a language CONSTRUCT, not a function: php has no` |
|         - |  631 | ``	                           * `empty`, `isset`, `unset`, `eval`, `print`, `include`,`` |
|         - |  632 | ``	                           * `include_once`, `require` or `require_once` in its function`` |
|         - |  633 | `	                           * table, and every door that answers about a name says so.` |
|         - |  634 | `	                           * The record stays -- the compiler dispatches the construct` |
|         - |  635 | `	                           * through it -- but PH7_VmGetHostFunction hides it from every` |
|         - |  636 | `	                           * SCRIPT-spelled lookup. See PH7_CALL_CONSTRUCT. */` |
|         - |  637 | `	sxu8 bSelfChecked;        /* This builtin words its own argument refusals and must not be` |
|         - |  638 | `	                           * pre-empted by the shared screen (php overloads it on arity, or` |
|         - |  639 | `	                           * its declared type and its refusal text disagree). */` |
|         - |  640 | `	sxu8 bScreenStamped;      /* Everything this screen keeps on the record -- nPathMask,` |
|         - |  641 | `	                           * bSelfChecked, nSigLen and the aSigParam table -- has been` |
|         - |  642 | `	                           * worked out. The struct is SyZero'd at creation, so 0 means` |
|         - |  643 | `	                           * "not yet" and never "no".` |
|         - |  644 | `	                           * Stamped lazily rather than at VM init because a NATIVE` |
|         - |  645 | `	                           * METHOD's record is reached through ph7_vm_func::pNative and` |
|         - |  646 | `	                           * is not in the host function table the init pass walks. */` |
|         - |  647 | `	VmArgScreenParam *aSigParam; /* zSig's parameters, parsed ONCE (see the struct above and` |
|         - |  648 | `	                           * VmArgScreenStamp). 0 when there are none, or when the` |
|         - |  649 | `	                           * allocation failed -- the screen then walks the text, which` |
|         - |  650 | `	                           * is the same code and the same answers, just per call. */` |
|         - |  651 | `	sxu16 nSigParam;          /* how many aSigParam holds; beyond it the screen stops, which` |
|         - |  652 | `	                           * is what the walk did at a variadic tail or a malformed row. */` |
|         - |  653 | `	sxu32 nSigLen;            /* SyStrlen(zSig), worked out with the rest. zSig is a` |
|         - |  654 | `	                           * literal from aBuiltinSig[] (or a native method's table) and` |
|         - |  655 | `	                           * is assigned exactly once, so its length is a constant of the` |
|         - |  656 | `	                           * DECLARATION -- but the shared argument screen measured it on` |
|         - |  657 | `	                           * every call, which on the ecosystem gate's phpcs step was` |
|         - |  658 | `	                           * 425,987,828 bytes of strlen across 11,391,725 calls.` |
|         - |  659 | `	                           * Only meaningful once bScreenStamped. */` |
|         - |  660 | `};` |
|         - |  661 | `/*` |
|         - |  662 | ` * The 'context' argument for an installable function. A pointer to an` |
|         - |  663 | ` * instance of this structure is the first argument to the routines used` |
|         - |  664 | ` * implement the foreign functions.` |
|         - |  665 | ` */` |
|         - |  666 | `typedef struct VmCallArgMap VmCallArgMap; /* Forward decl; full definition below. */` |
|         - |  667 | `struct ph7_context` |
|         - |  668 | `{` |
|         - |  669 | `	ph7_user_func *pFunc;   /* Function information. */` |
|         - |  670 | `	ph7_value *pRet;        /* Return value is stored here. */` |
|         - |  671 | `	SySet sVar;             /* Container of dynamically allocated ph7_values` |
|         - |  672 | `							 * [i.e: Garbage collection purposes.]` |
|         - |  673 | `							 */` |
|         - |  674 | `	SySet sChunk;           /* Track dynamically allocated chunks [ph7_aux_data instance].` |
|         - |  675 | `							 * [i.e: Garbage collection purposes.]` |
|         - |  676 | `							 */` |
|         - |  677 | `	ph7_vm *pVm;            /* Virtual machine that own this context */` |
|         - |  678 | `	sxi32 iFlags;           /* Call flags (PH7_CTX_CALL_*) */` |
|         - |  679 | `	sxi32 nThrowRc;         /* Status of a throw this host function raised through` |
|         - |  680 | `	                         * PH7_VmThrowException (0 when it never threw). The` |
|         - |  681 | `	                         * OP_CALL boundary re-reads it: a builtin that threw and` |
|         - |  682 | `	                         * still returned PH7_OK would otherwise let the VM carry` |
|         - |  683 | `	                         * on inside the try the throw abandoned. See` |
|         - |  684 | `	                         * VmHostFuncThrowRc(). */` |
|         - |  685 | `	VmCallArgMap *pArgMap;  /* Call-site named-argument map (or 0). Lets a builtin` |
|         - |  686 | `	                         * such as call_user_func forward its callers' name:` |
|         - |  687 | `	                         * arguments to the inner callback. */` |
|         - |  688 | `	ph7_class_instance *pThis; /* VM_FUNC_NATIVE method only: the receiver, or 0 for a static` |
|         - |  689 | `	                         * call and for every plain host function. Read through` |
|         - |  690 | `	                         * PH7_ContextThis(); the reference is owned by the CALLER for the` |
|         - |  691 | `	                         * duration of the call, so a native body must not unref it. */` |
|         - |  692 | `	ph7_class *pCalledClass;/* VM_FUNC_NATIVE method only: the class the call was made` |
|         - |  693 | `	                         * THROUGH (php's late-static-binding target), which for an` |
|         - |  694 | `	                         * inherited method is the subclass, not the declaring class. 0 for` |
|         - |  695 | `	                         * a plain host function. */` |
|         - |  696 | `	ph7_value sThis;        /* Scratch MEMOBJ_OBJ view of pThis, materialized on the first` |
|         - |  697 | `	                         * PH7_ContextThisValue() call so a native body can reach the` |
|         - |  698 | `	                         * receiver through the ordinary ph7_value object helpers` |
|         - |  699 | `	                         * (ph7_object_fetch_attr & co). bThisInit gates the lazy init;` |
|         - |  700 | `	                         * VmReleaseCallContext tears it down. */` |
|         - |  701 | `	sxu8 bThisInit;         /* 1 once sThis has been initialized */` |
|         - |  702 | `	struct PH7_NativePropCtx *pPropCtx; /* Non-zero while this SCRATCH context is running a native` |
|         - |  703 | `	                         * class's property handler (ph7_class::xProp). A handler shares` |
|         - |  704 | `	                         * its bodies with the ordinary method path, and those raise a` |
|         - |  705 | `	                         * refusal by throwing -- which here would run the enclosing catch` |
|         - |  706 | `	                         * in the middle of the member opcode. With this set the DOM` |
|         - |  707 | `	                         * refusal helpers RECORD into the hook's context instead, and the` |
|         - |  708 | `	                         * opcode raises it where the access would have landed. */` |
|         - |  709 | `};` |
|         - |  710 | `/* ph7_context::iFlags */` |
|         - |  711 | `#define PH7_CTX_CALL_DYNAMIC 0x01 /* php's ZEND_CALL_DYNAMIC: this host function was reached` |
|         - |  712 | ``                                   * through a name the program computed (`$n()`), a Closure`` |
|         - |  713 | ``                                   * (`compact(...)`, Closure::fromCallable), or an internal`` |
|         - |  714 | `                                   * function driving a callback (array_map, usort,` |
|         - |  715 | `                                   * Reflection's invoke, a call_user_func php's compiler could` |
|         - |  716 | `                                   * not fold into a direct call). The six functions that read` |
|         - |  717 | `                                   * their CALLER's frame (compact, extract, get_defined_vars,` |
|         - |  718 | `                                   * func_get_args, func_num_args, func_get_arg) refuse such a` |
|         - |  719 | `                                   * call the way php does -- see PH7_VmForbidDynamicCall. */` |
|         - |  720 | `/*` |
|         - |  721 | ` * Each hashmap entry [i.e: array(4,5,6)] is recorded in an instance` |
|         - |  722 | ` * of the following structure.` |
|         - |  723 | ` */` |
|         - |  724 | `/* Allowed hashmap node key types (iType below) */` |
|         - |  725 | `#define HASHMAP_INT_NODE   1  /* Node with an int [i.e: 64-bit integer] key */` |
|         - |  726 | `#define HASHMAP_BLOB_NODE  2  /* Node with a string/BLOB key */` |
|         - |  727 | `/* Node control flags (iFlags below) */` |
|         - |  728 | `#define HASHMAP_NODE_FOREIGN_OBJ 0x001 /* Node holds a reference to a foreign ph7_value` |
|         - |  729 | `                                        * [i.e: array(&var) / $a[] =& $var ] */` |
|         - |  730 | `/*` |
|         - |  731 | ` * A string key of up to this many bytes lives INSIDE its node, and xKey.sKey is pointed` |
|         - |  732 | ` * at it (SXBLOB_STATIC, so nothing grows it and nothing frees it). Every reader still` |
|         - |  733 | ` * goes through SyBlobData()/SyBlobLength(), so none of the sixty-odd places that read a` |
|         - |  734 | ` * node's key changed.` |
|         - |  735 | ` *` |
|         - |  736 | ` * Why it is free: the node is 96 bytes and the pool serves it out of a 128-byte chunk` |
|         - |  737 | ` * (96 + the pool's own 8-byte header rounds up), so 24 bytes were already being paid for` |
|         - |  738 | ` * and thrown away. 96 + 24 = 120, +8 = 128 -- the same chunk, to the byte.` |
|         - |  739 | ` *` |
|         - |  740 | ` * Why it is worth having: a string-keyed lookup compares the key bytes through` |
|         - |  741 | ` * sKey.pBlob, which used to be a SEPARATE allocation somewhere else in the heap. Counted` |
|         - |  742 | ` * on the ecosystem gate's phpcs step, that is 114,418,815 dereferences to a foreign cache` |
|         - |  743 | ` * line in one run, for keys averaging 7.7 bytes -- while the three tests in front of them` |
|         - |  744 | ` * (iType, nHash, length) all live in the node's first cache line. It also deletes one` |
|         - |  745 | ` * allocation per string-keyed node.` |
|         - |  746 | ` *` |
|         - |  747 | ` * A key LONGER than this keeps the old arrangement, so the size is a tuning constant and` |
|         - |  748 | ` * not a limit. Nothing appends to a node's key after HashmapNewBlobNode builds it -- if` |
|         - |  749 | ` * that ever changes, the LOCKED blob would silently truncate rather than grow.` |
|         - |  750 | ` */` |
|         - |  751 | `#define HASHMAP_NODE_INLINE_KEY 24` |
|         - |  752 | `struct ph7_hashmap_node` |
|         - |  753 | `{` |
|         - |  754 | `	ph7_hashmap *pMap;     /* Hashmap that own this instance */` |
|         - |  755 | `	sxi32 iType;           /* Node type */` |
|         - |  756 | `	union{` |
|         - |  757 | `		sxi64 iKey;        /* Int key */` |
|         - |  758 | `		SyBlob sKey;       /* Blob key */` |
|         - |  759 | `	}xKey;` |
|         - |  760 | `	sxi32 iFlags;          /* Control flags */` |
|         - |  761 | `	sxu32 nHash;           /* Key hash value */` |
|         - |  762 | `	sxu32 nValIdx;         /* Value stored in this node */` |
|         - |  763 | `	ph7_hashmap_node *pNext,*pPrev;               /* Link to other entries [i.e: linear traversal] */` |
|         - |  764 | `	ph7_hashmap_node *pNextCollide,*pPrevCollide; /* Collision chain */` |
|         - |  765 | `	char zKey[HASHMAP_NODE_INLINE_KEY];           /* A short blob key, in the node itself */` |
|         - |  766 | `};` |
|         - |  767 | `/*` |
|         - |  768 | ` * Each active hashmap aka array in the PHP jargon is represented` |
|         - |  769 | ` * by an instance of the following structure.` |
|         - |  770 | ` */` |
|         - |  771 | `struct ph7_hashmap` |
|         - |  772 | `{` |
|         - |  773 | `	ph7_vm *pVm;                  /* VM that own this instance */` |
|         - |  774 | `	ph7_hashmap_node **apBucket;  /* Hash bucket */` |
|         - |  775 | `	ph7_hashmap_node *pFirst;     /* First inserted entry */` |
|         - |  776 | `	ph7_hashmap_node *pLast;      /* Last inserted entry */` |
|         - |  777 | `	ph7_hashmap_node *pCur;       /* Current entry */` |
|         - |  778 | `	sxu32 nSize;                  /* Bucket size */` |
|         - |  779 | `	sxu32 nEntry;                 /* Total number of inserted entries */` |
|         - |  780 | `	sxu32 (*xIntHash)(sxi64);     /* Hash function for int_keys */` |
|         - |  781 | `	sxu32 (*xBlobHash)(const void *,sxu32); /* Hash function for blob_keys */` |
|         - |  782 | `	sxi64 iNextIdx;               /* Next available automatically assigned index */` |
|         - |  783 | `	sxu8 bIntKeySeen;             /* An integer key has been inserted at least once. php 8.3` |
|         - |  784 | `	                               * carries the auto-index through NEGATIVE keys: the first` |
|         - |  785 | `	                               * int key sets the next index to key+1 even when negative` |
|         - |  786 | `	                               * ($a[-4]=x; $a[]=y stores y at -3), where it used to` |
|         - |  787 | `	                               * restart at 0. Only the FIRST key may move the index` |
|         - |  788 | `	                               * downwards, hence the flag. */` |
|         - |  789 | `	sxi64 iMaxIntKey;             /* Upper bound on the integer keys this map has held. Read` |
|         - |  790 | `	                               * only to decide whether the auto-index advance has to` |
|         - |  791 | `	                               * SCAN for a free slot: it can find one occupied only when` |
|         - |  792 | `	                               * a key ABOVE the one just inserted exists, which is` |
|         - |  793 | ``	                               * exactly `iNextIdx <= iMaxIntKey`. Without the test every`` |
|         - |  794 | `` 	                               * int-keyed store paid a failing hash lookup -- `$a[$i]=$i` `` |
|         - |  795 | ``	                               * over 200k keys ran 6x slower than `$a[]=$i`, and the`` |
|         - |  796 | `	                               * key-preserving array builtins inherited it. A stale-HIGH` |
|         - |  797 | `	                               * bound (a key that was since removed, a renumbering) only` |
|         - |  798 | `	                               * costs an extra scan, so it is never lowered. */` |
|         - |  799 | `	sxi32 iRef;                   /* Reference count. INVARIANT: the number of` |
|         - |  800 | `								   * SHARERS for copy-on-write purposes is` |
|         - |  801 | `								   * iRef minus the by-REFERENCE foreach steps` |
|         - |  802 | `								   * on pActiveSteps (a by-ref loop iterates` |
|         - |  803 | `								   * the LIVE map, php semantics) — any future` |
|         - |  804 | `								   * separate/dup gate must use the discounted` |
|         - |  805 | `								   * count like PH7_HashmapCowSeparate, never` |
|         - |  806 | `								   * raw iRef. */` |
|         - |  807 | `	sxi32 iFlags;                 /* Control flags (see HASHMAP_* below) */` |
|         - |  808 | `	sxu32 nGcRoot;                /* 1-based row in the collector's root buffer, 0 while unbuffered */` |
|         - |  809 | `	sxu8 iGcColor;                /* PH7_GC_* -- see vm_gc.c */` |
|         - |  810 | `	ph7_foreach_step *pActiveSteps; /* foreach steps currently iterating this map` |
|         - |  811 | `									 * (per-step cursors — PH7_HashmapUnlinkNode` |
|         - |  812 | `									 * advances any cursor parked on a dying node,` |
|         - |  813 | `									 * node link re-arms parked cursors) */` |
|         - |  814 | `};` |
|         - |  815 | `/*` |
|         - |  816 | ` * Hashmap control flags.` |
|         - |  817 | ` */` |
|         - |  818 | `#define HASHMAP_COUNTING 0x01 /* Set during recursive count to detect cycles */` |
|         - |  819 | `#define HASHMAP_DUMPING  0x02 /* php's GC_PROTECT_RECURSION for an array: set while a` |
|         - |  820 | `                               * dump is INSIDE this map, so a map that is its own` |
|         - |  821 | `                               * descendant renders php's *RECURSION* marker instead of` |
|         - |  822 | `                               * recursing. One bit shared by var_dump, print_r and` |
|         - |  823 | `                               * var_export, exactly as php shares its own. Read only` |
|         - |  824 | `                               * through PH7_MemObjDumpIsRecursive(). */` |
|         - |  825 | `#define HASHMAP_COMPARING 0x04 /* php's GC_PROTECT_RECURSION for an array, worn by the` |
|         - |  826 | `                               * COMPARISON walk instead of the dump walk. Set on the LEFT` |
|         - |  827 | `                               * map while PH7_HashmapCmp is inside it, so a map that is` |
|         - |  828 | `                               * its own descendant is refused with php's` |
|         - |  829 | `                               * "Nesting level too deep - recursive dependency?" instead` |
|         - |  830 | `                               * of recursing forever. Only the left one is marked --` |
|         - |  831 | `                               * zend_hash_compare protects ht1 alone, because a map` |
|         - |  832 | `                               * reachable from BOTH sides is not a cycle and marking both` |
|         - |  833 | `                               * would report one. A separate bit from HASHMAP_DUMPING: a` |
|         - |  834 | `                               * comparison can run a __toString that dumps, and a dump can` |
|         - |  835 | `                               * run a __toString that compares. */` |
|         - |  836 | `/* An instance of the following structure is the context` |
|         - |  837 | ` * for the FOREACH_STEP/FOREACH_INIT VM instructions.` |
|         - |  838 | ` * Those instructions are used to implement the 'foreach'` |
|         - |  839 | ` * statement.` |
|         - |  840 | ` * This structure is made available to these instructions` |
|         - |  841 | ` * as the P3 operand.` |
|         - |  842 | ` */` |
|         - |  843 | `struct ph7_foreach_info` |
|         - |  844 | `{` |
|         - |  845 | `	SyString sKey;      /* Key name. Empty otherwise*/` |
|         - |  846 | `	SyString sValue;    /* Value name */` |
|         - |  847 | `	sxi32 iFlags;       /* Control flags */` |
|         - |  848 | `	SySet aStep;        /* Stack of steps [i.e: ph7_foreach_step instance] */` |
|         - |  849 | `};` |
|         - |  850 | `/*` |
|         - |  851 | ` * One live walk of ONE object's property table.` |
|         - |  852 | ` *` |
|         - |  853 | ` * An object's attributes live in a SyHash, and SyHashGetNextEntry() shares a` |
|         - |  854 | `` * single cursor embedded in the table — which is wrong twice for `foreach`:`` |
|         - |  855 | ` * nested loops over one object rewind each other (an infinite loop, since the` |
|         - |  856 | ` * inner walk always leaves the cursor at the head), and the cursor is advanced` |
|         - |  857 | ` * before the body runs, so a body that unset()s the property the walk is about` |
|         - |  858 | ` * to reach freed the entry the cursor held. This is the object twin of the` |
|         - |  859 | `` * hashmap's per-loop `ph7_foreach_step::pCursor` + `pActiveSteps` pair: every`` |
|         - |  860 | ` * walker keeps its own position, and the instance keeps the list of walkers so` |
|         - |  861 | ` * an attribute added or removed under them can fix their cursors up.` |
|         - |  862 | ` */` |
|         - |  863 | `typedef struct PH7_AttrIter PH7_AttrIter;` |
|         - |  864 | `struct PH7_AttrIter` |
|         - |  865 | `{` |
|         - |  866 | `	SyHashEntry *pCursor;   /* Next attribute entry to yield; 0 once exhausted */` |
|         - |  867 | `	PH7_AttrIter *pNextIter;/* Next live walker on the instance */` |
|         - |  868 | `};` |
|         - |  869 | `struct ph7_foreach_step` |
|         - |  870 | `{` |
|         - |  871 | `	sxi32 iFlags;                   /* Control flags (see below) */` |
|         - |  872 | `	/* Iterate on those values */` |
|         - |  873 | `	union {` |
|         - |  874 | `		ph7_hashmap *pMap;          /* Hashmap [i.e: array in the PHP jargon] iteration` |
|         - |  875 | `									 * Ex: foreach(array(1,2,3) as $key=>$value){}` |
|         - |  876 | `									 */` |
|         - |  877 | `		ph7_class_instance *pThis;  /* Class instance [i.e: object] iteration */` |
|         - |  878 | `	}xIter;` |
|         - |  879 | `	ph7_class_instance *pOwner;     /* IteratorAggregate: keeps aggregate alive during foreach */` |
|         - |  880 | `	ph7_hashmap_node *pCursor;      /* Hashmap iteration: this loop's PRIVATE cursor.` |
|         - |  881 | `									 * php iterates each foreach independently — the map's` |
|         - |  882 | `									 * shared pCur would make nested loops over one array` |
|         - |  883 | `									 * rewind each other (infinite loop). */` |
|         - |  884 | `	struct VmFrame *pFrame;         /* Owning activation's frame (normalized past exception` |
|         - |  885 | `									 * frames). aStep is per-STATEMENT and shared by every` |
|         - |  886 | `									 * activation; OP_FOREACH_STEP selects the step whose` |
|         - |  887 | `									 * pFrame matches the running activation so two suspended` |
|         - |  888 | `									 * instances of one generator/fiber (or a recursive call)` |
|         - |  889 | `									 * paused in the same textual foreach cannot clash on` |
|         - |  890 | `									 * each other's cursor. */` |
|         - |  891 | `	ph7_foreach_step *pNextActive;  /* Next step on the map's pActiveSteps list */` |
|         - |  892 | `	ph7_foreach_info *pInfo;        /* The statement this step belongs to. Carried so the OWNING` |
|         - |  893 | `	                                 * FRAME can tear the step down without knowing which foreach` |
|         - |  894 | `	                                 * it came from (see pNextFrameStep). */` |
|         - |  895 | `	ph7_foreach_step *pNextFrameStep;/* Next step owned by the same activation (VmFrame::pForeachSteps).` |
|         - |  896 | `	                                 * A loop left through break/return/goto/an exception never` |
|         - |  897 | `	                                 * reaches the "no more entries" arm, so its step used to sit on` |
|         - |  898 | `	                                 * the per-STATEMENT aStep until the VM died -- reclaimed only if` |
|         - |  899 | `	                                 * a LATER activation happened to be handed the same frame` |
|         - |  900 | `	                                 * address. aStep therefore grew without bound, and INIT's` |
|         - |  901 | `	                                 * linear reclaim scan over it made every foreach in the program` |
|         - |  902 | `	                                 * quadratic. The frame that owns a step is the one that can` |
|         - |  903 | `	                                 * always end it: this list is how it finds them. */` |
|         - |  904 | `	PH7_AttrIter sAttrIter;         /* Object iteration: this loop's PRIVATE cursor over the` |
|         - |  905 | `	                                 * instance's property table (see PH7_AttrIter) */` |
|         - |  906 | `};` |
|         - |  907 | `/* Foreach step control flags */` |
|         - |  908 | `#define PH7_4EACH_STEP_HASHMAP 0x001 /* Hashmap iteration */` |
|         - |  909 | `#define PH7_4EACH_STEP_OBJECT  0x002 /* Object  iteration */` |
|         - |  910 | `#define PH7_4EACH_STEP_KEY     0x004 /* Make Key available */` |
|         - |  911 | `#define PH7_4EACH_STEP_REF     0x008 /* Pass value by reference not copy */` |
|         - |  912 | `#define PH7_4EACH_STEP_LIST    0x010 /* Value target is list() — destructure */` |
|         - |  913 | `#define PH7_4EACH_STEP_ITERATOR 0x020 /* Object implements Iterator */` |
|         - |  914 | `#define PH7_4EACH_STEP_FIRST    0x040 /* First iteration (skip next() call) */` |
|         - |  915 | `/*` |
|         - |  916 | ` * Each PH7 engine is identified by an instance of the following structure.` |
|         - |  917 | ` * Please refer to the official documentation for more information` |
|         - |  918 | ` * on how to configure your PH7 engine instance.` |
|         - |  919 | ` */` |
|         - |  920 | `struct ph7` |
|         - |  921 | `{` |
|         - |  922 | `	SyMemBackend sAllocator;     /* Low level memory allocation subsystem */` |
|         - |  923 | `	const ph7_vfs *pVfs;         /* Underlying Virtual File System */` |
|         - |  924 | `	ph7_conf xConf;              /* Configuration */` |
|         - |  925 | `#if defined(PH7_ENABLE_THREADS)` |
|         - |  926 | `	const SyMutexMethods *pMethods;  /* Mutex methods */` |
|         - |  927 | `	SyMutex *pMutex;                 /* Per-engine mutex */` |
|         - |  928 | `#endif` |
|         - |  929 | `	ph7_vm *pVms;      /* List of active VM */` |
|         - |  930 | `	sxi32 iVm;         /* Total number of active VM */` |
|         - |  931 | `	ph7 *pNext,*pPrev; /* List of active engines */` |
|         - |  932 | `	sxu32 nMagic;      /* Sanity check against misuse */` |
|         - |  933 | `};` |
|         - |  934 | `/* Code generation data structures */` |
|         - |  935 | `typedef sxi32 (*ProcErrorGen)(void *,sxi32,sxu32,const char *,...);` |
|         - |  936 | `typedef struct ph7_expr_node   ph7_expr_node;` |
|         - |  937 | `typedef struct ph7_expr_op     ph7_expr_op;` |
|         - |  938 | `typedef struct ph7_gen_state   ph7_gen_state;` |
|         - |  939 | `/*` |
|         - |  940 | ` * Lexer trivia sidecar record: a doc-comment (or, later, an attribute` |
|         - |  941 | ` * group) captured OUT of the token stream, keyed by the index the NEXT` |
|         - |  942 | ` * real token receives in the chunk's token set. sText points into the` |
|         - |  943 | ` * raw script buffer — consumers must duplicate before the buffer dies.` |
|         - |  944 | ` */` |
|         - |  945 | `typedef struct ph7_trivia ph7_trivia;` |
|         - |  946 | `struct ph7_trivia` |
|         - |  947 | `{` |
|         - |  948 | `	sxu32 nTokIdx;   /* Index of the next real token in the chunk token set */` |
|         - |  949 | `	sxu8  iKind;     /* PH7_TRIVIA_* */` |
|         - |  950 | `	SyString sText;  /* Raw span (docblock includes its delimiters) */` |
|         - |  951 | `	sxu32 nLine;     /* Line the trivia starts on */` |
|         - |  952 | `};` |
|         - |  953 | `#define PH7_TRIVIA_DOC  1 /* A doc-comment: slash-star-star ... star-slash */` |
|         - |  954 | `#define PH7_TRIVIA_ATTR 2 /* An attribute group: the span between #[ and its ] */` |
|         - |  955 | `/*` |
|         - |  956 | ` * One compiled attribute argument: an optional name (named argument) and` |
|         - |  957 | ` * the constant expression's bytecode, evaluated lazily at` |
|         - |  958 | ` * ReflectionAttribute::getArguments()/newInstance() time (PHP's` |
|         - |  959 | ` * lazy-instantiation semantics).` |
|         - |  960 | ` */` |
|         - |  961 | `typedef struct ph7_attr_arg ph7_attr_arg;` |
|         - |  962 | `struct ph7_attr_arg` |
|         - |  963 | `{` |
|         - |  964 | `	SyString sName;   /* Named-argument name (duplicated); nByte == 0 = positional */` |
|         - |  965 | `	SySet aByteCode;  /* Compiled expression, OP_DONE(p1=1) terminated (VmInstr) */` |
|         - |  966 | `	const void *pNativeValue; /* A NATIVE attribute's literal (PH7_NativeConstDef *), or 0.` |
|         - |  967 | ``	                   * php declares `#[Attribute(Attribute::TARGET_CLASS)]` on two of its`` |
|         - |  968 | `	                   * own classes, and a class declared from C has no compiler to emit` |
|         - |  969 | `	                   * byte-code for the argument — so the value rides as a literal and` |
|         - |  970 | `	                   * every reader takes this branch when aByteCode is empty. */` |
|         - |  971 | `};` |
|         - |  972 | `/*` |
|         - |  973 | ` * One #[...] attribute as declared: the compile-time-resolved FQN and its` |
|         - |  974 | ` * argument list.` |
|         - |  975 | ` */` |
|         - |  976 | `typedef struct ph7_attribute ph7_attribute;` |
|         - |  977 | `struct ph7_attribute` |
|         - |  978 | `{` |
|         - |  979 | `	SyString sName;   /* Fully-qualified class name (resolved via use imports /` |
|         - |  980 | `	                   * current namespace at compile time; duplicated) */` |
|         - |  981 | `	SySet aArgs;      /* ph7_attr_arg records */` |
|         - |  982 | `	sxu32 nLine;      /* Line the attribute appears on */` |
|         - |  983 | `};` |
|         - |  984 | `typedef struct GenBlock        GenBlock;` |
|         - |  985 | `typedef sxi32 (*ProcLangConstruct)(ph7_gen_state *);` |
|         - |  986 | `typedef sxi32 (*ProcNodeConstruct)(ph7_gen_state *,sxi32);` |
|         - |  987 | `/*` |
|         - |  988 | ` * Each supported operator [i.e: +, -, ==, *, %, >>, >=, new, etc.] is represented` |
|         - |  989 | ` * by an instance of the following structure.` |
|         - |  990 | ` * The PH7 parser does not use any external tools and is 100% handcoded.` |
|         - |  991 | ` * That is, the PH7 parser is thread-safe ,full reentrant, produce consistant` |
|         - |  992 | ` * compile-time errrors and at least 7 times faster than the standard PHP parser.` |
|         - |  993 | ` */` |
|         - |  994 | `struct ph7_expr_op` |
|         - |  995 | `{` |
|         - |  996 | `	SyString sOp;   /* String representation of the operator [i.e: "+","*","=="...] */` |
|         - |  997 | `	sxi32 iOp;      /* Operator ID */` |
|         - |  998 | `	sxi32 iPrec;    /* Operator precedence: 1 == Highest */` |
|         - |  999 | `	sxi32 iAssoc;   /* Operator associativity (either left,right or non-associative) */` |
|         - | 1000 | `	sxi32 iVmOp;    /* VM OP code for this operator [i.e: PH7_OP_EQ,PH7_OP_LT,PH7_OP_MUL...]*/` |
|         - | 1001 | `};` |
|         - | 1002 | `/*` |
|         - | 1003 | ` * Each expression node is parsed out and recorded` |
|         - | 1004 | ` * in an instance of the following structure.` |
|         - | 1005 | ` */` |
|         - | 1006 | `struct ph7_expr_node` |
|         - | 1007 | `{` |
|         - | 1008 | `	const ph7_expr_op *pOp;  /* Operator ID or NULL if literal, constant, variable, function or class method call */` |
|         - | 1009 | `	ph7_expr_node *pLeft;    /* Left expression tree */` |
|         - | 1010 | `	ph7_expr_node *pRight;   /* Right expression tree */` |
|         - | 1011 | `	SyToken *pStart;         /* Stream of tokens that belong to this node */` |
|         - | 1012 | `	SyToken *pEnd;           /* End of token stream */` |
|         - | 1013 | `	sxi32 iFlags;            /* Node construct flags */` |
|         - | 1014 | `	ProcNodeConstruct xCode; /* C routine responsible of compiling this node */` |
|         - | 1015 | `	SySet aNodeArgs;         /* Node arguments. Only used by postfix operators [i.e: function call]*/` |
|         - | 1016 | `	SyString sArgName;       /* Named argument label (empty if positional) */` |
|         - | 1017 | `	ph7_expr_node *pCond;    /* Condition: Only used by the ternary operator '?:' */` |
|         - | 1018 | `};` |
|         - | 1019 | `/* Node Construct flags */` |
|         - | 1020 | `#define EXPR_NODE_PRE_INCR    0x01 /* Pre-icrement/decrement [i.e: ++$i,--$j] node */` |
|         - | 1021 | `#define EXPR_NODE_SPREAD      0x02 /* Argument unpacking: ...$expr */` |
|         - | 1022 | `#define EXPR_NODE_NAMED_ARG   0x04 /* Named argument: name: $expr */` |
|         - | 1023 | `#define EXPR_NODE_PARENS      0x08 /* Root of a parenthesized sub-expression */` |
|         - | 1024 | ``#define EXPR_NODE_FCC         0x10 /* First-class callable marker: a lone `...` as the`` |
|         - | 1025 | `                                    * whole argument list, e.g. f(...) — wrap the callee` |
|         - | 1026 | `                                    * in a Closure instead of calling it. */` |
|         - | 1027 | `/*` |
|         - | 1028 | ` * A block of instructions is recorded in an instance of the following structure.` |
|         - | 1029 | ` * This structure is used only during compile-time and have no meaning` |
|         - | 1030 | ` * during bytecode execution.` |
|         - | 1031 | ` */` |
|         - | 1032 | `struct GenBlock` |
|         - | 1033 | `{` |
|         - | 1034 | `	ph7_gen_state *pGen;  /* State of the code generator */` |
|         - | 1035 | `	GenBlock *pParent;    /* Upper block or NULL if global */` |
|         - | 1036 | `	sxu32 nFirstInstr;    /* First instruction to execute  */` |
|         - | 1037 | `	sxi32 iFlags;         /* Block control flags (see below) */` |
|         - | 1038 | `	SySet aJumpFix;       /* Jump fixup (JumpFixup instance) */` |
|         - | 1039 | `	void *pUserData;      /* Upper layer private data */` |
|         - | 1040 | `	sxu32 nLoopId;        /* This block's loop/switch id (0 when it is neither) */` |
|         - | 1041 | `	sxu32 nOuterLoopId;   /* Loop/switch that was innermost when this one was entered */` |
|         - | 1042 | `	sxu32 nScopeId;       /* Try/catch scope in effect INSIDE this block (0 = none). An` |
|         - | 1043 | `	                       * exception block mints its own; every other block inherits. */` |
|         - | 1044 | `	sxu32 nOuterScopeId;  /* Scope that was innermost when this block was entered */` |
|         - | 1045 | `	/* The following two fields are used only when compiling` |
|         - | 1046 | `	 * the 'do..while()' language construct.` |
|         - | 1047 | `	 */` |
|         - | 1048 | `	sxu8 bPostContinue;    /* TRUE when compiling the do..while() statement */` |
|         - | 1049 | `	SySet aPostContFix;    /* Post-continue jump fix */` |
|         - | 1050 | `};` |
|         - | 1051 | `/*` |
|         - | 1052 | ` * Code generator state is remembered in an instance of the following` |
|         - | 1053 | ` * structure. We put the information in this structure and pass around` |
|         - | 1054 | ` * a pointer to this structure, rather than pass around  all of the` |
|         - | 1055 | ` * information separately. This helps reduce the number of  arguments` |
|         - | 1056 | ` * to generator functions.` |
|         - | 1057 | ` * This structure is used only during compile-time and have no meaning` |
|         - | 1058 | ` * during bytecode execution.` |
|         - | 1059 | ` */` |
|         - | 1060 | `struct ph7_gen_state` |
|         - | 1061 | `{` |
|         - | 1062 | `	ph7_vm *pVm;         /* VM that own this instance */` |
|         - | 1063 | `	SyHash hLiteral;     /* Constant string Literals table */` |
|         - | 1064 | `	SyHash hNumLiteral;  /* Numeric literals table */` |
|         - | 1065 | `	SyHash hVar;         /* Collected variable hashtable */` |
|         - | 1066 | `	GenBlock *pCurrent;  /* Current processed block */` |
|         - | 1067 | `	ph7_class *pCurClass; /* Class/interface/trait/enum whose BODY is currently being compiled` |
|         - | 1068 | `	                       * (0 at top level). Saved/restored around each class-body compiler so` |
|         - | 1069 | `	                       * a nested anonymous class overrides it. Lets a const-expression that` |
|         - | 1070 | `	                       * compiles OUTSIDE any function block — a property default or a` |
|         - | 1071 | `	                       * parameter default — resolve __TRAIT__ to the enclosing trait, which` |
|         - | 1072 | `	                       * the block-chain walk alone cannot see (no func block on the chain). */` |
|         - | 1073 | `	ph7_class *pCurBase; /* The BASE CLASS of pCurClass, known while its body compiles --` |
|         - | 1074 | `	                       * pCurClass->pBase is only filled at inheritance, which runs` |
|         - | 1075 | `	                       * AFTER the body. Saved/restored with pCurClass. 0 for an` |
|         - | 1076 | ``	                       * interface (php gives one no `parent` however many it extends)`` |
|         - | 1077 | `	                       * and for a trait (which defers the question to composition). */` |
|         - | 1078 | `	/* Whose SIGNATURE is being parsed, for php's scope-keyword screen -- see iSigScope. */` |
|         - | 1079 | `#define PH7_SIGSCOPE_MEMBER  0` |
|         - | 1080 | `#define PH7_SIGSCOPE_CLOSURE 1` |
|         - | 1081 | `#define PH7_SIGSCOPE_FUNC    2` |
|         - | 1082 | `	int iSigScope;       /* Whose SIGNATURE is being parsed, for php's scope-keyword screen` |
|         - | 1083 | ``	                       * (`self`/`parent`/`static` in a type). Saved and restored around`` |
|         - | 1084 | `	                       * each signature, so a nested one answers for itself:` |
|         - | 1085 | `	                       *   PH7_SIGSCOPE_MEMBER  -- a method, property or class constant:` |
|         - | 1086 | `	                       *                          the enclosing class body's scope applies` |
|         - | 1087 | `	                       *   PH7_SIGSCOPE_CLOSURE -- a closure or arrow function: EXEMPT, its` |
|         - | 1088 | `	                       *                          scope is decided when it is bound` |
|         - | 1089 | `	                       *   PH7_SIGSCOPE_FUNC    -- a named function: NO class scope, even` |
|         - | 1090 | `	                       *                          written inside a method body */` |
|         - | 1091 | `	int iInMemberDefault; /* > 0 while compiling a property/parameter DEFAULT value. Such a` |
|         - | 1092 | `	                       * const-expression belongs to pCurClass, never to a lexically-` |
|         - | 1093 | `	                       * enclosing method, so __TRAIT__ reads pCurClass directly rather than` |
|         - | 1094 | `	                       * walking the block chain (which would leak into the enclosing` |
|         - | 1095 | `	                       * function — e.g. an anonymous class's default inside a trait method). */` |
|         - | 1096 | `	GenBlock sGlobal;    /* Global block */` |
|         - | 1097 | `	ProcConsumer xErr;   /* Error consumer callback */` |
|         - | 1098 | `	void *pErrData;      /* Third argument to xErr() */` |
|         - | 1099 | `	SySet aLabel;        /* Label table */` |
|         - | 1100 | `	SySet aGoto;         /* Gotos table */` |
|         - | 1101 | `	SySet aNullsafeJmp;  /* Pending NULLSAFE_JMP instruction indices (sxu32) */` |
|         - | 1102 | `	/* An assignment whose TARGET carries dynamic subscript/property names evaluates them` |
|         - | 1103 | `	 * before the assigned value (php's order) by pushing them first and emitting the access` |
|         - | 1104 | `	 * chain last; while that chain is being emitted these nodes must not be compiled again,` |
|         - | 1105 | `	 * they are read back off the stack with PH7_OP_PICK. Installed for exactly that emission` |
|         - | 1106 | `	 * and saved/restored around it, so a nested assignment answers for itself. */` |
|         - | 1107 | `#define PH7_STORE_KEY_MAX 8` |
|         - | 1108 | `	ph7_expr_node **apStoreKey; /* the parked name expressions, in push order -- the emitting` |
|         - | 1109 | `	                             * frame's own array, borrowed for the length of that emission */` |
|         - | 1110 | `	int nStoreKey;       /* how many are parked (0 = no assignment target is being emitted) */` |
|         - | 1111 | `	int nCommaExprOk;    /* > 0 while compiling a for() clause, the ONLY place php's grammar` |
|         - | 1112 | `	                      * allows a comma-separated expression list (PH7's comma OPERATOR` |
|         - | 1113 | `	                      * is otherwise a PH7-ism php rejects, and we remove) */` |
|         - | 1114 | `	const char *zClauseCloser; /* When an expression has a trailing token the grammar can't` |
|         - | 1115 | `	                      * absorb, the tree builder names it and, if this is set, says what` |
|         - | 1116 | `` 	                      * the enclosing construct expected: `;` after `return`, `,`/`;` `` |
|         - | 1117 | ``	                      * after `echo`, `)` for a for() post clause, `]` inside an array`` |
|         - | 1118 | `	                      * literal, and so on. Each construct saves/sets/restores it around` |
|         - | 1119 | `	                      * its expression compile. NULL means "no expecting clause" — php` |
|         - | 1120 | `	                      * prints none for a plain expression statement. */` |
|         - | 1121 | `` 	int nExprEchoOk;     /* > 0 only while compiling the synthesized `echo` of a `<?= ... ?>` `` |
|         - | 1122 | `	                      * short tag, which is the one place an echo legitimately compiles` |
|         - | 1123 | ``	                      * as an EXPRESSION. Everywhere else `echo` in expression position`` |
|         - | 1124 | `	                      * is a php parse error (it was a Symisc extension — the scope policy) */` |
|         - | 1125 | `	sxu32 nLoopId;       /* Monotonic id handed to each loop/switch block as it is entered */` |
|         - | 1126 | `	sxu32 nCurLoopId;    /* Innermost loop/switch currently open (0 = none) */` |
|         - | 1127 | `	SySet aLoopParent;   /* aLoopParent[id-1] = enclosing loop id, so the ancestry of any loop` |
|         - | 1128 | `	                      * can be walked after compilation. php's only goto restriction is` |
|         - | 1129 | `	                      * "'goto' into loop or switch statement is disallowed": a jump is` |
|         - | 1130 | `	                      * illegal exactly when the LABEL sits in a loop that does not also` |
|         - | 1131 | `	                      * enclose the GOTO. Both ends record their loop id; the fixup pass` |
|         - | 1132 | `	                      * walks up from the goto's to look for the label's. */` |
|         - | 1133 | `	sxu32 nScopeId;      /* Monotonic id handed to each try/catch/finally block entered */` |
|         - | 1134 | `	sxu32 nCurScopeId;   /* Innermost such block currently open (0 = none) */` |
|         - | 1135 | `	SySet aScope;        /* aScope[id-1] = that block's GenScope: its enclosing scope id and` |
|         - | 1136 | `	                      * its kind. Same shape and purpose as aLoopParent above, for the` |
|         - | 1137 | `	                      * other after-the-fact goto question: a jump out of a try/catch is` |
|         - | 1138 | `	                      * legal exactly when the LABEL's scope also ENCLOSES the goto, and` |
|         - | 1139 | `	                      * what it must unwind on the way is read off the chain between them` |
|         - | 1140 | `	                      * (GenStateJumpScope). Comparing NESTING DEPTHS instead cannot tell` |
|         - | 1141 | `	                      * two sibling trys apart, which let a goto jump into one — skipping` |
|         - | 1142 | `	                      * its OP_LOAD_EXCEPTION, or landing in another bytecode array. */` |
|         - | 1143 | `	SyBlob sWorker;      /* General purpose working buffer */` |
|         - | 1144 | `	SyBlob sErrBuf;      /* Error buffer */` |
|         - | 1145 | `	SyBlob sFirstErr;    /* The BARE text of the FIRST refusal in this unit -- php reports one` |
|         - | 1146 | `	                      * compile-time refusal and stops, and an include's parse error is` |
|         - | 1147 | `	                      * handed to the caller as the message of php's ParseError. */` |
|         - | 1148 | `	sxu32 nFirstErrLine; /* ...and the line it was raised on. */` |
|         - | 1149 | `	sxi32 nFatal;        /* Refusals of E_ERROR severity in this unit. php's E_COMPILE_ERROR:` |
|         - | 1150 | `	                      * uncatchable, where a PARSE error is a catchable ParseError -- so` |
|         - | 1151 | `	                      * nErr says the unit failed and this says WHICH WAY. */` |
|         - | 1152 | `	int iFatalTrace;     /* WHICH stack trace php prints under the refusal being raised -- php's` |
|         - | 1153 | `	                      * three phases, and they answer differently:` |
|         - | 1154 | `	                      *   PH7_FATAL_TRACE_COMPILE (0) the compiler refused: the active frames,` |
|         - | 1155 | `	                      *     WITHOUT the include/require/eval that is loading this unit (php` |
|         - | 1156 | `	                      *     raises it before pushing that activation) -- the common case;` |
|         - | 1157 | `	                      *   PH7_FATAL_TRACE_RUNTIME (1) php makes this one at RUN time, so the` |
|         - | 1158 | `	                      *     activation IS on its trace. A class REDECLARATION is the only one:` |
|         - | 1159 | `	                      *     php cannot early-bind a name it already holds, so DECLARE_CLASS` |
|         - | 1160 | `	                      *     reports it;` |
|         - | 1161 | `	                      *   PH7_FATAL_TRACE_NONE (2) php's PARSER refused, while reading a` |
|         - | 1162 | `	                      *     modifier run, before any op array exists -- it prints no trace at` |
|         - | 1163 | `	                      *     all.` |
|         - | 1164 | `	                      * A ONE-SHOT: the call sites that need a non-default set it just before` |
|         - | 1165 | `	                      * raising, and PH7_GenCompileError consumes it. */` |
|         - | 1166 | ``	sxi8 bRefElemIsThis; /* One-shot, set by the array-literal `&` element validator: the`` |
|         - | 1167 | ``	                      * element IS `$this`. php cannot take a reference to it (it is`` |
|         - | 1168 | `	                      * not a variable slot there), so the entry copies the object and` |
|         - | 1169 | `	                      * a write through the entry leaves the receiver alone -- where` |
|         - | 1170 | `	                      * an OP_LOAD_REF would alias the receiver and re-point it. */` |
|         - | 1171 | `	int bParseThrows;    /* This unit's parse errors are the CALLER's to raise (include/require:` |
|         - | 1172 | `	                      * php throws a ParseError there and prints nothing until it goes` |
|         - | 1173 | `	                      * uncaught). A refusal of E_ERROR severity still prints at once. */` |
|         - | 1174 | `	SyBlob sNamespace;   /* Current namespace path (e.g. "App\\Models") */` |
|         - | 1175 | `	SyHash hUseImports;      /* use imports: short alias -> FQN (classes) */` |
|         - | 1176 | `	SyHash hUseFuncImports;  /* use function imports: short alias -> FQN */` |
|         - | 1177 | `	SyHash hUseConstImports; /* use const imports: short alias -> FQN */` |
|         - | 1178 | `	SyHash hSeenClass;       /* FQNs of the classes DECLARED so far in this compile unit */` |
|         - | 1179 | `	SyHash hSeenFunc;        /* FQNs of the functions DECLARED so far in this compile unit` |
|         - | 1180 | `	                          * (both: php refuses an import a declaration already took —` |
|         - | 1181 | `	                          * these outlive a namespace switch, unlike the import tables) */` |
|         - | 1182 | `	SyToken *pIn;        /* Current processed token */` |
|         - | 1183 | `	SyToken *pEnd;       /* Last token in the stream */` |
|         - | 1184 | `	sxu32 nErr;          /* Total number of compilation error */` |
|         - | 1185 | `	SyToken *pRawIn;     /* Current processed raw token */` |
|         - | 1186 | `	SyToken *pRawEnd;    /* Last raw token in the stream */` |
|         - | 1187 | `	SySet   *pTokenSet;  /* Token containers */` |
|         - | 1188 | `	sxi8 bStrictTypes;       /* Current file's strict_types mode (0 = weak/unset, 1 = strict) */` |
|         - | 1189 | `	sxi8 bStrictTypesLocked; /* 1 once the current file has emitted any non-declare, non-empty` |
|         - | 1190 | `	                          * top-level statement (php's zend_is_first_statement with nops allowed) */` |
|         - | 1191 | `	sxi8 bNsNamed;           /* php's FC(current_namespace): a NAMED namespace is in effect */` |
|         - | 1192 | ``	sxi8 bNsBracketed;       /* php's FC(has_bracketed_namespaces): this file used `namespace X { }` */`` |
|         - | 1193 | `	sxi8 bInNsBlock;         /* php's FC(in_namespace): the cursor is inside such a block */` |
|         - | 1194 | `	sxi8 bChunkAtEof;        /* 1 when the PHP chunk being compiled ran into the end of the` |
|         - | 1195 | ``	                          * FILE rather than being closed by a `?>`. php reads the closing`` |
|         - | 1196 | `	                          * tag as a statement terminator, so only this chunk can leave one` |
|         - | 1197 | `	                          * unfinished -- and that is a parse error there. */` |
|         - | 1198 | `	sxu32 nChunkEofLine;     /* Line the chunk's end-of-input sits on -- its last line, which is` |
|         - | 1199 | `	                          * NOT the last TOKEN's line when trailing blank lines follow. php` |
|         - | 1200 | ``	                          * reports `unexpected end of file` at the former. */`` |
|         - | 1201 | ``	sxi8 bHalted;            /* 1 once `__halt_compiler();` has been compiled in this file:`` |
|         - | 1202 | `	                          * everything after it -- the rest of the chunk, every later chunk` |
|         - | 1203 | `	                          * and every byte of inline text between them -- is DATA, and the` |
|         - | 1204 | `	                          * chunk loop stops. */` |
|         - | 1205 | `	sxi8 bHaltSeen;          /* 1 when the file HAS a halt (found by the pre-scan below, which` |
|         - | 1206 | `	                          * runs before any of it compiles because the offset may be read` |
|         - | 1207 | `	                          * ahead of the statement that sets it). */` |
|         - | 1208 | ``	sxu32 nHaltOffset;       /* What `__COMPILER_HALT_OFFSET__` expands to: the byte offset in`` |
|         - | 1209 | ``	                          * the FILE just past the halt statement's `;` -- a shebang line`` |
|         - | 1210 | `	                          * this compiler skipped included, since php counts from the first` |
|         - | 1211 | `	                          * byte on disk. Meaningful only while bHaltSeen. */` |
|         - | 1212 | `	const char *zScriptBase; /* First byte of the whole script, for the offset above. */` |
|         - | 1213 | `	sxi8 bListSrcNotRef;     /* 1 while compiling the TARGET list of an assignment whose SOURCE` |
|         - | 1214 | ``	                          * cannot hold a reference (`[&$r] = [7];`). php checks this at`` |
|         - | 1215 | `	                          * compile time, where it still knows what the right-hand side was` |
|         - | 1216 | `	                          * written as; by the time a by-ref entry is emitted the source is` |
|         - | 1217 | `	                          * an anonymous value on the stack, so the answer is carried here.` |
|         - | 1218 | ``	                          * A foreach `as` list has no such source and leaves it clear. */`` |
|         - | 1219 | `	sxi8 bInGenerator;       /* ROOT C: 1 while compiling a generator function body (a yield appears at` |
|         - | 1220 | `` 	                          * this function's own level). Gates inline try/catch/finally so `yield` `` |
|         - | 1221 | `	                          * inside a catch/finally suspends correctly; non-generators keep the` |
|         - | 1222 | `	                          * legacy detached-mini-program path. Saved/restored across nested funcs. */` |
|         - | 1223 | `	SySet aTrivia;       /* Trivia sidecar for the current chunk (ph7_trivia records from the` |
|         - | 1224 | `	                      * main-chunk tokenize calls; reset with the token set) */` |
|         - | 1225 | `	SyString sPendingDoc;/* Docblock immediately preceding the statement being dispatched;` |
|         - | 1226 | `	                      * consumed by the declaration compilers, discarded at the next` |
|         - | 1227 | `	                      * statement boundary (points into the raw script buffer) */` |
|         - | 1228 | `	SySet aPendingAttrs; /* Attribute-group trivia (ph7_trivia) bound to the statement being` |
|         - | 1229 | `	                      * dispatched; unlike docs, PHP requires attributes to be adjacent,` |
|         - | 1230 | `	                      * so this resets at every boundary */` |
|         - | 1231 | ``	SyString sPendingClosureName; /* php's `{closure:SCOPE:LINE}` name built for the closure`` |
|         - | 1232 | `	                      * whose body GenStateCompileFunc is about to compile. The caller` |
|         - | 1233 | `	                      * knows the 'function' keyword's LINE and the ENCLOSING scope; the` |
|         - | 1234 | `	                      * name must be on the ph7_vm_func before the body compiles, because` |
|         - | 1235 | `	                      * __FUNCTION__ inside it resolves at compile time. Consumed (and` |
|         - | 1236 | `	                      * cleared) the moment the function state is initialized. */` |
|         - | 1237 | `	SyString sPendingClosureScope; /* ...and the CLASS that closure belongs to, when it was written` |
|         - | 1238 | `	                               * inside a method (or inside a closure that was). php prefixes an` |
|         - | 1239 | ``	                               * argument diagnostic with it -- `C::{closure:C::m():5}` -- and`` |
|         - | 1240 | `	                               * the name above cannot be taken apart for it: a top-level` |
|         - | 1241 | ``	                               * closure's is a FILE PATH, which may hold a `::` of its own. */`` |
|         - | 1242 | `};` |
|         - | 1243 | `/* Forward references */` |
|         - | 1244 | `typedef struct ph7_vm_func_closure_env ph7_vm_func_closure_env;` |
|         - | 1245 | `typedef struct ph7_vm_func_static_var  ph7_vm_func_static_var;` |
|         - | 1246 | `typedef struct ph7_vm_func_arg ph7_vm_func_arg;` |
|         - | 1247 | `typedef struct ph7_vm_func ph7_vm_func;` |
|         - | 1248 | `/*` |
|         - | 1249 | ` * One ACTIVE include/require/eval, as php reports it in a backtrace: a frame whose` |
|         - | 1250 | ` * function is the construct's name, whose file and line are the CALL SITE, and whose` |
|         - | 1251 | ` * single argument is the unit being loaded.` |
|         - | 1252 | ` */` |
|         - | 1253 | `#define PH7_FATAL_TRACE_COMPILE 0 /* see ph7_gen_state::iFatalTrace */` |
|         - | 1254 | `#define PH7_FATAL_TRACE_RUNTIME 1` |
|         - | 1255 | `#define PH7_FATAL_TRACE_NONE    2` |
|         - | 1256 | `typedef struct VmIncFrame VmIncFrame;` |
|         - | 1257 | `struct VmIncFrame` |
|         - | 1258 | `{` |
|         - | 1259 | `	void *pFrame;      /* the VmFrame this activation was started from -- where it belongs in` |
|         - | 1260 | `	                    * the walk (php's trace is ordered by activation, and an include is` |
|         - | 1261 | `	                    * INNER to the function that wrote it) */` |
|         - | 1262 | `	SyString sFile;    /* the file the construct is written in ... */` |
|         - | 1263 | `	sxu32 nLine;       /* ...and the line */` |
|         - | 1264 | `	SyString sPath;    /* the unit being loaded: php's single argument for the frame, rendered` |
|         - | 1265 | ``	                    * as `'...'`. Empty for eval(), which php shows argument-less. */`` |
|         - | 1266 | `	const char *zName; /* "include" / "include_once" / "require" / "require_once" / "eval" */` |
|         - | 1267 | `};` |
|         - | 1268 | `typedef struct VmFrame VmFrame;` |
|         - | 1269 | `typedef struct VmInstr VmInstr;   /* defined below; a frame names the body it is numbered for */` |
|         - | 1270 | `/* How many of a body's variables get a NUMBER (see VmFrame's aLocalSlot and` |
|         - | 1271 | ` * VmNumberLocals). The frame's 512-byte pool bucket has 136 bytes spare once the` |
|         - | 1272 | ` * struct's named fields are laid out; a code pointer takes 8 of them and 28 slots` |
|         - | 1273 | ` * take the other 112, with room left over -- raising it past 32 reallocates every` |
|         - | 1274 | ` * frame out of the 512-byte bucket into the 1024-byte one, which is a doubling of` |
|         - | 1275 | ` * the engine's per-activation memory for the tail of the distribution. A static scan` |
|         - | 1276 | ` * of the ecosystem gate's phpcs sources puts 98% of function bodies at 28 distinct` |
|         - | 1277 | ` * variable names or fewer, and a body with more than that still numbers its 28` |
|         - | 1278 | ` * most-REFERENCED ones -- so the cap costs the tail its cold names, not its hot ones. */` |
|         - | 1279 | `#define PH7_VAR_SLOT_MAX 28` |
|         - | 1280 | `/* Chains in ph7_vm::apIdleOperandStack. A parked operand stack is reusable only by a` |
|         - | 1281 | `` * call of EXACTLY its slot count, so the count picks the chain: `nCap & (N-1)`, with`` |
|         - | 1282 | ` * the exact size still checked on each node (sizes sharing the low bits share a` |
|         - | 1283 | ` * chain). Sixty-four heads over a pool capped at 256 buffers is ~4 compares. */` |
|         - | 1284 | `#define PH7_STACK_POOL_BUCKETS 64` |
|         - | 1285 | `struct VmFrame` |
|         - | 1286 | `{` |
|         - | 1287 | `	VmFrame *pParent; /* Parent frame or NULL if global scope */` |
|         - | 1288 | `	void *pUserData;  /* Upper layer private data associated with this frame */` |
|         - | 1289 | `	ph7_class_instance *pThis; /* Current class instance [i.e: the '$this' variable].NULL otherwise */` |
|         - | 1290 | `	ph7_class *pBoundScope; /* Closure::bindTo/call scope override for private/protected access (Increment 2) */` |
|         - | 1291 | `	ph7_class *pSelfClass;  /* The class this activation was reached THROUGH (php's called-scope):` |
|         - | 1292 | `	                         * the receiver's class for an instance call, the named class for a` |
|         - | 1293 | `	                         * static one, 0 for a plain function. Only a trait method needs it --` |
|         - | 1294 | `	                         * its declaring class is the TRAIT, and the class php composed it` |
|         - | 1295 | `	                         * into is found by walking this one's ancestry (a STATIC trait method` |
|         - | 1296 | `	                         * has no $this to walk from). */` |
|         - | 1297 | `	SySet sLocal;     /* Local variables container (VmSlot instance) */` |
|         - | 1298 | `	ph7_vm *pVm;      /* VM that own this frame */` |
|         - | 1299 | `	SyHash hVar;      /* Variable hashtable for fast lookup */` |
|         - | 1300 | `	SySet sArg;       /* Function arguments container */` |
|         - | 1301 | `	SySet sRef;       /* Local reference table (VmSlot instance) */` |
|         - | 1302 | `	sxi32 iFlags;     /* Frame configuration flags (See below)*/` |
|         - | 1303 | `	sxu32 iExceptionJump; /* Exception jump destination */` |
|         - | 1304 | ``	ph7_value sRet;   /* Deferred catch/finally `return` value targeting THIS body frame */`` |
|         - | 1305 | `	int bHasRet;      /* TRUE when sRet holds a live pending return */` |
|         - | 1306 | `	sxu32 nRetGen;    /* Bumped on every sRet write (see VmThrowException finally path) */` |
|         - | 1307 | `	sxu32 nCatchJmpPc;/* Pending loop jump parked by a break/continue that left a DETACHED catch` |
|         - | 1308 | `	                   * mini-program (OP_CATCH_JMP): its target pc in this body frame's` |
|         - | 1309 | `	                   * bytecode, 0 when none is armed. The sibling of bHasRet/sRet — the same` |
|         - | 1310 | `	                   * park-here, act-at-the-landing-pad contract, cleared by the same` |
|         - | 1311 | `	                   * VmClearFramePending. Consumed by the owning try's OP_POP_EXCEPTION. */` |
|         - | 1312 | `	sxu16 nCatchJmpLevels;/* Detached-container boundaries still to leave before it is taken */` |
|         - | 1313 | `	sxu16 nCatchJmpCross; /* Enclosing try activations to drain (run their finally) before it */` |
|         - | 1314 | `	sxu32 nCallLine;  /* Line of the OP_CALL that pushed this frame (0 for the global frame).` |
|         - | 1315 | `	                   * debug_backtrace() reports a frame's line as the line of the call` |
|         - | 1316 | `	                   * SITE, not of the code running inside it. */` |
|         - | 1317 | `	SyString sCallFile;/* ...and the FILE that call site is in, captured when the frame is` |
|         - | 1318 | `	                   * pushed. It cannot be derived afterwards: the caller's own file is the` |
|         - | 1319 | `	                   * defining file of the CALLER's function, and for a call made by` |
|         - | 1320 | `	                   * top-level code it is whichever included unit was executing THEN --` |
|         - | 1321 | `	                   * the include stack has moved on by the time a trace is taken. Aliases` |
|         - | 1322 | `	                   * a VM-lifetime string (a function's sFile, or an aFiles entry). */` |
|         - | 1323 | `	SyString *pNativeCaller;/* When VM_FRAME_NATIVE_CALLER is set: the name of the INTERNAL` |
|         - | 1324 | `	                   * function that reached for this callback (pVm->pCalleeName at the` |
|         - | 1325 | `	                   * dispatch, which aliases the host function's own sName and so lives` |
|         - | 1326 | `	                   * as long as the VM). php shows that builtin as a FRAME OF ITS OWN in` |
|         - | 1327 | `	                   * a backtrace, carrying the userland call site, while the callback's` |
|         - | 1328 | `	                   * own frame carries no file or line at all -- see VmBuildBacktrace. */` |
|         - | 1329 | `	ph7_foreach_step *pForeachSteps; /* Foreach steps this activation still owns, newest first.` |
|         - | 1330 | `	                   * Every step OP_FOREACH_INIT pushes is linked here and unlinked by the one` |
|         - | 1331 | `	                   * teardown door (VmForeachStepUnlink); whatever is left when the frame dies` |
|         - | 1332 | `	                   * is released with it. Without this a broken loop's step outlived its` |
|         - | 1333 | `	                   * activation for the life of the VM -- ~140 bytes plus a retain of the` |
|         - | 1334 | `	                   * subject each -- and INIT's reclaim scan walked every one of them. */` |
|         - | 1335 | `	int nActualArgs;  /* Actual call arity (band A #4): how many arguments the CALLER passed,` |
|         - | 1336 | `	                   * stamped by the OP_CALL / generator-fiber install sites; -1 when` |
|         - | 1337 | `	                   * unknown (non-call frames) - func_num_args()/func_get_args() then fall` |
|         - | 1338 | `	                   * back to the installed-formals count. Unlike sArg this excludes` |
|         - | 1339 | `	                   * defaulted params and counts variadic-packed args individually. */` |
|         - | 1340 | `	/* Where this activation's variables live, BY NUMBER: aLocalSlot[k] is the value` |
|         - | 1341 | `	 * slot the body's k-th variable name is bound to, plus one (0 = not resolved yet).` |
|         - | 1342 | `	 * The number comes from the bytecode, not from the name -- VmNumberLocals walks a` |
|         - | 1343 | `	 * body once and writes each variable instruction's number into its nSite -- so a` |
|         - | 1344 | `	 * read is an array index and a name is hashed at most ONCE per activation instead` |
|         - | 1345 | `	 * of once per access.` |
|         - | 1346 | `	 *` |
|         - | 1347 | `	 * It replaced an eight-entry memo keyed by the name's ADDRESS, which missed 28.5%` |
|         - | 1348 | `	 * of reads on the ecosystem gate's phpcs step and whose misses were LUCK: the` |
|         - | 1349 | `	 * entry a name landed in depended on where the compiler's pool happened to intern` |
|         - | 1350 | `	 * it, so the same commit measured 343.6M, 349.4M and 414.0M frame lookups in three` |
|         - | 1351 | `	 * builds. A number the bytecode carries has none of that in it.` |
|         - | 1352 | `	 *` |
|         - | 1353 | `	 * pCodeBase is what makes a number MEAN anything: it is the instruction array this` |
|         - | 1354 | `	 * frame's numbers were assigned against, so a body sharing the frame but not the` |
|         - | 1355 | `	 * numbering -- an include, an eval, a default-argument mini-program -- is told` |
|         - | 1356 | `	 * apart by one compare and falls back to the hash. Emptied by the three doors that` |
|         - | 1357 | `	 * can move a name to another slot -- PH7_VmBindVarSlot, PH7_VmRebindVarSlot and` |
|         - | 1358 | `	 * VmUnsetVarByNameEx -- see VmVarMemoFlush. */` |
|         - | 1359 | `	const VmInstr *pCodeBase;             /* the body aLocalSlot is numbered for, 0 = none */` |
|         - | 1360 | `	sxu32 aLocalSlot[PH7_VAR_SLOT_MAX];   /* slot index + 1, 0 = this name is unresolved here */` |
|         - | 1361 | `};` |
|         - | 1362 | `/*` |
|         - | 1363 | ` * One INTERNAL function or method that is RUNNING right now.` |
|         - | 1364 | ` *` |
|         - | 1365 | ` * php gives every internal call an execute_data of its own, so a throw raised` |
|         - | 1366 | `` * inside a C body leaves a trace frame naming that body -- `#0 file(line):`` |
|         - | 1367 | `` * str_repeat()`, `#0 file(line): SplFileObject->__construct()`. PHL pushes no`` |
|         - | 1368 | ` * VmFrame for a native call, so the trace started at the CALLER and a program` |
|         - | 1369 | `` * reading it saw `#0 {main}` where php names the function that failed.`` |
|         - | 1370 | ` *` |
|         - | 1371 | ` * The records live on the C stack of the dispatch that entered them (no` |
|         - | 1372 | ` * allocation), chained newest-first through pVm->pNativeCall. pFrame is the` |
|         - | 1373 | ` * userland activation that was current at entry, which is what says whether the` |
|         - | 1374 | ` * caller was bytecode or another internal function: a record whose predecessor` |
|         - | 1375 | ` * shares its pFrame was reached from inside that predecessor, and php prints` |
|         - | 1376 | `` * such a frame with NO file and NO line at all (`#0 [internal function]:`` |
|         - | 1377 | `` * str_repeat()` under `array_map('str_repeat', ...)`).`` |
|         - | 1378 | ` */` |
|         - | 1379 | `typedef struct VmNativeCall VmNativeCall;` |
|         - | 1380 | `struct VmNativeCall` |
|         - | 1381 | `{` |
|         - | 1382 | `	SyString *pName;        /* the internal function's or method's own name */` |
|         - | 1383 | `	ph7_class *pClass;      /* declaring class for a native METHOD, 0 for a function */` |
|         - | 1384 | ``	int bStatic;            /* a method declared static: php's separator is `::` */`` |
|         - | 1385 | `	sxu32 nLine;            /* the line the call was WRITTEN on (pVm->nCurLine at entry) */` |
|         - | 1386 | `	void *pFrame;           /* VmFrame current at entry -- borrowed, never dereferenced */` |
|         - | 1387 | `	VmNativeCall *pPrev;    /* the internal call this one was made from, or 0 */` |
|         - | 1388 | `};` |
|         - | 1389 | `#define VM_FRAME_EXCEPTION  0x01 /* Special Exception frame */` |
|         - | 1390 | `#define VM_FRAME_THROW      0x02 /* An exception was thrown */` |
|         - | 1391 | `#define VM_FRAME_CATCH      0x04 /* Catch frame */` |
|         - | 1392 | `#define VM_FRAME_NATIVE_CALLER 0x08 /* This activation was entered by an INTERNAL function` |
|         - | 1393 | `                                  * reaching for a userland callback (array_map, usort,` |
|         - | 1394 | `                                  * array_walk, preg_replace_callback, a shutdown function,` |
|         - | 1395 | `                                  * Reflection's invoke), so there is no userland call SITE` |
|         - | 1396 | `                                  * above it. php asks the same question of` |
|         - | 1397 | `                                  * prev_execute_data -- and it asks about the frame` |
|         - | 1398 | `                                  * IMMEDIATELY above, never walking past an internal one --` |
|         - | 1399 | `                                  * to decide whether an argument diagnostic ends with` |
|         - | 1400 | ``                                  * `, called in FILE on line N`.`` |
|         - | 1401 | `                                  *` |
|         - | 1402 | `                                  * Set from the SAME latch (bCallbackWeak) that binds such` |
|         - | 1403 | `                                  * a call's arguments weakly, and the two dispatches that` |
|         - | 1404 | `                                  * do not set it are the two php also treats as userland:` |
|         - | 1405 | `                                  * call_user_func()/call_user_func_array(), whose frame` |
|         - | 1406 | `                                  * php's compiler elides, and the AUTOLOAD call, which php` |
|         - | 1407 | `                                  * makes on behalf of the code that named the class and` |
|         - | 1408 | `                                  * whose diagnostic names that code's file and line. */` |
|         - | 1409 | `/*` |
|         - | 1410 | ` * One entry of a userland handler STACK (set_error_handler /` |
|         - | 1411 | ` * set_exception_handler). php's stack has no depth limit and every entry is a` |
|         - | 1412 | `` * real one -- the `null` a reset pushes included -- so each restore brings back`` |
|         - | 1413 | ` * exactly what the matching set replaced, and nothing under it is lost.` |
|         - | 1414 | ` */` |
|         - | 1415 | `typedef struct VmHandlerSlot VmHandlerSlot;` |
|         - | 1416 | `struct VmHandlerSlot {` |
|         - | 1417 | `	ph7_value sCb;   /* the saved handler, MEMOBJ_NULL for a reset entry */` |
|         - | 1418 | `	sxi64 iLevels;   /* its set_error_handler() $error_levels (E_ALL elsewhere) */` |
|         - | 1419 | `};` |
|         - | 1420 | `/*` |
|         - | 1421 | ` * php 8's E_ALL. The default of error_reporting() AND of set_error_handler()'s` |
|         - | 1422 | ` * $error_levels, so both read it from here (E_STRICT/2048 left the set in php 8).` |
|         - | 1423 | ` */` |
|         - | 1424 | `#define PH7_E_ALL_MASK 30719` |
|         - | 1425 | `/*` |
|         - | 1426 | ` * Suspendable execution context.` |
|         - | 1427 | ` * Used by Fiber and Generator to save/restore execution state.` |
|         - | 1428 | ` */` |
|         - | 1429 | `typedef struct ph7_exec_ctx ph7_exec_ctx;` |
|         - | 1430 | `/* Execution context states */` |
|         - | 1431 | `#define PH7_CTX_STATE_CREATED    0  /* Allocated but never started */` |
|         - | 1432 | `#define PH7_CTX_STATE_RUNNING    1  /* Currently executing */` |
|         - | 1433 | `#define PH7_CTX_STATE_SUSPENDED  2  /* Paused at suspend point */` |
|         - | 1434 | `#define PH7_CTX_STATE_COMPLETED  3  /* Returned normally */` |
|         - | 1435 | `#define PH7_CTX_STATE_CLOSED     4  /* Destroyed */` |
|         - | 1436 | `/*` |
|         - | 1437 | ` * REAL COROUTINE STACKS.` |
|         - | 1438 | ` *` |
|         - | 1439 | `` * A `Fiber::suspend()` reached through a C->PHP callback -- `array_map()`'s`` |
|         - | 1440 | ``  * callback, a `usort()` comparator, `call_user_func()`, `preg_replace_callback()` `` |
|         - | 1441 | ` * -- has to park the C frame of the builtin's own loop along with the PHP one.` |
|         - | 1442 | ` * The trampoline cannot: it flattens PHP->PHP calls into records inside ONE` |
|         - | 1443 | ` * native VmByteCodeExec activation, and a builtin's loop is a real C activation` |
|         - | 1444 | ` * above it. php switches native stacks; so does this. A fiber body runs on its` |
|         - | 1445 | ` * OWN C stack, a suspend switches back to the resumer's, and everything between` |
|         - | 1446 | ` * the two -- builtin frames, mini-programs, eval'd code, catch/finally bodies --` |
|         - | 1447 | ` * simply stays where it is.` |
|         - | 1448 | ` *` |
|         - | 1449 | ` * Three ways to switch, in preference order:` |
|         - | 1450 | ` *` |
|         - | 1451 | ` *  - Win32 fibers on Windows. The OS owns the stack and the switch.` |
|         - | 1452 | ` *  - A HAND-WRITTEN switch on x86-64 ELF: six callee-saved registers, the two` |
|         - | 1453 | ` *    floating-point control words, and the stack pointer. It is preferred over` |
|         - | 1454 | `` *    ucontext for a reason that is not speed: ASan intercepts `swapcontext` and`` |
|         - | 1455 | ` *    prints "ASan doesn't fully support makecontext/swapcontext functions"` |
|         - | 1456 | ` *    unconditionally on the first call, which lands in the middle of every .phpt` |
|         - | 1457 | ` *    an ASan build runs -- and the ASan corpora are a gate. Owning the switch` |
|         - | 1458 | ` *    also drops the sigprocmask syscall glibc's swapcontext makes.` |
|         - | 1459 | `` *  - `<ucontext.h>` on the other unixes. Correct everywhere it exists; only an`` |
|         - | 1460 | ` *    ASan build on such a platform sees that warning, and none is gated.` |
|         - | 1461 | ` *` |
|         - | 1462 | ` * Elsewhere (the ESP32 port; anything with no ucontext in its libc)` |
|         - | 1463 | ` * PH7_CORO_STACK is undefined, fibers keep the record-parking path, and a` |
|         - | 1464 | ` * suspend across a C boundary keeps raising the FiberError it raised before.` |
|         - | 1465 | ` * PH7_DISABLE_CORO_STACK forces that fallback; PH7_DISABLE_CORO_ASM keeps the` |
|         - | 1466 | ` * coroutine stacks but takes ucontext instead of the written switch (the escape` |
|         - | 1467 | ` * hatch if a CET shadow stack is ever turned on by default -- glibc's` |
|         - | 1468 | `` * swapcontext knows about it and a bare `ret` to a seeded frame does not).`` |
|         - | 1469 | ` */` |
|         - | 1470 | `#if !defined(PH7_DISABLE_CORO_STACK)` |
|         - | 1471 | `# if defined(__WINNT__)` |
|         - | 1472 | `#  define PH7_CORO_STACK 1` |
|         - | 1473 | `#  define PH7_CORO_WIN32 1` |
|         - | 1474 | `# elif defined(__x86_64__) && defined(__ELF__) && !defined(PH7_DISABLE_CORO_ASM) \` |
|         - | 1475 | `    && (defined(__GNUC__) \|\| defined(__clang__))` |
|         - | 1476 | `#  define PH7_CORO_STACK 1` |
|         - | 1477 | `#  define PH7_CORO_ASM_X64 1` |
|         - | 1478 | `# elif defined(__linux__) \|\| defined(__GLIBC__) \|\| defined(__APPLE__) \` |
|         - | 1479 | `    \|\| defined(__FreeBSD__) \|\| defined(__NetBSD__) \|\| defined(__DragonFly__)` |
|         - | 1480 | `#  define PH7_CORO_STACK 1` |
|         - | 1481 | `#  define PH7_CORO_UCONTEXT 1` |
|         - | 1482 | `# endif` |
|         - | 1483 | `#endif` |
|         - | 1484 | `#ifdef PH7_CORO_STACK` |
|         - | 1485 | `/* The switchable native stack itself (vm_exec_ctx.c owns the definition: a` |
|         - | 1486 | ` * mapping plus whatever the chosen backend needs to point at it -- two saved` |
|         - | 1487 | ` * stack pointers, a ucontext_t pair, or one Win32 fiber handle). */` |
|         - | 1488 | `typedef struct VmCoro VmCoro;` |
|         - | 1489 | `/*` |
|         - | 1490 | ` * What "which side is running" means to the VM, swapped at every stack switch.` |
|         - | 1491 | ` *` |
|         - | 1492 | ` * A suspended fiber's C frames stay FROZEN, so every piece of VM state such a` |
|         - | 1493 | ` * frame reaches for -- a scalar it saved a copy of and will restore when it` |
|         - | 1494 | ` * eventually unwinds, or a stack it recorded an index into -- has to travel` |
|         - | 1495 | ` * with the fiber instead of leaking into the resumer.` |
|         - | 1496 | ` *` |
|         - | 1497 | ` * Swapping is symmetric: switching in saves the resumer's set and installs the` |
|         - | 1498 | ` * fiber's, switching out does the reverse, so a value the fiber never touches` |
|         - | 1499 | ` * comes back to the resumer unchanged either way. A fresh fiber inherits the` |
|         - | 1500 | ` * resumer's scalars, except the ones that describe the C STACK or the three` |
|         - | 1501 | ` * stacks it owns privately -- it gets a new one of each (VmCoroStateInit).` |
|         - | 1502 | ` *` |
|         - | 1503 | ` * The frame CHAIN is deliberately not here: the ordinary attach/detach a body` |
|         - | 1504 | ` * run already does (VmStartCtx / VmSuspendCtxDetach / VmFinishCtxRun) moves it,` |
|         - | 1505 | ` * and pCoroTop alone records where inside the fiber to come back to.` |
|         - | 1506 | ` */` |
|         - | 1507 | `typedef struct VmCoroVmState VmCoroVmState;` |
|         - | 1508 | `struct VmCoroVmState` |
|         - | 1509 | `{` |
|         - | 1510 | `	/* The three stacks a coroutine used to park SLICES of. With a real stack the` |
|         - | 1511 | `	 * fiber owns them OUTRIGHT: its frozen C activations recorded absolute floors` |
|         - | 1512 | `	 * into these sets (an activation's nExceptionBase, nFinallyActBase) as C` |
|         - | 1513 | `	 * LOCALS on the fiber's own stack, which nothing can reach to rebase — so the` |
|         - | 1514 | `	 * sets must never shift under them. A private set never does, and the fiber` |
|         - | 1515 | `	 * resumes at whatever depth the resumer happens to be at with no rebasing at` |
|         - | 1516 | `	 * all. An exception the fiber body does not catch therefore leaves the body` |
|         - | 1517 | `	 * as a status rather than finding the RESUMER's handler from inside the` |
|         - | 1518 | `	 * fiber's frames, which is php's model too (it is re-raised at start()/` |
|         - | 1519 | `	 * resume()). */` |
|         - | 1520 | `	SySet aException;            /* pVm->aException: this side's live try handlers */` |
|         - | 1521 | `	SySet aFinallyAction;        /* ...its pending finally actions */` |
|         - | 1522 | `	SySet aSelf;                 /* ...and its self::/static:: class stack */` |
|         - | 1523 | `	/* Then every scalar a frozen C frame has a saved copy of. A suspended fiber's` |
|         - | 1524 | `	 * frames stay put, so each of these has to travel with the fiber rather than` |
|         - | 1525 | `	 * leak into the resumer -- and come back untouched when the fiber is resumed. */` |
|         - | 1526 | `	int nVmExecDepth;            /* native activations live on THIS C stack */` |
|         - | 1527 | `	int nRecursionDepth;         /* PHP call depth this side has open */` |
|         - | 1528 | `	sxu32 nCurLine;              /* saved+restored per native activation (VmByteCodeExec) */` |
|         - | 1529 | `	sxi32 nBoundaryRc;           /* likewise: the parked C-boundary throw status */` |
|         - | 1530 | `	SyString *pCalleeName;       /* the three the OP_CALL native branch saves around xFunc */` |
|         - | 1531 | `	SyString *pNativeFrameName;` |
|         - | 1532 | `	int bHostDiscard;` |
|         - | 1533 | ``	int nErrSuppress;            /* '@' depth: a suspend inside `@f()` must not mute the resumer */`` |
|         - | 1534 | `	int nExceptDepth;` |
|         - | 1535 | `	int nExcCtorDepth;` |
|         - | 1536 | `	int nMuteThrow;              /* the muted / speculative / const-eval windows: all three are */` |
|         - | 1537 | `	int nSpeculative;            /* C regions with a matched decrement the fiber has not reached */` |
|         - | 1538 | `	sxi32 nConstEvalDepth;` |
|         - | 1539 | `	sxu32 nLazyInitLine;         /* and the lazy-initializer line override, which is depth-keyed */` |
|         - | 1540 | `	sxi32 nLazyInitDepth;        /* on nVmExecDepth and so is meaningless on the other stack */` |
|         - | 1541 | `	int nObDepth;                /* "inside an output handler": the handler's C frame is on one` |
|         - | 1542 | `	                              * stack only, so ob_get_level() must answer for the side asking */` |
|         - | 1543 | `	sxu32 nObActive;` |
|         - | 1544 | `	VmFrame *pObFrame;` |
|         - | 1545 | `	ph7_exec_ctx *pCoroCtx;      /* which fiber's stack this side is (NULL for a resumer) */` |
|         - | 1546 | `};` |
|         - | 1547 | `#endif /* PH7_CORO_STACK */` |
|         - | 1548 | `struct ph7_exec_ctx` |
|         - | 1549 | `{` |
|         - | 1550 | `	ph7_vm *pVm;              /* Owning VM */` |
|         - | 1551 | `	ph7_vm_func *pFunc;       /* The function being executed */` |
|         - | 1552 | `	VmFrame *pFrame;          /* Detached execution frame */` |
|         - | 1553 | `	ph7_value *pStack;        /* Private operand stack */` |
|         - | 1554 | `	sxu32 nStackCap;          /* Its allocated slot count (VmNewOperandStack size); grows` |
|         - | 1555 | `	                           * with pStack when an OP_SPREAD in this body reallocs it */` |
|         - | 1556 | `	sxu32 nStackOrig;         /* The ORIGINAL (ungrown) capacity — fixed at creation and used` |
|         - | 1557 | `	                           * to seed each resume's headroom reference, so a spread inside a` |
|         - | 1558 | `	                           * yield loop can't ratchet the stack up across resumes */` |
|         - | 1559 | `	sxi32 nTos;               /* Saved top-of-stack index */` |
|         - | 1560 | `	sxi32 pc;                 /* Saved program counter (resume point) */` |
|         - | 1561 | `	sxi32 iState;             /* One of PH7_CTX_STATE_* */` |
|         - | 1562 | `	sxu8 bThrew;              /* The body ENDED by letting an exception escape. php keeps the two` |
|         - | 1563 | `	                           * apart: such a fiber is terminated like any other, but getReturn()` |
|         - | 1564 | `	                           * says it threw rather than that it has not returned. */` |
|         - | 1565 | `	ph7_value sSuspendValue;  /* Value passed out via Fiber::suspend() / yield */` |
|         - | 1566 | `	ph7_value sRetValue;      /* Final return value */` |
|         - | 1567 | `	sxu32 nExceptionBase;     /* Exception-stack depth below this body's own handlers` |
|         - | 1568 | `	                           * (caller depth); refreshed at each resume */` |
|         - | 1569 | `	SySet aSavedException;    /* This body's own exception handlers (ph7_exception*),` |
|         - | 1570 | `	                           * parked here while suspended so a generator/fiber that` |
|         - | 1571 | `	                           * suspends inside a try does not corrupt the caller's` |
|         - | 1572 | `	                           * exception stack */` |
|         - | 1573 | `	SySet aSavedFinally;      /* ROOT C: this body's own pending finally actions` |
|         - | 1574 | `	                           * (VmFinallyAction), parked while suspended so a generator` |
|         - | 1575 | `	                           * that yields inside a finally reached by return/break/rethrow` |
|         - | 1576 | `	                           * does not leave its record on the shared VM stack (where an` |
|         - | 1577 | `	                           * out-of-order-resumed sibling generator would mis-pop it) */` |
|         - | 1578 | `	sxu32 nFinallyBase;       /* aFinallyAction depth below this body's own records */` |
|         - | 1579 | `	SySet aSavedSelf;         /* Stage 4: this coroutine's own aSelf (self::/static::)` |
|         - | 1580 | `	                           * entries, parked while suspended (ph7_class* pointers) */` |
|         - | 1581 | `	sxu32 nSelfBase;          /* aSelf depth below this coroutine's own pushes */` |
|         - | 1582 | `	ph7_class *pLsbClass;     /* The late-static-binding class the body runs under, captured` |
|         - | 1583 | `	                           * when the coroutine was CREATED. A generator body resumes long` |
|         - | 1584 | `	                           * after the call that made it returned, so pVm->aSelf no longer` |
|         - | 1585 | `` 	                           * carries the class the method was called through and `static::` `` |
|         - | 1586 | `	                           * inside the body answered "Class \"static\" not found" -- for` |
|         - | 1587 | ``	                           * `new static`, `static::method()` and `static::class` alike.`` |
|         - | 1588 | `	                           * php binds the called scope to the generator at creation and` |
|         - | 1589 | `	                           * restores it on every resume; this is that scope. Borrowed. */` |
|         - | 1590 | `	SySet aByRefArg;          /* Caller slots (sxu32) this body's by-REFERENCE parameters` |
|         - | 1591 | `	                           * alias. The body outlives its caller's frame, so whichever` |
|         - | 1592 | `	                           * of the two dies last releases the slot: the caller's` |
|         - | 1593 | `	                           * teardown counts this frame's name as a holder and skips it,` |
|         - | 1594 | `	                           * and this ctx's teardown asks PH7_VmReleaseUnheldSlot once` |
|         - | 1595 | `	                           * its own names are gone. */` |
|         - | 1596 | `	void *pPrivate;           /* Generator wrapper (ph7_generator*) or NULL for fibers */` |
|         - | 1597 | `	ph7_class_instance *pInjected; /* Generator::throw() inject-at-yield: exception to raise at` |
|         - | 1598 | `	                                * the suspended yield on the next resume, or NULL. One-shot:` |
|         - | 1599 | `	                                * consumed (cleared) by the loop-top inject check. Holds a` |
|         - | 1600 | `	                                * reference for the duration of the resume. */` |
|         - | 1601 | `	sxu8 bClosing;                 /* Set while VmCloseCtx force-drives this suspended generator's` |
|         - | 1602 | ``	                                * pending `finally` blocks at destruction (unset / out-of-scope`` |
|         - | 1603 | `	                                * / GC before completion). The body-resume entry redirects into` |
|         - | 1604 | `	                                * the innermost open try's finally chain instead of resuming at` |
|         - | 1605 | `	                                * the yield, and OP_YIELD raises PHP's "Cannot yield from finally` |
|         - | 1606 | `	                                * in a force-closed generator". Stays set for the whole close run. */` |
|         - | 1607 | ``	/* `yield from` delegation state — per generator instance, so independent`` |
|         - | 1608 | `	 * instances never clash (unlike the shared foreach aStep). */` |
|         - | 1609 | `	ph7_value sDelegate;             /* The iterable being delegated (kept alive) */` |
|         - | 1610 | `	ph7_hashmap_node *pDelegateNode; /* Array cursor: next node to read, else 0 */` |
|         - | 1611 | `	sxi32 iDelegateState;            /* 0=inactive, 1=array, 2=iterator, 3=generator */` |
|         - | 1612 | `	/* BYTECODE stage 4: deep Fiber::suspend() record-segment parking. */` |
|         - | 1613 | `	void *pParkedSegment;            /* VmParkedSegment* (opaque here): the trampoline` |
|         - | 1614 | `	                                  * record chain + innermost activation parked when a` |
|         - | 1615 | `	                                  * suspend fires inside a nested PHP call; NULL when` |
|         - | 1616 | `	                                  * suspended at the body level (pc/nTos above suffice) */` |
|         - | 1617 | `	int nBodyExecDepth;              /* pVm->nVmExecDepth of this ctx's body invocation. A` |
|         - | 1618 | `	                                  * suspend at a DEEPER native depth is inside a C->PHP` |
|         - | 1619 | `	                                  * callback (usort comparator, etc.) and cannot park` |
|         - | 1620 | `	                                  * across the native frame — it raises a catchable` |
|         - | 1621 | `	                                  * FiberError instead. Only meaningful on the fallback` |
|         - | 1622 | `	                                  * path: a fiber running on its own stack (pCoro != 0)` |
|         - | 1623 | `	                                  * suspends from any depth. */` |
|         - | 1624 | `#ifdef PH7_CORO_STACK` |
|         - | 1625 | `	VmCoro *pCoro;                   /* This fiber's own native stack, or NULL: generators never` |
|         - | 1626 | ``	                                  * take one (a `yield` is lexically in the body, so it never`` |
|         - | 1627 | `	                                  * crosses a C frame), and neither does a fiber on a build` |
|         - | 1628 | `	                                  * with no stack-switch primitive. */` |
|         - | 1629 | `	VmCoroVmState sSaved;            /* The fiber side's VM state while the resumer runs */` |
|         - | 1630 | `	VmCoroVmState sHost;             /* The resumer's, while the fiber runs */` |
|         - | 1631 | `	VmFrame *pCoroTop;               /* pVm->pFrame at the suspend the fiber is parked on --` |
|         - | 1632 | `	                                  * its innermost callee or open-try wrapper, which the` |
|         - | 1633 | `	                                  * resume makes current again so the frozen stack picks` |
|         - | 1634 | `	                                  * up where it stopped. The frame chain itself is not` |
|         - | 1635 | `	                                  * swapped: the ordinary attach/detach around a body run` |
|         - | 1636 | `	                                  * already moves it. */` |
|         - | 1637 | `	sxi32 iCoroRc;                   /* What the body invocation returned, read by the resumer` |
|         - | 1638 | `	                                  * after the final switch back (SXRET_OK / PH7_ABORT /` |
|         - | 1639 | `	                                  * PH7_EXCEPTION); PH7_SUSPEND is never stored — a suspend` |
|         - | 1640 | `	                                  * is a switch, not a return. */` |
|         - | 1641 | `	sxu8 bCoroDone;                  /* The body ran off its end: the stack is spent and must` |
|         - | 1642 | `	                                  * not be switched into again. */` |
|         - | 1643 | `	sxu8 bCoroKill;                  /* Set by the teardown before the last switch in: the` |
|         - | 1644 | `	                                  * suspend the fiber is parked on returns PH7_ABORT instead` |
|         - | 1645 | `	                                  * of a value, so its C frames unwind and free what they own` |
|         - | 1646 | `	                                  * rather than being freed underneath. */` |
|         - | 1647 | `	ph7_class_instance *pEscaped;    /* A throw the body did not catch. A fiber on its own stack` |
|         - | 1648 | `	                                  * has its own handler stack, so an unmatched throw is not` |
|         - | 1649 | `	                                  * "uncaught" -- it LEAVES the fiber, and php re-raises it` |
|         - | 1650 | `	                                  * at the start()/resume() that ran the body. This carries` |
|         - | 1651 | `	                                  * the instance across (holding a reference) for` |
|         - | 1652 | `	                                  * VmFiberRaiseEscaped to re-throw in the resumer's frame. */` |
|         - | 1653 | `#endif /* PH7_CORO_STACK */` |
|         - | 1654 | `};` |
|         - | 1655 | `/* Special return code from VmByteCodeExec signaling fiber suspension */` |
|         - | 1656 | `#define PH7_SUSPEND  0x100` |
|         - | 1657 | `/*` |
|         - | 1658 | ` * Generator wrapper around ph7_exec_ctx.` |
|         - | 1659 | ` * Adds yield key tracking on top of the suspendable execution context.` |
|         - | 1660 | ` */` |
|         - | 1661 | `typedef struct ph7_generator ph7_generator;` |
|         - | 1662 | `struct ph7_generator` |
|         - | 1663 | `{` |
|         - | 1664 | `	ph7_exec_ctx *pCtx;       /* Execution context (allocated separately) */` |
|         - | 1665 | `	ph7_value sYieldValue;    /* Last yielded value (for current()) */` |
|         - | 1666 | `	ph7_value sYieldKey;      /* Last yielded key (for key()) */` |
|         - | 1667 | `	sxi64 iImplicitKey;       /* Auto-increment key counter */` |
|         - | 1668 | `	sxu8 bAtFirstYield;       /* php's ZEND_GENERATOR_AT_FIRST_YIELD: set when the` |
|         - | 1669 | `	                           * PRIMING run suspends, cleared by every resume after` |
|         - | 1670 | `	                           * it. It is the whole of php's rewind rule — a` |
|         - | 1671 | `	                           * generator that has moved past its first yield, or` |
|         - | 1672 | `	                           * finished, cannot be rewound. */` |
|         - | 1673 | `};` |
|         - | 1674 | `/*` |
|         - | 1675 | ` * Output control buffer entry.` |
|         - | 1676 | ` */` |
|         - | 1677 | `typedef struct VmObEntry VmObEntry;` |
|         - | 1678 | `struct VmObEntry` |
|         - | 1679 | `{` |
|         - | 1680 | `	ph7_value sCallback; /* User defined callback */` |
|         - | 1681 | `	SyBlob sOB;          /* Output buffer consumer (RAW bytes: php runs the` |
|         - | 1682 | `	                      * handler on the way OUT, not on the way in) */` |
|         - | 1683 | `	ph7_int64 iFlags;    /* PH7_OB_* below, php's own numeric values. 64 bits wide` |
|         - | 1684 | `	                      * because php stores whatever it was given (minus the two` |
|         - | 1685 | `	                      * nibbles it reserves) and reports it back verbatim. */` |
|         - | 1686 | `	ph7_int64 nChunk;    /* ob_start()'s $chunk_size (0 or negative: buffer` |
|         - | 1687 | `	                      * everything). 64 bits: php accepts a chunk larger than a` |
|         - | 1688 | `	                      * 32-bit count and reports it back. */` |
|         - | 1689 | `	ph7_int64 nSize;     /* php's ALLOCATION for this buffer, which ob_get_status()` |
|         - | 1690 | `	                      * reports: 16 KB, or the chunk size rounded up to 4 KB, and` |
|         - | 1691 | `	                      * grown by php's own rule on each write. Tracked rather than` |
|         - | 1692 | `	                      * derived because the answer depends on how the bytes` |
|         - | 1693 | `	                      * ARRIVED — 40 writes of 1000 give 49152 where one write of` |
|         - | 1694 | `	                      * 40000 gives 40960. */` |
|         - | 1695 | `};` |
|         - | 1696 | `/*` |
|         - | 1697 | ` * Output-handler flags and phases. These are php's own values: the first group is` |
|         - | 1698 | `` * what ob_get_status() reports in its `flags` entry, the second what the handler`` |
|         - | 1699 | `` * receives as its `$phase` argument.`` |
|         - | 1700 | ` */` |
|         - | 1701 | `#define PH7_OB_USER      0x0001 /* Handler is a userland callback */` |
|         - | 1702 | `#define PH7_OB_CLEANABLE 0x0010` |
|         - | 1703 | `#define PH7_OB_FLUSHABLE 0x0020` |
|         - | 1704 | `#define PH7_OB_REMOVABLE 0x0040` |
|         - | 1705 | `#define PH7_OB_STDFLAGS  0x0070` |
|         - | 1706 | `#define PH7_OB_STARTED   0x1000 /* Handler has been invoked at least once */` |
|         - | 1707 | `#define PH7_OB_DISABLED  0x2000 /* Handler answered FALSE: never called again */` |
|         - | 1708 | `#define PH7_OB_PROCESSED 0x4000 /* Handler has produced output */` |
|         - | 1709 | `/* What ob_start() keeps of the $flags it is given: everything except the phase` |
|         - | 1710 | ` * nibble and the state nibble, which are the engine's own to set. */` |
|         - | 1711 | `#define PH7_OB_FLAGMASK  (~(ph7_int64)0xF00F)` |
|         - | 1712 | `/* Phases (an op, plus PH7_OB_START until the handler has run once) */` |
|         - | 1713 | `#define PH7_OB_WRITE 0` |
|         - | 1714 | `#define PH7_OB_START 1` |
|         - | 1715 | `#define PH7_OB_CLEAN 2` |
|         - | 1716 | `#define PH7_OB_FLUSH 4` |
|         - | 1717 | `#define PH7_OB_FINAL 8` |
|         - | 1718 | `/*` |
|         - | 1719 | ` * mt_srand()/srand()'s $mode. php compares the argument against MT_RAND_PHP for` |
|         - | 1720 | ` * EQUALITY, so every other value — including an out-of-range one — selects the` |
|         - | 1721 | ` * standard generator.` |
|         - | 1722 | ` */` |
|         - | 1723 | `/* stream_wrapper_register()'s $flags: php defines this one bit. A wrapper that` |
|         - | 1724 | ` * declares itself a URL is the one allow_url_fopen and allow_url_include gate. */` |
|         - | 1725 | `#define PH7_STREAM_IS_URL 1` |
|         - | 1726 | `/*` |
|         - | 1727 | ` * The rest of the streamWrapper protocol's vocabulary, in php's own numbers.` |
|         - | 1728 | ` *` |
|         - | 1729 | `` * USE_PATH / IGNORE_URL / REPORT_ERRORS / MUST_SEEK are the `$options` bits`` |
|         - | 1730 | ` * stream_open() is handed; URL_STAT_LINK / URL_STAT_QUIET are url_stat()'s` |
|         - | 1731 | `` * `$flags` (LINK means lstat, QUIET means report a miss in silence), and NOCACHE`` |
|         - | 1732 | ` * rides beside them on every ask php's stat family makes because php's own` |
|         - | 1733 | ` * one-entry stat cache sits ABOVE that door; MKDIR_RECURSIVE is mkdir()'s;` |
|         - | 1734 | ` * META_* names the verb stream_metadata() is asked for; the OPTION_ and BUFFER_` |
|         - | 1735 | ` * pair belong to stream_set_option(), and CAST_ to stream_cast().` |
|         - | 1736 | ` */` |
|         - | 1737 | `#define PH7_STREAM_USE_PATH           1` |
|         - | 1738 | `#define PH7_STREAM_IGNORE_URL         2` |
|         - | 1739 | `#define PH7_STREAM_REPORT_ERRORS      8` |
|         - | 1740 | `#define PH7_STREAM_MUST_SEEK          16` |
|         - | 1741 | `#define PH7_URL_STAT_LINK             1` |
|         - | 1742 | `#define PH7_URL_STAT_QUIET            2` |
|         - | 1743 | `#define PH7_URL_STAT_NOCACHE          4` |
|         - | 1744 | `#define PH7_STREAM_MKDIR_RECURSIVE    1` |
|         - | 1745 | `#define PH7_STREAM_META_TOUCH         1` |
|         - | 1746 | `#define PH7_STREAM_META_OWNER_NAME    2` |
|         - | 1747 | `#define PH7_STREAM_META_OWNER         3` |
|         - | 1748 | `#define PH7_STREAM_META_GROUP_NAME    4` |
|         - | 1749 | `#define PH7_STREAM_META_GROUP         5` |
|         - | 1750 | `#define PH7_STREAM_META_ACCESS        6` |
|         - | 1751 | `#define PH7_STREAM_OPTION_BLOCKING    1` |
|         - | 1752 | `#define PH7_STREAM_OPTION_READ_BUFFER 2` |
|         - | 1753 | `#define PH7_STREAM_OPTION_WRITE_BUFFER 3` |
|         - | 1754 | `#define PH7_STREAM_OPTION_READ_TIMEOUT 4` |
|         - | 1755 | `#define PH7_STREAM_BUFFER_NONE        0` |
|         - | 1756 | `#define PH7_STREAM_BUFFER_LINE        1` |
|         - | 1757 | `#define PH7_STREAM_BUFFER_FULL        2` |
|         - | 1758 | `#define PH7_STREAM_CAST_AS_STREAM     0` |
|         - | 1759 | `#define PH7_STREAM_CAST_FOR_SELECT    3` |
|         - | 1760 | `/*` |
|         - | 1761 | ` * What PH7_StreamUserUrlStat() answered: the wrapper filled the record, the` |
|         - | 1762 | ` * wrapper declined, or no userland wrapper owns this path at all (the caller then` |
|         - | 1763 | ` * asks the VFS exactly as it always did).` |
|         - | 1764 | ` */` |
|         - | 1765 | `#define PHL_URLSTAT_OK      0` |
|         - | 1766 | `#define PHL_URLSTAT_FAIL    1` |
|         - | 1767 | `#define PHL_URLSTAT_NOWRAP (-1)` |
|         - | 1768 | `/*` |
|         - | 1769 | ` * Which member of the stat family is asking. php routes them all through one` |
|         - | 1770 | `` * `php_stat`, and the code decides three things: the flags the wrapper is handed,`` |
|         - | 1771 | ` * whether a miss is silent, and which of the thirteen fields answers.` |
|         - | 1772 | ` * The seven QUIET ones come first on purpose -- that ORDER is the test.` |
|         - | 1773 | ` */` |
|         - | 1774 | `#define PH7_STAT_ASK_EXISTS   0` |
|         - | 1775 | `#define PH7_STAT_ASK_IS_FILE  1` |
|         - | 1776 | `#define PH7_STAT_ASK_IS_DIR   2` |
|         - | 1777 | `#define PH7_STAT_ASK_IS_LINK  3` |
|         - | 1778 | `#define PH7_STAT_ASK_IS_R     4` |
|         - | 1779 | `#define PH7_STAT_ASK_IS_W     5` |
|         - | 1780 | `#define PH7_STAT_ASK_IS_X     6` |
|         - | 1781 | `#define PH7_STAT_ASK_SIZE     7` |
|         - | 1782 | `#define PH7_STAT_ASK_ATIME    8` |
|         - | 1783 | `#define PH7_STAT_ASK_MTIME    9` |
|         - | 1784 | `#define PH7_STAT_ASK_CTIME    10` |
|         - | 1785 | `#define PH7_STAT_ASK_OWNER    11` |
|         - | 1786 | `#define PH7_STAT_ASK_GROUP    12` |
|         - | 1787 | `#define PH7_STAT_ASK_INODE    13` |
|         - | 1788 | `#define PH7_STAT_ASK_PERMS    14` |
|         - | 1789 | `#define PH7_STAT_ASK_TYPE     15` |
|         - | 1790 | `#define PH7_STAT_ASK_STAT     16` |
|         - | 1791 | `#define PH7_STAT_ASK_LSTAT    17` |
|         - | 1792 | `/* php's S_IFMT decode, spelled in octal so it means the same on every port. */` |
|         - | 1793 | `#define PH7_S_IFMT   0170000` |
|         - | 1794 | `#define PH7_S_IFIFO  0010000` |
|         - | 1795 | `#define PH7_S_IFCHR  0020000` |
|         - | 1796 | `#define PH7_S_IFDIR  0040000` |
|         - | 1797 | `#define PH7_S_IFBLK  0060000` |
|         - | 1798 | `#define PH7_S_IFREG  0100000` |
|         - | 1799 | `#define PH7_S_IFLNK  0120000` |
|         - | 1800 | `#define PH7_S_IFSOCK 0140000` |
|         - | 1801 | `/* stream_socket_client()'s $flags. CONNECT is its default; without it php` |
|         - | 1802 | ` * creates no socket at all. PERSISTENT is what pfsockopen() means, and is the` |
|         - | 1803 | ` * only one that changes what a second call ANSWERS. */` |
|         - | 1804 | `#define PH7_STREAM_CLIENT_PERSISTENT    1` |
|         - | 1805 | `#define PH7_STREAM_CLIENT_ASYNC_CONNECT 2` |
|         - | 1806 | `#define PH7_STREAM_CLIENT_CONNECT       4` |
|         - | 1807 | `/* One live persistent socket: php's registry key is the address the opener was` |
|         - | 1808 | ` * given, spelling included ("localhost:80" and "127.0.0.1:80" are two). The` |
|         - | 1809 | ` * handle itself is the VFS's io_private, declared with the rest of that layer. */` |
|         - | 1810 | `typedef struct io_private io_private;` |
|         - | 1811 | `typedef struct VmPersistSock VmPersistSock;` |
|         - | 1812 | `struct VmPersistSock` |
|         - | 1813 | `{` |
|         - | 1814 | `	char zKey[320];` |
|         - | 1815 | `	io_private *pDev;` |
|         - | 1816 | `};` |
|         - | 1817 | `/* stream_socket_server()'s $flags. php keeps the two apart because a DATAGRAM` |
|         - | 1818 | ` * server is bound and never listens; LISTEN is what makes a bound socket a` |
|         - | 1819 | ` * stream server, and dropping it leaves a socket nothing can connect to. */` |
|         - | 1820 | `#define PH7_STREAM_SERVER_BIND   4` |
|         - | 1821 | `#define PH7_STREAM_SERVER_LISTEN 8` |
|         - | 1822 | `/* stream_socket_shutdown()'s $mode and the recvfrom/sendto flags: php's own` |
|         - | 1823 | ` * numbering, which is NOT the OS's (MSG_OOB and MSG_PEEK are mapped in net.c). */` |
|         - | 1824 | `#define PH7_STREAM_SHUT_RD   0` |
|         - | 1825 | `#define PH7_STREAM_SHUT_WR   1` |
|         - | 1826 | `#define PH7_STREAM_SHUT_RDWR 2` |
|         - | 1827 | `#define PH7_STREAM_OOB       1` |
|         - | 1828 | `#define PH7_STREAM_PEEK      2` |
|         - | 1829 | `#define PH7_MT_RAND_MT19937 0` |
|         - | 1830 | `#define PH7_MT_RAND_PHP     1` |
|         - | 1831 | `/*` |
|         - | 1832 | ` * HTTP response header entry.` |
|         - | 1833 | ` * Stored in ph7_vm.aResponseHeaders (a SySet of VmResponseHeader).` |
|         - | 1834 | ` */` |
|         - | 1835 | `typedef struct VmResponseHeader VmResponseHeader;` |
|         - | 1836 | `struct VmResponseHeader` |
|         - | 1837 | `{` |
|         - | 1838 | `	SyString sName;   /* Header name (e.g. "Content-Type"), case-preserving */` |
|         - | 1839 | `	SyString sValue;  /* Header value (e.g. "text/html") */` |
|         - | 1840 | `};` |
|         - | 1841 | `/*` |
|         - | 1842 | ` * Each collected function argument is recorded in an instance` |
|         - | 1843 | ` * of the following structure.` |
|         - | 1844 | ` * Note that as an extension, PH7 implements full type hinting` |
|         - | 1845 | ` * which mean that any function can have it's own signature.` |
|         - | 1846 | ` * Example:` |
|         - | 1847 | ` *      function foo(int $a,string $b,float $c,ClassInstance $d){}` |
|         - | 1848 | ` * This is how the powerful function overloading mechanism is` |
|         - | 1849 | ` * implemented.` |
|         - | 1850 | ` * Note that as an extension, PH7 allow function arguments to have` |
|         - | 1851 | ` * any complex default value associated with them unlike the standard` |
|         - | 1852 | ` * PHP engine.` |
|         - | 1853 | ` * Example:` |
|         - | 1854 | ` *    function foo(int $a = rand() & 1023){}` |
|         - | 1855 | ` *    now, when foo is called without arguments [i.e: foo()] the` |
|         - | 1856 | ` *    $a variable (first parameter) will be set to a random number` |
|         - | 1857 | ` *    between 0 and 1023 inclusive.` |
|         - | 1858 | ` * Refer to the official documentation for more information on this` |
|         - | 1859 | ` * mechanism and other extension introduced by the PH7 engine.` |
|         - | 1860 | ` */` |
|         - | 1861 | `struct ph7_vm_func_arg` |
|         - | 1862 | `{` |
|         - | 1863 | `	SyString sName;      /* Argument name */` |
|         - | 1864 | `	SySet aByteCode;     /* Compiled default value associated with this argument */` |
|         - | 1865 | `	sxu32 nType;         /* Type of this argument [i.e: array, int, string, float, object, etc.] */` |
|         - | 1866 | `	SyString sClass;     /* Class name if the argument expect a class instance [i.e: function foo(BaseClass $bar){} ] */` |
|         - | 1867 | `	sxi32 iFlags;        /* Configuration flags */` |
|         - | 1868 | `	SySet aUnionAlts;    /* Union type alternatives (ph7_type_alt). Empty unless VM_FUNC_ARG_UNION is set. */` |
|         - | 1869 | `	SyString sTypeName;  /* Original type text for error messages, normalized in canonical PHP order */` |
|         - | 1870 | `	sxi32 iPromoteVis;   /* PH7_CLASS_PROT_* when VM_FUNC_ARG_PROMOTED is set */` |
|         - | 1871 | `	SySet aAttrs;        /* Declared #[...] attributes (ph7_attribute records) */` |
|         - | 1872 | `};` |
|         - | 1873 | `/*` |
|         - | 1874 | ` * One alternative within a union type declaration. Used by parameters,` |
|         - | 1875 | `` * return types, and properties when the declaration is `T1\|T2\|...`,`` |
|         - | 1876 | `` * `A&B` (intersection), or `(A&B)\|C` (DNF).`` |
|         - | 1877 | ` */` |
|         - | 1878 | `typedef struct ph7_type_alt ph7_type_alt;` |
|         - | 1879 | `struct ph7_type_alt` |
|         - | 1880 | `{` |
|         - | 1881 | `	sxu32 nType;     /* MEMOBJ_* bitmask, or SXU32_HIGH for a class/interface alternative */` |
|         - | 1882 | `	SyString sClass; /* Class/interface name when nType == SXU32_HIGH */` |
|         - | 1883 | `	sxu32 nGroup;    /* Intersection-group id: atoms sharing a group are ANDed (A&B),` |
|         - | 1884 | `	                  * distinct groups are ORed. A pure union is one atom per group. */` |
|         - | 1885 | `};` |
|         - | 1886 | `/* Maximum alternatives in one type declaration; bounds the on-stack atom array` |
|         - | 1887 | ` * in the parser and the per-group tally in the enforcer. Larger than any real` |
|         - | 1888 | ` * union/DNF type. */` |
|         - | 1889 | `#define PHL_UNION_MAX_ALTS 32` |
|         - | 1890 | `/*` |
|         - | 1891 | ` * Each static variable is parsed out and remembered in an instance` |
|         - | 1892 | ` * of the following structure.` |
|         - | 1893 | ` * Note that as an extension, PH7 allow static variable have` |
|         - | 1894 | ` * any complex default value associated with them unlike the standard` |
|         - | 1895 | ` * PHP engine.` |
|         - | 1896 | ` * Example:` |
|         - | 1897 | ` *   static $rand_str = 'PH7'.rand_str(3); // Concatenate 'PH7' with` |
|         - | 1898 | ` *                                         // a random three characters(English alphabet)` |
|         - | 1899 | ` *   var_dump($rand_str);` |
|         - | 1900 | ` *   //You should see something like this` |
|         - | 1901 | ` *   string(6 'PH7awt');` |
|         - | 1902 | ` */` |
|         - | 1903 | `struct ph7_vm_func_static_var` |
|         - | 1904 | `{` |
|         - | 1905 | `	SyString sName;   /* Static variable name */` |
|         - | 1906 | `	SySet aByteCode;  /* Compiled initialization expression  */` |
|         - | 1907 | `	sxu32 nIdx;       /* Object index in the global memory object container */` |
|         - | 1908 | `};` |
|         - | 1909 | `/*` |
|         - | 1910 | ` * Each imported variable from the outside closure environnment is recoded` |
|         - | 1911 | ` * in an instance of the following structure.` |
|         - | 1912 | ` */` |
|         - | 1913 | `struct ph7_vm_func_closure_env` |
|         - | 1914 | `{` |
|         - | 1915 | `	SyString sName;   /* Imported variable name */` |
|         - | 1916 | `	int iFlags;       /* Control flags */` |
|         - | 1917 | ``	sxu32 nLine;      /* Source line of this `use ($x)` capture (0 = unknown/implicit):`` |
|         - | 1918 | `					   * php reports an undefined by-value capture's E_WARNING at the` |
|         - | 1919 | `					   * variable's own line, which can differ from the closure keyword's` |
|         - | 1920 | ``					   * line when the `use` clause wraps. Set only for explicit captures. */`` |
|         - | 1921 | `	ph7_value sValue; /* Imported variable value */` |
|         - | 1922 | `	sxu32 nIdx;       /* Reference to the bounded variable if passed by reference` |
|         - | 1923 | `					   *[Example:` |
|         - | 1924 | `					   *  $x = 1;` |
|         - | 1925 | `					   *  $closure = function() use (&$x) { ++$x; }` |
|         - | 1926 | `					   *  $closure();` |
|         - | 1927 | `					   *]` |
|         - | 1928 | `					   */` |
|         - | 1929 | `};` |
|         - | 1930 | `/* Function configuration flags */` |
|         - | 1931 | `#define VM_FUNC_ARG_BY_REF   0x001 /* Argument passed by reference */` |
|         - | 1932 | `#define VM_FUNC_ARG_HAS_DEF  0x002 /* Argument has default value associated with it */` |
|         - | 1933 | `#define VM_FUNC_REF_RETURN   0x004 /* Return by reference */` |
|         - | 1934 | `#define VM_FUNC_CLASS_METHOD 0x008 /* VM function is in fact a class method */` |
|         - | 1935 | `#define VM_FUNC_CLOSURE      0x010 /* VM function is a closure */` |
|         - | 1936 | `#define VM_FUNC_ARG_IGNORE   0x020 /* Do not install argument in the current frame */` |
|         - | 1937 | `#define VM_FUNC_GENERATOR    0x040 /* VM function is a generator (contains yield) */` |
|         - | 1938 | `#define VM_FUNC_ARG_VARIADIC 0x080 /* Argument is variadic (...$args) */` |
|         - | 1939 | `#define VM_FUNC_ARG_NULLABLE 0x100 /* Argument type is nullable (?type or T\|null) */` |
|         - | 1940 | `#define VM_FUNC_ARG_UNION    0x200 /* Argument has a union type (use aUnionAlts) */` |
|         - | 1941 | `#define VM_FUNC_ARG_PROMOTED 0x400 /* Constructor promoted property (iPromoteVis holds visibility) */` |
|         - | 1942 | `#define VM_FUNC_ARG_READONLY 0x800 /* Promoted property is readonly (PHP 8.1) */` |
|         - | 1943 | `#define VM_FUNC_RETURN_NULLABLE 0x1000 /* Return type is nullable (?T, T\|null, A\|B\|null) — func-level */` |
|         - | 1944 | `#define VM_FUNC_INTERNAL     0x2000 /* Function was defined while compiling a builtin chunk` |
|         - | 1945 | `                                     * (embedded PHP library). Reflection reports it as internal:` |
|         - | 1946 | `                                     * isInternal() true, getFileName() false. */` |
|         - | 1947 | ``#define VM_FUNC_STATIC_CL    0x4000 /* Static closure/arrow fn (`static function () {}` /`` |
|         - | 1948 | ``                                     * `static fn () =>`): no $this auto-capture, bind refused. */`` |
|         - | 1949 | `#define VM_FUNC_ARG_PRIV_SET 0x8000  /* Promoted property is private(set) (PHP 8.4) */` |
|         - | 1950 | `#define VM_FUNC_ARG_PROT_SET 0x10000 /* Promoted property is protected(set) (PHP 8.4) */` |
|         - | 1951 | ``#define VM_FUNC_HOOK_SET_EXPR 0x20000 /* `set => expr` property hook (PHP 8.4): the dispatcher`` |
|         - | 1952 | `                                       * stores the implicit return value into the backing slot */` |
|         - | 1953 | `#define VM_FUNC_BOUND        0x40000 /* Bound by an UNCONDITIONAL top-level declaration; a second such` |
|         - | 1954 | `                                      * binding of the same name fatals ("Cannot redeclare function ..."),` |
|         - | 1955 | `                                      * matching PHP. Conditional declarations are not marked. */` |
|         - | 1956 | ``#define VM_FUNC_ARROW        0x80000 /* Arrow function (`fn()=>expr`): its aClosureEnv captures are ALL`` |
|         - | 1957 | ``                                       * implicit (auto-scanned from the body), never an explicit `use` `` |
|         - | 1958 | `                                      * clause. php does not warn about an undefined auto-capture at` |
|         - | 1959 | `                                      * closure creation — the read fires the warning inside the body —` |
|         - | 1960 | `                                      * so the OP_LOAD_CLOSURE undefined-capture warning is suppressed. */` |
|         - | 1961 | `#define VM_FUNC_NATIVE       0x100000 /* The body is a C routine, not bytecode: ph7_vm_func::pNative` |
|         - | 1962 | `                                       * holds it and aByteCode stays EMPTY. OP_CALL branches to the` |
|         - | 1963 | `                                       * host-function path (no frame, no call record, no operand` |
|         - | 1964 | `                                       * stack) while every step BEFORE the branch — the sVmName` |
|         - | 1965 | `                                       * lookup, $this/self resolution, visibility — runs unchanged,` |
|         - | 1966 | `                                       * so a native method inherits, overrides and dispatches like` |
|         - | 1967 | `                                       * any other. This is what lets a builtin class own its C code` |
|         - | 1968 | ``                                       * as a METHOD instead of a global `__prefix_verb` thunk. */`` |
|         - | 1969 | `#define VM_FUNC_NATIVE_STATIC 0x200000 /* A VM_FUNC_NATIVE method declared static. Staticness is` |
|         - | 1970 | `                                       * otherwise recorded only on ph7_class_method::iFlags, which` |
|         - | 1971 | `                                       * the OP_CALL dispatcher does not hold — and it must know,` |
|         - | 1972 | `                                       * because the method path falls back to the CALLER's $this` |
|         - | 1973 | `                                       * when the target slot carries a class name rather than an` |
|         - | 1974 | `                                       * object. For a bytecode method that fallback is harmless;` |
|         - | 1975 | `                                       * for a native one it would hand the body a receiver on a` |
|         - | 1976 | ``                                       * `C::m()` call and shift how it reads its arguments. Set by`` |
|         - | 1977 | `                                       * the native builder only: the compiler's behaviour for` |
|         - | 1978 | `                                       * bytecode methods is deliberately left untouched. */` |
|         - | 1979 | `#define VM_FUNC_NODISCARD 0x400000 /* php 8.5's #[\NoDiscard]: a caller that DROPS this` |
|         - | 1980 | `                                       * function's answer is warned at the call site. Set by` |
|         - | 1981 | `                                       * the compiler from the declared attribute, and by the` |
|         - | 1982 | `                                       * native builder for the internal members php marks` |
|         - | 1983 | `                                       * (PH7_VmFuncSetNoDiscard). The message, when there is` |
|         - | 1984 | `                                       * one, comes from zNoDiscard for a native member and` |
|         - | 1985 | `                                       * from the attribute's own argument for a compiled one. */` |
|         - | 1986 | ``#define VM_FUNC_ARG_FINAL 0x800000 /* PHP 8.4's `final` on a PROMOTED property. Kept apart from`` |
|         - | 1987 | `                                    * the class-body rule it mirrors: php refuses` |
|         - | 1988 | ``                                    * `final private $p` in a class body and ACCEPTS the same`` |
|         - | 1989 | `                                    * pair here (modifiers 36), so the screen cannot be shared. */` |
|         - | 1990 | `/* next free bit: 0x1000000 */` |
|         - | 1991 | `/*` |
|         - | 1992 | ` * Each user defined function is parsed out and stored in an instance` |
|         - | 1993 | ` * of the following structure.` |
|         - | 1994 | ` * PH7 introduced some powerfull extensions to the PHP 5 programming` |
|         - | 1995 | ` * language like function overloading, type hinting, complex default` |
|         - | 1996 | ` * arguments values and many more.` |
|         - | 1997 | ` * Please refer to the official documentation for more information.` |
|         - | 1998 | ` */` |
|         - | 1999 | `struct ph7_vm_func` |
|         - | 2000 | `{` |
|         - | 2001 | `	SySet aArgs;         /* Expected arguments (ph7_vm_func_arg instance) */` |
|         - | 2002 | `	SySet aStatic;       /* Static variable (ph7_vm_func_static_var instance) */` |
|         - | 2003 | `	SyString sName;      /* Function name */` |
|         - | 2004 | `	SySet aByteCode;     /* Compiled function body */` |
|         - | 2005 | `	SySet aClosureEnv;   /* Closure environment (ph7_vm_func_closure_env instace) */` |
|         - | 2006 | `	sxi32 iFlags;        /* VM function configuration */` |
|         - | 2007 | `	SyString sSignature; /* Function signature used to implement function overloading` |
|         - | 2008 | `						  * (Refer to the official docuemntation for more information` |
|         - | 2009 | `						  *  on this powerfull feature)` |
|         - | 2010 | `						  */` |
|         - | 2011 | `	sxu32 nReturnType;   /* Return type hint (MEMOBJ_* constant, MEMOBJ_VOID, or SXU32_HIGH for class) */` |
|         - | 2012 | `	SyString sReturnClass; /* Class name when nReturnType == SXU32_HIGH */` |
|         - | 2013 | `	SySet aReturnUnion;  /* Return-type union alternatives (ph7_type_alt). Empty unless union return. */` |
|         - | 2014 | `	SyString sReturnTypeName; /* Original return-type text for error messages, in canonical PHP order */` |
|         - | 2015 | `	sxu8 bStrictTypes;   /* 1 if defining file declared strict_types=1 (governs return-value coercion) */` |
|         - | 2016 | `	sxu16 nLocalName;    /* How many of this body's variable names VmNumberLocals gave a` |
|         - | 2017 | `	                      * number to, capped at PH7_VAR_SLOT_MAX. Meaningful only once` |
|         - | 2018 | `	                      * bNumbered is set; 0 with bNumbered set means the body names no` |
|         - | 2019 | `	                      * variable the compiler wrote down. */` |
|         - | 2020 | `	sxu8 bNumbered;      /* 1 = VmNumberLocals has walked this body. Lazily, on the first` |
|         - | 2021 | `	                      * activation, exactly like nMaxStack below and for the same` |
|         - | 2022 | `	                      * reason: a body that has not been walked yet just walks, where a` |
|         - | 2023 | `	                      * compile-time pass would have to answer for every path that can` |
|         - | 2024 | `	                      * build one. */` |
|         - | 2025 | `	sxu32 nMaxStack;     /* Cached operand-stack depth for this body (BYTECODE stage 7):` |
|         - | 2026 | `						  * 0 = not yet computed; otherwise the number of slots to allocate` |
|         - | 2027 | `						  * per call (a tight bound from VmComputeMaxStack, or the whole` |
|         - | 2028 | `						  * instruction count when the body is not statically modelable). */` |
|         - | 2029 | `	SySet aAttrs;        /* Declared #[...] attributes (ph7_attribute records) */` |
|         - | 2030 | `	SyString sDoc;       /* Doc-comment immediately preceding the declaration, delimiters` |
|         - | 2031 | `						  * included (duplicated into the VM allocator); nByte == 0 = none,` |
|         - | 2032 | `						  * Reflection getDocComment() then reports false. */` |
|         - | 2033 | `	SyString sFile;      /* Path of the defining file (aliases the VM-lifetime dup in pVm->aFiles).` |
|         - | 2034 | `						  * nByte == 0 when unknown (builtin chunk, eval, direct API compile):` |
|         - | 2035 | `						  * Reflection getFileName() then reports false. */` |
|         - | 2036 | `	sxu32 nLine;         /* Line of the 'function'/'fn' keyword (Reflection getStartLine) */` |
|         - | 2037 | `	sxu32 nEndLine;      /* Line of the closing brace of the body (Reflection getEndLine) */` |
|         - | 2038 | `	SyString sClosureName; /* A closure/arrow function's php-VISIBLE name, php 8.4's` |
|         - | 2039 | ``	                      * `{closure:SCOPE:LINE}` (Zend/zend_compile.c, zend_begin_func_decl).`` |
|         - | 2040 | `	                      * Built at COMPILE time — the scope part names the ENCLOSING` |
|         - | 2041 | `	                      * function, which only the compiler knows — and duplicated into the` |
|         - | 2042 | `	                      * VM allocator. sName stays the synthesized unique lookup key` |
|         - | 2043 | `	                      * ("[lambda_3]"); this is what __FUNCTION__, a backtrace, Reflection` |
|         - | 2044 | `	                      * and is_callable() report. nByte == 0 for anything but a closure. */` |
|         - | 2045 | ``	SyString sClosureScope;/* The class the closure was written inside, for the `C::` php prefixes`` |
|         - | 2046 | `	                        * its argument diagnostics with. Empty for a top-level one. */` |
|         - | 2047 | `	void *pUserData;     /* Upper layer private data associated with this instance */` |
|         - | 2048 | `	sxu8 bQueued;        /* VM_FUNC_CLOSURE only: already on the VM's pending-free list */` |
|         - | 2049 | `	sxi32 nRef;          /* VM_FUNC_CLOSURE only: how many things still need this` |
|         - | 2050 | `` 	                      * per-instantiation copy -- the Closure OBJECTS whose `$__fn` `` |
|         - | 2051 | `	                      * names it, plus every activation currently running it. At zero` |
|         - | 2052 | `	                      * it is unregistered from hFunction and freed. Every closure` |
|         - | 2053 | `	                      * expression evaluated used to mint one of these and leave it in` |
|         - | 2054 | `	                      * the function table for the life of the VM: ~3 KB per closure,` |
|         - | 2055 | `	                      * which on a real workload (phpcs) was over half the engine's` |
|         - | 2056 | `	                      * whole memory footprint. */` |
|         - | 2057 | `	void *pLsbClass;     /* For a closure: the late-static-binding class captured at its` |
|         - | 2058 | ``	                      * creation site (ph7_class*), so `static::` inside the body`` |
|         - | 2059 | `	                      * resolves like php. NULL for a plain function/method. */` |
|         - | 2060 | `	ph7_user_func *pNative; /* VM_FUNC_NATIVE only: the C body. A ph7_user_func rather than a` |
|         - | 2061 | `	                      * bespoke record because that struct ALREADY carries everything the` |
|         - | 2062 | `	                      * host-call path reads — xFunc, pUserData, sName, the min/max arity` |
|         - | 2063 | `	                      * bounds, zSig/zRet and nByRefMask — so the existing OP_CALL foreign` |
|         - | 2064 | `	                      * block, VmInitCallContext, ph7_context_user_data(), ph7_function_name()` |
|         - | 2065 | `	                      * and VmEnforceBuiltinArgTypes all work on it verbatim. It is NOT` |
|         - | 2066 | `	                      * registered in pVm->hHostFunction: it hangs off this method alone and` |
|         - | 2067 | `	                      * is reachable only through the method, never as a global name. */` |
|         - | 2068 | `	ph7_vm_func *pNextName; /* Next VM function with the same name as this one */` |
|         - | 2069 | `};` |
|         - | 2070 | `/* Forward reference */` |
|         - | 2071 | `typedef struct ph7_builtin_constant ph7_builtin_constant;` |
|         - | 2072 | `typedef struct ph7_builtin_func ph7_builtin_func;` |
|         - | 2073 | `/*` |
|         - | 2074 | ` * Each built-in foreign function (C function) is stored in an` |
|         - | 2075 | ` * instance of the following structure.` |
|         - | 2076 | ` * Please refer to the official documentation for more information` |
|         - | 2077 | ` * on how to create/install foreign functions.` |
|         - | 2078 | ` */` |
|         - | 2079 | `struct ph7_builtin_func` |
|         - | 2080 | `{` |
|         - | 2081 | `	const char *zName;        /* Function name [i.e: strlen(), rand(), array_merge(), etc.]*/` |
|         - | 2082 | `	ProchHostFunction xFunc;  /* C routine performing the computation */` |
|         - | 2083 | `};` |
|         - | 2084 | `/*` |
|         - | 2085 | ` * Each built-in foreign constant is stored in an instance` |
|         - | 2086 | ` * of the following structure.` |
|         - | 2087 | ` * Please refer to the official documentation for more information` |
|         - | 2088 | ` * on how to create/install foreign constants.` |
|         - | 2089 | ` */` |
|         - | 2090 | `struct ph7_builtin_constant` |
|         - | 2091 | `{` |
|         - | 2092 | `	const char *zName;     /* Constant name */` |
|         - | 2093 | `	ProcConstant xExpand;  /* C routine responsible of expanding constant value*/` |
|         - | 2094 | `};` |
|         - | 2095 | `/* Forward reference */` |
|         - | 2096 | `typedef struct ph7_class_method ph7_class_method;` |
|         - | 2097 | `typedef struct ph7_class_attr   ph7_class_attr;` |
|         - | 2098 | `/*` |
|         - | 2099 | ` * One subscript asked of a native class through ph7_class::xDim -- php's` |
|         - | 2100 | ` * read_dimension / has_dimension handlers, as one call.` |
|         - | 2101 | ` *` |
|         - | 2102 | ` * The hook answers by writing pResult (left NULL for a miss, which the ISSET` |
|         - | 2103 | ` * mode reads as "not set"), or REFUSES by naming an exception class in` |
|         - | 2104 | ` * zThrowClass and wording it in zThrowMsg. The refusal is carried back rather` |
|         - | 2105 | ` * than raised here because only the opcode knows how to route a throw out of a` |
|         - | 2106 | ` * mid-expression read, and because php's own two modes disagree about it: an` |
|         - | 2107 | `` * out-of-range `$map[-1]` is a ValueError to a READ (and to `??`, which reads)`` |
|         - | 2108 | `` * and a plain FALSE to `isset()`.`` |
|         - | 2109 | ` */` |
|         - | 2110 | `typedef struct PH7_NativeDimCtx PH7_NativeDimCtx;` |
|         - | 2111 | `#define PH7_NATIVE_DIM_READ  0 /* php's read_dimension: the value, or NULL for a miss */` |
|         - | 2112 | `#define PH7_NATIVE_DIM_ISSET 1 /* php's has_dimension: presence only, and never a refusal */` |
|         - | 2113 | `/*` |
|         - | 2114 | ` * The WRITE side. Two kinds of class arrive here.` |
|         - | 2115 | ` *` |
|         - | 2116 | ` * One answers reads and stores NOTHING, and may only REFUSE: php's` |
|         - | 2117 | ` * write_dimension and unset_dimension for such a container. The engine's own` |
|         - | 2118 | `` * sentence is `Cannot use object of type C as array`, and a class states its`` |
|         - | 2119 | `` * own here -- PDORow's three are `Cannot write to PDORow offset`, `Cannot`` |
|         - | 2120 | `` * append to PDORow offset` and `Cannot unset PDORow offset`. A refusal is`` |
|         - | 2121 | ` * asked with neither pOffset nor pResult (none of php's wordings names the` |
|         - | 2122 | ` * offset, and there is no answer to write): a hook that does not word one of` |
|         - | 2123 | ` * these must return without touching either.` |
|         - | 2124 | ` *` |
|         - | 2125 | `` * The other really STORES: `$x['a'] = '1'` on a SimpleXMLElement writes an`` |
|         - | 2126 | ` * attribute, and php's handler is a write_dimension like any other. Those` |
|         - | 2127 | ``  * three modes are asked a second way -- with pOffset (0 for the keyless `$o[]` `` |
|         - | 2128 | ` * spelling) and with pResult carrying the INCOMING VALUE -- and the hook says` |
|         - | 2129 | ` * it took the write by setting bStored. A hook that leaves bStored at 0 is the` |
|         - | 2130 | ` * first kind and the caller falls back to the refusal above, which is what` |
|         - | 2131 | ` * keeps DOMNodeList and PDORow answering exactly as they did.` |
|         - | 2132 | ` */` |
|         - | 2133 | `#define PH7_NATIVE_DIM_WRITE  2 /* php's write_dimension with a key */` |
|         - | 2134 | ``#define PH7_NATIVE_DIM_APPEND 3 /* ...and its keyless `$o[] = v` spelling */`` |
|         - | 2135 | `#define PH7_NATIVE_DIM_UNSET  4 /* php's unset_dimension */` |
|         - | 2136 | `/*` |
|         - | 2137 | `` * php's has_dimension asked the way `empty()` asks it -- a non-zero`` |
|         - | 2138 | `` * `check_empty`, which its handlers read as the EMPTINESS question rather than`` |
|         - | 2139 | ` * the null one. SimpleXMLElement is the one that answers it differently:` |
|         - | 2140 | `` * `empty($x['a'])` on `a="0"` is TRUE, judged on the ATTRIBUTE'S TEXT, where`` |
|         - | 2141 | ` * reading the same offset hands back a truthy object. A hook that has no such` |
|         - | 2142 | ` * distinction leaves pResult alone and the caller falls back to reading the` |
|         - | 2143 | ` * value and judging that, which is every other class's answer.` |
|         - | 2144 | ` */` |
|         - | 2145 | `#define PH7_NATIVE_DIM_NOTEMPTY 5` |
|         - | 2146 | `struct PH7_NativeDimCtx` |
|         - | 2147 | `{` |
|         - | 2148 | `	int iMode;               /* PH7_NATIVE_DIM_* */` |
|         - | 2149 | ``	ph7_value *pOffset;      /* The subscript. 0 for the keyless `$o[]` spelling. */`` |
|         - | 2150 | `	ph7_value *pResult;      /* READ: where the answer goes (the caller inits it NULL).` |
|         - | 2151 | `	                          * ISSET: set to a bool by the hook. */` |
|         - | 2152 | `	const char *zThrowClass; /* Set by the hook to refuse; 0 (the caller's init) means answered */` |
|         - | 2153 | `	char zThrowMsg[160];     /* ...and its message, formatted by the hook */` |
|         - | 2154 | `	int bStored;             /* WRITE/APPEND/UNSET only: the hook TOOK the write. 0 -- the` |
|         - | 2155 | `	                          * caller's init -- means it did not, and the access takes the` |
|         - | 2156 | ``	                          * `Cannot use object of type C as array` refusal (or the hook's`` |
|         - | 2157 | `	                          * own wording of it) instead. */` |
|         - | 2158 | `};` |
|         - | 2159 | `/*` |
|         - | 2160 | ` * One property WRITE asked of a native class through ph7_class::xSet -- php's` |
|         - | 2161 | ` * write_property handler.` |
|         - | 2162 | ` *` |
|         - | 2163 | ` * A native class whose properties are php's OWN C struct rather than real slots` |
|         - | 2164 | ` * states this: php converts the incoming value the way its struct field demands` |
|         - | 2165 | ``  * and stores THAT, so `$i->y = 1.5` reads back int(1) and `$i->f = 0.1234567` `` |
|         - | 2166 | ` * reads back 0.123456 (an int64 count of microseconds, shown divided). The hook` |
|         - | 2167 | ` * rewrites pValue IN PLACE to whatever must land in the slot -- it runs on every` |
|         - | 2168 | ` * write shape (a plain store, a compound assign, ++/--, a list() target, a` |
|         - | 2169 | ` * foreach target), because it hangs off the same store filter the typed-property` |
|         - | 2170 | ` * enforcement does.` |
|         - | 2171 | ` *` |
|         - | 2172 | ` * Refusing works the way the dimension hook's does: name an exception class in` |
|         - | 2173 | ` * zThrowClass and word it in zThrowMsg, and the filter raises it where the store` |
|         - | 2174 | ` * would have landed. A property php only lets a script write by CREATING a` |
|         - | 2175 | `` * deprecated dynamic one (DateInterval's `days`) is refused here, the scope policy.`` |
|         - | 2176 | ` */` |
|         - | 2177 | `typedef struct PH7_NativeSetCtx PH7_NativeSetCtx;` |
|         - | 2178 | `struct PH7_NativeSetCtx` |
|         - | 2179 | `{` |
|         - | 2180 | `	const SyString *pName;   /* The property being written */` |
|         - | 2181 | `	ph7_value *pValue;       /* The incoming value; the hook rewrites it in place */` |
|         - | 2182 | `	const char *zThrowClass; /* Set by the hook to refuse; 0 (the caller's init) means stored */` |
|         - | 2183 | `	char zThrowMsg[160];     /* ...and its message, formatted by the hook */` |
|         - | 2184 | `};` |
|         - | 2185 | `/*` |
|         - | 2186 | ` * One PROPERTY access asked of a native class through ph7_class::xProp --` |
|         - | 2187 | ` * php's read_property / has_property / write_property / unset_property` |
|         - | 2188 | ` * handlers, as one call told which is asking.` |
|         - | 2189 | ` *` |
|         - | 2190 | ` * This is the hook for a class whose properties are not storage at all: php's` |
|         - | 2191 | ` * PDORow answers every read from the statement's CURRENT row, so the object` |
|         - | 2192 | `` * holds no slot for any of them, `get_object_vars()` is EMPTY beside a read`` |
|         - | 2193 | ` * that works, and a write is a refusal rather than a store. It is asked only` |
|         - | 2194 | ` * where the instance has NO slot of that name, which for such a class is` |
|         - | 2195 | ` * everywhere -- a native class that keeps real slots and only CONVERTS what` |
|         - | 2196 | ` * lands in them wants ph7_class::xSet instead.` |
|         - | 2197 | ` *` |
|         - | 2198 | ` * READ answers by writing pResult (left NULL for a name the class does not` |
|         - | 2199 | `` * know, which is php's own answer -- not an `Undefined property` warning);`` |
|         - | 2200 | ` * ISSET and EXISTS answer by setting pResult to a bool. Either may DECLINE by leaving` |
|         - | 2201 | ` * bAnswered at 0, which puts the name back on the ordinary path. WRITE and` |
|         - | 2202 | ` * UNSET exist only to refuse, the way the dimension hook's write modes do.` |
|         - | 2203 | ` *` |
|         - | 2204 | ` * A refusal is carried back in zThrowClass/zThrowMsg rather than raised here,` |
|         - | 2205 | ` * exactly as PH7_NativeDimCtx's is: only the opcode knows how to route a throw` |
|         - | 2206 | ` * out of a mid-expression access.` |
|         - | 2207 | ` */` |
|         - | 2208 | `#define PH7_NATIVE_PROP_READ  0` |
|         - | 2209 | `#define PH7_NATIVE_PROP_ISSET 1` |
|         - | 2210 | `#define PH7_NATIVE_PROP_WRITE 2` |
|         - | 2211 | `#define PH7_NATIVE_PROP_UNSET 3` |
|         - | 2212 | `/*` |
|         - | 2213 | `` * php's has_property asked the way `empty()` asks it -- ZEND_PROPERTY_NOT_EMPTY,`` |
|         - | 2214 | `` * a non-zero `check_empty`, which its handlers read as the EMPTINESS question`` |
|         - | 2215 | ` * rather than the null one. It is the same handler and a different answer: a` |
|         - | 2216 | `` * PDORow column holding 0 or "" is `isset()` and is not this.`` |
|         - | 2217 | ` */` |
|         - | 2218 | `#define PH7_NATIVE_PROP_NOTEMPTY 4` |
|         - | 2219 | `/*` |
|         - | 2220 | ` * php's write_property, asked at the point the VALUE exists.` |
|         - | 2221 | ` *` |
|         - | 2222 | ` * The modes above are asked by the member opcode, which runs BEFORE the store` |
|         - | 2223 | ` * that carries the value -- enough for a class that only ever refuses a write` |
|         - | 2224 | ` * (PDORow), and not enough for one whose handler really stores (ext/dom's` |
|         - | 2225 | `` * `$el->nodeValue = 'x'`). STORE is the second half: pResult carries the`` |
|         - | 2226 | ` * incoming value, and the hook writes it or refuses. It is dispatched from the` |
|         - | 2227 | ` * one place every overloaded write funnels through, so a plain store, a` |
|         - | 2228 | `` * compound assign, a `??=` and Reflection's setValue() all reach it.`` |
|         - | 2229 | ` */` |
|         - | 2230 | `#define PH7_NATIVE_PROP_STORE  5` |
|         - | 2231 | `/*` |
|         - | 2232 | ` * php's has_property asked the third way -- ZEND_PROPERTY_EXISTS, which is what` |
|         - | 2233 | `` * `property_exists()` passes and nothing else does. A handler may answer it`` |
|         - | 2234 | ` * differently from the emptiness question, and ArrayObject's does: a storage key` |
|         - | 2235 | `` * holding 0 EXISTS and is not `empty()`-false, where PDORow's handler makes no`` |
|         - | 2236 | ` * distinction and answers both by truth.` |
|         - | 2237 | ` */` |
|         - | 2238 | `#define PH7_NATIVE_PROP_EXISTS 6` |
|         - | 2239 | `/*` |
|         - | 2240 | ` * "Would you take a WRITE of this name?", asked where the value does not exist` |
|         - | 2241 | ` * yet -- the member opcode's write shapes, which have to decide between the` |
|         - | 2242 | `` * handler, a magic `__set` and creating a property before the store runs.`` |
|         - | 2243 | ` *` |
|         - | 2244 | ` * Answering (bAnswered) means the write is the handler's and the rails route it` |
|         - | 2245 | ` * to STORE above; declining leaves the name on the ordinary path. It is the one` |
|         - | 2246 | ` * question a handler must answer without seeing a value, so it is about the NAME` |
|         - | 2247 | ` * and the object's state alone: ext/dom answers it from the class's virtual` |
|         - | 2248 | ` * declarations, ArrayObject from its ARRAY_AS_PROPS flag.` |
|         - | 2249 | ` */` |
|         - | 2250 | `#define PH7_NATIVE_PROP_OWNS   7` |
|         - | 2251 | `typedef struct PH7_NativePropCtx PH7_NativePropCtx;` |
|         - | 2252 | `struct PH7_NativePropCtx` |
|         - | 2253 | `{` |
|         - | 2254 | `	int iMode;               /* PH7_NATIVE_PROP_* */` |
|         - | 2255 | `	const SyString *pName;   /* The property being asked about */` |
|         - | 2256 | `	ph7_value *pResult;      /* READ: where the answer goes (the caller inits it NULL).` |
|         - | 2257 | `	                          * ISSET/NOTEMPTY/EXISTS: set to a bool by the hook.` |
|         - | 2258 | `	                          * STORE: the INCOMING value, which the hook stores. */` |
|         - | 2259 | `	int bAnswered;           /* Set by the hook when it OWNS this name; 0 (the caller's` |
|         - | 2260 | `	                          * init) leaves the access to the ordinary path */` |
|         - | 2261 | `	const char *zThrowClass; /* Set by the hook to refuse; 0 (the caller's init) means answered */` |
|         - | 2262 | `	char zThrowMsg[160];     /* ...and its message, formatted by the hook */` |
|         - | 2263 | `	sxi32 iThrowCode;        /* ...and its php $code. A DOM refusal is a DOMException whose` |
|         - | 2264 | `	                          * code a program reads (DOM_NOT_FOUND_ERR & co), so the number` |
|         - | 2265 | `	                          * has to survive the trip out to the site that raises. 0 -- the` |
|         - | 2266 | `	                          * caller's init -- is every other class's answer. */` |
|         - | 2267 | ``	int bQuiet;              /* READ only: this fetch is a LOOKUP (`$o->p ?? d`), php's third`` |
|         - | 2268 | `	                          * accessor level -- it takes the VALUE and says nothing about a` |
|         - | 2269 | `	                          * name that is not there, where a plain read reports it. */` |
|         - | 2270 | ``	int bWriteCtx;           /* READ only: this fetch is the BASE of a write -- `$o->p[0] = 1`,`` |
|         - | 2271 | ``	                          * `$o->p[] = 1`, a destructuring target -- so php asks`` |
|         - | 2272 | `	                          * get_property_ptr_ptr rather than read_property and a handler` |
|         - | 2273 | `	                          * that CAN hand out a real slot should create the name it is` |
|         - | 2274 | `	                          * missing. Set by the member opcode; ignored by a handler with` |
|         - | 2275 | `	                          * no slot to give. */` |
|         - | 2276 | `	sxu32 nSlot;             /* The memobj index the answer LIVES in, for a handler whose` |
|         - | 2277 | `	                          * property is a real element of something the object owns` |
|         - | 2278 | `	                          * (ArrayObject's storage). SXU32_HIGH -- the caller's init --` |
|         - | 2279 | `	                          * means the answer is a value and the access is not an lvalue,` |
|         - | 2280 | `	                          * which is every virtual property's answer. */` |
|         - | 2281 | `};` |
|         - | 2282 | `/*` |
|         - | 2283 | ` * One COMPARISON asked of a native class through ph7_class::xCmp -- php's` |
|         - | 2284 | ` * compare handler.` |
|         - | 2285 | ` *` |
|         - | 2286 | ` * php asks the LEFT operand's handler and takes whatever it answers, so the` |
|         - | 2287 | ` * handler decides for the pair: what the two objects are compared BY (a` |
|         - | 2288 | ` * DateTime is its instant, and neither its zone nor any property), whether the` |
|         - | 2289 | ` * right operand is even a partner it recognizes, and whether the pair is` |
|         - | 2290 | `` * comparable at all. Asked only for `==`/`<`/`<=>` and friends -- `===` is`` |
|         - | 2291 | ` * identity in php and never reaches a handler.` |
|         - | 2292 | ` *` |
|         - | 2293 | ` * The answer is an ordering in iResult. php's ZEND_UNCOMPARABLE is the value 1,` |
|         - | 2294 | ` * which the comparator already uses for every unordered pair (a NaN, two arrays` |
|         - | 2295 | `` * neither containing the other): the operator arms ask `<` from the other side`` |
|         - | 2296 | ` * rather than reading one side's sign, so 1 from BOTH directions leaves every` |
|         - | 2297 | `` * relational spelling false and `==` false, which is exactly what php answers`` |
|         - | 2298 | ` * for an uncomparable pair.` |
|         - | 2299 | ` *` |
|         - | 2300 | ` * A REFUSAL (php throws DateException out of the DateTimeZone handler) is` |
|         - | 2301 | ` * carried back in zThrowClass/zThrowMsg rather than raised here, the way the` |
|         - | 2302 | `` * dimension hook's is: PH7_MemObjCmp runs under `sort()` and `in_array()` as`` |
|         - | 2303 | ` * well as under an operator, and none of those has a throw boundary of its own.` |
|         - | 2304 | ` * The comparator records it on the VM (PH7_CmpRefusalRaise) and the sites that` |
|         - | 2305 | ` * CAN route a throw -- the comparison opcodes, the switch arm and the host-call` |
|         - | 2306 | ` * boundary -- raise it.` |
|         - | 2307 | ` */` |
|         - | 2308 | `typedef struct PH7_NativeCmpCtx PH7_NativeCmpCtx;` |
|         - | 2309 | `struct PH7_NativeCmpCtx` |
|         - | 2310 | `{` |
|         - | 2311 | `	ph7_class_instance *pOther; /* The RIGHT operand as an INSTANCE, or 0 when it is a scalar */` |
|         - | 2312 | `	ph7_value *pOtherValue;     /* ...and the scalar itself, for the object-versus-value door` |
|         - | 2313 | ``	                             * (php asks the same handler for `$n == 2`). 0 when pOther is set. */`` |
|         - | 2314 | `	int bReversed;              /* The instance is the RIGHT operand: the hook owes the` |
|         - | 2315 | `	                             * answer already flipped, EXCEPT for the uncomparable 1,` |
|         - | 2316 | `	                             * which php answers from both directions alike. */` |
|         - | 2317 | `	int bAnswered;              /* Set by the hook when it RECOGNIZED the partner. The scalar` |
|         - | 2318 | `	                             * door falls back to php's cast rule when it did not; the` |
|         - | 2319 | `	                             * instance door keeps its older "always decided" contract. */` |
|         - | 2320 | `	sxi32 iResult;              /* -1 / 0 / 1; 1 is also php's ZEND_UNCOMPARABLE.` |
|         - | 2321 | `	                             * The caller inits it to 1, so a hook that` |
|         - | 2322 | `	                             * recognizes nothing may simply return. */` |
|         - | 2323 | `	const char *zThrowClass;    /* Set by the hook to refuse; 0 (the caller's init) means answered */` |
|         - | 2324 | `	char zThrowMsg[160];        /* ...and its message, formatted by the hook */` |
|         - | 2325 | `};` |
|         - | 2326 | `/*` |
|         - | 2327 | ` * One ARITHMETIC operator asked of a native class through ph7_class::xArith --` |
|         - | 2328 | `` * php's do_operation handler, which is what makes `$a + $b` mean something for`` |
|         - | 2329 | ` * an object.` |
|         - | 2330 | ` *` |
|         - | 2331 | ` * php asks the LEFT operand's handler first and the RIGHT one's when the left` |
|         - | 2332 | ` * has none, so the handler sees a pair it may be either half of and decides for` |
|         - | 2333 | ` * both: what the other operand is allowed to be, how it converts, and what the` |
|         - | 2334 | ` * answer is. Declining (leaving bHandled at 0) puts the pair back on the` |
|         - | 2335 | `` * ordinary numeric path, where an object is `Unsupported operand types`.`` |
|         - | 2336 | ` *` |
|         - | 2337 | ` * A REFUSAL is carried back rather than raised here, the way the dimension and` |
|         - | 2338 | ` * compare hooks' are: the opcode owns the operand stack and has to settle it` |
|         - | 2339 | ` * before any throw, and the exception CLASS varies -- BcMath\Number answers` |
|         - | 2340 | ` * ValueError for a string that is not a number and DivisionByZeroError for a` |
|         - | 2341 | ` * zero divisor, neither of which is the TypeError the ordinary path raises.` |
|         - | 2342 | ` */` |
|         - | 2343 | `typedef struct PH7_NativeArithCtx PH7_NativeArithCtx;` |
|         - | 2344 | `struct PH7_NativeArithCtx` |
|         - | 2345 | `{` |
|         - | 2346 | `	const char *zOp;         /* "+", "-", "*", "/", "%" or "**" */` |
|         - | 2347 | `	ph7_value *pLeft;        /* The two operands, in SOURCE order */` |
|         - | 2348 | `	ph7_value *pRight;` |
|         - | 2349 | `	ph7_value *pResult;      /* Where the handler writes the answer */` |
|         - | 2350 | `	int bHandled;            /* Set by the hook to claim the pair */` |
|         - | 2351 | `	const char *zThrowClass; /* ...or set this to refuse; 0 means no refusal */` |
|         - | 2352 | `	char zThrowMsg[160];     /* ...and word it here */` |
|         - | 2353 | `};` |
|         - | 2354 | `/*` |
|         - | 2355 | ` * Each class is parsed out and stored in an instance of the following structure.` |
|         - | 2356 | ` * PH7 introduced powerfull extensions to the PHP 5 OO subsystems.` |
|         - | 2357 | ` * Please refer to the official documentation for more information.` |
|         - | 2358 | ` */` |
|         - | 2359 | `struct ph7_class` |
|         - | 2360 | `{` |
|         - | 2361 | `	ph7_class *pBase;     /* Base class if any */` |
|         - | 2362 | `	SyHash hDerived;      /* Derived [child] classes */` |
|         - | 2363 | `	SyString sName;       /* Class full qualified name */` |
|         - | 2364 | `	SyString sDisp;       /* The name a MESSAGE shows, aliasing sName's buffer. The two differ` |
|         - | 2365 | `	                       * for an anonymous class alone: php names one` |
|         - | 2366 | ``	                       * `<parent-or-interface-or-"class">@anonymous` + a NUL byte +`` |
|         - | 2367 | ``	                       * `file:line$hex`, and gets the short form everywhere for free`` |
|         - | 2368 | ``	                       * because every diagnostic prints a class name with `%s`, which`` |
|         - | 2369 | ``	                       * stops at the NUL. PHL prints names with the length-counted `%z`,`` |
|         - | 2370 | ``	                       * so the truncation has to be a field: `sName` is the identity (the`` |
|         - | 2371 | `	                       * hash key, and what get_class()/::class/Reflection::getName() hand` |
|         - | 2372 | ``	                       * back), `sDisp` is what var_dump, print_r, get_debug_type and every`` |
|         - | 2373 | `	                       * diagnostic show. For every other class they are the same bytes and` |
|         - | 2374 | `	                       * the same length. */` |
|         - | 2375 | `	sxi32 iFlags;         /* Class configuration flags [i.e: final, interface, abstract, etc.]  */` |
|         - | 2376 | `	sxu64 nShadowName;    /* One bit per PLAIN name this class holds a MANGLED slot for` |
|         - | 2377 | `	                       * (OoShadowNameBit). PH7_CLASS_SHADOW_PROP says the class has at` |
|         - | 2378 | `	                       * least one; this says WHICH, cheaply enough to ask on every` |
|         - | 2379 | `	                       * property access. Zero when the flag is clear. */` |
|         - | 2380 | `	sxu64 nPrivName;      /* ...and one bit per plain name this class declares as a PRIVATE` |
|         - | 2381 | `	                       * instance property of its own (its trait-composed ones included).` |
|         - | 2382 | `	                       * The other half of the same screen: a scope can only mean a` |
|         - | 2383 | `	                       * mangled slot for a name it declares private itself. */` |
|         - | 2384 | `	SyHash hAttr;         /* Class PROPERTIES [static + instance]. Constants live in hConst */` |
|         - | 2385 | `	SyHash hConst;        /* Class CONSTANTS [incl. enum cases] — php keeps constants and` |
|         - | 2386 | `` 	                       * properties in SEPARATE namespaces, so `const C` and `public $C` `` |
|         - | 2387 | `	                       * coexist. Keyed by name, disjoint from hAttr. */` |
|         - | 2388 | `	SyHash hMethod;       /* Class methods */` |
|         - | 2389 | `	sxu32 nLine;          /* Line number on which this class was declared */` |
|         - | 2390 | `	SySet aInterface;     /* Implemented interface container */` |
|         - | 2391 | `	SySet aTrait;         /* Used trait container */` |
|         - | 2392 | `	ph7_class *pNextName; /* Next class [interface, abstract, etc.] with the same name */` |
|         - | 2393 | `	int bMounted;         /* TRUE if class has been mounted (internal VM state) */` |
|         - | 2394 | `	SyString sFile;       /* Path of the defining file (aliases the VM-lifetime dup in pVm->aFiles).` |
|         - | 2395 | `	                       * nByte == 0 when unknown: Reflection getFileName() reports false. */` |
|         - | 2396 | `	sxu32 nEndLine;       /* Line of the class body's closing brace (Reflection getEndLine) */` |
|         - | 2397 | `	SyString sDoc;        /* Doc-comment preceding the declaration (duplicated; empty = none) */` |
|         - | 2398 | `	SySet aAttrs;         /* Declared #[...] attributes (ph7_attribute records) */` |
|         - | 2399 | `	sxu32 nEnumBacking;   /* Enum backing type: 0 = pure/not an enum, MEMOBJ_INT or MEMOBJ_STRING */` |
|         - | 2400 | `	SySet aEnumCases;     /* Enum cases (ph7_class_attr *) in declaration order. Case singletons` |
|         - | 2401 | `	                       * materialize lazily and INDIVIDUALLY on first access (php 8.1: a broken` |
|         - | 2402 | `	                       * sibling case does not poison a valid one); an unmaterialized case has` |
|         - | 2403 | `	                       * nIdx == SXU32_HIGH. */` |
|         - | 2404 | `	void (*xNew)(ph7_vm *,ph7_class_instance *); /* php's create_object handler, run once the` |
|         - | 2405 | `	                       * instance frame exists and before any constructor. A native class whose` |
|         - | 2406 | `	                       * php counterpart answers its declared properties through a READ handler` |
|         - | 2407 | ``	                       * uses it to SEED those slots: php's ZipArchive declares `public int`` |
|         - | 2408 | ``	                       * $numFiles;` with no default and still shows 0 on a fresh object, because`` |
|         - | 2409 | `	                       * the handler answers rather than the slot. Seeding is the same fact from` |
|         - | 2410 | `	                       * the other side and keeps Reflection honest -- hasDefaultValue() stays` |
|         - | 2411 | `	                       * false, because there is no default, only a starting value. Resolved` |
|         - | 2412 | `	                       * through the ANCESTORS exactly as xRelease is. */` |
|         - | 2413 | `	void (*xRelease)(ph7_vm *,ph7_class_instance *); /* Native teardown for an instance of this class,` |
|         - | 2414 | `	                       * run by PH7_ClassInstanceRelease while the instance's slots are still` |
|         - | 2415 | `	                       * readable. This is NOT __destruct: php's WeakReference declares no` |
|         - | 2416 | `	                       * destructor, so a native class that must release a C-side resource` |
|         - | 2417 | `	                       * states it here instead of growing a method Reflection would report. */` |
|         - | 2418 | `	const PH7_NativeIterVtab *pIterVtab; /* How an InternalIterator walks an instance of this class` |
|         - | 2419 | `	                       * (php's get_iterator handler). Set on native IteratorAggregates whose` |
|         - | 2420 | `	                       * getIterator() answers PH7_NativeIteratorNew(); 0 everywhere else. */` |
|         - | 2421 | `	sxi32 (*xPresent)(ph7_vm *,ph7_class_instance *,ph7_value *,int); /* php's get_properties /` |
|         - | 2422 | `	                       * get_debug_info handlers, as one callback told which is asking:` |
|         - | 2423 | `	                       * the SHAPE a class SHOWS, which for several native classes is nothing` |
|         - | 2424 | `	                       * like the engine state it keeps. php presents a DateTime as` |
|         - | 2425 | `	                       * date/timezone_type/timezone and a WeakReference as ["object"], while` |
|         - | 2426 | `	                       * the slots underneath are a timestamp and a C-side cell — those slots` |
|         - | 2427 | `	                       * carry PH7_MOD_HIDDEN, and this fills an ARRAY with what php shows.` |
|         - | 2428 | `	                       * The last argument is 1 for the DEBUG surfaces (var_dump/print_r) and 0` |
|         - | 2429 | `	                       * for the property ones (var_export, the (array) cast), because php's` |
|         - | 2430 | `	                       * two handlers do not agree: a WeakReference shows ["object"] to` |
|         - | 2431 | `	                       * var_dump and NOTHING to (array), while a DateTime shows the same three` |
|         - | 2432 | `	                       * keys to both. Never consulted by get_object_vars()/foreach, which php` |
|         - | 2433 | `	                       * answers from the real (scoped) properties, nor yet by serialize(),` |
|         - | 2434 | `	                       * where php's answer is an __serialize/__unserialize pair. */` |
|         - | 2435 | `	const char *zNewRefusalClass; /* ...and the exception CLASS that refusal is, when it is` |
|         - | 2436 | ``	                       * not the usual `Error`: php's PDORow refuses `new` with a`` |
|         - | 2437 | `	                       * PDOException. 0 selects Error. */` |
|         - | 2438 | `	const char *zNewRefusal; /* php's create_object refusal TEXT for a PH7_CLASS_NOINSTANTIATE` |
|         - | 2439 | `	                       * class, when it is not the usual "Instantiation of class %s is not` |
|         - | 2440 | `	                       * allowed". php words Directory's as "Cannot directly construct` |
|         - | 2441 | `	                       * Directory, use dir() instead"; 0 selects the standard sentence. */` |
|         - | 2442 | `	void (*xClone)(ph7_vm *,ph7_class_instance *,ph7_class_instance *); /* php's clone_obj handler` |
|         - | 2443 | ``	                       * analogue: what `clone $o` DOES for an instance beyond the slot-by-slot`` |
|         - | 2444 | `	                       * copy, run on (clone, source) after the copy and before any __clone().` |
|         - | 2445 | `	                       * A DOM node's copy must be a copy of the NODE, not a second object over` |
|         - | 2446 | `	                       * the same one -- without this, a mutation through either object writes` |
|         - | 2447 | `	                       * the other. This is NOT __clone: php declares no such method on these` |
|         - | 2448 | `	                       * classes, so Reflection must not report one. Inherited by user` |
|         - | 2449 | `	                       * subclasses (the nearest ancestor's hook runs), which is php's handler` |
|         - | 2450 | `	                       * inheritance. 0 everywhere else. PH7_NativeClassSpec has no field for` |
|         - | 2451 | `	                       * it (a 14th field would touch every row of every spec table under` |
|         - | 2452 | `	                       * -Werror=missing-field-initializers); the owning installer assigns it` |
|         - | 2453 | `	                       * on the mounted class right after PH7_InstallNativeClasses. */` |
|         - | 2454 | `	void (*xDim)(ph7_vm *,ph7_class_instance *,PH7_NativeDimCtx *); /* php's read_dimension /` |
|         - | 2455 | `	                       * has_dimension handlers, as one callback told which is asking.` |
|         - | 2456 | ``	                       * A class states this when `$o[$k]` MEANS something and the class`` |
|         - | 2457 | `	                       * does not implement ArrayAccess -- php 8.3 gave DOMNodeList and` |
|         - | 2458 | `	                       * DOMNamedNodeMap dimension handlers WITHOUT declaring the` |
|         - | 2459 | ``	                       * interface, so `$list[0]` reads there while`` |
|         - | 2460 | ``	                       * `$list instanceof ArrayAccess` is false. No spec field can say`` |
|         - | 2461 | `	                       * that: the interface list is what a class DECLARES, and this is a` |
|         - | 2462 | `	                       * handler underneath it. Assigned on the mounted class by the` |
|         - | 2463 | `	                       * owning installer, like xClone, and inherited by user subclasses` |
|         - | 2464 | `	                       * (the nearest ancestor's hook runs) -- php's handler inheritance,` |
|         - | 2465 | `	                       * which is why a subclass's own offsetGet is NOT consulted for a` |
|         - | 2466 | `	                       * read even when it declares ArrayAccess. The WRITE half stays` |
|         - | 2467 | `	                       * php's: a store, an append and an unset are all` |
|         - | 2468 | ``	                       * `Cannot use object of type C as array` unless the class really`` |
|         - | 2469 | `	                       * implements ArrayAccess. 0 everywhere else. */` |
|         - | 2470 | `	void (*xSet)(ph7_vm *,ph7_class_instance *,PH7_NativeSetCtx *); /* php's write_property` |
|         - | 2471 | `	                       * handler: what a WRITE to one of this class's declared properties` |
|         - | 2472 | `	                       * converts to (or refuses), for a class whose properties are php's` |
|         - | 2473 | `	                       * own C struct. Reached from the store filter through the slot` |
|         - | 2474 | `	                       * table, so every write shape goes through it. Assigned on the` |
|         - | 2475 | `	                       * mounted class by the owning installer, like xClone and xDim,` |
|         - | 2476 | `	                       * which also flags the class's properties PH7_CLASS_ATTR_NATIVE_SET` |
|         - | 2477 | `	                       * so their slots get registered; inherited by user subclasses the` |
|         - | 2478 | `	                       * way php inherits a handler. 0 everywhere else. */` |
|         - | 2479 | `	void (*xProp)(ph7_vm *,ph7_class_instance *,PH7_NativePropCtx *); /* php's` |
|         - | 2480 | `	                       * read_property / has_property / write_property /` |
|         - | 2481 | `	                       * unset_property handlers, as one callback told which is` |
|         - | 2482 | `	                       * asking; see PH7_NativePropCtx. Consulted only where the` |
|         - | 2483 | `	                       * instance has no slot of that name. Assigned on the mounted` |
|         - | 2484 | `	                       * class by the owning installer, like xClone/xDim/xSet, and` |
|         - | 2485 | `	                       * inherited by user subclasses -- php's handler inheritance.` |
|         - | 2486 | `	                       * 0 everywhere else. */` |
|         - | 2487 | `	int (*xBool)(ph7_vm *,ph7_class_instance *); /* php's cast_object for _IS_BOOL: an` |
|         - | 2488 | `	                       * object is ALWAYS truthy unless its class says otherwise, and` |
|         - | 2489 | `	                       * BcMath\Number is the one that does -- a zero Number is falsy. */` |
|         - | 2490 | `	void (*xArith)(ph7_vm *,ph7_class_instance *,PH7_NativeArithCtx *); /* php's do_operation` |
|         - | 2491 | `	                       * handler; see PH7_NativeArithCtx. 0 for every class that has none,` |
|         - | 2492 | `	                       * which is all of them but BcMath\Number. */` |
|         - | 2493 | `	void (*xCmp)(ph7_vm *,ph7_class_instance *,PH7_NativeCmpCtx *); /* php's compare handler:` |
|         - | 2494 | ``	                       * what `==`, `<` and `<=>` MEAN for an instance of this class,`` |
|         - | 2495 | `	                       * asked instead of the property-by-property walk. php gives one` |
|         - | 2496 | `	                       * to the three date classes whose state is not their properties --` |
|         - | 2497 | `	                       * a DateTime compares as an INSTANT across DateTime and` |
|         - | 2498 | `	                       * DateTimeImmutable alike, two DateIntervals are never comparable,` |
|         - | 2499 | `	                       * two DateTimeZones of different kinds are a refusal. Asked of the` |
|         - | 2500 | `	                       * LEFT operand only, before the same-class screen and after the` |
|         - | 2501 | ``	                       * identity shortcut, and never for `===`. Assigned on the mounted`` |
|         - | 2502 | `	                       * class by the owning installer, like xClone/xDim/xSet, and` |
|         - | 2503 | `	                       * inherited by user subclasses (php's handler inheritance: a` |
|         - | 2504 | `	                       * subclass of DateTime still compares as an instant, extra` |
|         - | 2505 | `	                       * properties and all). 0 everywhere else. */` |
|         - | 2506 | `};` |
|         - | 2507 | `/* Class configuration flags */` |
|         - | 2508 | `#define PH7_CLASS_FINAL       0x001 /* Class is final [cannot be extended] */` |
|         - | 2509 | `#define PH7_CLASS_INTERFACE   0x002 /* Class is interface */` |
|         - | 2510 | `#define PH7_CLASS_ABSTRACT    0x004 /* Class is abstract */` |
|         - | 2511 | `#define PH7_CLASS_TRAIT       0x008 /* Class is a trait */` |
|         - | 2512 | `#define PH7_CLASS_TRAIT_VISITING 0x010 /* Trait is currently being applied (cycle detection) */` |
|         - | 2513 | `#define PH7_CLASS_READONLY    0x020 /* Class is readonly (PHP 8.2): every declared property is readonly */` |
|         - | 2514 | `#define PH7_CLASS_INTERNAL    0x040 /* Class was defined while compiling a builtin chunk (embedded PHP` |
|         - | 2515 | `                                     * library). Reflection reports it as internal: isInternal() true,` |
|         - | 2516 | `                                     * getFileName() false. */` |
|         - | 2517 | `#define PH7_CLASS_ENUM        0x080 /* Class is an enum (PHP 8.1). Also carries PH7_CLASS_FINAL. */` |
|         - | 2518 | `#define PH7_CLASS_STATIC_DEFER 0x200 /* This class's static table is not fully materialized: at least` |
|         - | 2519 | `                                      * one static property's default THREW when it was evaluated at` |
|         - | 2520 | `                                      * mount (PH7_CLASS_ATTR_STATIC_DEFER on the attribute) or failed` |
|         - | 2521 | `                                      * its type check (VM_CLASS_ATTR_TYPE_DEFER on the slot). A hint` |
|         - | 2522 | `                                      * only: the access/instantiation sites call` |
|         - | 2523 | `                                      * PH7_VmMaterializeClassStatics, which re-scans the whole base` |
|         - | 2524 | `                                      * chain. Set on the class whose mount saw the failure; the gate` |
|         - | 2525 | `                                      * (VmClassStaticDeferPending) walks the bases, so mount ORDER` |
|         - | 2526 | `                                      * between a base and its subclass does not matter. */` |
|         - | 2527 | `#define PH7_CLASS_LINT_UNBOUND 0x400000 /* Syntax-check compile (phl -l) only: a parent, interface or` |
|         - | 2528 | `                                        * trait this declaration names could not be resolved, and the` |
|         - | 2529 | `                                        * mode carried on with the body rather than refusing. Every` |
|         - | 2530 | `                                        * check that needs the missing member's contents -- #[\Override],` |
|         - | 2531 | `                                        * the unimplemented-abstract count -- is then skipped, which is` |
|         - | 2532 | `                                        * what php does: it reports those only for a class it could` |
|         - | 2533 | `                                        * EARLY-BIND, and it binds nothing whose base it cannot see. */` |
|         - | 2534 | `#define PH7_CLASS_TOPLEVEL    0x200000 /* Declared UNCONDITIONALLY at file top level. php runs such a` |
|         - | 2535 | `                                     * declaration whatever else is in the file, so two of them under one` |
|         - | 2536 | `                                     * name is a redeclaration even when neither was early-bound -- which` |
|         - | 2537 | `                                     * is the difference between this flag and PH7_CLASS_BOUND below. */` |
|         - | 2538 | `#define PH7_CLASS_BOUND       0x100 /* Bound by an UNCONDITIONAL top-level declaration. PHP fatals on a` |
|         - | 2539 | `                                     * second such binding of the same name ("Cannot redeclare ..."); a` |
|         - | 2540 | `                                     * conditional (if/loop/func-nested) declaration is NOT marked, so the` |
|         - | 2541 | ``                                     * `if(false){class C{}}` / `if(!class_exists){..}` guard idioms hoist. */`` |
|         - | 2542 | ``#define PH7_CLASS_NOCLONE     0x400 /* `clone $o` is a catchable Error for this class. A native class whose`` |
|         - | 2543 | `                                     * instances own a C-side resource keyed by a private slot cannot be` |
|         - | 2544 | `                                     * copied slot-by-slot (WeakReference's shared cell would be dropped` |
|         - | 2545 | `                                     * twice), which is exactly why php makes those classes uncloneable.` |
|         - | 2546 | `                                     * Enum cases carry the same rule through PH7_CLASS_ENUM, and` |
|         - | 2547 | `                                     * Generator/Fiber are named directly at the OP_CLONE test. */` |
|         - | 2548 | `#define PH7_CLASS_NOSERIALIZE 0x800 /* serialize() of an instance is a catchable Exception naming the` |
|         - | 2549 | `                                     * class, php's answer for every class holding engine state.` |
|         - | 2550 | `                                     * Without it the default object path emits the private slots —` |
|         - | 2551 | `                                     * for these classes a raw POINTER, which unserialize() would` |
|         - | 2552 | `                                     * hand straight back to a method. php's ZEND_ACC_NOT_SERIALIZABLE:` |
|         - | 2553 | `                                     * tested FIRST and unconditionally, so a subclass declaring` |
|         - | 2554 | `                                     * __serialize() is refused too (DOMXPath is the case that shows` |
|         - | 2555 | `                                     * it). INHERITED — the serializer walks pBase, because php's flag` |
|         - | 2556 | `                                     * rides down to every user subclass. */` |
|         - | 2557 | ``#define PH7_CLASS_NOSERIALIZE_SUBOK 0x2000 /* The SOFT refusal: php's `ce->serialize` deny HANDLER,`` |
|         - | 2558 | `                                     * which the serializer consults only AFTER looking for` |
|         - | 2559 | `                                     * __serialize()/__sleep() — so a SUBCLASS that declares either` |
|         - | 2560 | `                                     * one serializes normally, and php says so in the sentence` |
|         - | 2561 | `                                     * ("…is not allowed, unless serialization methods are` |
|         - | 2562 | `                                     * implemented in a subclass"). The DOM node classes are the` |
|         - | 2563 | `                                     * users; __wakeup() alone does NOT rescue them. Inherited the` |
|         - | 2564 | `                                     * same way as the hard flag. */` |
|         - | 2565 | ``#define PH7_CLASS_DIM_WRITABLE 0x4000 /* A write through `$obj[k]` LANDS on this class's storage.`` |
|         - | 2566 | `                                       * php's split is the read_dimension handler: an internal` |
|         - | 2567 | `                                       * class whose own handler hands back the real element` |
|         - | 2568 | `                                       * (ArrayObject, ArrayIterator, WeakMap) supports indirect` |
|         - | 2569 | `                                       * modification, while everything routed through` |
|         - | 2570 | `                                       * zend_std_read_dimension — every userland ArrayAccess, and` |
|         - | 2571 | `                                       * the SPL classes that keep the standard handler — gets a` |
|         - | 2572 | `                                       * TEMPORARY, so php notices and drops the write. Inherited` |
|         - | 2573 | `                                       * by subclasses (the flag is looked up along pBase), but` |
|         - | 2574 | `                                       * only while the native offsetGet is still the one that` |
|         - | 2575 | `                                       * answers: an override takes the class off the fast handler` |
|         - | 2576 | `                                       * in php too. See PH7_VmDimFetchWritable. */` |
|         - | 2577 | `/*` |
|         - | 2578 | `` * ph7_class::iFlags bit: `(int)` on an instance of this class answers the OBJECT`` |
|         - | 2579 | `` * HANDLE, silently, instead of php's `Object of class X could not be converted to`` |
|         - | 2580 | `` * int` warning and its 1. php gives exactly two classes that cast_object -- the`` |
|         - | 2581 | ` * curl easy and multi handles -- and the reason is stated in its own source: both` |
|         - | 2582 | `` * used to be RESOURCES, whose `(int)` was the resource id, and a program that keyed`` |
|         - | 2583 | ` * a table by it had to keep working. Composer's CurlDownloader is that program.` |
|         - | 2584 | `` * `(float)`, `(string)` and every other cast stay php's refusal, and so does the`` |
|         - | 2585 | ` * COMPARISON, which is a different handler (see PH7_NativeCmpOpaqueHandle).` |
|         - | 2586 | ` */` |
|         - | 2587 | `#define PH7_CLASS_HANDLE_ID   0x40000` |
|         - | 2588 | `/*` |
|         - | 2589 | `` * ph7_class::iFlags bit: `get_object_vars()` on an instance of this class is`` |
|         - | 2590 | ` * answered by the class's ph7_class::xPresent table rather than by its real` |
|         - | 2591 | ` * slots.` |
|         - | 2592 | ` *` |
|         - | 2593 | ` * php's get_properties handler is asked for three PURPOSES, and its native` |
|         - | 2594 | ` * classes mostly disagree between them -- a DateTime shows its three keys to` |
|         - | 2595 | `` * var_dump and to `(array)` and NOTHING to get_object_vars, a DOM node shows a`` |
|         - | 2596 | ` * table to var_dump and nothing to either of the others. SimpleXMLElement is` |
|         - | 2597 | ` * the one that answers all three the same way, so the third purpose is a` |
|         - | 2598 | ` * per-class opt-in instead of a fourth argument every handler would have to` |
|         - | 2599 | ` * learn.` |
|         - | 2600 | ` */` |
|         - | 2601 | `#define PH7_CLASS_VARS_PRESENT 0x80000` |
|         - | 2602 | `/*` |
|         - | 2603 | `` * ph7_class::iFlags bit: `(int)`, `(float)` and every numeric COERCION of an`` |
|         - | 2604 | ` * instance of this class run through the class's string form, silently, instead` |
|         - | 2605 | `` * of php's `Object of class X could not be converted to int` warning and its 1.`` |
|         - | 2606 | ` *` |
|         - | 2607 | `` * php's SimpleXMLElement is the one that does it: `(int)$xml->count` is the`` |
|         - | 2608 | `` * number the element CONTAINS, and `$xml->n + 1` adds to it, because its`` |
|         - | 2609 | ``  * cast_object answers IS_LONG and IS_DOUBLE from the node's text. Its `(bool)` `` |
|         - | 2610 | `` * is a different question again (ph7_class::xBool), and `(string)` is the`` |
|         - | 2611 | ` * ordinary __toString().` |
|         - | 2612 | ` */` |
|         - | 2613 | `#define PH7_CLASS_NUM_AS_STRING 0x100000` |
|         - | 2614 | ``#define PH7_CLASS_ANON        0x20000 /* Declared by `new class {...}`. php has no NAME to put in a`` |
|         - | 2615 | `                                    * type text for it while its body compiles, which is why` |
|         - | 2616 | ``                                    * `self` inside one may not be part of an intersection`` |
|         - | 2617 | `                                    * (see the scope-keyword screen in the type parser). */` |
|         - | 2618 | `#define PH7_CLASS_SHADOW_PROP 0x10000 /* At least one property of this class is filed under php's` |
|         - | 2619 | `                                       * MANGLED storage name -- a base's PRIVATE instance property,` |
|         - | 2620 | `                                       * carried down so the subclass's objects still hold its slot` |
|         - | 2621 | `                                       * (PH7_ClassAttrStorageName). The only way two slots of one` |
|         - | 2622 | `                                       * object can share a plain NAME, which is what the by-name` |
|         - | 2623 | `                                       * presentation surfaces have to de-duplicate. */` |
|         - | 2624 | `#define PH7_CLASS_LAZY_ATTR    0x8000 /* This class declares at least one PH7_CLASS_ATTR_NATIVE_LAZY` |
|         - | 2625 | `                                       * property. The O(1) gate in front of the materialization walk:` |
|         - | 2626 | `                                       * every native class writes its slots through the same setters,` |
|         - | 2627 | `                                       * and only these two classes have anything to install. */` |
|         - | 2628 | ``#define PH7_CLASS_NOINSTANTIATE 0x1000 /* `new C` is refused by the OBJECT-CREATION step, before the`` |
|         - | 2629 | `                                     * constructor's visibility is ever consulted — php's` |
|         - | 2630 | `                                     * "Instantiation of class %s is not allowed", which its` |
|         - | 2631 | `                                     * create_object handler raises. The distinction is visible:` |
|         - | 2632 | `                                     * Closure's __construct is PRIVATE (Reflection prints it that` |
|         - | 2633 | ``                                     * way), so without this flag `new Closure` reports a visibility`` |
|         - | 2634 | `                                     * refusal ("Call to private Closure::__construct() from global` |
|         - | 2635 | `                                     * scope") where php reports the instantiation one. A class that` |
|         - | 2636 | `                                     * merely wants a private ctor does NOT want this bit. */` |
|         - | 2637 | `/* Class attribute/methods/constants protection levels */` |
|         - | 2638 | `#define PH7_CLASS_PROT_PUBLIC     1 /* public */` |
|         - | 2639 | `#define PH7_CLASS_PROT_PROTECTED  2 /* protected */` |
|         - | 2640 | `#define PH7_CLASS_PROT_PRIVATE    3 /* private */` |
|         - | 2641 | `/*` |
|         - | 2642 | ` * each class attribute (variable, constants) is parsed out and stored` |
|         - | 2643 | ` * in an instance of the following structure.` |
|         - | 2644 | ` */` |
|         - | 2645 | `struct ph7_class_attr` |
|         - | 2646 | `{` |
|         - | 2647 | `	SyString sName;      /* Atrribute name */` |
|         - | 2648 | `	SyString sStoreName; /* php's MANGLED storage name for a PRIVATE instance property --` |
|         - | 2649 | `	                      * "\0DeclaringClass\0name" -- materialized the first time this` |
|         - | 2650 | `	                      * attribute is filed in a class that did not declare it. Empty` |
|         - | 2651 | `	                      * (nByte == 0) until then, and for every other attribute, whose` |
|         - | 2652 | `	                      * storage name is sName. See PH7_ClassAttrStorageName. */` |
|         - | 2653 | `	sxi32 iFlags;        /* Attribute configuration [i.e: static, variable, constant, etc.] */` |
|         - | 2654 | `	sxi32 iProtection;   /* Protection level [i.e: public, private, protected] */` |
|         - | 2655 | `	SySet aByteCode;     /* Compiled attribute body */` |
|         - | 2656 | `	sxu32 nIdx;          /* Attribute index */` |
|         - | 2657 | `	sxu32 nLine;         /* Line number on which this attribute was defined */` |
|         - | 2658 | `	sxu32 nType;         /* Declared type: MEMOBJ_* bitmask, SXU32_HIGH for class, 0 = untyped */` |
|         - | 2659 | `	SyString sClass;     /* Class/interface name when nType == SXU32_HIGH */` |
|         - | 2660 | `	SyString sTypeName;  /* Original type text for error messages (e.g. "?int", "Foo", "string\|int") */` |
|         - | 2661 | `	SySet aUnionAlts;    /* Union alternatives (ph7_type_alt). Empty unless PH7_CLASS_ATTR_UNION is set. */` |
|         - | 2662 | `	ph7_class *pDeclClass; /* Class that originally declared this attribute */` |
|         - | 2663 | `	SyString sDoc;       /* Doc-comment preceding the declaration (duplicated; empty = none) */` |
|         - | 2664 | `	SySet aAttrs;        /* Declared #[...] attributes (ph7_attribute records) */` |
|         - | 2665 | `	const void *pNativeValue; /* A native class's literal initializer (PH7_NativeConstDef*), or 0.` |
|         - | 2666 | `	                      * A compiled declaration expresses its default as aByteCode evaluated at` |
|         - | 2667 | `	                      * mount; the C builder has no compiler to emit that, so it hands the` |
|         - | 2668 | `	                      * literal here and the mount writes it straight into the reserved slot.` |
|         - | 2669 | `	                      * Mutually exclusive with a non-empty aByteCode. */` |
|         - | 2670 | `};` |
|         - | 2671 | `/* Attribute configuration */` |
|         - | 2672 | `#define PH7_CLASS_ATTR_STATIC       0x001  /* Static attribute */` |
|         - | 2673 | `#define PH7_CLASS_ATTR_CONSTANT     0x002  /* Constant attribute */` |
|         - | 2674 | `#define PH7_CLASS_ATTR_ABSTRACT     0x004  /* Abstract method */` |
|         - | 2675 | `#define PH7_CLASS_ATTR_FINAL        0x008  /* Final method */` |
|         - | 2676 | `#define PH7_CLASS_ATTR_TYPED        0x010  /* Property has an explicit declared type */` |
|         - | 2677 | `#define PH7_CLASS_ATTR_NULLABLE     0x020  /* Type allows null (?type prefix or T\|null union) */` |
|         - | 2678 | `#define PH7_CLASS_ATTR_UNION        0x040  /* Property has a union type (use aUnionAlts) */` |
|         - | 2679 | `#define PH7_CLASS_ATTR_READONLY     0x080  /* readonly property (PHP 8.1) */` |
|         - | 2680 | `#define PH7_CLASS_ATTR_DYNAMIC      0x100  /* Runtime-added (dynamic) property: the ph7_class_attr is` |
|         - | 2681 | `                                            * instance-owned (synthesized, not class-declared) and must` |
|         - | 2682 | `                                            * be freed when the instance is released. */` |
|         - | 2683 | `#define PH7_CLASS_ATTR_ENUMCASE     0x200  /* Enum case: a class constant whose value is the lazily` |
|         - | 2684 | `                                            * materialized case singleton (aByteCode holds the BACKING` |
|         - | 2685 | `                                            * value expression for backed enums; empty when pure). */` |
|         - | 2686 | `#define PH7_CLASS_ATTR_EVALING      0x400  /* Transient: this constant's initializer is being evaluated` |
|         - | 2687 | `                                            * (on-demand, VmClassConstEvalOnDemand). Re-entry means a` |
|         - | 2688 | `                                            * self-referencing constant — php's catchable Error. */` |
|         - | 2689 | `#define PH7_CLASS_ATTR_PRIVATE_SET  0x800  /* private(set) asymmetric visibility (PHP 8.4): writes` |
|         - | 2690 | `                                            * only from the DECLARING class scope (subclasses excluded) */` |
|         - | 2691 | `#define PH7_CLASS_ATTR_PROTECTED_SET 0x1000 /* protected(set) asymmetric visibility (PHP 8.4): writes` |
|         - | 2692 | `                                            * from the declaring class or a subclass scope */` |
|         - | 2693 | `#define PH7_CLASS_ATTR_PUBLIC_SET   0x2000 /* explicit public(set): behaviorally the default, kept` |
|         - | 2694 | `                                            * for the weaker-than-set check and reflection output */` |
|         - | 2695 | ``#define PH7_CLASS_ATTR_HOOK_GET     0x4000 /* property has a `get` hook (PHP 8.4): reads dispatch`` |
|         - | 2696 | `                                            * __phl_hook_get_NAME (guard-bypassed inside hooks) */` |
|         - | 2697 | ``#define PH7_CLASS_ATTR_HOOK_SET     0x8000 /* property has a `set` hook (PHP 8.4): plain writes`` |
|         - | 2698 | `                                            * dispatch __phl_hook_set_NAME */` |
|         - | 2699 | `#define PH7_CLASS_ATTR_HOOK_VIRTUAL 0x10000 /* PHP 8.4 VIRTUAL hooked property: none of its own` |
|         - | 2700 | ``                                            * hook bodies references `$this->NAME`, so php gives it`` |
|         - | 2701 | `                                            * no backing store — excluded from the raw object` |
|         - | 2702 | `                                            * surfaces (var_dump/(array)/print_r/serialize/` |
|         - | 2703 | `                                            * get_class_vars and the get-dispatching walks when it` |
|         - | 2704 | `                                            * has no get hook), no default allowed, reads without a` |
|         - | 2705 | `                                            * get hook are php's "is write-only" Error. PHL still` |
|         - | 2706 | `                                            * allocates the (null) backing slot; this flag hides it. */` |
|         - | 2707 | `#define PH7_CLASS_ATTR_NATIVE_SET   0x100000 /* A NATIVE class's property whose WRITES run through` |
|         - | 2708 | `                                            * ph7_class::xSet (php's write_property). Set by the` |
|         - | 2709 | `                                            * installer that assigns the hook, and read by the two` |
|         - | 2710 | `                                            * places that care: instantiation, which registers the` |
|         - | 2711 | `                                            * slot so the store filter can find it, and the filter` |
|         - | 2712 | `                                            * itself. A slot carrying it is registered in` |
|         - | 2713 | `                                            * pVm->hTypedSlot exactly as a typed one is -- that table` |
|         - | 2714 | `                                            * is "slots a store must be filtered through", and the` |
|         - | 2715 | `                                            * two reasons compose (a native property may also be` |
|         - | 2716 | `                                            * typed). */` |
|         - | 2717 | `#define PH7_CLASS_ATTR_REFSRCPIN    0x8000000 /* STATIC property that is the SOURCE of a reference:` |
|         - | 2718 | `                                               * it holds one counted pin on its own slot, the` |
|         - | 2719 | `                                               * instance-side VM_CLASS_ATTR_REFSRCPIN's twin. A` |
|         - | 2720 | `                                               * class static lives as long as the VM, so the pin` |
|         - | 2721 | `                                               * is never given back -- which is the point: it` |
|         - | 2722 | `                                               * stops the other end's unpin from freeing it. */` |
|         - | 2723 | `` #define PH7_CLASS_ATTR_REFBOUND     0x80000 /* STATIC property currently bound to another slot by `=&` `` |
|         - | 2724 | ``                                             * (`C::$s =& $x`). The instance side records this per`` |
|         - | 2725 | `                                             * INSTANCE (VM_CLASS_ATTR_REFBOUND); a static has one slot` |
|         - | 2726 | `                                             * per declaration, so the bit lives here. It says the slot` |
|         - | 2727 | `                                             * this attribute points at is held by a COUNTED pin, and a` |
|         - | 2728 | `                                             * rebind must give that pin back rather than free a slot the` |
|         - | 2729 | `                                             * attribute never owned. */` |
|         - | 2730 | `#define PH7_CLASS_ATTR_HIDDEN       0x40000 /* A NATIVE class's engine slot: real storage that php keeps` |
|         - | 2731 | `                                            * in its own C struct and therefore never shows. Excluded` |
|         - | 2732 | `                                            * from every PRESENTATION surface — var_dump/print_r/` |
|         - | 2733 | `                                            * var_export, (array), get_object_vars, foreach, json_encode,` |
|         - | 2734 | `                                            * serialize, http_build_query and Reflection's property` |
|         - | 2735 | ``                                            * listing — while `new`, clone and the native bodies' own`` |
|         - | 2736 | `                                            * PH7_NativeAttr() reads still see it. Set from` |
|         - | 2737 | `                                            * PH7_MOD_HIDDEN on a PH7_NativePropDef. Use it for a slot` |
|         - | 2738 | `                                            * php shows NOTHING for (a handle, a cursor cache); a slot` |
|         - | 2739 | ``                                            * php shows under a DIFFERENT name (ArrayObject's `storage`,`` |
|         - | 2740 | ``                                            * DateTime's `date`) wants the recorded presentation hook`` |
|         - | 2741 | `                                            * still asks for, not this bit. */` |
|         - | 2742 | `#define PH7_CLASS_ATTR_STATIC_DEFER 0x20000 /* STATIC property whose default initializer THREW when it` |
|         - | 2743 | `                                            * was evaluated at class mount. php never evaluates a static` |
|         - | 2744 | `                                            * default at declaration time — it materializes the class's` |
|         - | 2745 | `                                            * static table at the FIRST static-property access — so the` |
|         - | 2746 | `                                            * mount-time throw is raised MUTED (VmEvalDefaultMuted: no` |
|         - | 2747 | `                                            * catch runs, nothing is reported) and rolled back whole,` |
|         - | 2748 | `                                            * and the initializer re-runs at that first access` |
|         - | 2749 | `                                            * (PH7_VmMaterializeClassStatics), where php raises it.` |
|         - | 2750 | `                                            * Cleared once the initializer completes without throwing:` |
|         - | 2751 | `                                            * a class whose bad default is never READ stays silent in` |
|         - | 2752 | `                                            * both engines, and a re-run that now succeeds (the constant` |
|         - | 2753 | `                                            * it names was define()d after the declaration) answers the` |
|         - | 2754 | `                                            * value, as php's does. */` |
|         - | 2755 | `#define PH7_CLASS_ATTR_NATIVE_VIRTUAL 0x200000 /* A NATIVE class's property that php FABRICATES on` |
|         - | 2756 | `                                            * demand (its get_properties handler) instead of keeping` |
|         - | 2757 | `                                            * in the object's real property table. DatePeriod's seven` |
|         - | 2758 | `                                            * are php's case: they read and write like ordinary` |
|         - | 2759 | `                                            * properties, so PHL declares real slots for them, but` |
|         - | 2760 | `                                            * php's COMPARISON walks the real table and finds nothing` |
|         - | 2761 | `                                            * there -- which is why any two DatePeriods are equal in` |
|         - | 2762 | `                                            * php whatever they contain, while a subclass's own` |
|         - | 2763 | `                                            * property still decides. Read only by the object` |
|         - | 2764 | `                                            * comparator; presentation is the HIDDEN bit's business,` |
|         - | 2765 | `                                            * and these are shown. */` |
|         - | 2766 | `#define PH7_CLASS_ATTR_NATIVE_LAZY  0x400000 /* A NATIVE class's property the OBJECT does not hold until` |
|         - | 2767 | `                                            * its constructor fills it. php's DateInterval and` |
|         - | 2768 | `                                            * DatePeriod are the case: the state lives in a C struct the` |
|         - | 2769 | `                                            * constructor allocates, and the property table is written` |
|         - | 2770 | `                                            * FROM that struct -- so an object nobody constructed has` |
|         - | 2771 | ``                                            * no such property at all, and `$i->y` there is an`` |
|         - | 2772 | ``                                            * `Undefined property` warning, `isset()` is false and`` |
|         - | 2773 | `                                            * get_object_vars()/foreach see nothing. The instance frame` |
|         - | 2774 | ``                                            * skips these at `new`; the whole set is installed, in`` |
|         - | 2775 | `                                            * declared order, the first time a C body writes one` |
|         - | 2776 | `                                            * (PH7_NativeMaterializeLazy), which is every constructor` |
|         - | 2777 | `                                            * and every C factory. */` |
|         - | 2778 | `#define PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT 0x800000 /* A LAZY property php really does DECLARE -- so` |
|         - | 2779 | `                                            * Reflection lists it whatever the object holds -- and whose` |
|         - | 2780 | `                                            * READ, while the slot is still absent, answers the declared` |
|         - | 2781 | `                                            * literal in SILENCE. php's split between its two handlers:` |
|         - | 2782 | `                                            * DatePeriod declares its seven and reads them through a` |
|         - | 2783 | `                                            * read_property that answers the ZEROED struct` |
|         - | 2784 | `                                            * (null/0/false), while DateInterval declares nothing at all` |
|         - | 2785 | `                                            * and its ten are undefined until the constructor runs. The` |
|         - | 2786 | `                                            * literal IS that zeroed field, which is why one bit says` |
|         - | 2787 | `                                            * both things. */` |
|         - | 2788 | `#define PH7_CLASS_ATTR_NATIVE_NOWRITE 0x1000000 /* A NATIVE class's property whose write_property` |
|         - | 2789 | `                                            * handler REFUSES every write. DatePeriod's seven are` |
|         - | 2790 | ``                                            * php's case: its handler answers `Cannot modify readonly`` |
|         - | 2791 | ``                                            * property C::$p` -- the readonly WORDING without the`` |
|         - | 2792 | `                                            * readonly flag, so Reflection still reports isReadOnly()` |
|         - | 2793 | ``                                            * false -- and its unset handler answers `Cannot unset`` |
|         - | 2794 | ``                                            * C::$p`. Every write form is refused, not just `=`:`` |
|         - | 2795 | ``                                            * `++`, a by-reference bind, a destructuring target, and`` |
|         - | 2796 | `                                            * a write to an object that was never constructed. */` |
|         - | 2797 | `#define PH7_CLASS_ATTR_NATIVE_ONDEMAND 0x2000000 /* A LAZY property the group materialization` |
|         - | 2798 | `                                            * SKIPS: it is installed only when a C body writes` |
|         - | 2799 | `                                            * it by name, so an object that never took one does` |
|         - | 2800 | `                                            * not carry the name at all. php's DateInterval` |
|         - | 2801 | ``                                            * `date_string` is the case -- it exists on an`` |
|         - | 2802 | `                                            * interval built from a STRING and on no other, and` |
|         - | 2803 | ``                                            * `isset()`/`property_exists()` answer false there. */`` |
|         - | 2804 | `#define PH7_CLASS_ATTR_FABRICATED   0x10000000 /* METHOD php builds on demand instead of keeping in` |
|         - | 2805 | `                                 * the class's function table. See PH7_MOD_FABRICATED for the five` |
|         - | 2806 | `                                 * doors and how each of them answers. */` |
|         - | 2807 | `#define PH7_CLASS_ATTR_NATIVE_NOSLOT 0x4000000 /* A NATIVE class's VIRTUAL property: php DECLARES the` |
|         - | 2808 | `                                            * name (Reflection lists it, property_exists() answers` |
|         - | 2809 | `                                            * true, isVirtual() true and hasDefaultValue() false) and` |
|         - | 2810 | `                                            * keeps NO slot for it -- every value is a read_property /` |
|         - | 2811 | `                                            * write_property handler over the extension's own state.` |
|         - | 2812 | ``                                            * ext/dom is the case: all forty of `DOMDocument`'s are`` |
|         - | 2813 | ``                                            * handlers, which is why php's `(array)` cast,`` |
|         - | 2814 | ``                                            * `get_object_vars()`, `json_encode()`, `foreach`,`` |
|         - | 2815 | ``                                            * `var_export()` and `get_mangled_object_vars()` show`` |
|         - | 2816 | ``                                            * NOTHING for one while `print_r`/`var_dump` show the`` |
|         - | 2817 | `                                            * whole forty (the get_debug_info handler, ph7_class::` |
|         - | 2818 | ``                                            * xPresent). The instance frame skips these at `new`, so`` |
|         - | 2819 | `                                            * a read, a write and an isset() all take the miss path` |
|         - | 2820 | `                                            * and reach the class's __get/__set/__isset exactly as an` |
|         - | 2821 | ``                                            * undeclared name does; `unset()` is php's`` |
|         - | 2822 | ``                                            * `Cannot unset C::$p` rather than a silent no-op. */`` |
|         - | 2823 | `/* next free bit: 0x8000000 */` |
|         - | 2824 | `/*` |
|         - | 2825 | ` * Does a store into this property's slot have to be FILTERED? Two unrelated` |
|         - | 2826 | ` * reasons say yes -- a declared TYPE to enforce and a native class's own write` |
|         - | 2827 | ` * handler -- and both are answered by one lookup, since pVm->hTypedSlot keys` |
|         - | 2828 | ` * every filtered slot by its memobj index. Instantiation registers on this` |
|         - | 2829 | ` * predicate and the teardown paths deregister on it, so the two must never` |
|         - | 2830 | ` * disagree.` |
|         - | 2831 | ` */` |
|         - | 2832 | `#define PH7_ATTR_STORE_FILTERED(pAttr) \` |
|         - | 2833 | `	(((pAttr)->iFlags & (PH7_CLASS_ATTR_TYPED\|PH7_CLASS_ATTR_NATIVE_SET \` |
|         - | 2834 | `	                     \|PH7_CLASS_ATTR_NATIVE_NOWRITE)) != 0)` |
|         - | 2835 | `/*` |
|         - | 2836 | ` * Declaring a class from C (oo_native.c).` |
|         - | 2837 | ` *` |
|         - | 2838 | ` * A subsystem describes its classes as static tables and hands them to` |
|         - | 2839 | ` * PH7_InstallNativeClasses(), which drives the very builders the compiler drives` |
|         - | 2840 | `` * for `class Foo {}`. The point of the exercise is the METHOD table: a method's`` |
|         - | 2841 | ` * body may be a C routine (VM_FUNC_NATIVE), so the engine-access helpers that had` |
|         - | 2842 | `` * to be global `__prefix_verb()` thunks — because only a global function could be`` |
|         - | 2843 | ` * C — become methods of the class they always belonged to.` |
|         - | 2844 | ` */` |
|         - | 2845 | `/* Member modifiers. Visibility defaults to public when none is given. */` |
|         - | 2846 | `#define PH7_MOD_PUBLIC     0x00` |
|         - | 2847 | `#define PH7_MOD_PROTECTED  0x01` |
|         - | 2848 | `#define PH7_MOD_PRIVATE    0x02` |
|         - | 2849 | `#define PH7_MOD_STATIC     0x04` |
|         - | 2850 | `#define PH7_MOD_FINAL      0x08` |
|         - | 2851 | `#define PH7_MOD_ABSTRACT   0x10 /* No body: an interface's method, or an abstract declaration */` |
|         - | 2852 | `#define PH7_MOD_HIDDEN     0x20 /* PROPERTY only: an engine slot php keeps in its own struct and` |
|         - | 2853 | `                                 * never presents (PH7_CLASS_ATTR_HIDDEN). */` |
|         - | 2854 | ``#define PH7_MOD_READONLY   0x40 /* PROPERTY only: php's `readonly` (PH7_CLASS_ATTR_READONLY) */`` |
|         - | 2855 | ``#define PH7_MOD_PROT_SET   0x80 /* PROPERTY only: php's `protected(set)` asymmetric visibility */`` |
|         - | 2856 | ``#define PH7_MOD_PRIV_SET   0x100 /* PROPERTY only: php's `private(set)` asymmetric visibility */`` |
|         - | 2857 | `#define PH7_MOD_ONDEMAND   0x200 /* PROPERTY only: installed on the object only when a C body` |
|         - | 2858 | `                                  * writes it (PH7_CLASS_ATTR_NATIVE_ONDEMAND) */` |
|         - | 2859 | `#define PH7_MOD_VIRTUAL    0x400 /* PROPERTY only: php's VIRTUAL native property -- declared on the` |
|         - | 2860 | `                                  * class and answered by its own handlers, with NO slot on the` |
|         - | 2861 | `                                  * object (PH7_CLASS_ATTR_NATIVE_NOSLOT) */` |
|         - | 2862 | `#define PH7_MOD_FABRICATED 0x800 /* METHOD only: php does not keep this one in the class's function` |
|         - | 2863 | `                                  * table -- it BUILDS it when something asks (Closure::__invoke is` |
|         - | 2864 | `                                  * the whole set). Present to method_exists(), hasMethod(),` |
|         - | 2865 | `                                  * getMethod() and getMethods(); absent from get_class_methods()` |
|         - | 2866 | `                                  * and from the class's own export, and refused by` |
|         - | 2867 | `                                  * ReflectionMethod::__construct, which is exactly the shape those` |
|         - | 2868 | `                                  * five doors have in php (PH7_CLASS_ATTR_FABRICATED). */` |
|         - | 2869 | `/* Literal kinds a native class constant may carry */` |
|         - | 2870 | `#define PH7_NATIVE_VAL_NULL   0` |
|         - | 2871 | `#define PH7_NATIVE_VAL_INT    1` |
|         - | 2872 | `#define PH7_NATIVE_VAL_STRING 2` |
|         - | 2873 | `#define PH7_NATIVE_VAL_BOOL   3` |
|         - | 2874 | `#define PH7_NATIVE_VAL_DOUBLE 4` |
|         - | 2875 | ``#define PH7_NATIVE_VAL_ARRAY  6 /* The EMPTY array, php's `private array $trace = [];`. The`` |
|         - | 2876 | `                                 * only array literal a stub default needs — anything with` |
|         - | 2877 | `                                 * elements would want the compiler's byte-code. */` |
|         - | 2878 | `#define PH7_NATIVE_VAL_NONE   5 /* On a PROPERTY row only: the slot has NO default at all,` |
|         - | 2879 | ``                                 * php's `public string $name;`. It needs a declared zType to`` |
|         - | 2880 | `                                 * mean anything (an untyped slot without a default is null),` |
|         - | 2881 | ``                                 * and it makes the property UNINITIALIZED at `new` — reading`` |
|         - | 2882 | `                                 * it before the class's own C body writes it is php's` |
|         - | 2883 | `                                 * "must not be accessed before initialization" Error, and` |
|         - | 2884 | `                                 * hasDefaultValue() answers false. PH7_NATIVE_VAL_NULL is the` |
|         - | 2885 | ``                                 * different thing it reads like: an explicit `= null`. */`` |
|         - | 2886 | `typedef struct PH7_NativeMethodDef PH7_NativeMethodDef;` |
|         - | 2887 | `typedef struct PH7_NativeConstDef  PH7_NativeConstDef;` |
|         - | 2888 | `typedef struct PH7_NativeClassSpec PH7_NativeClassSpec;` |
|         - | 2889 | `struct PH7_NativeMethodDef` |
|         - | 2890 | `{` |
|         - | 2891 | `	const char *zName;       /* php-visible method name */` |
|         - | 2892 | `	sxi32 iMods;             /* PH7_MOD_* */` |
|         - | 2893 | `	const char *zSig;        /* PHP-style parameter list ("string $name, int $flags = 0"),` |
|         - | 2894 | `	                          * or 0 for "unenforced". Static storage: never freed. Drives` |
|         - | 2895 | `	                          * arity enforcement, the by-ref mask AND Reflection, from the` |
|         - | 2896 | `	                          * one string — the same contract aBuiltinSig[] has. */` |
|         - | 2897 | `	const char *zRet;        /* Return-type text, or 0 */` |
|         - | 2898 | `	ProchHostFunction xFunc; /* The body */` |
|         - | 2899 | `};` |
|         - | 2900 | `struct PH7_NativeConstDef` |
|         - | 2901 | `{` |
|         - | 2902 | `	const char *zName;` |
|         - | 2903 | `	sxi32 iMods;` |
|         - | 2904 | `	sxi32 iType;             /* PH7_NATIVE_VAL_* */` |
|         - | 2905 | `	ph7_int64 iValue;        /* INT / BOOL */` |
|         - | 2906 | `	const char *zValue;      /* STRING */` |
|         - | 2907 | `	double rValue;           /* DOUBLE */` |
|         - | 2908 | `};` |
|         - | 2909 | `/*` |
|         - | 2910 | ` * A declared property. Its default is the same literal record a constant uses --` |
|         - | 2911 | ` * a compiled declaration would carry compiled byte-code here, which the builder` |
|         - | 2912 | ` * has no compiler to emit, so the value is stated directly and materialized at` |
|         - | 2913 | `` * `new` (instance) or at mount (static) by PH7_NativeLiteralValue.`` |
|         - | 2914 | ` */` |
|         - | 2915 | `typedef struct PH7_NativePropDef PH7_NativePropDef;` |
|         - | 2916 | `struct PH7_NativePropDef` |
|         - | 2917 | `{` |
|         - | 2918 | `	const char *zName;` |
|         - | 2919 | `	sxi32 iMods;             /* PH7_MOD_* (STATIC supported; FINAL ignored) */` |
|         - | 2920 | `	PH7_NativeConstDef sDefault; /* iType PH7_NATIVE_VAL_NULL = plain null default,` |
|         - | 2921 | `	                              * PH7_NATIVE_VAL_NONE = no default at all (typed slots) */` |
|         - | 2922 | `	const char *zType;       /* Declared type as php writes it ("?string", "int", "DateInterval"),` |
|         - | 2923 | `	                          * or 0 for an untyped slot. Enforced on every store and printed by` |
|         - | 2924 | ``	                          * Reflection exactly as a compiled `public ?string $p` would be —`` |
|         - | 2925 | `	                          * php declares a type on every property it presents, so a slot the` |
|         - | 2926 | ``	                          * class SHOWS wants one. Single atoms only (a leading `?` plus one`` |
|         - | 2927 | `	                          * scalar keyword or class name); a union needs the compiler's` |
|         - | 2928 | `	                          * alternative set and is not expressible here. */` |
|         - | 2929 | `};` |
|         - | 2930 | `struct PH7_NativeClassSpec` |
|         - | 2931 | `{` |
|         - | 2932 | `	const char *zName;` |
|         - | 2933 | `	const char *zParent;     /* or 0 */` |
|         - | 2934 | `	const char *zImplements; /* comma-separated list, or 0 */` |
|         - | 2935 | `	sxi32 iFlags;            /* PH7_CLASS_FINAL / ABSTRACT / INTERFACE / READONLY */` |
|         - | 2936 | `	const PH7_NativeMethodDef *aMethod; sxu32 nMethod;` |
|         - | 2937 | `	const PH7_NativeConstDef  *aConst;  sxu32 nConst;` |
|         - | 2938 | `	const PH7_NativePropDef   *aProp;   sxu32 nProp;` |
|         - | 2939 | `	void (*xRelease)(ph7_vm *,ph7_class_instance *); /* or 0; see ph7_class::xRelease */` |
|         - | 2940 | `	const PH7_NativeIterVtab *pIterVtab; /* or 0; see ph7_class::pIterVtab */` |
|         - | 2941 | `	/* xNew is not stated here: a spec that wants one installs it after mounting` |
|         - | 2942 | `	 * with PH7_NativeClassInstallNewHook(), the way the property, set and` |
|         - | 2943 | `	 * comparison hooks are installed. */` |
|         - | 2944 | `	sxi32 (*xPresent)(ph7_vm *,ph7_class_instance *,ph7_value *,int); /* or 0; see ph7_class::xPresent */` |
|         - | 2945 | `};` |
|         - | 2946 | `/*` |
|         - | 2947 | `` * One `case Name = <literal>;` of a native ENUM. The backing value is the same`` |
|         - | 2948 | ` * literal record a constant carries; iType PH7_NATIVE_VAL_NULL is a PURE enum's` |
|         - | 2949 | ` * case, which has no value at all.` |
|         - | 2950 | ` */` |
|         - | 2951 | `typedef struct PH7_NativeEnumCase PH7_NativeEnumCase;` |
|         - | 2952 | `struct PH7_NativeEnumCase` |
|         - | 2953 | `{` |
|         - | 2954 | `	const char *zName;` |
|         - | 2955 | `	PH7_NativeConstDef sValue;   /* the backing literal; zName/iMods unused */` |
|         - | 2956 | `};` |
|         - | 2957 | `PH7_PRIVATE sxi32 PH7_InstallNativeClasses(ph7_vm *pVm,const PH7_NativeClassSpec *aSpec,sxu32 nSpec);` |
|         - | 2958 | `PH7_PRIVATE int PH7_ClassInstancePresent(ph7_class_instance *pThis,ph7_value *pOut,int bDebug);` |
|         - | 2959 | `PH7_PRIVATE int PH7_ClassHasNativeDim(ph7_class *pClass);` |
|         - | 2960 | `PH7_PRIVATE int PH7_ClassNativeDim(ph7_class_instance *pThis,PH7_NativeDimCtx *pCtx);` |
|         - | 2961 | `/* Offer a dimension WRITE / APPEND / UNSET to the class's own handler, with the` |
|         - | 2962 | ` * offset and the incoming value. Answers 1 when the handler TOOK it (or refused` |
|         - | 2963 | ` * it in its own words, which zThrowClass then carries) and 0 when the caller` |
|         - | 2964 | ` * must raise the ordinary refusal. */` |
|         - | 2965 | `PH7_PRIVATE int PH7_ClassNativeDimStore(ph7_class_instance *pThis,int iMode,` |
|         - | 2966 | `	ph7_value *pOffset,ph7_value *pValue,PH7_NativeDimCtx *pCtx);` |
|         - | 2967 | `/* The refusal a native container gives a dimension WRITE/APPEND/UNSET: its own` |
|         - | 2968 | `` * sentence when its hook words one, and php's `Cannot use object of type C as`` |
|         - | 2969 | `` * array` for every class that does not. Answers the message length. */`` |
|         - | 2970 | `PH7_PRIVATE sxu32 PH7_ClassNativeDimRefusal(ph7_class_instance *pThis,int iMode,` |
|         - | 2971 | `	char *zMsg,sxu32 nMsg);` |
|         - | 2972 | `/* php's instantiation gate -- interface / trait / enum / abstract / a class` |
|         - | 2973 | `` * whose create_object handler refuses -- asked by every C-side `new`. */`` |
|         - | 2974 | `PH7_PRIVATE sxi32 PH7_VmCheckInstantiable(ph7_context *pCtx,ph7_class *pClass);` |
|         - | 2975 | `PH7_PRIVATE int PH7_ClassHasNativeProp(ph7_class *pClass);` |
|         - | 2976 | `PH7_PRIVATE int PH7_ClassNativeProp(ph7_class_instance *pThis,PH7_NativePropCtx *pCtx);` |
|         - | 2977 | `PH7_PRIVATE int PH7_ClassNativePropAsk(ph7_class_instance *pThis,PH7_NativePropCtx *pCtx,` |
|         - | 2978 | `	int iMode,const SyString *pName,ph7_value *pResult);` |
|         - | 2979 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallPropHook(ph7_vm *pVm,const char *zClass,` |
|         - | 2980 | `	void (*xProp)(ph7_vm *,ph7_class_instance *,PH7_NativePropCtx *));` |
|         - | 2981 | `PH7_PRIVATE int PH7_ClassNativePropOwns(ph7_class_instance *pThis,const SyString *pName);` |
|         - | 2982 | `PH7_PRIVATE int PH7_ClassNativeSet(ph7_class_instance *pThis,PH7_NativeSetCtx *pCtx);` |
|         - | 2983 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallSetHook(ph7_vm *pVm,const char *zClass,` |
|         - | 2984 | `	void (*xSet)(ph7_vm *,ph7_class_instance *,PH7_NativeSetCtx *));` |
|         - | 2985 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallNewHook(ph7_vm *pVm,const char *zClass,` |
|         - | 2986 | `	void (*xNew)(ph7_vm *,ph7_class_instance *));` |
|         - | 2987 | `PH7_PRIVATE int PH7_ClassNativeCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,sxi32 *pResult);` |
|         - | 2988 | `PH7_PRIVATE int PH7_ClassNativeCmpValue(ph7_class_instance *pLeft,ph7_value *pOther,` |
|         - | 2989 | `	int bReversed,sxi32 *pResult);` |
|         - | 2990 | `PH7_PRIVATE void PH7_NativeCmpOpaqueHandle(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeCmpCtx *pCtx);` |
|         - | 2991 | `PH7_PRIVATE int PH7_ClassCastsToHandleId(ph7_class *pClass);` |
|         - | 2992 | `PH7_PRIVATE int PH7_ClassNumberIsString(ph7_class *pClass);` |
|         - | 2993 | `PH7_PRIVATE int PH7_ClassVarsFromPresent(ph7_class *pClass);` |
|         - | 2994 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallCmpHook(ph7_vm *pVm,const char *zClass,` |
|         - | 2995 | `	void (*xCmp)(ph7_vm *,ph7_class_instance *,PH7_NativeCmpCtx *));` |
|         - | 2996 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallArithHook(ph7_vm *pVm,const char *zClass,` |
|         - | 2997 | `	void (*xArith)(ph7_vm *,ph7_class_instance *,PH7_NativeArithCtx *));` |
|         - | 2998 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallBoolHook(ph7_vm *pVm,const char *zClass,` |
|         - | 2999 | `	int (*xBool)(ph7_vm *,ph7_class_instance *));` |
|         - | 3000 | `PH7_PRIVATE int PH7_ClassNativeBool(ph7_class_instance *pThis,int *pOut);` |
|         - | 3001 | `/*` |
|         - | 3002 | ` * What VmArithOperandStep() decided about one operator's pair.` |
|         - | 3003 | ` */` |
|         - | 3004 | `#define PH7_ARITH_ORDINARY  0   /* no handler: run the numeric arithmetic */` |
|         - | 3005 | `#define PH7_ARITH_HANDLED   1   /* a handler answered; the destination already holds it */` |
|         - | 3006 | `#define PH7_ARITH_REFUSED  (-1) /* throw *pzClass with the message in pMsgOut */` |
|         - | 3007 | `PH7_PRIVATE int VmArithOperandStep(ph7_vm *pVm,ph7_value *pLeft,ph7_value *pRight,` |
|         - | 3008 | `	const char *zOp,ph7_value *pDest,const char **pzClass,SyBlob *pMsgOut);` |
|         - | 3009 | `PH7_PRIVATE int PH7_ValueHasArithHandler(ph7_value *pVal);` |
|         - | 3010 | `PH7_PRIVATE sxi32 PH7_NativeClassMarkVirtualProps(ph7_vm *pVm,const char *zClass);` |
|         - | 3011 | `PH7_PRIVATE sxi32 PH7_NativeClassMarkLazyProps(ph7_vm *pVm,const char *zClass,int bDefaultRead);` |
|         - | 3012 | `PH7_PRIVATE sxi32 PH7_NativeClassMarkNoWriteProps(ph7_vm *pVm,const char *zClass);` |
|         - | 3013 | `PH7_PRIVATE void PH7_NativeMaterializeLazy(ph7_vm *pVm,ph7_class_instance *pObj);` |
|         - | 3014 | `PH7_PRIVATE void PH7_NativeHideAttr(ph7_class_instance *pObj,const char *zName);` |
|         - | 3015 | `/*` |
|         - | 3016 | ` * The refusal a native compare handler carried back (ph7_vm::zCmpRefusalClass):` |
|         - | 3017 | ` * pending? raise it here, where a throw can be routed; raise it on a host CALL` |
|         - | 3018 | ` * CONTEXT, so a builtin that compared reports it the way its own throws are` |
|         - | 3019 | ` * reported; or drop it, for the two comparison doors that are not PHP execution` |
|         - | 3020 | ` * at all (the public ph7_value_compare, a VM reset).` |
|         - | 3021 | ` */` |
|         - | 3022 | `PH7_PRIVATE int PH7_CmpRefusalPending(ph7_vm *pVm);` |
|         - | 3023 | `/* Record php's comparison-recursion refusal ("Nesting level too deep - recursive` |
|         - | 3024 | ` * dependency?", an Error) through the same channel, for the two walks -- arrays and` |
|         - | 3025 | ` * objects -- that find themselves inside a container they are already inside. */` |
|         - | 3026 | `PH7_PRIVATE void PH7_CmpRefusalNesting(ph7_vm *pVm);` |
|         - | 3027 | `PH7_PRIVATE sxi32 PH7_CmpRefusalRaise(ph7_vm *pVm);` |
|         - | 3028 | `PH7_PRIVATE sxi32 PH7_CmpRefusalRaiseCtx(ph7_context *pCtx);` |
|         - | 3029 | `PH7_PRIVATE void PH7_CmpRefusalClear(ph7_vm *pVm);` |
|         - | 3030 | `PH7_PRIVATE sxi32 PH7_InstallEnumInterfaceMethods(ph7_vm *pVm,ph7_class *pClass);` |
|         - | 3031 | `PH7_PRIVATE sxi32 PH7_InstallNativeEnum(ph7_vm *pVm,const char *zName,sxu32 nBacking,` |
|         - | 3032 | `	const PH7_NativeEnumCase *aCase,sxu32 nCase,` |
|         - | 3033 | `	const PH7_NativeMethodDef *aMethod,sxu32 nMethod);` |
|         - | 3034 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallMethod(ph7_vm *pVm,ph7_class *pClass,` |
|         - | 3035 | `	const PH7_NativeMethodDef *pDef,void *pUserData);` |
|         - | 3036 | `/*` |
|         - | 3037 | `` * Attach one `#[Name(literal, ...)]` to a class declared from C. php puts an`` |
|         - | 3038 | ` * attribute on two of its own attribute classes, and the whole record — the FQN` |
|         - | 3039 | ` * plus its arguments — is what the engine reads to VALIDATE a target and what` |
|         - | 3040 | ` * ReflectionAttribute answers.` |
|         - | 3041 | ` */` |
|         - | 3042 | `typedef struct PH7_NativeAttrArg PH7_NativeAttrArg;` |
|         - | 3043 | `struct PH7_NativeAttrArg` |
|         - | 3044 | `{` |
|         - | 3045 | `	const char *zName;           /* named argument, or 0 for a positional one */` |
|         - | 3046 | `	PH7_NativeConstDef sValue;   /* the literal; zName/iMods unused */` |
|         - | 3047 | `};` |
|         - | 3048 | `PH7_PRIVATE sxi32 PH7_NativeClassAddAttribute(ph7_vm *pVm,ph7_class *pClass,` |
|         - | 3049 | `	const char *zAttr,const PH7_NativeAttrArg *aArg,sxu32 nArg);` |
|         - | 3050 | `PH7_PRIVATE sxi32 PH7_NativeMethodSetNoDiscard(ph7_vm *pVm,ph7_class *pClass,` |
|         - | 3051 | `	const char *zMethod,const PH7_NativeAttrArg *aArg,sxu32 nArg);` |
|         - | 3052 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallProperty(ph7_vm *pVm,ph7_class *pClass,` |
|         - | 3053 | `	const PH7_NativePropDef *pDef);` |
|         - | 3054 | `PH7_PRIVATE void PH7_NativeLiteralValue(ph7_vm *pVm,const void *pLiteral,ph7_value *pOut);` |
|         - | 3055 | `PH7_PRIVATE void PH7_NativeSetProp(ph7_vm *pVm,ph7_class_instance *pObj,` |
|         - | 3056 | `	const char *zProp,sxu32 nProp,ph7_value *pSrcVal);` |
|         - | 3057 | `/*` |
|         - | 3058 | ` * Reading and writing a native instance's own declared slots. Every native class` |
|         - | 3059 | ` * does this constantly (the date family had a private copy of the whole set), so` |
|         - | 3060 | ` * the accessors live with the builder that declares the slots.` |
|         - | 3061 | ` */` |
|         - | 3062 | `PH7_PRIVATE ph7_value * PH7_NativeAttr(ph7_class_instance *pObj,const char *zName);` |
|         - | 3063 | `PH7_PRIVATE int PH7_NativeAttrIsUninit(ph7_class_instance *pObj,const char *zName);` |
|         - | 3064 | `PH7_PRIVATE sxi64 PH7_NativeAttrInt(ph7_class_instance *pObj,const char *zName);` |
|         - | 3065 | `PH7_PRIVATE ph7_class_instance * PH7_NativeAttrObj(ph7_class_instance *pObj,const char *zName);` |
|         - | 3066 | `PH7_PRIVATE int PH7_NativeAttrTruthy(ph7_class_instance *pObj,const char *zName);` |
|         - | 3067 | `PH7_PRIVATE void PH7_NativeAttrStr(ph7_class_instance *pObj,const char *zName,` |
|         - | 3068 | `	const char **pzOut,int *pnOut);` |
|         - | 3069 | `PH7_PRIVATE void PH7_NativeSetAttrInt(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,sxi64 iVal);` |
|         - | 3070 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         - | 3071 | `PH7_PRIVATE void PH7_NativeSetAttrReal(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,ph7_real rVal);` |
|         - | 3072 | `#endif` |
|         - | 3073 | `PH7_PRIVATE void PH7_NativeSetAttrStr(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,` |
|         - | 3074 | `	const char *zVal,int nVal);` |
|         - | 3075 | `PH7_PRIVATE void PH7_NativeSetAttrBool(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,int bVal);` |
|         - | 3076 | `PH7_PRIVATE void PH7_NativeSetAttrObj(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,` |
|         - | 3077 | `	ph7_class_instance *pVal);` |
|         - | 3078 | `PH7_PRIVATE void PH7_NativeResultObject(ph7_context *pCtx,ph7_class_instance *pObj);` |
|         - | 3079 | `/*` |
|         - | 3080 | ` * php's InternalIterator: the Iterator a native IteratorAggregate answers when the` |
|         - | 3081 | ` * PHP it replaced was a GENERATOR -- the one body a C method cannot be. It is one` |
|         - | 3082 | ` * class in php too, wrapping whatever internal iterator the aggregate handed over,` |
|         - | 3083 | ` * so PHL gives it the same shape: fixed state slots on the iterator, and a vtable` |
|         - | 3084 | ` * on the AGGREGATE'S CLASS (ph7_class::pIterVtab, php's get_iterator handler) that` |
|         - | 3085 | ` * knows how to position and advance that aggregate's cursor. current()/key()/valid()` |
|         - | 3086 | ` * need no vtable entry -- they read the slots the two below leave behind.` |
|         - | 3087 | ` */` |
|         - | 3088 | `struct PH7_NativeIterVtab` |
|         - | 3089 | `{` |
|         - | 3090 | `	void (*xRewind)(ph7_vm *pVm,ph7_class_instance *pIt); /* settle on the first element */` |
|         - | 3091 | `	void (*xNext)(ph7_vm *pVm,ph7_class_instance *pIt);   /* settle on the one after */` |
|         - | 3092 | `	/* Optional: publish the cursor back onto the AGGREGATE, for a class that shows` |
|         - | 3093 | `	 * its walk as one of its own properties. DatePeriod is the case -- its` |
|         - | 3094 | ``	 * `current` is the cursor, and php writes it from every iterator method rather`` |
|         - | 3095 | ``	 * than from the walk itself, so `getIterator()` alone leaves it where the last`` |
|         - | 3096 | `	 * walk left it and the first valid()/current()/key()/rewind()/next() moves it.` |
|         - | 3097 | `	 * Called by the InternalIterator methods, never by the vtable's own halves. */` |
|         - | 3098 | `	void (*xPublish)(ph7_vm *pVm,ph7_class_instance *pIt);` |
|         - | 3099 | `	/* Optional: may this iterator be WALKED at all? Answered per call rather than` |
|         - | 3100 | `	 * once at creation because php refuses at the walk and not at the door --` |
|         - | 3101 | `	 * DatePeriod::getIterator() on an object nobody constructed hands back a real` |
|         - | 3102 | `	 * InternalIterator there, and the DateObjectError arrives at the first` |
|         - | 3103 | `	 * rewind(). Non-zero means the guard raised; the method then answers nothing` |
|         - | 3104 | `	 * and the host-call boundary reports the throw. */` |
|         - | 3105 | `	int (*xGuard)(ph7_context *pCtx,ph7_class_instance *pIt);` |
|         - | 3106 | `};` |
|         - | 3107 | `/* The state slots, private to InternalIterator and shared by every vtable:` |
|         - | 3108 | ` * the aggregate, the value and key at the cursor, an integer cursor and a spare` |
|         - | 3109 | ` * one for the vtable's own bookkeeping, and whether the walk is over. */` |
|         - | 3110 | `#define PH7_NATIVE_IT_SRC  "__src"` |
|         - | 3111 | `#define PH7_NATIVE_IT_CUR  "__cur"` |
|         - | 3112 | `#define PH7_NATIVE_IT_KEY  "__key"` |
|         - | 3113 | `#define PH7_NATIVE_IT_POS  "__pos"` |
|         - | 3114 | `#define PH7_NATIVE_IT_AUX  "__aux"` |
|         - | 3115 | `#define PH7_NATIVE_IT_DONE "__done"` |
|         - | 3116 | `PH7_PRIVATE sxi32 PH7_VmInstallNativeIterator(ph7_vm *pVm);` |
|         - | 3117 | `PH7_PRIVATE ph7_class_instance * PH7_NativeIteratorNew(ph7_vm *pVm,ph7_class_instance *pSrc);` |
|         - | 3118 | `/*` |
|         - | 3119 | ` * Each class method is parsed out and stored in an instance of the following` |
|         - | 3120 | ` * structure.` |
|         - | 3121 | ` * PH7 introduced some powerfull extensions to the PHP 5 programming` |
|         - | 3122 | ` * language like function overloading,type hinting,complex default` |
|         - | 3123 | ` * arguments and many more.` |
|         - | 3124 | ` * Please refer to the official documentation for more information.` |
|         - | 3125 | ` */` |
|         - | 3126 | `struct ph7_class_method` |
|         - | 3127 | `{` |
|         - | 3128 | `	ph7_vm_func sFunc;   /* Compiled method body */` |
|         - | 3129 | `	SyString sVmName;    /* Automatically generated name assigned to this method.` |
|         - | 3130 | `						  * Typically this is "[class_name__method_name@random_string]"` |
|         - | 3131 | `						  */` |
|         - | 3132 | `	sxi32 iProtection;   /* Protection level [i.e: public,private,protected] */` |
|         - | 3133 | `	sxi32 iFlags;        /* Methods configuration */` |
|         - | 3134 | `	sxi32 iCloneDepth;   /* Clone depth [Only used by the magic method __clone ] */` |
|         - | 3135 | `    sxu32 nLine;         /* Line on which this method was defined */` |
|         - | 3136 | `};` |
|         - | 3137 | `/*` |
|         - | 3138 | ` * Each active object (class instance) is represented by an instance of` |
|         - | 3139 | ` * the following structure.` |
|         - | 3140 | ` */` |
|         - | 3141 | `struct ph7_class_instance` |
|         - | 3142 | `{` |
|         - | 3143 | `	ph7_vm *pVm;        /* VM that own this instance */` |
|         - | 3144 | `	ph7_class *pClass;  /* Object is an instance of this class */` |
|         - | 3145 | `	SyHash hAttr;       /* Hashtable of active class members */` |
|         - | 3146 | `	sxi32 iRef;         /* Reference count */` |
|         - | 3147 | `	sxi32 iFlags;       /* Control flags */` |
|         - | 3148 | `	sxu32 nObjId;       /* Per-instance monotonic handle id (from pVm->nNextObjId,` |
|         - | 3149 | `	                     * never reused). Drives spl_object_id/hash + var_dump #N. */` |
|         - | 3150 | `	sxu32 nGcRoot;      /* 1-based row in the collector's root buffer, 0 while unbuffered */` |
|         - | 3151 | `	sxu8 iGcColor;      /* PH7_GC_* -- see vm_gc.c */` |
|         - | 3152 | `	PH7_AttrIter *pActiveIters; /* Walks of hAttr currently in flight over this object` |
|         - | 3153 | `	                     * (foreach, array_walk). A property removed under one of them` |
|         - | 3154 | `	                     * advances its cursor; one appended re-arms an exhausted one. */` |
|         - | 3155 | `};` |
|         - | 3156 | `/*` |
|         - | 3157 | ` * ph7_class_instance::iFlags bit set while the object's __clone() magic method` |
|         - | 3158 | ` * runs. PHP 8.3 lets __clone() re-initialize the cloned object's readonly` |
|         - | 3159 | ` * properties, so the readonly store guard consults this flag on the executing` |
|         - | 3160 | ` * $this. (Other iFlags bits are declared privately in their owning .c file:` |
|         - | 3161 | ` * 0x001 destroyed, 0x002 dumping, 0x004 fcc-bound.)` |
|         - | 3162 | ` */` |
|         - | 3163 | `#define CLASS_INSTANCE_DESTROYED 0x001 /* Instance is released (oo.c's teardown latch;` |
|         - | 3164 | `                                        * read by the cycle collector, which must not` |
|         - | 3165 | `                                        * walk a table being torn down) */` |
|         - | 3166 | `/* ph7_class_instance::iFlags bit: php's GC_PROTECT_RECURSION for an object -- the` |
|         - | 3167 | ` * instance counterpart of HASHMAP_DUMPING, and read through the same predicate. */` |
|         - | 3168 | `#define VM_INSTANCE_DUMPING 0x002` |
|         - | 3169 | `#define VM_INSTANCE_CLONING 0x008` |
|         - | 3170 | `/*` |
|         - | 3171 | ` * ph7_class_instance::iFlags bit: this object's __destruct has already been reached for` |
|         - | 3172 | ` * (or the refusal that stands in for it raised), so no later teardown may run it a second` |
|         - | 3173 | ` * time. php keeps the same bit (IS_OBJ_DESTRUCTOR_CALLED) for the same reason -- its` |
|         - | 3174 | ` * shutdown pass calls destructors on objects it does NOT free, and the free that follows` |
|         - | 3175 | ` * must not repeat them. Distinct from CLASS_INSTANCE_DESTROYED 0x001 (oo.c), which says` |
|         - | 3176 | ` * the whole instance is gone. 0x080 because 0x002..0x040 are claimed by unrelated readers` |
|         - | 3177 | ` * of this same word, each on its own kind of object.` |
|         - | 3178 | ` */` |
|         - | 3179 | `#define CLASS_INSTANCE_DTOR_CALLED 0x080` |
|         - | 3180 | `/*` |
|         - | 3181 | ` * ph7_class_instance::iFlags bit: php's GC_PROTECT_RECURSION for an object worn by the` |
|         - | 3182 | ` * COMPARISON walk -- the instance counterpart of HASHMAP_COMPARING, set on the LEFT` |
|         - | 3183 | ` * instance while PH7_ClassInstanceCmp walks its properties, exactly where` |
|         - | 3184 | ` * zend_std_compare_objects protects o1. An object that is its own descendant is refused` |
|         - | 3185 | ` * with php's "Nesting level too deep - recursive dependency?" instead of recursing` |
|         - | 3186 | ` * forever. 0x100 because 0x001..0x080 are claimed by unrelated readers of this word.` |
|         - | 3187 | ` */` |
|         - | 3188 | `#define VM_INSTANCE_COMPARING 0x100` |
|         - | 3189 | `/*` |
|         - | 3190 | ` * ph7_class_instance::iFlags bit set once this object's LAZY native properties` |
|         - | 3191 | ` * (PH7_CLASS_ATTR_NATIVE_LAZY) have been installed. It is the difference between` |
|         - | 3192 | ` * "the constructor has never run, so the name is not a property of this object at` |
|         - | 3193 | ` * all" and "the table exists and this one name was unset()" -- the first is php's` |
|         - | 3194 | ` * dynamic-property creation on a write and an undefined-property warning on a` |
|         - | 3195 | ` * read, the second re-creates the declared slot the ordinary way.` |
|         - | 3196 | ` */` |
|         - | 3197 | `#define VM_INSTANCE_LAZY_DONE 0x010` |
|         - | 3198 | `/*` |
|         - | 3199 | ` * Is this instance slot kept OUT of every surface that shows the object? Two` |
|         - | 3200 | ` * unrelated reasons say yes: the class calls it an engine slot` |
|         - | 3201 | ` * (PH7_CLASS_ATTR_HIDDEN) or this one OBJECT hides it (VM_CLASS_ATTR_UNSEEN).` |
|         - | 3202 | ` * Class-level members are not the object's either, so the one test covers all` |
|         - | 3203 | ` * three.` |
|         - | 3204 | ` */` |
|         - | 3205 | `#define PH7_ATTR_UNPRESENTED(pVmAttr) \` |
|         - | 3206 | `	(((pVmAttr)->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT \` |
|         - | 3207 | `	                              \|PH7_CLASS_ATTR_HIDDEN)) != 0 \` |
|         - | 3208 | `	 \|\| ((pVmAttr)->iState & VM_CLASS_ATTR_UNSEEN) != 0)` |
|         - | 3209 | `/*` |
|         - | 3210 | ` * Is this DECLARED attribute absent from the object because its class declares it` |
|         - | 3211 | ` * LAZILY and nothing has installed the set yet? The two miss paths -- a property` |
|         - | 3212 | ` * write and a by-reference bind -- ask before they re-create a declared slot.` |
|         - | 3213 | ` */` |
|         - | 3214 | `#define PH7_ATTR_LAZY_ABSENT(pAttr,pInst) \` |
|         - | 3215 | `	((((pAttr)->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY) != 0) \` |
|         - | 3216 | `	 && (((pInst)->iFlags & VM_INSTANCE_LAZY_DONE) == 0))` |
|         - | 3217 | `/*` |
|         - | 3218 | ` * A single instruction of the virtual machine has an opcode` |
|         - | 3219 | ` * and as many as three operands.` |
|         - | 3220 | ` * Each VM instruction resulting from compiling a PHP script` |
|         - | 3221 | ` * is stored in an instance of the following structure.` |
|         - | 3222 | ` */` |
|         - | 3223 | `struct VmInstr` |
|         - | 3224 | `{` |
|         - | 3225 | `	sxu8  iOp; /* Operation to preform */` |
|         - | 3226 | `	sxu8  bStrict; /* strict_types mode of the COMPILATION UNIT this instruction came from.` |
|         - | 3227 | `	                * Stamped by PH7_VmEmitInstr beside nLine, and published to` |
|         - | 3228 | `	                * pVm->bCurStrict under the same nLine != 0 gate, so an engine-dispatched` |
|         - | 3229 | `	                * call (a magic method, a property hook) can bind its arguments under the` |
|         - | 3230 | `	                * CALLING file's mode — php's rule — instead of always coercing. Sits in` |
|         - | 3231 | `	                * the padding after iOp: sizeof(VmInstr) is unchanged. */` |
|         - | 3232 | `	sxu8  bDiscard; /* PH7_OP_CALL only: php's !RETURN_VALUE_USED. The statement that owns` |
|         - | 3233 | ``	                * this call throws its answer away (`f();`, not `$x = f();` and not`` |
|         - | 3234 | ``	                * `f() + 1;`), which is the one thing a #[\NoDiscard] callee warns`` |
|         - | 3235 | `	                * about. Set by the codegen at the statement-discard site and cleared` |
|         - | 3236 | ``	                * by a `(void)` cast in front of it, which is php's way of saying the`` |
|         - | 3237 | `	                * drop is deliberate. Padding after bStrict, like bStrict itself. */` |
|         - | 3238 | `	sxu8  bRefSrc; /* PH7_OP_MEMBER only: this property fetch is a reference SOURCE --` |
|         - | 3239 | ``	                * php's `zend_compile_var(source, BP_VAR_W)`, the fetch a `=&` bind, a`` |
|         - | 3240 | ``	                * `[&$o->p]` element and a by-reference `foreach` make. It stays a`` |
|         - | 3241 | `	                * PH7_MEMBER_READ (a handler-backed property still hands back a COPY),` |
|         - | 3242 | `	                * but a MISSING name is CREATED rather than warned about, exactly as a` |
|         - | 3243 | `	                * write would create it. Padding after bDiscard, like bStrict itself. */` |
|         - | 3244 | `	sxi32 iP1; /* First operand */` |
|         - | 3245 | `	sxu32 iP2; /* Second operand (Often the jump destination) */` |
|         - | 3246 | `	sxu32 nAux; /* A per-instruction scratch word the RUNTIME owns, zero until it writes` |
|         - | 3247 | `	             * one. Two opcodes use it, each for an answer that cannot change under it:` |
|         - | 3248 | `	             *` |
|         - | 3249 | `	             *   PH7_OP_LOAD      the length of the variable NAME in p3. The name is a` |
|         - | 3250 | `	             *   PH7_OP_STORE     NUL-terminated compile-time buffer, and measuring it` |
|         - | 3251 | `	             *                    again on every execution was ~2% of a phpcs run.` |
|         - | 3252 | `	             *                    Written by VmNumberLocals for a body it walks, and` |
|         - | 3253 | `	             *                    lazily on first execution for one it does not.` |
|         - | 3254 | `	             *   PH7_OP_CALL_INIT the pVm->nCallableGen this call site was last screened` |
|         - | 3255 | `	             *                    at, written only when the callee is a compile-time` |
|         - | 3256 | `	             *                    constant (the push behind it is an OP_LOADC).` |
|         - | 3257 | `	             *   PH7_OP_LOADC     how many times this site has run, capped at two -- a` |
|         - | 3258 | `	             *                    site that runs once can never repay a cache record.` |
|         - | 3259 | `	             *   PH7_OP_CALL      the same count, for the same reason (VmCallSiteFor).` |
|         - | 3260 | `	             *` |
|         - | 3261 | `	             * Lives in the padding after iP2, so VmInstr is still 32 bytes and the` |
|         - | 3262 | `	             * bytecode costs nothing extra. */` |
|         - | 3263 | `	void *p3;  /* Third operand (Often Upper layer private data) */` |
|         - | 3264 | `	sxu32 nLine; /* Source line this instruction was compiled from (0 = unknown).` |
|         - | 3265 | `	              * Stamped by PH7_VmEmitInstr from the codegen's current token, so` |
|         - | 3266 | `	              * every one of its ~150 call sites keeps its signature. */` |
|         - | 3267 | `	sxu32 nSite; /* Two opcodes' worth of "this SITE already knows the answer", sharing one` |
|         - | 3268 | `	              * word because no instruction is ever both.` |
|         - | 3269 | `	              *` |
|         - | 3270 | `	              *   PH7_OP_CALL      this site's entry in pVm->aCallSite, plus one (0 = it` |
|         - | 3271 | `	              *   PH7_OP_LOADC     has never asked for one). A CALL remembers which` |
|         - | 3272 | `	              *                    function table entry its callee NAME resolved to; a` |
|         - | 3273 | `	              *                    LOADC which hConstant entry its constant name did.` |
|         - | 3274 | `	              *                    Both are a name hashed once instead of once per` |
|         - | 3275 | `	              *                    execution -- see VmCallSite.` |
|         - | 3276 | `	              *   PH7_OP_LOAD      the NUMBER this body gave the variable in p3, plus` |
|         - | 3277 | `	              *   PH7_OP_STORE     one (0 = the body was never numbered, or the name` |
|         - | 3278 | `	              *                    did not fit PH7_VAR_SLOT_MAX). It indexes the running` |
|         - | 3279 | `	              *                    frame's aLocalSlot -- see VmNumberLocals.` |
|         - | 3280 | `	              *` |
|         - | 3281 | `	              * Lives in the trailing padding after nLine, so VmInstr is still 32 bytes. */` |
|         - | 3282 | `};` |
|         - | 3283 | `/*` |
|         - | 3284 | ` * Named-argument metadata attached to PH7_OP_CALL instructions via p3.` |
|         - | 3285 | ` * Also carries the namespace-qualification flag formerly stored as p3=(void*)1.` |
|         - | 3286 | ` */` |
|         - | 3287 | `struct VmCallArgMap` |
|         - | 3288 | `{` |
|         - | 3289 | `	sxu8 bHasNamed;      /* 1 if any argument uses name: syntax */` |
|         - | 3290 | `	sxu8 bFromUnpack;    /* 1 when this is the EFFECTIVE map an argument UNPACK` |
|         - | 3291 | `	                      * produced (VmBuildEffectiveArgMap). php words its` |
|         - | 3292 | `	                      * positional-after-named refusal with a trailing` |
|         - | 3293 | ``	                      * ` during unpacking` only there; the same rule broken by`` |
|         - | 3294 | `	                      * call_user_func_array's array gets the bare sentence. */` |
|         - | 3295 | `	sxu8 bIsNamespaced;  /* 1 if compiler namespace-qualified the call */` |
|         - | 3296 | `	sxu8 bStrict;        /* 1 if the call site's file declared strict_types=1 */` |
|         - | 3297 | `	sxu32 nOrigNameLit;  /* Original (unqualified) name-literal index + 1, stored` |
|         - | 3298 | `						  * when the CALL handler namespace-qualified the name so` |
|         - | 3299 | `						  * a following NEW can re-qualify with CLASS imports.` |
|         - | 3300 | `						  * 0 = unset. (Formerly abused OP_CALL's iP2, colliding` |
|         - | 3301 | ``						  * with the hasSpread flag: `new N\C(...$args)`.) */`` |
|         - | 3302 | `	sxu32 nNewClassInstr;/* Instruction index + 1 of the class-name push, for a call node` |
|         - | 3303 | ``	                      * that is a `new`'s operand. The reorder puts that push before`` |
|         - | 3304 | `	                      * the constructor arguments, so the NEW codegen can no longer` |
|         - | 3305 | `	                      * find it by peeking one instruction back. 0 = unset. */` |
|         - | 3306 | `	sxu8 bArgShapes;     /* 1 when the two masks below describe THIS call's argument` |
|         - | 3307 | `						  * positions. The compiler sets it for every call whose actual` |
|         - | 3308 | `						  * positions survive to the runtime stack unchanged — i.e. no` |
|         - | 3309 | `						  * spread and at most 31 arguments. 0 means "unknown shapes":` |
|         - | 3310 | `						  * the by-ref binders fall back to their runtime nIdx test. */` |
|         - | 3311 | `	sxu32 nNonLvalMask;  /* bit N: argument N is a HARD non-lvalue (a literal, an operator` |
|         - | 3312 | ``						  * result, a cast, a class constant, `@$x`, `$o?->p`, an assignment`` |
|         - | 3313 | ``						  * — php's `zend_is_variable` says no and it is not a call either).`` |
|         - | 3314 | `						  * Binding one to a by-ref parameter is php's catchable` |
|         - | 3315 | ``						  * `Argument #N ($p) could not be passed by reference` Error, raised`` |
|         - | 3316 | `						  * at the CALL before the callee's ZPP runs. */` |
|         - | 3317 | ``	sxu32 nTempCallMask; /* bit N: argument N is the RESULT of a call or a `new` — php's`` |
|         - | 3318 | `						  * SEND_VAR_NO_REF: an E_NOTICE ("Only variables should be passed` |
|         - | 3319 | `						  * by reference") and then it operates on the temporary. */` |
|         - | 3320 | `	sxu32 nTotal;        /* Total number of compile-time arguments */` |
|         - | 3321 | `	SyString *aNames;    /* Array of nTotal names. nByte==0 means positional. */` |
|         - | 3322 | `	SyString sAssertSrc; /* Direct assert() calls only: the first argument's rendered` |
|         - | 3323 | ``						  * source text (php's zend_ast_export shape, e.g. `1 == 2`),`` |
|         - | 3324 | `						  * captured at compile time so a failing assertion reports` |
|         - | 3325 | ``						  * `assert(1 == 2)` like php instead of the evaluated value.`` |
|         - | 3326 | `						  * {0,0} for every other call site; bytes live in the VM` |
|         - | 3327 | `						  * allocator. See PH7_GenRenderAssertSpan (compile_literal.c). */` |
|         - | 3328 | `};` |
|         - | 3329 | `/*` |
|         - | 3330 | ` * A class declaration whose parent/interface/trait could not be resolved at` |
|         - | 3331 | ` * compile time (the enclosing file's statements — spl_autoload_register — had` |
|         - | 3332 | ` * not executed yet). The compiler captures the declaration's SOURCE plus a` |
|         - | 3333 | ` * reconstructed namespace/use-import prefix and defers the whole compile to` |
|         - | 3334 | ` * OP_CLASS_DEFER at the declaration's execution point (VmExecDeferredClass,` |
|         - | 3335 | ` * vm_include.c), where the autoloader is live. aRequired lists the names that` |
|         - | 3336 | ` * were missing; each still-missing one throws php's catchable` |
|         - | 3337 | `` * `Class/Interface/Trait "X" not found` Error before the re-compile runs.`` |
|         - | 3338 | ` */` |
|         - | 3339 | `typedef struct VmDeferredReq VmDeferredReq;` |
|         - | 3340 | `struct VmDeferredReq` |
|         - | 3341 | `{` |
|         - | 3342 | `	SyString sName;  /* Fully-qualified name (allocator-owned) */` |
|         - | 3343 | `	sxu8 cKind;      /* PH7_DEFER_KIND_* — picks the not-found noun */` |
|         - | 3344 | `};` |
|         - | 3345 | `#define PH7_DEFER_KIND_CLASS     0` |
|         - | 3346 | `#define PH7_DEFER_KIND_INTERFACE 1` |
|         - | 3347 | `#define PH7_DEFER_KIND_TRAIT     2` |
|         - | 3348 | `typedef struct VmDeferredClass VmDeferredClass;` |
|         - | 3349 | `struct VmDeferredClass` |
|         - | 3350 | `{` |
|         - | 3351 | `	SyString sText;     /* Re-compilable chunk: namespace + use-imports prefix +` |
|         - | 3352 | ``						 * the declaration source (anon: wrapped in `if (false) { new ... }`) */`` |
|         - | 3353 | `	SyString sSelfName; /* FQN the compile must install (anon: the synthesized name) —` |
|         - | 3354 | `						 * the post-eval existence check */` |
|         - | 3355 | `	SyString sAnonName; /* Synthesized anonymous-class name to inject via` |
|         - | 3356 | `						 * pVm->sDeferAnonName ({0,0} for a named declaration) */` |
|         - | 3357 | `	SySet aRequired;    /* VmDeferredReq — names unresolved at compile time */` |
|         - | 3358 | `	sxu32 nLine;        /* Declaration line (diagnostics) */` |
|         - | 3359 | `	sxu8 bDone;         /* 1 once the declaration executed successfully (idempotent site) */` |
|         - | 3360 | `};` |
|         - | 3361 | `/* Each active class instance attribute is represented by an instance` |
|         - | 3362 | ` * of the following structure.` |
|         - | 3363 | ` */` |
|         - | 3364 | `typedef struct VmClassAttr VmClassAttr;` |
|         - | 3365 | `/*` |
|         - | 3366 | ` * One property SLOT on one object -- the engine's per-instance record, and the` |
|         - | 3367 | ` * engine's single most numerous heap object after the array node: 128,024 of them` |
|         - | 3368 | ` * live at the peak of the ecosystem gate's phpcs step.` |
|         - | 3369 | ` *` |
|         - | 3370 | ` * It carries ONE holder pointer, not two. It used to name both the instance the` |
|         - | 3371 | ` * slot belongs to and the class that owns the property, and the second was` |
|         - | 3372 | ` * derivable from the first at every site that ever set it: an instance record is` |
|         - | 3373 | `` * built by PH7_VmCreateClassInstanceFrame, whose `pClass` IS `pObj->pClass`, and`` |
|         - | 3374 | `` * the two dynamic-property doors write `pThis->pClass` beside `pThis`. The only`` |
|         - | 3375 | ` * records that are not an instance's are the class's own statics, which have no` |
|         - | 3376 | ` * instance at all -- so one pointer says both, and VM_CLASS_ATTR_CLASSHELD says` |
|         - | 3377 | ` * which kind it is.` |
|         - | 3378 | ` *` |
|         - | 3379 | ` * That is not a tidy-up, it is a bucket. The struct was 32 bytes, and a pool` |
|         - | 3380 | ` * request of 32 needs 40 with its header and so lands in the 64-byte bucket --` |
|         - | 3381 | ` * 7.81 MB at peak against 3.91 MB asked for, the census's only row of 100%` |
|         - | 3382 | ` * rounding waste. At 24 it needs 32 exactly and lands in the 32-byte bucket.` |
|         - | 3383 | ` * KEEP IT AT 24 BYTES: one more field of any size doubles this row again.` |
|         - | 3384 | ` *` |
|         - | 3385 | ` * Read it through PH7_VmAttrOwner / PH7_VmAttrInst below; nothing outside them` |
|         - | 3386 | ` * should touch pHolder.` |
|         - | 3387 | ` */` |
|         - | 3388 | `struct VmClassAttr` |
|         - | 3389 | `{` |
|         - | 3390 | `	ph7_class_attr *pAttr; /* Class attribute */` |
|         - | 3391 | `	void *pHolder;         /* The ph7_class_instance this slot belongs to, or -- when` |
|         - | 3392 | `	                        * VM_CLASS_ATTR_CLASSHELD is set -- the ph7_class whose own` |
|         - | 3393 | `	                        * static it is. The store filter reaches the instance for` |
|         - | 3394 | `	                        * ph7_class::xSet, which is a handler ON AN OBJECT (php's` |
|         - | 3395 | `	                        * write_property takes the object); DateInterval's writes its` |
|         - | 3396 | `	                        * own microsecond slot from there. The record lives in the` |
|         - | 3397 | `	                        * holder's hAttr and dies with it, so this never outlives` |
|         - | 3398 | `	                        * what it names. */` |
|         - | 3399 | `	sxu32 nIdx;            /* Memory object index */` |
|         - | 3400 | `	sxi32 iState;          /* Per-instance state: VM_CLASS_ATTR_UNINIT */` |
|         - | 3401 | `};` |
|         - | 3402 | `#define VM_CLASS_ATTR_UNINIT  0x01 /* Typed property never written (PHP 7.4+); also the` |
|         - | 3403 | `                                    * write-once latch for readonly properties (cleared on` |
|         - | 3404 | `                                    * the first successful write — see VmEnforcePropertyTypeOnStore) */` |
|         - | 3405 | `#define VM_CLASS_ATTR_RDONLY  0x08 /* php's read-only handler property, marked on the INSTANCE:` |
|         - | 3406 | ``                                    * a plain store and an unset() refuse with `Property p is`` |
|         - | 3407 | ``                                    * read only` while every path that takes a POINTER to it`` |
|         - | 3408 | ``                                    * goes through (a compound assign, `++`, `??=`, a`` |
|         - | 3409 | `                                    * reference bind). It is per-OBJECT rather than per-class` |
|         - | 3410 | `                                    * because php's own handler is: a PDOStatement nobody` |
|         - | 3411 | `                                    * built a cursor for takes the write, and only one` |
|         - | 3412 | `                                    * carrying a statement refuses. */` |
|         - | 3413 | `#define VM_CLASS_ATTR_UNSEEN  0x10 /* Per-INSTANCE presentation hide: the slot reads, writes and` |
|         - | 3414 | `                                    * answers isset() the way it always did, and every surface` |
|         - | 3415 | `                                    * that SHOWS an object -- var_dump/print_r/var_export, the` |
|         - | 3416 | `                                    * (array) cast, get_object_vars, foreach, json_encode,` |
|         - | 3417 | `                                    * serialize, http_build_query and Reflection's object dump` |
|         - | 3418 | `                                    * -- walks past it. php's from-string DateInterval is the` |
|         - | 3419 | ``                                    * case: it answers `$i->d` from the string it kept while`` |
|         - | 3420 | ``                                    * presenting `from_string` and `date_string` alone. */`` |
|         - | 3421 | `#define VM_CLASS_ATTR_TYPE_DEFER 0x04 /* Typed STATIC property whose eagerly-evaluated DEFAULT failed` |
|         - | 3422 | `                                       * its type check at class mount. php evaluates static defaults` |
|         - | 3423 | `                                       * lazily, so the failure is deferred: any static-property access` |
|         - | 3424 | `                                       * on the class (read/write/isset, any property) and any` |
|         - | 3425 | `                                       * instantiation throws the catchable "Cannot assign <kind> to` |
|         - | 3426 | `                                       * property C::$s of type T" TypeError; a never-touched class` |
|         - | 3427 | `                                       * stays silent. Raised by PH7_VmMaterializeClassStatics, which` |
|         - | 3428 | `                                       * also sets the flag when a DEFERRED default (the sibling` |
|         - | 3429 | `                                       * PH7_CLASS_ATTR_STATIC_DEFER) evaluates at first access and` |
|         - | 3430 | `                                       * only then fails its type check. */` |
|         - | 3431 | ``#define VM_CLASS_ATTR_REFBOUND 0x02 /* Property is bound to a reference (`$o->p =& $x`): its nIdx`` |
|         - | 3432 | `                                    * slot is SHARED with (and pinned by) the source variable, so` |
|         - | 3433 | `                                    * PH7_VmReleaseInstanceAttr must NOT release/recycle it — the` |
|         - | 3434 | `                                    * surviving alias would dangle. Mirrors the use(&$x) pin. */` |
|         - | 3435 | `#define VM_CLASS_ATTR_CLASSHELD 0x40 /* pHolder is the ph7_class whose STATIC this slot is,` |
|         - | 3436 | `                                     * not a ph7_class_instance. A static belongs to the class` |
|         - | 3437 | `                                     * and is filed in every instance's hAttr as well, so the` |
|         - | 3438 | `                                     * record has no instance behind it -- which is exactly` |
|         - | 3439 | `                                     * what "no instance" used to be spelled as a NULL pInst` |
|         - | 3440 | `                                     * beside a non-NULL pOwner. */` |
|         - | 3441 | ``#define VM_CLASS_ATTR_REFSRCPIN 0x20 /* Property is the SOURCE of a reference (`$r =& $o->p`,`` |
|         - | 3442 | ``                                     * `$q->p =& $o->p`, `foreach ($o->p as &$v)`): php makes both`` |
|         - | 3443 | `                                     * ends references, and the other end pins the slot. A property` |
|         - | 3444 | `                                     * is not a holder the reference table can NAME, so the slot's` |
|         - | 3445 | `                                     * only recorded holder was that pin -- and when the other end` |
|         - | 3446 | `                                     * died, the unpin freed the value out from under THIS property,` |
|         - | 3447 | `                                     * which then read NULL. The bit says this property holds one` |
|         - | 3448 | `                                     * counted pin of its own, given back when it is released. */` |
|         - | 3449 | ` /*` |
|         - | 3450 | ` * The two questions the one holder pointer answers.` |
|         - | 3451 | ` *` |
|         - | 3452 | ` * PH7_VmAttrInst -- the object this slot sits on, or 0 for a class static, which is` |
|         - | 3453 | ` * the NULL pInst every caller used to test for.` |
|         - | 3454 | ` * PH7_VmAttrOwner -- the class that owns the property, for the diagnostics in` |
|         - | 3455 | ` * vm_error.c. For an instance record that is the instance's own class, which is what` |
|         - | 3456 | `` * the frame builder wrote there by hand (its `pClass` is `pObj->pClass`); for a`` |
|         - | 3457 | ` * static it is the holder itself.` |
|         - | 3458 | ` */` |
|   1476152 | 3459 | `SX_STATIC_INLINE ph7_class_instance * PH7_VmAttrInst(const VmClassAttr *pVmAttr)` |
|         5 | 3460 | `{` |
|   1476157 | 3461 | `	if( pVmAttr->iState & VM_CLASS_ATTR_CLASSHELD ){` |
|        65 | 3462 | `		return 0;` |
|         - | 3463 | `	}` |
|   1476093 | 3464 | `	return (ph7_class_instance *)pVmAttr->pHolder;` |
|    738081 | 3465 | `}` |
|    201566 | 3466 | `SX_STATIC_INLINE ph7_class * PH7_VmAttrOwner(const VmClassAttr *pVmAttr)` |
|         5 | 3467 | `{` |
|    201571 | 3468 | `	if( pVmAttr->iState & VM_CLASS_ATTR_CLASSHELD ){` |
|        69 | 3469 | `		return (ph7_class *)pVmAttr->pHolder;` |
|         - | 3470 | `	}` |
|    201507 | 3471 | `	return pVmAttr->pHolder ? ((ph7_class_instance *)pVmAttr->pHolder)->pClass : 0;` |
|    100788 | 3472 | `}` |
|         - | 3473 | `/* The two ways a record is filed. Both leave iState's other bits alone, so a caller` |
|         - | 3474 | ` * may set its state before or after saying who holds the slot. */` |
|  10463933 | 3475 | `SX_STATIC_INLINE void PH7_VmAttrSetInst(VmClassAttr *pVmAttr,ph7_class_instance *pInst)` |
|         5 | 3476 | `{` |
|  10463938 | 3477 | `	pVmAttr->pHolder = (void *)pInst;` |
|  10463938 | 3478 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_CLASSHELD;` |
|  10463938 | 3479 | `}` |
|      1446 | 3480 | `SX_STATIC_INLINE void PH7_VmAttrSetClass(VmClassAttr *pVmAttr,ph7_class *pClass)` |
|         5 | 3481 | `{` |
|      1451 | 3482 | `	pVmAttr->pHolder = (void *)pClass;` |
|      1451 | 3483 | `	pVmAttr->iState \|= VM_CLASS_ATTR_CLASSHELD;` |
|      1451 | 3484 | `}` |
|         - | 3485 | `/* Forward reference */` |
|         - | 3486 | `typedef struct VmSlot VmSlot;` |
|         - | 3487 | `struct VmSlot` |
|         - | 3488 | `{` |
|         - | 3489 | `	sxu32 nIdx;      /* Index in pVm->aMemObj[] */` |
|         - | 3490 | `	void *pUserData; /* Upper-layer private data */` |
|         - | 3491 | `};` |
|         - | 3492 | `/*` |
|         - | 3493 | ` * The segmented memory-object table.` |
|         - | 3494 | ` *` |
|         - | 3495 | ` * aMemObj used to be one doubling SySet: every value pointer died on any growth,` |
|         - | 3496 | ` * the doubling realloc moved a whole 33 MB block, and the buffer never shrank.` |
|         - | 3497 | ` * Here the value slots live in FIXED-SIZE segments (VM_MEMPOOL_SEG_SLOTS each,` |
|         - | 3498 | ` * one pool allocation per segment), addressed by the same flat nIdx the engine` |
|         - | 3499 | ` * has always carried -- the split is a shift and a mask here, in the accessor,` |
|         - | 3500 | ` * so every caller's index means what it always meant. A value's address never` |
|         - | 3501 | ` * moves, growth appends a segment instead of copying the table, and a fully-free` |
|         - | 3502 | ` * trailing segment is handed back on truncate.` |
|         - | 3503 | ` *` |
|         - | 3504 | ` * SEGMENT SIZE is a floor, not just a granularity: the first segment is` |
|         - | 3505 | ` * allocated when the VM is, so every VM pays for one whether it holds three` |
|         - | 3506 | ` * values or three hundred thousand. 256 slots is 16 KB, which is what the` |
|         - | 3507 | ` * SySetAlloc(&pVm->aMemObj,0xFF) this replaced opened with -- deliberately, so` |
|         - | 3508 | ` * that segmenting the table did not raise the per-VM floor. It matters in two` |
|         - | 3509 | ` * places that are not this box: the -S server caches PHL_VM_CACHE_SIZE (16) VMs,` |
|         - | 3510 | ` * so the floor is paid sixteen times, and on ESP32-S3 internal RAM dips to 32 KB` |
|         - | 3511 | ` * free, where a 256 KB opening allocation is not a cost but a failure.` |
|         - | 3512 | ` * The price of a small segment is one direct block and one segment-table entry` |
|         - | 3513 | ` * per 256 slots: at the phpcs peak of record (~356K slots) that is ~1,400` |
|         - | 3514 | ` * segments, ~33 KB of allocator headers and a 2,048-entry pointer table -- under` |
|         - | 3515 | ` * 0.04% of the peak. Override with -DPH7_VM_MEMPOOL_SEG_SHIFT=n for a target` |
|         - | 3516 | ` * that wants a different trade; nothing but the two constants below depends on it.` |
|         - | 3517 | ` *` |
|         - | 3518 | ` * Freed slots form a single INTRUSIVE free list threaded through the slots'` |
|         - | 3519 | ` * own (dead) nIdx word -- VmMemPoolFreeSlot writes the link into the slot, so` |
|         - | 3520 | ` * slot reuse is O(1) and costs zero extra memory. nFreeHead is the head, or` |
|         - | 3521 | ` * SXU32_HIGH when the list is empty. This replaced the aFreeObj SySet, which` |
|         - | 3522 | ` * grew one 16-byte VmSlot per free index to describe exactly what the link now` |
|         - | 3523 | ` * describes for free. Because the link lives INSIDE the slot, a slot on the` |
|         - | 3524 | ` * list carries MEMOBJ_POOLFREE: a second free of the same index would otherwise` |
|         - | 3525 | ` * write the head into the slot the head already points at, and every later` |
|         - | 3526 | ` * reserve would hand out that one slot forever. The old stack merely handed the` |
|         - | 3527 | ` * index out twice and drained; this one would not.` |
|         - | 3528 | ` */` |
|         - | 3529 | `#ifndef PH7_VM_MEMPOOL_SEG_SHIFT` |
|         - | 3530 | `#define PH7_VM_MEMPOOL_SEG_SHIFT 8` |
|         - | 3531 | `#endif` |
|         - | 3532 | `#define VM_MEMPOOL_SEG_SHIFT  PH7_VM_MEMPOOL_SEG_SHIFT` |
|         - | 3533 | `#define VM_MEMPOOL_SEG_SLOTS  (1u << VM_MEMPOOL_SEG_SHIFT)` |
|         - | 3534 | `#define VM_MEMPOOL_SEG_MASK   (VM_MEMPOOL_SEG_SLOTS - 1u)` |
|         - | 3535 | `typedef struct VmMemPool VmMemPool;` |
|         - | 3536 | `struct VmMemPool` |
|         - | 3537 | `{` |
|         - | 3538 | `	SyMemBackend *pAllocator; /* Memory backend the segments come from */` |
|         - | 3539 | `	ph7_value   **apSeg;      /* Segment pointer table (VM_MEMPOOL_SEG_SLOTS slots each) */` |
|         - | 3540 | `	sxu32         nSeg;       /* Segments currently allocated */` |
|         - | 3541 | `	sxu32         nCap;       /* Capacity of apSeg */` |
|         - | 3542 | `	sxu32         nUsed;      /* Logical slots in use -- SySetUsed(aMemObj) semantics */` |
|         - | 3543 | `	sxu32         nFreeHead;  /* First free slot, or SXU32_HIGH; freed slots chain through their nIdx */` |
|         - | 3544 | `};` |
|         - | 3545 | `/*` |
|         - | 3546 | ` * The nIdx'th slot of the pool, or NULL when the index is past the end -- the` |
|         - | 3547 | ` * same bounds contract SySetAt kept on the set this replaces, so a call site` |
|         - | 3548 | ` * that leaned on NULL for "that index has not been allocated" still works.` |
|         - | 3549 | ` * INLINE because this is the engine's hottest read: every array element,` |
|         - | 3550 | ` * property and variable is reached through it -- which is also why the shift and` |
|         - | 3551 | ` * the mask are the COMPILE-TIME constants and not fields of the pool. They can` |
|         - | 3552 | ` * only ever hold these two values, and reading them out of the struct would put` |
|         - | 3553 | ` * two loads and a variable shift on every value access to say what an immediate` |
|         - | 3554 | ` * already says.` |
|         - | 3555 | ` */` |
| 115450720 | 3556 | `SX_STATIC_INLINE ph7_value * PH7_MemObjAt(VmMemPool *pPool,sxu32 nIdx)` |
|         5 | 3557 | `{` |
| 115450725 | 3558 | `	if( nIdx >= pPool->nUsed ){` |
|       138 | 3559 | `		return 0;   /* Out of range */` |
|         - | 3560 | `	}` |
| 115450591 | 3561 | `	return &pPool->apSeg[nIdx >> VM_MEMPOOL_SEG_SHIFT][nIdx & VM_MEMPOOL_SEG_MASK];` |
|  57732058 | 3562 | `}` |
|         - | 3563 | `/*` |
|         - | 3564 | ` * Cycle-collector colours (vm_gc.c). php's, and Bacon & Rajan's before it.` |
|         - | 3565 | ` * BLACK is "in use", GREY "being trial-deleted", WHITE "counted zero",` |
|         - | 3566 | ` * PURPLE "buffered as a possible root", DEAD "proved garbage, being freed".` |
|         - | 3567 | ` * A container is born BLACK because its struct is zeroed.` |
|         - | 3568 | ` */` |
|         - | 3569 | `#define PH7_GC_BLACK   0` |
|         - | 3570 | `#define PH7_GC_GREY    1` |
|         - | 3571 | `#define PH7_GC_WHITE   2` |
|         - | 3572 | `#define PH7_GC_PURPLE  3` |
|         - | 3573 | `#define PH7_GC_DEAD    4` |
|         - | 3574 | `typedef struct VmGcRef VmGcRef;` |
|         - | 3575 | `/* One container, in the root buffer or in a traversal worklist. */` |
|         - | 3576 | `struct VmGcRef` |
|         - | 3577 | `{` |
|         - | 3578 | `	void *pPtr;  /* ph7_hashmap * or ph7_class_instance *; 0 once the row is spent */` |
|         - | 3579 | `	sxu8 bMap;   /* which of the two it is */` |
|         - | 3580 | `};` |
|         - | 3581 | `typedef struct VmRefObj VmRefObj;` |
|         - | 3582 | `typedef struct VmRefSpill VmRefSpill;` |
|         - | 3583 | `/*` |
|         - | 3584 | ` * The SECOND and later holder of each kind. Allocated only for a slot that really` |
|         - | 3585 | `` * has two names on it, or two array nodes -- which is what a PHP `&` reference is,`` |
|         - | 3586 | ` * and which almost no slot is: an ordinary variable is named once and an ordinary` |
|         - | 3587 | ` * array element is pointed at by one node. Every slot used to carry both of these` |
|         - | 3588 | ` * sets inline (80 bytes) plus the 32-byte buffer each grew on its first row, so the` |
|         - | 3589 | ` * engine paid a reference's price for every variable and every element it created.` |
|         - | 3590 | ` */` |
|         - | 3591 | `struct VmRefSpill` |
|         - | 3592 | `{` |
|         - | 3593 | `	SySet aReference;  /* Holders beyond pEntry0 */` |
|         - | 3594 | `	SySet aArrEntries; /* Holders beyond pNode0 */` |
|         - | 3595 | `};` |
|         - | 3596 | `/* Reference-object body (vm.c reference machinery; shared with vm_builtin_var.c's unset).` |
|         - | 3597 | ` *` |
|         - | 3598 | ` * A record is the FALLBACK shape, not the ordinary one: apRefObj[nIdx] is a tagged` |
|         - | 3599 | ` * WORD (see VM_REF_TAG_* below) and only a slot whose answer will not fit in one` |
|         - | 3600 | ` * ever allocates this. */` |
|         - | 3601 | `struct VmRefObj` |
|         - | 3602 | `{` |
|         - | 3603 | `	SyHashEntry *pEntry0;     /* The one name bound to this slot; 0 once it is gone */` |
|         - | 3604 | `	ph7_hashmap_node *pNode0; /* The one array node pointing here; 0 once it is gone */` |
|         - | 3605 | `	VmRefSpill *pSpill;       /* The 2nd..nth holder of either kind; 0 while there is none */` |
|         - | 3606 | `	sxu32 nIdx;        /* Referenced object index -- also this record's cell in apRefObj[] */` |
|         - | 3607 | `	sxu32 nPin;        /* Holders the table cannot name, COUNTED so the last one to go` |
|         - | 3608 | `	                    * can release the slot: reference-bound properties (one per` |
|         - | 3609 | `	                    * binding, dropped when the property is released or re-bound).` |
|         - | 3610 | `	                    * A slot pinned by a site that never unpins (a use(&$x) capture,` |
|         - | 3611 | `	                    * a static, an enum case) leaves this 0 and relies on the` |
|         - | 3612 | `	                    * VM_REF_IDX_KEEP flag alone, which is a permanent pin. */` |
|         - | 3613 | `	sxi32 iFlags;      /* Configuration flags */` |
|         - | 3614 | `};` |
|         - | 3615 | `#define VM_REF_IDX_KEEP  0x001 /* Do not restore the memory object to the free list */` |
|         - | 3616 | `/*` |
|         - | 3617 | ` * apRefObj[nIdx] is ONE TAGGED WORD, not a pointer to a record.` |
|         - | 3618 | ` *` |
|         - | 3619 | ` * What the reference table has to say about the ordinary slot is one sentence long --` |
|         - | 3620 | ` * "this name holds it", "this array node points at it", "the object that declares it` |
|         - | 3621 | ` * holds it" -- and a whole heap record to say it is the engine's single largest` |
|         - | 3622 | ` * per-value cost. The word says the sentence itself; a record is allocated only when` |
|         - | 3623 | ` * the answer needs more than one holder, which a census of the ecosystem gate's phpcs` |
|         - | 3624 | ` * step puts at 14 of the 360264 records live at peak.` |
|         - | 3625 | ` *` |
|         - | 3626 | ` *   0                          nothing has ever been registered against the slot` |
|         - | 3627 | ` *   pEntry \| VM_REF_TAG_NAME   exactly ONE holder: the name bound to the slot` |
|         - | 3628 | ` *   pNode  \| VM_REF_TAG_NODE   exactly ONE holder: the array node pointing at it` |
|         - | 3629 | ` *   bits   \| VM_REF_TAG_MARK   registered, NO NAMED holder; the rest of the word is` |
|         - | 3630 | ` *                              the pin count and the flags (this is both the spent` |
|         - | 3631 | ` *                              record a dropped holder leaves behind -- which is what` |
|         - | 3632 | ` *                              still returns the slot to the free pool -- and the` |
|         - | 3633 | ` *                              declared property's own VM_REF_IDX_KEEP)` |
|         - | 3634 | ` *   pRef   (tag 0, non-zero)   a VmRefObj *: two or more holders, or a pin beside one` |
|         - | 3635 | ` *` |
|         - | 3636 | ` * The two pointer tags ride in the low bits of a pool-allocated address; the allocator` |
|         - | 3637 | ` * keeps every chunk 8-aligned (see the alignment note on sxmem.c's OS methods), and a` |
|         - | 3638 | ` * pointer that is not 4-aligned falls back to a record rather than being tagged.` |
|         - | 3639 | ` */` |
|         - | 3640 | `#define VM_REF_TAG_MASK    3` |
|         - | 3641 | `#define VM_REF_TAG_FULL    0  /* a VmRefObj * */` |
|         - | 3642 | `#define VM_REF_TAG_NAME    1  /* a SyHashEntry * */` |
|         - | 3643 | `#define VM_REF_TAG_NODE    2  /* a ph7_hashmap_node * */` |
|         - | 3644 | `#define VM_REF_TAG_MARK    3  /* no pointer: pin count and flags in the upper bits */` |
|         - | 3645 | `#define VM_REF_MARK_KEEP   0x4        /* bit 2 of a MARK word: VM_REF_IDX_KEEP */` |
|         - | 3646 | `#define VM_REF_MARK_PIN    0x8        /* bit 3 and up: the counted pin */` |
|         - | 3647 | `#define VM_REF_MARK_PINMAX 0x0FFFFFFF /* a pin count past this promotes to a record */` |
|         - | 3648 | `/* The tag of a word. Only the low two bits are read, so the cast may narrow. */` |
|         - | 3649 | `#define VM_REF_TAGOF(W)    (SX_PTR_TO_INT(W) & VM_REF_TAG_MASK)` |
|         - | 3650 | `/* VmObEntry struct moved to ph7int.h */` |
|         - | 3651 |  |
|         - | 3652 | `/*` |
|         - | 3653 | ` * Each catch [i.e catch(Exception $e){ } ] block is parsed out and stored` |
|         - | 3654 | ` * in an instance of the following structure.` |
|         - | 3655 | ` */` |
|         - | 3656 | `typedef struct ph7_exception_block ph7_exception_block;` |
|         - | 3657 | `typedef struct ph7_exception ph7_exception;` |
|         - | 3658 | `struct ph7_exception_block` |
|         - | 3659 | `{` |
|         - | 3660 | `	SySet aClasses;  /* Exception class names (SyString instances) for multi-catch */` |
|         - | 3661 | `	SyString sThis;  /* Instance name [i.e: $e..] */` |
|         - | 3662 | `	SySet *pByteCode;/* Compiled instructions of a DETACHED catch body (the path every` |
|         - | 3663 | `	                  * non-generator try takes; NULL for a ROOT C inline catch, which compiles` |
|         - | 3664 | `	                  * into the function's own array). Heap-allocated so its ADDRESS is stable:` |
|         - | 3665 | ``	                  * a `break`/`continue` inside the body records this container in its`` |
|         - | 3666 | `	                  * JumpFixup, resolved long after PH7_CompileCatch returned and after` |
|         - | 3667 | `	                  * sEntry grew (both would move an embedded SySet). */` |
|         - | 3668 | `	sxu32 iHandlerPc;/* ROOT C: inline PC where this catch body begins (0 = not inlined) */` |
|         - | 3669 | `};` |
|         - | 3670 | `/*` |
|         - | 3671 | ` * Context for the exception mechanism.` |
|         - | 3672 | ` */` |
|         - | 3673 | `struct ph7_exception` |
|         - | 3674 | `{` |
|         - | 3675 | `	ph7_vm *pVm;    /* VM that own this exception */` |
|         - | 3676 | `	SySet sEntry;   /* Compiled 'catch' blocks (ph7_exception_block instance)` |
|         - | 3677 | `				     * container.` |
|         - | 3678 | `					 */` |
|         - | 3679 | `	SySet sFinally; /* Compiled 'finally' block bytecode (legacy; unused once ROOT C inlining lands) */` |
|         - | 3680 | `	int iHasFinally;/* TRUE if a finally block was compiled */` |
|         - | 3681 | `	int iFinallyDone;/* TRUE if the finally block was already executed (legacy VmLocalExec path) */` |
|         - | 3682 | `	int iInlined;   /* ROOT C: TRUE when this try's catch/finally are inlined into the function` |
|         - | 3683 | `					 * bytecode (generator body). FALSE = legacy detached-mini-program path. */` |
|         - | 3684 | `	sxu32 iFinallyPc;/* ROOT C: inline PC where the finally body begins (0 = no finally) */` |
|         - | 3685 | `	sxu32 iEndCatchPc;/* ROOT C: inline PC just after the whole try/catch/finally (normal exit) */` |
|         - | 3686 | `	sxu32 iNextFinallyPc;/* ROOT C: iFinallyPc of the lexically-enclosing try-with-finally in the` |
|         - | 3687 | `					   * same function, or 0 — threads a return/break out through nested finallys */` |
|         - | 3688 | `	int iInCatch;   /* ROOT C: TRUE while a catch body of this try is running (finally still owed) */` |
|         - | 3689 | `	ph7_class_instance *pInflight;/* ROOT C: exception to bind at OP_CATCH / re-raise at END_FINALLY */` |
|         - | 3690 | `	VmFrame *pFrame; /* Frame that trigger the exception */` |
|         - | 3691 | `	sxu32 iLandingPc;/* Post-try landing pad (= OP_LOAD_EXCEPTION's iP2). Mirrors the` |
|         - | 3692 | `					  * exception frame's iExceptionJump but survives that frame's` |
|         - | 3693 | `					  * teardown, so an in-place catch can record where to resume. */` |
|         - | 3694 | `	void *pOwnerInstr;/* Bytecode array (VmInstr*) this try was compiled into. iLandingPc` |
|         - | 3695 | `					   * indexes THIS array; the resume only fires in the exec running it` |
|         - | 3696 | `					   * (distinguishes a mini-program from the body that shares its frame). */` |
|         - | 3697 | `` 	sxi32 iErrSuppress;/* '@' suppression depth at try entry. A throw from inside `@expr` `` |
|         - | 3698 | `	                    * unwinds past the ERR_CTRL that would have closed the window, so` |
|         - | 3699 | `	                    * the catch restores this snapshot instead of leaking the depth —` |
|         - | 3700 | ``	                    * and a try/catch nested INSIDE an `@` still stays suppressed. */`` |
|         - | 3701 | `	sxu32 nSelfDepth;/* pVm->aSelf depth when this try opened. php runs a catch and a` |
|         - | 3702 | `					   * finally in the scope of the body that DECLARED the try; this` |
|         - | 3703 | `					   * engine runs them at the THROW SITE, which can be several calls` |
|         - | 3704 | `					   * deeper, so the late-static-binding stack still carries the class` |
|         - | 3705 | `					   * of every call still open above the try. The handler parks that` |
|         - | 3706 | ``					   * slice back to this depth, and `static::` / `new static` /`` |
|         - | 3707 | `					   * get_called_class() inside it answer the try owner's called class` |
|         - | 3708 | `					   * the way php's do. */` |
|         - | 3709 | `	sxi32 iStackDepth;/* Operand-stack base (0-based TOS index = pTos-pStack, -1 when empty)` |
|         - | 3710 | `					   * captured when this try opened at OP_LOAD_EXCEPTION. Used only by` |
|         - | 3711 | `					   * Generator::throw() inject-at-yield to drain the abandoned` |
|         - | 3712 | `					   * (mid-expression) operand slots back to the try's base before` |
|         - | 3713 | `					   * landing at iLandingPc. */` |
|         - | 3714 | `	ph7_exception *pCompiled;/* BYTECODE stage 2b: NULL on the compiler-owned object; on a` |
|         - | 3715 | `					   * runtime ACTIVATION (clone pushed by OP_LOAD_EXCEPTION) this points` |
|         - | 3716 | `					   * at the compiled origin. Every activation of a lexical try carries` |
|         - | 3717 | `					   * its OWN mutable state (pFrame/iFinallyDone/iInCatch/pInflight/` |
|         - | 3718 | `					   * iStackDepth) — recursion levels no longer share one object, which` |
|         - | 3719 | `					   * ran every level's catch/finally against the deepest frame. */` |
|         - | 3720 | `};` |
|         - | 3721 | `/*` |
|         - | 3722 | `` * ROOT C: a pending non-local exit for an inline `finally` body. When control`` |
|         - | 3723 | ` * enters a finally (normal fall-through, a caught/unmatched throw, or a return/` |
|         - | 3724 | ` * break/continue crossing the try), one of these is pushed onto pVm->aFinallyAction;` |
|         - | 3725 | ` * the finally's terminating OP_END_FINALLY pops it and dispatches accordingly. A` |
|         - | 3726 | ` * return/break/continue crossing several nested finallys keeps ONE record on the` |
|         - | 3727 | ` * stack and re-drives it through each finally via ph7_exception.iNextFinallyPc.` |
|         - | 3728 | ` */` |
|         - | 3729 | `#define PH7_FA_FALLTHROUGH 0  /* Resume at iNextPc (post-construct landing) */` |
|         - | 3730 | `#define PH7_FA_RETHROW     1  /* Re-raise pExc after the finally runs */` |
|         - | 3731 | `#define PH7_FA_RETURN      2  /* Return sRet from pTargetBody after the finally chain */` |
|         - | 3732 | `#define PH7_FA_JMP         3  /* Break/continue: resume at iNextPc after the finally chain */` |
|         - | 3733 | `typedef struct VmFinallyAction VmFinallyAction;` |
|         - | 3734 | `struct VmFinallyAction` |
|         - | 3735 | `{` |
|         - | 3736 | `	int eKind;                    /* One of PH7_FA_* */` |
|         - | 3737 | `	sxu32 iNextPc;                /* FALLTHROUGH/JMP: pc (0-based) to resume at in this array */` |
|         - | 3738 | `	ph7_class_instance *pExc;     /* RETHROW: exception to re-raise (holds a ref) */` |
|         - | 3739 | `	ph7_value sRet;               /* RETURN: the value to return (owned) */` |
|         - | 3740 | ``	int bHasRetVal;               /* RETURN: TRUE if sRet holds a real value (vs bare `return;`) */`` |
|         - | 3741 | `	void *pTargetBody;            /* RETURN: VmFrame* the return materializes on */` |
|         - | 3742 | `	int nCross;                   /* trys still to cross through their finallys (-1 = unbounded,` |
|         - | 3743 | `	                               * for RETURN; a positive count bounds a break/continue to the` |
|         - | 3744 | `	                               * trys between it and its target loop) */` |
|         - | 3745 | `};` |
|         - | 3746 | `/* Forward reference */` |
|         - | 3747 | `typedef struct ph7_case_expr ph7_case_expr;` |
|         - | 3748 | `typedef struct ph7_switch ph7_switch;` |
|         - | 3749 | `/*` |
|         - | 3750 | ` * Each compiled case block in a swicth statement is compiled` |
|         - | 3751 | ` * and stored in an instance of the following structure.` |
|         - | 3752 | ` */` |
|         - | 3753 | `struct ph7_case_expr` |
|         - | 3754 | `{` |
|         - | 3755 | `	SySet aByteCode;   /* Compiled body of the case block */` |
|         - | 3756 | `	sxu32 nStart;      /* First instruction to execute */` |
|         - | 3757 | `};` |
|         - | 3758 | `/*` |
|         - | 3759 | ` * Each compiled switch statement is parsed out and stored` |
|         - | 3760 | ` * in an instance of the following structure.` |
|         - | 3761 | ` */` |
|         - | 3762 | `struct ph7_switch` |
|         - | 3763 | `{` |
|         - | 3764 | `	SySet aCaseExpr;  /* Compile case block */` |
|         - | 3765 | `	sxu32 nOut;       /* First instruction to execute after this statement */` |
|         - | 3766 | `	sxu32 nDefault;   /* First instruction to execute in the default block */` |
|         - | 3767 | `};` |
|         - | 3768 | `/*` |
|         - | 3769 | ` * Each arm of a PHP 8.0 match expression is compiled into` |
|         - | 3770 | ` * an instance of the following structure.` |
|         - | 3771 | ` */` |
|         - | 3772 | `typedef struct ph7_match_arm ph7_match_arm;` |
|         - | 3773 | `typedef struct ph7_match     ph7_match;` |
|         - | 3774 | `struct ph7_match_arm` |
|         - | 3775 | `{` |
|         - | 3776 | `	SySet aConds;   /* SySet of SySet (VmInstr) — one compiled bytecode block per condition value */` |
|         - | 3777 | `	SySet aResult;  /* Compiled bytecode of the arm's result expression */` |
|         - | 3778 | `	int   bDefault; /* 1 if this is the 'default' arm */` |
|         - | 3779 | `};` |
|         - | 3780 | `struct ph7_match` |
|         - | 3781 | `{` |
|         - | 3782 | `	SySet aArms;    /* SySet of ph7_match_arm */` |
|         - | 3783 | `};` |
|         - | 3784 | `/* Assertion flags */` |
|         - | 3785 | `#define PH7_ASSERT_DISABLE    0x01  /* Disable assertion */` |
|         - | 3786 | `#define PH7_ASSERT_WARNING    0x02  /* Deprecated in PHP 8: kept for constant compatibility only */` |
|         - | 3787 | `#define PH7_ASSERT_BAIL       0x04  /* Terminate execution on failed assertions */` |
|         - | 3788 | `#define PH7_ASSERT_QUIET_EVAL 0x08  /* Not used */` |
|         - | 3789 | `#define PH7_ASSERT_CALLBACK   0x10  /* Callback to call on failed assertions */` |
|         - | 3790 | `#define PH7_ASSERT_ZEND_OFF   0x20  /* zend.assertions < 1: assert() compiled out (php CLI default -1) */` |
|         - | 3791 | `/*` |
|         - | 3792 | ` * error_log() consumer function signature.` |
|         - | 3793 | ` * Refer to the [PH7_VM_CONFIG_ERR_LOG_HANDLER] configuration directive` |
|         - | 3794 | ` * for more information on how to register an error_log consumer().` |
|         - | 3795 | ` */` |
|         - | 3796 | `typedef void (*ProcErrLog)(const char *,int,const char *,const char *);` |
|         - | 3797 | `/*` |
|         - | 3798 | ` * An instance of the following structure hold the bytecode instructions` |
|         - | 3799 | ` * resulting from compiling a PHP script.` |
|         - | 3800 | ` * This structure contains the complete state of the virtual machine.` |
|         - | 3801 | ` */` |
|         - | 3802 | `/* In-flight magic-accessor guard entry (band A #3a; see vm.c helpers). */` |
|         - | 3803 | `typedef struct VmMagicGuard VmMagicGuard;` |
|         - | 3804 | `struct VmMagicGuard` |
|         - | 3805 | `{` |
|         - | 3806 | `	void *pThis;      /* instance identity */` |
|         - | 3807 | `	sxu32 nNameHash;  /* property-name hash (SyBinHash) */` |
|         - | 3808 | `	sxu8 cKind;       /* accessor kind: 'g' = __get */` |
|         - | 3809 | `};` |
|         - | 3810 | `/* Pending property write-back entry (PHP 8.4 hooks + magic ??=): a LIFO of` |
|         - | 3811 | ` * these (ph7_vm.aHookRmw) carries every write whose dispatch is deferred past` |
|         - | 3812 | ` * OP_MEMBER to a later opcode:` |
|         - | 3813 | ` *   VM_HOOK_PEND_RMW        — read-modify-write on a hooked property: OP_MEMBER` |
|         - | 3814 | ` *                             dispatched the get hook (or read the raw backing` |
|         - | 3815 | ` *                             store when set-only) into a fresh SCRATCH memobj` |
|         - | 3816 | ` *                             slot; the modify op (++/--/compound-assign)` |
|         - | 3817 | ` *                             mutates the scratch and its tail consumes the` |
|         - | 3818 | ` *                             entry (matched by kind + scratch index) to` |
|         - | 3819 | ` *                             dispatch the set hook with the computed value.` |
|         - | 3820 | `` *   VM_HOOK_PEND_COAL_HOOK  — `$o->p ??= v` on a hooked property: the entry is`` |
|         - | 3821 | ` *                             consumed by the OP_NULLC_STORE at nPc (matched by` |
|         - | 3822 | ` *                             owner + pc) to dispatch the set hook.` |
|         - | 3823 | `` *   VM_HOOK_PEND_COAL_MAGIC — `$o->p ??= v` on a missing property whose class`` |
|         - | 3824 | ` *                             declares __set: consumed the same way, dispatching` |
|         - | 3825 | ` *                             __set(sName, value).` |
|         - | 3826 | ` *   VM_HOOK_PEND_RMW_MAGIC  — read-modify-write on an OVERLOADED property` |
|         - | 3827 | `` *                             (`$o->n++`, `$o->n .= 'x'`): the same scratch-slot`` |
|         - | 3828 | ` *                             rail as VM_HOOK_PEND_RMW, with __get having` |
|         - | 3829 | ` *                             provided the current value and __set(sName, value)` |
|         - | 3830 | ` *                             taking the computed one.` |
|         - | 3831 | `` *   VM_HOOK_PEND_RMW_DIM    — `$o[$k] op= v` on an ArrayAccess element (php's`` |
|         - | 3832 | ` *                             ASSIGN_DIM_OP): offsetGet($k) provided the current` |
|         - | 3833 | ` *                             value and offsetSet($k, value) takes the computed` |
|         - | 3834 | ` *                             one. The KEY lives in its own reserved memobj,` |
|         - | 3835 | ` *                             whose index this kind keeps in nBackIdx.` |
|         - | 3836 | ` * The armed window is [nJmpPc, nPc]: an owner fetch outside it means the` |
|         - | 3837 | ` * statement was abandoned (a routed throw) or the ??= short-circuit jump was` |
|         - | 3838 | ` * taken — the entry is dropped, no set dispatch (php: the throw/skip discards` |
|         - | 3839 | ` * the write). LIFO order makes nested arms (a ??= RHS containing further` |
|         - | 3840 | ` * hooked stores or coalesce-assigns, a recursive re-entry through a cast` |
|         - | 3841 | ` * inside a modify op) nest correctly. Each entry owns one instance reference;` |
|         - | 3842 | ` * MAGIC entries own their name blob. */` |
|         - | 3843 | `#define VM_HOOK_PEND_RMW         0` |
|         - | 3844 | `#define VM_HOOK_PEND_COAL_HOOK   1` |
|         - | 3845 | `#define VM_HOOK_PEND_COAL_MAGIC  2` |
|         - | 3846 | `#define VM_HOOK_PEND_RMW_MAGIC   3` |
|         - | 3847 | `#define VM_HOOK_PEND_RMW_DIM     4` |
|         - | 3848 | `/* The SCRATCH-slot kinds: armed by the fetch (OP_MEMBER / OP_LOAD_IDX) and` |
|         - | 3849 | ` * consumed by the modify op's tail through VmHookRmwConsume, matched by the` |
|         - | 3850 | ` * scratch index the fetch left in pTos->nIdx. */` |
|         - | 3851 | `#define VM_HOOK_PEND_IS_RMW(iKind) \` |
|         - | 3852 | `	((iKind) == VM_HOOK_PEND_RMW \|\| (iKind) == VM_HOOK_PEND_RMW_MAGIC \` |
|         - | 3853 | `	 \|\| (iKind) == VM_HOOK_PEND_RMW_DIM)` |
|         - | 3854 | `typedef struct VmHookRmw VmHookRmw;` |
|         - | 3855 | `struct VmHookRmw` |
|         - | 3856 | `{` |
|         - | 3857 | `	sxu8 iKind;                 /* VM_HOOK_PEND_* */` |
|         - | 3858 | `	ph7_class_instance *pThis;  /* receiver (owns one reference while pending) */` |
|         - | 3859 | `	ph7_class_attr *pAttr;      /* hooked property (hook kinds; 0 for MAGIC) */` |
|         - | 3860 | ``	sxu32 nBackIdx;             /* BACKING slot index (for `set => expr` stores) */`` |
|         - | 3861 | `	sxu32 nScratchIdx;          /* RMW: scratch slot the modify op operates on;` |
|         - | 3862 | `	                             * SXU32_HIGH for the coalesce kinds */` |
|         - | 3863 | `	SyBlob sName;               /* COAL_MAGIC: property name copy (entry-owned) */` |
|         - | 3864 | `	void *pOwnerStack;          /* arming activation's operand-stack base (identity;` |
|         - | 3865 | `	                             * a nested exec — even a recursive one over the same` |
|         - | 3866 | `	                             * bytecode — has a different base, so it never drops` |
|         - | 3867 | `	                             * an enclosing activation's pending entry) */` |
|         - | 3868 | `	void *pInstrs;              /* arming activation's bytecode array */` |
|         - | 3869 | `	sxu32 nJmpPc;               /* first pc of the armed window (RMW: == nPc;` |
|         - | 3870 | `	                             * coalesce: the OP_NULLC_JMP right after the arm) */` |
|         - | 3871 | `	sxu32 nPc;                  /* pc of the consuming op (RMW: the modify op;` |
|         - | 3872 | `	                             * coalesce: the OP_NULLC_STORE) */` |
|         - | 3873 | `};` |
|         - | 3874 |  |
|         - | 3875 | `/* A shared weak cell: one per weakly-referenced target instance. pObj nulls` |
|         - | 3876 | ` * when the target is released (the PH7_ClassInstanceRelease hook); nRef` |
|         - | 3877 | ` * counts the PHP-side handles (WeakReference objects, WeakMap entries). */` |
|         - | 3878 | `typedef struct VmWeakCell VmWeakCell;` |
|         - | 3879 | `struct VmWeakCell` |
|         - | 3880 | `{` |
|         - | 3881 | `	ph7_class_instance *pObj; /* target instance; 0 once dead */` |
|         - | 3882 | `	ph7_class_instance *pRef; /* the ONE WeakReference handed out for pObj, so` |
|         - | 3883 | `	                           * WeakReference::create($o) answers the same object` |
|         - | 3884 | `	                           * twice as php's does. NOT owned: the WeakReference's` |
|         - | 3885 | `	                           * own release nulls it. */` |
|         - | 3886 | `	sxu32 nRef;               /* PHP-side handle count */` |
|         - | 3887 | `};` |
|         - | 3888 | `/* The OPEN directory handle behind one DirectoryIterator (php's u.dir.dirp).` |
|         - | 3889 | ` * Registered per instance rather than in a property slot: a slot holding a C` |
|         - | 3890 | `` * pointer would be compared by `==` (php's two equal-positioned iterators are`` |
|         - | 3891 | ` * equal) and COPIED by clone, and php's clone opens the directory again. The` |
|         - | 3892 | ` * class's xRelease closes it. */` |
|         - | 3893 | `typedef struct VmDirHandle VmDirHandle;` |
|         - | 3894 | `struct VmDirHandle` |
|         - | 3895 | `{` |
|         - | 3896 | `	const ph7_io_stream *pStream; /* device that opened it */` |
|         - | 3897 | `	void *pHandle;                /* its handle */` |
|         - | 3898 | `	ph7_class_instance *pThis;    /* the owning instance -- and the hash KEY's bytes,` |
|         - | 3899 | `	                               * which SyHashInsert borrows rather than copies */` |
|         - | 3900 | `};` |
|         - | 3901 | `/* One -d/-c php.ini directive queued for the INI chunk (name/value are` |
|         - | 3902 | ` * allocator-owned copies; see PH7_VM_CONFIG_INI_ENTRY). sFile/nLine are the` |
|         - | 3903 | ` * host's source for the refusal warning ("Unknown" plus a virtual line for` |
|         - | 3904 | ` * -d, the real path and line for a -c file) -- empty when the host never` |
|         - | 3905 | ` * supplied one, which silences the warning rather than misattributing it.` |
|         - | 3906 | ` * iStop is the scanner's stop condition, which is what dates that warning and` |
|         - | 3907 | ` * -- for the PH7_INI_STOP_SECTION* codes -- is the whole entry, an unclosed` |
|         - | 3908 | `` * `[` standing in the queue where a directive would be. */`` |
|         - | 3909 | `typedef struct VmIniEntry VmIniEntry;` |
|         - | 3910 | `struct VmIniEntry` |
|         - | 3911 | `{` |
|         - | 3912 | `	SyString sName;` |
|         - | 3913 | `	SyString sValue;` |
|         - | 3914 | `	SyString sFile;` |
|         - | 3915 | `	sxu32 nLine;` |
|         - | 3916 | `	int iStop;           /* PH7_INI_STOP_*: how the host's scanner stopped on this one */` |
|         - | 3917 | `};` |
|         - | 3918 | `/*` |
|         - | 3919 | ` * One live php.ini directive. The table was an embedded-PHP array on a private` |
|         - | 3920 | `` * `__IniS` class; it is C now, seeded lazily on the first INI call from the static`` |
|         - | 3921 | ` * defaults merged with the CLI's -d/-c queue (aIniCli).` |
|         - | 3922 | ` *` |
|         - | 3923 | ` * php exposes both a global_value and a local_value per directive: ini_set() moves` |
|         - | 3924 | ` * the local one, ini_restore() puts the global one back, and get_cfg_var() answers` |
|         - | 3925 | ` * the global one. Both blobs live on the VM allocator, so they are freed with it --` |
|         - | 3926 | ` * no release hook, exactly as aIniCli needs none.` |
|         - | 3927 | ` */` |
|         - | 3928 | `/*` |
|         - | 3929 | ` * The diagnostics of the last date parse — DateTime::getLastErrors()'s whole answer.` |
|         - | 3930 | ` *` |
|         - | 3931 | ` * php keeps one such record per request and both date classes read it, so this is a` |
|         - | 3932 | `` * VM field rather than the PHL-only `public static DateTime::$__dtLastErr` it used to`` |
|         - | 3933 | ` * be. Every message is a static literal owned by the parser, so nothing here owns` |
|         - | 3934 | ` * memory and the record needs no release hook. The kept-vs-total split is php's:` |
|         - | 3935 | `` * `error_count` counts every error the scan raised, while the `errors` map holds one`` |
|         - | 3936 | ` * entry per POSITION (a later error at a position php has already reported replaces` |
|         - | 3937 | ` * the message rather than adding a row).` |
|         - | 3938 | ` */` |
|         - | 3939 | `#define PH7_DT_MAX_WARN 3` |
|         - | 3940 | `#define PH7_DT_MAX_ERR  8` |
|         - | 3941 | `/* One diagnostic row. The message is always a static literal, so a row keeps` |
|         - | 3942 | ` * the pointer rather than the bytes. */` |
|         - | 3943 | `typedef struct phl_dt_diag_row phl_dt_diag_row;` |
|         - | 3944 | `struct phl_dt_diag_row` |
|         - | 3945 | `{` |
|         - | 3946 | `	int iPos;` |
|         - | 3947 | `	const char *zMsg;` |
|         - | 3948 | `};` |
|         - | 3949 | `typedef struct phl_dt_lasterr phl_dt_lasterr;` |
|         - | 3950 | `struct phl_dt_lasterr` |
|         - | 3951 | `{` |
|         - | 3952 | ``	sxu8 bSet;                        /* 0 -> getLastErrors() answers php's `false` */`` |
|         - | 3953 | `	int nWarn;                        /* warning_count (total) */` |
|         - | 3954 | `	int nWarnKept;                    /* rows in the warnings map */` |
|         - | 3955 | `	int aWarnPos[PH7_DT_MAX_WARN];` |
|         - | 3956 | `	const char *azWarn[PH7_DT_MAX_WARN];` |
|         - | 3957 | `	int nErr;                         /* error_count (total) */` |
|         - | 3958 | `	int nErrKept;                     /* rows in the errors map */` |
|         - | 3959 | `	/* php's scanner records an error and READS ON, so a string may carry one per` |
|         - | 3960 | `	 * byte of itself: the rows grow rather than fitting a fixed array. The blob` |
|         - | 3961 | `	 * holds nErrKept phl_dt_diag_row, allocated from the VM's own backend and` |
|         - | 3962 | `	 * released wholesale with it. */` |
|         - | 3963 | `	SyBlob sErr;` |
|         - | 3964 | `};` |
|         - | 3965 | `typedef struct VmIniSlot VmIniSlot;` |
|         - | 3966 | `struct VmIniSlot` |
|         - | 3967 | `{` |
|         - | 3968 | `	SyString sName;   /* static default name, or a VM-lifetime dup of a CLI name */` |
|         - | 3969 | `	sxi32 iAccess;    /* INI_USER\|INI_PERDIR\|INI_SYSTEM bitmask php reports */` |
|         - | 3970 | `	SyBlob sGlobal;   /* php's global_value */` |
|         - | 3971 | `	SyBlob sLocal;    /* php's local_value (what ini_get answers, modulo live wiring) */` |
|         - | 3972 | `	/* php's third state for a value: UNSET. A directive php declares with no` |
|         - | 3973 | `	 * default at all reports NULL rather than the empty string from every` |
|         - | 3974 | `	 * surface that shows the raw value, and the empty string IS a different` |
|         - | 3975 | `	 * value -- one a script can write. An empty blob cannot tell them apart, so` |
|         - | 3976 | `	 * the two flags do. */` |
|         - | 3977 | `	sxu8 bGlobalNull; /* the directive was declared with no value */` |
|         - | 3978 | `	sxu8 bLocalNull;  /* and nothing has written one since */` |
|         - | 3979 | `};` |
|         - | 3980 | ``/* php's default spl_autoload_extensions() list: the `.inc` is tried FIRST,`` |
|         - | 3981 | ` * which is what decides the answer when two files with the same base name` |
|         - | 3982 | ` * both declare the class. */` |
|         - | 3983 | `#define PH7_SPL_AUTOLOAD_EXT ".inc,.php"` |
|         - | 3984 | `struct ph7_vm` |
|         - | 3985 | `{` |
|         - | 3986 | `	SyMemBackend sAllocator;	/* Memory backend */` |
|         - | 3987 | `#if defined(PH7_ENABLE_THREADS)` |
|         - | 3988 | `	SyMutex *pMutex;           /* Recursive mutex associated with VM. */` |
|         - | 3989 | `#endif` |
|         - | 3990 | `	ph7 *pEngine;               /* Interpreter that own this VM */` |
|         - | 3991 | `	SySet aByteCode;            /* Default bytecode container */` |
|         - | 3992 | `	SySet *pByteContainer;      /* Current bytecode container */` |
|         - | 3993 | `	VmFrame *pFrame;            /* Stack of active frames */` |
|         - | 3994 | `	SyPRNGCtx sPrng;            /* PRNG context (engine-internal, OS-seeded entropy) */` |
|         - | 3995 | `	SyMT19937Ctx sMt;           /* MT19937 backing rand()/mt_rand(); reset by srand()/mt_srand() */` |
|         - | 3996 | `	sxi32 mtSeeded;             /* TRUE once sMt holds a seed (lazy: first draw seeds from the OS CSPRNG, like PHP) */` |
|         - | 3997 | `	VmMemPool aMemObj;          /* Object allocation table (segmented) */` |
|         - | 3998 | `	SySet aLitObj;              /* Literals allocation table */` |
|         - | 3999 | `	ph7_value *aOps;            /* Operand stack */` |
|         - | 4000 | `	SyHash hClass;              /* Compiled classes container */` |
|         - | 4001 | `	SyHash hConstant;           /* Host-application and user defined constants container */` |
|         - | 4002 | `	SyHash hHostFunction;       /* Host-application installable functions */` |
|         - | 4003 | `	SyHash hFunction;           /* Compiled functions */` |
|         - | 4004 | `	SyHash hSuper;              /* Superglobals hashtable */` |
|         - | 4005 | `	sxu32 aSuperFirst[8];       /* Which FIRST BYTES any superglobal name starts with, as a` |
|         - | 4006 | `	                             * 256-bit set. Every variable access asks hSuper before the` |
|         - | 4007 | `	                             * frame -- php's rule, and the order cannot change -- and` |
|         - | 4008 | `	                             * that question hashed the whole name to answer "no" for the` |
|         - | 4009 | `	                             * ~9 names that are superglobals ($GLOBALS and the $_* set).` |
|         - | 4010 | `	                             * Two thirds of the engine's hash lookups on a real workload` |
|         - | 4011 | `	                             * were that miss. One bit test now settles it for a name that` |
|         - | 4012 | `	                             * cannot be one. */` |
|         - | 4013 | `	SyHash hPDO;                /* PDO installed drivers */` |
|         - | 4014 | `	SyBlob sConsumer;           /* Default VM consumer [i.e Redirect all VM output to this blob] */` |
|         - | 4015 | `	SyBlob sWorker;             /* General purpose working buffer */` |
|         - | 4016 | `	SySet aFiles;               /* Stack of processed files */` |
|         - | 4017 | ``	SyBlob sReflectConstName;   /* Scratch for the `Class::MEMBER` name ReflectionParameter::`` |
|         - | 4018 | `	                             * getDefaultValueConstantName() answers for a class-constant` |
|         - | 4019 | `	                             * default: the two halves live in separate literals, so the` |
|         - | 4020 | `	                             * joined text needs somewhere to live past the return. */` |
|         - | 4021 | `	SySet aIncFrame;            /* Stack of ACTIVE include/require/eval activations (VmIncFrame).` |
|         - | 4022 | `	                             * php shows each of them as a trace frame of its own -- the` |
|         - | 4023 | ``	                             * `#N main.php(4): require()` between the included file's frames`` |
|         - | 4024 | `	                             * and the caller's -- and nothing else in this engine records` |
|         - | 4025 | `	                             * one: an include shares its caller's variable scope, so it` |
|         - | 4026 | `	                             * pushes no VmFrame to be found later. */` |
|         - | 4027 | `	SySet aPaths;               /* Set of import paths */` |
|         - | 4028 | `	SySet aIncluded;            /* Set of included files */` |
|         - | 4029 | ``	SySet aEvalFile;            /* Interned `<file>(<line>) : eval()'d code` unit names, one per`` |
|         - | 4030 | `	                             * eval() SITE. A compiled function or class copies the name it` |
|         - | 4031 | `	                             * was declared in, so the text has to outlive the eval that` |
|         - | 4032 | `	                             * made it -- and an eval in a loop must not mint a fresh copy` |
|         - | 4033 | `	                             * every turn, since the site's file and line never change. */` |
|         - | 4034 | `	SySet aOB;                  /* Stackable output buffers */` |
|         - | 4035 | `	SySet aResponseHeaders;     /* HTTP response headers (VmResponseHeader entries) */` |
|         - | 4036 | `	int iResponseStatus;        /* HTTP response status code (default 200) */` |
|         - | 4037 | `	int bHeadersSent;           /* TRUE once non-OB output has been emitted */` |
|         - | 4038 | `	SyBlob sOutStartFile;       /* WHERE that first output went out: php names the file and the` |
|         - | 4039 | `	                             * line in four diagnostics ("output started at %s:%u", and the` |
|         - | 4040 | `	                             * session pair's "sent from %s on line %u") and hands them to` |
|         - | 4041 | `	                             * headers_sent()'s two by-ref out-params. Empty until output. */` |
|         - | 4042 | `	sxu32 nOutStartLine;        /* ... its line (0 while nothing has been emitted) */` |
|         - | 4043 | `	SyBlob sSessStartFile;      /* WHERE the active session was started: php's session-locked ini` |
|         - | 4044 | `	                             * diagnostic names it ("started from %s on line %u"). */` |
|         - | 4045 | `	sxu32 nSessStartLine;       /* ... its line */` |
|         - | 4046 | `	int bHttpContext;           /* TRUE when an HTTP request has been fed (server/CGI mode) */` |
|         - | 4047 | `	int bInlineTryCatch;        /* ROOT C: TRUE once the inline try/catch/finally VM handlers exist,` |
|         - | 4048 | `	                             * enabling the compiler to inline generator-body try/catch (so a` |
|         - | 4049 | ``	                             * `yield` in a catch/finally suspends). Default 0 = legacy path. */`` |
|         - | 4050 | `	int bRenderingUncaught;     /* TRUE while the uncaught-exception report is being rendered. That` |
|         - | 4051 | `	                             * report asks the exception for its own trace (getTraceAsString,` |
|         - | 4052 | `	                             * userland code), so anything that throws in there would re-enter` |
|         - | 4053 | `	                             * the renderer and recurse until the process dies — which is exactly` |
|         - | 4054 | `	                             * what a bad max-arity stamp did (18 GB before the OOM killer, 19 Jul` |
|         - | 4055 | `	                             * 2026). The guard makes the second entry fall back to the` |
|         - | 4056 | `	                             * synthesized trace instead of looping. */` |
|         - | 4057 | `	int bCompilingBuiltin;      /* TRUE while the embedded builtin PHP library chunks compile at VM` |
|         - | 4058 | `	                             * init: classes/functions defined then are stamped INTERNAL so` |
|         - | 4059 | `	                             * Reflection reports isInternal() like Zend does for C-level code. */` |
|         - | 4060 | ``	int bSyntaxCheck;           /* TRUE for a `phl -l` compile (PH7_SYNTAX_CHECK): the unit is only`` |
|         - | 4061 | `	                             * ever PARSED, never run. A class declaration is then never` |
|         - | 4062 | `	                             * DEFERRED -- nothing autoloads here, so deferring would leave its` |
|         - | 4063 | `	                             * whole body unparsed and lint an unparsable file clean -- and the` |
|         - | 4064 | `	                             * refusals that only a resolved parent/interface/trait can answer` |
|         - | 4065 | `	                             * are not raised, because php binds inheritance at run time and` |
|         - | 4066 | ``	                             * `php -l` does not report those either. */`` |
|         - | 4067 | `	int bReflectBypass;         /* Consume-once: the next method OP_CALL skips the visibility` |
|         - | 4068 | `	                             * check (ReflectionMethod::invoke bypasses protection like PHP` |
|         - | 4069 | `	                             * 8.1+). Cleared by the check site; never survives past one call. */` |
|         - | 4070 | `	VmNativeCall *pNativeCall;  /* The INTERNAL functions and methods running right now,` |
|         - | 4071 | `	                             * newest first -- the frames php's trace carries for a` |
|         - | 4072 | `	                             * throw raised inside a C body. See VmNativeCall. */` |
|         - | 4073 | `	SyString *pNativeFrameName; /* Consume-once: the next OP_CALL's frame was entered by this` |
|         - | 4074 | `	                             * INTERNAL function and so has no userland call site, even` |
|         - | 4075 | `	                             * though its argument BINDING still follows the caller. Armed` |
|         - | 4076 | `	                             * only by a call_user_func/_array php's compiler could not` |
|         - | 4077 | `	                             * elide (an unqualified one inside a namespace). */` |
|         - | 4078 | `	int bCallbackWeak;          /* Consume-once: the next OP_CALL is an INTERNAL function invoking a` |
|         - | 4079 | `	                             * userland callback (array_map, usort, an autoloader, a shutdown` |
|         - | 4080 | `	                             * function, Reflection's invoke, Closure::call), which php runs in` |
|         - | 4081 | `	                             * WEAK mode however strict the file that reached the builtin is —` |
|         - | 4082 | `	                             * there is no "calling file" at such a boundary. The two php` |
|         - | 4083 | `	                             * FORWARDS, call_user_func and call_user_func_array, do not set it:` |
|         - | 4084 | `	                             * they pass the caller's own mode on an argument map. Cleared at` |
|         - | 4085 | `	                             * the head of OP_CALL like the latches below. */` |
|         - | 4086 | `	int bHostDiscard;           /* The HOST (C) function now running was called from a statement` |
|         - | 4087 | `	                             * that throws its answer away. Set around the foreign-function` |
|         - | 4088 | `	                             * dispatch in OP_CALL from the instruction's bDiscard, and read` |
|         - | 4089 | `	                             * by exactly two builtins: php's two callback FORWARDS. */` |
|         - | 4090 | `	int bDiscardCallback;       /* Consume-once: the next call dispatched through` |
|         - | 4091 | `	                             * PH7_VmCallUserFunction inherits that drop, which is how` |
|         - | 4092 | ``	                             * `call_user_func('f');` warns for a #[\NoDiscard] `f` and`` |
|         - | 4093 | ``	                             * `array_map('f', $a);` does not (php special-cases the same two`` |
|         - | 4094 | `	                             * names at compile time). Consumed at the head of OP_CALL. */` |
|         - | 4095 | `	int bDynamicForward;        /* Consume-once, armed by the same two FORWARDS: the callback they` |
|         - | 4096 | `	                             * are about to dispatch is a call php's compiler could NOT fold` |
|         - | 4097 | `	                             * into a direct one, because the callable was not a literal` |
|         - | 4098 | ``	                             * string (`call_user_func($n)`), so php runs it as a DYNAMIC call`` |
|         - | 4099 | `	                             * (ZEND_INIT_USER_CALL). The folded shape,` |
|         - | 4100 | ``	                             * `call_user_func('compact', 'a')`, is a plain direct call there`` |
|         - | 4101 | `	                             * and leaves this clear. Read into PH7_CTX_CALL_DYNAMIC at the` |
|         - | 4102 | `	                             * head of OP_CALL and cleared with the latches above. */` |
|         - | 4103 | `	int bMagicDispatch;         /* Consume-once: the next method OP_CALL is the ENGINE reaching for` |
|         - | 4104 | `	                             * a magic method (PH7_VmCallMagicMethod), so the visibility check` |
|         - | 4105 | `	                             * lets a non-public one through — php only WARNS at such a` |
|         - | 4106 | `	                             * declaration and dispatches anyway. Set at the engine's own` |
|         - | 4107 | `	                             * dispatch sites only, so a call the USER wrote (including a` |
|         - | 4108 | ``	                             * first-class `$o->__get(...)`, which reaches the same C`` |
|         - | 4109 | `	                             * dispatcher) is still denied. Cleared at the head of OP_CALL. */` |
|         - | 4110 | `	int bClosureScreened;       /* Consume-once: the method call now being dispatched comes out of a` |
|         - | 4111 | `	                             * Closure whose callee was RESOLVED and screened when the closure was` |
|         - | 4112 | ``	                             * BUILT (`$this->p(...)`, `Closure::fromCallable([$this,'p'])`,`` |
|         - | 4113 | `	                             * ReflectionMethod::getClosure), so no site may re-decide its` |
|         - | 4114 | `	                             * visibility against the CALLER. php stores a resolved function +` |
|         - | 4115 | `	                             * scope in the Closure and never looks the name up again; PHL keeps a` |
|         - | 4116 | `	                             * name, so without this latch an escaped closure over a private method` |
|         - | 4117 | `	                             * died at the invocation php runs. Armed by VmClosureUnwrap, read by` |
|         - | 4118 | `	                             * the array-callable dispatch sites, cleared at the head of OP_CALL. */` |
|         - | 4119 | `	char zDefTz[68];            /* date_default_timezone_set() identifier, stored verbatim like php` |
|         - | 4120 | `	                             * (default "UTC"; only UTC/GMT are accepted — no tz database) */` |
|         - | 4121 | `	sxu32 nDefTz;               /* zDefTz length in bytes */` |
|         - | 4122 | `	sxu8 bDefTzExplicit;        /* a script called date_default_timezone_set(). php latches on` |
|         - | 4123 | `	                             * that: a later ini_set('date.timezone') records the DIRECTIVE` |
|         - | 4124 | `	                             * and no longer moves the default. */` |
|         - | 4125 | `	phl_dt_lasterr sDtLastErr;  /* DateTime::getLastErrors()'s answer. Was a PHL-only` |
|         - | 4126 | ``	                             * `public static $__dtLastErr` on DateTime, a property php has`` |
|         - | 4127 | `	                             * no equivalent of; both date classes read this field now. */` |
|         - | 4128 | `	SySet aShutdown;            /* Stack of shutdown user callbacks */` |
|         - | 4129 | `	SySet aIniCli;              /* php.ini directives from the CLI (-d/-c): VmIniEntry copies,` |
|         - | 4130 | `	                             * merged into aIniTab when the directive table is seeded */` |
|         - | 4131 | `	SySet aIniTab;              /* The live directive table (VmIniSlot), sorted by name so` |
|         - | 4132 | `	                             * ini_get_all() needs no sort of its own */` |
|         - | 4133 | `	SySet aPersistSock;         /* PERSISTENT socket handles (VmPersistSock), keyed by the` |
|         - | 4134 | `	                             * address as the opener spelled it: php hands the SAME` |
|         - | 4135 | `	                             * resource back for a second pfsockopen() of one address */` |
|         - | 4136 | `	sxu8 bIniSeeded;            /* aIniTab has been built (lazily, on the first INI call) */` |
|         - | 4137 | `	int iPosixErr;              /* ext/posix's remembered errno: what` |
|         - | 4138 | `	                             * posix_get_last_error()/posix_errno() answer.` |
|         - | 4139 | `	                             * php keeps one per MODULE; per VM is the same` |
|         - | 4140 | `	                             * lifetime for a program and keeps two embedded` |
|         - | 4141 | `	                             * VMs apart. Nothing ever clears it -- a later` |
|         - | 4142 | `	                             * SUCCESS leaves the last failure standing,` |
|         - | 4143 | `	                             * which is php's own contract. */` |
|         - | 4144 | `	void *pSyslog;              /* ext/standard's syslog state (builtin_syslog.c owns the` |
|         - | 4145 | `	                             * shape): the prefix openlog() was given -- POSIX keeps the` |
|         - | 4146 | `	                             * POINTER, so it has to outlive the call -- and, on Windows,` |
|         - | 4147 | `	                             * the event-source handle a record is reported through.` |
|         - | 4148 | `	                             * Allocated on the first call, freed by PH7_SyslogVmRelease. */` |
|         - | 4149 | `	void *pPcntl;               /* ext/pcntl's per-VM state (builtin_pcntl.c owns the` |
|         - | 4150 | `	                             * shape): the handler each signal was last given, the` |
|         - | 4151 | `	                             * remembered errno and the async-dispatch flag. Allocated` |
|         - | 4152 | `	                             * lazily on the first call and freed by PH7_PcntlVmRelease,` |
|         - | 4153 | `	                             * which also puts every disposition this VM took over back` |
|         - | 4154 | `	                             * to SIG_DFL. */` |
|         - | 4155 | `	void *pGettext;             /* ext/gettext's per-VM state (builtin_gettext.c owns the` |
|         - | 4156 | `	                             * shape): the domain bindings, the current textdomain and` |
|         - | 4157 | `	                             * the catalog each domain last resolved. Allocated lazily` |
|         - | 4158 | `	                             * from sAllocator on the first call, and freed with it. */` |
|         - | 4159 | ``	/* Session state. Was a private `__SessS` class with five static properties, which`` |
|         - | 4160 | `	 * the INI subsystem had to reach into to live-wire session.name/session.save_path;` |
|         - | 4161 | `	 * both subsystems read these fields now, so neither depends on the other's shape. */` |
|         - | 4162 | `	sxi32 iSessStatus;          /* PHP_SESSION_NONE / _ACTIVE */` |
|         - | 4163 | `	SyBlob sSessId;             /* current session id ("" = none yet) */` |
|         - | 4164 | `	SyBlob sSessName;           /* cookie/session name (default "PHPSESSID") */` |
|         - | 4165 | `	SyBlob sSessPath;           /* save path ("" = not resolved yet -> sys_get_temp_dir()) */` |
|         - | 4166 | `	ph7_value sSessHandler;     /* session_set_save_handler(): the handler OBJECT, or the array` |
|         - | 4167 | `	                             * of callables the procedural form passes. NULL = the built-in` |
|         - | 4168 | ``	                             * `files` store. */`` |
|         - | 4169 | `	sxu8 bSessOpened;           /* a userland handler's open() has run for this session */` |
|         - | 4170 | `	SyBlob sSessData;           /* the payload the store last handed back or was handed: what` |
|         - | 4171 | `	                             * session.lazy_write compares the next write against */` |
|         - | 4172 | `	SyHash hWeakCell;           /* instance pointer bytes -> VmWeakCell* (weak-reference registry;` |
|         - | 4173 | `	                             * PH7_ClassInstanceRelease kills matching cells on free) */` |
|         - | 4174 | `	SyHash hDirHandle;          /* instance pointer bytes -> VmDirHandle* (the open DIR* behind a` |
|         - | 4175 | `	                             * DirectoryIterator; the class's xRelease closes and unregisters) */` |
|         - | 4176 | `	SySet aAutoload;            /* Stack of spl_autoload callbacks */` |
|         - | 4177 | `	SyBlob sAutoloadExt;        /* spl_autoload_extensions(): the comma-separated list` |
|         - | 4178 | `	                             * spl_autoload() tries when it is handed none.` |
|         - | 4179 | `	                             * php's own default is ".inc,.php" and the ORDER is` |
|         - | 4180 | `	                             * observable -- it is what decides which of two files` |
|         - | 4181 | `	                             * with the same base name defines the class. */` |
|         - | 4182 | `	SyHash hAutoloadActive;     /* Classes currently being autoloaded (reentrancy guard) */` |
|         - | 4183 | `	SyHash hTypedSlot;          /* memobj nIdx -> VmClassAttr* for every slot a store must be` |
|         - | 4184 | `	                             * FILTERED through: a declared TYPE to enforce, a native` |
|         - | 4185 | `	                             * class's write handler, or both (PH7_ATTR_STORE_FILTERED).` |
|         - | 4186 | `	                             * Registered and dropped through the two helpers below, which` |
|         - | 4187 | `	                             * are the only writers -- the predicate must not be spelled` |
|         - | 4188 | `	                             * out at a call site again. */` |
|         - | 4189 | `	unsigned char *pFilterBits; /* One BIT per memobj slot: is it in hTypedSlot? A store to a` |
|         - | 4190 | `	                             * property asks that on every write, and it is a hash of a` |
|         - | 4191 | `	                             * dense small INTEGER to hear "no" -- 9M of the engine's 225M` |
|         - | 4192 | `	                             * lookups on the ecosystem gate's phpcs step. The slot index` |
|         - | 4193 | `	                             * indexes this directly instead. Kept by the same two helpers` |
|         - | 4194 | `	                             * that own the table, so it cannot drift from it; a slot past` |
|         - | 4195 | `	                             * nFilterBits was never registered, which is the same answer. */` |
|         - | 4196 | `	sxu32 nFilterBits;          /* How many slots pFilterBits covers (0 = never allocated) */` |
|         - | 4197 | `	sxu8 bFilterBitsOff;        /* The bitmap could not be grown to cover a slot that IS` |
|         - | 4198 | `	                             * registered, so it can no longer answer for anything and` |
|         - | 4199 | `	                             * every question goes back to the table. Sticky, because a` |
|         - | 4200 | `	                             * fresh bitmap would be missing the bits of everything` |
|         - | 4201 | `	                             * registered before it. An allocation CAN fail here without` |
|         - | 4202 | `	                             * the box being out of memory -- a script's memory_limit is a` |
|         - | 4203 | `	                             * real ceiling since the 137th session -- and answering "not` |
|         - | 4204 | `	                             * filtered" there would silently skip a typed property's` |
|         - | 4205 | `	                             * type check, its readonly screen and a native write` |
|         - | 4206 | `	                             * handler. */` |
|         - | 4207 | `	sxu32 nNativeSetSlot;       /* How many of those slots carry a native WRITE HANDLER. Kept` |
|         - | 4208 | `	                             * by the same two helpers, and read by the in-place mutation` |
|         - | 4209 | ``	                             * opcodes: `$i++` on an ordinary variable must not pay for a`` |
|         - | 4210 | `	                             * hash lookup just because some class in the script declares a` |
|         - | 4211 | `	                             * typed property, and with no handler-backed slot alive there` |
|         - | 4212 | `	                             * is nothing for one to find. */` |
|         - | 4213 | `	SySet aException;           /* Stack of loaded exception */` |
|         - | 4214 | `	SySet aFinallyAction;       /* ROOT C: stack of VmFinallyAction — pending action (fallthrough /` |
|         - | 4215 | `	                             * rethrow / return / break-continue) for each inline finally in flight */` |
|         - | 4216 | `	ph7_class_instance *pPendingException; /* Exception deferred past a finally block */` |
|         - | 4217 | `	ph7_class_instance *pInflightException; /* Exception being unwound while a finally runs; a throw from` |
|         - | 4218 | `	                                         * that finally that escapes the finally chains it as $previous` |
|         - | 4219 | `	                                         * (PHP finally-supersede) */` |
|         - | 4220 | `	sxu32 nInflightExcBase;                 /* Exception-stack depth when the in-flight finally started; a throw` |
|         - | 4221 | `	                                         * is "leaving the finally" once the stack unwinds to/below this */` |
|         - | 4222 | `	/* The in-place-catch resume target (ROOT B). The four fields are ONE record and` |
|         - | 4223 | `	 * only mean anything together: a frame paired with another try's landing pad` |
|         - | 4224 | `	 * drains the operand stack to a foreign base and lands mid-statement. They are` |
|         - | 4225 | `	 * written, cleared, saved and restored only through VmSetResumeTarget /` |
|         - | 4226 | `	 * VmClearResumeTarget / VmSaveResumeTarget / VmRestoreResumeTarget — never one` |
|         - | 4227 | `	 * at a time. */` |
|         - | 4228 | `	VmFrame *pResumeFrame;      /* Body frame whose in-place catch consumed the live throw */` |
|         - | 4229 | `	sxu32 iResumePc;            /* Its post-try landing pad (1-based, as iExceptionJump) */` |
|         - | 4230 | `	void *pResumeInstr;         /* Bytecode array the catching try lives in; resume only in that exec */` |
|         - | 4231 | `	sxi32 iResumeStackDepth;    /* Operand-stack base (0-based TOS index) of the catching try, recorded` |
|         - | 4232 | `	                             * with the resume target. Used only by Generator::throw() inject-at-yield` |
|         - | 4233 | `	                             * to drain abandoned mid-expression operands before landing at iResumePc. */` |
|         - | 4234 | `	/* ROOT C inline redirect: set by VmThrowException when a throw is caught by an INLINE` |
|         - | 4235 | `	 * try (generator body). The throw site checks the pair (pInlineInstr, pInlineFrame)` |
|         - | 4236 | `	 * against its own (aInstr, pEntryFrame), drains the operand stack to iInlineDrain, and` |
|         - | 4237 | `	 * jumps to iInlinePc; a mismatch means another activation owns it, so the throw` |
|         - | 4238 | `	 * propagates. The bytecode array alone is NOT identity: two live activations of the` |
|         - | 4239 | `	 * same function share it, so a generator whose sibling activation owned the try` |
|         - | 4240 | `	 * consumed the redirect and ran that try's finally against its OWN variables (twig's` |
|         - | 4241 | ``	 * `Template::yieldBlock`, whose recursive delegation runs three activations of one`` |
|         - | 4242 | ``	 * method at once, read an unset `$level` there). The frame pins the activation, the`` |
|         - | 4243 | `	 * same pairing VmRecordedResume and OP_LOAD_EXCEPTION's activation match already use.` |
|         - | 4244 | `	 * Separate from the ROOT B fields above (legacy path). */` |
|         - | 4245 | `	void *pInlineInstr;         /* Owner bytecode array of the catching inline try (0 = none) */` |
|         - | 4246 | `	void *pInlineFrame;         /* Body frame that owns that try (the activation's identity) */` |
|         - | 4247 | `	sxu32 iInlinePc;            /* 0-based target pc (iHandlerPc or iFinallyPc) */` |
|         - | 4248 | `	sxi32 iInlineDrain;         /* Operand-stack base to drain to before landing (0-based TOS idx) */` |
|         - | 4249 | `	SySet aMagicGuard;          /* In-flight magic-accessor guard (php's property guard):` |
|         - | 4250 | `	                             * {instance, property-name hash, kind} entries pushed around a` |
|         - | 4251 | `	                             * __get dispatch so a self-recursive read of the same property` |
|         - | 4252 | `	                             * falls back to the undefined-property path instead of looping. */` |
|         - | 4253 | `	ph7_class_instance *pMagicSetThis; /* Pending __set receiver (band A #3b): OP_MEMBER detected a` |
|         - | 4254 | `	                             * plain store to a missing/inaccessible property whose class` |
|         - | 4255 | `	                             * declares __set; the VALUE only exists at the immediately-` |
|         - | 4256 | `	                             * following OP_STORE, which consumes this (with sMagicSetName)` |
|         - | 4257 | `	                             * and dispatches __set($name,$value). Holds a reference;` |
|         - | 4258 | `	                             * one-instruction lifetime by construction. */` |
|         - | 4259 | `	SyBlob sMagicSetName;       /* Pending __set property name (stable copy) */` |
|         - | 4260 | `	ph7_class_instance *pHookSetThis; /* Pending property-hook set receiver (PHP 8.4): OP_MEMBER` |
|         - | 4261 | `	                             * detected a plain store to a hooked property; the following` |
|         - | 4262 | `	                             * OP_STORE consumes this (with pHookSetAttr/nHookSetIdx) and` |
|         - | 4263 | `	                             * dispatches __phl_hook_set_NAME — or throws the read-only` |
|         - | 4264 | `	                             * Error when the property has no set hook. Owns one instance` |
|         - | 4265 | `	                             * reference while armed. */` |
|         - | 4266 | `	ph7_class_attr *pHookSetAttr; /* Pending hook-set property (declared attr; name + flags) */` |
|         - | 4267 | ``	sxu32 nHookSetIdx;          /* Pending hook-set BACKING slot index (for `set => expr`) */`` |
|         - | 4268 | ``	VmClassAttr *pRefTargetAttr; /* Pending reference-store target (`$o->p =& $x`): OP_MEMBER tagged`` |
|         - | 4269 | `	                             * PH7_MEMBER_REF_TARGET resolved the instance property slot and` |
|         - | 4270 | `	                             * stashed it here; the immediately-following member-marked` |
|         - | 4271 | `	                             * OP_STORE_REF rebinds it to alias the source variable's slot.` |
|         - | 4272 | `	                             * One-instruction lifetime by construction. */` |
|         - | 4273 | ``	ph7_class_attr *pRefTargetStaticAttr; /* Same, for a static-property target (`self::$s =& $x`). */`` |
|         - | 4274 | `	ph7_class_instance *pRefTargetThis;   /* Instance owning pRefTargetAttr; retained (iRef++) by` |
|         - | 4275 | `	                             * OP_MEMBER, released by the consuming OP_STORE_REF. */` |
|         - | 4276 | `	SySet aHookRmw;             /* Pending property-hook read-modify-write write-backs (LIFO;` |
|         - | 4277 | `	                             * VmHookRmw entries — see the struct above ph7_vm). */` |
|         - | 4278 | `	ph7_class_instance *pMagicCallThis; /* Pending __call receiver (band A #3b): OP_MEMBER hit a` |
|         - | 4279 | `	                             * missing (or inaccessible) method on a class declaring` |
|         - | 4280 | `	                             * __call/__callStatic and marked the callee slot` |
|         - | 4281 | `	                             * MEMOBJ_AUX_MAGICCALL; the packing body OP_CALL then runs` |
|         - | 4282 | `	                             * (VmMagicCallDispatch) consumes this + the class + the original` |
|         - | 4283 | `	                             * name. Holds a reference; NULL for __callStatic. */` |
|         - | 4284 | `	ph7_class *pMagicCallClass; /* Pending __call/__callStatic declaring class */` |
|         - | 4285 | `	ph7_class *pConstEvalClass; /* Transient: class whose constant/property initializer bytecode is` |
|         - | 4286 | `	                             * being evaluated (VmLocalExec has no method frame, so self::/parent::` |
|         - | 4287 | `	                             * inside an initializer resolve through this fallback — consulted by` |
|         - | 4288 | `	                             * PH7_VmPeekDeclaringClass/PH7_VmPeekTopClass when no frame matches). */` |
|         - | 4289 | `	void *pConstEvalFrame;      /* The VmFrame that was current when an ON-DEMAND const initializer` |
|         - | 4290 | `	                             * eval began (VmLocalExec pushes no frame). While the current frame` |
|         - | 4291 | `	                             * still equals it, self::/parent:: resolve to pConstEvalClass even` |
|         - | 4292 | `	                             * though an outer method frame exists (e.g. Base::CONST accessed from` |
|         - | 4293 | `	                             * Sub::method() must NOT resolve self to Sub). A method call inside the` |
|         - | 4294 | `	                             * initializer pushes a new frame, so the marker no longer matches and` |
|         - | 4295 | `	                             * that method's own declaring class wins. NULL outside on-demand eval. */` |
|         - | 4296 | `	sxi32 nConstEvalDepth;      /* Nesting depth of constant/enum-case initializer evaluations. A` |
|         - | 4297 | `	                             * cycle detected at an inner level (pConstCycleAttr) is thrown only` |
|         - | 4298 | `	                             * when depth returns to 0 — a throw INSIDE an initializer mini-exec` |
|         - | 4299 | `	                             * cannot be routed to a user catch (pre-existing engine restriction),` |
|         - | 4300 | `	                             * so the outermost, opcode-level evaluation raises it instead. */` |
|         - | 4301 | `	ph7_class_attr *pConstCycleAttr;  /* Self-referencing constant detected during evaluation */` |
|         - | 4302 | `	ph7_class *pConstCycleClass;      /* ...and the class it belongs to (for the Error message) */` |
|         - | 4303 | `	SyBlob sMagicCallName;      /* Pending original method name (stable copy) */` |
|         - | 4304 | `	ph7_user_func *pMagicCallFunc; /* The __call/__callStatic packing body's function record, built on` |
|         - | 4305 | `	                             * first use (PH7_VmMagicCallFunc) and NOT registered in` |
|         - | 4306 | `	                             * hHostFunction: OP_CALL points straight at it, so the dispatch has` |
|         - | 4307 | `	                             * no PHP-visible name to reach it by. */` |
|         - | 4308 | `	sxi32 nBoundaryRc;          /* C-boundary parked throw status (0 / PH7_EXCEPTION / PH7_ABORT).` |
|         - | 4309 | `	                             * Set by VmBoundaryPark when a PHP callee invoked from a C site` |
|         - | 4310 | `	                             * (magic method, cast hook, __destruct, user callback) raised and` |
|         - | 4311 | `	                             * that C site has no status channel to route it. Consumed once per` |
|         - | 4312 | `	                             * dispatch at the executor's fetch point (and cleared wherever the` |
|         - | 4313 | `	                             * same in-flight throw is landed via VmRecordedResume or the inline` |
|         - | 4314 | `	                             * redirect), so a swallowed throw outlives at most the C remainder` |
|         - | 4315 | `	                             * of one opcode instead of silently resuming execution. */` |
|         - | 4316 | `	SySet aIOstream;            /* Installed IO stream container */` |
|         - | 4317 | `	/* Devices a script has taken OUT of service with stream_wrapper_unregister().` |
|         - | 4318 | `	 * Held as DEVICE pointers rather than names, so a userland wrapper registered` |
|         - | 4319 | `	 * over an unregistered built-in coexists with it in the list above and is the` |
|         - | 4320 | `	 * one the lookup finds. */` |
|         - | 4321 | `	SySet aSuppressedIo;` |
|         - | 4322 | `	const ph7_io_stream *pDefStream; /* Default IO stream [i.e: typically this is the 'file://' stream] */` |
|         - | 4323 | `	ph7_value sExec;           /* Compiled script return value [Can be extracted via the PH7_VM_CONFIG_EXEC_VALUE directive]*/` |
|         - | 4324 | `	ph7_value sExceptionCB;    /* ACTIVE set_exception_handler() handler */` |
|         - | 4325 | `	ph7_value sErrCB;          /* ACTIVE set_error_handler() handler */` |
|         - | 4326 | `	sxi64 iErrCBLevels;        /* sErrCB's $error_levels mask: a handler is only called for the` |
|         - | 4327 | `	                            * levels it was REGISTERED for, and every other one falls` |
|         - | 4328 | `	                            * through to the engine's own reporting. Read at full width --` |
|         - | 4329 | `	                            * php ANDs a zend_long, so 2^32+1024 still selects` |
|         - | 4330 | `	                            * E_USER_NOTICE. */` |
|         - | 4331 | `	SySet aExceptionCBSaved;   /* VmHandlerSlot stack underneath sExceptionCB */` |
|         - | 4332 | `	SySet aErrCBSaved;         /* VmHandlerSlot stack underneath sErrCB */` |
|         - | 4333 | `	void *pStdin;              /* STDIN IO stream */` |
|         - | 4334 | `	void *pStdout;             /* STDOUT IO stream */` |
|         - | 4335 | `	void *pStderr;             /* STDERR IO stream */` |
|         - | 4336 | `	int bErrReport;            /* TRUE to report all runtime Error/Warning/Notice */` |
|         - | 4337 | `	int iDisplayErrors;        /* display_errors ini DESTINATION, not a gate: OFF emits no` |
|         - | 4338 | `` 	                            * DISPLAY copy, STDOUT emits `\nWarning: msg in F on line N` `` |
|         - | 4339 | `	                            * to the program output stream, STDERR emits the same sentence` |
|         - | 4340 | `	                            * WITHOUT the leading blank line to the error stream, outside` |
|         - | 4341 | `	                            * the output layer. php CLI default: off.` |
|         - | 4342 | `	                            * See PH7_VmDisplayErrorsMode() for the value table. */` |
|         - | 4343 | `	int bLogErrors;            /* log_errors ini gate: TRUE emits the LOG copy of a runtime` |
|         - | 4344 | ``	                            * diagnostic (`PHP Warning:  msg in F on line N`) to the error`` |
|         - | 4345 | `	                            * stream (stderr via sVmErrConsumer). php CLI default: on. */` |
|         - | 4346 | ``	SyBlob sErrLogPath;        /* `error_log` ini destination: the file every LOG copy and`` |
|         - | 4347 | `	                            * error_log()'s configured-logger types are appended to,` |
|         - | 4348 | `	                            * timestamped. EMPTY means unset, which is php's SAPI logger` |
|         - | 4349 | `	                            * -- the error stream. A path that will not open falls back` |
|         - | 4350 | `	                            * to that stream too, exactly as php's logger does. */` |
|         - | 4351 | `	int bGcEnabled;            /* gc_enable()/gc_disable(): whether the cycle collector may` |
|         - | 4352 | `	                            * buffer a possible root at all. Off means PHL frees by` |
|         - | 4353 | `	                            * reference count alone, which strands every cycle. */` |
|         - | 4354 | `	SySet aGcRoot;             /* Possible cycle roots: a container whose refcount dropped` |
|         - | 4355 | `	                            * without reaching zero. See vm_gc.c */` |
|         - | 4356 | `	SySet aGcWork;             /* Traversal worklist (VM-owned so a collection allocates` |
|         - | 4357 | `	                            * nothing per run) */` |
|         - | 4358 | `	SySet aGcAux;              /* ...and the one scan_black runs on, since it is entered` |
|         - | 4359 | `	                            * mid-drain of the primary */` |
|         - | 4360 | `	SySet aGcDead;             /* What the collect phase proved garbage */` |
|         - | 4361 | `	sxu8 bGcWanted;            /* The root buffer filled: collect at the next fetch point */` |
|         - | 4362 | `	sxu8 bGcRunning;           /* A collection is in flight; nothing may buffer or re-enter */` |
|         - | 4363 | `	SySet aDeadClosure;        /* Run-time closures whose last holder went: freed at the VM's` |
|         - | 4364 | `	                            * next fetch point rather than on the spot, because the drop` |
|         - | 4365 | `	                            * happens in the middle of a dispatch that is still about to` |
|         - | 4366 | `	                            * look the function up. See PH7_VmPurgeDeadClosures. */` |
|         - | 4367 | `	sxu8 bClosurePurge;        /* ...and whether that list has anything on it */` |
|         - | 4368 | `	sxu32 nGcThreshold;        /* Buffered roots that trigger a collection; adaptive (vm_gc.c) */` |
|         - | 4369 | `	sxu32 nGcRuns;             /* Collections run, for gc_status() */` |
|         - | 4370 | `	sxu32 nGcCollected;        /* Containers freed by them, for gc_status() */` |
|         - | 4371 | `	sxi32 iErrMask;      /* error_reporting() level. PH7 collapsed it to the bErrReport` |
|         - | 4372 | `	                      * boolean, so E_ALL & ~E_DEPRECATED still printed every` |
|         - | 4373 | `	                      * deprecation — any non-zero level meant "report all". */` |
|         - | 4374 | `	int bErrMaskSet;     /* Has anybody SAID what the level is? The main script's own` |
|         - | 4375 | `	                      * compile runs inside ph7_compile_file, which is what CREATES` |
|         - | 4376 | `	                      * the VM, so a diagnostic raised there is older than the host's` |
|         - | 4377 | `	                      * first ph7_vm_config() call and iErrMask is still zero — which` |
|         - | 4378 | ``	                      * a compile diagnostic must not read as `error_reporting(0)`.`` |
|         - | 4379 | `	                      * Set by every door that writes iErrMask, never cleared. */` |
|         - | 4380 | `	int nRecursionDepth;       /* Current PHP call depth (OP_CALL frames only) */` |
|         - | 4381 | `	int nErrSuppress;          /* '@' error-control depth: >0 means the diagnostics raised` |
|         - | 4382 | `	                            * while evaluating the suppressed expression are not printed` |
|         - | 4383 | `	                            * (a user error handler is still invoked, as in php). Nests. */` |
|         - | 4384 | `	int nMaxDepth;             /* Maximum PHP call depth; 0 == unbounded (the host` |
|         - | 4385 | `	                            * default: PHP frames are heap-bound since the` |
|         - | 4386 | `	                            * iterative executor, so recursion is limited by` |
|         - | 4387 | `	                            * memory like the main PHP engine). Embedders opt in` |
|         - | 4388 | `	                            * via PH7_VM_CONFIG_RECURSION_DEPTH. */` |
|         - | 4389 | `	int nVmExecDepth;          /* Live native VmByteCodeExec activations (C-stack guard;` |
|         - | 4390 | `	                            * see the VmByteCodeExec wrapper in vm.c) */` |
|         - | 4391 | `	int nMaxNativeDepth;       /* Maximum native VmByteCodeExec nesting (mini-programs,` |
|         - | 4392 | `	                            * C->PHP callbacks, ctx start/resume, eval/include) —` |
|         - | 4393 | `	                            * what actually protects the C stack now that PHP` |
|         - | 4394 | `	                            * recursion is iterative. Platform-sized default,` |
|         - | 4395 | `	                            * PH7_VM_CONFIG_NATIVE_DEPTH overrides. */` |
|         - | 4396 | `	void *pIdleCallFrames;     /* Freelist of VmCallFrame nodes (BYTECODE stage 2):` |
|         - | 4397 | `	                            * fixed-size, strictly LIFO per invocation — reusing` |
|         - | 4398 | `	                            * them skips a pool alloc/free round-trip per PHP` |
|         - | 4399 | `	                            * call (the measured trampoline overhead). Backing` |
|         - | 4400 | `	                            * memory is allocator-owned; freed wholesale. */` |
|         - | 4401 | `	/* Freelists of recycled operand-stack buffers (BYTECODE stage 7): a returning PHP` |
|         - | 4402 | `	 * call recycles its (tight-sized) operand stack here instead of freeing it, so a` |
|         - | 4403 | `	 * same-size call reuses it -- skipping the buffer alloc AND the per-slot init.` |
|         - | 4404 | `	 * Bounded by an entry count AND a total-slot budget; buffers are plain allocator` |
|         - | 4405 | `	 * blocks so cold/suspend/abort paths can still raw-free them.` |
|         - | 4406 | `	 *` |
|         - | 4407 | `	 * KEYED BY SIZE, because only an EXACT size is reusable. One list held every` |
|         - | 4408 | `	 * parked buffer and every call walked it looking for its own size: with the cap` |
|         - | 4409 | `	 * at 256 buffers that walk was 2.2% of a phpcs run, spent almost entirely on` |
|         - | 4410 | `	 * sizes the caller was never going to take. The size picks the chain now, so a` |
|         - | 4411 | `	 * call compares against the handful of buffers whose size ends in the same six` |
|         - | 4412 | `	 * bits instead of against all of them. */` |
|         - | 4413 | `	void *apIdleOperandStack[PH7_STACK_POOL_BUCKETS];` |
|         - | 4414 | `	int nIdleOperandStacks;    /* Buffers parked across every chain (cap: VM_STACK_POOL_MAX) */` |
|         - | 4415 | `	sxu32 nIdleOperandSlots;   /* Slots parked across those buffers. The pool's real cost is` |
|         - | 4416 | `	                            * memory, not entries, so this -- not the entry count alone --` |
|         - | 4417 | `	                            * is what bounds it (VM_STACK_POOL_SLOTS). */` |
|         - | 4418 | `	void *pIdleStackNodes;     /* Freelist of spare VmIdleStack nodes (BYTECODE stage 7b):` |
|         - | 4419 | `	                            * reused across recycle/reuse cycles so a parked buffer's` |
|         - | 4420 | `	                            * wrapper node isn't pool-alloc/freed per call (mirrors` |
|         - | 4421 | `	                            * pIdleCallFrames). Allocator-owned; freed wholesale. */` |
|         - | 4422 | `	int nObDepth;              /* Output handlers currently running (0 outside one) */` |
|         - | 4423 | `	sxu32 nObActive;           /* 1-based index of the buffer whose handler is running` |
|         - | 4424 | `	                            * (0 outside one). php truncates the ob stack at that` |
|         - | 4425 | `	                            * buffer for the duration: ob_get_level()/contents()/` |
|         - | 4426 | `	                            * length()/list_handlers() answer for IT, not for` |
|         - | 4427 | `	                            * whatever is stacked above it. */` |
|         - | 4428 | `	int bConstEnum;            /* Expanding constants to DESCRIBE them` |
|         - | 4429 | `	                            * (get_defined_constants): php reports a deprecated` |
|         - | 4430 | `	                            * constant when it is READ, and listing the table is` |
|         - | 4431 | `	                            * not a read. */` |
|         - | 4432 | `	int bObRefused;            /* An ob call refused from inside a handler ended the` |
|         - | 4433 | `	                            * request: that operation delivers nothing more. */` |
|         - | 4434 | `	VmFrame *pObFrame;         /* Frame that CALLED the running output handler. The` |
|         - | 4435 | `	                            * handler's own body runs in a deeper frame, so` |
|         - | 4436 | ``	                            * `nObDepth > 0 && pFrame != pObFrame` is "we are`` |
|         - | 4437 | `	                            * inside the handler" — and it stays false for the` |
|         - | 4438 | `	                            * in-place catch PHL runs, in the caller's frame,` |
|         - | 4439 | `	                            * when the handler throws. */` |
|         - | 4440 | `	int nExceptDepth;          /* Exception depth */` |
|         - | 4441 | `	int nExcCtorDepth;         /* Engine-raised throws whose exception __construct is running` |
|         - | 4442 | `	                            * (VmExcCtorEnter): caps the self-feeding case where building` |
|         - | 4443 | `	                            * an exception throws again. */` |
|         - | 4444 | `	sxu32 nLazyInitLine;       /* While a LAZY class initializer runs (a static property's` |
|         - | 4445 | `	                            * deferred default, a class constant's on-demand evaluation):` |
|         - | 4446 | `	                            * the line of the ACCESS that triggered it. A Throwable born` |
|         - | 4447 | `	                            * in the initializer's OWN bytecode is stamped with THIS line` |
|         - | 4448 | `	                            * rather than the initializer's, because that is where php` |
|         - | 4449 | `	                            * evaluates the expression. 0 = not in one, or the access site` |
|         - | 4450 | `	                            * was internal (prelude) code whose line means nothing in the` |
|         - | 4451 | `	                            * file the stamp names. See PH7_VmStampThrowableSite. */` |
|         - | 4452 | `	sxi32 nLazyInitDepth;      /* nVmExecDepth of that initializer's own activation. The` |
|         - | 4453 | `	                            * override applies at THIS depth only: anything the` |
|         - | 4454 | `	                            * initializer manages to call — an autoloader, a nested` |
|         - | 4455 | `	                            * constant's evaluation — runs its own lines and keeps them. */` |
|         - | 4456 | `	int nMuteThrow;            /* > 0 while an initializer runs MUTED (VmEvalDefaultMuted): an` |
|         - | 4457 | `	                            * uncaught throw runs no exception handler, prints no report and` |
|         - | 4458 | `	                            * leaves iExitStatus alone, because php has not reached that code` |
|         - | 4459 | `	                            * yet. Depth-counted (an initializer can mount another class). */` |
|         - | 4460 | `	int nSpeculative;          /* > 0 while a program is run only to LOOK at the value it would` |
|         - | 4461 | `	                            * produce (PH7_VmEvalConstExpr, which renders a parameter default` |
|         - | 4462 | `	                            * for a declaration message). php's own compiler folds such an` |
|         - | 4463 | `	                            * expression and gives up the moment evaluating it raises` |
|         - | 4464 | `	                            * ANYTHING, so nothing raised here may be observable: no user` |
|         - | 4465 | `	                            * error handler runs, no error_get_last() record is written and` |
|         - | 4466 | `	                            * nothing is printed. Depth-counted like nMuteThrow, which mutes` |
|         - | 4467 | `	                            * the THROW half of the same window. */` |
|         - | 4468 | `	sxu32 nSpecDiag;           /* Diagnostics dropped by nSpeculative, monotonic. A speculative` |
|         - | 4469 | `	                            * evaluation that moved this counter is one php would not have` |
|         - | 4470 | ``	                            * folded, so its caller renders php's `<expression>` instead. */`` |
|         - | 4471 | `	int closure_cnt;           /* Loaded closures counter */` |
|         - | 4472 | `	int json_rc;               /* JSON return status [refer to json_encode()/json_decode()]*/` |
|         - | 4473 | `	sxi32 iLcgS1;              /* php's combined LCG, the generator behind uniqid()'s $more_entropy` |
|         - | 4474 | `	                            * tail (and php's own lcg_value()). Two L'Ecuyer streams whose` |
|         - | 4475 | `	                            * DIFFERENCE is the answer; seeded lazily from the clock and the` |
|         - | 4476 | `	                            * engine's own entropy, once per VM, the way php seeds its pair` |
|         - | 4477 | `	                            * once per process. */` |
|         - | 4478 | `	sxi32 iLcgS2;` |
|         - | 4479 | `	int bLcgSeeded;            /* ...and whether that has happened yet */` |
|         - | 4480 | `	sxu32 nNextObjId;          /* Next object handle id to hand out (monotonic; reset to 1 per exec` |
|         - | 4481 | `	                            * so a reused VM looks like a fresh process). See ph7_class_instance.nObjId */` |
|         - | 4482 | `	ProcErrLog xErrLog;        /* error_log() consumer [refer to PH7_VM_CONFIG_ERR_LOG_HANDLER] */` |
|         - | 4483 | `	sxu32 nOutputLen;          /* Total number of generated output */` |
|         - | 4484 | `	ph7_output_consumer sVmConsumer; /* Registered output consumer callback */` |
|         - | 4485 | `	ph7_output_consumer sVmErrConsumer; /* Diagnostics (stderr) consumer [PH7_VM_CONFIG_ERR_STREAM].` |
|         - | 4486 | `	                            * When xConsumer is 0 the log copy falls back to sVmConsumer so` |
|         - | 4487 | `	                            * embedders that never wire a stderr stream still see diagnostics. */` |
|         - | 4488 | `	int iAssertFlags;          /* Assertion flags */` |
|         - | 4489 | `	ph7_value sAssertCallback; /* Callback to call on failed assertions */` |
|         - | 4490 | `	void **apRefObj;           /* Reference WORD per memory-object slot, INDEXED BY SLOT:` |
|         - | 4491 | `	                            * apRefObj[nIdx] describes the holders of aMemObj[nIdx], or` |
|         - | 4492 | `	                            * is 0 when nothing has ever been registered against it. A` |
|         - | 4493 | `	                            * slot index is already a dense small integer, so hashing it` |
|         - | 4494 | `	                            * bought nothing and cost a rehash of every record each time` |
|         - | 4495 | `	                            * the table doubled. See VM_REF_TAG_* for what a word says --` |
|         - | 4496 | `	                            * nearly every slot's answer fits in the word itself and` |
|         - | 4497 | `	                            * allocates no record at all. */` |
|         - | 4498 | `	sxu32 nRefSize;            /* apRefObj[] length, in slots */` |
|         - | 4499 | `	sxu32 nRefUsed;            /* Cells currently filled (a word or a record) */` |
|         - | 4500 | `	SySet aSelf;               /* 'self' stack used for static member access [i.e: self::MyConstant] */` |
|         - | 4501 | `	ph7_hashmap *pGlobal;      /* $GLOBALS hashmap */` |
|         - | 4502 | `	sxu32 nGlobalIdx;          /* $GLOBALS index */` |
|         - | 4503 | `	SySet aCallSite;           /* VmCallSite -- one per PH7_OP_CALL site that has run, holding` |
|         - | 4504 | `	                            * the function-table entry its callee name resolved to. Indexed` |
|         - | 4505 | `	                            * by VmInstr.nSite - 1, and claimed only by a site that actually` |
|         - | 4506 | `	                            * executes. */` |
|         - | 4507 | `	SyHash hCallName;          /* The callee names aCallSite records point at, interned. 43,375` |
|         - | 4508 | `	                            * call sites execute on the ecosystem gate's phpcs step and they` |
|         - | 4509 | `	                            * spell only a few thousand distinct names between them, so a` |
|         - | 4510 | `	                            * copy per SITE was 2.8 MB where a copy per NAME is a fifth of` |
|         - | 4511 | `	                            * one -- and the shared copy is the one already in cache when` |
|         - | 4512 | `	                            * the next site checks its own record. Keyed by the name BYTES` |
|         - | 4513 | `	                            * (case-sensitively: a site spells its callee the same way every` |
|         - | 4514 | `	                            * time), and the entry's key IS the interned copy. */` |
|         - | 4515 | `	sxu32 nFreeCallSite;       /* Head of aCallSite's free list (index + 1, 0 = empty). An` |
|         - | 4516 | `	                            * eval()/include compiles into a bytecode container that is` |
|         - | 4517 | `	                            * RELEASED when the chunk finishes, so the records its call` |
|         - | 4518 | `` 	                            * sites claimed go back here -- without it, `while(1) eval(...)` `` |
|         - | 4519 | `	                            * would grow aCallSite for ever. */` |
|         - | 4520 | `	sxu32 nCallableGen;        /* Bumped whenever the set of things a NAME can call changes --` |
|         - | 4521 | `	                            * a function, a class or a host function installed or removed.` |
|         - | 4522 | `	                            * PH7_OP_CALL_INIT stamps a call site it has screened with the` |
|         - | 4523 | `	                            * generation it screened at, so a site whose callee is a` |
|         - | 4524 | `	                            * compile-time constant asks the question once per generation` |
|         - | 4525 | `	                            * instead of once per call. Starts at 1: 0 is 'never screened'. */` |
|         - | 4526 | `	sxu32 nConstGen;           /* The same idea for the CONSTANT table: bumped whenever a name` |
|         - | 4527 | `	                            * is installed in or removed from hConstant. A PH7_OP_LOADC` |
|         - | 4528 | `	                            * site's answer can only change then -- both the constant it` |
|         - | 4529 | `	                            * resolved to and, for a namespaced site, WHICH of its two` |
|         - | 4530 | `	                            * candidate names won -- so a site stamped with this generation` |
|         - | 4531 | `	                            * skips the lookups. Starts at 1: 0 is 'never resolved'. */` |
|         - | 4532 | `	sxu32 nCurLine;            /* Line of the instruction currently executing (0 outside the` |
|         - | 4533 | `	                            * dispatch loop). Every runtime diagnostic, debug_backtrace()` |
|         - | 4534 | `	                            * and Throwable reads its line from here. */` |
|         - | 4535 | `	sxu8 bCurStrict;           /* strict_types mode of the unit that instruction came from,` |
|         - | 4536 | `	                            * published beside nCurLine. Read by the argument binder when` |
|         - | 4537 | `	                            * an OP_CALL carries no compiled call map — which is every` |
|         - | 4538 | `	                            * ENGINE-dispatched call (magic method, property hook), where` |
|         - | 4539 | `	                            * php still applies the calling file's mode. */` |
|         - | 4540 | `	sxi32 nLastErrType;        /* error_get_last(): severity of the last UNHANDLED diagnostic` |
|         - | 4541 | `	                            * (0 = none yet). php records one even when '@' or` |
|         - | 4542 | `	                            * error_reporting() hides it, but NOT when a user handler` |
|         - | 4543 | `	                            * claimed it by returning true. */` |
|         - | 4544 | `	sxu32 nLastErrLine;        /* ... its line */` |
|         - | 4545 | `	SyBlob sLastErrMsg;        /* ... its message */` |
|         - | 4546 | `	SyBlob sLastErrFile;       /* ... its file */` |
|         - | 4547 | `	char zDisplayName[256];    /* Scratch for PH7_VmFuncDisplayName: a closure's INTERNAL name is a` |
|         - | 4548 | `	                            * synthesized unique key ("[closure_3]"), but php shows` |
|         - | 4549 | `	                            * "{closure:file:line}". Valid until the next call. */` |
|         - | 4550 | `	sxu32 nSuperBaseline;      /* SySetUsed(aMemObj) snapshot taken in PH7_VmMakeReady` |
|         - | 4551 | `								* right before the superglobals are created. ph7_vm_reset()` |
|         - | 4552 | `								* releases and truncates aMemObj back to this watermark then` |
|         - | 4553 | `								* rebuilds the per-exec object graph, so a compiled VM can be` |
|         - | 4554 | `								* re-executed (compile-once / execute-many) without state` |
|         - | 4555 | `								* bleed or unbounded heap growth. */` |
|         - | 4556 | `	/* Index of the shared empty-string literal reserved at VM init */` |
|         - | 4557 | `	sxu32 nEmptyStringIdx;` |
|         - | 4558 | `	/* Argument-unpacking capture (PHP 8.1 named-parameter semantics for spreads).` |
|         - | 4559 | `	 * Populated by OP_SPREAD; CALL/NEW derive each call's own arg-count growth from` |
|         - | 4560 | `	 * these runs (VmSpreadOwnExtra) and replay the keys (VmBuildEffectiveArgMap),` |
|         - | 4561 | `	 * then consume this call's runs. See the VmSpreadRun/VmSpreadKey machinery in vm.c. */` |
|         - | 4562 | `	SySet aSpreadRun;          /* VmSpreadRun: one entry per expansion in the current arg list */` |
|         - | 4563 | `	sxu32 nSpreadCallBase;     /* Index into aSpreadRun of the first run owned by the CALL/NEW` |
|         - | 4564 | `	                            * currently dispatching (VmSpreadOwnExtra records it; the replay` |
|         - | 4565 | `	                            * and consume use it instead of an ambiguous pStart scan, which a` |
|         - | 4566 | ``	                            * zero-width `...[]` run sharing a nested call's base slot fooled) */`` |
|         - | 4567 | `	SySet aSpreadKey;          /* VmSpreadKey: one (off,len) per expanded element, in order */` |
|         - | 4568 | `	SyBlob sSpreadKeyBlob;     /* Backing bytes for the string keys referenced by aSpreadKey */` |
|         - | 4569 | `	SySet aEffArgName;         /* SyString: effective per-actual-slot arg names built at CALL */` |
|         - | 4570 | `	const char *zCmpRefusalClass; /* A native compare handler (ph7_class::xCmp) REFUSED the pair,` |
|         - | 4571 | `	                            * and this is the exception class it named -- php throws` |
|         - | 4572 | `	                            * DateException out of the DateTimeZone handler. Recorded rather` |
|         - | 4573 | `	                            * than raised because PH7_MemObjCmp has no throw boundary: it runs` |
|         - | 4574 | `	                            * under sort(), in_array() and max() as often as under an operator.` |
|         - | 4575 | `	                            * The sites that DO have one (the comparison opcodes, the switch` |
|         - | 4576 | `	                            * arm, the host-call boundary) raise it through` |
|         - | 4577 | `	                            * PH7_CmpRefusalRaise. FIRST refusal wins, like nBoundaryRc: a` |
|         - | 4578 | `	                            * driver that keeps comparing after one must not overwrite the` |
|         - | 4579 | `	                            * message the script will see. 0 when none is pending. */` |
|         - | 4580 | `	char zCmpRefusalMsg[160];  /* ...and its wording, copied out of the hook's context */` |
|         - | 4581 | `	sxi32 iCmpCallbackExc;     /* The dispatch STATUS a comparison callback did not return with` |
|         - | 4582 | `								* (PH7_EXCEPTION, or PH7_ABORT for an UNCAUGHT throw), so the` |
|         - | 4583 | `								* driver (usort/uasort/uksort and the array_udiff/` |
|         - | 4584 | `								* array_uintersect families) can abort and propagate exactly` |
|         - | 4585 | `								* it. Zero when no comparison raised; a comparator has no` |
|         - | 4586 | `								* status channel, so this latch is the only way out. */` |
|         - | 4587 | `	int iMbEncoding;           /* mbstring's internal encoding, an MB_ENC_* id from` |
|         - | 4588 | `								* builtin_mb.c; 0 is UTF-8, which is why zeroing the` |
|         - | 4589 | `								* VM leaves php's default in place. */` |
|         - | 4590 | `	sxu8 aMbDetectOrder[8];    /* mbstring's DETECT ORDER, as builtin_mb.c detect ids. It is` |
|         - | 4591 | ``	                            * what `mb_detect_encoding($s)` walks with no list of its`` |
|         - | 4592 | ``	                            * own, and what `mb_detect_order()` reads and writes. NOT`` |
|         - | 4593 | ``	                            * what the name `auto` means: that one is the LANGUAGE's`` |
|         - | 4594 | `	                            * default order (ASCII, UTF-8) whatever this holds --` |
|         - | 4595 | `	                            * probed, because the two read alike in the default state` |
|         - | 4596 | `	                            * and only diverge once a script has set an order. */` |
|         - | 4597 | `	sxu8 nMbDetectOrder;       /* how many of them; 0 at VM init means the default pair. */` |
|         - | 4598 | `	sxi32 iMbSubstitute;       /* mbstring's substitute code point ('?' at VM init;` |
|         - | 4599 | `								* 0 is a code point a script may really ask for). */` |
|         - | 4600 | `	sxu8 iMbSubstMode;         /* how it is written: builtin_mb.c's MB_SUBST_* — the` |
|         - | 4601 | `								* code point itself, nothing at all, or the U+/entity` |
|         - | 4602 | `								* spelling of what could not be represented. php keeps` |
|         - | 4603 | `								* the two apart, so setting "long" does not forget the` |
|         - | 4604 | `								* code point an error character still takes. */` |
|         - | 4605 | `	sxi32 iExitStatus;         /* Script exit status */` |
|         - | 4606 | `	sxu8 bHaltRequested;       /* Set by exit/die (OP_HALT or the builtin) so the halt` |
|         - | 4607 | `								* cascades out of nested execution units (include/require/` |
|         - | 4608 | `								* eval chunks) instead of hard-exiting the process; the` |
|         - | 4609 | `								* top-level executor then runs shutdown callbacks normally. */` |
|         - | 4610 | `	sxu8 bInReset;             /* Set while ph7_vm_reset() bulk-releases the per-exec` |
|         - | 4611 | `								* object pool. Suppresses user __destruct invocation during` |
|         - | 4612 | `								* that teardown: destructors would run arbitrary PHP against a` |
|         - | 4613 | `								* half-reset VM (reference table already gone, $GLOBALS` |
|         - | 4614 | `								* nulled). PH7 never ran` |
|         - | 4615 | `								* global-scope destructors before (release nuked the arena),` |
|         - | 4616 | `								* so this preserves prior semantics while staying crash-safe.` |
|         - | 4617 | `								* Engine-level instance memory is still reclaimed. */` |
|         - | 4618 | `	sxu8 bNoFrameLoc;          /* Set around a diagnostic raised with NO php frame under it.` |
|         - | 4619 | `								* php then has no file and no line to name and reports the` |
|         - | 4620 | ``								* location as `in Unknown on line 0` (its`` |
|         - | 4621 | `								* EG(current_execute_data) == NULL branch). See` |
|         - | 4622 | `								* VmDiagnosticWhere. */` |
|         - | 4623 | `	sxu8 bShutdownAborted;     /* Set when a destructor in the shutdown pass left an uncaught` |
|         - | 4624 | `								* throwable. php's phase runs under one zend_try, so the first` |
|         - | 4625 | `								* bailout abandons every destructor still owed -- the flag is` |
|         - | 4626 | `								* what carries that decision across the two passes. */` |
|         - | 4627 | `	sxu8 bInShutdownDtor;      /* Set while the shutdown destructor pass runs (php's` |
|         - | 4628 | `								* zend_call_destructors, between the shutdown callbacks and` |
|         - | 4629 | `								* the output-buffer flush). php reads this state as` |
|         - | 4630 | ``								* `EG(current_execute_data) == NULL`: a non-public __destruct`` |
|         - | 4631 | `								* reached with no PHP frame on the stack is not the Error a` |
|         - | 4632 | `								* running program gets but an E_WARNING that says the call was` |
|         - | 4633 | `								* ignored, and the object is left undestructed. */` |
|         - | 4634 | `	ph7_gen_state sCodeGen;    /* Code generator module */` |
|         - | 4635 | `	sxu32 nLastEvalErr;        /* Compile-error count of the most recent VmEvalChunk unit. Unlike` |
|         - | 4636 | `								* sCodeGen.nErr it survives the nested-compile state save/restore,` |
|         - | 4637 | `								* so VmExecDeferredClass can tell whether ITS chunk failed even` |
|         - | 4638 | `								* when the deferred declaration executes inside an outer compile` |
|         - | 4639 | `								* (an autoload-during-compile require). */` |
|         - | 4640 | `	sxu32 nAnonSeq;            /* Anonymous-class sequence number, appended to the synthesized` |
|         - | 4641 | ``								* name as `$%x`. php's CG(rtd_key_counter): one counter for the`` |
|         - | 4642 | `								* whole request, bumped in COMPILE order, which is what makes` |
|         - | 4643 | `								* two anonymous classes written on the same line distinguishable. */` |
|         - | 4644 | `	SyString sDeferAnonName;   /* One-shot synthesized-name override for the next anonymous-class` |
|         - | 4645 | `								* compile: set by VmExecDeferredClass before re-compiling a` |
|         - | 4646 | ``								* deferred `new class ... {}` chunk so the runtime-installed`` |
|         - | 4647 | `								* class carries the SAME name the site's OP_NEW loads; consumed` |
|         - | 4648 | `								* (cleared) by PH7_CompileAnnonClass. {0,0} otherwise. */` |
|         - | 4649 | `	int nReflectFactory;       /* > 0 while a ReflectionClass door is BUILDING a member reflector` |
|         - | 4650 | `	                            * rather than a user calling its constructor. php's own doors do` |
|         - | 4651 | `	                            * not go through ReflectionMethod::__construct at all -- they are` |
|         - | 4652 | `	                            * handed the function directly -- which is why` |
|         - | 4653 | ``	                            * `(new ReflectionClass('Closure'))->getMethod('__invoke')` answers`` |
|         - | 4654 | ``	                            * and `new ReflectionMethod('Closure','__invoke')` refuses. Here`` |
|         - | 4655 | `	                            * both spellings reach the same constructor, so this is what tells` |
|         - | 4656 | `	                            * them apart. Depth-counted; nothing user-visible runs inside the` |
|         - | 4657 | `	                            * window (the constructor it brackets is native). */` |
|         - | 4658 | `	ph7_exec_ctx *pActiveCtx;  /* Currently executing fiber/generator context (NULL in normal code) */` |
|         - | 4659 | `#ifdef PH7_CORO_STACK` |
|         - | 4660 | `	ph7_exec_ctx *pCoroCtx;    /* The fiber whose own native STACK is the one executing, or NULL.` |
|         - | 4661 | `	                            * Unlike pActiveCtx this does not change when the fiber's body` |
|         - | 4662 | `	                            * drives a generator, so it answers the one question the throw` |
|         - | 4663 | `	                            * path asks: is there a fiber to leave? Travels with the` |
|         - | 4664 | `	                            * VM-state swap, so nesting one fiber inside another restores` |
|         - | 4665 | `	                            * the outer one by construction. */` |
|         - | 4666 | `#endif` |
|         - | 4667 | `	ph7_class_instance *pCurFiber; /* The Fiber whose body the running code is inside, or NULL --` |
|         - | 4668 | `	                            * php's EG(active_fiber), which is what Fiber::getCurrent()` |
|         - | 4669 | `	                            * answers. Distinct from pActiveCtx: that one is whatever` |
|         - | 4670 | `	                            * coroutine is executing (a GENERATOR started inside a fiber` |
|         - | 4671 | `	                            * is the active ctx while the fiber is still the current one),` |
|         - | 4672 | `	                            * and it names no object. Saved and restored around a fiber's` |
|         - | 4673 | `	                            * start/resume, so nesting is the call structure itself. */` |
|         - | 4674 | `	ph7_class *pFiberClass;    /* Cached Fiber class pointer for fast dispatch */` |
|         - | 4675 | `	ph7_class *pGeneratorClass; /* Cached Generator class pointer */` |
|         - | 4676 | `	ph7_class *pClosureClass;  /* Cached Closure class pointer (closures are instances of it) */` |
|         - | 4677 | `	ph7_class_instance *pClosureThis; /* Transient: bound $this for a bound PLAIN closure about to be` |
|         - | 4678 | `	                                   * invoked, set by VmClosureUnwrap, consumed (ref transferred) at` |
|         - | 4679 | `	                                   * the OP_CALL user-function frame setup. Owns one reference. */` |
|         - | 4680 | `	ph7_class *pClosureScope; /* Transient: bound $__scope class for the same bound PLAIN closure` |
|         - | 4681 | `	                           * (private/protected visibility override); consumed alongside pClosureThis. */` |
|         - | 4682 | `	ph7_class *pStdClass;      /* Cached stdClass pointer (target of (object) cast + dynamic props) */` |
|         - | 4683 | `	ph7_class *pIncClass;      /* Cached __PHP_Incomplete_Class pointer: unserialize()'s carrier for a` |
|         - | 4684 | `	                            * disallowed or unknown class. Every script-level property access or` |
|         - | 4685 | `	                            * method call on an instance is php's incomplete-object diagnostic` |
|         - | 4686 | `	                            * (PH7_VmIncompleteMsg); the engine itself reads hAttr freely. */` |
|         - | 4687 | `	ph7_class *pArrayAccessClass; /* Cached ArrayAccess interface pointer */` |
|         - | 4688 | `	ph7_class *pCountableClass;   /* Cached Countable interface pointer */` |
|         - | 4689 | `	ph7_class *pStringableClass;  /* Cached Stringable interface pointer */` |
|         - | 4690 | `	ph7_class *pJsonSerializableClass; /* Cached JsonSerializable interface pointer */` |
|         - | 4691 | `	ph7_class *pTraversableClass; /* Cached Traversable interface pointer (iterable type check) */` |
|         - | 4692 | `	/* Pending null-coalesce-assign target on an ArrayAccess subscript.` |
|         - | 4693 | `	 * Set by LOAD_IDX iP2=3 when the key is missing on an ArrayAccess` |
|         - | 4694 | `	 * object; consumed by NULLC_STORE so it can dispatch to offsetSet` |
|         - | 4695 | `	 * instead of writing through the (synthetic) pNos->nIdx. NULLC_STORE` |
|         - | 4696 | `	 * always clears it, matched or not. */` |
|         - | 4697 | `	ph7_class_instance *pCoalesceObj;` |
|         - | 4698 | `	ph7_value sCoalesceKey;` |
|         - | 4699 | `	int bCoalesceArmed;` |
|         - | 4700 | `#ifdef PH7_ENABLE_PCRE` |
|         - | 4701 | `	int iPcreLastError;        /* preg_last_error() return value */` |
|         - | 4702 | `	/* mbstring's regex family (vm_pcre.c). Every field reads as php's default` |
|         - | 4703 | `	 * when it is ZERO, so a freshly zeroed VM already answers "UTF-8" and "pr"` |
|         - | 4704 | `	 * and needs no init hook of its own: iMbReOpt carries MBRE_OPT_SET once a` |
|         - | 4705 | `	 * script has set options, iMbReSyntax holds the syntax letter and 0 means` |
|         - | 4706 | `	 * 'r', and iMbReEnc is an index into builtin_mb.c's encoding table whose` |
|         - | 4707 | `	 * entry 0 is UTF-8. The search state is allocated out of the VM allocator,` |
|         - | 4708 | `	 * so it goes away with the VM. */` |
|         - | 4709 | `	sxu32 iMbReOpt;            /* mb_regex_set_options() bits, 0 = untouched */` |
|         - | 4710 | `	sxu8 iMbReSyntax;          /* its syntax letter, 0 = 'r' */` |
|         - | 4711 | `	int iMbReEnc;              /* mb_regex_encoding(), an encoding-table index */` |
|         - | 4712 | `	char *zMbReStr;            /* mb_ereg_search_init()'s subject, 0 = none set */` |
|         - | 4713 | `	sxu32 nMbReStr;` |
|         - | 4714 | `	char *zMbRePat;            /* ...and its pattern, 0 = none set */` |
|         - | 4715 | `	sxu32 nMbRePat;` |
|         - | 4716 | `	sxu32 iMbReOptCur;         /* the options that pattern was set with */` |
|         - | 4717 | `	sxu8 iMbReSynCur;` |
|         - | 4718 | `	sxu32 iMbRePos;            /* the search cursor, a BYTE offset */` |
|         - | 4719 | `	sxu32 *aMbReOv;            /* the last match's offsets, 2 per group */` |
|         - | 4720 | `	int nMbReOv;               /* how many groups are in there; 0 = no match yet */` |
|         - | 4721 | `#endif` |
|         - | 4722 | `#ifdef PH7_ENABLE_LIBXML` |
|         - | 4723 | `	SySet aLibxmlErr;          /* Queued phl_libxml_err entries (libxml_get_errors) */` |
|         - | 4724 | `	SyBlob sLibxmlPend;        /* libxml message text held back because it has no trailing` |
|         - | 4725 | `	                            * newline: php buffers such a fragment and prints it JOINED` |
|         - | 4726 | `	                            * to the next diagnostic, whenever that arrives (see` |
|         - | 4727 | `	                            * PH7_LibxmlCaptureEnd). Reset per request. */` |
|         - | 4728 | `	int bLibxmlInternalErr;    /* libxml_use_internal_errors(true) is active */` |
|         - | 4729 | `	void *pLibxmlLastErr;      /* phl_libxml_err* slot backing libxml_get_last_error */` |
|         - | 4730 | `	void *pXmlDocs;            /* phl_xmldoc registry chain; freed on reset/release */` |
|         - | 4731 | `	void *pXmlLimbo;           /* The OWNERLESS shell (phl_xmldoc with no xmlDoc): every` |
|         - | 4732 | ``	                            * constructed-but-never-adopted DOM node -- php's `new`` |
|         - | 4733 | ``	                            * DOMText('t')`, whose node has NO document until the first`` |
|         - | 4734 | `	                            * insertion adopts it -- parks on its orphan set, freed with` |
|         - | 4735 | `	                            * the registry chain it sits on. Lazily created by the DOM's` |
|         - | 4736 | `	                            * constructors; reset to 0 whenever the chain is freed. */` |
|         - | 4737 | `	void *pXmlWriters;         /* XMLWriter registry chain; freed on reset/release */` |
|         - | 4738 | `	void *pXmlParsers;         /* phl_xmlparser registry chain (ext/xml); freed on reset/release */` |
|         - | 4739 | `	void *pPdoConns;           /* phl_pdo registry chain (ext/pdo); freed on reset/release --` |
|         - | 4740 | `	                            * a sqlite3 handle lives outside SyMemBackend, so the` |
|         - | 4741 | `	                            * wholesale release would leak both it and the file lock */` |
|         - | 4742 | `	void *pSq3Conns;           /* phl_sq3 registry chain (ext/sqlite3); freed on reset/release.` |
|         - | 4743 | `	                            * A SEPARATE chain from pPdoConns: the two extensions share` |
|         - | 4744 | `	                            * libsqlite3 and nothing else -- different error model, different` |
|         - | 4745 | `	                            * open flags, different object -- so they own their handles apart */` |
|         - | 4746 | `	void *pCurlHandles;        /* phl_curl registry chain (ext/curl); freed on reset/release --` |
|         - | 4747 | `	                            * a CURL* lives outside SyMemBackend too, and holds a socket` |
|         - | 4748 | `	                            * and a connection cache with it */` |
|         - | 4749 | `	void *pCurlMultis;         /* phl_curlm registry chain (ext/curl); swept BEFORE` |
|         - | 4750 | `	                            * pCurlHandles, since a multi still holds the easy handles` |
|         - | 4751 | `	                            * that were added to it */` |
|         - | 4752 | `	void *pCurlShares;         /* phl_curlsh registry chain (ext/curl); swept AFTER` |
|         - | 4753 | `	                            * pCurlHandles, since a CURLSH an easy handle still names` |
|         - | 4754 | `	                            * refuses to be cleaned up */` |
|         - | 4755 | `	ph7_value sXmlEntLoader;   /* libxml_set_external_entity_loader()'s callable; NULL = default.` |
|         - | 4756 | `	                            * Stored and answered, never invoked: no PHL parse path loads an` |
|         - | 4757 | `	                            * external entity (php's sanitized defaults keep it off too) —` |
|         - | 4758 | `	                            * a recorded divergence. */` |
|         - | 4759 | `	ph7_value sXmlStreamsCtx;  /* libxml_set_streams_context()'s stream-context resource; read by` |
|         - | 4760 | `	                            * nothing until an http:// wrapper exists. */` |
|         - | 4761 | `#endif` |
|         - | 4762 | `	void *pPhars;              /* phl_phar registry chain (ext/phar): every archive this run` |
|         - | 4763 | `	                            * opened, freed on reset/release. php's own cache is` |
|         - | 4764 | `	                            * per-request and behaves the same way. */` |
|         - | 4765 | `	void *pZips;               /* phl_zip registry chain (ext/zip): every archive a ZipArchive` |
|         - | 4766 | ``	                            * or a `zip://` open is holding, freed on reset/release */`` |
|         - | 4767 | `	void *pLastDir;            /* php's "last opened directory stream": the io_private the` |
|         - | 4768 | `	                            * most recent opendir() handed out, which readdir(),` |
|         - | 4769 | `	                            * rewinddir() and closedir() fall back to when they are` |
|         - | 4770 | `	                            * given null (deprecated since 8.1). Cleared when THAT` |
|         - | 4771 | `	                            * handle is closed and at reset; never owns anything. */` |
|         - | 4772 | `	SyBlob sPharRunning;       /* The archive the running script came from, as Phar::running()` |
|         - | 4773 | `	                            * answers it: set by Phar::mapPhar(), empty outside one. */` |
|         - | 4774 | `#ifdef PH7_ENABLE_NET` |
|         - | 4775 | `	void *pSockets;            /* phl_socket registry chain (ext/sockets); freed on reset/release` |
|         - | 4776 | `	                            * -- a DESCRIPTOR is not the allocator's, so the wholesale` |
|         - | 4777 | `	                            * release would leak the file handle and its port */` |
|         - | 4778 | `	void *pAddrInfos;          /* phl_addrinfo registry chain (ext/sockets), same rule: each` |
|         - | 4779 | `	                            * record holds a copied ai_canonname of its own */` |
|         - | 4780 | `	int iSocketLastErr;        /* php's SOCKETS_G(last_error): the per-REQUEST errno` |
|         - | 4781 | `	                            * socket_last_error() answers with no argument, beside the` |
|         - | 4782 | `	                            * per-socket one every record carries */` |
|         - | 4783 | `#endif` |
|         - | 4784 | `	SyBlob sPharErr;           /* The phar wrapper's open-failure sentence. It has to outlive the` |
|         - | 4785 | `	                            * xOpen that formatted it -- the engine keeps the POINTER and the` |
|         - | 4786 | `	                            * caller prints it after the open returned -- so it cannot be a` |
|         - | 4787 | `	                            * stack buffer (ASan caught exactly that). */` |
|         - | 4788 | `#ifdef PH7_ENABLE_ZLIB` |
|         - | 4789 | `	void *pZlibCtx;            /* phl_zctx registry chain (ext/zlib); freed on reset/release --` |
|         - | 4790 | `	                            * a z_stream's window is libz's own allocation, outside` |
|         - | 4791 | `	                            * SyMemBackend, so the wholesale release would leak it */` |
|         - | 4792 | `	int iZlibLevel;            /* the compression level the NEXT compress.zlib open uses, and` |
|         - | 4793 | `	                            * the strategy with it: gzopen()'s mode string carries both` |
|         - | 4794 | `	                            * ("wb9f") and an xOpen is handed flags rather than the string,` |
|         - | 4795 | `	                            * so the door that parsed them arms them here. Reset to libz's` |
|         - | 4796 | `	                            * defaults by the open that reads them. */` |
|         - | 4797 | `	int iZlibStrategy;` |
|         - | 4798 | `	int bZlibDirect;           /* 1 while a gzopen()-family open is in flight. The two doors` |
|         - | 4799 | `	                            * onto this device report a failure differently: gzopen() reads` |
|         - | 4800 | `	                            * as the FILE open it is ("No such file or directory"), while` |
|         - | 4801 | `	                            * compress.zlib:// is a wrapper and php gives every one of its` |
|         - | 4802 | `	                            * failures the same flat "operation failed". */` |
|         - | 4803 | `#endif` |
|         - | 4804 | `#ifdef PH7_ENABLE_OPENSSL` |
|         - | 4805 | `	void *pSslObjs;            /* phl_ssl_obj registry chain (ext/openssl); freed on reset/release` |
|         - | 4806 | `	                            * -- an X509/EVP_PKEY/X509_REQ is OpenSSL's own allocation, outside` |
|         - | 4807 | `	                            * SyMemBackend, so the wholesale release would leak it */` |
|         - | 4808 | `	void *pSslErrors;          /* phl_ssl_errors: php's 16-slot ring, drained from OpenSSL's own` |
|         - | 4809 | `	                            * error queue after a failure and read one entry at a time by` |
|         - | 4810 | `	                            * openssl_error_string() */` |
|         - | 4811 | `#endif` |
|         - | 4812 | `	/* php numbers every resource with a small sequential id that (int) casts and` |
|         - | 4813 | `	 * "Resource id #N" render, and that distinguishes two live resources from one` |
|         - | 4814 | `	 * another. PHL's resource value is a bare void*, so the id lives in this` |
|         - | 4815 | `	 * per-VM registry: pointer -> phl_res_id, assigned on first observation.` |
|         - | 4816 | `	 * Freed with the VM (ids are never recycled, as php's may be). */` |
|         - | 4817 | `	SyHash hResourceId;        /* void* -> phl_res_id* */` |
|         - | 4818 | `	sxu32 nResourceIdNext;     /* Next id to hand out (php's start at 1) */` |
|         - | 4819 | `	/* Stream contexts (stream_context_create). The chain owns every context the` |
|         - | 4820 | `	 * script made; pDefaultCtx is the one stream_context_get_default() hands` |
|         - | 4821 | `	 * back and every opener falls back to. */` |
|         - | 4822 | `	void *pStreamCtx;          /* phl_stream_ctx registry chain; freed on reset */` |
|         - | 4823 | `	void *pDefaultCtx;         /* phl_stream_ctx* — the default context, or 0 */` |
|         - | 4824 | `	void *pOpenCtx;            /* the context the open in flight runs under */` |
|         - | 4825 | `	char zOpenMode[16];        /* the mode string the open in flight was ASKED with, when a` |
|         - | 4826 | `	                            * caller had one: php hands a userland wrapper's stream_open()` |
|         - | 4827 | `	                            * the caller's own spelling ('rb', 'w+', 'x'), and PHL could` |
|         - | 4828 | `	                            * only rebuild an approximation from the flag bits -- so` |
|         - | 4829 | `	                            * file_put_contents() told a wrapper it was opening for` |
|         - | 4830 | `	                            * READING. Empty when the opener has no string of its own` |
|         - | 4831 | `	                            * (the C-level readers), and cleared after every open. */` |
|         - | 4832 | `	/* What a FAILED open says. php names the URI the script wrote -- scheme and` |
|         - | 4833 | `	 * all -- and gives the WRAPPER's reason for it, where only the plain-file` |
|         - | 4834 | `	 * wrapper's reason is an errno. PH7_VmGetStreamDevice() advances past the` |
|         - | 4835 | `	 * scheme, so the two halves of the name are remembered here as it does:` |
|         - | 4836 | `	 * zOpenUriTail is the pointer it handed back, and a warning printing THAT` |
|         - | 4837 | `	 * pointer is reporting THIS open and may name the whole thing instead. */` |
|         - | 4838 | `	const char *zOpenUri;      /* the URI as written, or 0 */` |
|         - | 4839 | `	int nOpenUri;              /* its length */` |
|         - | 4840 | `	const char *zOpenUriTail;  /* the scheme-stripped remainder handed to the wrapper */` |
|         - | 4841 | `	const char *zOpenCaller;   /* the FUNCTION reporting this open, for a wrapper that` |
|         - | 4842 | `	                            * raises a diagnostic of its own before the caller's` |
|         - | 4843 | `	                            * (php's resolver failure is two warnings, not one) */` |
|         - | 4844 | `	const char *zOpenErr;      /* the wrapper's own reason for the open in flight, or 0` |
|         - | 4845 | `	                            * for the plain-file wrapper's errno */` |
|         - | 4846 | `	char zOpenErrBuf[512];     /* storage for a reason that has to be BUILT -- a userland` |
|         - | 4847 | `	                            * wrapper names its own class and method, and the http` |
|         - | 4848 | `	                            * wrapper interpolates a host name or a whole status` |
|         - | 4849 | `	                            * line -- since the caller's buffer does not outlive the` |
|         - | 4850 | `	                            * call */` |
|         - | 4851 | `	int nOpenDepth;            /* opens in flight. The three fields above belong to the` |
|         - | 4852 | `	                            * OUTERMOST one: php://filter opens its own resource from` |
|         - | 4853 | `	                            * inside its xOpen, and that inner open would otherwise` |
|         - | 4854 | `	                            * report the RESOURCE's errno under the filter's name --` |
|         - | 4855 | `	                            * and leave zOpenUriTail pointing into a blob it frees on` |
|         - | 4856 | `	                            * the way out. */` |
|         - | 4857 | `	/* The response headers of the last http:// exchange, one line per '\n'. Two` |
|         - | 4858 | ``	 * consumers outlive the handle that produced them: `$http_response_header`,`` |
|         - | 4859 | `	 * which the stream layer writes into the frame that called the opener, and` |
|         - | 4860 | `	 * php 8.4's http_get_last_response_headers(), which answers them until` |
|         - | 4861 | `	 * http_clear_last_response_headers() drops the store. */` |
|         - | 4862 | `	SyBlob sHttpRespHdrs;      /* the lines, '\n'-separated */` |
|         - | 4863 | `	sxu8 bHttpRespHdrs;        /* something has been recorded (the getter's NULL/array split) */` |
|         - | 4864 | `	sxu8 bHttpRespFresh;       /* recorded by the open in flight and not yet published */` |
|         - | 4865 | `	sxu8 bHttpGetHeaders;      /* the open in flight is get_headers()', which php makes` |
|         - | 4866 | ``	                            * two things at once: a context with `ignore_errors` on,`` |
|         - | 4867 | `	                            * so a refused status is an ordinary set of headers, and` |
|         - | 4868 | `	                            * STREAM_ONLY_GET_HEADERS, which skips the dechunk filter` |
|         - | 4869 | `	                            * and so KEEPS the Transfer-Encoding header the ordinary` |
|         - | 4870 | `	                            * read consumes */` |
|         - | 4871 | `	/* Stream filters (stream_filter_append and the php://filter wrapper). The` |
|         - | 4872 | `	 * chain owns every filter INSTANCE the script created, so one that is never` |
|         - | 4873 | `	 * removed still goes back at reset. */` |
|         - | 4874 | `	void *pStreamFilter;       /* phl_stream_filter registry chain; freed on reset */` |
|         - | 4875 | `	void *pUserFilters;        /* stream_filter_register() name => class chain */` |
|         - | 4876 | ``	void *pFilterCall;         /* phl_brigade_res* — the `$out` of the filter() call`` |
|         - | 4877 | `	                            * in flight, which is what stream_bucket_new()` |
|         - | 4878 | `	                            * hangs its token on */` |
|         - | 4879 | `	SyString *pCalleeName;     /* the builtin currently running, for diagnostics` |
|         - | 4880 | `	                            * raised where no ph7_context reaches (see vm_exec.c) */` |
|         - | 4881 | `	ph7_vm *pNext,*pPrev;      /* List of active VM's */` |
|         - | 4882 | `	sxu32 nMagic;              /* Sanity check against misuse */` |
|         - | 4883 | `};` |
|         - | 4884 | `/*` |
|         - | 4885 | ` * Allowed value for ph7_vm.nMagic` |
|         - | 4886 | ` */` |
|         - | 4887 | `#define PH7_VM_INIT   0xFADE9512  /* VM correctly initialized */` |
|         - | 4888 | `#define PH7_VM_RUN    0xEA271285  /* VM ready to execute PH7 bytecode */` |
|         - | 4889 | `#define PH7_VM_EXEC   0xCAFE2DAD  /* VM executing PH7 bytecode */` |
|         - | 4890 | `#define PH7_VM_STALE  0xBAD1DEAD  /* Stale VM */` |
|         - | 4891 | `/*` |
|         - | 4892 | ` * Error codes according to the PHP language reference manual.` |
|         - | 4893 | ` */` |
|         - | 4894 | `enum iErrCode` |
|         - | 4895 | `{` |
|         - | 4896 | `	E_ERROR             = 1,   /* Fatal run-time errors. These indicate errors that can not be recovered` |
|         - | 4897 | `							    * from, such as a memory allocation problem. Execution of the script is` |
|         - | 4898 | `							    * halted.` |
|         - | 4899 | `								* The only fatal error under PH7 is an out-of-memory. All others erros` |
|         - | 4900 | `								* even a call to undefined function will not halt script execution.` |
|         - | 4901 | `							    */` |
|         - | 4902 | `	E_WARNING           = 2,   /* Run-time warnings (non-fatal errors). Execution of the script is not halted.  */` |
|         - | 4903 | `	E_PARSE             = 4,   /* Compile-time parse errors. Parse errors should only be generated by the parser.*/` |
|         - | 4904 | `	E_NOTICE            = 8,   /* Run-time notices. Indicate that the script encountered something that could` |
|         - | 4905 | `							    * indicate an error, but could also happen in the normal course of running a script.` |
|         - | 4906 | `							    */` |
|         - | 4907 | `	E_CORE_WARNING      = 16,  /* Fatal errors that occur during PHP's initial startup. This is like an E_ERROR` |
|         - | 4908 | `							    * except it is generated by the core of PHP.` |
|         - | 4909 | `							    */` |
|         - | 4910 | `	E_USER_ERROR        = 256,  /* User-generated error message.*/` |
|         - | 4911 | `	E_USER_WARNING      = 512,  /* User-generated warning message.*/` |
|         - | 4912 | `	E_USER_NOTICE       = 1024, /* User-generated notice message.*/` |
|         - | 4913 | `	E_STRICT            = 2048, /* Enable to have PHP suggest changes to your code which will ensure the best interoperability` |
|         - | 4914 | `								 * and forward compatibility of your code.` |
|         - | 4915 | `								 */` |
|         - | 4916 | `	E_RECOVERABLE_ERROR = 4096, /* Catchable fatal error. It indicates that a probably dangerous error occured, but did not` |
|         - | 4917 | `								 * leave the Engine in an unstable state. If the error is not caught by a user defined handle` |
|         - | 4918 | `								 * the application aborts as it was an E_ERROR.` |
|         - | 4919 | `								 */` |
|         - | 4920 | `	E_DEPRECATED        = 8192, /* Run-time notices. Enable this to receive warnings about code that will not` |
|         - | 4921 | `								 * work in future versions.` |
|         - | 4922 | `								 */` |
|         - | 4923 | `	E_USER_DEPRECATED   = 16384, /* User-generated warning message. */` |
|         - | 4924 | `	E_ALL               = 32767  /* All errors and warnings */` |
|         - | 4925 | `};` |
|         - | 4926 | `/*` |
|         - | 4927 | ` * Each VM instruction resulting from compiling a PHP script is represented` |
|         - | 4928 | ` * by one of the following OP codes.` |
|         - | 4929 | ` * The program consists of a linear sequence of operations. Each operation` |
|         - | 4930 | ` * has an opcode and 3 operands.Operands P1 is an integer.` |
|         - | 4931 | ` * Operand P2 is an unsigned integer and operand P3 is a memory address.` |
|         - | 4932 | ` * Few opcodes use all 3 operands.` |
|         - | 4933 | ` */` |
|         - | 4934 | `enum ph7_vm_op {` |
|         - | 4935 | `  PH7_OP_DONE =   1,   /* Done */` |
|         - | 4936 | `  PH7_OP_HALT,         /* Halt */` |
|         - | 4937 | `  PH7_OP_LOAD,         /* Load memory object */` |
|         - | 4938 | `  PH7_OP_LOADC,        /* Load constant */` |
|         - | 4939 | `  PH7_OP_LOAD_IDX,     /* Load array entry */` |
|         - | 4940 | `  PH7_OP_LOAD_MAP,     /* Load hashmap('array') */` |
|         - | 4941 | `  PH7_OP_LOAD_LIST,    /* Load list */` |
|         - | 4942 | `  PH7_OP_LOAD_CLOSURE, /* Load closure */` |
|         - | 4943 | `  PH7_OP_LOAD_FCC,     /* Load first-class callable: wrap a function/method as a Closure */` |
|         - | 4944 | `  PH7_OP_NOOP,         /* NOOP */` |
|         - | 4945 | `  PH7_OP_JMP,          /* Unconditional jump */` |
|         - | 4946 | `  PH7_OP_JZ,           /* Jump on zero (FALSE jump) */` |
|         - | 4947 | `  PH7_OP_JNZ,          /* Jump on non-zero (TRUE jump) */` |
|         - | 4948 | `  PH7_OP_POP,          /* Stack POP */` |
|         - | 4949 | `  PH7_OP_CAT,          /* Concatenation */` |
|         - | 4950 | `  PH7_OP_CVT_INT,      /* Integer cast */` |
|         - | 4951 | `  PH7_OP_CVT_STR,      /* String cast */` |
|         - | 4952 | `  PH7_OP_CVT_REAL,     /* Float cast */` |
|         - | 4953 | `  PH7_OP_CALL,         /* Function call */` |
|         - | 4954 | `  PH7_OP_UMINUS,       /* Unary minus '-'*/` |
|         - | 4955 | `  PH7_OP_UPLUS,        /* Unary plus '+'*/` |
|         - | 4956 | `  PH7_OP_BITNOT,       /* Bitwise not '~' */` |
|         - | 4957 | `  PH7_OP_LNOT,         /* Logical not '!' */` |
|         - | 4958 | `  PH7_OP_MUL,          /* Multiplication '*' */` |
|         - | 4959 | `  PH7_OP_DIV,          /* Division '/' */` |
|         - | 4960 | `  PH7_OP_MOD,          /* Modulus '%' */` |
|         - | 4961 | `  PH7_OP_POW,          /* Exponentiation '**' */` |
|         - | 4962 | `  PH7_OP_ADD,          /* Add '+' */` |
|         - | 4963 | `  PH7_OP_SUB,          /* Sub '-' */` |
|         - | 4964 | `  PH7_OP_SHL,          /* Left shift '<<' */` |
|         - | 4965 | `  PH7_OP_SHR,          /* Right shift '>>' */` |
|         - | 4966 | `  PH7_OP_LT,           /* Less than '<' */` |
|         - | 4967 | `  PH7_OP_LE,           /* Less or equal '<=' */` |
|         - | 4968 | `  PH7_OP_GT,           /* Greater than '>' */` |
|         - | 4969 | `  PH7_OP_GE,           /* Greater or equal '>=' */` |
|         - | 4970 | `  PH7_OP_SPACESHIP,    /* Spaceship '<=>' */` |
|         - | 4971 | `  PH7_OP_EQ,           /* Equal '==' */` |
|         - | 4972 | `  PH7_OP_NEQ,          /* Not equal '!=' */` |
|         - | 4973 | `  PH7_OP_TEQ,          /* Type equal '===' */` |
|         - | 4974 | `  PH7_OP_TNE,          /* Type not equal '!==' */` |
|         - | 4975 | `  PH7_OP_BAND,         /* Bitwise and '&' */` |
|         - | 4976 | `  PH7_OP_BXOR,         /* Bitwise xor '^' */` |
|         - | 4977 | `  PH7_OP_BOR,          /* Bitwise or '\|' */` |
|         - | 4978 | `  PH7_OP_LAND,         /* Logical and '&&','and' */` |
|         - | 4979 | `  PH7_OP_LOR,          /* Logical or  '\|\|','or' */` |
|         - | 4980 | `  PH7_OP_LXOR,         /* Logical xor 'xor' */` |
|         - | 4981 | `  PH7_OP_STORE,        /* Store Object */` |
|         - | 4982 | `  PH7_OP_STORE_IDX,    /* Store indexed object */` |
|         - | 4983 | `  PH7_OP_STORE_IDX_REF,/* Store indexed object by reference */` |
|         - | 4984 | `  PH7_OP_PULL,         /* Stack pull */` |
|         - | 4985 | `  PH7_OP_SWAP,         /* Stack swap */` |
|         - | 4986 | `  PH7_OP_YIELD,        /* Stack yield */` |
|         - | 4987 | `  PH7_OP_YIELD_FROM,   /* Generator delegation (yield from <iterable>) */` |
|         - | 4988 | `  PH7_OP_CVT_BOOL,     /* Boolean cast */` |
|         - | 4989 | `  PH7_OP_CVT_NUMC,     /* Numeric (integer,real or both) type cast */` |
|         - | 4990 | `  PH7_OP_INCR,         /* Increment ++ */` |
|         - | 4991 | `  PH7_OP_DECR,         /* Decrement -- */` |
|         - | 4992 | `  PH7_OP_NEW,          /* new */` |
|         - | 4993 | `  PH7_OP_CLONE,        /* clone */` |
|         - | 4994 | `  PH7_OP_ADD_STORE,    /* Add and store '+=' */` |
|         - | 4995 | `  PH7_OP_SUB_STORE,    /* Sub and store '-=' */` |
|         - | 4996 | `  PH7_OP_MUL_STORE,    /* Mul and store '*=' */` |
|         - | 4997 | `  PH7_OP_DIV_STORE,    /* Div and store '/=' */` |
|         - | 4998 | `  PH7_OP_MOD_STORE,    /* Mod and store '%=' */` |
|         - | 4999 | `  PH7_OP_POW_STORE,    /* Pow and store '**=' */` |
|         - | 5000 | `  PH7_OP_CAT_STORE,    /* Cat and store '.=' */` |
|         - | 5001 | `  PH7_OP_SHL_STORE,    /* Shift left and store '>>=' */` |
|         - | 5002 | `  PH7_OP_SHR_STORE,    /* Shift right and store '<<=' */` |
|         - | 5003 | `  PH7_OP_BAND_STORE,   /* Bitand and store '&=' */` |
|         - | 5004 | `  PH7_OP_BOR_STORE,    /* Bitor and store '\|=' */` |
|         - | 5005 | `  PH7_OP_BXOR_STORE,   /* Bitxor and store '^=' */` |
|         - | 5006 | `  PH7_OP_CONSUME,      /* Consume VM output */` |
|         - | 5007 | `  PH7_OP_LOAD_REF,     /* Load reference */` |
|         - | 5008 | `  PH7_OP_STORE_REF,    /* Store a reference to a variable*/` |
|         - | 5009 | `  PH7_OP_MEMBER,       /* Class member run-time access */` |
|         - | 5010 | `  PH7_OP_UPLINK,       /* Run-Time frame link */` |
|         - | 5011 | `  PH7_OP_CVT_NULL,     /* NULL cast */` |
|         - | 5012 | `  PH7_OP_CVT_ARRAY,    /* Array cast */` |
|         - | 5013 | `  PH7_OP_CVT_OBJ,      /* Object cast */` |
|         - | 5014 | `  PH7_OP_FOREACH_INIT, /* For each init */` |
|         - | 5015 | `  PH7_OP_FOREACH_STEP, /* For each step */` |
|         - | 5016 | `  PH7_OP_IS_A,         /* Instanceof */` |
|         - | 5017 | `  PH7_OP_LOAD_EXCEPTION,/* Load an exception */` |
|         - | 5018 | `  PH7_OP_POP_EXCEPTION, /* POP an exception */` |
|         - | 5019 | `  PH7_OP_THROW,         /* Throw exception */` |
|         - | 5020 | `  PH7_OP_SWITCH,        /* Switch operation */` |
|         - | 5021 | `  PH7_OP_MATCH,         /* Match expression (PHP 8.0) */` |
|         - | 5022 | `  PH7_OP_ERR_CTRL,     /* Error control */` |
|         - | 5023 | `  PH7_OP_DUP,          /* Duplicate top of stack */` |
|         - | 5024 | `  PH7_OP_NULLC,         /* Null coalescing ?? */` |
|         - | 5025 | `  PH7_OP_NULLC_JMP,     /* Null coalescing assign short-circuit jump */` |
|         - | 5026 | `  PH7_OP_NULLC_STORE,   /* Null coalescing assign store */` |
|         - | 5027 | `  PH7_OP_NULLSAFE_JMP,  /* Nullsafe (?->) short-circuit jump */` |
|         - | 5028 | `  PH7_OP_SPREAD,        /* Mark TOS for argument unpacking (...$arr) */` |
|         - | 5029 | `  PH7_OP_FLAG_SPREAD,   /* Flag TOS as a spread source for the next LOAD_MAP */` |
|         - | 5030 | `  PH7_OP_CATCH,         /* Bind the in-flight exception into a catch variable (ROOT C inline catch) */` |
|         - | 5031 | `  PH7_OP_END_FINALLY,   /* Terminate an inline finally: dispatch the pending action (ROOT C) */` |
|         - | 5032 | `  PH7_OP_SET_FINALLY_RET,/* Seed a pending RETURN and enter the innermost enclosing finally (ROOT C) */` |
|         - | 5033 | `  PH7_OP_SET_FINALLY_JMP,/* Seed a pending BREAK/CONTINUE (jump target) and enter a finally (ROOT C) */` |
|         - | 5034 | `  PH7_OP_CATCH_JMP,     /* Jump to iP2, leaving the try/catch structures iP1 describes (see` |
|         - | 5035 | `                         * PH7_CATCH_JMP_P1): LEVELS detached catch/finally mini-programs and` |
|         - | 5036 | `                         * CROSS enclosing trys whose OP_POP_EXCEPTION the jump skips. Two` |
|         - | 5037 | `                         * regimes. LEVELS > 0: iP2 is a pc in the OWNING body's bytecode, which` |
|         - | 5038 | `                         * this mini-program cannot address — park it on that body's frame and` |
|         - | 5039 | `                         * end the mini-program; each try's OP_POP_EXCEPTION landing pad on the` |
|         - | 5040 | `                         * way out decrements, and the last one drains CROSS and takes the jump,` |
|         - | 5041 | ``                         * exactly as it materializes a catch's parked `return`. LEVELS == 0:`` |
|         - | 5042 | ``                         * iP2 is in THIS array (a `goto` out of a try body) — just drain CROSS`` |
|         - | 5043 | `                         * and jump. Emitted for break/continue/goto alike. */` |
|         - | 5044 | `  PH7_OP_UNSET_VAR,     /* unset($name): drop ONE name binding (p3 = name), never the shared slot */` |
|         - | 5045 | `  PH7_OP_CALL_INIT,     /* Screen a call's callee where it is WRITTEN, before its arguments run:` |
|         - | 5046 | `                         * php resolves one at INIT_FCALL / INIT_DYNAMIC_CALL and raises the` |
|         - | 5047 | `                         * direct dispatch's own Error there. Emitted only for a callee the` |
|         - | 5048 | `                         * following OP_CALL would be the first to look at — a member callee` |
|         - | 5049 | `                         * was already screened by its OP_MEMBER. iP2 = 1 when the compiler` |
|         - | 5050 | `                         * namespace-qualified the name, which the global fallback needs. */` |
|         - | 5051 | `  PH7_OP_ROT_CALLEE,    /* Rotate this call's CALLEE — which the codegen pushed BEFORE the` |
|         - | 5052 | `                         * arguments, because php resolves a callee where it is written — up` |
|         - | 5053 | `                         * to the top of the stack, so OP_CALL sees the [args…][callee] layout` |
|         - | 5054 | `                         * its whole dispatch is written against. iP1 = compile-time argument` |
|         - | 5055 | `                         * count, iP2 = PH7_ROT_* flags (SPREAD: re-derive the runtime count` |
|         - | 5056 | `                         * from this call's unpack runs; TWOSLOT: the callee is an OP_MEMBER` |
|         - | 5057 | `                         * method pair [receiver][name], not a single value). */` |
|         - | 5058 | `  PH7_OP_FUNC_DECL,     /* Bind a CONDITIONAL function declaration: p3 = ph7_vm_func. php binds` |
|         - | 5059 | `                         * a function written at a unit's top level when the unit compiles and` |
|         - | 5060 | ``                         * one written anywhere else (inside an `if`, a loop, another function's`` |
|         - | 5061 | `                         * body) when execution REACHES it -- which is what makes` |
|         - | 5062 | ``                          * `if (!function_exists('f')) { function f(){} }` a no-op when `f` `` |
|         - | 5063 | `                         * exists, and every symfony/polyfill-* package harmless beside a real` |
|         - | 5064 | `                         * mbstring. Redeclaring is php's runtime fatal, raised here. */` |
|         - | 5065 | `  PH7_OP_CLASS_DEFER,   /* Deferred class declaration: p3 = VmDeferredClass. Compile-time` |
|         - | 5066 | `                         * resolution of a parent/interface/trait failed (autoloader not yet` |
|         - | 5067 | `                         * REGISTERED — the declaring file's own statements had not run), so the` |
|         - | 5068 | `                         * whole declaration re-compiles here, at its execution point, where` |
|         - | 5069 | `                         * spl_autoload_register has taken effect. php's own model: classes with` |
|         - | 5070 | `                         * unresolved parents are declared in execution order, not hoisted. */` |
|         - | 5071 | `  PH7_OP_SNAPSHOT       /* Give the top P1 stack slots their own copy of the string bytes they` |
|         - | 5072 | `                         * were loaded from (P1 = 0 means the top slot alone). A value copy only` |
|         - | 5073 | `                         * BORROWS the source's bytes (PH7_MemObjLoad), which is right while the` |
|         - | 5074 | `                         * source cannot change and wrong the moment it can: a value already` |
|         - | 5075 | `                         * pushed then reads a LATER write to that source through the alias, with` |
|         - | 5076 | `                         * the length it captured at the push. Emitted where something that can` |
|         - | 5077 | `                         * RUN still sits between a push and the instruction that consumes it --` |
|         - | 5078 | `                         * a by-value argument before a later argument, an array literal's` |
|         - | 5079 | `                         * entries before a later entry, a binary operator's left operand before` |
|         - | 5080 | `                         * its right -- and nowhere else, so ordinary code pays nothing for it. */` |
|         - | 5081 | `  ,PH7_OP_PICK          /* Push a copy of the stack slot P1 below the top (P1 = 0 is DUP).` |
|         - | 5082 | `                         * php evaluates an assignment target's dynamic subscript and property` |
|         - | 5083 | `                         * NAMES before the assigned value and performs the FETCHES after it, so` |
|         - | 5084 | ``                         * `$a[k()] = v()` runs k() first and only then vivifies. A stack machine`` |
|         - | 5085 | `                         * cannot emit that in one pass: the names are pushed first, the value` |
|         - | 5086 | `                         * lands on top of them, and the access chain -- emitted last, so nothing` |
|         - | 5087 | `                         * it creates is visible to the value -- reads each name back from where` |
|         - | 5088 | `                         * it was parked. That read is this. */` |
|         - | 5089 | `};` |
|         - | 5090 | `/*` |
|         - | 5091 | ` * PH7_OP_CATCH_JMP.iP1 payload. Both halves are nesting depths of the source, never` |
|         - | 5092 | ` * large: LEVELS = detached catch/finally boundaries the jump leaves (0 = none, it` |
|         - | 5093 | ` * stays in this bytecode array), CROSS = enclosing try activations whose` |
|         - | 5094 | ` * OP_POP_EXCEPTION the jump skips, and whose finally it must therefore drain itself.` |
|         - | 5095 | ` */` |
|         - | 5096 | `#define PH7_CATCH_JMP_P1(LEVELS,CROSS) \` |
|         - | 5097 | `	((sxi32)((((sxu32)(CROSS)) << 16) \| ((sxu32)(LEVELS) & 0xFFFFu)))` |
|         - | 5098 | `#define PH7_CATCH_JMP_LEVELS(P1) ((sxu16)((sxu32)(P1) & 0xFFFFu))` |
|         - | 5099 | `#define PH7_CATCH_JMP_CROSS(P1)  ((sxu16)(((sxu32)(P1) >> 16) & 0xFFFFu))` |
|         - | 5100 | `/* LOADC.iP1 bit flags */` |
|         - | 5101 | `#define PH7_LOADC_EXPAND   0x01 /* Candidate for constant/function/class expansion */` |
|         - | 5102 | `#define PH7_LOADC_NOKEY    0x04 /* The nil this pushes is an ABSENT array-literal key (auto-index),` |
|         - | 5103 | ``                                 * not an explicit `null =>` one. The two are both MEMOBJ_NULL on the`` |
|         - | 5104 | `                                 * stack, and LOAD_MAP must tell them apart: an absent key auto-indexes` |
|         - | 5105 | `                                 * silently, an explicit null key deprecates and stores under "". */` |
|         - | 5106 | `#define PH7_LOADC_ABSOLUTE 0x02 /* Fully-qualified — skip namespace prefixing */` |
|         - | 5107 | ``#define PH7_LOADC_NOGLOBAL 0x08 /* The p3 candidate came from a `use const` import, which php`` |
|         - | 5108 | `                                 * resolves WITHOUT a global fallback: if that exact name is` |
|         - | 5109 | `                                 * undefined the read is an Error, even when a global constant` |
|         - | 5110 | `                                 * of the imported alias's short name exists. */` |
|         - | 5111 | `/* ROT_CALLEE.iP2 — what the rotation has to know about the region it is turning over. */` |
|         - | 5112 | `#define PH7_ROT_SPREAD  0x1 /* This call unpacks: the runtime argument count is iP1 plus the net` |
|         - | 5113 | `                             * growth of its OWN captured spread runs (VmSpreadOwnExtra). */` |
|         - | 5114 | `#define PH7_ROT_TWOSLOT 0x2 /* The callee is an OP_MEMBER method pair — [receiver][method name] —` |
|         - | 5115 | `                             * which OP_CALL reads as two slots ($this / the late-static-binding` |
|         - | 5116 | `                             * class from the receiver, the name from the top). A __call routing` |
|         - | 5117 | `                             * collapses that pair to ONE marked carrier slot at run time, which` |
|         - | 5118 | `                             * the handler detects rather than guessing. */` |
|         - | 5119 | `/* CALL.iP2 — a bit SET, not the plain hasSpread boolean it started as. */` |
|         - | 5120 | `#define PH7_CALL_SPREAD    0x1 /* This call unpacks (the ROT_SPREAD of the call itself). */` |
|         - | 5121 | `#define PH7_CALL_CONSTRUCT 0x2 /* This call was emitted by a language CONSTRUCT's codegen --` |
|         - | 5122 | ``                                * `isset`, `empty`, `unset`, `eval`, `print`, `include`,`` |
|         - | 5123 | ``                                * `include_once`, `require`, `require_once`. Each is dispatched`` |
|         - | 5124 | `                                * here as a host function of the same name, and php has no such` |
|         - | 5125 | ``                                * function: `function_exists('empty')` is false there and every`` |
|         - | 5126 | `                                * other name door agrees. The implementations stay registered and` |
|         - | 5127 | `                                * stay hidden (ph7_user_func::bConstruct), and this bit is what` |
|         - | 5128 | `                                * says the ENGINE spelled the name -- the exact counterpart of` |
|         - | 5129 | `                                * MEMOBJ_AUX_ENGINEFN for the compiled-function table. A script` |
|         - | 5130 | `                                * cannot produce a call site carrying it: every one of those` |
|         - | 5131 | `                                * names is a lexer KEYWORD, so the only OP_CALL that can ever` |
|         - | 5132 | `                                * name one comes from the construct codegen. */` |
|         - | 5133 | `/* CALL_INIT.iP2 — the same two compile-time questions, asked one instruction earlier.` |
|         - | 5134 | ` * (Its own bit names: the value is not a CALL's iP2 and the two never mix.) */` |
|         - | 5135 | `#define PH7_CALLINIT_NAMESPACED 0x1 /* The callee name was written unqualified inside a namespace,` |
|         - | 5136 | `                                     * so php's global fallback may still resolve it. */` |
|         - | 5137 | `#define PH7_CALLINIT_CONSTRUCT  0x2 /* PH7_CALL_CONSTRUCT's twin: the call this screens is a` |
|         - | 5138 | `                                     * language construct's, whose callee is a host function no` |
|         - | 5139 | `                                     * script can name. The screen asks PH7_VmIsCallable, which` |
|         - | 5140 | `                                     * (rightly) answers NO for those nine names, so it has to` |
|         - | 5141 | `                                     * stand down here exactly as the OP_CALL lookup does. */` |
|         - | 5142 | `/* MEMBER.iP2 — member-access context. 0=read is the default; the unset/isset/empty modes mirror the` |
|         - | 5143 | ` * array LOAD_IDX context modes so unset()/isset()/empty() on a property behave like on an array elem. */` |
|         - | 5144 | `/* Two of PH7_OP_LOAD_IDX's own iP2 context codes (they do NOT line up with the` |
|         - | 5145 | ` * PH7_MEMBER_* set below). Shared because the PROPERTY opcode has to recognize the` |
|         - | 5146 | `` * base of an unset-subscript: `unset($o->p[$k])` reaches into what $p holds, which`` |
|         - | 5147 | ` * is an indirect modification of $p. */` |
|         - | 5148 | `#define VM_IDX_CTX_UNSET 5` |
|         - | 5149 | ``/* An INTERMEDIATE subscript of an unset chain (`unset($a['k']['n'])`): every unset`` |
|         - | 5150 | ` * rule applies to it — COW-separate the parent, never vivify a missing key, unset's` |
|         - | 5151 | ` * own wording for a bad base — except the removal itself, which belongs to the` |
|         - | 5152 | ` * OUTERMOST subscript alone. */` |
|         - | 5153 | `#define VM_IDX_CTX_UNSET_BASE 10` |
|         - | 5154 | ``/* A READ-MODIFY-WRITE subscript (`$a[k] += v`, `$a[k]++`, `$a[k] .= v`) — php's`` |
|         - | 5155 | ` * BP_VAR_RW fetch. It needs a writable slot exactly as the plain write context` |
|         - | 5156 | ` * (1) does, and everything downstream treats it as one; the single thing that` |
|         - | 5157 | ` * separates them is that php READS the element first, so a missing key WARNS` |
|         - | 5158 | ``  * before it is created. Every level of a chain carries it (`$a['x']['y'] += 1` `` |
|         - | 5159 | ` * warns for both), which is why it is a compile-time context and not a peek at` |
|         - | 5160 | ` * the instruction that follows. */` |
|         - | 5161 | `#define VM_IDX_CTX_RMW 11` |
|         - | 5162 | `#define VM_IDX_IS_UNSET(iP2) ((iP2) == VM_IDX_CTX_UNSET \|\| (iP2) == VM_IDX_CTX_UNSET_BASE)` |
|         - | 5163 | `#define PH7_MEMBER_READ   0 /* attribute read */` |
|         - | 5164 | `#define PH7_MEMBER_METHOD 1 /* method-call preparation */` |
|         - | 5165 | `#define PH7_MEMBER_UNSET  2 /* unset($o->p): remove the property */` |
|         - | 5166 | `#define PH7_MEMBER_ISSET  3 /* isset($o->p): silent on a read-miss */` |
|         - | 5167 | `#define PH7_MEMBER_EMPTY  4 /* empty($o->p): silent on a read-miss */` |
|         - | 5168 | `#define PH7_MEMBER_WRITE  5 /* write-lvalue base ($o->arr[..]=, $o->p??=): auto-create a missing prop */` |
|         - | 5169 | `#define PH7_MEMBER_REF_TARGET 6 /* reference-store target ($o->p =& $x, C::$s =& $x): resolve the` |
|         - | 5170 | `                                 * property slot and stash it for the following OP_STORE_REF; skip` |
|         - | 5171 | `                                 * the read/hook/magic machinery (a ref bind neither reads nor coerces) */` |
|         - | 5172 | `#define PH7_MEMBER_DEFPATH 7    /* D1 commit 2: deferred by-ref/by-value property call arg ($o->p). Reads a` |
|         - | 5173 | `                                 * present property (like READ); on a miss/magic, records the lvalue path` |
|         - | 5174 | `                                 * (MEMOBJ_AUX_DEFPATH) that OP_CALL re-walks in vivify or read+warn mode */` |
|         - | 5175 | ``#define PH7_MEMBER_COALESCE 9 /* `$o->p ?? d`: php's THIRD accessor level, between a read and an`` |
|         - | 5176 | `                               * isset(). Silent on a read-miss like isset()/empty(), but the` |
|         - | 5177 | `                               * expression takes the property's VALUE, not a truth: __isset()` |
|         - | 5178 | `                               * GATES the access and __get() (or a get HOOK) ANSWERS it, and` |
|         - | 5179 | `                               * with no __isset declared the accessor answers on its own.` |
|         - | 5180 | ``                               * `??` used to compile as PH7_MEMBER_ISSET, so every accessor`` |
|         - | 5181 | `                               * path handed the coalesce a BOOLEAN. */` |
|         - | 5182 | `#define PH7_MEMBER_LIST_TARGET 8 /* positional list-destructuring store target ([$o->p] = [...]): a pure` |
|         - | 5183 | `                                  * write whose value arrives only at the following OP_LOAD_LIST, which` |
|         - | 5184 | `                                  * writes the slot directly (with typed-slot enforcement). Skip the` |
|         - | 5185 | `                                  * uninitialized-typed read Error and the __get consult, and vivify a` |
|         - | 5186 | `                                  * missing property like a write base */` |
|         - | 5187 | `/* -- END-OF INSTRUCTIONS -- */` |
|         - | 5188 | `/*` |
|         - | 5189 | ` * Expression Operators ID.` |
|         - | 5190 | ` */` |
|         - | 5191 | `enum ph7_expr_id {` |
|         - | 5192 | `	EXPR_OP_NEW = 1,   /* new */` |
|         - | 5193 | `	EXPR_OP_CLONE,     /* clone */` |
|         - | 5194 | `	EXPR_OP_ARROW,     /* -> */` |
|         - | 5195 | `	EXPR_OP_NULLSAFE_ARROW, /* ?-> (PHP 8.0 nullsafe) */` |
|         - | 5196 | `	EXPR_OP_DC,        /* :: */` |
|         - | 5197 | `	EXPR_OP_SUBSCRIPT, /* []: Subscripting */` |
|         - | 5198 | `	EXPR_OP_FUNC_CALL, /* func_call() */` |
|         - | 5199 | `	EXPR_OP_INCR,      /* ++ */` |
|         - | 5200 | `	EXPR_OP_DECR,      /* -- */` |
|         - | 5201 | `	EXPR_OP_BITNOT,    /* ~ */` |
|         - | 5202 | `	EXPR_OP_UMINUS,    /* Unary minus  */` |
|         - | 5203 | `	EXPR_OP_UPLUS,     /* Unary plus */` |
|         - | 5204 | `	EXPR_OP_TYPECAST,  /* Type cast [i.e: (int),(float),(string)...] */` |
|         - | 5205 | `	EXPR_OP_ALT,       /* @ */` |
|         - | 5206 | `	EXPR_OP_INSTOF,    /* instanceof */` |
|         - | 5207 | `	EXPR_OP_LOGNOT,    /* logical not ! */` |
|         - | 5208 | `	EXPR_OP_MUL,       /* Multiplication */` |
|         - | 5209 | `	EXPR_OP_DIV,       /* division */` |
|         - | 5210 | `	EXPR_OP_MOD,       /* Modulus */` |
|         - | 5211 | `	EXPR_OP_POW,       /* Exponentiation ** */` |
|         - | 5212 | `	EXPR_OP_ADD,       /* Addition */` |
|         - | 5213 | `	EXPR_OP_SUB,       /* Substraction */` |
|         - | 5214 | `	EXPR_OP_DOT,       /* Concatenation */` |
|         - | 5215 | `	EXPR_OP_SHL,       /* Left shift */` |
|         - | 5216 | `	EXPR_OP_SHR,       /* Right shift */` |
|         - | 5217 | `	EXPR_OP_LT,        /* Less than */` |
|         - | 5218 | `	EXPR_OP_LE,        /* Less equal */` |
|         - | 5219 | `	EXPR_OP_GT,        /* Greater than */` |
|         - | 5220 | `	EXPR_OP_GE,        /* Greater equal */` |
|         - | 5221 | `	EXPR_OP_SPACESHIP, /* Spaceship <=> */` |
|         - | 5222 | `	EXPR_OP_EQ,        /* Equal == */` |
|         - | 5223 | `	EXPR_OP_NE,        /* Not equal != <> */` |
|         - | 5224 | `	EXPR_OP_TEQ,       /* Type equal === */` |
|         - | 5225 | `	EXPR_OP_TNE,       /* Type not equal !== */` |
|         - | 5226 | `	EXPR_OP_BAND,      /* Biwise and '&' */` |
|         - | 5227 | `	EXPR_OP_REF,       /* Reference operator '&' */` |
|         - | 5228 | `	EXPR_OP_XOR,       /* bitwise xor '^' */` |
|         - | 5229 | `	EXPR_OP_BOR,       /* bitwise or '\|' */` |
|         - | 5230 | `	EXPR_OP_LAND,      /* Logical and '&&','and' */` |
|         - | 5231 | `	EXPR_OP_LOR,       /* Logical or  '\|\|','or'*/` |
|         - | 5232 | `	EXPR_OP_LXOR,      /* Logical xor 'xor' */` |
|         - | 5233 | `	EXPR_OP_QUESTY,    /* Ternary operator '?' */` |
|         - | 5234 | `	EXPR_OP_NULLC,     /* Null coalescing '??' */` |
|         - | 5235 | `	EXPR_OP_ASSIGN,    /* Assignment '=' */` |
|         - | 5236 | `	EXPR_OP_ADD_ASSIGN, /* Combined operator: += */` |
|         - | 5237 | `	EXPR_OP_SUB_ASSIGN, /* Combined operator: -= */` |
|         - | 5238 | `	EXPR_OP_MUL_ASSIGN, /* Combined operator: *= */` |
|         - | 5239 | `	EXPR_OP_DIV_ASSIGN, /* Combined operator: /= */` |
|         - | 5240 | `	EXPR_OP_MOD_ASSIGN, /* Combined operator: %= */` |
|         - | 5241 | `	EXPR_OP_POW_ASSIGN, /* Combined operator: **= */` |
|         - | 5242 | `	EXPR_OP_DOT_ASSIGN, /* Combined operator: .= */` |
|         - | 5243 | `	EXPR_OP_AND_ASSIGN, /* Combined operator: &= */` |
|         - | 5244 | `	EXPR_OP_OR_ASSIGN,  /* Combined operator: \|= */` |
|         - | 5245 | `	EXPR_OP_XOR_ASSIGN, /* Combined operator: ^= */` |
|         - | 5246 | `	EXPR_OP_SHL_ASSIGN, /* Combined operator: <<= */` |
|         - | 5247 | `	EXPR_OP_SHR_ASSIGN, /* Combined operator: >>= */` |
|         - | 5248 | `	EXPR_OP_NULLC_ASSIGN, /* Combined operator: null coalescing assign */` |
|         - | 5249 | `	EXPR_OP_PIPE,       /* PHP 8.5 pipe operator: \|> */` |
|         - | 5250 | `	EXPR_OP_COMMA       /* Comma expression */` |
|         - | 5251 | `};` |
|         - | 5252 | `/*` |
|         - | 5253 | ` * Very high level tokens.` |
|         - | 5254 | ` */` |
|         - | 5255 | `#define PH7_TOKEN_RAW 0x001 /* Raw text [i.e: HTML,XML...] */` |
|         - | 5256 | `#define PH7_TOKEN_PHP 0x002 /* PHP chunk */` |
|         - | 5257 | `/*` |
|         - | 5258 | ` * Lexer token codes` |
|         - | 5259 | ` * The following set of constants are the tokens recognized` |
|         - | 5260 | ` * by the lexer when processing PHP input.` |
|         - | 5261 | ` * Important: Token values MUST BE A POWER OF TWO.` |
|         - | 5262 | ` */` |
|         - | 5263 | `#define PH7_TK_INTEGER   0x0000001  /* Integer */` |
|         - | 5264 | `#define PH7_TK_REAL      0x0000002  /* Real number */` |
|         - | 5265 | `#define PH7_TK_NUM       (PH7_TK_INTEGER\|PH7_TK_REAL) /* Numeric token,either integer or real */` |
|         - | 5266 | `#define PH7_TK_KEYWORD   0x0000004 /* Keyword [i.e: while,for,if,foreach...] */` |
|         - | 5267 | `#define PH7_TK_ID        0x0000008 /* Alphanumeric or UTF-8 stream */` |
|         - | 5268 | `#define PH7_TK_DOLLAR    0x0000010 /* '$' Dollar sign */` |
|         - | 5269 | `#define PH7_TK_OP        0x0000020 /* Operator [i.e: +,*,/...] */` |
|         - | 5270 | `#define PH7_TK_OCB       0x0000040 /* Open curly brace'{' */` |
|         - | 5271 | `#define PH7_TK_CCB       0x0000080 /* Closing curly brace'}' */` |
|         - | 5272 | `#define PH7_TK_NSSEP     0x0000100 /* Namespace separator '\' */` |
|         - | 5273 | `#define PH7_TK_LPAREN    0x0000200 /* Left parenthesis '(' */` |
|         - | 5274 | `#define PH7_TK_RPAREN    0x0000400 /* Right parenthesis ')' */` |
|         - | 5275 | `#define PH7_TK_OSB       0x0000800 /* Open square bracket '[' */` |
|         - | 5276 | `#define PH7_TK_CSB       0x0001000 /* Closing square bracket ']' */` |
|         - | 5277 | `#define PH7_TK_DSTR      0x0002000 /* Double quoted string "$str" */` |
|         - | 5278 | `#define PH7_TK_SSTR      0x0004000 /* Single quoted string 'str' */` |
|         - | 5279 | `#define PH7_TK_HEREDOC   0x0008000 /* Heredoc <<< */` |
|         - | 5280 | `#define PH7_TK_NOWDOC    0x0010000 /* Nowdoc <<< */` |
|         - | 5281 | `#define PH7_TK_COMMA     0x0020000 /* Comma ',' */` |
|         - | 5282 | `#define PH7_TK_SEMI      0x0040000 /* Semi-colon ";" */` |
|         - | 5283 | ``#define PH7_TK_BSTR      0x0080000 /* Backtick quoted string [i.e: Shell command `date`] */`` |
|         - | 5284 | `#define PH7_TK_COLON     0x0100000 /* single Colon ':' */` |
|         - | 5285 | `#define PH7_TK_AMPER     0x0200000 /* Ampersand '&' */` |
|         - | 5286 | `#define PH7_TK_EQUAL     0x0400000 /* Equal '=' */` |
|         - | 5287 | `#define PH7_TK_ARRAY_OP  0x0800000 /* Array operator '=>' */` |
|         - | 5288 | `#define PH7_TK_ELLIPSIS  0x1000000 /* Ellipsis '...' */` |
|         - | 5289 | `#define PH7_TK_OTHER     0x2000000 /* Other symbols */` |
|         - | 5290 | ``#define PH7_TK_VOID_CAST 0x8000000 /* php 8.5's `(void)` cast, assembled by the lexer from the three`` |
|         - | 5291 | `                                      * tokens the way every other cast operator is. It is NOT an` |
|         - | 5292 | `                                      * expression operator: php's grammar takes it only at the head` |
|         - | 5293 | ``                                      * of an expression STATEMENT or of a `for` clause, so anywhere`` |
|         - | 5294 | `                                      * else it stays an unrecognized token and the parser reports` |
|         - | 5295 | ``                                      * php's `unexpected token "(void)"`. */`` |
|         - | 5296 | `#define PH7_TK_UNTERM    0x10000000 /* The lexeme ran into the END OF THE INPUT without its closing` |
|         - | 5297 | `                                     * delimiter: an unterminated quote, heredoc or block comment.` |
|         - | 5298 | `                                     * php refuses each of those; this engine used to consume them` |
|         - | 5299 | `                                     * up to EOF and run the program. */` |
|         - | 5300 | ``#define PH7_TK_FQNAME    0x20000000 /* A PH7_TK_OTHER whose text is a whole `\A\B` name standing where`` |
|         - | 5301 | ``                                    * php's grammar has no place for one (`A \B`): php's scanner made it`` |
|         - | 5302 | `                                    * ONE T_NAME_FULLY_QUALIFIED token and its parse error names it so.` |
|         - | 5303 | `                                    * The lexer takes the whole name into the token for that message. */` |
|         - | 5304 | `#define PH7_TK_ALIAS_CAST 0x40000000 /* A cast written with one of php's four NON-CANONICAL spellings --` |
|         - | 5305 | `                                      * (integer), (boolean), (double), (binary). The lexer hands the` |
|         - | 5306 | `                                      * parser the canonical token text, so the alias is gone by the` |
|         - | 5307 | `                                      * time anything downstream could report it; this bit is what` |
|         - | 5308 | `                                      * remembers that the source said the other word, and the alias` |
|         - | 5309 | `                                      * is recoverable from the canonical name because each of the` |
|         - | 5310 | `                                      * four is the only alias of its target. */` |
|         - | 5311 | `#define PH7_TK_MEMBER_NAME 0x4000000 /* Reserved word used as a member NAME right after -> / ?-> / ::` |
|         - | 5312 | `                                      * (Enum::Null, C::Array, $o->list()): a plain identifier, never` |
|         - | 5313 | `                                      * the literal value — GenStateLoadLiteral skips its value conversion. */` |
|         - | 5314 | `/*` |
|         - | 5315 | ` * PHP keyword.` |
|         - | 5316 | ` * These words have special meaning in PHP. Some of them represent things which look like` |
|         - | 5317 | ` * functions, some look like constants, and so on, but they're not, really: they are language constructs.` |
|         - | 5318 | ` * You cannot use any of the following words as constants, class names, function or method names.` |
|         - | 5319 | ` * Using them as variable names is generally OK, but could lead to confusion.` |
|         - | 5320 | ` */` |
|         - | 5321 | `#define PH7_TKWRD_EXTENDS      1 /* extends */` |
|         - | 5322 | `#define PH7_TKWRD_ENDSWITCH    2 /* endswitch */` |
|         - | 5323 | `#define PH7_TKWRD_SWITCH       3 /* switch */` |
|         - | 5324 | `#define PH7_TKWRD_PRINT        4 /* print */` |
|         - | 5325 | `#define PH7_TKWRD_INTERFACE    5 /* interface */` |
|         - | 5326 | `#define PH7_TKWRD_ENDDEC       6 /* enddeclare */` |
|         - | 5327 | `#define PH7_TKWRD_DECLARE      7 /* declare */` |
|         - | 5328 | `/* The number '8' is reserved for PH7_TK_ID */` |
|         - | 5329 | `#define PH7_TKWRD_REQONCE      9 /* require_once */` |
|         - | 5330 | `#define PH7_TKWRD_REQUIRE      10 /* require */` |
|         - | 5331 | `#define PH7_TKWRD_ELIF         0x4000000 /* elseif: MUST BE A POWER OF TWO */` |
|         - | 5332 | `#define PH7_TKWRD_ELSE         0x8000000 /* else:  MUST BE A POWER OF TWO */` |
|         - | 5333 | `#define PH7_TKWRD_IF           13 /* if */` |
|         - | 5334 | `#define PH7_TKWRD_FINAL        14 /* final */` |
|         - | 5335 | `#define PH7_TKWRD_LIST         15 /* list */` |
|         - | 5336 | `#define PH7_TKWRD_STATIC       16 /* static */` |
|         - | 5337 | `#define PH7_TKWRD_CASE         17 /* case */` |
|         - | 5338 | `#define PH7_TKWRD_SELF         18 /* self */` |
|         - | 5339 | `#define PH7_TKWRD_FUNCTION     19 /* function */` |
|         - | 5340 | `#define PH7_TKWRD_NAMESPACE    20 /* namespace */` |
|         - | 5341 | `#define PH7_TKWRD_ENDIF        0x400000 /* endif: MUST BE A POWER OF TWO */` |
|         - | 5342 | `#define PH7_TKWRD_CLONE        0x80 /* clone: MUST BE A POWER OF TWO  */` |
|         - | 5343 | `#define PH7_TKWRD_NEW          0x100 /* new: MUST BE A POWER OF TWO  */` |
|         - | 5344 | `#define PH7_TKWRD_CONST        22 /* const */` |
|         - | 5345 | `#define PH7_TKWRD_THROW        23 /* throw */` |
|         - | 5346 | `#define PH7_TKWRD_USE          24 /* use */` |
|         - | 5347 | `#define PH7_TKWRD_ENDWHILE     0x800000 /* endwhile: MUST BE A POWER OF TWO */` |
|         - | 5348 | `#define PH7_TKWRD_WHILE        26 /* while */` |
|         - | 5349 | `#define PH7_TKWRD_EVAL         27 /* eval */` |
|         - | 5350 | `#define PH7_TKWRD_VAR          28 /* var */` |
|         - | 5351 | `#define PH7_TKWRD_ARRAY        0x200 /* array: MUST BE A POWER OF TWO */` |
|         - | 5352 | `#define PH7_TKWRD_ABSTRACT     29 /* abstract */` |
|         - | 5353 | `#define PH7_TKWRD_TRY          30 /* try */` |
|         - | 5354 | `#define PH7_TKWRD_AND          0x400 /* and: MUST BE A POWER OF TWO  */` |
|         - | 5355 | `#define PH7_TKWRD_DEFAULT      31 /* default */` |
|         - | 5356 | `#define PH7_TKWRD_CLASS        32 /* class */` |
|         - | 5357 | `#define PH7_TKWRD_AS           33 /* as */` |
|         - | 5358 | `#define PH7_TKWRD_CONTINUE     34 /* continue */` |
|         - | 5359 | `#define PH7_TKWRD_EXIT         35 /* exit */` |
|         - | 5360 | `#define PH7_TKWRD_DIE          36 /* die */` |
|         - | 5361 | `#define PH7_TKWRD_ECHO         37 /* echo */` |
|         - | 5362 | `#define PH7_TKWRD_GLOBAL       38 /* global */` |
|         - | 5363 | `#define PH7_TKWRD_IMPLEMENTS   39 /* implements */` |
|         - | 5364 | `#define PH7_TKWRD_INCONCE      40 /* include_once */` |
|         - | 5365 | `#define PH7_TKWRD_INCLUDE      41 /* include */` |
|         - | 5366 | `#define PH7_TKWRD_EMPTY        42 /* empty */` |
|         - | 5367 | `#define PH7_TKWRD_INSTANCEOF   0x800 /* instanceof: MUST BE A POWER OF TWO  */` |
|         - | 5368 | `#define PH7_TKWRD_ISSET        43 /* isset */` |
|         - | 5369 | `#define PH7_TKWRD_PARENT       44 /* parent */` |
|         - | 5370 | `#define PH7_TKWRD_PRIVATE      45 /* private */` |
|         - | 5371 | `#define PH7_TKWRD_ENDFOR       0x1000000 /* endfor: MUST BE A POWER OF TWO */` |
|         - | 5372 | `#define PH7_TKWRD_END4EACH     0x2000000 /* endforeach: MUST BE A POWER OF TWO */` |
|         - | 5373 | `#define PH7_TKWRD_FOR          48 /* for */` |
|         - | 5374 | `#define PH7_TKWRD_FOREACH      49 /* foreach */` |
|         - | 5375 | `#define PH7_TKWRD_OR           0x1000 /* or: MUST BE A POWER OF TWO  */` |
|         - | 5376 | `#define PH7_TKWRD_PROTECTED    50 /* protected */` |
|         - | 5377 | `#define PH7_TKWRD_DO           51 /* do */` |
|         - | 5378 | `#define PH7_TKWRD_PUBLIC       52 /* public */` |
|         - | 5379 | `#define PH7_TKWRD_CATCH        53 /* catch */` |
|         - | 5380 | `#define PH7_TKWRD_RETURN       54 /* return */` |
|         - | 5381 | `#define PH7_TKWRD_UNSET        0x2000 /* unset: MUST BE A POWER OF TWO  */` |
|         - | 5382 | `#define PH7_TKWRD_XOR          0x4000 /* xor: MUST BE A POWER OF TWO  */` |
|         - | 5383 | `#define PH7_TKWRD_BREAK        55 /* break */` |
|         - | 5384 | `#define PH7_TKWRD_GOTO         56 /* goto */` |
|         - | 5385 | `#define PH7_TKWRD_TRAIT        57 /* trait */` |
|         - | 5386 | `#define PH7_TKWRD_INSTEADOF    58 /* insteadof */` |
|         - | 5387 | `#define PH7_TKWRD_FINALLY      59 /* finally */` |
|         - | 5388 | `#define PH7_TKWRD_YIELD        60 /* yield */` |
|         - | 5389 | `#define PH7_TKWRD_FN           61 /* fn (PHP 7.4 arrow function) */` |
|         - | 5390 | `#define PH7_TKWRD_MATCH        62 /* match (PHP 8.0 match expression) */` |
|         - | 5391 | `#define PH7_TKWRD_BOOL         0x8000  /* bool:  MUST BE A POWER OF TWO */` |
|         - | 5392 | `#define PH7_TKWRD_INT          0x10000  /* int:   MUST BE A POWER OF TWO */` |
|         - | 5393 | `#define PH7_TKWRD_FLOAT        0x20000  /* float:  MUST BE A POWER OF TWO */` |
|         - | 5394 | `#define PH7_TKWRD_STRING       0x40000  /* string: MUST BE A POWER OF TWO */` |
|         - | 5395 | `#define PH7_TKWRD_OBJECT       0x80000 /* object: MUST BE A POWER OF TWO */` |
|         - | 5396 | `/* 0x100000 and 0x200000 are free: they were the PH7-ism 'eq'/'ne' string` |
|         - | 5397 | ` * comparison operators, removed so both stay usable as plain identifiers. */` |
|         - | 5398 | `/*` |
|         - | 5399 | ` * PHP-exact ENT_* flag values for the html-entity family. Single source of` |
|         - | 5400 | ` * truth: constant.c declares the PHP-visible ENT_* constants from these and` |
|         - | 5401 | ` * builtin.c implements the semantics against them. The low two bits are the` |
|         - | 5402 | ` * quote bits (ENT_QUOTES = both, ENT_COMPAT = double only, ENT_NOQUOTES = 0)` |
|         - | 5403 | ` * and bits 16\|32 select the doctype — composites, not independent flags.` |
|         - | 5404 | ` */` |
|         - | 5405 | `#define PH7_ENT_QUOTE_SINGLE 0x01 /* encode/decode ' */` |
|         - | 5406 | `#define PH7_ENT_QUOTE_DOUBLE 0x02 /* encode/decode " (== ENT_COMPAT) */` |
|         - | 5407 | `#define PH7_ENT_QUOTES       (PH7_ENT_QUOTE_DOUBLE\|PH7_ENT_QUOTE_SINGLE)` |
|         - | 5408 | `#define PH7_ENT_IGNORE       0x04 /* drop invalid UTF-8 units */` |
|         - | 5409 | `#define PH7_ENT_SUBSTITUTE   0x08 /* invalid UTF-8 unit -> U+FFFD */` |
|         - | 5410 | `#define PH7_ENT_DOC_MASK     0x30 /* doctype selector */` |
|         - | 5411 | `#define PH7_ENT_DOC_HTML401  0x00` |
|         - | 5412 | `#define PH7_ENT_DOC_XML1     0x10` |
|         - | 5413 | `#define PH7_ENT_DOC_XHTML    0x20` |
|         - | 5414 | `#define PH7_ENT_DOC_HTML5    0x30` |
|         - | 5415 | `#define PH7_ENT_DISALLOWED   0x80 /* substitute doctype-disallowed codepoints */` |
|         - | 5416 | `/* The shared default for all five builtins (php 8.1+): ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401. */` |
|         - | 5417 | `#define PH7_ENT_DEFAULT      (PH7_ENT_QUOTES\|PH7_ENT_SUBSTITUTE\|PH7_ENT_DOC_HTML401)` |
|         - | 5418 | `/* JSON encoding/decoding related definition */` |
|         - | 5419 | `enum json_err_code{` |
|         - | 5420 | `	JSON_ERROR_NONE = 0,  /* No error has occurred. */` |
|         - | 5421 | `	JSON_ERROR_DEPTH,     /* The maximum stack depth has been exceeded.  */` |
|         - | 5422 | `	JSON_ERROR_STATE_MISMATCH, /* Occurs with underflow or with the modes mismatch.  */` |
|         - | 5423 | `	JSON_ERROR_CTRL_CHAR, /* Control character error, possibly incorrectly encoded.  */` |
|         - | 5424 | `	JSON_ERROR_SYNTAX,    /* Syntax error. */` |
|         - | 5425 | `	JSON_ERROR_UTF8,      /* Malformed UTF-8 characters */` |
|         - | 5426 | `	JSON_ERROR_RECURSION, /* A container already being encoded shows up inside itself (php value 6) */` |
|         - | 5427 | `	JSON_ERROR_INF_OR_NAN = 7, /* Inf or NaN given to json_encode (php value) */` |
|         - | 5428 | `	JSON_ERROR_UNSUPPORTED_TYPE = 8, /* A resource given to json_encode (php value) */` |
|         - | 5429 | `	JSON_ERROR_INVALID_PROPERTY_NAME = 9, /* Object-mode decode of a property name with a` |
|         - | 5430 | `	                                       * LEADING NUL byte — php reserves that prefix for` |
|         - | 5431 | `	                                       * mangled private/protected names (php value) */` |
|         - | 5432 | `	JSON_ERROR_UTF16 = 10, /* Unpaired UTF-16 surrogate in a \uXXXX escape (php value) */` |
|         - | 5433 | `	JSON_ERROR_NON_BACKED_ENUM = 11 /* Non-backed enum given to json_encode (php 8.1 value) */` |
|         - | 5434 | `};` |
|         - | 5435 | `/* The following constants can be combined to form options for json_encode(). */` |
|         - | 5436 | `#define	JSON_HEX_TAG           0x01  /* All < and > are converted to \u003C and \u003E. */` |
|         - | 5437 | `#define JSON_HEX_AMP           0x02  /* All &s are converted to \u0026. */` |
|         - | 5438 | `#define JSON_HEX_APOS          0x04  /* All ' are converted to \u0027. */` |
|         - | 5439 | `#define JSON_HEX_QUOT          0x08  /* All " are converted to \u0022. */` |
|         - | 5440 | `#define JSON_FORCE_OBJECT      0x10  /* Outputs an object rather than an array */` |
|         - | 5441 | `#define JSON_NUMERIC_CHECK     0x20  /* Encodes numeric strings as numbers. */` |
|         - | 5442 | `#define JSON_PRETTY_PRINT      0x80  /* Use whitespace in returned data to format it.*/` |
|         - | 5443 | `#define JSON_UNESCAPED_SLASHES 0x40  /* Don't escape '/' */` |
|         - | 5444 | `#define JSON_UNESCAPED_UNICODE 0x100 /* Emit multibyte UTF-8 raw instead of \uXXXX */` |
|         - | 5445 | `#define JSON_PARTIAL_OUTPUT_ON_ERROR 0x200 /* Substitute (0 / null / "") for an unencodable` |
|         - | 5446 | `                                            * piece and record the error instead of failing */` |
|         - | 5447 | `#define JSON_PRESERVE_ZERO_FRACTION  0x400 /* A float with no fractional digits prints ".0"` |
|         - | 5448 | `                                            * (1.0 encodes as "1.0", not "1") */` |
|         - | 5449 | `#define JSON_UNESCAPED_LINE_TERMINATORS 0x800 /* ...U+2028/U+2029 included */` |
|         - | 5450 | `#define JSON_INVALID_UTF8_IGNORE     0x100000 /* Drop ill-formed UTF-8 instead of failing */` |
|         - | 5451 | `#define JSON_INVALID_UTF8_SUBSTITUTE 0x200000 /* ...replace it with U+FFFD */` |
|         - | 5452 | `#define JSON_THROW_ON_ERROR    0x400000 /* Throw JsonException on encode/decode error */` |
|         - | 5453 | `/* DECODE flags: php numbers the decode options in their own space, so each shares a` |
|         - | 5454 | ` * bit with an encode flag exactly as php's JSON_BIGINT_AS_STRING shares 2 with` |
|         - | 5455 | ` * JSON_HEX_AMP (and JSON_OBJECT_AS_ARRAY shares 1 with JSON_HEX_TAG). */` |
|         - | 5456 | `#define JSON_OBJECT_AS_ARRAY   0x01` |
|         - | 5457 | `#define JSON_BIGINT_AS_STRING  0x02` |
|         - | 5458 | `/*` |
|         - | 5459 | ` * extract() $flags — php's ENUM (ext/standard/php_array.h), not a bitmask.` |
|         - | 5460 | ` * PH7 exposed a legacy power-of-two bitmask here (1/2/4/8/16/32/64), which` |
|         - | 5461 | ` * changed the meaning of valid php source: extract($a,1) is EXTR_SKIP in php` |
|         - | 5462 | ` * but was EXTR_OVERWRITE in PHL, and EXTR_PREFIX_ALL printed 8 instead of 3.` |
|         - | 5463 | ` * The values below ARE php's, and vm_builtin_extract() dispatches on` |
|         - | 5464 | ` * (flags & 0xff) exactly like php does.` |
|         - | 5465 | ` */` |
|         - | 5466 | `#define PH7_EXTR_OVERWRITE        0` |
|         - | 5467 | `#define PH7_EXTR_SKIP             1` |
|         - | 5468 | `#define PH7_EXTR_PREFIX_SAME      2` |
|         - | 5469 | `#define PH7_EXTR_PREFIX_ALL       3` |
|         - | 5470 | `#define PH7_EXTR_PREFIX_INVALID   4` |
|         - | 5471 | `#define PH7_EXTR_PREFIX_IF_EXISTS 5` |
|         - | 5472 | `#define PH7_EXTR_IF_EXISTS        6` |
|         - | 5473 | `#define PH7_EXTR_REFS             0x100 /* php's by-reference extraction (rides above the mode) */` |
|         - | 5474 | `/*` |
|         - | 5475 | ` * pathinfo() $flags, glob() $flags and parse_ini_*() $scanner_mode — php's VALUES.` |
|         - | 5476 | ` *` |
|         - | 5477 | ` * Each of these was a PH7 invention (pathinfo counted 1/2/3/4 where php's are POWERS` |
|         - | 5478 | ` * OF TWO, glob used its own 1..64 ladder, and the ini scanner started at 1), which` |
|         - | 5479 | `` * changes the meaning of valid php source: `PATHINFO_DIRNAME\|PATHINFO_BASENAME` is 3`` |
|         - | 5480 | ` * in both engines but PHL read 3 as PATHINFO_EXTENSION, a script passing php's literal` |
|         - | 5481 | ` * 64 to json_encode() got JSON_BIGINT_AS_STRING instead of JSON_UNESCAPED_SLASHES, and` |
|         - | 5482 | ` * INI_SCANNER_RAW (php 1) selected nothing.` |
|         - | 5483 | ` *` |
|         - | 5484 | ` * The GLOB_* values are php 8.5's OWN portable set (main/php_glob.h, new in 8.5), which` |
|         - | 5485 | ` * a default build uses on every platform including MSVC — the bundled branch is taken` |
|         - | 5486 | ` * unless the POSIX build is configured with --enable-system-glob (off by default), and` |
|         - | 5487 | ` * the win32 build has no such option. Do NOT read them off the host <glob.h>: glibc's` |
|         - | 5488 | ` * ladder is different (MARK 2, NOSORT 4, BRACE 1024, ONLYDIR 8192), and so was php's` |
|         - | 5489 | ` * own pre-8.5 win32/glob.h (NOESCAPE 0x2000). PATHINFO_*, INI_SCANNER_* and JSON_* are` |
|         - | 5490 | ` * plain #defines in ext/standard and ext/json, never platform-conditional.` |
|         - | 5491 | ` */` |
|         - | 5492 | `#define PH7_PATHINFO_DIRNAME    1` |
|         - | 5493 | `#define PH7_PATHINFO_BASENAME   2` |
|         - | 5494 | `#define PH7_PATHINFO_EXTENSION  4` |
|         - | 5495 | `#define PH7_PATHINFO_FILENAME   8` |
|         - | 5496 | `#define PH7_PATHINFO_ALL        (PH7_PATHINFO_DIRNAME\|PH7_PATHINFO_BASENAME\|\` |
|         - | 5497 | `                                 PH7_PATHINFO_EXTENSION\|PH7_PATHINFO_FILENAME)` |
|         - | 5498 | `#define PH7_GLOB_ERR            0x0004` |
|         - | 5499 | `#define PH7_GLOB_MARK           0x0008` |
|         - | 5500 | `#define PH7_GLOB_NOCHECK        0x0010` |
|         - | 5501 | `#define PH7_GLOB_NOSORT         0x0020` |
|         - | 5502 | `#define PH7_GLOB_BRACE          0x0080` |
|         - | 5503 | `#define PH7_GLOB_NOESCAPE       0x1000` |
|         - | 5504 | `#define PH7_GLOB_ONLYDIR        0x40000000` |
|         - | 5505 | `#define PH7_INI_SCANNER_NORMAL  0` |
|         - | 5506 | `#define PH7_INI_SCANNER_RAW     1` |
|         - | 5507 | `#define PH7_INI_SCANNER_TYPED   2` |
|         - | 5508 | `/* php's INI_SCANNER_TYPED is 2 — not defined here because PHL does not register the` |
|         - | 5509 | ` * constant (nor honour any scanner mode yet); it is tracked with the missing JSON_*. */` |
|         - | 5510 | `/*` |
|         - | 5511 | ` * Each parsed URI is recorded and stored in an instance of the following structure.` |
|         - | 5512 | ` */` |
|         - | 5513 | `typedef struct SyhttpUri SyhttpUri;` |
|         - | 5514 | `struct SyhttpUri` |
|         - | 5515 | `{` |
|         - | 5516 | `	SyString sHost;     /* Hostname or IP address */` |
|         - | 5517 | `	SyString sPort;     /* Port number */` |
|         - | 5518 | `	SyString sPath;     /* Mandatory resource path passed verbatim (Not decoded) */` |
|         - | 5519 | `	SyString sQuery;    /* Query part */` |
|         - | 5520 | `	SyString sFragment; /* Fragment part */` |
|         - | 5521 | `	SyString sScheme;   /* Scheme */` |
|         - | 5522 | `	SyString sUser;     /* Username */` |
|         - | 5523 | `	SyString sPass;     /* Password */` |
|         - | 5524 | `	SyString sRaw;      /* Raw URI */` |
|         - | 5525 | `};` |
|         - | 5526 | `/*` |
|         - | 5527 | ` * An instance of the following structure is used to record all MIME headers seen` |
|         - | 5528 | ` * during a HTTP interaction.` |
|         - | 5529 | ` */` |
|         - | 5530 | `typedef struct SyhttpHeader SyhttpHeader;` |
|         - | 5531 | `struct SyhttpHeader` |
|         - | 5532 | `{` |
|         - | 5533 | `	SyString sName;    /* Header name [i.e:"Content-Type","Host","User-Agent"]. NOT NUL TERMINATED */` |
|         - | 5534 | `	SyString sValue;   /* Header values [i.e: "text/html"]. NOT NUL TERMINATED */` |
|         - | 5535 | `};` |
|         - | 5536 | `/*` |
|         - | 5537 | ` * Supported HTTP methods.` |
|         - | 5538 | ` */` |
|         - | 5539 | `#define HTTP_METHOD_GET  1 /* GET */` |
|         - | 5540 | `#define HTTP_METHOD_HEAD 2 /* HEAD */` |
|         - | 5541 | `#define HTTP_METHOD_POST 3 /* POST */` |
|         - | 5542 | `#define HTTP_METHOD_PUT  4 /* PUT */` |
|         - | 5543 | `#define HTTP_METHOD_OTHR 5 /* Other HTTP methods [i.e: DELETE,TRACE,OPTIONS...]*/` |
|         - | 5544 | `/*` |
|         - | 5545 | ` * Supported HTTP protocol version.` |
|         - | 5546 | ` */` |
|         - | 5547 | `#define HTTP_PROTO_10 1 /* HTTP/1.0 */` |
|         - | 5548 | `#define HTTP_PROTO_11 2 /* HTTP/1.1 */` |
|         - | 5549 | `/* memobj.c function prototypes */` |
|         - | 5550 | `/*` |
|         - | 5551 | ` * Bound on how deep var_dump()/print_r()/var_export() will walk. php has NO limit:` |
|         - | 5552 | ` * it recurses until the process runs out of memory (a 10,000-level array dumps 300 MB` |
|         - | 5553 | ` * and a 100,000-level one dies on the allocator, not on a depth). PHL walks the same` |
|         - | 5554 | ` * tree on the same C stack, so it needs one — this is a backstop for pathological` |
|         - | 5555 | ` * FINITE nesting, not a cycle guard: a container that is its own descendant is caught` |
|         - | 5556 | ` * by PH7_MemObjDumpIsRecursive() and rendered as php's *RECURSION*. Matches` |
|         - | 5557 | ` * SERIALIZE_MAX_DEPTH, the bound the serializer walk has always used, and the one` |
|         - | 5558 | ` * var_export has carried since it was written. Costs ~250 bytes of C stack per level,` |
|         - | 5559 | ` * so a host that hands the engine less than ~2 MB of stack wants a lower one.` |
|         - | 5560 | ` */` |
|         - | 5561 | `#define PH7_DUMP_MAX_DEPTH 4096` |
|         - | 5562 | `PH7_PRIVATE sxi32 PH7_MemObjDump(SyBlob *pOut,ph7_value *pObj,int ShowType,int nTab,int nDepth,int isRef);` |
|         - | 5563 | `PH7_PRIVATE int PH7_MemObjDumpIsRecursive(ph7_value *pObj);` |
|         - | 5564 | `PH7_PRIVATE const char * PH7_MemObjTypeDump(ph7_value *pVal);` |
|         - | 5565 | `PH7_PRIVATE sxi32 PH7_MemObjAdd(ph7_value *pObj1,ph7_value *pObj2,int bAddStore);` |
|         - | 5566 | `/*` |
|         - | 5567 | ` * Bound on how deep PH7_MemObjCmp() will walk. Like PH7_DUMP_MAX_DEPTH this is a` |
|         - | 5568 | ` * backstop for pathological FINITE nesting, NOT the cycle guard: a container that is` |
|         - | 5569 | ` * its own descendant is caught by HASHMAP_COMPARING / VM_INSTANCE_COMPARING, php's own` |
|         - | 5570 | ` * mechanism. php has no depth limit here either -- it compares a 200-level graph and` |
|         - | 5571 | ` * dies on the C stack, not on a count -- so this only has to sit above anything real.` |
|         - | 5572 | ` * Each array level spends TWO counts (PH7_HashmapCmp, then HashmapNodeCmp) and roughly` |
|         - | 5573 | ` * 400 bytes of C stack, so 4096 needs under 1 MB.` |
|         - | 5574 | ` */` |
|         - | 5575 | `#define PH7_CMP_MAX_DEPTH 4096` |
|         - | 5576 | `PH7_PRIVATE sxi32 PH7_MemObjCmp(ph7_value *pObj1,ph7_value *pObj2,int bStrict,int iNest);` |
|         - | 5577 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromString(ph7_vm *pVm,ph7_value *pObj,const SyString *pVal);` |
|         - | 5578 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromArray(ph7_vm *pVm,ph7_value *pObj,ph7_hashmap *pArray);` |
|         - | 5579 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromReal(ph7_vm *pVm,ph7_value *pObj,ph7_real rVal);` |
|         - | 5580 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromInt(ph7_vm *pVm,ph7_value *pObj,sxi64 iVal);` |
|         - | 5581 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromBool(ph7_vm *pVm,ph7_value *pObj,sxi32 iVal);` |
|         - | 5582 | `PH7_PRIVATE sxi32 PH7_MemObjInit(ph7_vm *pVm,ph7_value *pObj);` |
|         - | 5583 | `PH7_PRIVATE sxi32 PH7_MemObjStringAppend(ph7_value *pObj,const char *zData,sxu32 nLen);` |
|         - | 5584 | `#if 0` |
|         - | 5585 | `/* Not used in the current release of the PH7 engine */` |
|         - | 5586 | `PH7_PRIVATE sxi32 PH7_MemObjStringFormat(ph7_value *pObj,const char *zFormat,va_list ap);` |
|         - | 5587 | `#endif` |
|         - | 5588 | `PH7_PRIVATE sxi32 PH7_MemObjStore(ph7_value *pSrc,ph7_value *pDest);` |
|         - | 5589 | `/*` |
|         - | 5590 | ` * The components php's parse_url() answers, and the split that produces them.` |
|         - | 5591 | ` * Shared with filter_var()'s FILTER_VALIDATE_URL, which php builds on the same` |
|         - | 5592 | ` * parse (a component that is ABSENT is what its b* flag reports; an empty one` |
|         - | 5593 | ` * is present-and-empty).` |
|         - | 5594 | ` */` |
|         - | 5595 | `typedef struct VmUrlParts VmUrlParts;` |
|         - | 5596 | `struct VmUrlParts` |
|         - | 5597 | `{` |
|         - | 5598 | `	SyString sScheme,sUser,sPass,sHost,sPath,sQuery,sFragment;` |
|         - | 5599 | `	int iPort;     /* Resolved port, meaningful only when bPort is set */` |
|         - | 5600 | `	sxu8 bScheme,bUser,bPass,bHost,bPort,bPath,bQuery,bFragment;` |
|         - | 5601 | `};` |
|         - | 5602 | `PH7_PRIVATE int PH7_VmUrlSplit(const char *z,int n,VmUrlParts *pOut);` |
|         - | 5603 | `/*` |
|         - | 5604 | ` * PHL_VALUE_CENSUS -- the value-primitive census (memobj.c has the` |
|         - | 5605 | ` * instrument and build-aux/valuecensus.sh drives it). Off in every shipping build.` |
|         - | 5606 | ` *` |
|         - | 5607 | ` * PHL_VC_DOOR is what makes a row a call SITE: the two hot doors below are inlined` |
|         - | 5608 | ` * in the build that ships, and __builtin_return_address(0) inside an INLINED function` |
|         - | 5609 | ` * names the caller's caller. Under the census they are compiled out of line, so a row` |
|         - | 5610 | `` * is the line that called. `unused` is on it because a translation unit that never`` |
|         - | 5611 | `` * calls the door would otherwise warn -- gcc refuses `inline` and `noinline` together,`` |
|         - | 5612 | ` * so the usual static-inline exemption is not available here.` |
|         - | 5613 | ` *` |
|         - | 5614 | ` * PHL_VCENSUS_CALLER lifts every row one frame -- the hashcensus -c convention, and` |
|         - | 5615 | ` * the same warning applies: it needs -fno-omit-frame-pointer, and it is how a row that` |
|         - | 5616 | ` * is a whole subsystem funnelled through one line gets decomposed.` |
|         - | 5617 | ` */` |
|         - | 5618 | `#if defined(PHL_VALUE_CENSUS)` |
|         - | 5619 | `#define PHL_VC_RELEASE 0` |
|         - | 5620 | `#define PHL_VC_LOAD    1` |
|         - | 5621 | `#define PHL_VC_STORE   2` |
|         - | 5622 | `#define PHL_VC_INIT    3` |
|         - | 5623 | `#define PHL_VC_KINDS   4` |
|         - | 5624 | `PH7_PRIVATE void PH7_ValueCensusNote(void *pSite,sxu32 iKind,int bWork);` |
|         - | 5625 | `#if defined(PHL_VCENSUS_CALLER)` |
|         - | 5626 | `#define PHL_VCENSUS_SITE() __builtin_return_address(1)` |
|         - | 5627 | `#else` |
|         - | 5628 | `#define PHL_VCENSUS_SITE() __builtin_return_address(0)` |
|         - | 5629 | `#endif` |
|         - | 5630 | `#define PHL_VC_NOTE(K,W) PH7_ValueCensusNote(PHL_VCENSUS_SITE(),(K),(W))` |
|         - | 5631 | `#define PHL_VC_DOOR static __attribute__((noinline,unused))` |
|         - | 5632 | `#else` |
|         - | 5633 | `#define PHL_VC_NOTE(K,W) ((void)0)` |
|         - | 5634 | `#define PHL_VC_DOOR SX_STATIC_INLINE` |
|         - | 5635 | `#endif` |
|         - | 5636 | `/* Take/drop the reference a ph7_value holds on a stream handle. Both accept ANY` |
|         - | 5637 | ` * resource pointer and do nothing unless it is a live io_private -- see nValRef. */` |
|         - | 5638 | `PH7_PRIVATE int PH7_StreamValueRef(void *pResource);` |
|         - | 5639 | `PH7_PRIVATE void PH7_StreamValueUnref(void *pResource);` |
|         - | 5640 | `/*` |
|         - | 5641 | ` * Load an ALIASING copy of a value: the destination gets the scalar half verbatim, one` |
|         - | 5642 | ` * more reference on a container, and a READ-ONLY view of the source's string bytes. It is` |
|         - | 5643 | ` * how a variable, an element and a property all reach the operand stack, and it is the` |
|         - | 5644 | ` * engine's second-most-called function -- 1.34 BILLION times on the ecosystem gate's phpcs` |
|         - | 5645 | ` * step (counted), from only 62 call sites.` |
|         - | 5646 | ` *` |
|         - | 5647 | ` * Inline for the same reason SySetAt, PH7_MemObjAt and PH7_MemObjRelease are: the body is` |
|         - | 5648 | ` * a dozen instructions and it lived in memobj.c while every hot caller lived elsewhere, so` |
|         - | 5649 | ` * with no LTO every one of those 1.34 billion was a real call. Sixty-two sites is a cheap` |
|         - | 5650 | ` * place to spend that.` |
|         - | 5651 | ` *` |
|         - | 5652 | ` * The destination's blob is released first because a Load OVERWRITES it. On the workload of` |
|         - | 5653 | ` * record that branch was taken **0 times in 1.34 billion** -- the destination is nearly` |
|         - | 5654 | ` * always a fresh operand slot -- so it stays a call rather than more inline bytes.` |
|         - | 5655 | ` */` |
|  35395773 | 5656 | `PHL_VC_DOOR sxi32 PH7_MemObjLoad(ph7_value *pSrc,ph7_value *pDest)` |
|         5 | 5657 | `{` |
|         - | 5658 | `	PHL_VC_NOTE(PHL_VC_LOAD,(pSrc->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) != 0);` |
|  35395778 | 5659 | `	PH7_MEMOBJ_COPY_SCALAR(pDest,pSrc);` |
|         - | 5660 | `	/* D1 commit 2: a MEMOBJ_AUX_DEFPATH carrier OWNS its heap descriptor via x.pOther, and` |
|         - | 5661 | `	 * PH7_MemObjRelease frees it exactly once. An aliasing Load copies iFlags+x.pOther` |
|         - | 5662 | `	 * verbatim, so a Load-duplicated carrier would let two slots free the same descriptor.` |
|         - | 5663 | `	 * Carriers are transient (produced by LOAD_IDX/MEMBER, consumed at OP_CALL) and are never` |
|         - | 5664 | `	 * Load-copied today; strip the flag defensively so the invariant can't be violated. */` |
|  35395778 | 5665 | `	pDest->iFlags &= ~MEMOBJ_AUX_DEFPATH;` |
|  35395778 | 5666 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|         - | 5667 | `		/* Increment reference count */` |
|   1214602 | 5668 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  34788149 | 5669 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|         - | 5670 | `		/* Increment reference count */` |
|    574736 | 5671 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|  33893593 | 5672 | `	}else if( pSrc->iFlags & MEMOBJ_STREAMRES ){` |
|         - | 5673 | `		/* One more holder of the stream handle: php closes a stream when its` |
|         - | 5674 | `		 * last value goes, and this door is how a variable reaches the stack. */` |
|    174797 | 5675 | `		PH7_StreamValueRef(pSrc->x.pOther);` |
|     86709 | 5676 | `	}` |
|  35395778 | 5677 | `	if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|       237 | 5678 | `		SyBlobRelease(&pDest->sBlob);` |
|       116 | 5679 | `	}` |
|  35395778 | 5680 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  19612235 | 5681 | `		SyBlobReadOnly(&pDest->sBlob,SyBlobData(&pSrc->sBlob),SyBlobLength(&pSrc->sBlob));` |
|   9811223 | 5682 | `	}` |
|  35395778 | 5683 | `	return SXRET_OK;` |
|         5 | 5684 | `}` |
|         - | 5685 | `PH7_PRIVATE ph7_value * PH7_ValuePeek(ph7_value *pVal,ph7_value *pScratch);` |
|         - | 5686 | `PH7_PRIVATE sxi64 PH7_ValuePeekInt64(ph7_value *pVal);` |
|         - | 5687 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         - | 5688 | `PH7_PRIVATE ph7_real PH7_ValuePeekReal(ph7_value *pVal);` |
|         - | 5689 | `#endif` |
|         - | 5690 | `PH7_PRIVATE int PH7_ValuePeekBool(ph7_value *pVal);` |
|         - | 5691 | `PH7_PRIVATE sxi32 PH7_MemObjReleaseSlow(ph7_value *pObj);` |
|         - | 5692 | `/*` |
|         - | 5693 | ` * Drop whatever a value owns. THE most-called function in the engine: 2.72 billion` |
|         - | 5694 | ` * times on the ecosystem gate's phpcs step, out of ~4.5 billion calls into the four` |
|         - | 5695 | ` * value primitives together (counted).` |
|         - | 5696 | ` *` |
|         - | 5697 | ` * 43.1% of those calls -- 1.17 billion of them -- had NOTHING TO DO, and this test is` |
|         - | 5698 | ` * why they no longer make the call. A value already typed MEMOBJ_NULL owns no hashmap,` |
|         - | 5699 | `` * no instance, and no string (the slow path's own `(iFlags & MEMOBJ_NULL) == 0` guard is`` |
|         - | 5700 | ` * what skips SyBlobRelease, so a NULL value's blob is not released today either). What` |
|         - | 5701 | ` * IS still owned by a NULL-typed value is one of the three AUX carriers, each of which` |
|         - | 5702 | ` * holds a heap descriptor this is the universal free site for -- so they are the mask,` |
|         - | 5703 | `` * and they must stay in it: a `??=` peek, a __call carrier and a deferred-path lvalue`` |
|         - | 5704 | ` * are all NULL-typed by construction.` |
|         - | 5705 | ` *` |
|         - | 5706 | ` * Inline because the body it guards is three flag tests and a return for nearly half of` |
|         - | 5707 | ` * those 2.72 billion calls, and the call and return around them cost more than they do.` |
|         - | 5708 | ` * The same reason SySetAt and PH7_MemObjAt are inline.` |
|         - | 5709 | ` */` |
|         - | 5710 | `#define MEMOBJ_AUX_OWNED (MEMOBJ_AUX_COALSTROFF\|MEMOBJ_AUX_MAGICCALL\|MEMOBJ_AUX_DEFPATH)` |
|         - | 5711 | `/*` |
|         - | 5712 | ` * The two markers that describe a value's place in ONE array literal under construction --` |
|         - | 5713 | ` * "this key is ABSENT, auto-index it" and "this value is a SPREAD source" -- and that mean` |
|         - | 5714 | ` * nothing once the LOAD_MAP they were written for has read them.` |
|         - | 5715 | ` *` |
|         - | 5716 | ` * They are the only flags whose lifetime is shorter than their slot's, and MemObjSetType` |
|         - | 5717 | ` * keeps every AUX bit, so without this they SURVIVED the pop: a constant pushed onto a slot` |
|         - | 5718 | `` * that had last held an absent-key nil came out marked absent, and `[E_USER_ERROR => 'x',`` |
|         - | 5719 | `` * E_USER_WARNING => 'y']` built `[256 => 'x', 257 => 'y']` -- a wrong array, silently.`` |
|         - | 5720 | ` * Cleared at the release every pop routes through, which is why they belong in the fast` |
|         - | 5721 | ` * path's mask: a marked slot must not take its "owns nothing" return.` |
|         - | 5722 | ` */` |
|         - | 5723 | `#define MEMOBJ_AUX_STACKMARK (MEMOBJ_AUX_NOKEY\|MEMOBJ_AUX_SPREAD)` |
| 123211511 | 5724 | `PHL_VC_DOOR sxi32 PH7_MemObjRelease(ph7_value *pObj)` |
|         5 | 5725 | `{` |
|         - | 5726 | `	PHL_VC_NOTE(PHL_VC_RELEASE,` |
|         - | 5727 | `		(pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_AUX_OWNED\|MEMOBJ_AUX_STACKMARK)) != MEMOBJ_NULL);` |
| 123211516 | 5728 | `	if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_AUX_OWNED\|MEMOBJ_AUX_STACKMARK)) == MEMOBJ_NULL ){` |
|  25984875 | 5729 | `		return SXRET_OK;   /* Owns nothing -- 43.1% of every release the engine makes */` |
|         - | 5730 | `	}` |
|  97226646 | 5731 | `	return PH7_MemObjReleaseSlow(pObj);` |
|  61610956 | 5732 | `}` |
|         - | 5733 | `PH7_PRIVATE sxi32 PH7_MemObjToNumeric(ph7_value *pObj);` |
|         - | 5734 | `PH7_PRIVATE sxi32 PH7_MemObjStringIncrement(ph7_value *pObj);` |
|         - | 5735 | `PH7_PRIVATE sxi32 PH7_MemObjTryInteger(ph7_value *pObj);` |
|         - | 5736 | `PH7_PRIVATE ProcMemObjCast PH7_MemObjCastMethod(sxi32 iFlags);` |
|         - | 5737 | `PH7_PRIVATE int PH7_MemObjStringIsNumeric(ph7_value *pValue);` |
|         - | 5738 | `PH7_PRIVATE int PH7_MemObjStringNumericPrefix(ph7_value *pValue,const char **pzTail);` |
|         - | 5739 | `PH7_PRIVATE sxi32 PH7_MemObjIsNumeric(ph7_value *pObj);` |
|         - | 5740 | `PH7_PRIVATE sxi32 PH7_MemObjIsEmpty(ph7_value *pObj);` |
|         - | 5741 | `PH7_PRIVATE sxi32 PH7_MemObjToHashmap(ph7_value *pObj);` |
|         - | 5742 | `PH7_PRIVATE sxi32 PH7_MemObjToObject(ph7_value *pObj);` |
|         - | 5743 | `PH7_PRIVATE sxi32 PH7_MemObjToString(ph7_value *pObj);` |
|         - | 5744 | `PH7_PRIVATE sxi32 PH7_MemObjToStringUV(ph7_value *pObj);` |
|         - | 5745 | `PH7_PRIVATE int PH7_MemObjIsNotStringable(ph7_value *pObj);` |
|         - | 5746 | `PH7_PRIVATE sxi32 PH7_MemObjToNull(ph7_value *pObj);` |
|         - | 5747 | `PH7_PRIVATE sxi32 PH7_MemObjToReal(ph7_value *pObj);` |
|         - | 5748 | `PH7_PRIVATE sxi32 PH7_MemObjToInteger(ph7_value *pObj);` |
|         - | 5749 | `PH7_PRIVATE sxi32 PH7_MemObjToBool(ph7_value *pObj);` |
|         - | 5750 | `PH7_PRIVATE int PH7_RealFitsInt64(double r);` |
|         - | 5751 | `PH7_PRIVATE sxi64 PH7_RealToInt64(double r);` |
|         - | 5752 | `PH7_PRIVATE void PH7_RealWarnIntCast(ph7_vm *pVm,double r);` |
|         - | 5753 | `PH7_PRIVATE void PH7_MemObjWarnIntCast(ph7_value *pObj);` |
|         - | 5754 | `PH7_PRIVATE sxi64 PH7_TokenValueToInt64(SyString *pData);` |
|         - | 5755 | `/* lex.c function prototypes */` |
|         - | 5756 | `PH7_PRIVATE sxi32 PH7_TokenizeRawText(const char *zInput,sxu32 nLen,SySet *pOut,sxu32 nBaseLine);` |
|         - | 5757 | `PH7_PRIVATE sxi32 PH7_TokenizePHP(const char *zInput,sxu32 nLen,sxu32 nLineStart,SySet *pOut,SySet *pTrivia);` |
|         - | 5758 | `/* vm.c function prototypes */` |
|         - | 5759 | `PH7_PRIVATE void PH7_VmReleaseContextValue(ph7_context *pCtx,ph7_value *pValue);` |
|         - | 5760 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(ph7_vm *pVm,ph7_vm_func *pFunc,const char *zName,sxu32 nByte,` |
|         - | 5761 | `	sxi32 iFlags,void *pUserData);` |
|         - | 5762 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(ph7_vm *pVm,ph7_vm_func *pFunc,SyString *pName);` |
|         - | 5763 | `PH7_PRIVATE SyHashEntry * PH7_VmGetUserFunction(ph7_vm *pVm,const void *pName,sxu32 nByte,int bEngineName);` |
|         - | 5764 | `PH7_PRIVATE SyHashEntry * PH7_VmGetHostFunction(ph7_vm *pVm,const void *pName,sxu32 nByte,int bEngineName);` |
|         - | 5765 | `PH7_PRIVATE void PH7_VmMarkLanguageConstructs(ph7_vm *pVm);` |
|         - | 5766 | `PH7_PRIVATE int PH7_VmNameIsInternalFunc(ph7_vm *pVm,const char *zName,sxu32 nByte);` |
|         - | 5767 | `PH7_PRIVATE SyHashEntry * PH7_VmCallSiteAnswer(ph7_vm *pVm,VmInstr *pInstr,const SyString *pName,int bEngineName,int *pbHost);` |
|         - | 5768 | `PH7_PRIVATE void PH7_VmCallSiteRecord(ph7_vm *pVm,VmInstr *pInstr,const SyString *pName,int bEngineName,int bHost,SyHashEntry *pEntry);` |
|         - | 5769 | `PH7_PRIVATE void PH7_VmCallSiteReleaseChunk(ph7_vm *pVm,SySet *pByteCode);` |
|         - | 5770 | `PH7_PRIVATE SyHashEntry * PH7_VmConstSiteAnswer(ph7_vm *pVm,VmInstr *pInstr);` |
|         - | 5771 | `PH7_PRIVATE void PH7_VmConstSiteRecord(ph7_vm *pVm,VmInstr *pInstr,SyHashEntry *pEntry);` |
|         - | 5772 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(ph7_vm *pVm,ph7_class_instance *pObj);` |
|         - | 5773 | `PH7_PRIVATE ph7_value * PH7_VmCreateDynamicAttr(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nName,VmClassAttr **ppAttr);` |
|         - | 5774 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(ph7_vm *pVm,sxu32 nIdx,SyHashEntry *pEntry,ph7_hashmap_node *pMapEntry);` |
|         - | 5775 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(ph7_vm *pVm,sxu32 nIdx,SyHashEntry *pEntry,ph7_hashmap_node *pMapEntry,sxi32 iFlags);` |
|         - | 5776 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew);` |
|         - | 5777 | `PH7_PRIVATE int PH7_VmIncludePathSep(void);` |
|         - | 5778 | `PH7_PRIVATE void PH7_VmSetIncludePath(ph7_vm *pVm,const char *zPath,sxu32 nByte);` |
|         - | 5779 | `PH7_PRIVATE void PH7_VmApplyEngineIni(ph7_vm *pVm);` |
|         - | 5780 | `PH7_PRIVATE void PH7_VmGetIncludePath(ph7_vm *pVm,SyBlob *pOut);` |
|         - | 5781 | `PH7_PRIVATE int PH7_VmExecutingDir(ph7_vm *pVm,SyString *pOut);` |
|         - | 5782 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable,sxi32 iNest);` |
|         - | 5783 | `PH7_PRIVATE void PH7_VmClassNameAnchor(const char **pzName,sxu32 *pnByte);` |
|         - | 5784 | `PH7_PRIVATE sxi32 PH7_VmMaterializeClassConst(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr);` |
|         - | 5785 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable);` |
|         - | 5786 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(ph7_vm *pVm,const SyString *pName,ProcConstant xExpand,void *pUserData);` |
|         - | 5787 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstantEx(ph7_vm *pVm,const SyString *pName,ProcConstant xExpand,` |
|         - | 5788 | `	void *pUserData,const SyString *pFile,sxu32 nLine,int bUser);` |
|         - | 5789 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(ph7_vm *pVm,const SyString *pName,ProchHostFunction xFunc,void *pUserData);` |
|         - | 5790 | `/* Builds a ph7_user_func WITHOUT registering it as a global name. The native-class` |
|         - | 5791 | ` * builder uses it for method bodies, which are reachable only through their class. */` |
|         - | 5792 | `PH7_PRIVATE sxi32 PH7_NewForeignFunction(ph7_vm *pVm,const SyString *pName,ProchHostFunction xFunc,` |
|         - | 5793 | `	void *pUserData,ph7_user_func **ppOut);` |
|         - | 5794 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(ph7_vm *pVm,ph7_class *pClass);` |
|         - | 5795 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(const void *pSrc,unsigned int nLen,void *pUserData);` |
|         - | 5796 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm);` |
|         - | 5797 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex);` |
|         - | 5798 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(ph7_vm *pVm,SyString *pString);` |
|         - | 5799 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(ph7_vm *pVm,const char *zFormat,va_list ap);` |
|         - | 5800 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap);` |
|         - | 5801 | `PH7_PRIVATE sxi32 PH7_VmThrowError(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zMessage);` |
|         - | 5802 | `PH7_PRIVATE sxi32 PH7_VmMemoryError(ph7_vm *pVm);` |
|         - | 5803 | `PH7_PRIVATE sxi32 PH7_ContextMemoryError(ph7_context *pCtx);` |
|         - | 5804 | `PH7_PRIVATE sxu32 PH7_VmResourceId(ph7_vm *pVm,void *pRes);` |
|         - | 5805 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...);` |
|         - | 5806 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionCode(ph7_context *pCtx,const char *zClass,sxi32 iCode,const char *zFormat,...);` |
|         - | 5807 | `PH7_PRIVATE sxi32 VmHostFuncThrowRc(ph7_context *pCtx,sxi32 rc);` |
|         - | 5808 | `PH7_PRIVATE sxi32 PH7_VmThrowArrayNextIndexError(ph7_vm *pVm);` |
|         - | 5809 | `PH7_PRIVATE sxi32 PH7_VmThrowGlobalsAppendError(ph7_vm *pVm);` |
|         - | 5810 | `PH7_PRIVATE sxi32 PH7_VmInstallGlobalVar(ph7_vm *pVm,const char *zName,sxu32 nByte,ph7_value *pValue,sxu32 nRefIdx);` |
|         - | 5811 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...);` |
|         - | 5812 | `PH7_PRIVATE void  PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData);` |
|         - | 5813 | `PH7_PRIVATE sxi32 PH7_VmDump(ph7_vm *pVm,ProcConsumer xConsumer,void *pUserData);` |
|         - | 5814 | `PH7_PRIVATE sxi32 PH7_VmInstallDateTime(ph7_vm *pVm);` |
|         - | 5815 | `PH7_PRIVATE int PH7_VmErrorLogToFile(ph7_vm *pVm,const char *zMsg,sxu32 nMsg);` |
|         - | 5816 | ``/* `display_errors` destinations -- php's PHP_DISPLAY_ERRORS_* numbering, which is`` |
|         - | 5817 | `` * observable: the directive accepts the numbers as well as the words, so `2` is`` |
|         - | 5818 | `` * the error stream and `1` the program output. */`` |
|         - | 5819 | `#define PH7_DISPLAY_ERRORS_OFF    0` |
|         - | 5820 | `#define PH7_DISPLAY_ERRORS_STDOUT 1` |
|         - | 5821 | `#define PH7_DISPLAY_ERRORS_STDERR 2` |
|         - | 5822 | `PH7_PRIVATE int PH7_VmDisplayErrorsMode(const char *zVal,sxu32 nVal);` |
|         - | 5823 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 5824 | `/* Shared between builtin_date.c (procedural date functions) and` |
|         - | 5825 | ` * builtin_date_parse.c (the DateTime family) */` |
|         - | 5826 | `PH7_PRIVATE sxi32 DateFormat(ph7_context *pCtx,const char *zIn,int nLen,Sytm *pTm,int uSec);` |
|         - | 5827 | `PH7_PRIVATE void PH7_VmLogTimestamp(ph7_vm *pVm,SyBlob *pOut);` |
|         - | 5828 | `PH7_PRIVATE sxi64 DtDaysFromCivil(sxi64 y,int m,int d);` |
|         - | 5829 | `PH7_PRIVATE void DtCivilFromDays(sxi64 z,sxi64 *py,int *pm,int *pd);` |
|         - | 5830 | `PH7_PRIVATE sxi64 DtFloorDiv(sxi64 a,sxi64 b);` |
|         - | 5831 | `PH7_PRIVATE void DtFillSytm(sxi64 iTs,sxi32 iOff,char *zZone,Sytm *pTm);` |
|         - | 5832 | `PH7_PRIVATE void DtNowUs(ph7_vm *pVm,sxi64 *piSec,int *puSec);` |
|         - | 5833 | `/* The script default zone as the tz database sees it: an index or -1, and the` |
|         - | 5834 | ` * offset/abbreviation/is-DST that index is on at an instant. Shared with` |
|         - | 5835 | ` * builtin_date.c so the procedural doors ask the same question the DateTime` |
|         - | 5836 | ` * family does. Both answer "no database zone" harmlessly with the flag off. */` |
|         - | 5837 | `PH7_PRIVATE int DtDefaultTzIndex(ph7_vm *pVm);` |
|         - | 5838 | `PH7_PRIVATE sxi32 DtTzOffsetOf(int iTz,sxi32 iFixed,sxi64 iTs,int *pbDst,` |
|         - | 5839 | `	const char **pzAbbr,int *pnAbbr);` |
|         - | 5840 | `#ifdef PH7_ENABLE_TZDB` |
|         - | 5841 | `/* The embedded IANA database (builtin_date_tzdb.c). Absent from the tiny build` |
|         - | 5842 | ` * on purpose -- the payload is ~296 KB -- so every caller asks PH7_TzFind()` |
|         - | 5843 | ` * first and keeps its fixed-offset path for the answer -1. */` |
|         - | 5844 | `PH7_PRIVATE int PH7_TzFind(const char *zName,int nName);` |
|         - | 5845 | `PH7_PRIVATE int PH7_TzAbbrFind(const char *zName,int nName,sxi32 *piOff,int *pbDst,` |
|         - | 5846 | `	const char **pzCanon,int *pnCanon);` |
|         - | 5847 | `PH7_PRIVATE int PH7_TzAbbrCount(void);` |
|         - | 5848 | `PH7_PRIVATE const char * PH7_TzAbbrAt(int i,int *pnName,int *pnRow);` |
|         - | 5849 | `PH7_PRIVATE int PH7_TzAbbrRowAt(int i,int j,sxi32 *piOff,int *pbDst,int *piZone);` |
|         - | 5850 | `PH7_PRIVATE const char * PH7_TzAbbrZoneFind(const char *zName,int nName,sxi64 iOff,` |
|         - | 5851 | `	sxi64 iDst,int *pnZone);` |
|         - | 5852 | `PH7_PRIVATE int PH7_TzCount(void);` |
|         - | 5853 | `PH7_PRIVATE int PH7_TzAt(int i);` |
|         - | 5854 | `PH7_PRIVATE const char * PH7_TzName(int iZone,int *pnName,int *pbBackward);` |
|         - | 5855 | `PH7_PRIVATE int PH7_TzGroup(int iZone);` |
|         - | 5856 | `PH7_PRIVATE const char * PH7_TzCountry(int iZone);` |
|         - | 5857 | `PH7_PRIVATE void PH7_TzLocation(int iZone,double *prLat,double *prLong,` |
|         - | 5858 | `	const char **pzComment,int *pnComment);` |
|         - | 5859 | `PH7_PRIVATE const char * PH7_TzVersion(void);` |
|         - | 5860 | `PH7_PRIVATE int PH7_TzOffsetAt(int iZone,sxi64 iTs,sxi32 *piOff,int *pbDst,` |
|         - | 5861 | `	const char **pzAbbr,int *pnAbbr);` |
|         - | 5862 | `PH7_PRIVATE int PH7_TzTransCount(int iZone);` |
|         - | 5863 | `PH7_PRIVATE int PH7_TzTransAt(int iZone,int i,sxi64 *piTs,sxi32 *piOff,int *pbDst,` |
|         - | 5864 | `	const char **pzAbbr,int *pnAbbr);` |
|         - | 5865 | `PH7_PRIVATE int PH7_TzTransNextPosix(int iZone,sxi64 iTs,sxi64 *piTs,sxi32 *piOff,` |
|         - | 5866 | `	int *pbDst,const char **pzAbbr,int *pnAbbr);` |
|         - | 5867 | `PH7_PRIVATE int PH7_TzLocalToUtc(int iZone,sxi64 iLocal,sxi64 *piTs,sxi32 *piOff);` |
|         - | 5868 | `PH7_PRIVATE int PH7_TzLocalToUtcFirst(int iZone,sxi64 iLocal,sxi64 *piTs,sxi32 *piOff);` |
|         - | 5869 | `PH7_PRIVATE int PH7_TzLocalToUtcSeed(int iZone,sxi64 iLocal,sxi32 iOffNow,int bDstNow,` |
|         - | 5870 | `	sxi64 *piTs,sxi32 *piOff);` |
|         - | 5871 | `#endif /* PH7_ENABLE_TZDB */` |
|         - | 5872 | `#ifdef PH7_ENABLE_JIS` |
|         - | 5873 | `/* The Japanese legacy character sets (builtin_jis.c). Absent from the tiny` |
|         - | 5874 | ` * build on purpose -- the table is ~31 KB -- so a framing that needs it is not` |
|         - | 5875 | ` * a known encoding there at all, rather than a known one that answers wrong. */` |
|         - | 5876 | `PH7_PRIVATE sxu32 PH7_JisX0208ToUni(int iRow,int iCell);` |
|         - | 5877 | `PH7_PRIVATE int PH7_JisX0208FromUni(sxu32 cp,int *piRow,int *piCell);` |
|         - | 5878 | `PH7_PRIVATE sxu32 PH7_JisX0201RomanToUni(int c);` |
|         - | 5879 | `PH7_PRIVATE int PH7_JisX0201RomanFromUni(sxu32 cp,int *piByte);` |
|         - | 5880 | `PH7_PRIVATE sxu32 PH7_JisX0201KanaToUni(int c);` |
|         - | 5881 | `PH7_PRIVATE int PH7_JisX0201KanaFromUni(sxu32 cp,int *piByte);` |
|         - | 5882 | `#endif /* PH7_ENABLE_JIS */` |
|         - | 5883 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 5884 | `PH7_PRIVATE const char * PH7_VmBuiltinSigLookup(const char *zName,sxu32 nLen,const char **pzRet);` |
|         - | 5885 | `PH7_PRIVATE void PH7_VmStoreArgByRef(ph7_vm *pVm,ph7_value *pArg,ph7_value *pNewVal);` |
|         - | 5886 | `PH7_PRIVATE sxi32 PH7_ValueToStringUV(ph7_context *pCtx,ph7_value *pValue,const char **pzData,int *pnLen);` |
|         - | 5887 | `PH7_PRIVATE void PH7_ValueToStringUVOnce(ph7_context *pCtx,ph7_value *pValue,const char **pzData,int *pnLen);` |
|         - | 5888 | `PH7_PRIVATE ph7_class *PH7_ValueToStringUVDefer(ph7_context *pCtx,ph7_value *pValue,const char **pzData,int *pnLen);` |
|         - | 5889 | `PH7_PRIVATE void PH7_VmThrowWarningFmt(ph7_vm *pVm,const char *zFmt,...);` |
|         - | 5890 | `PH7_PRIVATE sxi32 PH7_CheckCallbackArg(ph7_context *pCtx,ph7_value *pCb,int iArg,const char *zParam,int bNullable);` |
|         - | 5891 | `PH7_PRIVATE sxi32 PH7_VmInstallReflection(ph7_vm *pVm);` |
|         - | 5892 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionTypes(ph7_vm *pVm); /* vm_builtin_reflection.c */` |
|         - | 5893 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionSmall(ph7_vm *pVm); /* vm_builtin_reflection.c */` |
|         - | 5894 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionAttribute(ph7_vm *pVm); /* vm_builtin_reflection.c */` |
|         - | 5895 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionEnum(ph7_vm *pVm); /* vm_builtin_reflection.c */` |
|         - | 5896 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionClass(ph7_vm *pVm); /* vm_builtin_reflection.c */` |
|         - | 5897 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionFunc(ph7_vm *pVm);  /* vm_builtin_reflection.c */` |
|         - | 5898 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionHookType(ph7_vm *pVm); /* vm_builtin_reflection.c */` |
|         - | 5899 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionMember(ph7_vm *pVm); /* vm_builtin_reflection.c */` |
|         - | 5900 | `PH7_PRIVATE sxi32 PH7_ClosurePresent(ph7_vm *pVm,ph7_class_instance *pThis,` |
|         - | 5901 | `	ph7_value *pOut,int bDebug); /* vm_builtin_reflection.c: Closure's ph7_class::xPresent */` |
|         - | 5902 | `PH7_PRIVATE sxi32 PH7_VmInstallBuiltinLib(ph7_vm *pVm); /* vm_builtin_lib.c */` |
|         - | 5903 | `PH7_PRIVATE ph7_class_instance * PH7_VmNewClosure(ph7_vm *pVm,const SyString *pName,` |
|         - | 5904 | `	ph7_class_instance *pBoundThis,const SyString *pScope);` |
|         - | 5905 | `PH7_PRIVATE sxi32 PH7_VmEnforcePropStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue);` |
|         - | 5906 | `PH7_PRIVATE int PH7_VmSlotRefCount(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 5907 | `PH7_PRIVATE sxi32 PH7_VmInit(ph7_vm *pVm,ph7 *pEngine);` |
|         - | 5908 | `PH7_PRIVATE sxi32 PH7_VmConfigure(ph7_vm *pVm,sxi32 nOp,va_list ap);` |
|         - | 5909 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm);` |
|         - | 5910 | `/* Fiber API helpers (used by api.c) */` |
|         - | 5911 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal);` |
|         - | 5912 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult);` |
|         - | 5913 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult);` |
|         - | 5914 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber);` |
|         - | 5915 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber);` |
|         - | 5916 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber);` |
|         - | 5917 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm);` |
|         - | 5918 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm);` |
|         - | 5919 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(ph7_vm *pVm);` |
|         - | 5920 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm);` |
|         - | 5921 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm);` |
|         - | 5922 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm);` |
|         - | 5923 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm);` |
|         - | 5924 | `PH7_PRIVATE VmInstr *PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex);` |
|         - | 5925 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm);` |
|         - | 5926 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer);` |
|         - | 5927 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(ph7_vm *pVm,sxi32 iOp,sxi32 iP1,sxu32 iP2,void *p3,sxu32 *pIndex);` |
|         - | 5928 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm);` |
|         - | 5929 | `/* The wall clock as php reads it: epoch seconds and the sub-second microseconds,` |
|         - | 5930 | ` * through whatever source this build/embedder has (see DateNow). uniqid() is the` |
|         - | 5931 | ` * second caller after the date surface itself. */` |
|         - | 5932 | `PH7_PRIVATE void PH7_VmClockNow(ph7_vm *pVm,ph7_int64 *pSec,ph7_int64 *pUsec);` |
|         - | 5933 | ``/* php's `php_combined_lcg()`: a double in [0,1) from the two L'Ecuyer streams on`` |
|         - | 5934 | ` * the VM. Seeds them on first use. */` |
|         - | 5935 | `PH7_PRIVATE double PH7_VmCombinedLcg(ph7_vm *pVm);` |
|         - | 5936 | `PH7_PRIVATE void PH7_VmMtSrand(ph7_vm *pVm,sxu32 nSeed,int bLegacyTwist);` |
|         - | 5937 | `PH7_PRIVATE sxu32 PH7_VmMtRand(ph7_vm *pVm);` |
|         - | 5938 | `PH7_PRIVATE sxi64 PH7_VmMtRandRange(ph7_vm *pVm,sxi64 iMin,sxi64 iMax);` |
|         - | 5939 | `PH7_PRIVATE void PH7_VmReleaseInstanceAttr(ph7_vm *pVm,VmClassAttr *pVmAttr);` |
|         - | 5940 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_method *pMethod,` |
|         - | 5941 | `	ph7_value *pResult,int nArg,ph7_value **apArg);` |
|         - | 5942 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethodMap(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_method *pMethod,` |
|         - | 5943 | `	ph7_value *pResult,int nArg,ph7_value **apArg,VmCallArgMap *pMap);` |
|         - | 5944 | `PH7_PRIVATE sxi32 PH7_VmCallMethodSwallow(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_method *pMethod,` |
|         - | 5945 | `	ph7_value *pResult,int nArg,ph7_value **apArg,int *pbThrew);` |
|         - | 5946 | `PH7_PRIVATE sxi32 PH7_VmCallMethodUnchecked(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_method *pMethod,` |
|         - | 5947 | `	ph7_value *pResult,int nArg,ph7_value **apArg);` |
|         - | 5948 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(ph7_vm *pVm,ph7_value *pFunc,int nArg,ph7_value **apArg,ph7_value *pResult);` |
|         - | 5949 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionWithMap(ph7_vm *pVm,ph7_value *pFunc,int nArg,ph7_value **apArg,ph7_value *pResult,VmCallArgMap *pArgMap);` |
|         - | 5950 | `PH7_PRIVATE ph7_class_instance * PH7_VmCallerThisFor(ph7_vm *pVm,ph7_class *pClass);` |
|         - | 5951 | `PH7_PRIVATE ph7_class_instance * PH7_VmStaticFallbackThis(ph7_vm *pVm,ph7_class *pClass);` |
|         - | 5952 | `PH7_PRIVATE sxi32 PH7_VmDispatchMagicCall(ph7_vm *pVm,ph7_class *pClass,ph7_class_instance *pThis,` |
|         - | 5953 | `	const char *zName,sxu32 nName,ph7_value *pResult,int nArg,ph7_value **apArg,VmCallArgMap *pArgMap);` |
|         - | 5954 | `/* Per-element callback for PH7_VmIteratorWalk: return SXRET_OK to continue,` |
|         - | 5955 | ` * SXERR_EOF to stop early (not an error), or PH7_EXCEPTION/PH7_ABORT to propagate. */` |
|         - | 5956 | `typedef sxi32 (*ProcIterStep)(ph7_vm *pVm,ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|         - | 5957 | `PH7_PRIVATE sxi32 PH7_VmIteratorWalk(ph7_vm *pVm,ph7_value *pObj,ProcIterStep xStep,void *pUserData);` |
|         - | 5958 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(ph7_vm *pVm,ph7_value *pFunc,ph7_value *pResult,...);` |
|         - | 5959 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce);` |
|         - | 5960 | `PH7_PRIVATE void PH7_VmWarnByRefArgsGivenValue(ph7_vm *pVm,ph7_value *pCallable,int nArg,ph7_hashmap_node **apNode,SyString *aNames);` |
|         - | 5961 | `PH7_PRIVATE void PH7_VmCufDropByRefArgs(ph7_context *pCtx,ph7_value *pCallable,int nArg,ph7_value **apArg);` |
|         - | 5962 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen);` |
|         - | 5963 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm);` |
|         - | 5964 | `PH7_PRIVATE ph7_class * PH7_VmPeekSelfClass(ph7_vm *pVm);` |
|         - | 5965 | `PH7_PRIVATE ph7_class * PH7_VmTraitUsingClass(ph7_vm *pVm,ph7_class *pTrait,ph7_class *pFrom);` |
|         - | 5966 | `PH7_PRIVATE ph7_class * PH7_VmMemberOwnerClass(ph7_class *pDeclClass,ph7_class *pFrom);` |
|         - | 5967 | `PH7_PRIVATE int PH7_VmEvalConstExpr(ph7_vm *pVm,SySet *pByteCode,ph7_value *pOut);` |
|         - | 5968 | `PH7_PRIVATE void PH7_ClassRenderDecl(ph7_vm *pVm,ph7_class *pScope,ph7_vm_func *pFunc,SyBlob *pOut);` |
|         - | 5969 | `PH7_PRIVATE sxi32 PH7_ClassCheckOverrideCompat(ph7_gen_state *pGen,ph7_class *pBase,ph7_class *pSub,` |
|         - | 5970 | `	ph7_class_method *pParent,ph7_class_method *pChild,int bCtorExempt);` |
|         - | 5971 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm);` |
|         - | 5972 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke);` |
|         - | 5973 | `PH7_PRIVATE int PH7_VmArrayCallableParts(ph7_vm *pVm,ph7_hashmap *pMap,ph7_value **ppTarget,ph7_value **ppMethod);` |
|         - | 5974 | `PH7_PRIVATE int PH7_VmCallableMethodAccessible(ph7_vm *pVm,ph7_class *pClass,ph7_class_method *pMethod);` |
|         - | 5975 | `PH7_PRIVATE int PH7_VmCallableStringParts(const char *zName,sxu32 nName,` |
|         - | 5976 | `	const char **pzCls,sxu32 *pnCls,const char **pzMeth,sxu32 *pnMeth);` |
|         - | 5977 | `PH7_PRIVATE int PH7_VmClassLookupRaised(ph7_vm *pVm,sxi32 nBrcBefore,const void *pResumeBefore);` |
|         - | 5978 | `PH7_PRIVATE const char * PH7_VmCallableReason(ph7_vm *pVm,ph7_value *pValue,char *zBuf,int nBuf);` |
|         - | 5979 | `PH7_PRIVATE void PH7_VmCallableName(ph7_vm *pVm,ph7_value *pValue,SyBlob *pOut);` |
|         - | 5980 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(ph7_vm *pVm,const char *zName,sxu32 nByte);` |
|         - | 5981 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(ph7_hashmap *pMap,const char *zKey,int nKeylen,const char *zData,int nLen);` |
|         - | 5982 | `/* The file:// strip is pure string work and the VFS layer needs it in every` |
|         - | 5983 | ` * build, disk IO enabled or not. */` |
|         - | 5984 | `PH7_PRIVATE const char * PH7_VmFileUrlLocalPath(const char *zPath);` |
|         - | 5985 | `PH7_PRIVATE int PH7_VmUrlSchemeLen(const char *zIn,int nByte);` |
|         - | 5986 | `#ifndef PH7_DISABLE_DISK_IO` |
|         - | 5987 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(ph7_vm *pVm,const char **pzDevice,int nByte);` |
|         - | 5988 | `PH7_PRIVATE int PH7_VmStreamDeviceSuppressed(ph7_vm *pVm,const ph7_io_stream *pStream);` |
|         - | 5989 | `PH7_PRIVATE ph7_io_stream * PH7_VmFindStreamDevice(ph7_vm *pVm,const char *zName,int nName);` |
|         - | 5990 | `PH7_PRIVATE int PH7_VmStreamSchemeDisabled(ph7_vm *pVm,const char *zName,int nName);` |
|         - | 5991 | `PH7_PRIVATE int PH7_VmStreamDeviceIsRemoteHost(const char *zUri,int nByte,int *pnScheme);` |
|         - | 5992 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|         - | 5993 | `/* vm_http.c function prototypes */` |
|         - | 5994 | `PH7_PRIVATE sxi32 PH7_VmHttpSplitURI(SyhttpUri *pOut,const char *zUri,sxu32 nLen);` |
|         - | 5995 | `PH7_PRIVATE sxi32 PH7_VmHttpProcessRequest(ph7_vm *pVm,const char *zRequest,int nByte);` |
|         - | 5996 | `/* vm_http_response.c function prototypes */` |
|         - | 5997 | `PH7_PRIVATE void PH7_RegisterHttpResponseFunctions(ph7_vm *pVm);` |
|         - | 5998 | `PH7_PRIVATE void PH7_VmReleaseResponseHeaders(ph7_vm *pVm);` |
|         - | 5999 | `PH7_PRIVATE int PH7_VmHttpDate(sxi64 iWhen,char *zBuf,int nBuf);` |
|         - | 6000 | `PH7_PRIVATE void PH7_VmSetResponseHeader(ph7_vm *pVm,const char *zName,const char *zValue,sxu32 nValue);` |
|         - | 6001 | `PH7_PRIVATE void PH7_VmRemoveCookieByName(ph7_vm *pVm,const char *zName,sxu32 nName);` |
|         - | 6002 | `PH7_PRIVATE void PH7_VmEmitCookie(ph7_vm *pVm,const char *zName,sxu32 nName,` |
|         - | 6003 | `	const char *zValue,sxu32 nValue,int bEncode,sxi64 iExpires,` |
|         - | 6004 | `	const char *zPath,sxu32 nPath,const char *zDomain,sxu32 nDomain,` |
|         - | 6005 | `	int bSecure,int bHttpOnly,const char *zSame,sxu32 nSame,int bPartitioned);` |
|         - | 6006 | `/* vm_pcre.c function prototypes */` |
|         - | 6007 | `#ifdef PH7_ENABLE_PCRE` |
|         - | 6008 | `PH7_PRIVATE void PH7_RegisterPcreFunctions(ph7_vm *pVm);` |
|         - | 6009 | `PH7_PRIVATE int PH7_MbEncodingLookup(const char *z,int n);` |
|         - | 6010 | `PH7_PRIVATE const char * PH7_MbEncodingCanonical(int iNameId);` |
|         - | 6011 | `PH7_PRIVATE int PH7_MbEncodingIsUtf8(int iNameId);` |
|         - | 6012 | `PH7_PRIVATE void PH7_RegisterPcreConstants(ph7_vm *pVm);` |
|         - | 6013 | `PH7_PRIVATE sxi32 PH7_PcreMatchQuiet(ph7_context *pCtx,const char *zPat,int nPat,` |
|         - | 6014 | `	const char *zSub,int nSub,int *pMatched);` |
|         - | 6015 | `PH7_PRIVATE int PH7_PcrePatternCheck(ph7_vm *pVm,const char *zPattern,int nLen,` |
|         - | 6016 | `	char *zErr,sxu32 nErr);` |
|         - | 6017 | `/* RegexIterator's five operation modes, php's REGIT_MODE_* values — they are the` |
|         - | 6018 | ` * class constants, so the numbers are php-visible and fixed. */` |
|         - | 6019 | `#define PH7_REGIT_MATCH        0` |
|         - | 6020 | `#define PH7_REGIT_GET_MATCH    1` |
|         - | 6021 | `#define PH7_REGIT_ALL_MATCHES  2` |
|         - | 6022 | `#define PH7_REGIT_SPLIT        3` |
|         - | 6023 | `#define PH7_REGIT_REPLACE      4` |
|         - | 6024 | `PH7_PRIVATE sxi32 PH7_PcreRegitApply(ph7_context *pCtx,int iMode,ph7_value *pPattern,` |
|         - | 6025 | `	ph7_value *pSubject,int iPregFlags,ph7_value *pRepl,ph7_value *pOut,int *pbOk);` |
|         - | 6026 | `#endif /* PH7_ENABLE_PCRE */` |
|         - | 6027 | `/* One resource pointer's php-visible id. Allocated per distinct resource and` |
|         - | 6028 | `` * owned by ph7_vm.hResourceId, whose key is the `pRes` field itself (SyHash`` |
|         - | 6029 | ` * stores the key POINTER, so it must outlive the entry). Core (used by` |
|         - | 6030 | ` * PH7_VmResourceId) — must NOT sit under PH7_ENABLE_LIBXML or the tiny build,` |
|         - | 6031 | ` * which omits libxml, fails to compile it. */` |
|         - | 6032 | `typedef struct phl_res_id phl_res_id;` |
|         - | 6033 | `struct phl_res_id {` |
|         - | 6034 | `	void *pRes;   /* The resource pointer, and the hash key */` |
|         - | 6035 | `	sxu32 nId;    /* php-visible id */` |
|         - | 6036 | `};` |
|         - | 6037 | `#ifdef PH7_ENABLE_LIBXML` |
|         - | 6038 | `/* One entry in the per-VM libxml error queue (mirrors php's LibXMLError:` |
|         - | 6039 | ` * level/code/column/message/file/line).  Strings are SyMemBackend copies` |
|         - | 6040 | ` * owned by the queue and released by PH7_LibxmlClearErrors(). */` |
|         - | 6041 | `typedef struct phl_libxml_err phl_libxml_err;` |
|         - | 6042 | `struct phl_libxml_err {` |
|         - | 6043 | `	int iLevel;      /* LIBXML_ERR_WARNING/ERROR/FATAL */` |
|         - | 6044 | `	int iCode;       /* raw libxml2 error code */` |
|         - | 6045 | `	int iLine;` |
|         - | 6046 | `	int iColumn;` |
|         - | 6047 | `	SyString sMsg;   /* message text, trailing newline preserved (php parity) */` |
|         - | 6048 | `	SyString sFile;  /* source file/URI, empty for in-memory strings */` |
|         - | 6049 | `};` |
|         - | 6050 | `/* Per-VM owner of one libxml document tree.  See the lifetime notes at the` |
|         - | 6051 | ` * top of vm_libxml.c: docs are only freed at VM reset/release, never while` |
|         - | 6052 | ` * PHP code could still hold a wrapper into them. */` |
|         - | 6053 | `typedef struct phl_xmldoc phl_xmldoc;` |
|         - | 6054 | `struct phl_xmldoc {` |
|         - | 6055 | `	void *pDoc;         /* xmlDocPtr (void* keeps libxml headers out of ph7int.h) */` |
|         - | 6056 | `	SySet aOrphans;     /* xmlNodePtr's unlinked from the tree but still owned */` |
|         - | 6057 | `	/* The stand-in NODES a DTD's NOTATION declarations are answered through:` |
|         - | 6058 | `	 * an xmlNotation is {name, PublicID, SystemID} and nothing else -- it has` |
|         - | 6059 | `	 * no type field, so it cannot be walked as a node -- and php builds an` |
|         - | 6060 | `	 * entity-shaped node per lookup. One per declaration is built here and` |
|         - | 6061 | `	 * kept, so the wrapper identity every other node has holds for these too;` |
|         - | 6062 | `	 * they need their own free (vm_libxml.c), since xmlFreeNode would read` |
|         - | 6063 | `	 * an xmlEntity's length/etype pair as a node's property list. */` |
|         - | 6064 | `	SySet aNotations;   /* synthesized XML_NOTATION_NODE xmlNodePtr's */` |
|         - | 6065 | `	ph7_vm *pVm;        /* Owning VM (error routing from libxml callbacks) */` |
|         - | 6066 | `	void *pDocObj;      /* The DOMDocument wrapper for this tree, BORROWED, or 0.` |
|         - | 6067 | `	                     * ext/dom keys its per-node wrapper cache on the document` |
|         - | 6068 | ``	                     * OBJECT, so `dom_import_simplexml()` needs the one this`` |
|         - | 6069 | `	                     * tree already has -- that is what makes two imports of the` |
|         - | 6070 | `	                     * same node the same DOMElement, and what makes an import` |
|         - | 6071 | `	                     * back out of a SimpleXML that came FROM a DOMDocument` |
|         - | 6072 | `	                     * answer that document's own nodes. Cleared by` |
|         - | 6073 | `	                     * DOMDocument's xRelease when the object goes, so the` |
|         - | 6074 | `	                     * pointer is never stale. */` |
|         - | 6075 | `	int bPreserveWS;    /* DOMDocument->preserveWhiteSpace */` |
|         - | 6076 | `	int bFormatOutput;  /* DOMDocument->formatOutput */` |
|         - | 6077 | `	phl_xmldoc *pNext;  /* Registry chain (pVm->pXmlDocs) */` |
|         - | 6078 | `};` |
|         - | 6079 | `/* One PHP-visible DOM node handle: the MEMOBJ_RES payload behind every DOM` |
|         - | 6080 | ` * wrapper object.  pNode points into pShell's tree (or IS the xmlDoc); the` |
|         - | 6081 | ` * shell outlives every handle (docs are only freed at VM reset/release). */` |
|         - | 6082 | `typedef struct phl_domnode phl_domnode;` |
|         - | 6083 | `struct phl_domnode {` |
|         - | 6084 | `	phl_xmldoc *pShell; /* Owning document registry entry */` |
|         - | 6085 | `	void *pNode;        /* xmlNodePtr / xmlDocPtr / xmlAttrPtr */` |
|         - | 6086 | `};` |
|         - | 6087 | `/* vm_libxml.c */` |
|         - | 6088 | `PH7_PRIVATE void PH7_RegisterLibxmlConstants(ph7_vm *pVm);` |
|         - | 6089 | `PH7_PRIVATE sxi32 PH7_VmInstallLibxml(ph7_vm *pVm);` |
|         - | 6090 | `PH7_PRIVATE void PH7_LibxmlVmReset(ph7_vm *pVm);` |
|         - | 6091 | `PH7_PRIVATE void PH7_LibxmlVmRelease(ph7_vm *pVm);` |
|         - | 6092 | `PH7_PRIVATE void PH7_LibxmlClearErrors(ph7_vm *pVm);` |
|         - | 6093 | `PH7_PRIVATE phl_xmldoc * PH7_LibxmlNewDoc(ph7_vm *pVm,void *pXmlDocPtr);` |
|         - | 6094 | `PH7_PRIVATE sxu32 PH7_LibxmlCaptureBegin(ph7_vm *pVm);` |
|         - | 6095 | `PH7_PRIVATE sxu32 PH7_LibxmlCaptureBeginRaw(ph7_vm *pVm);` |
|         - | 6096 | `PH7_PRIVATE void PH7_LibxmlCaptureEnd(ph7_vm *pVm,sxu32 nMark,const char *zFnName);` |
|         - | 6097 | `PH7_PRIVATE void PH7_LibxmlDropErrors(ph7_vm *pVm,sxu32 nMark);` |
|         - | 6098 | `/* Push one error onto the per-VM queue + last-error slot (strings copied).` |
|         - | 6099 | ` * The shared structured-error callback and the DOM schema error hooks both` |
|         - | 6100 | ` * funnel through this so ph7int.h needs no libxml types. */` |
|         - | 6101 | `PH7_PRIVATE void PH7_LibxmlCaptureEndOpts(ph7_vm *pVm,sxu32 nMark,const char *zFnName,int iOpts);` |
|         - | 6102 | `PH7_PRIVATE void PH7_LibxmlRaiseGeneric(ph7_vm *pVm,const char *zFnName,const char *zMsg);` |
|         - | 6103 | `PH7_PRIVATE void PH7_LibxmlQueueError(ph7_vm *pVm,int iLevel,int iCode,int iLine,int iColumn,` |
|         - | 6104 | `	const char *zMsg,const char *zFile);` |
|         - | 6105 | `/* vm_dom.c */` |
|         - | 6106 | `PH7_PRIVATE sxi32 PH7_VmInstallDom(ph7_vm *pVm);` |
|         - | 6107 | `/* Read a document's bytes for a loader, through php's own stream layer, with` |
|         - | 6108 | `` * libxml's `failed to load external entity` warning already raised for a file`` |
|         - | 6109 | ` * that is not there. ext/simplexml's two file doors want exactly what` |
|         - | 6110 | ` * DOMDocument::load() wants. */` |
|         - | 6111 | `PH7_PRIVATE int PH7_DomReadFile(ph7_context *pCtx,const char *zFile,int nFile,const char *zFn,` |
|         - | 6112 | `	SyBlob *pBody,SyBlob *pPath);` |
|         - | 6113 | `/* The DOMDocument-cached wrapper for one node of pShell's tree, creating the` |
|         - | 6114 | ` * document object if this tree has none yet. ext/simplexml's` |
|         - | 6115 | ` * dom_import_simplexml() is the only caller: every other wrap already has a` |
|         - | 6116 | ` * document object in hand. */` |
|         - | 6117 | `PH7_PRIVATE ph7_class_instance * PH7_DomWrapForeign(ph7_vm *pVm,phl_xmldoc *pShell,void *pNode);` |
|         - | 6118 | `/* vm_simplexml.c */` |
|         - | 6119 | `PH7_PRIVATE sxi32 PH7_VmInstallSimpleXml(ph7_vm *pVm);` |
|         - | 6120 | `/* vm_xmlwriter.c */` |
|         - | 6121 | `PH7_PRIVATE sxi32 PH7_VmInstallXmlWriter(ph7_vm *pVm);` |
|         - | 6122 | `PH7_PRIVATE void PH7_XmlWriterVmSweep(ph7_vm *pVm);` |
|         - | 6123 | `/* vm_xml.c (php's ext/xml: the expat-style push-parser surface over libxml2) */` |
|         - | 6124 | `PH7_PRIVATE sxi32 PH7_VmInstallXml(ph7_vm *pVm);` |
|         - | 6125 | `PH7_PRIVATE void PH7_XmlParserVmSweep(ph7_vm *pVm);` |
|         - | 6126 | `#endif /* PH7_ENABLE_LIBXML */` |
|         - | 6127 | `#ifdef PH7_ENABLE_SQLITE` |
|         - | 6128 | `/* vm_pdo.c (ext/pdo: the driver-independent class library) */` |
|         - | 6129 | `PH7_PRIVATE sxi32 PH7_VmInstallPdo(ph7_vm *pVm);` |
|         - | 6130 | `PH7_PRIVATE void PH7_PdoVmReset(ph7_vm *pVm);` |
|         - | 6131 | `PH7_PRIVATE void PH7_PdoVmRelease(ph7_vm *pVm);` |
|         - | 6132 | `/* vm_pdo_sqlite.c (ext/pdo_sqlite: the driver and its Pdo\Sqlite subclass) */` |
|         - | 6133 | `PH7_PRIVATE sxi32 PH7_VmInstallPdoSqlite(ph7_vm *pVm);` |
|         - | 6134 | `/* vm_sqlite3.c (ext/sqlite3: php's other sqlite surface, the SQLite3 class family) */` |
|         - | 6135 | `PH7_PRIVATE sxi32 PH7_VmInstallSqlite3(ph7_vm *pVm);` |
|         - | 6136 | `PH7_PRIVATE void PH7_RegisterSqlite3Constants(ph7_vm *pVm);` |
|         - | 6137 | `PH7_PRIVATE void PH7_Sqlite3VmReset(ph7_vm *pVm);` |
|         - | 6138 | `PH7_PRIVATE void PH7_Sqlite3VmRelease(ph7_vm *pVm);` |
|         - | 6139 | `#endif /* PH7_ENABLE_SQLITE */` |
|         - | 6140 | `#ifdef PH7_ENABLE_CURL` |
|         - | 6141 | `/* vm_curl.c (ext/curl: php's libcurl binding) */` |
|         - | 6142 | `PH7_PRIVATE sxi32 PH7_VmInstallCurl(ph7_vm *pVm);` |
|         - | 6143 | `PH7_PRIVATE void PH7_RegisterCurlConstants(ph7_vm *pVm);` |
|         - | 6144 | `PH7_PRIVATE void PH7_CurlVmReset(ph7_vm *pVm);` |
|         - | 6145 | `PH7_PRIVATE void PH7_CurlVmRelease(ph7_vm *pVm);` |
|         - | 6146 | `#endif /* PH7_ENABLE_CURL */` |
|         - | 6147 | `/* net.c types and function prototypes */` |
|         - | 6148 | `#ifdef PH7_ENABLE_NET` |
|         - | 6149 | `#ifdef __WINNT__` |
|         - | 6150 | `#include <winsock2.h>` |
|         - | 6151 | `typedef SOCKET ph7_socket;` |
|         - | 6152 | `typedef int ph7_socklen;` |
|         - | 6153 | `#define PH7_NET_INVALID_SOCKET INVALID_SOCKET` |
|         - | 6154 | `#else` |
|         - | 6155 | `typedef int ph7_socket;` |
|         - | 6156 | `typedef unsigned int ph7_socklen;` |
|         - | 6157 | `#define PH7_NET_INVALID_SOCKET (-1)` |
|         - | 6158 | `#endif` |
|         - | 6159 | `struct sockaddr; /* Forward declaration */` |
|         - | 6160 | `/* The one failure whose MESSAGE only the caller can word: php answers` |
|         - | 6161 | `` * `php_network_getaddresses: getaddrinfo for <host> failed: ...` with the host`` |
|         - | 6162 | ` * in it, reports no OS code beside it, and raises it TWICE — once from the` |
|         - | 6163 | ` * transport and once from the opener that asked. */` |
|         - | 6164 | `#define PH7_NET_ERR_RESOLVE (-3)` |
|         - | 6165 | `PH7_PRIVATE int PH7_NetInit(void);` |
|         - | 6166 | `PH7_PRIVATE int PH7_NetEnsureInit(void);` |
|         - | 6167 | `PH7_PRIVATE void PH7_NetCleanup(void);` |
|         - | 6168 | `PH7_PRIVATE ph7_socket PH7_NetListen(const char *zHost,int iPort,int iBacklog);` |
|         - | 6169 | `/*` |
|         - | 6170 | `` * The `socket` context options net.c can apply, php's own option names. A NULL`` |
|         - | 6171 | ` * pointer means "none of them", which is what every internal opener passes.` |
|         - | 6172 | ` * so_broadcast and ipv6_v6only describe a datagram socket and an address family` |
|         - | 6173 | ` * this build has not got (recorded), so they are stored on the context` |
|         - | 6174 | ` * and never reach a socket.` |
|         - | 6175 | ` */` |
|         - | 6176 | `typedef struct ph7_sockopts ph7_sockopts;` |
|         - | 6177 | `struct ph7_sockopts` |
|         - | 6178 | `{` |
|         - | 6179 | ``	const char *zBindHost; /* `bindto`'s host half, already parsed (0 = no bind) */`` |
|         - | 6180 | ``	int iBindPort;         /* `bindto`'s port half */`` |
|         - | 6181 | ``	int bReusePort;        /* `so_reuseport` */`` |
|         - | 6182 | ``	int bNoDelay;          /* `tcp_nodelay` */`` |
|         - | 6183 | ``	int bBroadcast;        /* `so_broadcast`: what a DATAGRAM socket needs before`` |
|         - | 6184 | `	                        * it may address 255.255.255.255 at all */` |
|         - | 6185 | ``	int bV6Only;           /* `ipv6_v6only`, applied to an AF_INET6 listener */`` |
|         - | 6186 | ``	int iBacklog;          /* `backlog`; <= 0 keeps the transport's default */`` |
|         - | 6187 | `	/* OUT: how the local bind failed on the socket that was USED, which php` |
|         - | 6188 | `	 * warns about in two different wordings and never treats as fatal. */` |
|         - | 6189 | `	int iBindErr;          /* PH7_SOCKOPT_BIND_* (0 = it worked, or none asked) */` |
|         - | 6190 | `	int iBindErrno;        /* the OS code behind PH7_SOCKOPT_BIND_REFUSED */` |
|         - | 6191 | `};` |
|         - | 6192 | `#define PH7_SOCKOPT_BIND_RESOLVE 1 /* not a numeric address (php never resolves one) */` |
|         - | 6193 | `#define PH7_SOCKOPT_BIND_REFUSED 2 /* bind() itself said no */` |
|         - | 6194 | `PH7_PRIVATE ph7_socket PH7_NetBind(const char *zHost,int iPort,int bDgram,int bListen,` |
|         - | 6195 | `	int iBacklog,const ph7_sockopts *pOpt,int *pErrno,const char **pzErr);` |
|         - | 6196 | `PH7_PRIVATE ph7_socket PH7_NetConnect(const char *zHost,int iPort,int iTimeoutMs,` |
|         - | 6197 | `	int bDgram,int bAsync,ph7_sockopts *pOpt,int *pErrno,const char **pzErr);` |
|         - | 6198 | `PH7_PRIVATE ph7_socket PH7_NetAccept(ph7_socket listenSock,struct sockaddr *pAddr,ph7_socklen *pAddrLen);` |
|         - | 6199 | `PH7_PRIVATE ph7_socket PH7_NetAcceptTimed(ph7_socket listenSock,int iTimeoutMs,int *pbTimedOut,` |
|         - | 6200 | `	char *zPeer,int nPeer);` |
|         - | 6201 | `PH7_PRIVATE int PH7_NetSockName(ph7_socket sock,int bPeer,char *zBuf,int nBuf);` |
|         - | 6202 | `PH7_PRIVATE int PH7_NetHostName(char *zBuf,int nBuf,int *pErrno);` |
|         - | 6203 | `PH7_PRIVATE int PH7_NetWait(ph7_socket sock,int bWrite,int iTimeoutMs);` |
|         - | 6204 | `/* The platform-numbered socket constants, asked for by id because their VALUES` |
|         - | 6205 | ` * differ per OS (AF_INET6 is 10, 23 and 30 on three of them). */` |
|         - | 6206 | `#define PH7_NETC_PF_INET        1` |
|         - | 6207 | `#define PH7_NETC_PF_INET6       2` |
|         - | 6208 | `#define PH7_NETC_PF_UNIX        3` |
|         - | 6209 | `#define PH7_NETC_SOCK_STREAM    4` |
|         - | 6210 | `#define PH7_NETC_SOCK_DGRAM     5` |
|         - | 6211 | `#define PH7_NETC_SOCK_RAW       6` |
|         - | 6212 | `#define PH7_NETC_SOCK_SEQPACKET 7` |
|         - | 6213 | `#define PH7_NETC_SOCK_RDM       8` |
|         - | 6214 | `#define PH7_NETC_IPPROTO_IP     9` |
|         - | 6215 | `#define PH7_NETC_IPPROTO_TCP   10` |
|         - | 6216 | `#define PH7_NETC_IPPROTO_UDP   11` |
|         - | 6217 | `#define PH7_NETC_IPPROTO_ICMP  12` |
|         - | 6218 | `#define PH7_NETC_IPPROTO_RAW   13` |
|         - | 6219 | `PH7_PRIVATE ph7_int64 PH7_NetSocketConst(int iWhich);` |
|         - | 6220 | `PH7_PRIVATE int PH7_NetShutdown(ph7_socket sock,int iHow);` |
|         - | 6221 | `PH7_PRIVATE int PH7_NetAtEnd(ph7_socket sock);` |
|         - | 6222 | `PH7_PRIVATE int PH7_NetIsAlive(ph7_socket sock);` |
|         - | 6223 | `PH7_PRIVATE int PH7_NetRecvFrom(ph7_socket sock,void *pBuf,int nLen,int iFlags,char *zAddr,int nAddr);` |
|         - | 6224 | `PH7_PRIVATE int PH7_NetSendTo(ph7_socket sock,const void *pBuf,int nLen,int iFlags,` |
|         - | 6225 | `	const char *zHost,int iPort,int *pErrno);` |
|         - | 6226 | `PH7_PRIVATE int PH7_NetSocketPair(int iDomain,int iType,int iProtocol,ph7_socket *aOut,int *pErrno);` |
|         - | 6227 | `PH7_PRIVATE int PH7_NetLastError(void);` |
|         - | 6228 | `PH7_PRIVATE int PH7_NetWouldBlock(void);` |
|         - | 6229 | `PH7_PRIVATE const char * PH7_NetStrError(int iErr);` |
|         - | 6230 | `PH7_PRIVATE int PH7_NetRecv(ph7_socket sock,void *pBuf,int nLen,int flags);` |
|         - | 6231 | `PH7_PRIVATE int PH7_NetSend(ph7_socket sock,const void *pBuf,int nLen,int flags);` |
|         - | 6232 | `PH7_PRIVATE int PH7_NetSendAll(ph7_socket sock,const void *pBuf,int nLen);` |
|         - | 6233 | `PH7_PRIVATE void PH7_NetClose(ph7_socket sock);` |
|         - | 6234 | `PH7_PRIVATE void PH7_NetSetTimeout(ph7_socket sock,int iMilliseconds);` |
|         - | 6235 | `PH7_PRIVATE void PH7_NetSetRwTimeout(ph7_socket sock,ph7_int64 iSeconds,ph7_int64 iMicroseconds);` |
|         - | 6236 | `PH7_PRIVATE void PH7_NetSetBlocking(ph7_socket sock,int bBlocking);` |
|         - | 6237 | `PH7_PRIVATE void PH7_NetAddrToString(const struct sockaddr *pAddr,char *zBuf,int nBufLen);` |
|         - | 6238 | `PH7_PRIVATE int PH7_NetAddrPort(const struct sockaddr *pAddr);` |
|         - | 6239 | `/* ext/sockets (builtin_sockets.c): php's BSD socket API, which is the other` |
|         - | 6240 | ` * face of the descriptors net.c drives for the stream wrappers. */` |
|         - | 6241 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 6242 | `PH7_PRIVATE const ph7_builtin_func * PH7_SocketsFuncTable(sxu32 *pnEntry);` |
|         - | 6243 | `PH7_PRIVATE sxi32 PH7_VmInstallSockets(ph7_vm *pVm);` |
|         - | 6244 | `PH7_PRIVATE void PH7_RegisterSocketsConstants(ph7_vm *pVm);` |
|         - | 6245 | `PH7_PRIVATE void PH7_SocketsVmReset(ph7_vm *pVm);` |
|         - | 6246 | `PH7_PRIVATE void PH7_SocketsVmRelease(ph7_vm *pVm);` |
|         - | 6247 | `/* socket_strerror()'s table: the platform's own, plus php's -10000 host-lookup` |
|         - | 6248 | ` * range, which no errno occupies. */` |
|         - | 6249 | `PH7_PRIVATE const char * PH7_SocketStrError(int iErr);` |
|         - | 6250 | `#endif` |
|         - | 6251 | `/* The three doors ext/sockets uses onto the stream device stack (vfs_stream.c). */` |
|         - | 6252 | `PH7_PRIVATE int PH7_StreamSocketHandle(io_private *pDev,ph7_socket *pOut);` |
|         - | 6253 | `PH7_PRIVATE io_private * PH7_StreamWrapSocket(ph7_context *pCtx,ph7_socket sock,int bDgram,` |
|         - | 6254 | `	const char *zLabel,const char *zUri);` |
|         - | 6255 | `PH7_PRIVATE void PH7_StreamCloseExported(io_private *pDev);` |
|         - | 6256 | `#endif /* PH7_ENABLE_NET */` |
|         - | 6257 | `/* vm_json.c function prototypes */` |
|         - | 6258 | `PH7_PRIVATE int vm_builtin_json_encode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6259 | `PH7_PRIVATE int vm_builtin_json_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6260 | `PH7_PRIVATE int vm_builtin_json_last_error_msg(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6261 | `PH7_PRIVATE int vm_builtin_json_decode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6262 | `PH7_PRIVATE int vm_builtin_json_validate(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6263 | `/* vm_serialize.c function prototypes */` |
|         - | 6264 | `PH7_PRIVATE int vm_builtin_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6265 | `PH7_PRIVATE int vm_builtin_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6266 | `PH7_PRIVATE sxi32 PH7_VmUnserializeOne(ph7_context *pCtx,const char *zIn,int nByte,int *pnRead,ph7_value *pOut);` |
|         - | 6267 | `PH7_PRIVATE void PH7_AppendShortestReal(SyBlob *pOut,double d);` |
|         - | 6268 | `/* memobj.c float-shape helper (php_gcvt/smart_str_append_double semantics);` |
|         - | 6269 | ` * shared by the float->string cast and builtin.c's printf float conversions */` |
|         - | 6270 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         - | 6271 | `PH7_PRIVATE sxi32 PH7_PhpFloatShape(char *zBuf,sxi32 nLen,int bGeneric);` |
|         - | 6272 | `#endif` |
|         - | 6273 | `/* builtin.c utf8 function prototypes */` |
|         - | 6274 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult,int bReturnPropagates);` |
|         - | 6275 | `PH7_PRIVATE int VmLocalExecThrew(ph7_vm *pVm,sxi32 rc,const void *pResumeBefore,const void *pInlineBefore);` |
|         - | 6276 | `PH7_PRIVATE int PH7_VmIsAutoGlobal(const char *zName,sxu32 nByte);` |
|         - | 6277 | `/* The engine's two NON-refusing offset rules, shared with the native ArrayAccess` |
|         - | 6278 | ` * classes: a RESOURCE key warns and is rewritten in place to its integer id, and a` |
|         - | 6279 | ` * NULL key deprecates and then folds to the "" key (the caller falls through). */` |
|         - | 6280 | `PH7_PRIVATE void PH7_VmOffsetResourceWarn(ph7_vm *pVm,ph7_value *pKey);` |
|         - | 6281 | `PH7_PRIVATE int PH7_VmNullOffsetDeprecate(ph7_vm *pVm,ph7_value *pKey);` |
|         - | 6282 | `/* Wording modes for PH7_VmArrayKeyArg(): the RULES are the engine's subscript` |
|         - | 6283 | ` * rules in all three, only the two sentences differ. */` |
|         - | 6284 | ``#define PH7_ARRAYKEY_OFFSET 0 /* the engine's own offset wording, `$a[$k]`'s */`` |
|         - | 6285 | `#define PH7_ARRAYKEY_AKE    1 /* array_key_exists(): engine type wording, its own null clause */` |
|         - | 6286 | `#define PH7_ARRAYKEY_ZPP    2 /* key_exists(): php's ZPP type wording, the same null clause */` |
|         - | 6287 | `PH7_PRIVATE sxi32 PH7_VmArrayKeyArg(ph7_context *pCtx,ph7_value *pKey,int iWording);` |
|         - | 6288 | `PH7_PRIVATE sxi32 PH7_VmExecAttrArg(ph7_vm *pVm,SySet *pByteCode,ph7_class *pDeclCls,ph7_value *pResult);` |
|         - | 6289 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...);` |
|         - | 6290 | `PH7_PRIVATE sxi32 PH7_VmFatalError(ph7_vm *pVm,const char *zFormat,...);` |
|         - | 6291 | `PH7_PRIVATE sxi32 PH7_VmEmitCompileDiagnostic(ph7_vm *pVm,sxi32 iErr,const char *zLabel,const char *zBody,sxu32 nBody,const char *zBare,sxu32 nBare,sxu32 nLine);` |
|         - | 6292 | `PH7_PRIVATE sxu32 PH7_ClassAbstractGap(ph7_vm *pVm,ph7_class *pClass,SyBlob *pMsg);` |
|         - | 6293 | `/* vm_builtin_class.c function prototypes */` |
|         - | 6294 | `PH7_PRIVATE int PH7_VmInstanceOf(ph7_class *pThis,ph7_class *pClass);` |
|         - | 6295 | `PH7_PRIVATE int PH7_SplOuterForward(ph7_vm *pVm,ph7_class_instance *pThis,SyString *pName,ph7_class_instance **ppInner,ph7_class_method **ppMeth);` |
|         - | 6296 | `PH7_PRIVATE void PH7_VmStampThrowableSite(ph7_vm *pVm,ph7_class_instance *pThis);` |
|         - | 6297 | `PH7_PRIVATE int PH7_VmClassMemberAccess(ph7_vm *pVm,ph7_class *pClass,const SyString *pAttrName,sxi32 iProtection,int bLog);` |
|         - | 6298 | `PH7_PRIVATE int PH7_VmClassAttrAccess(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,int bLog);` |
|         - | 6299 | `PH7_PRIVATE ph7_class * PH7_VmCallerScope(ph7_vm *pVm);` |
|         - | 6300 | `PH7_PRIVATE ph7_class * PH7_VmMethodScopeName(ph7_vm *pVm,ph7_class *pClass,ph7_class_method *pMeth);` |
|         - | 6301 | `PH7_PRIVATE int PH7_VmFccMethodIsDirect(ph7_vm *pVm,ph7_class *pClass,const char *zName,sxu32 nName);` |
|         - | 6302 | `PH7_PRIVATE ph7_class * PH7_VmClosureScopeClass(ph7_vm *pVm,ph7_class_instance *pClosure);` |
|         - | 6303 | `PH7_PRIVATE ph7_class * PH7_VmComposingClass(ph7_class *pClass,ph7_class *pDecl);` |
|         - | 6304 | `PH7_PRIVATE void PH7_ClassMethodRegisteredName(ph7_class *pClass,const char *zName,sxu32 nByte,SyString *pOut);` |
|         - | 6305 | `PH7_PRIVATE ph7_class * PH7_VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg);` |
|         - | 6306 | `PH7_PRIVATE int vm_builtin_get_class(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6307 | `PH7_PRIVATE int vm_builtin_get_parent_class(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6308 | `PH7_PRIVATE int vm_builtin_get_called_class(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6309 | `PH7_PRIVATE int vm_builtin_property_exists(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6310 | `PH7_PRIVATE int vm_builtin_method_exists(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6311 | `PH7_PRIVATE int vm_builtin_class_exists(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6312 | `PH7_PRIVATE int vm_builtin_interface_exists(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6313 | `PH7_PRIVATE int vm_builtin_trait_exists(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6314 | `PH7_PRIVATE int vm_builtin_class_alias(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6315 | `PH7_PRIVATE int vm_builtin_get_declared_classes(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6316 | `PH7_PRIVATE int vm_builtin_get_declared_interfaces(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6317 | `PH7_PRIVATE int vm_builtin_get_declared_traits(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6318 | `PH7_PRIVATE int vm_builtin_get_class_methods(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6319 | `PH7_PRIVATE int vm_builtin_class_parents(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6320 | `PH7_PRIVATE int vm_builtin_class_implements(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6321 | `PH7_PRIVATE int vm_builtin_class_uses(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6322 | `PH7_PRIVATE int vm_builtin_get_class_vars(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6323 | `PH7_PRIVATE int vm_builtin_get_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6324 | `PH7_PRIVATE int vm_builtin_get_mangled_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6325 | `PH7_PRIVATE int vm_builtin_is_a(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6326 | `PH7_PRIVATE int vm_builtin_clone(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6327 | `PH7_PRIVATE int vm_builtin_spl_object_id(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6328 | `PH7_PRIVATE int vm_builtin_spl_object_hash(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6329 | `PH7_PRIVATE int vm_builtin_is_subclass_of(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6330 | `PH7_PRIVATE int vm_builtin_call_user_func(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6331 | `PH7_PRIVATE int vm_builtin_call_user_func_array(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6332 | `/* vm_builtin_ob.c function prototypes */` |
|         - | 6333 | `PH7_PRIVATE int VmObConsumer(const void *pData,unsigned int nDataLen,void *pUserData);` |
|         - | 6334 | `PH7_PRIVATE void PH7_VmObFlushAll(ph7_vm *pVm);` |
|         - | 6335 | `PH7_PRIVATE int vm_builtin_ob_clean(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6336 | `PH7_PRIVATE int vm_builtin_ob_end_clean(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6337 | `PH7_PRIVATE int vm_builtin_ob_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6338 | `PH7_PRIVATE int vm_builtin_ob_get_clean(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6339 | `PH7_PRIVATE int vm_builtin_flush(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6340 | `PH7_PRIVATE int vm_builtin_ob_get_flush(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6341 | `PH7_PRIVATE int vm_builtin_ob_get_status(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6342 | `PH7_PRIVATE int vm_builtin_ob_get_length(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6343 | `PH7_PRIVATE int vm_builtin_ob_get_level(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6344 | `PH7_PRIVATE int vm_builtin_ob_start(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6345 | `PH7_PRIVATE int vm_builtin_ob_flush(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6346 | `PH7_PRIVATE int vm_builtin_ob_end_flush(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6347 | `PH7_PRIVATE int vm_builtin_ob_implicit_flush(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6348 | `PH7_PRIVATE int vm_builtin_ob_list_handlers(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6349 | `/* vm_builtin_getopt.c function prototypes */` |
|         - | 6350 | `PH7_PRIVATE int vm_builtin_getopt(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6351 | `/* vm_random.c function prototypes */` |
|         - | 6352 | `/* php's ext/random object surface: the Random\Engine contract, its errors,` |
|         - | 6353 | ` * the seeded engines and the Randomizer that consumes them. */` |
|         - | 6354 | `PH7_PRIVATE sxi32 PH7_VmInstallRandom(ph7_vm *pVm);` |
|         - | 6355 | `/* builtin_math.c function prototypes */` |
|         - | 6356 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|         - | 6357 | `PH7_PRIVATE int PH7_builtin_sqrt(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6358 | `PH7_PRIVATE int PH7_builtin_acosh(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6359 | `PH7_PRIVATE int PH7_builtin_asinh(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6360 | `PH7_PRIVATE int PH7_builtin_atanh(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6361 | `PH7_PRIVATE int PH7_builtin_expm1(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6362 | `PH7_PRIVATE int PH7_builtin_log1p(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6363 | `PH7_PRIVATE int PH7_builtin_deg2rad(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6364 | `PH7_PRIVATE int PH7_builtin_rad2deg(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6365 | `PH7_PRIVATE int PH7_builtin_fpow(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6366 | `PH7_PRIVATE int PH7_builtin_exp(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6367 | `PH7_PRIVATE int PH7_builtin_floor(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6368 | `PH7_PRIVATE int PH7_builtin_cos(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6369 | `PH7_PRIVATE int PH7_builtin_acos(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6370 | `PH7_PRIVATE int PH7_builtin_cosh(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6371 | `PH7_PRIVATE int PH7_builtin_sin(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6372 | `PH7_PRIVATE int PH7_builtin_asin(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6373 | `PH7_PRIVATE int PH7_builtin_sinh(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6374 | `PH7_PRIVATE int PH7_builtin_ceil(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6375 | `PH7_PRIVATE int PH7_builtin_tan(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6376 | `PH7_PRIVATE int PH7_builtin_atan(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6377 | `PH7_PRIVATE int PH7_builtin_tanh(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6378 | `PH7_PRIVATE int PH7_builtin_atan2(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6379 | `PH7_PRIVATE int PH7_builtin_abs(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6380 | `PH7_PRIVATE int PH7_builtin_log(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6381 | `PH7_PRIVATE int PH7_builtin_log10(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6382 | `PH7_PRIVATE int PH7_builtin_pow(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6383 | `PH7_PRIVATE int PH7_builtin_pi(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6384 | `PH7_PRIVATE int PH7_builtin_fmod(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6385 | `PH7_PRIVATE int PH7_builtin_hypot(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6386 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|         - | 6387 | `PH7_PRIVATE int PH7_builtin_round(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6388 | `/*` |
|         - | 6389 | ` * PHP's rounding modes (mirror ext/standard/php_math_round_mode.h). Only the` |
|         - | 6390 | ` * four HALF_* integers are exposed to userland (PHP_ROUND_HALF_UP..HALF_ODD,` |
|         - | 6391 | ` * see constant.c); the CEILING/FLOOR/TOWARD_ZERO/AWAY_FROM_ZERO modes (5..8)` |
|         - | 6392 | ` * have no userland constant but are reachable by passing the raw integer to` |
|         - | 6393 | ` * round()'s 3rd argument, which PHP 8.5 still accepts, so all eight are` |
|         - | 6394 | `` * honored. `enum RoundingMode` names all eight and numbers them DIFFERENTLY --`` |
|         - | 6395 | ` * PH7_RoundingModeCase() is the translation.` |
|         - | 6396 | ` */` |
|         - | 6397 | `#define PH7_ROUND_HALF_UP        1` |
|         - | 6398 | `#define PH7_ROUND_HALF_DOWN      2` |
|         - | 6399 | `#define PH7_ROUND_HALF_EVEN      3` |
|         - | 6400 | `#define PH7_ROUND_HALF_ODD       4` |
|         - | 6401 | `#define PH7_ROUND_CEILING        5` |
|         - | 6402 | `#define PH7_ROUND_FLOOR          6` |
|         - | 6403 | `#define PH7_ROUND_TOWARD_ZERO    7` |
|         - | 6404 | `#define PH7_ROUND_AWAY_FROM_ZERO 8` |
|         - | 6405 | ``/* php 8.4's `enum RoundingMode`, declared beside round() -- round() and`` |
|         - | 6406 | ` * bcround() are its two consumers. */` |
|         - | 6407 | `PH7_PRIVATE sxi32 PH7_VmInstallRoundingMode(ph7_vm *pVm);` |
|         - | 6408 | `PH7_PRIVATE int PH7_RoundingModeCase(ph7_value *pVal,int *pMode);` |
|         - | 6409 | `PH7_PRIVATE int PH7_builtin_intdiv(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6410 | `PH7_PRIVATE int PH7_builtin_number_format(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6411 | `PH7_PRIVATE int PH7_builtin_dechex(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6412 | `PH7_PRIVATE int PH7_builtin_decoct(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6413 | `PH7_PRIVATE int PH7_builtin_decbin(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6414 | `PH7_PRIVATE int PH7_builtin_hexdec(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6415 | `PH7_PRIVATE int PH7_builtin_bindec(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6416 | `PH7_PRIVATE int PH7_builtin_octdec(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6417 | `PH7_PRIVATE int PH7_builtin_srand(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6418 | `PH7_PRIVATE int PH7_builtin_base_convert(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6419 | `/* builtin_date.c function prototypes */` |
|         - | 6420 | `PH7_PRIVATE int PH7_builtin_time(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6421 | `PH7_PRIVATE int PH7_builtin_microtime(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6422 | `PH7_PRIVATE int PH7_builtin_hrtime(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6423 | `PH7_PRIVATE int PH7_builtin_getrusage(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6424 | `#ifdef PH7_ENABLE_PCRE` |
|         - | 6425 | `PH7_PRIVATE void PH7_PcreVersionInfo(char *zBuf,int nBuf,int *pMajor,int *pMinor,int *pJit);` |
|         - | 6426 | `#endif` |
|         - | 6427 | `PH7_PRIVATE int PH7_builtin_getdate(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6428 | `PH7_PRIVATE int PH7_builtin_gettimeofday(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6429 | `PH7_PRIVATE int PH7_builtin_date(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6430 | `PH7_PRIVATE int PH7_builtin_gmdate(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6431 | `PH7_PRIVATE int PH7_builtin_localtime(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6432 | `PH7_PRIVATE int PH7_builtin_idate(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6433 | `PH7_PRIVATE int PH7_builtin_mktime(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6434 | `PH7_PRIVATE int PH7_builtin_date_default_timezone_get(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6435 | `PH7_PRIVATE int PH7_builtin_date_default_timezone_set(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6436 | `PH7_PRIVATE int PH7_builtin_date_sun_info(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6437 | `PH7_PRIVATE int PH7_builtin_date_sunrise(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6438 | `PH7_PRIVATE int PH7_builtin_date_sunset(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6439 | `/* builtin_mb.c (UTF-8-only mb_* family) */` |
|         - | 6440 | ``/* Shared ZPP helper: resolve an `int`-typed parameter with php's full contract`` |
|         - | 6441 | ` * (null deprecation, lossy float / float-string deprecations, TypeErrors for` |
|         - | 6442 | ` * NAN/INF/non-numeric). Builtins with int params should use it instead of a` |
|         - | 6443 | ` * bare ph7_value_to_int64(), which coerces silently. */` |
|         - | 6444 | `PH7_PRIVATE sxi32 PH7_IntArgResolve(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,` |
|         - | 6445 | `	int iArgNum,const char *zParamName,const char *zTypeStr,sxi64 *pOut);` |
|         - | 6446 | `PH7_PRIVATE int PH7_builtin_mb_strlen_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6447 | `PH7_PRIVATE int PH7_builtin_mb_substr_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6448 | `PH7_PRIVATE int PH7_builtin_mb_case_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6449 | `PH7_PRIVATE int PH7_builtin_mb_convert_case_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6450 | `PH7_PRIVATE int PH7_builtin_mb_strpos_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6451 | `PH7_PRIVATE int PH7_builtin_mb_str_split_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6452 | `PH7_PRIVATE int PH7_builtin_mb_trim_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6453 | `PH7_PRIVATE int PH7_builtin_mb_internal_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6454 | `PH7_PRIVATE int PH7_builtin_mb_substitute_character_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6455 | `PH7_PRIVATE int PH7_builtin_mb_scrub_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6456 | `PH7_PRIVATE int PH7_builtin_mb_check_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6457 | `PH7_PRIVATE int PH7_builtin_mb_strwidth_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6458 | `PH7_PRIVATE int PH7_builtin_mb_chr_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6459 | `PH7_PRIVATE int PH7_builtin_mb_ord_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6460 | `PH7_PRIVATE int PH7_builtin_mb_ucfirst_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6461 | `PH7_PRIVATE int PH7_builtin_mb_strstr_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6462 | `PH7_PRIVATE int PH7_builtin_mb_substr_count_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6463 | `PH7_PRIVATE int PH7_builtin_mb_str_pad_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6464 | `PH7_PRIVATE int PH7_builtin_mb_strcut_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6465 | `PH7_PRIVATE int PH7_builtin_mb_strimwidth_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6466 | `PH7_PRIVATE int PH7_builtin_mb_detect_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6467 | `PH7_PRIVATE int PH7_builtin_mb_detect_order_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6468 | `PH7_PRIVATE int PH7_builtin_mb_list_encodings_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6469 | `PH7_PRIVATE int PH7_builtin_mb_encoding_aliases_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6470 | `PH7_PRIVATE int PH7_builtin_mb_convert_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6471 | `/* iconv (builtin_iconv.c) */` |
|         - | 6472 | `PH7_PRIVATE int PH7_builtin_iconv_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6473 | `PH7_PRIVATE int PH7_IconvTranslate(SyBlob *pOut,const char *zIn,int nIn,` |
|         - | 6474 | `	const char *zFrom,int nFrom,const char *zTo,int nTo);` |
|         - | 6475 | `PH7_PRIVATE int PH7_builtin_iconv_strlen_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6476 | `PH7_PRIVATE int PH7_builtin_iconv_substr_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6477 | `PH7_PRIVATE int PH7_builtin_iconv_strpos_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6478 | `PH7_PRIVATE int PH7_builtin_iconv_strrpos_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6479 | `PH7_PRIVATE int PH7_builtin_iconv_get_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6480 | `PH7_PRIVATE int PH7_builtin_iconv_mime_encode_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6481 | `PH7_PRIVATE int PH7_builtin_iconv_mime_decode_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6482 | `PH7_PRIVATE int PH7_builtin_iconv_mime_decode_headers_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6483 | `/* vm_builtin_spl.c */` |
|         - | 6484 | `PH7_PRIVATE sxi32 PH7_VmInstallSpl(ph7_vm *pVm);` |
|         - | 6485 | `/* vm_builtin_tokenizer.c */` |
|         - | 6486 | `PH7_PRIVATE sxi32 PH7_VmInstallTokenizer(ph7_vm *pVm);` |
|         - | 6487 | `PH7_PRIVATE void PH7_RegisterTokenizerConstants(ph7_vm *pVm);` |
|         - | 6488 | `/* vm_builtin_session.c */` |
|         - | 6489 | `PH7_PRIVATE sxi32 PH7_VmInstallSession(ph7_vm *pVm);` |
|         - | 6490 | `PH7_PRIVATE void PH7_VmSessionShutdown(ph7_vm *pVm);` |
|         - | 6491 | `/* vm_builtin_ini.c */` |
|         - | 6492 | `PH7_PRIVATE sxi32 PH7_VmInstallIni(ph7_vm *pVm);` |
|         - | 6493 | `PH7_PRIVATE sxi64 PH7_VmIniGetInt(ph7_vm *pVm,const char *zName,sxi64 iDefault);` |
|         - | 6494 | `PH7_PRIVATE int PH7_VmApplyMemoryLimit(ph7_vm *pVm,const char *zVal,sxu32 nVal);` |
|         - | 6495 | `PH7_PRIVATE void PH7_VmIniGetStr(ph7_vm *pVm,const char *zName,SyBlob *pOut);` |
|         - | 6496 | `PH7_PRIVATE int PH7_VmIniIsUnset(ph7_vm *pVm,const char *zName);` |
|         - | 6497 | `PH7_PRIVATE int PH7_VmIniDescribe(ph7_vm *pVm,const char *zName,sxu32 nName,` |
|         - | 6498 | `	sxi32 *piAccess,SyBlob *pOut,SyBlob *pDef);` |
|         - | 6499 | `PH7_PRIVATE int PH7_VmIniGetBool(ph7_vm *pVm,const char *zName,int bDefault);` |
|         - | 6500 | `PH7_PRIVATE int PH7_VmIniSet(ph7_vm *pVm,const char *zName,sxu32 nName,const char *zVal,sxu32 nVal,const char *zWho);` |
|         - | 6501 | `/* vfs_win.c / vfs_unix.c exported structs */` |
|         - | 6502 | `#ifdef __WINNT__` |
|         - | 6503 | `extern const ph7_vfs sWinVfs;` |
|         - | 6504 | `extern const ph7_io_stream sWinFileStream;` |
|         - | 6505 | `/* Would php's MapViewOfFile() copy of this plain file map a view of zero` |
|         - | 6506 | ` * requested bytes? stream_copy_to_stream() answers false then. */` |
|         - | 6507 | `PH7_PRIVATE int PH7_WinFileMapsEmptyView(void *pHandle,ph7_int64 nAhead);` |
|         - | 6508 | `/* The Win32 code (0: none) and php's text for the last failed opendir(). */` |
|         - | 6509 | `PH7_PRIVATE unsigned long PH7_WinOpenDirReason(char *zBuf,int nBuf);` |
|         - | 6510 | `#elif defined(__UNIXES__)` |
|         - | 6511 | `extern const ph7_vfs sUnixVfs;` |
|         - | 6512 | `extern const ph7_io_stream sUnixFileStream;` |
|         - | 6513 | `#endif` |
|         - | 6514 | `/* Built-in IO stream drivers: tcp:// lives in vfs_stream.c; php://, data://` |
|         - | 6515 | ` * and the pipe (popen) stream live in vfs_io_driver.c. Registration (vfs.c)` |
|         - | 6516 | ` * and the standard-stream exporters reference them across those files. */` |
|         - | 6517 | `extern const ph7_io_stream sTCP_Stream;` |
|         - | 6518 | `/* vfs_http.c -- what a script reads BACK from an http:// exchange. The store is` |
|         - | 6519 | ` * in every build: the two php 8.4 getters over it are ordinary builtins, and a` |
|         - | 6520 | ` * build with no network simply never records anything into it. */` |
|         - | 6521 | `PH7_PRIVATE void PH7_HttpPublishHeaders(ph7_vm *pVm,SyBlob *pLines);` |
|         - | 6522 | `PH7_PRIVATE ph7_value * PH7_HttpHeaderArray(ph7_vm *pVm,SyBlob *pLines);` |
|         - | 6523 | `PH7_PRIVATE void PH7_HttpFlushResponseHeaders(ph7_vm *pVm);` |
|         - | 6524 | `PH7_PRIVATE void PH7_HttpClearResponseHeaders(ph7_vm *pVm);` |
|         - | 6525 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 6526 | `PH7_PRIVATE void PH7_HttpInstallFuncs(ph7_vm *pVm);` |
|         - | 6527 | `#endif` |
|         - | 6528 | `#if !defined(PH7_DISABLE_DISK_IO) && defined(PH7_ENABLE_NET)` |
|         - | 6529 | `/* ... and the wrapper itself, which is where the store is filled. */` |
|         - | 6530 | `extern const ph7_io_stream sHTTP_Stream;` |
|         - | 6531 | `#ifdef PH7_ENABLE_OPENSSL` |
|         - | 6532 | `/* The same wrapper over the ssl:// transport, which is all php's https:// is:` |
|         - | 6533 | ` * a device of its own so the SCHEME a URL was opened under is known without` |
|         - | 6534 | ` * re-reading it, since only the name it was found under says so. */` |
|         - | 6535 | `extern const ph7_io_stream sHTTPS_Stream;` |
|         - | 6536 | `/* The ssl:// transport's client handshake, for a socket that is not a stream` |
|         - | 6537 | ` * handle: the http wrapper dials its own. */` |
|         - | 6538 | `struct phl_stream_ctx;` |
|         - | 6539 | `PH7_PRIVATE int PH7_SslClientHandshake(ph7_vm *pVm,ph7_socket sock,` |
|         - | 6540 | `	struct phl_stream_ctx *pCtxRes,const char *zPeerName,void **ppSsl,void **ppSslCtx,` |
|         - | 6541 | `	char *zErr,int nErr);` |
|         - | 6542 | `PH7_PRIVATE void PH7_SslDropSession(void **ppSsl,void **ppSslCtx);` |
|         - | 6543 | `#endif` |
|         - | 6544 | `PH7_PRIVATE int PH7_HttpStreamIs(const ph7_io_stream *pStream);` |
|         - | 6545 | `PH7_PRIVATE ph7_value * PH7_HttpStreamHeaderArray(ph7_vm *pVm,void *pHandle);` |
|         - | 6546 | `PH7_PRIVATE ph7_socket * PH7_HttpStreamSocket(void *pHandle);` |
|         - | 6547 | `PH7_PRIVATE sxu32 PH7_HttpStreamUnread(void *pHandle);` |
|         - | 6548 | `PH7_PRIVATE int PH7_HttpStreamAtEof(void *pHandle);` |
|         - | 6549 | `#endif /* !PH7_DISABLE_DISK_IO && PH7_ENABLE_NET */` |
|         - | 6550 | `extern const ph7_io_stream sDATA_Stream;` |
|         - | 6551 | `extern const ph7_io_stream sPHP_Stream;` |
|         - | 6552 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 6553 | `/* glob:// (vfs.c): a directory whose entries are a pattern's matches. It has no` |
|         - | 6554 | ` * xOpen at all, which is php's wrapper too. The three accessors are what SPL` |
|         - | 6555 | ` * asks of a directory handle that turns out to be this one: php's` |
|         - | 6556 | ` * php_stream_is(), php_glob_stream_get_path() and php_glob_stream_get_count(). */` |
|         - | 6557 | `extern const ph7_io_stream sGLOB_Stream;` |
|         - | 6558 | `PH7_PRIVATE int PH7_GlobStreamIs(const ph7_io_stream *pStream);` |
|         - | 6559 | `PH7_PRIVATE const char * PH7_GlobStreamPath(void *pHandle,int *pnLen);` |
|         - | 6560 | `PH7_PRIVATE sxi64 PH7_GlobStreamCount(void *pHandle);` |
|         - | 6561 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 6562 | `/* IO private state carried by every open stream handle (fopen/opendir/popen` |
|         - | 6563 | ` * resources and the exported std streams). Shared between vfs.c,` |
|         - | 6564 | ` * vfs_stream.c and vfs_io_driver.c. */` |
|         - | 6565 | `struct io_private` |
|         - | 6566 | `{` |
|         - | 6567 | `	/* THE FIRST WORD, and the only field a probe may read on a resource pointer` |
|         - | 6568 | ``	 * it has not identified yet. `iMagic` below sits ~240 bytes in, and a`` |
|         - | 6569 | `	 * resource that is not a stream can be much smaller than that -- a` |
|         - | 6570 | `	 * Generator's context is 184 bytes -- so reading the tail magic to ask "is` |
|         - | 6571 | `	 * this a stream?" reads past the end of the object. Offset zero is in bounds` |
|         - | 6572 | `	 * for every allocation there is. Written by InitIOPrivate and by nothing` |
|         - | 6573 | `	 * else, so it also says the layout below is real: the handles that are NOT` |
|         - | 6574 | `	 * streams but open with an io_private header (a context, a filter, a bucket` |
|         - | 6575 | `	 * brigade) build theirs by hand and leave this zero. */` |
|         - | 6576 | `	sxu32 iHead;` |
|         - | 6577 | `	const ph7_io_stream *pStream; /* Underlying IO device */` |
|         - | 6578 | `	void *pHandle; /* IO handle */` |
|         - | 6579 | `	/* Unbuffered IO */` |
|         - | 6580 | `	SyBlob sBuffer; /* Working buffer */` |
|         - | 6581 | `	sxu32 nOfft;    /* Current read offset */` |
|         - | 6582 | `	/* What the opener was ASKED for. php reports both back from` |
|         - | 6583 | ``	 * stream_get_meta_data() and neither was retained here, so the `uri` and`` |
|         - | 6584 | ``	 * `mode` keys of that array simply did not exist. sUri stays EMPTY for a`` |
|         - | 6585 | `	 * stream php opens without a wrapper (a popen() pipe), which is exactly` |
|         - | 6586 | `	 * when php omits the key. */` |
|         - | 6587 | `	SyBlob sUri;     /* the path/URI as the opener received it */` |
|         - | 6588 | `	char zMode[16];  /* the mode string, php's own field width */` |
|         - | 6589 | `	/* Per-handle settings the stream_set_* family writes. */` |
|         - | 6590 | `	sxu32 nChunk;    /* stream_set_chunk_size(), php's 8192 by default */` |
|         - | 6591 | `	sxu8 bNonBlock;  /* stream_set_blocking(false) took effect at the descriptor */` |
|         - | 6592 | `	sxu8 bHasTimeout;/* stream_set_timeout() armed one, so an EAGAIN read EXPIRED */` |
|         - | 6593 | ``	sxu8 bTimedOut;  /* the last read expired; php's meta `timed_out`, cleared by the next */`` |
|         - | 6594 | `	sxu8 bEof;       /* a read on this handle has already come back empty */` |
|         - | 6595 | `	int iLastReadErr;/* errno of the last device read that FAILED, latched for the` |
|         - | 6596 | `	                  * reader to announce (php's "Read of N bytes failed with` |
|         - | 6597 | `	                  * errno=..." notice) and cleared once it has. 0 = nothing` |
|         - | 6598 | `	                  * to report; a read that merely found EOF never sets it. */` |
|         - | 6599 | `	sxu8 bDir;       /* opendir()/dir() handle rather than a byte stream */` |
|         - | 6600 | `	/* Where the SCRIPT is on a device that cannot say it itself -- a popen()` |
|         - | 6601 | `	 * pipe, a socket, a directory handle. php's stream layer tracks a position` |
|         - | 6602 | `	 * for EVERY stream and only asks the device when it seeks, so ftell() on a` |
|         - | 6603 | `	 * pipe answers the bytes that have gone past rather than failing; this is` |
|         - | 6604 | `	 * that counter, and it is read only when the device has no xTell. A` |
|         - | 6605 | `	 * directory handle steps it by one php_stream_dirent per entry read` |
|         - | 6606 | `	 * (PHL_DIR_RECORD), which is the number php's own ftell() reports.` |
|         - | 6607 | `	 *` |
|         - | 6608 | `	 * It is the ONLY answer ftell() gives: php never asks the device again, and` |
|         - | 6609 | `	 * on a handle opened for APPEND the two numbers part company on the first` |
|         - | 6610 | `	 * write -- the descriptor jumps to the end of the file, php's counter moves` |
|         - | 6611 | `	 * by the bytes written. bPosSeeded records that the device has been asked` |
|         - | 6612 | `	 * once, which is php's own single lseek() at open time: it is 0 for a file` |
|         - | 6613 | `	 * and -1 for a descriptor that cannot say (a proc_open() pipe), and ftell()` |
|         - | 6614 | `	 * on the latter answers false until enough bytes have gone past. */` |
|         - | 6615 | `	ph7_int64 iPos;` |
|         - | 6616 | `	sxu8 bPosSeeded; /* the device was asked where it started */` |
|         - | 6617 | `	sxu8 bPersist;   /* opened PERSISTENTLY: get_resource_type() names it apart */` |
|         - | 6618 | `	/* The stream CONTEXT this handle carries (phl_stream_ctx*), owned by the VM` |
|         - | 6619 | `	 * registry. php attaches the opener's context to a TRANSPORT stream and to` |
|         - | 6620 | `	 * nothing else, and creates one on demand for a` |
|         - | 6621 | `	 * stream_context_set_option($stream,…). */` |
|         - | 6622 | `	void *pCtxRes;` |
|         - | 6623 | `	/* The two FILTER chains this handle carries (phl_stream_filter*, head first).` |
|         - | 6624 | `	 * php runs the read chain on what came off the device before the script sees` |
|         - | 6625 | `	 * it and the write chain on what the script wrote before the device does, so` |
|         - | 6626 | `	 * a filtered read cannot be served straight into the caller's buffer: a` |
|         - | 6627 | `	 * filter changes the byte COUNT. sFilt is where the chain's output waits. */` |
|         - | 6628 | `	void *pReadFilters;   /* phl_stream_filter* — read chain head, or 0 */` |
|         - | 6629 | `	void *pWriteFilters;  /* phl_stream_filter* — write chain head, or 0 */` |
|         - | 6630 | `	SyBlob sFilt;         /* filtered bytes not yet handed to a reader */` |
|         - | 6631 | `	sxu32 nFiltOfft;      /* read offset inside sFilt */` |
|         - | 6632 | `	sxu8 bFiltDone;       /* the read chain already had its CLOSING call */` |
|         - | 6633 | `	sxu8 bFiltErr;        /* a filter REFUSED: the next read answers false, once */` |
|         - | 6634 | `	ph7_int64 iFiltPos;   /* bytes the read CHAIN has delivered: php's position` |
|         - | 6635 | `	                       * for a filtered stream counts what came OUT, which` |
|         - | 6636 | `	                       * has nothing to do with the device's own offset */` |
|         - | 6637 | `	sxu32 iMagic;   /* Sanity check to avoid misuse */` |
|         - | 6638 | `	/* How many ph7_values name this handle. php's stream is refcounted and its` |
|         - | 6639 | ``	 * last holder closes it: `$h = fopen(...); $h = null;` releases the`` |
|         - | 6640 | `	 * descriptor there and used to leak it here, because a handle was a bare` |
|         - | 6641 | `	 * pointer nobody owned. Counted at the three value doors (Load, Store,` |
|         - | 6642 | `	 * Release) exactly the way a hashmap and an instance already are; a holder` |
|         - | 6643 | `	 * that is NOT a ph7_value -- the VM's own STDIN/STDOUT/STDERR, the` |
|         - | 6644 | `	 * persistent-socket registry, a native object's handle slot -- takes its own` |
|         - | 6645 | `	 * count so a script dropping its copy cannot close the handle underneath it.` |
|         - | 6646 | `	 * Only a live io_private (iMagic == IO_PRIVATE_MAGIC) carries one: every` |
|         - | 6647 | `	 * other resource kind shares this header's magic word and nothing else. */` |
|         - | 6648 | `	sxi32 nValRef;` |
|         - | 6649 | `};` |
|         - | 6650 | `#define IO_PRIVATE_MAGIC 0xFEAC14` |
|         - | 6651 | `/* io_private.iHead: "the bytes at this pointer really are an io_private". */` |
|         - | 6652 | `#define IO_PRIVATE_HEAD_MAGIC 0x10DEA5` |
|         - | 6653 | `/* proc_open()'s handle is an io_private with this magic in the same field, which` |
|         - | 6654 | `` * is what lets one probe tell the two apart — and what php names `process`. */`` |
|         - | 6655 | `#define PROC_PRIVATE_MAGIC 0x9C0DE5` |
|         - | 6656 | `/* A user-facing handle (fopen/opendir/popen) that has been fclose()'d/closedir()'d/` |
|         - | 6657 | ` * pclose()'d keeps its io_private alive but stamped with this magic, so every` |
|         - | 6658 | ` * ph7_value that still references it observes a closed resource` |
|         - | 6659 | ` * (gettype()=='resource (closed)', is_resource()==false), matching php. */` |
|         - | 6660 | `#define IO_PRIVATE_CLOSED_MAGIC 0xC105ED` |
|         - | 6661 | `/* stream_context_create()'s handle carries this magic in the same field, for the` |
|         - | 6662 | ` * same reason proc_open()'s does: php makes a context a RESOURCE, and the only` |
|         - | 6663 | ` * thing a resource probe can look at here is that word. */` |
|         - | 6664 | `#define STREAM_CTX_MAGIC 0xC07E47` |
|         - | 6665 | `/* Make sure we are dealing with a valid io_private instance */` |
|         - | 6666 | `#define IO_PRIVATE_INVALID(IO) ( IO == 0 \|\| IO->iMagic != IO_PRIVATE_MAGIC )` |
|         - | 6667 | `/*` |
|         - | 6668 | ` * One php stream CONTEXT: the wrapper => option => value map a script hands an` |
|         - | 6669 | ``  * opener, plus the `notification` parameter. php's is a `stream-context` `` |
|         - | 6670 | ` * resource, and a PHL resource is a bare void*, so the struct opens with an` |
|         - | 6671 | ` * io_private-compatible header (proc_open()'s handle does the same) — every` |
|         - | 6672 | ` * resource probe reads that magic and stays in bounds. The VM owns the chain` |
|         - | 6673 | ` * and drops it at reset, so a reused VM does not carry one request's default` |
|         - | 6674 | ` * context into the next.` |
|         - | 6675 | ` */` |
|         - | 6676 | `/*` |
|         - | 6677 | `` * php's STREAM_NOTIFY_* -- WHICH event a context's `notification` callback is`` |
|         - | 6678 | ` * being told about -- and the three severities beside them. RESOLVE, the two` |
|         - | 6679 | ` * AUTH codes and AUTH_RESULT are php's own numbers for events no wrapper here` |
|         - | 6680 | ` * raises; they are defined because a script SWITCHES on them, and a name that` |
|         - | 6681 | ` * is missing is a fatal where an unreachable one is simply never matched.` |
|         - | 6682 | ` */` |
|         - | 6683 | `#define PHL_STREAM_NOTIFY_RESOLVE       1` |
|         - | 6684 | `#define PHL_STREAM_NOTIFY_CONNECT       2` |
|         - | 6685 | `#define PHL_STREAM_NOTIFY_AUTH_REQUIRED 3` |
|         - | 6686 | `#define PHL_STREAM_NOTIFY_MIME_TYPE_IS  4` |
|         - | 6687 | `#define PHL_STREAM_NOTIFY_FILE_SIZE_IS  5` |
|         - | 6688 | `#define PHL_STREAM_NOTIFY_REDIRECTED    6` |
|         - | 6689 | `#define PHL_STREAM_NOTIFY_PROGRESS      7` |
|         - | 6690 | `#define PHL_STREAM_NOTIFY_COMPLETED     8` |
|         - | 6691 | `#define PHL_STREAM_NOTIFY_FAILURE       9` |
|         - | 6692 | `#define PHL_STREAM_NOTIFY_AUTH_RESULT   10` |
|         - | 6693 | `#define PHL_STREAM_NOTIFY_SEVERITY_INFO 0` |
|         - | 6694 | `#define PHL_STREAM_NOTIFY_SEVERITY_WARN 1` |
|         - | 6695 | `#define PHL_STREAM_NOTIFY_SEVERITY_ERR  2` |
|         - | 6696 | `typedef struct phl_stream_ctx phl_stream_ctx;` |
|         - | 6697 | `struct phl_stream_ctx` |
|         - | 6698 | `{` |
|         - | 6699 | `	io_private base;        /* io_private-compatible header (base.iMagic == STREAM_CTX_MAGIC) */` |
|         - | 6700 | `	ph7_vm *pVm;            /* owning VM */` |
|         - | 6701 | `	ph7_value *pOptions;    /* the wrapper => (option => value) map; never 0 */` |
|         - | 6702 | ``	ph7_value *pNotify;     /* the `notification` param, or 0 when none was set */`` |
|         - | 6703 | `	/* php's notifier carries the PROGRESS counter on the CONTEXT rather than on` |
|         - | 6704 | `	 * the stream, and never disarms it: a context reused for a second exchange` |
|         - | 6705 | `	 * reports that exchange's request write and header read under the FIRST` |
|         - | 6706 | `	 * one's running total, until the wrapper's own progress_init resets it.` |
|         - | 6707 | `	 * That is visible from a script, so it is modelled rather than approximated. */` |
|         - | 6708 | `	sxi64 iProgress;        /* bytes counted since the last init */` |
|         - | 6709 | `	sxi64 iProgressMax;     /* what the wrapper announced, or 0 for "unknown" */` |
|         - | 6710 | `	int bProgress;          /* has an init armed the counter yet? */` |
|         - | 6711 | `	int bNotifyDead;        /* the callback threw: php stops calling it */` |
|         - | 6712 | `	/* Registry chain (pVm->pStreamCtx), DOUBLY linked: a context whose last` |
|         - | 6713 | `	 * holder goes away is freed there and then, the way php's refcounted` |
|         - | 6714 | ``	 * `stream-context` resource is, so unlinking one may not walk the chain.`` |
|         - | 6715 | `	 * base.nValRef counts the holders -- every ph7_value naming it through the` |
|         - | 6716 | `	 * three value doors, plus one for each NON-value holder (the VM's default` |
|         - | 6717 | `	 * context, a stream that carries one in io_private.pCtxRes). */` |
|         - | 6718 | `	phl_stream_ctx *pNext;` |
|         - | 6719 | `	phl_stream_ctx *pPrev;` |
|         - | 6720 | `};` |
|         - | 6721 | `/* The context behind a ph7_value, or 0 when the value is not one. */` |
|         - | 6722 | `PH7_PRIVATE phl_stream_ctx * PH7_StreamCtxFromValue(ph7_value *pVal);` |
|         - | 6723 | `/* The per-VM DEFAULT context (stream_context_get_default), created on demand. */` |
|         - | 6724 | `PH7_PRIVATE phl_stream_ctx * PH7_StreamCtxDefault(ph7_vm *pVm);` |
|         - | 6725 | `/* One wrapper option, or 0 when the context does not carry it. */` |
|         - | 6726 | `PH7_PRIVATE ph7_value * PH7_StreamCtxOption(phl_stream_ctx *pCtxRes,const char *zWrapper,const char *zOption);` |
|         - | 6727 | `/* Drop every context this VM created (called from PH7_VmReset). */` |
|         - | 6728 | `PH7_PRIVATE void PH7_StreamCtxVmReset(ph7_vm *pVm);` |
|         - | 6729 | ``/* The `$context` argument of an opener, php's rules applied (see the body). */`` |
|         - | 6730 | `PH7_PRIVATE phl_stream_ctx * PH7_StreamCtxFromArg(ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|         - | 6731 | `	int iArg,const char *zArgName,int bNoDefault,int *pbThrew);` |
|         - | 6732 | `/* Arm the context PH7_StreamOpenHandle's next open runs under. */` |
|         - | 6733 | `PH7_PRIVATE void PH7_StreamCtxArm(ph7_vm *pVm,phl_stream_ctx *pRes);` |
|         - | 6734 | ``/* Tell the context's `notification` callback about one event. A NULL zMsg is`` |
|         - | 6735 | ` * php's null third argument; nMsg < 0 means "NUL-terminated". */` |
|         - | 6736 | `PH7_PRIVATE void PH7_StreamCtxNotify(phl_stream_ctx *pCtxRes,int iCode,int iSeverity,` |
|         - | 6737 | `	const char *zMsg,int nMsg,int iMsgCode,sxi64 iBytes,sxi64 iBytesMax);` |
|         - | 6738 | `/* php's progress notifier: arm the counter at 0 with a known maximum (and say` |
|         - | 6739 | ` * so), add to it, or report the end of the transfer. */` |
|         - | 6740 | `PH7_PRIVATE void PH7_StreamCtxProgressInit(phl_stream_ctx *pCtxRes,sxi64 iMax);` |
|         - | 6741 | `PH7_PRIVATE void PH7_StreamCtxProgressAdd(phl_stream_ctx *pCtxRes,sxi64 nDelta);` |
|         - | 6742 | `PH7_PRIVATE void PH7_StreamCtxCompleted(phl_stream_ctx *pCtxRes);` |
|         - | 6743 | `/*` |
|         - | 6744 | ` * ---------------------------------------------------------------------------` |
|         - | 6745 | ` * Stream filters.` |
|         - | 6746 | ` *` |
|         - | 6747 | ` * php runs a stream's bytes through a CHAIN on the way in and another on the` |
|         - | 6748 | ` * way out. A filter is handed a BRIGADE — the buckets that came off the device,` |
|         - | 6749 | ` * or that the script wrote — and appends what it made to a second one; what it` |
|         - | 6750 | ` * ANSWERS says whether that output may go on (PASS_ON), whether it needs more` |
|         - | 6751 | ` * input before it can produce any (FEED_ME), or whether the stream is finished` |
|         - | 6752 | ` * (ERR_FATAL). The brigade rather than one string is what lets a filter split` |
|         - | 6753 | ` * or merge its input, and what a userland filter walks with` |
|         - | 6754 | ` * stream_bucket_make_writeable().` |
|         - | 6755 | ` * ---------------------------------------------------------------------------` |
|         - | 6756 | ` */` |
|         - | 6757 | `/* php's PSFS_* filter results. */` |
|         - | 6758 | `#define PHL_PSFS_ERR_FATAL 0` |
|         - | 6759 | `#define PHL_PSFS_FEED_ME   1` |
|         - | 6760 | `#define PHL_PSFS_PASS_ON   2` |
|         - | 6761 | `/* php's PSFS_FLAG_* — which kind of call this is. FLUSH_CLOSE is the last one a` |
|         - | 6762 | ` * filter ever gets and the only chance a buffering filter has to emit its tail. */` |
|         - | 6763 | `#define PHL_PSFS_FLAG_NORMAL      0` |
|         - | 6764 | `#define PHL_PSFS_FLAG_FLUSH_INC   1` |
|         - | 6765 | `#define PHL_PSFS_FLAG_FLUSH_CLOSE 2` |
|         - | 6766 | `/* php's STREAM_FILTER_* chain selectors. */` |
|         - | 6767 | `#define PHL_STREAM_FILTER_READ  1` |
|         - | 6768 | `#define PHL_STREAM_FILTER_WRITE 2` |
|         - | 6769 | `#define PHL_STREAM_FILTER_ALL   3` |
|         - | 6770 | `/* stream_filter_append()'s handle carries this magic in the io_private-compatible` |
|         - | 6771 | ` * header every PHL resource opens with; php names the resource "stream filter". */` |
|         - | 6772 | `#define STREAM_FILTER_MAGIC 0xF117E4` |
|         - | 6773 | `/* A filter that has been removed from its chain. A stream keeps its own closed` |
|         - | 6774 | ` * magic because the struct outlives the close; a filter needs one of its own so` |
|         - | 6775 | ` * the header can still say WHICH kind it is after the fact -- that is what lets` |
|         - | 6776 | ` * the last ph7_value naming it hand the memory back instead of parking it on` |
|         - | 6777 | ` * the VM registry until reset. Every probe that reports a closed resource` |
|         - | 6778 | ` * treats it exactly like IO_PRIVATE_CLOSED_MAGIC. */` |
|         - | 6779 | `#define STREAM_FILTER_CLOSED_MAGIC 0xF11DEA` |
|         - | 6780 | `typedef struct phl_bucket phl_bucket;` |
|         - | 6781 | `typedef struct phl_brigade phl_brigade;` |
|         - | 6782 | `typedef struct phl_stream_filter phl_stream_filter;` |
|         - | 6783 | `typedef struct phl_filter_ops phl_filter_ops;` |
|         - | 6784 | `/* stream_filter_register()'s two script-visible handles. php names them` |
|         - | 6785 | `` * `userfilter.bucket brigade` and `userfilter.bucket`. */`` |
|         - | 6786 | `#define STREAM_BRIGADE_MAGIC 0xB817AD` |
|         - | 6787 | `#define STREAM_BUCKET_MAGIC  0xB0C4E7` |
|         - | 6788 | `/* One bucket: a run of bytes travelling through a chain. */` |
|         - | 6789 | `struct phl_bucket` |
|         - | 6790 | `{` |
|         - | 6791 | `	SyBlob sData;      /* the bytes */` |
|         - | 6792 | `	phl_bucket *pNext; /* next bucket in the brigade */` |
|         - | 6793 | `};` |
|         - | 6794 | `struct phl_brigade` |
|         - | 6795 | `{` |
|         - | 6796 | `	phl_bucket *pHead,*pTail;` |
|         - | 6797 | `};` |
|         - | 6798 | ``/* The brigade a userland filter() is handed. Both `$in` and `$out` are one of`` |
|         - | 6799 | ` * these. They belong to the FILTER rather than to the call, so a script that` |
|         - | 6800 | ` * held one past the call it was handed in still has something in bounds to look` |
|         - | 6801 | ` * at — what it loses is the brigade behind it, which is cleared on the way out` |
|         - | 6802 | ` * and makes a stale handle answer "empty". */` |
|         - | 6803 | `typedef struct phl_brigade_res phl_brigade_res;` |
|         - | 6804 | `struct phl_brigade_res` |
|         - | 6805 | `{` |
|         - | 6806 | `	io_private base;      /* resource header (base.iMagic == STREAM_BRIGADE_MAGIC) */` |
|         - | 6807 | `	ph7_vm *pVm;` |
|         - | 6808 | `	phl_brigade *pBrig;   /* the brigade it stands for, 0 between calls */` |
|         - | 6809 | `	phl_stream_filter *pOwner; /* the filter these bytes live inside */` |
|         - | 6810 | `};` |
|         - | 6811 | `/* What a built-in filter IS. A userland filter has no ops and runs its class. */` |
|         - | 6812 | `struct phl_filter_ops` |
|         - | 6813 | `{` |
|         - | 6814 | `	const char *zName;  /* php's own registered name */` |
|         - | 6815 | `	/* Read the $params argument, once, when the filter is created. A non-zero` |
|         - | 6816 | `	 * answer is php's "filter refused to be created". */` |
|         - | 6817 | `	int (*xCreate)(phl_stream_filter *pFilter,ph7_value *pParams);` |
|         - | 6818 | `	int (*xFilter)(phl_stream_filter *pFilter,phl_brigade *pIn,phl_brigade *pOut,int iFlags);` |
|         - | 6819 | `	void (*xClose)(phl_stream_filter *pFilter);` |
|         - | 6820 | `};` |
|         - | 6821 | `struct phl_stream_filter` |
|         - | 6822 | `{` |
|         - | 6823 | `	io_private base;            /* resource header (base.iMagic == STREAM_FILTER_MAGIC) */` |
|         - | 6824 | `	ph7_vm *pVm;                /* owning VM */` |
|         - | 6825 | `	const phl_filter_ops *pOps; /* built-in behaviour, or 0 for a userland filter */` |
|         - | 6826 | `	SyBlob sName;               /* the name it was CREATED under (a wildcard match keeps the request) */` |
|         - | 6827 | `	SyBlob sCarry;              /* bytes the filter could not encode yet (base64/qp/dechunk) */` |
|         - | 6828 | `	int iState;                 /* per-filter scalar state */` |
|         - | 6829 | `	sxu8 bClosed;               /* the FLUSH_CLOSE call has already been made */` |
|         - | 6830 | `	sxu8 bDead;                 /* it answered ERR_FATAL: the chain is finished */` |
|         - | 6831 | `	int iChain;                 /* PHL_STREAM_FILTER_READ or _WRITE */` |
|         - | 6832 | `	io_private *pDev;           /* the handle it is attached to; 0 once removed */` |
|         - | 6833 | `	phl_stream_filter *pNext;   /* next filter in that chain */` |
|         - | 6834 | `	phl_stream_filter *pRegNext;/* VM registry chain (pVm->pStreamFilter) */` |
|         - | 6835 | `	void *pPriv;                /* per-filter private state, freed by xClose */` |
|         - | 6836 | `	phl_brigade_res sIn,sOut;   /* the two handles filter() is given */` |
|         - | 6837 | `	void *pObj;                 /* userland filter instance (ph7_class_instance*) */` |
|         - | 6838 | `	ph7_value *pStreamRes;      /* the $stream the userland filter's property answers */` |
|         - | 6839 | `};` |
|         - | 6840 | `/* Brigade plumbing, shared with the userland-filter half. */` |
|         - | 6841 | `PH7_PRIVATE phl_bucket * PH7_FilterBucketNew(ph7_vm *pVm,const void *pData,sxu32 nLen);` |
|         - | 6842 | `PH7_PRIVATE phl_bucket * PH7_FilterBucketPop(phl_brigade *pBrig);` |
|         - | 6843 | `PH7_PRIVATE void PH7_FilterBucketAppend(phl_brigade *pBrig,phl_bucket *pBucket);` |
|         - | 6844 | `PH7_PRIVATE void PH7_FilterBucketFree(ph7_vm *pVm,phl_bucket *pBucket);` |
|         - | 6845 | `PH7_PRIVATE void PH7_FilterBrigadeRelease(ph7_vm *pVm,phl_brigade *pBrig);` |
|         - | 6846 | `/* Run one chain over nLen bytes, appending what came out to pOut. Answers a` |
|         - | 6847 | ` * PHL_PSFS_* code; ERR_FATAL means the stream is finished. */` |
|         - | 6848 | `PH7_PRIVATE int PH7_FilterChainProcess(phl_stream_filter *pHead,` |
|         - | 6849 | `	const void *pData,sxu32 nLen,int iFlags,int iRestFlags,SyBlob *pOut,int *pbUnread);` |
|         - | 6850 | `/* A seek moved the device: a chain that had already been CLOSED at the old end` |
|         - | 6851 | ` * of file has to be able to run again. */` |
|         - | 6852 | `PH7_PRIVATE void PH7_StreamFilterRewound(io_private *pDev);` |
|         - | 6853 | `/* Drop both chains of a handle, flushing the write one while the device is` |
|         - | 6854 | ` * still open (every close path and the io_private reset paths). */` |
|         - | 6855 | `PH7_PRIVATE void PH7_StreamFilterReleaseChains(io_private *pDev);` |
|         - | 6856 | `/* Attach a filter by NAME, php's own failure diagnostics raised from pCtx.` |
|         - | 6857 | ` * Answers the filter, or 0 when there is no such name. */` |
|         - | 6858 | `PH7_PRIVATE phl_stream_filter * PH7_StreamFilterAttach(ph7_vm *pVm,io_private *pDev,` |
|         - | 6859 | `	const char *zName,int nName,int iChain,int bPrepend,ph7_value *pParams,` |
|         - | 6860 | `	ph7_value *pStreamVal);` |
|         - | 6861 | `/* The filter behind a ph7_value, or 0 when the value is not a live one. */` |
|         - | 6862 | `PH7_PRIVATE phl_stream_filter * PH7_StreamFilterFromValue(ph7_value *pVal);` |
|         - | 6863 | `/* Drop every filter this VM created (called from PH7_VmReset). */` |
|         - | 6864 | `PH7_PRIVATE void PH7_StreamFilterVmReset(ph7_vm *pVm);` |
|         - | 6865 | `/* The last ph7_value naming a filter handle -- or one of the two brigade handles` |
|         - | 6866 | ` * that live inside it -- has gone away. Hands the memory back when the chain has` |
|         - | 6867 | ` * let go of it too. */` |
|         - | 6868 | `PH7_PRIVATE void PH7_StreamFilterValueGone(void *pResource);` |
|         - | 6869 | `/* The stream_filter_register()/php_user_filter half. */` |
|         - | 6870 | `PH7_PRIVATE int PH7_builtin_stream_filter_register(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6871 | `PH7_PRIVATE int PH7_builtin_stream_bucket_make_writeable(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6872 | `PH7_PRIVATE int PH7_builtin_stream_bucket_append(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6873 | `PH7_PRIVATE int PH7_builtin_stream_bucket_prepend(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6874 | `PH7_PRIVATE int PH7_builtin_stream_bucket_new(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6875 | `PH7_PRIVATE sxi32 PH7_VmInstallStreamFilter(ph7_vm *pVm);` |
|         - | 6876 | `/* Attach the filters a php://filter URL names to the handle it wrapped. */` |
|         - | 6877 | `PH7_PRIVATE int PH7_StreamFilterParseUrl(ph7_vm *pVm,const char *zSpec,int nSpec,` |
|         - | 6878 | `	io_private *pDev,int iChains);` |
|         - | 6879 | ``/* php's `$stream` screen: a TypeError for a non-resource and for a closed one. */`` |
|         - | 6880 | `PH7_PRIVATE io_private * PH7_StreamHandleArg(ph7_context *pCtx,ph7_value *pArg,int iPos,` |
|         - | 6881 | `	const char *zName,int *pRc);` |
|         - | 6882 | `PH7_PRIVATE void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut);` |
|         - | 6883 | `PH7_PRIVATE void SetIOPrivateOpenedAs(io_private *pDev,const char *zUri,int nUriLen,const char *zMode,int nModeLen);` |
|         - | 6884 | `PH7_PRIVATE const char * PH7_StreamWrapperLabel(ph7_vm *pVm,const ph7_io_stream *pStream);` |
|         - | 6885 | `PH7_PRIVATE void MarkIOPrivateClosed(io_private *pDev);` |
|         - | 6886 | `PH7_PRIVATE void PH7_StreamReleaseUnopened(ph7_context *pCtx,io_private *pDev);` |
|         - | 6887 | `/* One buffered line off a handle, php's php_stream_get_line: the newline is` |
|         - | 6888 | ` * INCLUDED, nMaxLen (0 = no cap) bounds the bytes handed back and the remainder` |
|         - | 6889 | ` * stays buffered. The pointer is into the handle's own working buffer and the` |
|         - | 6890 | ` * next read invalidates it. */` |
|         - | 6891 | `PH7_PRIVATE ph7_int64 StreamReadLine(io_private *pDev,const char **pzData,ph7_int64 nMaxLen);` |
|         - | 6892 | `/* php's feof(): the end flag, but only once the line readers' look-ahead buffer` |
|         - | 6893 | ` * has been drained -- buffered bytes are not an end, and a userland wrapper is` |
|         - | 6894 | ` * asked the question itself. */` |
|         - | 6895 | `PH7_PRIVATE int PH7_StreamAtEof(io_private *pDev);` |
|         - | 6896 | ``/* php's `Read of N bytes failed with errno=...` notice, raised from whichever`` |
|         - | 6897 | ` * builtin or METHOD is asking. A no-op unless the last device read failed. */` |
|         - | 6898 | `PH7_PRIVATE void StreamReportReadFailure(ph7_context *pCtx,io_private *pDev);` |
|         - | 6899 | `/* Why PH7_StreamOpenPath() answered 0. It reports nothing itself: fopen() warns` |
|         - | 6900 | ` * and SplFileObject's constructor throws, which is php's own split. */` |
|         - | 6901 | `#define PH7_STREAM_OPEN_OK       0` |
|         - | 6902 | `#define PH7_STREAM_OPEN_NODEVICE 1 /* no wrapper is registered for the scheme */` |
|         - | 6903 | `#define PH7_STREAM_OPEN_FAILED   2 /* the wrapper refused the name (errno is set) */` |
|         - | 6904 | `#define PH7_STREAM_OPEN_NOMEM    3` |
|         - | 6905 | `#define PH7_STREAM_OPEN_BADMODE  4 /* the plain-file wrapper refused the MODE */` |
|         - | 6906 | `PH7_PRIVATE io_private * PH7_StreamOpenPath(ph7_context *pCtx,ph7_value *pPath,` |
|         - | 6907 | `	const char *zMode,int nMode,int bUseInclude,phl_stream_ctx *pCtxRes,` |
|         - | 6908 | `	ph7_value *pCtxArg,int *piErr,const char **pzErrUri);` |
|         - | 6909 | `/* "Failed to open stream" warning helper (vfs.c, errno-based); used by the` |
|         - | 6910 | ` * fopen/opendir/file_* family in vfs_stream.c. */` |
|         - | 6911 | `PH7_PRIVATE void VfsThrowOpenWarning(ph7_context *pCtx,const char *zFile);` |
|         - | 6912 | `/* strerror() for a failed stream OPEN: php's own substitution of ENOENT for` |
|         - | 6913 | ` * ENOTDIR, which belongs to the open and to no other file operation. */` |
|         - | 6914 | `PH7_PRIVATE const char * PH7_VfsOpenStrerror(int iErr);` |
|         - | 6915 | `PH7_PRIVATE void VfsThrowUnknownWrapperWarning(ph7_context *pCtx,const char *zUri);` |
|         - | 6916 | `PH7_PRIVATE const ph7_io_stream * PH7_VfsStreamDeviceOrFile(ph7_context *pCtx,const char **pzUri,int nByte);` |
|         - | 6917 | `PH7_PRIVATE int PH7_VfsEmptyPathRefused(ph7_context *pCtx,int nPath);` |
|         - | 6918 | `/* The separator php's own path expansion writes on this platform. */` |
|         - | 6919 | `#ifdef __WINNT__` |
|         - | 6920 | `#define PH7_PATH_SEP      '\\'` |
|         - | 6921 | `#define PH7_PATH_SEP_STR  "\\"` |
|         - | 6922 | `#else` |
|         - | 6923 | `#define PH7_PATH_SEP      '/'` |
|         - | 6924 | `#define PH7_PATH_SEP_STR  "/"` |
|         - | 6925 | `#endif` |
|         - | 6926 | `PH7_PRIVATE int PH7_VfsPathIsAbsolute(const char *zPath,int nPath);` |
|         - | 6927 | `PH7_PRIVATE void PH7_VfsExpandPath(ph7_context *pCtx,const char *zPath,int nPath,SyBlob *pOut);` |
|         - | 6928 | `/*` |
|         - | 6929 | ` * One name a directory walk produced, as an OFFSET into the blob that holds` |
|         - | 6930 | ` * them back to back -- the blob grows as the walk does, so a pointer would not` |
|         - | 6931 | ` * survive the next append. It is the shape glob:// keeps its matches in and the` |
|         - | 6932 | ` * shape ext/zip's addGlob()/addPattern() read them back out of.` |
|         - | 6933 | ` */` |
|         - | 6934 | `typedef struct PH7_GlobHit PH7_GlobHit;` |
|         - | 6935 | `struct PH7_GlobHit` |
|         - | 6936 | `{` |
|         - | 6937 | `	sxu32 nOfs;` |
|         - | 6938 | `	sxu32 nLen;` |
|         - | 6939 | `};` |
|         - | 6940 | ``/* Every entry of ONE directory, `.` and `..` included and sorted by bytes --`` |
|         - | 6941 | `` * php's `php_stream_scandir` with its alphasort comparator, which is what`` |
|         - | 6942 | ` * ZipArchive::addPattern() walks. */` |
|         - | 6943 | `PH7_PRIVATE sxi32 PH7_VfsListDir(ph7_vm *pVm,const char *zDir,int nDir,SyBlob *pHit,SySet *pSet);` |
|         - | 6944 | `/* A wrapper's own reason for refusing the open in flight, which is what php` |
|         - | 6945 | ` * prints after "Failed to open stream:" instead of an errno. Set from an xOpen` |
|         - | 6946 | ` * body; PH7_StreamOpenHandle() re-arms the default before every open. */` |
|         - | 6947 | `PH7_PRIVATE void PH7_StreamSetOpenError(ph7_vm *pVm,const char *zReason);` |
|         - | 6948 | ``/* The same, for php's `"Cls::method" call failed`: a userland wrapper's refusal`` |
|         - | 6949 | ` * names the call that made it, so the sentence is built and copied. */` |
|         - | 6950 | `PH7_PRIVATE void PH7_StreamSetOpenErrorCall(ph7_vm *pVm,const char *zClass,const char *zMethod);` |
|         - | 6951 | `PH7_PRIVATE void VfsThrowNoDeviceWarning(ph7_context *pCtx,const char *zUri,int bDir);` |
|         - | 6952 | `PH7_PRIVATE int PH7_VfsStatDoubleUp(ph7_value *pIn,ph7_value *pOut);` |
|         - | 6953 | `PH7_PRIVATE int PH7_VfsStatFill(ph7_value *pArray,ph7_value *pWorker,const ph7_int64 *aVal);` |
|         - | 6954 | `PH7_PRIVATE int PH7_VfsStatFromFd(int iFd,ph7_value *pArray,ph7_value *pWorker);` |
|         - | 6955 | `PH7_PRIVATE const char * VfsStrerror(int iErr);` |
|         - | 6956 | `/* Stream-device predicates (vfs_io_driver.c) */` |
|         - | 6957 | `PH7_PRIVATE int is_php_stream(const ph7_io_stream *pStream);` |
|         - | 6958 | `PH7_PRIVATE int is_data_stream(const ph7_io_stream *pStream);` |
|         - | 6959 | `/* Which php:// sub-stream a handle opened (PH7_IO_STREAM_*, 0 when unknown).` |
|         - | 6960 | ` * stream_get_meta_data() names MEMORY, TEMP and STDIO apart, and the device` |
|         - | 6961 | ` * itself is the only place that knows. */` |
|         - | 6962 | `PH7_PRIVATE int PH7_PhpStreamKind(void *pHandle);` |
|         - | 6963 | `/* Has a php://temp handle handed out its last byte? php's temp stream copies its` |
|         - | 6964 | ` * inner memory stream's eof, so it reports the end one read EARLIER than a bare` |
|         - | 6965 | ` * php://memory; 0 for every other device. */` |
|         - | 6966 | `PH7_PRIVATE int PH7_PhpStreamTempDrained(void *pHandle);` |
|         - | 6967 | `/* The handle a php://filter proxy wraps, or 0 for any other php:// stream. */` |
|         - | 6968 | `PH7_PRIVATE io_private * PH7_PhpStreamInner(void *pHandle);` |
|         - | 6969 | `/* That handle, or pDev itself when it is not a proxy. */` |
|         - | 6970 | `PH7_PRIVATE io_private * PH7_StreamUnwrap(io_private *pDev);` |
|         - | 6971 | `/* Where the SCRIPT is on a handle: the device position less what was read ahead. */` |
|         - | 6972 | `PH7_PRIVATE ph7_int64 PH7_StreamLogicalTell(io_private *pDev);` |
|         - | 6973 | `/* Seek the stream a php://filter proxy wraps, in the same model. */` |
|         - | 6974 | `PH7_PRIVATE int PH7_StreamSeekWrapped(io_private *pDev,ph7_int64 iOfft,int whence);` |
|         - | 6975 | `/* The POSIX descriptor behind an open handle, or -1 for a device that has none` |
|         - | 6976 | ` * (a memory buffer, a data:// payload, a userland wrapper) and on Windows,` |
|         - | 6977 | ` * where the file devices carry a HANDLE instead. Only the settings php applies` |
|         - | 6978 | ` * AT the descriptor need it. */` |
|         - | 6979 | `PH7_PRIVATE int PH7_StreamPosixFd(io_private *pDev);` |
|         - | 6980 | `/* Is this a handle php's plain-files device would own: a file, a pipe, a` |
|         - | 6981 | ` * standard stream? */` |
|         - | 6982 | `PH7_PRIVATE int PH7_StreamIsPlainDevice(io_private *pDev);` |
|         - | 6983 | ``/* The `stream_type` label a handle reports (STDIO / MEMORY / TEMP / Input /`` |
|         - | 6984 | ` * RFC2397 / dir / user-space / a transport's), which is what ext/posix names in` |
|         - | 6985 | ` * php's "Could not use stream of type '%s'". */` |
|         - | 6986 | `PH7_PRIVATE const char * PH7_StreamTypeLabel(io_private *pDev);` |
|         - | 6987 | `/* 1 / 0 / -1 ("ask the device instead"): can this handle report a position?` |
|         - | 6988 | `` * php's stream_get_meta_data() `seekable` is exactly this question. */`` |
|         - | 6989 | `PH7_PRIVATE int PH7_StreamHandleCanSeek(io_private *pDev);` |
|         - | 6990 | `/* The php:// sub-streams, as PH7_PhpStreamKind() reports them. */` |
|         - | 6991 | `#define PH7_IO_STREAM_STDIN  1 /* php://stdin */` |
|         - | 6992 | `#define PH7_IO_STREAM_STDOUT 2 /* php://stdout */` |
|         - | 6993 | `#define PH7_IO_STREAM_STDERR 3 /* php://stderr */` |
|         - | 6994 | `#define PH7_IO_STREAM_OUTPUT 4 /* php://output */` |
|         - | 6995 | `#define PH7_IO_STREAM_MEMORY 5 /* php://memory, php://temp, and data:// payloads */` |
|         - | 6996 | `#define PH7_IO_STREAM_FILTER 6 /* php://filter/…/resource=… — a stream wrapped around another */` |
|         - | 6997 | `/* php://input — the REQUEST BODY. There is none under a command line, and php's` |
|         - | 6998 | ` * CLI answers an empty stream for it rather than reading standard input (the` |
|         - | 6999 | `` * body a `php x.php < file` supplies arrives through STDIN, and php://input`` |
|         - | 7000 | ` * stays ""). It runs on the memory machinery, so it is seekable and can be read` |
|         - | 7001 | ` * twice, and it differs from php://memory in exactly four answers: fflush() is` |
|         - | 7002 | ` * FALSE, fstat() is FALSE, ftruncate() is unsupported, and its metadata names` |
|         - | 7003 | `` * it `Input`. */`` |
|         - | 7004 | `#define PH7_IO_STREAM_INPUT  7` |
|         - | 7005 | `/*` |
|         - | 7006 | ` * How far php's directory stream advances per entry read: one` |
|         - | 7007 | ``  * `php_stream_dirent`, which is `char d_name[MAXPATHLEN]` plus the `d_type` `` |
|         - | 7008 | ` * byte. php's MAXPATHLEN is PATH_MAX where the platform has one and a flat 2048` |
|         - | 7009 | ` * on Windows; both numbers were read back from the two php builds rather than` |
|         - | 7010 | `` * assumed (`readdir($d); ftell($d)` answers 4097 here and 2049 there). It is the`` |
|         - | 7011 | ` * only thing ftell() on a directory handle reports, and fseek()/rewind() rewind` |
|         - | 7012 | ` * the directory WITHOUT putting it back.` |
|         - | 7013 | ` */` |
|         - | 7014 | `#ifndef __WINNT__` |
|         - | 7015 | `#include <limits.h>    /* PATH_MAX, which is where php's MAXPATHLEN comes from */` |
|         - | 7016 | `#endif` |
|         - | 7017 | `#ifdef __WINNT__` |
|         - | 7018 | `#define PHL_DIR_RECORD 2049            /* php's win32 MAXPATHLEN is 2048 */` |
|         - | 7019 | `#elif defined(PATH_MAX)` |
|         - | 7020 | `#define PHL_DIR_RECORD (PATH_MAX + 1)  /* php takes MAXPATHLEN from PATH_MAX */` |
|         - | 7021 | `#else` |
|         - | 7022 | `#define PHL_DIR_RECORD 4097` |
|         - | 7023 | `#endif` |
|         - | 7024 | `PH7_PRIVATE int PH7_Utf8Read(` |
|         - | 7025 | `  const unsigned char *z,         /* First byte of UTF-8 character */` |
|         - | 7026 | `  const unsigned char *zTerm,     /* Pretend this byte is 0x00 */` |
|         - | 7027 | `  const unsigned char **pzNext    /* Write first byte past UTF-8 char here */` |
|         - | 7028 | `);` |
|         - | 7029 | `PH7_PRIVATE sxi32 PH7_Utf8ReadStrict(const unsigned char *z,sxu32 n,sxu32 *pLen);` |
|         - | 7030 | `/* parse.c function prototypes */` |
|         - | 7031 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc);` |
|         - | 7032 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot);` |
|         - | 7033 | `PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode);` |
|         - | 7034 | `PH7_PRIVATE int PH7_ExprNodeIsThis(ph7_expr_node *pNode);` |
|         - | 7035 | `PH7_PRIVATE int PH7_ExprNodeIsClassConst(ph7_expr_node *pNode);` |
|         - | 7036 | `PH7_PRIVATE int PH7_ExprIsModifiableValue(ph7_expr_node *pNode);` |
|         - | 7037 | `PH7_PRIVATE SyToken * PH7_ExprTokenInStream(ph7_gen_state *pGen,SyToken *pTok);` |
|         - | 7038 | `PH7_PRIVATE sxi32 PH7_ExprOperandNotAVariable(ph7_gen_state *pGen,ph7_expr_node *pOperand);` |
|         - | 7039 | `/* OP_STORE_REF / OP_STORE_IDX_REF iP1 bit 1: the reference SOURCE was written as a` |
|         - | 7040 | ` * CALL. php's compiler records the same thing (ZEND_RETURNS_FUNCTION) so the bind` |
|         - | 7041 | `` * can raise `Only variables should be assigned by reference` when the callee did`` |
|         - | 7042 | ` * not return by reference. Bit 0 stays STORE_IDX_REF's "a key is on the stack". */` |
|         - | 7043 | `#define PH7_STOREREF_CALLSRC 0x02` |
|         - | 7044 | `/* Context bits for GenStateWriteTargetCheck — php's write-target rules are the` |
|         - | 7045 | ` * same everywhere except for these two distinctions. */` |
|         - | 7046 | ``#define PH7_WTC_UNSET   0x01 /* `unset()`: the $this refusal takes its own wording */`` |
|         - | 7047 | ``#define PH7_WTC_REFSRC  0x02 /* the SOURCE of `=&`: php compiles it in write context`` |
|         - | 7048 | `                              * (so a temporary base is still refused) but never runs` |
|         - | 7049 | `                              * zend_ensure_writable_variable over it, which is why` |
|         - | 7050 | ``                              * `$r =& f()` is legal where `f() =& $x` is not */`` |
|         - | 7051 | ``#define PH7_WTC_RMW     0x04 /* a READ-MODIFY-WRITE target -- `+=`, `.=`, `++`, `--`.`` |
|         - | 7052 | ``                              * php's `$this` rule belongs to the ASSIGNMENT compiler`` |
|         - | 7053 | ``                              * (zend_compile_assign / assign_ref), so `$this += 1` and`` |
|         - | 7054 | ``                              * `$this++` compile and fail at RUN time on the operand`` |
|         - | 7055 | `                              * types instead. The temporary and call rules still apply:` |
|         - | 7056 | ``                              * `(new A)->p++` is refused exactly as `= 1` is. */`` |
|         - | 7057 | ``#define PH7_WTC_THISSRC 0x08 /* an ARRAY LITERAL's `&$x` element. php compiles it in`` |
|         - | 7058 | ``                              * write context -- `[&f()]` is its "Can't use function`` |
|         - | 7059 | `                              * return value in write context" -- but the element only` |
|         - | 7060 | `                              * takes a REFERENCE to the slot, it never re-points it, so` |
|         - | 7061 | ``                              * `$this` is legal there and writing through the element`` |
|         - | 7062 | ``                              * leaves the receiver alone. `[&$this, 'cmp']` is how the`` |
|         - | 7063 | `                              * pre-5.4 callable idiom is spelled and phpseclib's SFTP` |
|         - | 7064 | `                              * still writes it. */` |
|         - | 7065 | `PH7_PRIVATE sxi32 GenStateWriteTargetCheck(ph7_gen_state *pGen,ph7_expr_node *pTarget,int iCtx);` |
|         - | 7066 | `PH7_PRIVATE void PH7_ExprSubtreeSpan(ph7_expr_node *pNode,SyToken **ppMin,SyToken **ppMax);` |
|         - | 7067 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext);` |
|         - | 7068 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd);` |
|         - | 7069 | ``/* TRUE when a KEYWORD token opens `[static] fn[&](…) =>` rather than naming a`` |
|         - | 7070 | ` * variable/member/label ($fn, $o->fn, C::fn, \A\fn, f(fn: 1)). Every raw-token` |
|         - | 7071 | ` * lookahead that has to step over an arrow function must ask this first; the` |
|         - | 7072 | `` * test is positional, so a MALFORMED `fn` still reaches the arrow parser and`` |
|         - | 7073 | `` * keeps php's `expecting "("`. */`` |
|         - | 7074 | `PH7_PRIVATE int PH7_TokenOpensArrowFunc(SyToken *pStart,SyToken *pTok,SyToken *pEnd);` |
|         - | 7075 | `PH7_PRIVATE const ph7_expr_op * PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast);` |
|         - | 7076 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet);` |
|         - | 7077 | `/* compile.c function prototypes */` |
|         - | 7078 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType);` |
|         - | 7079 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 7080 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 7081 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 7082 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 7083 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 7084 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 7085 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 7086 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 7087 | `PH7_PRIVATE sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 7088 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 7089 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 7090 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 7091 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 7092 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 7093 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 7094 | `PH7_PRIVATE int GenStateIsReadonly(SyToken *pTok);` |
|         - | 7095 | `PH7_PRIVATE int GenStateStartsReadonlyAnonClass(SyToken *pIn,SyToken *pEnd);` |
|         - | 7096 | `PH7_PRIVATE int GenStateStartsClosureExpr(SyToken *pIn,SyToken *pEnd);` |
|         - | 7097 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 7098 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 7099 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 7100 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(ph7_vm *pVm,ProcConsumer xErr,void *pErrData);` |
|         - | 7101 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(ph7_vm *pVm,ProcConsumer xErr,void *pErrData);` |
|         - | 7102 | `PH7_PRIVATE void PH7_CompilerSaveState(ph7_vm *pVm,ph7_gen_state *pSaved,ProcConsumer xErr,void *pErrData);` |
|         - | 7103 | `PH7_PRIVATE void PH7_CompilerRestoreState(ph7_vm *pVm,ph7_gen_state *pSaved);` |
|         - | 7104 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...);` |
|         - | 7105 | `PH7_PRIVATE sxi32 PH7_GenSyntaxError(ph7_gen_state *pGen,SyToken *pTok,const char *zExpecting);` |
|         - | 7106 | `PH7_PRIVATE sxi32 PH7_CompileScript(ph7_vm *pVm,SyString *pScript,sxi32 iFlags);` |
|         - | 7107 | `/* constant.c function prototypes */` |
|         - | 7108 | `PH7_PRIVATE void PH7_RegisterBuiltInConstant(ph7_vm *pVm);` |
|         - | 7109 | `PH7_PRIVATE void PH7_MarkDeprecatedConstants(ph7_vm *pVm);` |
|         - | 7110 | `/* vm.c reference/frame internals shared with vm_builtin_var.c */` |
|         - | 7111 | `/* vm_gc.c -- the cycle collector */` |
|         - | 7112 | `PH7_PRIVATE void PH7_GcInit(ph7_vm *pVm);` |
|         - | 7113 | `PH7_PRIVATE void PH7_GcResetBuffer(ph7_vm *pVm);` |
|         - | 7114 | `PH7_PRIVATE void PH7_GcRelease(ph7_vm *pVm);` |
|         - | 7115 | `PH7_PRIVATE void PH7_GcPossibleRoot(ph7_vm *pVm,void *pCont,int bMap);` |
|         - | 7116 | `PH7_PRIVATE void PH7_GcForget(ph7_vm *pVm,void *pCont,int bMap);` |
|         - | 7117 | `PH7_PRIVATE sxu32 PH7_GcCollect(ph7_vm *pVm);` |
|         - | 7118 | `/* vm.c -- lifetime of a run-time closure's per-instantiation ph7_vm_func */` |
|         - | 7119 | `PH7_PRIVATE ph7_vm_func * PH7_VmRuntimeClosure(ph7_vm *pVm,const char *zName,sxu32 nByte);` |
|         - | 7120 | `PH7_PRIVATE void PH7_VmClosureFuncRef(ph7_vm_func *pFunc);` |
|         - | 7121 | `PH7_PRIVATE void PH7_VmClosureFuncUnref(ph7_vm *pVm,ph7_vm_func *pFunc);` |
|         - | 7122 | `PH7_PRIVATE void PH7_VmClosureInstanceRef(ph7_vm *pVm,ph7_class_instance *pObj,int iDelta);` |
|         - | 7123 | `PH7_PRIVATE void PH7_VmPurgeDeadClosures(ph7_vm *pVm);` |
|         - | 7124 | `PH7_PRIVATE SyHashEntry * PH7_VmSuperGet(ph7_vm *pVm,const char *zName,sxu32 nByte);` |
|         - | 7125 | `PH7_PRIVATE void PH7_VmSuperNote(ph7_vm *pVm,const char *zName,sxu32 nByte);` |
|         - | 7126 | `/* The reference table asks its questions BY SLOT: a slot that answers from its word` |
|         - | 7127 | ` * has no record for a caller to hold, so there is no VmRefObjExtract any more. */` |
|         - | 7128 | `PH7_PRIVATE int PH7_VmSlotDropIfBare(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 7129 | `PH7_PRIVATE sxu32 PH7_VmSlotEntryCount(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 7130 | `PH7_PRIVATE sxu32 PH7_VmSlotNodeCount(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 7131 | `PH7_PRIVATE sxu32 PH7_VmSlotPinCount(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 7132 | `PH7_PRIVATE int PH7_VmSlotKeepPinned(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 7133 | `PH7_PRIVATE int PH7_VmSlotRegistered(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 7134 | `PH7_PRIVATE int PH7_VmSlotSoleNodeIs(ph7_vm *pVm,sxu32 nIdx,ph7_hashmap_node *pNode);` |
|         - | 7135 | `PH7_PRIVATE void PH7_VmSlotUnlink(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 7136 | `PH7_PRIVATE int VmDropFrameLocalSlot(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 7137 | `PH7_PRIVATE void VmDropFrameRefEntry(ph7_vm *pVm,sxu32 nIdx,SyHashEntry *pEntry);` |
|         - | 7138 | `PH7_PRIVATE sxu32 PH7_VmSlotHolderCount(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 7139 | `PH7_PRIVATE int PH7_VmSlotSelfPinned(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 7140 | `PH7_PRIVATE int PH7_VmSlotDropOwnerHold(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 7141 | `PH7_PRIVATE void PH7_VmReleaseUnheldSlot(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 7142 | `PH7_PRIVATE void PH7_VmRebindVarSlot(ph7_vm *pVm,VmFrame *pFrame,SyHashEntry *pEntry,` |
|         - | 7143 | `	const char *zName,sxu32 nByte,sxu32 nIdx);` |
|         - | 7144 | `PH7_PRIVATE void PH7_VmBindVarSlot(ph7_vm *pVm,VmFrame *pFrame,const char *zName,sxu32 nByte,` |
|         - | 7145 | `	sxu32 nIdx);` |
|         - | 7146 | `PH7_PRIVATE int PH7_VmVarNameIsInternal(const char *zName,sxu32 nByte);` |
|         - | 7147 | `/* vm_builtin_var.c function prototypes (rows stay in vm.c's aVmFunc[]) */` |
|         - | 7148 | `PH7_PRIVATE sxi32 VmUnsetVarByName(ph7_vm *pVm,VmFrame *pFrame,const char *zName,sxu32 nByte);` |
|         - | 7149 | `PH7_PRIVATE sxi32 VmUnsetVarByNameEx(ph7_vm *pVm,VmFrame *pFrame,const char *zName,sxu32 nByte,int bNameGuard);` |
|         - | 7150 | `PH7_PRIVATE int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7151 | `PH7_PRIVATE int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7152 | `PH7_PRIVATE int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7153 | `PH7_PRIVATE int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7154 | `PH7_PRIVATE int vm_builtin_get_debug_type(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7155 | `PH7_PRIVATE int vm_builtin_settype(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7156 | `PH7_PRIVATE int vm_builtin_get_resource_id(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7157 | `PH7_PRIVATE int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7158 | `PH7_PRIVATE int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7159 | `PH7_PRIVATE int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7160 | `PH7_PRIVATE int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7161 | `/* vm_builtin_lang.c function prototypes (rows stay in vm.c's aVmFunc[]) */` |
|         - | 7162 | `PH7_PRIVATE sxi32 VmClassConstEvalOnDemand(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr);` |
|         - | 7163 | `PH7_PRIVATE void VmDeprecatedConstNotice(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pMember);` |
|         - | 7164 | `PH7_PRIVATE void VmNoDiscardWarn(ph7_vm *pVm,ph7_vm_func *pFunc,ph7_class *pDeclClass);` |
|         - | 7165 | `PH7_PRIVATE ph7_value * VmEnumCaseBackingValue(ph7_vm *pVm,ph7_class_attr *pCase);` |
|         - | 7166 | `PH7_PRIVATE sxi32 VmEnumMaterialize(ph7_vm *pVm,ph7_class *pClass);` |
|         - | 7167 | `PH7_PRIVATE void VmExpandConstantWithNotice(ph7_vm *pVm,ph7_constant *pCons,ph7_value *pOut);` |
|         - | 7168 | `PH7_PRIVATE sxi32 VmExpandConstantOnce(ph7_vm *pVm,ph7_constant *pCons,ph7_value *pOut);` |
|         - | 7169 | `PH7_PRIVATE void VmTrackOutput(ph7_vm *pVm, sxu32 nLen);` |
|         - | 7170 | `PH7_PRIVATE int PH7_VmOutputOrigin(ph7_vm *pVm,SyString *pFile,sxu32 *pnLine);` |
|         - | 7171 | `PH7_PRIVATE int PH7_VmSessionOrigin(ph7_vm *pVm,SyString *pFile,sxu32 *pnLine);` |
|         - | 7172 | `PH7_PRIVATE void PH7_VmSetSessionOrigin(ph7_vm *pVm);` |
|         - | 7173 | `PH7_PRIVATE void PH7_VmAppendWhere(ph7_vm *pVm,SyBlob *pMsg,int bSessionActive);` |
|         - | 7174 | `PH7_PRIVATE ph7_value * VmExtractMemObj(ph7_vm *pVm,const SyString *pName,int bDup,int bCreate);` |
|         - | 7175 | `PH7_PRIVATE ph7_value * PH7_VmExtractVarSlot(ph7_vm *pVm,const SyString *pName,int bCreate,sxu32 nSlot,const VmInstr *aCode);` |
|         - | 7176 | `/* Is a store to this slot filtered at all? The screen in front of every hTypedSlot` |
|         - | 7177 | ` * lookup on a hot path; a false answer is final, a true one still asks the table.` |
|         - | 7178 | ` * With the bitmap disabled it degrades to the emptiness test every one of those` |
|         - | 7179 | ` * call sites used before it existed, which over-answers and never under-answers. */` |
|         - | 7180 | `#define PH7_VM_STORE_FILTERED(pVm,nIdx) \` |
|         - | 7181 | `	((pVm)->bFilterBitsOff \` |
|         - | 7182 | `	 ? SyHashTotalEntry(&(pVm)->hTypedSlot) > 0 \` |
|         - | 7183 | `	 : ((nIdx) < (pVm)->nFilterBits \` |
|         - | 7184 | `	    && ((pVm)->pFilterBits[(nIdx) >> 3] & (1 << ((nIdx) & 7))) != 0))` |
|         - | 7185 | `PH7_PRIVATE void VmVarMemoFlush(VmFrame *pFrame);` |
|         - | 7186 | `/* D1 commit 2: deferred-lvalue-path capture (built by the LOAD_IDX/MEMBER record modes) */` |
|         - | 7187 | `PH7_PRIVATE VmDeferredPath * VmDeferPathNew(ph7_vm *pVm,int eRoot,sxu32 nRootIdx,const SyString *pName);` |
|         - | 7188 | `PH7_PRIVATE VmDeferredPath * VmDeferPathNewPrefetch(ph7_vm *pVm,int nKind,ph7_class *pClass,` |
|         - | 7189 | `	const SyString *pName,ph7_value *pVal);` |
|         - | 7190 | `PH7_PRIVATE sxi32 VmDeferPathPushElem(VmDeferredPath *pPath,ph7_value *pKey);` |
|         - | 7191 | `PH7_PRIVATE sxi32 VmDeferPathPushAppend(VmDeferredPath *pPath);` |
|         - | 7192 | `PH7_PRIVATE sxi32 VmDeferPathPushProp(VmDeferredPath *pPath,const SyString *pName);` |
|         - | 7193 | `PH7_PRIVATE void VmFreeDeferredPath(VmDeferredPath *pPath);` |
|         - | 7194 | `PH7_PRIVATE VmCoalStrOff * VmCoalStrOffNew(ph7_vm *pVm,ph7_value *pKey);` |
|         - | 7195 | `PH7_PRIVATE void VmFreeCoalStrOff(VmCoalStrOff *pCoal);` |
|         - | 7196 | `PH7_PRIVATE VmMagicCall * VmMagicCallNew(ph7_vm *pVm,ph7_class_instance *pRecv,` |
|         - | 7197 | `	ph7_class *pClass,const SyString *pName);` |
|         - | 7198 | `PH7_PRIVATE void VmFreeMagicCall(VmMagicCall *pPend);` |
|         - | 7199 | `PH7_PRIVATE void VmExpandUserConstant(ph7_value *pVal,void *pUserData);` |
|         - | 7200 | `PH7_PRIVATE ph7_class * VmExtractEnumClass(ph7_vm *pVm,ph7_value *pName);` |
|         - | 7201 | `PH7_PRIVATE int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7202 | `PH7_PRIVATE int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7203 | `PH7_PRIVATE int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7204 | `PH7_PRIVATE int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7205 | `PH7_PRIVATE int vm_builtin_enum_cases(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7206 | `PH7_PRIVATE int vm_builtin_enum_exists(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7207 | `PH7_PRIVATE int vm_builtin_enum_from(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7208 | `PH7_PRIVATE int vm_builtin_enum_tryfrom(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7209 | `PH7_PRIVATE int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7210 | `PH7_PRIVATE int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7211 | `PH7_PRIVATE int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7212 | `PH7_PRIVATE int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7213 | `PH7_PRIVATE int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7214 | `PH7_PRIVATE int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7215 | `PH7_PRIVATE int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7216 | `PH7_PRIVATE int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7217 | `PH7_PRIVATE int vm_builtin_php_sapi_name(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7218 | `PH7_PRIVATE int vm_builtin_php_ini_loaded_file(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7219 | `PH7_PRIVATE int vm_builtin_php_ini_scanned_files(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7220 | `PH7_PRIVATE int vm_builtin_phpversion(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7221 | `PH7_PRIVATE int vm_builtin_extension_loaded(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7222 | `PH7_PRIVATE int vm_builtin_get_loaded_extensions(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7223 | `/* vm_extension.c -- the extension partition every internal name is placed in */` |
|         - | 7224 | `PH7_PRIVATE const char * PH7_VmExtensionName(int iExt);` |
|         - | 7225 | `PH7_PRIVATE int PH7_VmExtensionLookup(const char *zName,int nName);` |
|         - | 7226 | `PH7_PRIVATE int PH7_VmExtensionIsLoaded(ph7_vm *pVm,const char *zName,int nName);` |
|         - | 7227 | `PH7_PRIVATE int PH7_VmExtOfFunc(const char *zName,int nName);` |
|         - | 7228 | `PH7_PRIVATE int PH7_VmExtOfClass(const char *zName,int nName);` |
|         - | 7229 | `PH7_PRIVATE int PH7_VmExtOfConstant(const char *zName,int nName);` |
|         - | 7230 | `PH7_PRIVATE int PH7_VmExtOfIni(const char *zName,int nName);` |
|         - | 7231 | `#define PH7_EXT_KIND_FUNC   0` |
|         - | 7232 | `#define PH7_EXT_KIND_CLASS  1` |
|         - | 7233 | `#define PH7_EXT_KIND_CONST  2` |
|         - | 7234 | `#define PH7_EXT_KIND_INI    3` |
|         - | 7235 | `PH7_PRIVATE int PH7_VmExtWalk(int iExt,int iKind,int (*xVisit)(const char *,int,void *),void *pData);` |
|         - | 7236 | `PH7_PRIVATE int PH7_VmInternalNameExists(ph7_vm *pVm,int iKind,const char *zName,int nName);` |
|         - | 7237 | `PH7_PRIVATE int PH7_VmExtWalkDep(int iExt,int (*xVisit)(const char *,const char *,void *),void *pData);` |
|         - | 7238 | `#define PH7_EXT_CORE 0        /* the engine itself; every other id is vm_extension_names.h's */` |
|         - | 7239 | `#define PH7_EXT_MAX  64        /* a caller's per-extension scratch bound; the table is far under it */` |
|         - | 7240 | `PH7_PRIVATE int PH7_VmExtHasName(int iKind,const char *zName,int nName);` |
|         - | 7241 | `PH7_PRIVATE int PH7_VmExtensionCount(void);` |
|         - | 7242 | `PH7_PRIVATE int PH7_VmExtensionAvailable(int iExt);` |
|         - | 7243 | `PH7_PRIVATE int vm_builtin_get_extension_funcs(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7244 | `PH7_PRIVATE int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7245 | `PH7_PRIVATE int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7246 | `PH7_PRIVATE int vm_builtin_random_bytes(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7247 | `PH7_PRIVATE int vm_builtin_random_int(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7248 | `PH7_PRIVATE int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7249 | `PH7_PRIVATE int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7250 | `/* vm.c frame/backtrace internals shared with vm_builtin_error.c */` |
|         - | 7251 | `PH7_PRIVATE VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|         - | 7252 | `PH7_PRIVATE void VmBuildBacktrace(ph7_vm *pVm,sxi32 iOptions,sxi32 iLimit,ph7_value *pList);` |
|         - | 7253 | `PH7_PRIVATE void PH7_GenAppendFatalTrace(ph7_vm *pVm,SyBlob *pOut,int iTraceKind);` |
|         - | 7254 | `PH7_PRIVATE void PH7_VmTraceToString(ph7_vm *pVm,ph7_value *pTrace,int bMainMarker,SyBlob *pOut);` |
|         - | 7255 | `PH7_PRIVATE void PH7_VmFrameActualArgs(ph7_vm *pVm,VmFrame *pFrame,ph7_value *pArray);` |
|         - | 7256 | `/* vm_builtin_error.c function prototypes (rows stay in vm.c's aVmFunc[]) */` |
|         - | 7257 | `PH7_PRIVATE int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7258 | `PH7_PRIVATE int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7259 | `PH7_PRIVATE int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7260 | `PH7_PRIVATE int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7261 | `PH7_PRIVATE int vm_builtin_error_clear_last(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7262 | `PH7_PRIVATE int vm_builtin_error_get_last(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7263 | `PH7_PRIVATE int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7264 | `PH7_PRIVATE int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7265 | `PH7_PRIVATE int vm_builtin_get_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7266 | `PH7_PRIVATE int vm_builtin_get_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7267 | `PH7_PRIVATE int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7268 | `PH7_PRIVATE int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7269 | `PH7_PRIVATE int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7270 | `PH7_PRIVATE int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7271 | `PH7_PRIVATE int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7272 | `/* Autoload-callback record (spl_autoload_register in vm_include.c; walked by` |
|         - | 7273 | ` * VmTriggerAutoload in vm.c) */` |
|         - | 7274 | `typedef struct VmAutoloadCB VmAutoloadCB;` |
|         - | 7275 | `struct VmAutoloadCB` |
|         - | 7276 | `{` |
|         - | 7277 | `	ph7_value sCallback; /* Autoload callback (string or [obj,method] array) */` |
|         - | 7278 | `};` |
|         - | 7279 | `/* Shutdown-callback record (register_shutdown_function in vm_builtin_call.c;` |
|         - | 7280 | ` * invoked by VmInvokeShutdownCallbacks in vm.c) */` |
|         - | 7281 | `typedef struct VmShutdownCB VmShutdownCB;` |
|         - | 7282 | `struct VmShutdownCB` |
|         - | 7283 | `{` |
|         - | 7284 | `	ph7_value sCallback; /* Shutdown callback */` |
|         - | 7285 | `	ph7_value aArg[10];   /* Callback arguments (10 maximum arguments) */` |
|         - | 7286 | `	int nArg;             /* Total number of given arguments */` |
|         - | 7287 | `};` |
|         - | 7288 | `/* Operand-stack guard slack (vm.c allocator; checked by the call machinery) */` |
|         - | 7289 | `#define VM_STACK_GUARD 16` |
|         - | 7290 | `/* vm.c closure/exception internals shared with vm_builtin_call.c */` |
|         - | 7291 | `PH7_PRIVATE int VmValueIsClosure(ph7_vm *pVm,ph7_value *pVal);` |
|         - | 7292 | `PH7_PRIVATE int PH7_VmFuncDisplayName(ph7_vm *pVm,ph7_vm_func *pFunc,const char **pzOut);` |
|         - | 7293 | `PH7_PRIVATE sxi32 VmClosureUnwrap(ph7_vm *pVm,ph7_value *pVal,ph7_value *pOut);` |
|         - | 7294 | `PH7_PRIVATE sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|         - | 7295 | `PH7_PRIVATE sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|         - | 7296 | `PH7_PRIVATE ph7_value * VmNewOperandStack(ph7_vm *pVm,sxu32 nInstr);` |
|         - | 7297 | `/* Fiber/generator trampoline state (BYTECODE stages 2-4); shared between` |
|         - | 7298 | ` * vm.c's interpreter and vm_exec_ctx.c's park/resume machinery. */` |
|         - | 7299 | `/*` |
|         - | 7300 | ` * Boundary state of one VmByteCodeExec activation:` |
|         - | 7301 | ` * everything the executor must restore to continue an activation after a` |
|         - | 7302 | ` * nested call returns. pc/pTos are authoritative here only at activation` |
|         - | 7303 | ` * boundaries — the dispatch loop keeps them in locals for the hot path and` |
|         - | 7304 | ` * syncs around the call epilogue and the terminal labels. Stage 2 stacks` |
|         - | 7305 | ` * these records to replace the native recursion.` |
|         - | 7306 | ` */` |
|         - | 7307 | `typedef struct VmExecState VmExecState;` |
|         - | 7308 | `struct VmExecState` |
|         - | 7309 | `{` |
|         - | 7310 | `	VmInstr *aInstr;        /* Bytecode of this activation */` |
|         - | 7311 | `	ph7_value *pStack;      /* Operand-stack base (owned by this activation) */` |
|         - | 7312 | `	ph7_value *pTos;        /* Top-of-stack (synced at boundaries) */` |
|         - | 7313 | `	ph7_value *pHigh;       /* WATERMARK: the deepest pTos this activation ever reached,` |
|         - | 7314 | `	                         * sampled at each instruction fetch and synced with pTos at` |
|         - | 7315 | `	                         * the same boundaries. Nothing above it was ever written, so` |
|         - | 7316 | `	                         * it is what the operand stack's teardown sweep walks to --` |
|         - | 7317 | `	                         * see VmOperandStackRecycle. An OP_SPREAD that reallocs the` |
|         - | 7318 | `	                         * buffer resets it to the whole (grown) capacity rather than` |
|         - | 7319 | `	                         * carrying a pointer into the freed one. */` |
|         - | 7320 | `	sxu32 nStackCap;        /* pStack's allocated slot count; grows when an OP_SPREAD in` |
|         - | 7321 | `	                         * this activation reallocs the operand stack (see` |
|         - | 7322 | `	                         * VmGrowOperandStack). Saved/restored with the activation. */` |
|         - | 7323 | `	sxu32 nStackOrig;       /* The activation's ORIGINAL (ungrown) capacity — nMaxStack+guard,` |
|         - | 7324 | `	                         * fixed at entry. VmGrowOperandStack sizes headroom relative to` |
|         - | 7325 | `	                         * THIS (not the grown nStackCap) so capacity can't ratchet up` |
|         - | 7326 | `	                         * across statements that share one operand stack. */` |
|         - | 7327 | `	sxi32 pc;               /* Program counter (synced at boundaries) */` |
|         - | 7328 | `	sxu32 nExceptionBase;   /* Exception-stack depth at entry (finally-drain floor) */` |
|         - | 7329 | `	sxu32 nFinallyActBase;  /* aFinallyAction depth at entry: actions above it belong to` |
|         - | 7330 | `	                         * this activation and are DISCARDED (refs released) when the` |
|         - | 7331 | ``	                         * activation ends — a `return` inside a redirect-entered`` |
|         - | 7332 | `	                         * finally short-circuits OP_END_FINALLY, orphaning its` |
|         - | 7333 | `	                         * pending action (an FA_RETHROW holding the swallowed` |
|         - | 7334 | `	                         * exception), which would otherwise be mis-popped by an` |
|         - | 7335 | `	                         * enclosing function's next END_FINALLY. */` |
|         - | 7336 | `	VmFrame *pEntryFrame;   /* Active frame at entry (exec identity for VmRecordedResume) */` |
|         - | 7337 | `	ph7_value *pResult;     /* Where the terminal OP_DONE stores the result (or NULL) */` |
|         - | 7338 | `	sxu32 *pLastRef;        /* By-ref return out-param (or NULL) */` |
|         - | 7339 | `	ph7_vm_func *pEnforceRetFunc; /* Return-type enforcement target (user-fn bodies only) */` |
|         - | 7340 | `	sxu8 is_callback;       /* TRUE only for a C->PHP callback trampoline activation */` |
|         - | 7341 | `	sxu8 bReturnPropagates; /* TRUE only for a catch/finally mini-program */` |
|         - | 7342 | `};` |
|         - | 7343 | `/*` |
|         - | 7344 | ` * One in-flight user-function call: what the caller's OP_CALL set up and the` |
|         - | 7345 | ` * pop boundary (VmCallFinish) must tear down.` |
|         - | 7346 | ` */` |
|         - | 7347 | `typedef struct VmCallRecord VmCallRecord;` |
|         - | 7348 | `struct VmCallRecord` |
|         - | 7349 | `{` |
|         - | 7350 | `	ph7_vm_func *pVmFunc;   /* Callee */` |
|         - | 7351 | `	VmFrame *pFrame;        /* Callee's VM frame (entered by the OP_CALL setup) */` |
|         - | 7352 | `	ph7_value *pFrameStack; /* Callee's operand stack (owned; freed here). NULL when the body was skipped */` |
|         - | 7353 | `	sxu32 nStackCap;        /* pFrameStack's allocated slot count — nMaxStack+VM_STACK_GUARD` |
|         - | 7354 | `	                         * at setup, updated if an OP_SPREAD in the callee grew it; the` |
|         - | 7355 | `	                         * pop-time recycle releases exactly this many slots */` |
|         - | 7356 | `	sxu32 nLiveTos;         /* How many of pFrameStack's slots this activation ever` |
|         - | 7357 | `	                         * touched: its operand-stack watermark + 1. The pop-time` |
|         - | 7358 | `	                         * recycle releases exactly this many and leaves the rest` |
|         - | 7359 | `	                         * alone -- see VmOperandStackRecycle */` |
|         - | 7360 | `	sxu32 nLastRef;         /* Callee body's last-referenced slot (by-ref return) */` |
|         - | 7361 | `	sxu8 bSelfPushed;       /* TRUE when the setup pushed onto pVm->aSelf */` |
|         - | 7362 | `};` |
|         - | 7363 | `/*` |
|         - | 7364 | ` * One node of the in-loop call-record stack (BYTECODE stage 2): the caller's` |
|         - | 7365 | ` * activation to restore plus the in-flight call to finish, linked to the` |
|         - | 7366 | ` * next-outer record. Nodes are pool-allocated individually so pointers into` |
|         - | 7367 | ` * them (sState.pLastRef aims at sCall.nLastRef while the callee runs) stay` |
|         - | 7368 | ` * stable — a growable array would invalidate them on realloc. The stack is a` |
|         - | 7369 | ` * LOCAL of each native VmByteCodeExec invocation: an inner native entry` |
|         - | 7370 | ` * (mini-program, C->PHP callback, ctx resume) can never unwind records that` |
|         - | 7371 | ` * belong to an outer invocation, preserving the old nesting isolation by` |
|         - | 7372 | ` * construction.` |
|         - | 7373 | ` */` |
|         - | 7374 | `typedef struct VmCallFrame VmCallFrame;` |
|         - | 7375 | `struct VmCallFrame` |
|         - | 7376 | `{` |
|         - | 7377 | `	VmExecState sCaller;   /* Caller activation, restored on pop */` |
|         - | 7378 | `	VmCallRecord sCall;    /* The in-flight call, finished (VmCallFinish) on pop */` |
|         - | 7379 | `	VmCallFrame *pPrev;    /* Next-outer record, or NULL at this invocation's base */` |
|         - | 7380 | `};` |
|         - | 7381 | `typedef struct VmParkedSegment VmParkedSegment;` |
|         - | 7382 | `/*` |
|         - | 7383 | ` * BYTECODE stage 4: a Fiber::suspend() from inside a nested PHP call parks the` |
|         - | 7384 | ` * whole trampoline record segment here instead of unwinding it. The records,` |
|         - | 7385 | ` * their VmFrames and operand stacks all stay alive on the heap (that IS what a` |
|         - | 7386 | ` * suspended fiber is); only the dispatch loop's pointers move into the ctx.` |
|         - | 7387 | ` * Resume re-pushes the chain and continues INSIDE the innermost callee.` |
|         - | 7388 | ` */` |
|         - | 7389 | `struct VmParkedSegment` |
|         - | 7390 | `{` |
|         - | 7391 | `	VmExecState sState;    /* Innermost activation — resume re-enters here (pTos synced) */` |
|         - | 7392 | `	VmCallFrame *pCallTop; /* Parked record chain (caller activations toward the body) */` |
|         - | 7393 | `	VmFrame *pTopFrame;    /* pVm->pFrame at suspend (innermost callee / open-try frame) */` |
|         - | 7394 | `	sxu32 nOldExcBase;     /* pCtx->nExceptionBase at park — resume rebases the segment's` |
|         - | 7395 | `	                        * absolute nExceptionBase floors by (newBase - nOldExcBase) */` |
|         - | 7396 | `	sxu32 nOldFinBase;     /* pCtx->nFinallyBase at park — resume rebases the segment's` |
|         - | 7397 | `	                        * absolute nFinallyActBase floors by its OWN delta (the two` |
|         - | 7398 | `	                        * stacks move independently) */` |
|         - | 7399 | `	int nRecords;          /* Chain length: each record contributed one nRecursionDepth++` |
|         - | 7400 | `	                        * (and, if bSelfPushed, one aSelf push) that VmCallFinish never` |
|         - | 7401 | `	                        * ran. Deactivate that accounting while parked, reactivate on` |
|         - | 7402 | `	                        * resume; an abandoned segment stays deactivated. */` |
|         - | 7403 | `};` |
|         - | 7404 |  |
|         - | 7405 | `PH7_PRIVATE sxi32 VmByteCodeExec(ph7_vm *pVm,VmInstr *aInstr,ph7_value *pStack,int nTos,` |
|         - | 7406 | `	ph7_value *pResult,sxu32 *pLastRef,int is_callback,sxi32 nPc,` |
|         - | 7407 | `	ph7_vm_func *pEnforceRetFunc,int bReturnPropagates,` |
|         - | 7408 | `	VmParkedSegment *pAdoptSegment,ph7_value **ppBaseOwner,` |
|         - | 7409 | `	sxu32 *pnBaseCap,sxu32 nStackOrig);` |
|         - | 7410 | `/* vm.c frame/type-enforcement internals shared with vm_exec_ctx.c (and the` |
|         - | 7411 | ` * upcoming vm_error.c) */` |
|         - | 7412 | `PH7_PRIVATE int VmCheckPseudoType(ph7_vm *pVm, ph7_value *pValue, const SyString *pClass);` |
|         - | 7413 | `PH7_PRIVATE sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable, int bStrict,` |
|         - | 7414 | `	ph7_class *pSelf);` |
|         - | 7415 | `PH7_PRIVATE void VmMaterializeIntTyped(ph7_value *pVal, sxu32 nType);` |
|         - | 7416 | `PH7_PRIVATE void VmDropResumeTarget(ph7_vm *pVm, VmFrame *pFrame);` |
|         - | 7417 | `PH7_PRIVATE void VmReleaseFrameForeachSteps(ph7_vm *pVm, VmFrame *pFrame);` |
|         - | 7418 | `/* The in-place-catch resume record, moved as a whole. See its four fields in ph7_vm. */` |
|         - | 7419 | `typedef struct VmResumeTarget {` |
|         - | 7420 | `	VmFrame *pFrame;` |
|         - | 7421 | `	sxu32 iPc;` |
|         - | 7422 | `	void *pInstr;` |
|         - | 7423 | `	sxi32 iStackDepth;` |
|         - | 7424 | `} VmResumeTarget;` |
|         - | 7425 | `PH7_PRIVATE void VmSetResumeTarget(ph7_vm *pVm,VmFrame *pFrame,sxu32 iPc,void *pInstr,sxi32 iStackDepth);` |
|         - | 7426 | `PH7_PRIVATE void VmClearResumeTarget(ph7_vm *pVm);` |
|         - | 7427 | `PH7_PRIVATE void VmSaveResumeTarget(ph7_vm *pVm,VmResumeTarget *pSave);` |
|         - | 7428 | `PH7_PRIVATE void VmRestoreResumeTarget(ph7_vm *pVm,const VmResumeTarget *pSave);` |
|         - | 7429 | `/* Flags for VmEnforcePropertyTypeOnStore(). CLONE_INIT is php 8.5's` |
|         - | 7430 | ` * clone-with re-initialization of a readonly property; VIA_REF says the write` |
|         - | 7431 | ` * arrived through a REFERENCE to the slot rather than through the property` |
|         - | 7432 | ` * itself, which is a sentence of its own in php. */` |
|         - | 7433 | `#define VM_TYPED_STORE_CLONE_INIT 0x01` |
|         - | 7434 | `#define VM_TYPED_STORE_VIA_REF    0x02` |
|         - | 7435 | `PH7_PRIVATE sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue,int iStoreFlags);` |
|         - | 7436 | `PH7_PRIVATE sxi32 PH7_VmNativeSetSlot(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue);` |
|         - | 7437 | `PH7_PRIVATE sxi32 PH7_VmStoreFilterRegister(ph7_vm *pVm,VmClassAttr *pVmAttr);` |
|         - | 7438 | `PH7_PRIVATE void PH7_VmStoreFilterDrop(ph7_vm *pVm,ph7_class_attr *pAttr,sxu32 nIdx);` |
|         - | 7439 | `PH7_PRIVATE sxi32 VmEnforceScalarType(ph7_value *pVal, sxu32 nType, int bStrict);` |
|         - | 7440 | `PH7_PRIVATE void VmExcReleaseAll(ph7_vm *pVm,SySet *pSet);` |
|         - | 7441 | `PH7_PRIVATE const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf);` |
|         - | 7442 | `PH7_PRIVATE int VmFuncHasReturnType(ph7_vm_func *pFunc);` |
|         - | 7443 | `PH7_PRIVATE int PH7_VmHookSplitName(SyString *pName,SyString *pProp,const char **pzKind);` |
|         - | 7444 | `PH7_PRIVATE int PH7_VmHookFuncName(ph7_class *pClass,ph7_vm_func *pFunc,SyBlob *pOut);` |
|         - | 7445 | `PH7_PRIVATE sxu32 VmFuncRequiredArgCount(ph7_vm_func *pFunc,sxu32 *pnNonVariadic);` |
|         - | 7446 | `PH7_PRIVATE void VmLeaveFrame(ph7_vm *pVm);` |
|         - | 7447 | `PH7_PRIVATE int VmNativeNestingExceeded(ph7_vm *pVm);` |
|         - | 7448 | `PH7_PRIVATE sxi32 VmNativeNestingFatal(ph7_vm *pVm);` |
|         - | 7449 | `PH7_PRIVATE VmFrame * VmNewFrame(ph7_vm *pVm, void *pUserData, ph7_class_instance *pThis);` |
|         - | 7450 | `PH7_PRIVATE ph7_class *VmHintScopeClass(ph7_vm *pVm, ph7_class *pDecl, ph7_class *pUsing);` |
|         - | 7451 | `PH7_PRIVATE int VmClassHintMatches(ph7_vm *pVm,const SyString *pName,ph7_class *pScope,` |
|         - | 7452 | `	ph7_value *pVal,ph7_class **ppResolved);` |
|         - | 7453 | `PH7_PRIVATE const char *VmHintTextResolved(ph7_vm *pVm,const SyString *pDeclared,ph7_class *pScope,` |
|         - | 7454 | `	char *zBuf,sxu32 nBuf);` |
|         - | 7455 | ``#define PH7_HINT_TEXT_ITERABLE 0x1 /* expand a standalone `iterable` to Traversable\|array */`` |
|         - | 7456 | ``#define PH7_HINT_TEXT_STATIC   0x2 /* resolve `static` beside `self`/`parent` */`` |
|         - | 7457 | `PH7_PRIVATE const char *VmHintTextResolvedEx(ph7_vm *pVm,const SyString *pDeclared,ph7_class *pScope,` |
|         - | 7458 | `	int iFlags,char *zBuf,sxu32 nBuf);` |
|         - | 7459 | `PH7_PRIVATE ph7_class *VmHintScopeDeclared(ph7_class *pDecl);` |
|         - | 7460 | `PH7_PRIVATE ph7_class *VmResolveTypeClass(ph7_vm *pVm, const SyString *pCN, ph7_class *pSelf);` |
|         - | 7461 | `PH7_PRIVATE const char *VmScalarTypeName(sxu32 nType, SyString *pDeclared, char *zBuf, sxu32 nBuf);` |
|         - | 7462 | `PH7_PRIVATE const char *VmClassHintTypeName(const SyString *pAsWritten,ph7_class *pResolved,` |
|         - | 7463 | `	int bNullable,char *zBuf,sxu32 nBuf);` |
|         - | 7464 | `PH7_PRIVATE sxi32 VmThrowBuiltinTooFewArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName, sxu32 nPassed,sxu32 nRequired,sxu32 nMaxDeclared);` |
|         - | 7465 | `PH7_PRIVATE sxi32 VmThrowBuiltinTooManyArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName, sxu32 nPassed,sxu32 nMax,sxu32 nRequired);` |
|         - | 7466 | `PH7_PRIVATE sxi32 VmThrowTooFewArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName, ph7_vm_func *pCallee,sxu32 nPassed,sxu32 nRequired,sxu32 nNonVariadic,int bCallSite);` |
|         - | 7467 | `PH7_PRIVATE void PH7_VmWarnByRefValueGiven(ph7_vm *pVm,ph7_class *pOwnerClass,ph7_vm_func *pCallee,sxu32 nArgPos,SyString *pArgName);` |
|         - | 7468 | `PH7_PRIVATE sxi32 VmThrowByRefRefusal(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,ph7_vm_func *pCallee,sxu32 nArgPos,SyString *pArgName);` |
|         - | 7469 | `PH7_PRIVATE sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,ph7_class *pOwnerClass,ph7_vm_func *pCallee,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven);` |
|         - | 7470 | `PH7_PRIVATE sxi32 VmVariadicElementTypeCheck(ph7_vm *pVm,ph7_class *pSelfHint,ph7_vm_func *pCallee,ph7_vm_func_arg *pFormal,ph7_value *pVal,sxu32 nArgPos,int bCallIsStrict);` |
|         - | 7471 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex);` |
|         - | 7472 | `PH7_PRIVATE sxi32 VmMemPoolInit(VmMemPool *pPool,SyMemBackend *pAllocator);` |
|         - | 7473 | `PH7_PRIVATE ph7_value * VmMemPoolReserve(VmMemPool *pPool,sxu32 *pIndex);` |
|         - | 7474 | `PH7_PRIVATE sxi32 VmMemPoolTruncate(VmMemPool *pPool,sxu32 nNewSize);` |
|         - | 7475 | `PH7_PRIVATE void VmMemPoolFreeSlot(VmMemPool *pPool,sxu32 nIdx);` |
|         - | 7476 | `/* Argument-unpacking key capture (PHP 8.1 named-parameter semantics for spreads).` |
|         - | 7477 | `` * `pMap->aNames` is COMPILE-TIME metadata indexed by compile-time argument`` |
|         - | 7478 | ` * position, but a runtime spread expands its slot to a variable element count,` |
|         - | 7479 | ` * so any spread that expands to !=1 element shifts the following actual stack` |
|         - | 7480 | ` * positions out of alignment with aNames — and the element keys (which PHP 8.1` |
|         - | 7481 | ` * treats as named arguments) are otherwise discarded. OP_SPREAD records one` |
|         - | 7482 | ` * VmSpreadRun per expansion plus one VmSpreadKey per element (in order) on the` |
|         - | 7483 | ` * VM; CALL/NEW replay them (VmBuildEffectiveArgMap) into an effective map with` |
|         - | 7484 | ` * one name entry per ACTUAL slot, then let the existing named-argument resolver` |
|         - | 7485 | ` * run unchanged. The same runs give each call its own argument-count growth` |
|         - | 7486 | ` * (VmSpreadOwnExtra). This call's runs are consumed (truncated) at the CALL. */` |
|         - | 7487 | `typedef struct VmSpreadRun VmSpreadRun;` |
|         - | 7488 | `struct VmSpreadRun {` |
|         - | 7489 | `	ph7_value *pStart;   /* First stack slot the expansion wrote (the source slot) */` |
|         - | 7490 | `	sxu32 nCount;        /* Elements produced (0 for an empty array) */` |
|         - | 7491 | `	sxu32 nKeyStart;     /* aSpreadKey index of this run's first element key */` |
|         - | 7492 | `	sxu32 nBlobStart;    /* sSpreadKeyBlob length before this run's keys were appended */` |
|         - | 7493 | `};` |
|         - | 7494 | `typedef struct VmSpreadKey VmSpreadKey;` |
|         - | 7495 | `struct VmSpreadKey {` |
|         - | 7496 | `	sxu32 nOff;          /* Byte offset into pVm->sSpreadKeyBlob (valid iff nLen>0) */` |
|         - | 7497 | `	sxu32 nLen;          /* Key length; 0 == integer key == positional element */` |
|         - | 7498 | `};` |
|         - | 7499 | `#define VM_STACK_UNMODELED SXU32_HIGH /* shared by vm.c (stack modeling) and vm_exec.c */` |
|         - | 7500 | `/* vm_ops_*.c — opcode handlers extracted from VmByteCodeExecBody. The loop` |
|         - | 7501 | ` * syncs pTos/pc into its VmExecState, calls the handler, reloads them and` |
|         - | 7502 | ` * maps the returned code onto its labels. */` |
|         - | 7503 | `typedef enum VmOpRc {` |
|         - | 7504 | `	VM_OP_NEXT = 0,   /* arm done: fall to the loop's trailing pc++ */` |
|         - | 7505 | `	VM_OP_ABORT,      /* -> the loop's Abort label */` |
|         - | 7506 | `	VM_OP_EXCEPTION   /* -> the loop's Exception label */` |
|         - | 7507 | `} VmOpRc;` |
|         - | 7508 | `PH7_PRIVATE int VmRecordedResume(ph7_vm *pVm,sxi32 *pResumePc,VmFrame *pEntryFrame,VmInstr *aInstr);` |
|         - | 7509 | `/*` |
|         - | 7510 | ` * Does the pending ROOT C inline redirect belong to the activation running` |
|         - | 7511 | ` * (aInstrArg, pEntryArg)? Both halves are needed: the bytecode array alone is` |
|         - | 7512 | ` * shared by every live activation of one function (see pInlineFrame). A redirect` |
|         - | 7513 | ` * whose owning frame was invalidated (VmDrainFinally retires a handler by zeroing` |
|         - | 7514 | ` * it) names no activation, so it falls back to the bytecode array alone — losing` |
|         - | 7515 | ` * the catch entirely would be worse than landing it one activation over.` |
|         - | 7516 | ` */` |
|         - | 7517 | `#define VmInlineOwnedBy(pVm,aInstrArg,pEntryArg) \` |
|         - | 7518 | `	((pVm)->pInlineInstr == (void *)(aInstrArg) \` |
|         - | 7519 | `	 && ((pVm)->pInlineFrame == 0 \|\| (pVm)->pInlineFrame == (void *)(pEntryArg)))` |
|         - | 7520 | `PH7_PRIVATE void VmPopOperand(ph7_value **ppTos, sxi32 nPop);` |
|         - | 7521 | `PH7_PRIVATE void VmForeachStepUnlink(ph7_foreach_info *pInfo,ph7_foreach_step *pStep);` |
|         - | 7522 | `PH7_PRIVATE int VmHookGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName);` |
|         - | 7523 | `PH7_PRIVATE void VmForeachHashmapStepRelease(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,int bPop);` |
|         - | 7524 | `PH7_PRIVATE void VmForeachStepRelease(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep);` |
|         - | 7525 | `PH7_PRIVATE void VmForeachStepAbandon(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,ph7_class_instance *pThis);` |
|         - | 7526 | `PH7_PRIVATE int VmClassAllowsDynamicProps(ph7_vm *pVm,ph7_class *pClass);` |
|         - | 7527 | `PH7_PRIVATE int VmClassHasAttributeNamed(ph7_class *pClass,const char *zName,sxu32 nName);` |
|         - | 7528 | `/* __PHP_Incomplete_Class: unserialize()'s carrier object (vm.c helpers).` |
|         - | 7529 | ` * PH7_INCOMPLETE_MAGIC_MEMBER is php's MAGIC_MEMBER — the dynamic property that` |
|         - | 7530 | ` * remembers the original class name; the serializer strips it back out. */` |
|         - | 7531 | `#define PH7_INCOMPLETE_MAGIC_MEMBER "__PHP_Incomplete_Class_Name"` |
|         - | 7532 | `PH7_PRIVATE int PH7_VmIsIncompleteClass(ph7_vm *pVm,ph7_class *pClass);` |
|         - | 7533 | `PH7_PRIVATE void PH7_VmIncompleteMsg(ph7_vm *pVm,ph7_class_instance *pThis,const char *zWhat,SyBlob *pOut);` |
|         - | 7534 | `PH7_PRIVATE void PH7_VmIncompleteAccessWarn(ph7_vm *pVm,ph7_class_instance *pThis,SyString *pFuncName);` |
|         - | 7535 | `PH7_PRIVATE void VmRecreateDeclaredAttr(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_attr *pAttr,VmClassAttr **ppAttr);` |
|         - | 7536 | `PH7_PRIVATE int VmMemberCtxIsLookup(sxi32 iP2);` |
|         - | 7537 | `PH7_PRIVATE int VmMemberCtxWantsValue(sxi32 iP2);` |
|         - | 7538 | `PH7_PRIVATE int VmMemberNextIsWrite(const VmInstr *pNext);` |
|         - | 7539 | `PH7_PRIVATE int VmMemberNextIsRmw(const VmInstr *pNext);` |
|         - | 7540 | `PH7_PRIVATE int VmNextIsCompoundAssign(const VmInstr *pNext);` |
|         - | 7541 | `PH7_PRIVATE sxi32 VmSpreadOwnExtra(ph7_vm *pVm, sxi32 iP1, ph7_value *pTos);` |
|         - | 7542 | `PH7_PRIVATE VmCallArgMap *VmEffCallArgMap(ph7_vm *pVm, VmInstr *pInstr, ph7_value *pArg, sxu32 nActual, VmCallArgMap *pStorage);` |
|         - | 7543 | `PH7_PRIVATE int VmMagicGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind);` |
|         - | 7544 | `PH7_PRIVATE void VmMagicGuardPush(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind);` |
|         - | 7545 | `PH7_PRIVATE void VmMagicGuardPop(ph7_vm *pVm);` |
|         - | 7546 | `PH7_PRIVATE void VmPinMemObjSlot(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 7547 | `PH7_PRIVATE void PH7_VmBindAttrRef(ph7_vm *pVm,VmClassAttr *pVmAttr,sxu32 nSrcIdx);` |
|         - | 7548 | `PH7_PRIVATE void VmPinMemObjSlotCounted(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 7549 | `PH7_PRIVATE void VmUnpinMemObjSlot(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 7550 | `PH7_PRIVATE sxi32 VmSpreadMergeStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData);` |
|         - | 7551 | `PH7_PRIVATE int VmValueIsTraversable(ph7_vm *pVm, ph7_value *pVal);` |
|         - | 7552 | `PH7_PRIVATE sxi32 VmHashmapRefInsert(ph7_hashmap *pMap, const char *zKey, sxu32 nByte, sxu32 nRefIdx);` |
|         - | 7553 | `PH7_PRIVATE void VmWarnCannotUseAsArray(ph7_vm *pVm,sxi32 iFlags);` |
|         - | 7554 | `PH7_PRIVATE sxi32 VmHookRmwConsume(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 7555 | `PH7_PRIVATE void VmHookRmwFreeScratch(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 7556 | `PH7_PRIVATE sxi32 VmHookSetDispatch(ph7_vm *pVm,ph7_class_instance *pHThis,ph7_class_attr *pHAttr,sxu32 nBackIdx,ph7_value *pValue);` |
|         - | 7557 | `PH7_PRIVATE void VmMagicSetDispatch(ph7_vm *pVm,ph7_class_instance *pSetThis,const SyString *pName,ph7_value *pValue);` |
|         - | 7558 | `PH7_PRIVATE ph7_exception * VmExcActivate(ph7_vm *pVm,ph7_exception *pCompiled);` |
|         - | 7559 | `PH7_PRIVATE ph7_exception * VmExcLive(ph7_vm *pVm,ph7_exception *pCompiled);` |
|         - | 7560 | `PH7_PRIVATE sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight);` |
|         - | 7561 | `PH7_PRIVATE sxu32 VmComputeMaxStack(ph7_vm *pVm, VmInstr *aInstr, sxu32 nInstr);` |
|         - | 7562 | `PH7_PRIVATE sxi32 VmDrainFinally(ph7_vm *pVm, sxu32 nExceptionBase);` |
|         - | 7563 | `PH7_PRIVATE sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName);` |
|         - | 7564 | `PH7_PRIVATE void VmHookRmwDropTop(ph7_vm *pVm);` |
|         - | 7565 | `PH7_PRIVATE sxi32 VmInitCallContext(ph7_context *pOut, ph7_vm *pVm, ph7_user_func *pFunc, ph7_value *pRet, sxi32 iFlags);` |
|         - | 7566 | `PH7_PRIVATE void VmMaterializeCatchReturn(ph7_vm *pVm, ph7_value *pResult, VmFrame *pEntryFrame);` |
|         - | 7567 | `PH7_PRIVATE ph7_value * VmOperandStackAlloc(ph7_vm *pVm, sxu32 nSlots);` |
|         - | 7568 | `PH7_PRIVATE void VmOperandStackRecycle(ph7_vm *pVm, ph7_value *pStack, sxu32 nCap, sxu32 nLive);` |
|         - | 7569 | `PH7_PRIVATE ph7_vm_func * VmOverload(ph7_vm *pVm, ph7_vm_func *pList, ph7_value *aArg, int nArg);` |
|         - | 7570 | `PH7_PRIVATE void VmReleaseCallContext(ph7_context *pCtx);` |
|         - | 7571 | `PH7_PRIVATE sxi32 VmResolveNamedArgs(ph7_vm *pVm, VmCallArgMap *pMap, ph7_vm_func_arg *aFormalArg, sxu32 nNonVariadic, sxi32 iVariadicIdx, sxu32 nActual, sxi32 *aSlot, sxu8 *aUsed);` |
|         - | 7572 | `PH7_PRIVATE void VmSpreadExpandMap(ph7_vm *pVm, ph7_value **ppTos, ph7_hashmap *pMap, int bVarSource);` |
|         - | 7573 | `PH7_PRIVATE sxi32 VmSpreadValuesStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData);` |
|         - | 7574 | `PH7_PRIVATE int VmStringWantsPerlIncr(ph7_value *pVal);` |
|         - | 7575 | `PH7_PRIVATE sxi32 VmSuspendCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, sxi32 pc, sxi32 nTos);` |
|         - | 7576 | `PH7_PRIVATE int VmExcMatches(ph7_exception *pExc,ph7_exception *pCompiled);` |
|         - | 7577 | `PH7_PRIVATE void VmSpreadConsume(ph7_vm *pVm);` |
|         - | 7578 | `PH7_PRIVATE VmOpRc VmExecOpSwitch(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 7579 | `PH7_PRIVATE VmOpRc VmExecOpCatch(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 7580 | `PH7_PRIVATE VmOpRc VmExecOpLoadException(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 7581 | `PH7_PRIVATE VmOpRc VmExecOpSpaceship(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 7582 | `PH7_PRIVATE VmOpRc VmExecOpGe(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 7583 | `PH7_PRIVATE VmOpRc VmExecOpLe(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 7584 | `PH7_PRIVATE VmOpRc VmExecOpTne(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 7585 | `PH7_PRIVATE VmOpRc VmExecOpTeq(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 7586 | `PH7_PRIVATE VmOpRc VmExecOpNeq(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 7587 | `PH7_PRIVATE VmOpRc VmExecOpLor(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 7588 | `PH7_PRIVATE VmOpRc VmExecOpShrStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 7589 | `PH7_PRIVATE VmOpRc VmExecOpShr(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 7590 | `PH7_PRIVATE VmOpRc VmExecOpMulStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 7591 | `PH7_PRIVATE VmOpRc VmExecOpLoadList(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 7592 | `PH7_PRIVATE VmOpRc VmExecOpUnsetVar(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 7593 | `PH7_PRIVATE VmOpRc VmExecOpConsume(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 7594 | `PH7_PRIVATE VmOpRc VmExecOpMatch(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 7595 | `PH7_PRIVATE VmOpRc VmExecOpClone(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 7596 | `PH7_PRIVATE VmOpRc VmExecOpThrow(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 7597 | `PH7_PRIVATE VmOpRc VmExecOpNullcStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 7598 | `PH7_PRIVATE VmOpRc VmExecOpDiv(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 7599 | `PH7_PRIVATE VmOpRc VmExecOpModStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 7600 | `PH7_PRIVATE VmOpRc VmExecOpMod(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 7601 | `PH7_PRIVATE VmOpRc VmExecOpSubStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 7602 | `PH7_PRIVATE VmOpRc VmExecOpSub(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 7603 | `PH7_PRIVATE VmOpRc VmExecOpPowStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 7604 | `PH7_PRIVATE void PH7_MemObjPow(ph7_value *pBase,ph7_value *pExp,ph7_value *pOut);` |
|         - | 7605 | `PH7_PRIVATE VmOpRc VmExecOpLoadMap(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 7606 | `PH7_PRIVATE VmOpRc VmExecOpLoadIdx(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 7607 | `PH7_PRIVATE VmOpRc VmExecOpLoadClosure(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 7608 | `PH7_PRIVATE VmOpRc VmExecOpStoreIdxRef(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 7609 | `PH7_PRIVATE VmOpRc VmExecOpStoreRef(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 7610 | `PH7_PRIVATE VmOpRc VmExecOpMember(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 7611 | `PH7_PRIVATE VmOpRc VmExecOpNew(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 7612 | `PH7_PRIVATE VmOpRc VmExecOpForeachInit(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 7613 | `PH7_PRIVATE VmOpRc VmExecOpForeachStep(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 7614 | `/* vm_error.c — error/diagnostics/type-enforcement machinery shared with vm.c */` |
|         - | 7615 | `PH7_PRIVATE sxi32 PH7_VmErrPhpBit(sxi32 iErr);` |
|         - | 7616 | `PH7_PRIVATE void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|         - | 7617 | `PH7_PRIVATE void PH7_VmActiveFuncName(ph7_vm *pVm,SyBlob *pOut);` |
|         - | 7618 | `PH7_PRIVATE sxi32 VmEnterFrame(ph7_vm *pVm,void *pUserData,ph7_class_instance *pThis,VmFrame **ppFrame);` |
|         - | 7619 | `PH7_PRIVATE void VmExcRelease(ph7_vm *pVm,ph7_exception *pExc);` |
|         - | 7620 | `PH7_PRIVATE void VmClearFramePending(VmFrame *pFrame);` |
|         - | 7621 | `PH7_PRIVATE sxi32 VmDrainCrossedTrys(ph7_vm *pVm,sxu32 nCross,sxu32 nFloor);` |
|         - | 7622 | `PH7_PRIVATE sxi32 VmLocalExecIntoObj(ph7_vm *pVm,SySet *pByteCode,ph7_value **ppMemObj,int bReturnPropagates);` |
|         - | 7623 |  |
|         - | 7624 | `PH7_PRIVATE sxi32 VmArithOperandCheck(ph7_vm *pVm,ph7_value *pLeft,ph7_value *pRight,const char *zOp,SyBlob *pMsgOut);` |
|         - | 7625 | `PH7_PRIVATE const char * VmArithTypeName(ph7_value *pVal);` |
|         - | 7626 | `PH7_PRIVATE const char * VmArithValueName(ph7_value *pVal);` |
|         - | 7627 | `PH7_PRIVATE void VmIncDecTypeErrorMsg(ph7_value *pVal,int bIncr,SyBlob *pMsgOut);` |
|         - | 7628 | `PH7_PRIVATE sxi32 VmUnaryArithOperandCheck(ph7_vm *pVm,ph7_value *pVal,SyBlob *pMsgOut);` |
|         - | 7629 | `PH7_PRIVATE void VmBitNotTypeErrorMsg(ph7_value *pVal,SyBlob *pMsgOut);` |
|         - | 7630 | `PH7_PRIVATE void VmStringBitwise(ph7_value *pLeft,ph7_value *pRight,int cOp,SyBlob *pOut);` |
|         - | 7631 | `PH7_PRIVATE sxi32 VmCheckReadonlyMutate(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 7632 | `PH7_PRIVATE sxi32 VmCheckSetVisibility(ph7_vm *pVm,ph7_class *pOwner,ph7_class_attr *pAttr);` |
|         - | 7633 | `PH7_PRIVATE sxi32 VmThrowNativeNoWrite(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr);` |
|         - | 7634 | `PH7_PRIVATE sxi32 VmThrowNativeNoUnset(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr);` |
|         - | 7635 | `PH7_PRIVATE sxi32 VmThrowNativeReadOnly(ph7_vm *pVm,ph7_class_attr *pAttr);` |
|         - | 7636 | `PH7_PRIVATE void PH7_NativeMarkAttrReadOnly(ph7_class_instance *pThis,const char *zProp);` |
|         - | 7637 | `PH7_PRIVATE sxi32 VmCheckReadonlyUnset(ph7_vm *pVm,ph7_class *pClass,VmClassAttr *pVmAttr);` |
|         - | 7638 | `PH7_PRIVATE sxi32 PH7_VmCheckIndirectModify(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 7639 | `PH7_PRIVATE sxi32 VmCloneApplyUpdate(ph7_vm *pVm,ph7_class_instance *pClone, const char *zName,sxu32 nName,ph7_value *pValue);` |
|         - | 7640 | `PH7_PRIVATE void VmCoalesceDisarm(ph7_vm *pVm);` |
|         - | 7641 | `PH7_PRIVATE sxi32 VmConstCycleThrow(ph7_vm *pVm);` |
|         - | 7642 | `PH7_PRIVATE ph7_class * VmCurrentSelf(ph7_vm *pVm);` |
|         - | 7643 | `PH7_PRIVATE sxi32 VmEnforceConstantType(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue,int bLazy);` |
|         - | 7644 | `PH7_PRIVATE sxi32 VmEnforceTypedDefault(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue);` |
|         - | 7645 | `PH7_PRIVATE sxi32 VmCheckTypedDefault(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue);` |
|         - | 7646 | `PH7_PRIVATE sxi32 VmEnforceArgType(ph7_vm *pVm,ph7_vm_func *pFunc,ph7_vm_func_arg *pFormal,` |
|         - | 7647 | `	sxu32 nArgPos,ph7_value *pVal,int bStrict,ph7_class *pSelfHint);` |
|         - | 7648 | `PH7_PRIVATE int VmClassStaticDeferPending(ph7_class *pClass);` |
|         - | 7649 | `PH7_PRIVATE sxi32 PH7_VmMaterializeClassStatics(ph7_vm *pVm,ph7_class *pClass);` |
|         - | 7650 | `PH7_PRIVATE sxi32 VmEnforceReturnType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue);` |
|         - | 7651 | `PH7_PRIVATE sxi32 VmEnumMaterializeCase(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pCase);` |
|         - | 7652 | `PH7_PRIVATE int VmFinallyAdvance(ph7_vm *pVm, VmInstr *aInstr, int *pnCross, sxu32 *pPc);` |
|         - | 7653 | `PH7_PRIVATE int VmRecursionExceeded(ph7_vm *pVm);` |
|         - | 7654 | `PH7_PRIVATE sxi32 VmRecursionFatal(ph7_vm *pVm);` |
|         - | 7655 | `PH7_PRIVATE int VmValueIsLossyToInt(ph7_value *pVal);` |
|         - | 7656 | `PH7_PRIVATE sxi32 VmRejectFloatOperand(ph7_vm *pVm,ph7_value *pVal);` |
|         - | 7657 | `PH7_PRIVATE sxi32 VmThrowArgNotPassed(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName, ph7_vm_func *pCallee,sxu32 nArg,SyString *pArgName);` |
|         - | 7658 | `PH7_PRIVATE sxi32 VmThrowBuiltinError(ph7_vm *pVm,const char *zClass,sxu32 nClass,SyBlob *pMsg);` |
|         - | 7659 | `PH7_PRIVATE sxi32 VmThrowBuiltinErrorCode(ph7_vm *pVm,const char *zClass,sxu32 nClass,` |
|         - | 7660 | `	SyBlob *pMsg,sxi32 iCode);` |
|         - | 7661 | `PH7_PRIVATE sxi32 VmThrowFixedError(ph7_vm *pVm, const char *zClass, const char *zMsg);` |
|         - | 7662 | `PH7_PRIVATE sxi32 VmThrowFixedErrorCode(ph7_vm *pVm,const char *zClass,sxi32 iCode,` |
|         - | 7663 | `	const char *zMsg);` |
|         - | 7664 | `PH7_PRIVATE sxi32 VmThrowFromVm(ph7_vm *pVm, const char *zClass, const char *zMsg, sxu32 nMsg);` |
|         - | 7665 | `PH7_PRIVATE sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg);` |
|         - | 7666 | `PH7_PRIVATE sxi32 VmThrowSpreadError(ph7_vm *pVm,ph7_value *pBad,int bArgUnpack);` |
|         - | 7667 | `PH7_PRIVATE sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr);` |
|         - | 7668 | `PH7_PRIVATE sxi32 VmThrowAutoInitArrayError(ph7_vm *pVm,VmClassAttr *pVmAttr);` |
|         - | 7669 | `PH7_PRIVATE sxi32 VmAutoInitArrayProperty(ph7_vm *pVm,VmClassAttr *pVmAttr,ph7_value *pSlot);` |
|         - | 7670 | `PH7_PRIVATE sxi32 VmRefUninitTypedProperty(ph7_vm *pVm,ph7_class *pClass,VmClassAttr *pVmAttr,ph7_value *pSlot);` |
|         - | 7671 | `PH7_PRIVATE sxi32 VmUncaughtException(ph7_vm *pVm, ph7_class_instance *pThis);` |
|         - | 7672 | `/* vm_arg_check.c — builtin arity/signature enforcement, called from vm.c */` |
|         - | 7673 | `PH7_PRIVATE void VmSetBuiltinArity(ph7_vm *pVm);` |
|         - | 7674 | `PH7_PRIVATE void VmSetBuiltinSignatures(ph7_vm *pVm);` |
|         - | 7675 | `/* Signature-string derivations, shared with the native-class builder: one` |
|         - | 7676 | ` * PHP-style parameter list ("string $s, int $o = 0") is the single source of a` |
|         - | 7677 | ` * callee's arity bounds and by-ref positions, for a builtin and a native method` |
|         - | 7678 | ` * alike — which is how a native method gets the too-few/too-many ArgumentCountError` |
|         - | 7679 | ` * that a prelude-declared method never had. */` |
|         - | 7680 | `PH7_PRIVATE void VmDeriveArityFromSig(const char *zSig,sxi16 *pnMin,sxu8 *pbAtLeast,sxi16 *pnMax,sxu8 *pbHasMax);` |
|         - | 7681 | `PH7_PRIVATE sxu32 VmDeriveByRefMaskFromSig(const char *zSig);` |
|         - | 7682 | `PH7_PRIVATE int VmBuiltinPrefersRef(SyString *pName);` |
|         - | 7683 | `PH7_PRIVATE sxi32 PH7_VmBindNamedArgsToSig(ph7_context *pCtx,ph7_user_func *pFunc,VmCallArgMap *pMap,int *pnArg,ph7_value **apArg);` |
|         - | 7684 | `PH7_PRIVATE sxi32 VmEnforceBuiltinArgTypes(ph7_context *pCtx,ph7_user_func *pFunc,int nGiven,ph7_value **apArg);` |
|         - | 7685 | `PH7_PRIVATE sxi32 PH7_VmScreenByRefArgShapes(ph7_context *pCtx,ph7_user_func *pFunc,VmCallArgMap *pMap,int nGiven,ph7_value **apArg);` |
|         - | 7686 | `PH7_PRIVATE int PH7_VmSigParamName(const char *zSig,int nPos,SyString *pOut);` |
|         - | 7687 | `PH7_PRIVATE int PH7_VmSigDefaultToValue(ph7_context *pCtx,const char *z,int n,ph7_value *pOut); /* vm_builtin_reflection.c */` |
|         - | 7688 | `PH7_PRIVATE int PH7_VmArgRefusedByRef(VmCallArgMap *pMap,sxu32 nPos,ph7_value *pVal);` |
|         - | 7689 | `PH7_PRIVATE sxi32 PH7_VmResolveDeferredArgs(ph7_vm *pVm,ph7_value *pArg,ph7_value *pTos,` |
|         - | 7690 | `	ph7_vm_func_arg *pFormal,sxu32 nFormal,sxu32 nByRefMask,int bAllByRef,int bAllByValue,` |
|         - | 7691 | `	VmCallArgMap *pCallMap);` |
|         - | 7692 | `PH7_PRIVATE void PH7_VmArgTempCallNotice(ph7_vm *pVm,VmCallArgMap *pMap,sxu32 nPos,ph7_value *pVal);` |
|         - | 7693 | `PH7_PRIVATE int PH7_ArgSatisfiesString(ph7_value *pArg);` |
|         - | 7694 | `PH7_PRIVATE int PH7_ArgIsUnstringableObject(ph7_value *pArg);` |
|         - | 7695 | `PH7_PRIVATE void VmDeprecatedAttrNotice(ph7_vm *pVm,ph7_vm_func *pFunc,ph7_class *pDeclClass);` |
|         - | 7696 | `PH7_PRIVATE void PH7_MarkDeprecatedFunctions(ph7_vm *pVm);` |
|         - | 7697 | `PH7_PRIVATE void PH7_VmDeprecatedCallNotice(ph7_vm *pVm,const ph7_deprecated_name *pDep);` |
|         - | 7698 | `/* vm_exec_ctx.c — Fiber/Generator/Closure engine shared with vm.c */` |
|         - | 7699 | `PH7_PRIVATE sxi32 PH7_VmInstallClosureNative(ph7_vm *pVm);` |
|         - | 7700 | `PH7_PRIVATE sxi32 PH7_VmInstallFiberNative(ph7_vm *pVm);` |
|         - | 7701 | `PH7_PRIVATE sxi32 PH7_VmInstallGeneratorNative(ph7_vm *pVm);` |
|         - | 7702 | `PH7_PRIVATE int vm_builtin_Closure_bindTo(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7703 | `PH7_PRIVATE int vm_builtin_Closure_call(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7704 | `PH7_PRIVATE int vm_builtin_Closure_construct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7705 | `PH7_PRIVATE int vm_builtin_Closure_invoke(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7706 | `PH7_PRIVATE int vm_builtin_Closure_fromCallable(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7707 | `PH7_PRIVATE int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7708 | `PH7_PRIVATE int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7709 | `PH7_PRIVATE int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7710 | `PH7_PRIVATE int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7711 | `PH7_PRIVATE int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7712 | `PH7_PRIVATE int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7713 | `PH7_PRIVATE int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7714 | `PH7_PRIVATE int vm_builtin_Fiber_getCurrent(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7715 | `PH7_PRIVATE int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7716 | `PH7_PRIVATE int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7717 | `PH7_PRIVATE int vm_builtin_Fiber_throw(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7718 | `PH7_PRIVATE int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7719 | `PH7_PRIVATE int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7720 | `PH7_PRIVATE int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7721 | `PH7_PRIVATE int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7722 | `PH7_PRIVATE int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7723 | `PH7_PRIVATE int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7724 | `PH7_PRIVATE int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7725 | `PH7_PRIVATE int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7726 | `PH7_PRIVATE int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7727 | `PH7_PRIVATE int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7728 | `PH7_PRIVATE ph7_class_instance * VmCreateClosure(ph7_vm *pVm, const SyString *pName, ph7_class_instance *pBoundThis, const SyString *pScope);` |
|         - | 7729 | `PH7_PRIVATE ph7_class * VmFccResolveScope(ph7_vm *pVm, ph7_value *pTarget);` |
|         - | 7730 | `PH7_PRIVATE ph7_class * PH7_VmResolveScopeName(ph7_vm *pVm, const char *zCls, sxu32 nCls);` |
|         - | 7731 | `PH7_PRIVATE int PH7_VmIsScopeKeyword(const char *zName,sxu32 nName);` |
|         - | 7732 | `PH7_PRIVATE ph7_class_instance * VmFccWrapValue(ph7_vm *pVm, ph7_value *pValue);` |
|         - | 7733 | `PH7_PRIVATE void PH7_VmByRefArgWriteBack(ph7_vm *pVm,ph7_value *pArg,sxi32 iPreFlags);` |
|         - | 7734 | `PH7_PRIVATE sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx, ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg, int bStrict, ph7_class *pSelfHint, int bCallSiteInMsg, int bAliasByRef);` |
|         - | 7735 | `PH7_PRIVATE ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj);` |
|         - | 7736 | `PH7_PRIVATE int PH7_VmGeneratorIsClosed(ph7_vm *pVm, ph7_class_instance *pThis);` |
|         - | 7737 | `PH7_PRIVATE sxi32 PH7_VmGeneratorPrime(ph7_vm *pVm, ph7_class_instance *pThis);` |
|         - | 7738 | `PH7_PRIVATE ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc);` |
|         - | 7739 | `PH7_PRIVATE ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|         - | 7740 | `PH7_PRIVATE void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|         - | 7741 | `PH7_PRIVATE void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen);` |
|         - | 7742 | `PH7_PRIVATE sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult);` |
|         - | 7743 | `/* vm_include.c function prototypes (rows stay in vm.c's aVmFunc[]) */` |
|         - | 7744 | `PH7_PRIVATE sxi32 VmMountUserClass(ph7_vm *pVm,ph7_class *pClass);` |
|         - | 7745 | `PH7_PRIVATE sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);` |
|         - | 7746 | `PH7_PRIVATE SyString * PH7_VmExecutingUnitFile(ph7_vm *pVm);` |
|         - | 7747 | `PH7_PRIVATE ph7_vm_func * PH7_VmPreludeBuiltinFrame(ph7_vm *pVm,SyString **ppFile,sxu32 *pnLine);` |
|         - | 7748 | `PH7_PRIVATE const char * PH7_CtxDiagFuncName(ph7_context *pCtx,char *zBuf,int nBuf);` |
|         - | 7749 | `PH7_PRIVATE void PH7_VmIncFramePush(ph7_vm *pVm,const char *zName,SyString *pPath);` |
|         - | 7750 | `PH7_PRIVATE void PH7_VmIncFramePop(ph7_vm *pVm);` |
|         - | 7751 | `PH7_PRIVATE sxi32 VmExecDeferredClass(ph7_vm *pVm,VmDeferredClass *pDefer,VmDeferredReq **ppMissing);` |
|         - | 7752 | `PH7_PRIVATE ph7_user_func * PH7_VmMagicCallFunc(ph7_vm *pVm);` |
|         - | 7753 | `PH7_PRIVATE int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7754 | `PH7_PRIVATE int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7755 | `PH7_PRIVATE int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7756 | `PH7_PRIVATE int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7757 | `PH7_PRIVATE int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7758 | `PH7_PRIVATE int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7759 | `PH7_PRIVATE int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7760 | `PH7_PRIVATE int vm_builtin_set_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7761 | `PH7_PRIVATE int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7762 | `PH7_PRIVATE int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7763 | `PH7_PRIVATE int vm_builtin_spl_autoload_extensions(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7764 | `PH7_PRIVATE int vm_builtin_spl_autoload_call(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7765 | `PH7_PRIVATE int vm_builtin_spl_classes(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7766 | `PH7_PRIVATE int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7767 | `PH7_PRIVATE int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7768 | `/* vm_builtin_call.c — callable machinery shared with vm.c's interpreter */` |
|         - | 7769 | `PH7_PRIVATE ph7_class * PH7_VmResolveParentClass(ph7_vm *pVm);` |
|         - | 7770 | `PH7_PRIVATE void VmBoundaryPark(ph7_vm *pVm,sxi32 rc);` |
|         - | 7771 | `/*` |
|         - | 7772 | ` * The status a C->PHP dispatch answers when the callee did NOT return: the` |
|         - | 7773 | ` * builtin driving the loop must abandon it and hand the status straight out.` |
|         - | 7774 | ` * Both members matter and testing only the first is a silent wrong answer —` |
|         - | 7775 | ` * php stops an internal function the moment its callback throws, and an` |
|         - | 7776 | ` * UNCAUGHT throw comes back as PH7_ABORT (VmUncaughtException reports the` |
|         - | 7777 | ` * fatal and answers SXERR_ABORT), not as PH7_EXCEPTION. A loop that tested` |
|         - | 7778 | ` * only PH7_EXCEPTION therefore ran the callback again for every remaining` |
|         - | 7779 | ` * element — repeating its side effects and re-reporting the fatal once per` |
|         - | 7780 | ` * element. Same set VmBoundaryPark parks; see its comment.` |
|         - | 7781 | ` */` |
|         - | 7782 | `#define PH7_CALLBACK_UNWOUND(rc) ((rc) == PH7_EXCEPTION \|\| (rc) == PH7_ABORT)` |
|         - | 7783 | `PH7_PRIVATE sxi32 PH7_VmCallCallbackByValue(ph7_vm *pVm,ph7_value *pFunc,int nArg,` |
|         - | 7784 | `	ph7_value **apArg,ph7_value *pResult,sxu32 nRefOkMask);` |
|         - | 7785 | `PH7_PRIVATE sxi32 VmIterCallMethod(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nLen,ph7_value *pResult);` |
|         - | 7786 | `PH7_PRIVATE sxi32 VmCallClassMethodLsb(ph7_vm *pVm,ph7_class *pCalled,ph7_class_instance *pThis,` |
|         - | 7787 | `	ph7_class_method *pMethod,ph7_value *pResult,int nArg,ph7_value **apArg,VmCallArgMap *pMap);` |
|         - | 7788 | `PH7_PRIVATE sxi32 VmCallClassMethodWithMap(ph7_vm *pVm,ph7_class_instance *pThis,` |
|         - | 7789 | `	ph7_class_method *pMethod,ph7_value *pResult,int nArg,` |
|         - | 7790 | `	ph7_value **apArg,VmCallArgMap *pMap);` |
|         - | 7791 | `PH7_PRIVATE sxi32 VmCallObjectInvoke(ph7_vm *pVm,ph7_class_instance *pThis,` |
|         - | 7792 | `	int nArg,ph7_value **apArg,ph7_value *pResult,VmCallArgMap *pMap);` |
|         - | 7793 | `PH7_PRIVATE sxi32 VmRaiseNotCallable(ph7_vm *pVm,ph7_class_instance *pThis);` |
|         - | 7794 | `PH7_PRIVATE sxi32 PH7_VmForbidDynamicCall(ph7_context *pCtx);` |
|         - | 7795 | `PH7_PRIVATE int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7796 | `PH7_PRIVATE int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7797 | `PH7_PRIVATE int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7798 | `PH7_PRIVATE int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7799 | `PH7_PRIVATE int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7800 | `PH7_PRIVATE int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7801 | `PH7_PRIVATE int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7802 | `PH7_PRIVATE int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7803 | `/* builtin.c function prototypes */` |
|         - | 7804 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm);` |
|         - | 7805 | `/* builtin_hash.c function prototypes */` |
|         - | 7806 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 7807 | `/* Binary-to-hex consumer shared by bin2hex() (builtin.c) and the hash` |
|         - | 7808 | ` * builtins (builtin_hash.c). */` |
|         - | 7809 | `PH7_PRIVATE int HashConsumer(const void *pData,unsigned int nLen,void *pUserData);` |
|         - | 7810 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|         - | 7811 | `PH7_PRIVATE int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7812 | `PH7_PRIVATE int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7813 | `PH7_PRIVATE int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7814 | `PH7_PRIVATE int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7815 | `PH7_PRIVATE int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7816 | `PH7_PRIVATE int PH7_builtin_hash_hmac_algos(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7817 | `PH7_PRIVATE int PH7_builtin_hash_init(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7818 | `PH7_PRIVATE int PH7_builtin_hash_update(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7819 | `PH7_PRIVATE int PH7_builtin_hash_final(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7820 | `PH7_PRIVATE int PH7_builtin_hash_copy(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7821 | `PH7_PRIVATE int PH7_builtin_hash_pbkdf2(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7822 | `PH7_PRIVATE int PH7_builtin_hash_hkdf(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7823 | `PH7_PRIVATE int PH7_builtin_hash_file(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7824 | `PH7_PRIVATE int PH7_builtin_hash_hmac_file(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7825 | `PH7_PRIVATE int PH7_builtin_hash_update_file(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7826 | `PH7_PRIVATE int PH7_builtin_hash_update_stream(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7827 | `PH7_PRIVATE sxi32 PH7_VmInstallHashContext(ph7_vm *pVm);` |
|         - | 7828 | `/* php's one hash_init() flag: request an HMAC rather than a plain digest. */` |
|         - | 7829 | `#define PH7_HASH_HMAC 1` |
|         - | 7830 | `PH7_PRIVATE int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7831 | `PH7_PRIVATE int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7832 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|         - | 7833 | `PH7_PRIVATE int PH7_builtin_crypt(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7834 | `PH7_PRIVATE int PH7_builtin_password_algos(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7835 | `PH7_PRIVATE int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7836 | `PH7_PRIVATE int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7837 | `PH7_PRIVATE int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7838 | `PH7_PRIVATE int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7839 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 7840 | `/* builtin_fmt.c function prototypes (PH7_NEED_FMT_AND_INI: compiled whenever` |
|         - | 7841 | ` * disk I/O is enabled, independently of PH7_DISABLE_BUILTIN_FUNC) */` |
|         - | 7842 | `#ifndef PH7_DISABLE_DISK_IO` |
|         - | 7843 | `PH7_PRIVATE int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7844 | `PH7_PRIVATE int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7845 | `PH7_PRIVATE int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7846 | `PH7_PRIVATE int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7847 | `#endif /* PH7_DISABLE_DISK_IO */` |
|         - | 7848 | `#include <signal.h>   /* sig_atomic_t: PH7_PcntlAsyncPending, read at the fetch point */` |
|         - | 7849 | `/*` |
|         - | 7850 | ` * ext/pcntl (builtin_pcntl.c). These four are OUTSIDE every guard because the` |
|         - | 7851 | ` * engine links against them in every build: vm_exec.c reads the flag at its` |
|         - | 7852 | ` * fetch point, vm.c registers the constants and releases the state. Where the` |
|         - | 7853 | ` * extension is not compiled in -- Windows, or a build with no builtins -- the` |
|         - | 7854 | ` * flag is simply always zero and the three functions are no-ops.` |
|         - | 7855 | ` */` |
|         - | 7856 | `extern volatile sig_atomic_t PH7_PcntlAsyncPending;` |
|         - | 7857 | `PH7_PRIVATE void PH7_PcntlDrainAsync(ph7_vm *pVm);` |
|         - | 7858 | `PH7_PRIVATE void PH7_PcntlVmRelease(ph7_vm *pVm);` |
|         - | 7859 | `PH7_PRIVATE void PH7_RegisterPcntlConstants(ph7_vm *pVm);` |
|         - | 7860 | `PH7_PRIVATE sxi32 PH7_VmInstallPcntl(ph7_vm *pVm);` |
|         - | 7861 | ``/* Its uncatchable `Error installing signal handler for %d`, which lives with the`` |
|         - | 7862 | ` * engine's other clean-halt fatals in vm_error.c rather than with the extension. */` |
|         - | 7863 | `PH7_PRIVATE sxi32 PH7_VmSignalInstallFatal(ph7_vm *pVm,int signo);` |
|         - | 7864 | `#if defined(PH7_ENABLE_THREADS)` |
|         - | 7865 | `/* Drop this process to single-threaded mode in the CHILD of a fork() (api.c).` |
|         - | 7866 | ` * Declared beside pcntl's names because pcntl_fork() is its only caller, but it` |
|         - | 7867 | ` * belongs to the library core and is built wherever threads are. */` |
|         - | 7868 | `PH7_PRIVATE void PH7_LibForkChild(void);` |
|         - | 7869 | `#endif` |
|         - | 7870 | `/* php's syslog trio (builtin_syslog.c). Not an extension -- ext/standard, and` |
|         - | 7871 | ` * therefore present on every platform php is. */` |
|         - | 7872 | `PH7_PRIVATE void PH7_RegisterSyslogConstants(ph7_vm *pVm);` |
|         - | 7873 | `PH7_PRIVATE void PH7_SyslogVmRelease(ph7_vm *pVm);` |
|         - | 7874 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 7875 | `PH7_PRIVATE int PH7_builtin_openlog(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7876 | `PH7_PRIVATE int PH7_builtin_syslog(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7877 | `PH7_PRIVATE int PH7_builtin_closelog(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7878 | `#endif` |
|         - | 7879 | `/* builtin_parse.c function prototypes */` |
|         - | 7880 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 7881 | `/* HTML entity escape engine: shared by the htmlspecialchars/htmlentities` |
|         - | 7882 | ` * family (builtin.c) and filter_var's SANITIZE filters (builtin_parse.c). */` |
|         - | 7883 | `/* The charsets the HTML entity family models: php's own UTF-8 and ISO-8859-1` |
|         - | 7884 | ` * (one byte per character, its VALUE the code point). Everything else keeps` |
|         - | 7885 | ` * php's unsupported-charset warning. */` |
|         - | 7886 | `#define PH7_HTML_CS_UTF8   0` |
|         - | 7887 | `#define PH7_HTML_CS_LATIN1 1` |
|         - | 7888 | `PH7_PRIVATE void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,int iFlags,int bAll,int bDoubleEncode,int iCs);` |
|         - | 7889 | `PH7_PRIVATE void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,int iFlags,int bFull,int iCs);` |
|         - | 7890 | `PH7_PRIVATE int HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx);` |
|         - | 7891 | `PH7_PRIVATE void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags,int iCs);` |
|         - | 7892 | `PH7_PRIVATE int PH7_builtin_filter_var(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7893 | `PH7_PRIVATE int PH7_builtin_filter_input(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7894 | `PH7_PRIVATE int PH7_builtin_filter_list(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7895 | `PH7_PRIVATE int PH7_builtin_filter_id(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7896 | `PH7_PRIVATE int PH7_builtin_filter_has_var(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7897 | `PH7_PRIVATE int PH7_builtin_filter_var_array(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7898 | `PH7_PRIVATE int PH7_builtin_filter_input_array(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7899 | `PH7_PRIVATE int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7900 | `PH7_PRIVATE int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7901 | `PH7_PRIVATE int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7902 | `PH7_PRIVATE int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7903 | `PH7_PRIVATE int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7904 | `PH7_PRIVATE int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7905 | `PH7_PRIVATE int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7906 | `PH7_PRIVATE int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7907 | `PH7_PRIVATE int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7908 | `PH7_PRIVATE int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7909 | `PH7_PRIVATE int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7910 | `PH7_PRIVATE int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7911 | `PH7_PRIVATE int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7912 | `PH7_PRIVATE int PH7_builtin_convert_uuencode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7913 | `PH7_PRIVATE int PH7_builtin_convert_uudecode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7914 | `PH7_PRIVATE int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7915 | `PH7_PRIVATE int PH7_builtin_rawurlencode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7916 | `PH7_PRIVATE int PH7_builtin_http_build_query(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7917 | `PH7_PRIVATE int PH7_builtin_parse_str(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7918 | `PH7_PRIVATE int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7919 | `PH7_PRIVATE int PH7_builtin_rawurldecode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7920 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 7921 | `#ifndef PH7_DISABLE_DISK_IO` |
|         - | 7922 | `PH7_PRIVATE int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7923 | `PH7_PRIVATE int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7924 | `PH7_PRIVATE int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7925 | `#endif /* PH7_DISABLE_DISK_IO */` |
|         - | 7926 | `/* builtin_string.c function prototypes */` |
|         - | 7927 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 7928 | `PH7_PRIVATE int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7929 | `PH7_PRIVATE int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7930 | `PH7_PRIVATE int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7931 | `PH7_PRIVATE int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7932 | `PH7_PRIVATE int PH7_builtin_chunk_split(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7933 | `PH7_PRIVATE int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7934 | `PH7_PRIVATE int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7935 | `PH7_PRIVATE int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7936 | `PH7_PRIVATE int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7937 | `PH7_PRIVATE int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7938 | `PH7_PRIVATE int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7939 | `PH7_PRIVATE int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7940 | `PH7_PRIVATE int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7941 | `PH7_PRIVATE int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7942 | `PH7_PRIVATE int PH7_builtin_levenshtein(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7943 | `PH7_PRIVATE int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7944 | `PH7_PRIVATE int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7945 | `PH7_PRIVATE int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7946 | `PH7_PRIVATE int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7947 | `PH7_PRIVATE int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7948 | `PH7_PRIVATE int PH7_builtin_similar_text(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7949 | `PH7_PRIVATE int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7950 | `PH7_PRIVATE int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7951 | `PH7_PRIVATE int PH7_builtin_str_rot13(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7952 | `PH7_PRIVATE int PH7_builtin_metaphone(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7953 | `/* builtin_pack.c — the binary-string pair */` |
|         - | 7954 | `PH7_PRIVATE int PH7_builtin_pack(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7955 | `PH7_PRIVATE int PH7_builtin_unpack(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7956 | `/* builtin_bcmath.c -- ext/bcmath: arbitrary-precision decimal arithmetic */` |
|         - | 7957 | `PH7_PRIVATE int PH7_builtin_bcadd(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7958 | `PH7_PRIVATE int PH7_builtin_bcsub(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7959 | `PH7_PRIVATE int PH7_builtin_bcmul(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7960 | `PH7_PRIVATE int PH7_builtin_bccomp(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7961 | `PH7_PRIVATE int PH7_builtin_bcdiv(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7962 | `PH7_PRIVATE int PH7_builtin_bcmod(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7963 | `PH7_PRIVATE int PH7_builtin_bcdivmod(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7964 | `PH7_PRIVATE int PH7_builtin_bcpow(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7965 | `PH7_PRIVATE int PH7_builtin_bcpowmod(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7966 | `PH7_PRIVATE int PH7_builtin_bcsqrt(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7967 | `PH7_PRIVATE int PH7_builtin_bcround(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7968 | `PH7_PRIVATE int PH7_builtin_bcfloor(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7969 | `PH7_PRIVATE int PH7_builtin_bcceil(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7970 | `PH7_PRIVATE sxi32 PH7_VmInstallBcMath(ph7_vm *pVm);` |
|         - | 7971 | `PH7_PRIVATE int PH7_builtin_bcscale(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7972 | `/* builtin_calendar.c -- ext/calendar: the serial day number and its calendars */` |
|         - | 7973 | `PH7_PRIVATE int PH7_builtin_gregoriantojd(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7974 | `PH7_PRIVATE int PH7_builtin_jdtogregorian(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7975 | `PH7_PRIVATE int PH7_builtin_juliantojd(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7976 | `PH7_PRIVATE int PH7_builtin_jdtojulian(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7977 | `PH7_PRIVATE int PH7_builtin_frenchtojd(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7978 | `PH7_PRIVATE int PH7_builtin_jdtofrench(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7979 | `PH7_PRIVATE int PH7_builtin_jewishtojd(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7980 | `PH7_PRIVATE int PH7_builtin_jdtojewish(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7981 | `PH7_PRIVATE int PH7_builtin_cal_info(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7982 | `/* posix (builtin_posix.c) -- php builds no ext/posix on Windows, and neither` |
|         - | 7983 | ` * does this, so every name below is absent there. */` |
|         - | 7984 | `#ifndef __WINNT__` |
|         - | 7985 | `/* ext/pcntl's own builtins (builtin_pcntl.c); the names the engine links against` |
|         - | 7986 | ` * in EVERY build are declared above, outside both guards. */` |
|         - | 7987 | `PH7_PRIVATE int PH7_builtin_pcntl_fork(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7988 | `PH7_PRIVATE int PH7_builtin_pcntl_waitpid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7989 | `PH7_PRIVATE int PH7_builtin_pcntl_waitid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7990 | `PH7_PRIVATE int PH7_builtin_pcntl_wait(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7991 | `PH7_PRIVATE int PH7_builtin_pcntl_signal(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7992 | `PH7_PRIVATE int PH7_builtin_pcntl_signal_get_handler(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7993 | `PH7_PRIVATE int PH7_builtin_pcntl_signal_dispatch(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7994 | `PH7_PRIVATE int PH7_builtin_pcntl_sigprocmask(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7995 | `PH7_PRIVATE int PH7_builtin_pcntl_sigwaitinfo(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7996 | `PH7_PRIVATE int PH7_builtin_pcntl_sigtimedwait(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7997 | `PH7_PRIVATE int PH7_builtin_pcntl_wifexited(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7998 | `PH7_PRIVATE int PH7_builtin_pcntl_wifstopped(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7999 | `PH7_PRIVATE int PH7_builtin_pcntl_wifcontinued(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8000 | `PH7_PRIVATE int PH7_builtin_pcntl_wifsignaled(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8001 | `PH7_PRIVATE int PH7_builtin_pcntl_wexitstatus(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8002 | `PH7_PRIVATE int PH7_builtin_pcntl_wtermsig(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8003 | `PH7_PRIVATE int PH7_builtin_pcntl_wstopsig(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8004 | `PH7_PRIVATE int PH7_builtin_pcntl_exec(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8005 | `PH7_PRIVATE int PH7_builtin_pcntl_alarm(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8006 | `PH7_PRIVATE int PH7_builtin_pcntl_get_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8007 | `PH7_PRIVATE int PH7_builtin_pcntl_getpriority(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8008 | `PH7_PRIVATE int PH7_builtin_pcntl_setpriority(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8009 | `PH7_PRIVATE int PH7_builtin_pcntl_strerror(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8010 | `PH7_PRIVATE int PH7_builtin_pcntl_async_signals(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8011 | `#ifdef __linux__` |
|         - | 8012 | `PH7_PRIVATE int PH7_builtin_pcntl_unshare(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8013 | `PH7_PRIVATE int PH7_builtin_pcntl_getcpuaffinity(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8014 | `PH7_PRIVATE int PH7_builtin_pcntl_setcpuaffinity(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8015 | `PH7_PRIVATE int PH7_builtin_pcntl_getcpu(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8016 | `#endif` |
|         - | 8017 | `PH7_PRIVATE int PH7_builtin_posix_kill(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8018 | `PH7_PRIVATE int PH7_builtin_posix_getpid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8019 | `PH7_PRIVATE int PH7_builtin_posix_getppid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8020 | `PH7_PRIVATE int PH7_builtin_posix_getuid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8021 | `PH7_PRIVATE int PH7_builtin_posix_setuid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8022 | `PH7_PRIVATE int PH7_builtin_posix_geteuid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8023 | `PH7_PRIVATE int PH7_builtin_posix_seteuid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8024 | `PH7_PRIVATE int PH7_builtin_posix_getgid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8025 | `PH7_PRIVATE int PH7_builtin_posix_setgid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8026 | `PH7_PRIVATE int PH7_builtin_posix_getegid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8027 | `PH7_PRIVATE int PH7_builtin_posix_setegid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8028 | `PH7_PRIVATE int PH7_builtin_posix_getgroups(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8029 | `PH7_PRIVATE int PH7_builtin_posix_getlogin(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8030 | `PH7_PRIVATE int PH7_builtin_posix_getpgrp(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8031 | `PH7_PRIVATE int PH7_builtin_posix_setsid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8032 | `PH7_PRIVATE int PH7_builtin_posix_setpgid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8033 | `PH7_PRIVATE int PH7_builtin_posix_getpgid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8034 | `PH7_PRIVATE int PH7_builtin_posix_getsid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8035 | `PH7_PRIVATE int PH7_builtin_posix_uname(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8036 | `PH7_PRIVATE int PH7_builtin_posix_times(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8037 | `PH7_PRIVATE int PH7_builtin_posix_ctermid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8038 | `PH7_PRIVATE int PH7_builtin_posix_ttyname(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8039 | `PH7_PRIVATE int PH7_builtin_posix_isatty(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8040 | `PH7_PRIVATE int PH7_builtin_posix_getcwd(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8041 | `PH7_PRIVATE int PH7_builtin_posix_mkfifo(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8042 | `PH7_PRIVATE int PH7_builtin_posix_mknod(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8043 | `PH7_PRIVATE int PH7_builtin_posix_access(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8044 | `PH7_PRIVATE int PH7_builtin_posix_eaccess(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8045 | `PH7_PRIVATE int PH7_builtin_posix_getgrnam(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8046 | `PH7_PRIVATE int PH7_builtin_posix_getgrgid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8047 | `PH7_PRIVATE int PH7_builtin_posix_getpwnam(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8048 | `PH7_PRIVATE int PH7_builtin_posix_getpwuid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8049 | `PH7_PRIVATE int PH7_builtin_posix_getrlimit(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8050 | `PH7_PRIVATE int PH7_builtin_posix_setrlimit(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8051 | `PH7_PRIVATE int PH7_builtin_posix_get_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8052 | `PH7_PRIVATE int PH7_builtin_posix_strerror(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8053 | `PH7_PRIVATE int PH7_builtin_posix_initgroups(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8054 | `PH7_PRIVATE int PH7_builtin_posix_sysconf(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8055 | `PH7_PRIVATE int PH7_builtin_posix_pathconf(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8056 | `PH7_PRIVATE int PH7_builtin_posix_fpathconf(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8057 | `#endif /* __WINNT__ */` |
|         - | 8058 | `/* fileinfo (builtin_fileinfo.c) -- php's ext/fileinfo, over PHL's own signature` |
|         - | 8059 | ` * table rather than a magic database file. */` |
|         - | 8060 | `#ifdef PH7_ENABLE_ZLIB` |
|         - | 8061 | `/* vm_openssl.c / vm_openssl_x509.c */` |
|         - | 8062 | `PH7_PRIVATE sxi32 PH7_VmInstallOpenSsl(ph7_vm *pVm);` |
|         - | 8063 | `PH7_PRIVATE const ph7_builtin_func * PH7_OpenSslFuncTable(sxu32 *pnEntry);` |
|         - | 8064 | `PH7_PRIVATE sxi32 PH7_VmInstallOpenSslX509(ph7_vm *pVm);` |
|         - | 8065 | `PH7_PRIVATE const ph7_builtin_func * PH7_OpenSslX509FuncTable(sxu32 *pnEntry);` |
|         - | 8066 | `PH7_PRIVATE void PH7_SslVmReset(ph7_vm *pVm);` |
|         - | 8067 | `PH7_PRIVATE void PH7_SslVmRelease(ph7_vm *pVm);` |
|         - | 8068 | `/* ext/zip: the ZipArchive class, its ten deprecated procedural verbs and the` |
|         - | 8069 | `` * read-only `zip://` wrapper. It rides ext/zlib's build guard because php's own`` |
|         - | 8070 | ` * requires zlib -- a deflated member is the format's normal case. */` |
|         - | 8071 | `#ifdef PH7_ENABLE_ZLIB` |
|         - | 8072 | `PH7_PRIVATE sxi32 PH7_VmInstallZip(ph7_vm *pVm);` |
|         - | 8073 | `PH7_PRIVATE const ph7_builtin_func * PH7_ZipFuncTable(sxu32 *pnEntry);` |
|         - | 8074 | `PH7_PRIVATE int PH7_ZipStreamIs(const ph7_io_stream *pStream);` |
|         - | 8075 | `PH7_PRIVATE int PH7_ZipStreamViaWrapper(void *pHandle);` |
|         - | 8076 | `#endif` |
|         - | 8077 | `PH7_PRIVATE void PH7_ZipVmReset(ph7_vm *pVm);` |
|         - | 8078 | `PH7_PRIVATE void PH7_ZipVmRelease(ph7_vm *pVm);` |
|         - | 8079 | `/* The name get_resource_type() gives one of ext/zip's two procedural handles,` |
|         - | 8080 | ` * or 0 when the resource is not one of them. */` |
|         - | 8081 | `PH7_PRIVATE const char * PH7_ZipResourceType(void *pResource);` |
|         - | 8082 | `PH7_PRIVATE sxi32 PH7_VmInstallZlib(ph7_vm *pVm);` |
|         - | 8083 | `PH7_PRIVATE const ph7_builtin_func * PH7_ZlibFuncTable(sxu32 *pnEntry);` |
|         - | 8084 | `PH7_PRIVATE void PH7_ZlibVmReset(ph7_vm *pVm);` |
|         - | 8085 | `PH7_PRIVATE void PH7_ZlibVmRelease(ph7_vm *pVm);` |
|         - | 8086 | `PH7_PRIVATE void PH7_ZlibArmOpen(ph7_vm *pVm,int iLevel,int iStrategy);` |
|         - | 8087 | `PH7_PRIVATE void PH7_ZlibArmDirect(ph7_vm *pVm,int bDirect);` |
|         - | 8088 | `PH7_PRIVATE int PH7_ZlibStreamIs(const ph7_io_stream *pStream);` |
|         - | 8089 | `PH7_PRIVATE int PH7_ZlibFilterCreate(phl_stream_filter *pFilter,ph7_value *pParams);` |
|         - | 8090 | `PH7_PRIVATE int PH7_ZlibFilterRun(phl_stream_filter *pFilter,phl_brigade *pIn,` |
|         - | 8091 | `	phl_brigade *pOut,int iFlags);` |
|         - | 8092 | `PH7_PRIVATE void PH7_ZlibFilterClose(phl_stream_filter *pFilter);` |
|         - | 8093 | `extern const ph7_io_stream sZLIB_Stream;` |
|         - | 8094 | `extern const ph7_io_stream sZIP_Stream;` |
|         - | 8095 | `#endif` |
|         - | 8096 | `PH7_PRIVATE void PH7_VmAddResponseHeader(ph7_vm *pVm,const char *zName,const char *zValue);` |
|         - | 8097 | `PH7_PRIVATE sxi32 PH7_VmInstallPhar(ph7_vm *pVm);` |
|         - | 8098 | `PH7_PRIVATE int PH7_PharStreamIs(const ph7_io_stream *pStream);` |
|         - | 8099 | `PH7_PRIVATE int PH7_PharUrlStat(ph7_vm *pVm,const char *zPath,ph7_int64 *aVal);` |
|         - | 8100 | `PH7_PRIVATE int PH7_PharCanonicalUrl(ph7_vm *pVm,const char *zPath,int nPath,SyBlob *pOut);` |
|         - | 8101 | `/* What PH7_PharPathOp() was asked to do. Mirrors vfs.c's VFS_POP_* codes, which` |
|         - | 8102 | ` * are file-local. */` |
|         - | 8103 | `#define PHAR_PATHOP_UNLINK 0` |
|         - | 8104 | `#define PHAR_PATHOP_RENAME 1` |
|         - | 8105 | `#define PHAR_PATHOP_MKDIR  2` |
|         - | 8106 | `#define PHAR_PATHOP_CHMOD  3` |
|         - | 8107 | `#define PHAR_PATHOP_RMDIR  4` |
|         - | 8108 | `PH7_PRIVATE int PH7_PharPathOp(ph7_context *pCtx,const char *zPath,const char *zDest,int eOp);` |
|         - | 8109 | `PH7_PRIVATE int PH7_VmSerializeValue(ph7_context *pCtx,ph7_value *pIn,SyBlob *pOut);` |
|         - | 8110 | `extern const ph7_io_stream sPHAR_Stream;` |
|         - | 8111 | `PH7_PRIVATE sxi32 PH7_VmInstallFileinfo(ph7_vm *pVm);` |
|         - | 8112 | `PH7_PRIVATE int PH7_builtin_finfo_open(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8113 | `PH7_PRIVATE int PH7_builtin_finfo_close(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8114 | `PH7_PRIVATE int PH7_builtin_finfo_set_flags(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8115 | `PH7_PRIVATE int PH7_builtin_finfo_file(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8116 | `PH7_PRIVATE int PH7_builtin_finfo_buffer(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8117 | `PH7_PRIVATE int PH7_builtin_mime_content_type(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8118 | `/* gettext (builtin_gettext.c) */` |
|         - | 8119 | `PH7_PRIVATE int PH7_builtin_textdomain(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8120 | `PH7_PRIVATE int PH7_builtin_bindtextdomain(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8121 | `PH7_PRIVATE int PH7_builtin_bind_textdomain_codeset(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8122 | `PH7_PRIVATE int PH7_builtin_gettext(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8123 | `PH7_PRIVATE int PH7_builtin_dgettext(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8124 | `PH7_PRIVATE int PH7_builtin_dcgettext(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8125 | `PH7_PRIVATE int PH7_builtin_ngettext(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8126 | `PH7_PRIVATE int PH7_builtin_dngettext(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8127 | `PH7_PRIVATE int PH7_builtin_dcngettext(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8128 | `PH7_PRIVATE int PH7_builtin_cal_days_in_month(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8129 | `PH7_PRIVATE int PH7_builtin_cal_to_jd(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8130 | `PH7_PRIVATE int PH7_builtin_cal_from_jd(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8131 | `PH7_PRIVATE int PH7_builtin_jddayofweek(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8132 | `PH7_PRIVATE int PH7_builtin_jdmonthname(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8133 | `PH7_PRIVATE int PH7_builtin_unixtojd(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8134 | `PH7_PRIVATE int PH7_builtin_jdtounix(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8135 | `PH7_PRIVATE int PH7_builtin_easter_days(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8136 | `PH7_PRIVATE int PH7_builtin_easter_date(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8137 | `/* builtin_image.c -- ext/standard's image surface: the IMAGETYPE_* space and` |
|         - | 8138 | ` * the container readers behind getimagesize(). */` |
|         - | 8139 | `PH7_PRIVATE const char * PH7_ImageTypeMime(ph7_int64 iType);` |
|         - | 8140 | `PH7_PRIVATE int PH7_builtin_image_type_to_mime_type(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8141 | `PH7_PRIVATE int PH7_builtin_image_type_to_extension(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8142 | `PH7_PRIVATE int PH7_builtin_getimagesize(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8143 | `PH7_PRIVATE int PH7_builtin_getimagesizefromstring(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8144 | `/* builtin_scanf.c -- sscanf() and the scanner fscanf() (vfs_stream.c) shares. */` |
|         - | 8145 | `PH7_PRIVATE sxi32 PH7_ScanfRun(ph7_context *pCtx,const char *zStr,int nStr,` |
|         - | 8146 | `	const char *zFmt,int nFmt,ph7_value **apVar,int nVar);` |
|         - | 8147 | `PH7_PRIVATE int PH7_builtin_sscanf(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8148 | `PH7_PRIVATE int PH7_builtin_fscanf(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8149 | `PH7_PRIVATE int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8150 | `PH7_PRIVATE int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8151 | `PH7_PRIVATE int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8152 | `PH7_PRIVATE int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8153 | `PH7_PRIVATE int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8154 | `PH7_PRIVATE int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8155 | `PH7_PRIVATE int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8156 | `PH7_PRIVATE int PH7_builtin_stripcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8157 | `PH7_PRIVATE int PH7_builtin_quoted_printable_encode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8158 | `PH7_PRIVATE int PH7_builtin_quoted_printable_decode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8159 | `PH7_PRIVATE int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8160 | `PH7_PRIVATE int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8161 | `PH7_PRIVATE int PH7_builtin_strnatcmp(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8162 | `PH7_PRIVATE int PH7_builtin_version_compare(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8163 | `PH7_PRIVATE int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8164 | `PH7_PRIVATE int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8165 | `PH7_PRIVATE int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8166 | `PH7_PRIVATE int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8167 | `PH7_PRIVATE int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8168 | `PH7_PRIVATE int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8169 | `PH7_PRIVATE int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8170 | `PH7_PRIVATE int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8171 | `PH7_PRIVATE int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8172 | `PH7_PRIVATE int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8173 | `PH7_PRIVATE int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8174 | `PH7_PRIVATE int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8175 | `PH7_PRIVATE int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8176 | `PH7_PRIVATE int PH7_builtin_count_chars(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8177 | `PH7_PRIVATE int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8178 | `PH7_PRIVATE int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8179 | `PH7_PRIVATE int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8180 | `PH7_PRIVATE int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8181 | `PH7_PRIVATE int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8182 | `PH7_PRIVATE int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8183 | `PH7_PRIVATE int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8184 | `PH7_PRIVATE int PH7_builtin_str_word_count(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8185 | `PH7_PRIVATE int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8186 | `PH7_PRIVATE int PH7_builtin_substr_compare(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8187 | `PH7_PRIVATE int PH7_builtin_substr_count(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8188 | `PH7_PRIVATE int PH7_builtin_substr_replace(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8189 | `PH7_PRIVATE int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8190 | `PH7_PRIVATE int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8191 | `PH7_PRIVATE int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8192 | `PH7_PRIVATE int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8193 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 8194 | `/* hashmap.c function prototypes */` |
|         - | 8195 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(ph7_vm *pVm,sxu32 (*xIntHash)(sxi64),sxu32 (*xBlobHash)(const void *,sxu32));` |
|         - | 8196 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm);` |
|         - | 8197 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS);` |
|         - | 8198 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap);` |
|         - | 8199 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(ph7_hashmap *pMap,ph7_value *pKey,ph7_hashmap_node **ppNode);` |
|         - | 8200 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(ph7_hashmap *pMap,ph7_value *pKey,ph7_value *pVal);` |
|         - | 8201 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(ph7_hashmap *pMap,ph7_value *pKey,sxu32 nRefIdx);` |
|         - | 8202 | `PH7_PRIVATE sxi32 PH7_HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest);` |
|         - | 8203 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight);` |
|         - | 8204 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore);` |
|         - | 8205 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest);` |
|         - | 8206 | `PH7_PRIVATE sxi32 PH7_HashmapDupMaterialized(ph7_hashmap *pSrc,ph7_hashmap *pDest);` |
|         - | 8207 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue);` |
|         - | 8208 | `PH7_PRIVATE sxi32 PH7_HashmapCmp(ph7_hashmap *pLeft,ph7_hashmap *pRight,int bStrict,int iNest);` |
|         - | 8209 | `PH7_PRIVATE void PH7_HashmapRegisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep);` |
|         - | 8210 | `PH7_PRIVATE void PH7_HashmapUnregisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep);` |
|         - | 8211 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap);` |
|         - | 8212 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore);` |
|         - | 8213 | `PH7_PRIVATE int PH7_HashmapNodeIsRef(ph7_hashmap_node *pNode);` |
|         - | 8214 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey);` |
|         - | 8215 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm);` |
|         - | 8216 | `/* hashmap.c engine helpers shared with hashmap_sort.c / hashmap_builtin.c */` |
|         - | 8217 | `PH7_PRIVATE ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode);` |
|         - | 8218 | `PH7_PRIVATE sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict,int iNest);` |
|         - | 8219 | `PH7_PRIVATE void HashmapRehashIntNode(ph7_hashmap_node *pEntry);` |
|         - | 8220 | `PH7_PRIVATE sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected);` |
|         - | 8221 | `PH7_PRIVATE sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve);` |
|         - | 8222 | `PH7_PRIVATE int HashmapFindValue(ph7_hashmap *pMap,ph7_value *pNeedle,ph7_hashmap_node **ppNode,int bStrict);` |
|         - | 8223 | `PH7_PRIVATE int HashmapValueStrEq(ph7_value *pA,ph7_value *pB,int bUserVisible,sxi32 *pRc);` |
|         - | 8224 | `PH7_PRIVATE sxi32 HashmapStringifyElems(ph7_hashmap *pMap);` |
|         - | 8225 | `PH7_PRIVATE int HashmapFindStringValue(ph7_hashmap *pMap,ph7_value *pNeedle,ph7_hashmap_node **ppNode,sxi32 *pRc);` |
|         - | 8226 | `PH7_PRIVATE sxi32 HashmapOverwrite(ph7_hashmap *pSrc,ph7_hashmap *pDest);` |
|         - | 8227 | `PH7_PRIVATE sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest);` |
|         - | 8228 | `PH7_PRIVATE sxi32 HashmapInsert(ph7_hashmap *pMap,ph7_value *pKey,ph7_value *pVal);` |
|         - | 8229 | `PH7_PRIVATE sxi32 HashmapLookupIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_hashmap_node **ppNode);` |
|         - | 8230 | `PH7_PRIVATE sxi32 HashmapLookupBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_hashmap_node **ppNode);` |
|         - | 8231 | `PH7_PRIVATE sxi32 PH7_HashmapInsertRawKey(ph7_hashmap *pMap,const char *zKey,sxu32 nKey,ph7_value *pVal);` |
|         - | 8232 | `/* hashmap_sort.c: the node ordering primitive and the sort builtin family.` |
|         - | 8233 | ` * Shared with hashmap.c (shuffle/array_unique/array_rand) and referenced from` |
|         - | 8234 | ` * the aHashmapFunc[] registration table; compiled in every mode. */` |
|         - | 8235 | `typedef sxi32 (*ProcNodeCmp)(ph7_hashmap_node *,ph7_hashmap_node *,void *);` |
|         - | 8236 | `/* One entry as an ordering sees it: the node, plus where it stood before the` |
|         - | 8237 | ` * sort ran. php stamps every bucket the same way (Z_EXTRA), and the stamp is` |
|         - | 8238 | ` * what makes a quicksort stable for the sort builtins and what lets the` |
|         - | 8239 | ` * diff/intersect family answer in the source array's order. */` |
|         - | 8240 | `typedef struct HashmapSortEnt HashmapSortEnt;` |
|         - | 8241 | `struct HashmapSortEnt {` |
|         - | 8242 | `	ph7_hashmap_node *pNode;` |
|         - | 8243 | `	sxu32 nOrd;` |
|         - | 8244 | `};` |
|         - | 8245 | `PH7_PRIVATE void PH7_HashmapSortEntVector(HashmapSortEnt *aEnt,sxu32 n,ProcNodeCmp xCmp,void *pCmpData);` |
|         - | 8246 | `PH7_PRIVATE sxi32 PH7_HashmapShuffle(ph7_hashmap *pMap);` |
|         - | 8247 | `PH7_PRIVATE sxi32 HashmapNodeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData);` |
|         - | 8248 | `PH7_PRIVATE void HashmapSortRehash(ph7_hashmap *pMap);` |
|         - | 8249 | `PH7_PRIVATE int HashmapValueFlagEqual(ph7_vm *pVm,ph7_value *pA,ph7_value *pB,int base,int bFold);` |
|         - | 8250 | `PH7_PRIVATE int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8251 | `PH7_PRIVATE int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8252 | `PH7_PRIVATE int ph7_hashmap_natsort(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8253 | `PH7_PRIVATE int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8254 | `PH7_PRIVATE int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8255 | `PH7_PRIVATE int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8256 | `PH7_PRIVATE int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8257 | `PH7_PRIVATE int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8258 | `PH7_PRIVATE int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8259 | `PH7_PRIVATE int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8260 | `/* hashmap_builtin.c function prototypes (the array_* family; referenced from` |
|         - | 8261 | ` * the aHashmapFunc[] registration table in hashmap.c; compiled in every mode) */` |
|         - | 8262 | `PH7_PRIVATE int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8263 | `PH7_PRIVATE int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8264 | `PH7_PRIVATE int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8265 | `PH7_PRIVATE int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8266 | `PH7_PRIVATE int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8267 | `PH7_PRIVATE int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8268 | `PH7_PRIVATE int ph7_hashmap_unshift(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8269 | `PH7_PRIVATE int ph7_hashmap_merge_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8270 | `PH7_PRIVATE int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8271 | `PH7_PRIVATE int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8272 | `PH7_PRIVATE int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8273 | `PH7_PRIVATE int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8274 | `PH7_PRIVATE int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8275 | `PH7_PRIVATE int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8276 | `PH7_PRIVATE int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8277 | `PH7_PRIVATE int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8278 | `PH7_PRIVATE int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8279 | `PH7_PRIVATE int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8280 | `PH7_PRIVATE int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8281 | `PH7_PRIVATE int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8282 | `PH7_PRIVATE int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8283 | `PH7_PRIVATE int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8284 | `PH7_PRIVATE int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8285 | `PH7_PRIVATE int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8286 | `PH7_PRIVATE int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8287 | `PH7_PRIVATE int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8288 | `PH7_PRIVATE int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8289 | `PH7_PRIVATE int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8290 | `PH7_PRIVATE int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8291 | `PH7_PRIVATE int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8292 | `PH7_PRIVATE int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8293 | `PH7_PRIVATE int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8294 | `PH7_PRIVATE int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8295 | `PH7_PRIVATE int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8296 | `PH7_PRIVATE int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8297 | `PH7_PRIVATE int ph7_hashmap_diff_ukey(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8298 | `PH7_PRIVATE int ph7_hashmap_intersect_ukey(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8299 | `PH7_PRIVATE int ph7_hashmap_udiff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8300 | `PH7_PRIVATE int ph7_hashmap_uintersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8301 | `PH7_PRIVATE int ph7_hashmap_udiff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8302 | `PH7_PRIVATE int ph7_hashmap_uintersect_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8303 | `PH7_PRIVATE int ph7_hashmap_intersect_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8304 | `PH7_PRIVATE int ph7_hashmap_multisort(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8305 | `PH7_PRIVATE int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8306 | `PH7_PRIVATE int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8307 | `PH7_PRIVATE int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8308 | `PH7_PRIVATE int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8309 | `PH7_PRIVATE int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8310 | `PH7_PRIVATE int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8311 | `PH7_PRIVATE int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8312 | `PH7_PRIVATE int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8313 | `PH7_PRIVATE int ph7_hashmap_max(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8314 | `PH7_PRIVATE int ph7_hashmap_min(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8315 | `PH7_PRIVATE int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8316 | `PH7_PRIVATE int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8317 | `PH7_PRIVATE int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8318 | `PH7_PRIVATE int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8319 | `PH7_PRIVATE int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8320 | `PH7_PRIVATE int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8321 | `PH7_PRIVATE int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8322 | `PH7_PRIVATE int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8323 | `PH7_PRIVATE int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8324 | `PH7_PRIVATE int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8325 | `PH7_PRIVATE int ph7_hashmap_first(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8326 | `PH7_PRIVATE int ph7_hashmap_last(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8327 | `PH7_PRIVATE int ph7_hashmap_key_first(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8328 | `PH7_PRIVATE int ph7_hashmap_key_last(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8329 | `PH7_PRIVATE int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8330 | `PH7_PRIVATE int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8331 | `PH7_PRIVATE int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8332 | `PH7_PRIVATE int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8333 | `PH7_PRIVATE int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8334 | `PH7_PRIVATE int ph7_iterator_to_array(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8335 | `PH7_PRIVATE int ph7_iterator_count(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8336 | `PH7_PRIVATE int ph7_iterator_apply(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8337 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth);` |
|         - | 8338 | `PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth,int bProp);` |
|         - | 8339 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(ph7_hashmap *pMap,int (*xWalk)(ph7_value *,ph7_value *,void *),void *pUserData);` |
|         - | 8340 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap);` |
|         - | 8341 | `/* php's key fold, shared with the diagnostics that must print a key the way the` |
|         - | 8342 | ` * LOOKUP saw it. Leaves a non-integer key as a printable string value. */` |
|         - | 8343 | `PH7_PRIVATE int PH7_HashmapKeyIsInt(ph7_value *pKey);` |
|         - | 8344 | `/* php value-name helper (true/false/class-name/null); used by the range()/` |
|         - | 8345 | ` * array_rand() domain-error messages in hashmap.c, which are compiled in every` |
|         - | 8346 | ` * mode, so it must stay outside the PH7_DISABLE_DISK_IO guard. */` |
|         - | 8347 | `PH7_PRIVATE const char *VmValueGivenName(ph7_value *pVal,char *zBuf,sxu32 nBuf);` |
|         - | 8348 | `PH7_PRIVATE void PH7_ReflectInterfacesOf(ph7_vm *pVm,ph7_class *pClass,SySet *pOut);` |
|         - | 8349 | `/* Outcomes of php's STRING-container offset rules (VmStringOffsetResolve). */` |
|         - | 8350 | `#define VM_STROFF_OK      0  /* *piOfft holds php's offset */` |
|         - | 8351 | `#define VM_STROFF_REJECT  1  /* php's TypeError; pMsg carries its message */` |
|         - | 8352 | `#define VM_STROFF_MISS    2  /* lenient context: answer "not set", say nothing */` |
|         - | 8353 | `/* ...and its DIAGNOSTIC LEVEL, of which php has three, not two:` |
|         - | 8354 | ` *   VM_STROFF_LOUD      a real read or write: every warning, and an offset TYPE` |
|         - | 8355 | ` *                       php refuses is the TypeError.` |
|         - | 8356 | `` *   VM_STROFF_COALESCE  a `??` / `??=` fetch: the NOT-SET diagnostics are`` |
|         - | 8357 | `` *                       suppressed (no `Uninitialized string offset`, and a`` |
|         - | 8358 | ` *                       refused offset TYPE answers "not set"), and so is the` |
|         - | 8359 | ` *                       null/bool/float CAST notice — but the offset SHAPE` |
|         - | 8360 | ` *                       warning still fires and the offset is still read:` |
|         - | 8361 | ``  *                       `$s["1x"] ?? "d"` warns `Illegal string offset "1x"` `` |
|         - | 8362 | `` *                       and answers `$s[1]`.`` |
|         - | 8363 | ` *   VM_STROFF_ISSET     isset()/empty()/unset(): fully quiet, every shape.` |
|         - | 8364 | ` *   VM_STROFF_UNSETBASE an INTERMEDIATE subscript of an unset chain` |
|         - | 8365 | `` *                       (`unset($s[k][0])`, `unset($s[k]->p)`): php reads the`` |
|         - | 8366 | ` *                       offset to hand it on, so the CAST notice fires as in a` |
|         - | 8367 | ` *                       real write, but the int-then-garbage warning does not` |
|         - | 8368 | `` *                       (`unset($s["1x"][0])` says nothing about "1x") and an`` |
|         - | 8369 | ` *                       offset TYPE php refuses is not the read's TypeError —` |
|         - | 8370 | `` *                       it is the unset's own `Cannot unset string offsets`,`` |
|         - | 8371 | ` *                       which the caller raises on REJECT at this level.` |
|         - | 8372 | ` */` |
|         - | 8373 | `#define VM_STROFF_LOUD      0` |
|         - | 8374 | `#define VM_STROFF_COALESCE  1` |
|         - | 8375 | `#define VM_STROFF_ISSET     2` |
|         - | 8376 | `#define VM_STROFF_UNSETBASE 3` |
|         - | 8377 | `PH7_PRIVATE int VmStringOffsetResolve(ph7_vm *pVm,ph7_value *pIdx,int iLevel,sxi64 *piOfft,SyBlob *pMsg);` |
|         - | 8378 | `PH7_PRIVATE sxi32 VmStringOffsetWrite(ph7_vm *pVm,ph7_value *pStr,sxi64 iRawOfft,ph7_value *pVal);` |
|         - | 8379 | `/* Numeric-string classifier — php's is_numeric_string() grammar — shared from` |
|         - | 8380 | ` * hashmap.c (range/array_rand) for the stage-2 ZPP domain-error sweep` |
|         - | 8381 | ` * RangeStrToNumber only ever returns ERROR/LONG/DOUBLE; the` |
|         - | 8382 | ` * STRING/DIGIT codes are range()-internal endpoint tags. range() and array_rand()` |
|         - | 8383 | ` * are core builtins compiled in every mode, so these must stay outside the` |
|         - | 8384 | ` * PH7_DISABLE_DISK_IO guard. */` |
|         - | 8385 | `#define RANGE_IN_ERROR   0` |
|         - | 8386 | `#define RANGE_IN_LONG    1` |
|         - | 8387 | `#define RANGE_IN_DOUBLE  2` |
|         - | 8388 | `#define RANGE_IN_STRING  3` |
|         - | 8389 | `#define RANGE_IN_DIGIT   4` |
|         - | 8390 | `PH7_PRIVATE sxu8 RangeStrToNumber(const char *zIn,sxu32 nLen,sxi64 *pLong,double *pDouble);` |
|         - | 8391 | `#ifndef PH7_DISABLE_DISK_IO` |
|         - | 8392 | `PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut);` |
|         - | 8393 | `/* builtin.c function prototypes */` |
|         - | 8394 | `PH7_PRIVATE sxi32 PH7_InputFormat(int (*xConsumer)(ph7_context *,const char *,int,void *),` |
|         - | 8395 | `	ph7_context *pCtx,const char *zIn,int nByte,int nArg,ph7_value **apArg,void *pUserData,int vf);` |
|         - | 8396 | `PH7_PRIVATE sxi32 PH7_FormatValidate(ph7_context *pCtx,const char *zFormat,int nByte);` |
|         - | 8397 | `PH7_PRIVATE sxi32 PH7_FormatCheckArgCount(ph7_context *pCtx,const char *zFormat,int nByte,int nValues,int nFixed,int bVararg);` |
|         - | 8398 | `PH7_PRIVATE sxi32 PH7_FormatCheckFormatArg(ph7_context *pCtx,ph7_value *pArg,int iArg);` |
|         - | 8399 | `PH7_PRIVATE sxi32 PH7_CheckStreamArg(ph7_context *pCtx,ph7_value *pArg,int iArg,const char *zName);` |
|         - | 8400 | `/* $escape = "" disables escape processing: a sentinel outside 0..255 so no byte` |
|         - | 8401 | ` * of a field can ever compare equal to it. */` |
|         - | 8402 | `#define PH7_CSV_NO_ESCAPE 256` |
|         - | 8403 | `/* Cursor of the incremental "is this record still open?" scan (see` |
|         - | 8404 | ` * PH7_CsvScanOpen); fgetcsv() keeps one per record it is assembling. */` |
|         - | 8405 | `typedef struct PH7_CsvScan PH7_CsvScan;` |
|         - | 8406 | `struct PH7_CsvScan {` |
|         - | 8407 | `	int iState;   /* 0 field start, 1 unquoted, 2 inside the enclosure, 3 past it */` |
|         - | 8408 | `	sxu32 nPos;   /* how much of the record has been scanned */` |
|         - | 8409 | `};` |
|         - | 8410 | `PH7_PRIVATE void PH7_CsvScanInit(PH7_CsvScan *pScan);` |
|         - | 8411 | `PH7_PRIVATE int PH7_CsvScanOpen(PH7_CsvScan *pScan,const char *zIn,sxu32 nByte,` |
|         - | 8412 | `	int delim,int encl,int escape);` |
|         - | 8413 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(ph7_value *pArray,const char *zInput,int nByte,` |
|         - | 8414 | `	int delim,int encl,int escape,int *pbOpen);` |
|         - | 8415 | `PH7_PRIVATE sxi32 PH7_CsvCharArg(ph7_context *pCtx,ph7_value *pArg,int iArg,` |
|         - | 8416 | `	const char *zName,int bAllowEmpty,int *pChar);` |
|         - | 8417 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen,int bTagSpaces);` |
|         - | 8418 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection,int iScannerMode,const char *zFile);` |
|         - | 8419 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|         - | 8420 | `/* Constant lookup by name: unguarded because the php.ini value grammar (vm.c,` |
|         - | 8421 | ` * VmIniExprOperand) substitutes a constant for a bare identifier, and that runs` |
|         - | 8422 | ` * in every build. [[tiny-build-disk-io-guard-fragility]] */` |
|         - | 8423 | `PH7_PRIVATE int PH7_VmQueryConstant(ph7_vm *pVm,const char *zName,sxu32 nName,ph7_value *pOut);` |
|         - | 8424 | `PH7_PRIVATE int PH7_ExpandBuiltinConstant(ph7_vm *pVm,const char *zName,sxu32 nName,ph7_value *pOut);` |
|         - | 8425 | `/* Natural-order compare: unguarded because hashmap.c's SORT_NATURAL path (always` |
|         - | 8426 | ` * compiled) uses it, even in the tiny build. [[tiny-build-disk-io-guard-fragility]] */` |
|         - | 8427 | `PH7_PRIVATE int PH7_StrNatCmp(const char *zA,int nA,const char *zB,int nB,int bFold);` |
|         - | 8428 | `/* oo.c function prototypes */` |
|         - | 8429 | `PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine);` |
|         - | 8430 | `PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags);` |
|         - | 8431 | `PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,` |
|         - | 8432 | `	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags);` |
|         - | 8433 | `PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte);` |
|         - | 8434 | `PH7_PRIVATE ph7_class_attr   * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte);` |
|         - | 8435 | `PH7_PRIVATE ph7_class_attr   * PH7_ClassExtractConstant(ph7_class *pClass,const char *zName,sxu32 nByte);` |
|         - | 8436 | `PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr);` |
|         - | 8437 | `PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth);` |
|         - | 8438 | `PH7_PRIVATE int PH7_MagicMethodMustBePublic(const SyString *pName);` |
|         - | 8439 | `PH7_PRIVATE sxi32 PH7_VmCallMagicMethod(ph7_vm *pVm,ph7_class_instance *pThis,` |
|         - | 8440 | `	ph7_class_method *pMethod,ph7_value *pResult,int nArg,ph7_value **apArg);` |
|         - | 8441 | `PH7_PRIVATE sxi32 PH7_VmCallMagicMethodLsb(ph7_vm *pVm,ph7_class *pCalled,ph7_class_instance *pThis,` |
|         - | 8442 | `	ph7_class_method *pMethod,ph7_value *pResult,int nArg,ph7_value **apArg);` |
|         - | 8443 | `PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase);` |
|         - | 8444 | `PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait);` |
|         - | 8445 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase);` |
|         - | 8446 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceCheckRedeclare(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pParent);` |
|         - | 8447 | `PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface);` |
|         - | 8448 | `PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass);` |
|         - | 8449 | `PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc);` |
|         - | 8450 | `/* The two clone questions that follow php's HANDLER inheritance rather than the` |
|         - | 8451 | ` * class's own row: a user subclass of an uncloneable class is uncloneable` |
|         - | 8452 | `` * (`class M extends IteratorIterator {}` refuses `clone $m` with M's name), and`` |
|         - | 8453 | ` * a subclass of a class with a native clone hook clones through that hook. */` |
|         - | 8454 | `PH7_PRIVATE int PH7_ClassIsUncloneable(ph7_class *pClass);` |
|         - | 8455 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest);` |
|         - | 8456 | `PH7_PRIVATE void  PH7_ClassInstanceUnref(ph7_class_instance *pThis);` |
|         - | 8457 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallDestructor(ph7_class_instance *pThis);` |
|         - | 8458 | `PH7_PRIVATE void PH7_ClassInstanceCtorFailed(ph7_class_instance *pThis);` |
|         - | 8459 | `PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth);` |
|         - | 8460 | `PH7_PRIVATE int PH7_UnmangleAttrName(const char *zKey,sxu32 nKey,SyString *pClass,SyString *pName);` |
|         - | 8461 | `PH7_PRIVATE SyHashEntry * PH7_ClassInstanceAttrEntry(ph7_class_instance *pThis,const char *zName,sxu32 nName);` |
|         - | 8462 | `PH7_PRIVATE const SyString * PH7_ClassAttrStorageName(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr);` |
|         - | 8463 | `PH7_PRIVATE void PH7_ClassNotePrivateName(ph7_class *pClass,ph7_class_attr *pAttr);` |
|         - | 8464 | `PH7_PRIVATE ph7_class_attr * PH7_ClassScopedAttribute(ph7_vm *pVm,ph7_class *pClass,const char *zName,sxu32 nName);` |
|         - | 8465 | `PH7_PRIVATE SyHashEntry * PH7_ClassInstanceScopedAttrEntry(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nName,sxu32 nHash);` |
|         - | 8466 | `PH7_PRIVATE int PH7_ClassInstanceAttrShadowed(ph7_vm *pVm,ph7_class_instance *pThis,SyHashEntry *pEntry);` |
|         - | 8467 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(ph7_vm *pVm,ph7_class *pClass,ph7_class_instance *pThis,const char *zMethod,` |
|         - | 8468 | `	sxu32 nByte,const SyString *pAttrName,ph7_value *pResult);` |
|         - | 8469 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr);` |
|         - | 8470 | `PH7_PRIVATE void PH7_ClassInstanceAttrKey(ph7_class_instance *pThis,VmClassAttr *pAttr,ph7_value *pKey);` |
|         - | 8471 | `PH7_PRIVATE int PH7_ClassInstanceAttrPresented(VmClassAttr *pAttr);` |
|         - | 8472 | `PH7_PRIVATE void PH7_ClassInstanceIterOpen(ph7_class_instance *pThis,PH7_AttrIter *pIter);` |
|         - | 8473 | `PH7_PRIVATE SyHashEntry * PH7_ClassInstanceIterNext(PH7_AttrIter *pIter);` |
|         - | 8474 | `PH7_PRIVATE void PH7_ClassInstanceIterClose(ph7_class_instance *pThis,PH7_AttrIter *pIter);` |
|         - | 8475 | `PH7_PRIVATE void PH7_ClassInstanceDeleteAttrEntry(ph7_class_instance *pThis,SyHashEntry *pEntry);` |
|         - | 8476 | `PH7_PRIVATE void PH7_ClassInstanceAttrAppended(ph7_class_instance *pThis,SyHashEntry *pEntry);` |
|         - | 8477 | `PH7_PRIVATE sxi32 PH7_VmHookGetAttrValue(ph7_class_instance *pThis,VmClassAttr *pVmAttr,ph7_value *pOut);` |
|         - | 8478 | `PH7_PRIVATE void PH7_MemObjPrintRInline(SyBlob *pOut,ph7_value *pObj);` |
|         - | 8479 | `PH7_PRIVATE ph7_value * PH7_EnumCaseNameValue(ph7_class_instance *pThis);` |
|         - | 8480 | `PH7_PRIVATE ph7_value * PH7_EnumCaseBackingValueOf(ph7_class_instance *pThis);` |
|         - | 8481 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap);` |
|         - | 8482 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmapRaw(ph7_class_instance *pThis,ph7_hashmap *pMap);` |
|         - | 8483 | `PH7_PRIVATE int PH7_ClassAttrIsRef(ph7_class_instance *pThis,VmClassAttr *pVmAttr);` |
|         - | 8484 | `PH7_PRIVATE sxi32 PH7_ClassInstanceOwnPropsToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap);` |
|         - | 8485 | `PH7_PRIVATE int PH7_ClassAttrUninitialized(VmClassAttr *pVmAttr);` |
|         - | 8486 | `PH7_PRIVATE int PH7_ClassAttrUninitializedForRead(VmClassAttr *pVmAttr);` |
|         - | 8487 | `PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(ph7_class_instance *pThis,` |
|         - | 8488 | `	int (*xWalk)(const char *,ph7_value *,void *),void *pUserData);` |
|         - | 8489 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName);` |
|         - | 8490 | `PH7_PRIVATE int PH7_VmDimFetchWritable(ph7_class *pClass);` |
|         - | 8491 | `PH7_PRIVATE sxu32 PH7_SplDimElemSlot(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pKey,int bCreate);` |
|         - | 8492 | `PH7_PRIVATE void PH7_SplDirVmRelease(ph7_vm *pVm);` |
|         - | 8493 | `/* Called from the VM's reset and release paths, which every build has: the` |
|         - | 8494 | ` * tiny one answers them with the stubs at the tail of vm_phar.c. */` |
|         - | 8495 | `PH7_PRIVATE void PH7_PharVmReset(ph7_vm *pVm);` |
|         - | 8496 | `PH7_PRIVATE void PH7_PharVmRelease(ph7_vm *pVm);` |
|         - | 8497 | `PH7_PRIVATE void PH7_VmOverloadedElemNotice(ph7_vm *pVm,ph7_class *pClass,ph7_value *pVal);` |
|         - | 8498 | `PH7_PRIVATE void PH7_VmOverloadedPropNotice(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,ph7_value *pVal);` |
|         - | 8499 | `PH7_PRIVATE ph7_class_instance * PH7_ContextThis(ph7_context *pCtx);` |
|         - | 8500 | `PH7_PRIVATE ph7_class * PH7_ContextCalledClass(ph7_context *pCtx);` |
|         - | 8501 | `PH7_PRIVATE ph7_value * PH7_ContextThisValue(ph7_context *pCtx);` |
|         - | 8502 | `/* vfs.c */` |
|         - | 8503 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 8504 | `PH7_PRIVATE int PH7_StreamIsUrlWrapper(const ph7_io_stream *pStream);` |
|         - | 8505 | `PH7_PRIVATE void * PH7_StreamOpenHandle(ph7_vm *pVm,const ph7_io_stream *pStream,const char *zFile,` |
|         - | 8506 | `	int iFlags,int use_include,ph7_value *pResource,int bPushInclude,int *pNew,const char *zCaller);` |
|         - | 8507 | `PH7_PRIVATE sxi32 PH7_StreamReadWholeFile(void *pHandle,const ph7_io_stream *pStream,SyBlob *pOut);` |
|         - | 8508 | `PH7_PRIVATE int PH7_VfsAppendFile(ph7_context *pCtx,const char *zFile,const void *pData,int nLen);` |
|         - | 8509 | `PH7_PRIVATE void PH7_StreamCloseHandle(const ph7_io_stream *pStream,void *pHandle);` |
|         - | 8510 | `PH7_PRIVATE ph7_int64 PH7_StreamRead(io_private *pDev,void *pBuf,ph7_int64 nLen);` |
|         - | 8511 | `PH7_PRIVATE ph7_int64 PH7_StreamWrite(io_private *pDev,const void *pData,ph7_int64 nLen);` |
|         - | 8512 | `PH7_PRIVATE void PH7_StreamFlushWriteChain(io_private *pDev);` |
|         - | 8513 | `PH7_PRIVATE int PH7_StreamModeIsValid(const char *zMode,int nLen,int *piFlags);` |
|         - | 8514 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 8515 | `PH7_PRIVATE const char * PH7_ExtractDirName(const char *zPath,int nByte,int *pLen);` |
|         - | 8516 | `PH7_PRIVATE const char * PH7_ExtractBaseName(const char *zPath,int nByte,int *pLen);` |
|         - | 8517 | `PH7_PRIVATE sxi32 PH7_RegisterIORoutine(ph7_vm *pVm);` |
|         - | 8518 | `/* vfs_stream.c / vfs_io_driver.c function prototypes (referenced from the` |
|         - | 8519 | ` * registration tables in vfs.c's PH7_RegisterIORoutine) */` |
|         - | 8520 | `#ifndef PH7_DISABLE_DISK_IO` |
|         - | 8521 | `PH7_PRIVATE int PH7_builtin_closedir(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8522 | `PH7_PRIVATE int PH7_builtin_copy(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8523 | `PH7_PRIVATE int PH7_builtin_escapeshellarg(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8524 | `PH7_PRIVATE int PH7_builtin_exec(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8525 | `PH7_PRIVATE int PH7_builtin_escapeshellcmd(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8526 | `PH7_PRIVATE int PH7_builtin_fclose(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8527 | `PH7_PRIVATE int PH7_builtin_feof(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8528 | `PH7_PRIVATE int PH7_builtin_fflush(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8529 | `PH7_PRIVATE int PH7_builtin_fgetc(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8530 | `PH7_PRIVATE int PH7_builtin_fgetcsv(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8531 | `PH7_PRIVATE int PH7_builtin_fgets(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8532 | `PH7_PRIVATE int PH7_builtin_stream_get_line(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8533 | `PH7_PRIVATE int PH7_builtin_fgetss(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8534 | `PH7_PRIVATE int PH7_builtin_file(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8535 | `PH7_PRIVATE int PH7_builtin_file_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8536 | `PH7_PRIVATE int PH7_builtin_file_put_contents(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8537 | `PH7_PRIVATE int PH7_builtin_flock(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8538 | `PH7_PRIVATE int PH7_builtin_fopen(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8539 | `PH7_PRIVATE int PH7_builtin_fpassthru(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8540 | `PH7_PRIVATE int PH7_builtin_fprintf(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8541 | `PH7_PRIVATE int PH7_builtin_fputcsv(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8542 | `PH7_PRIVATE int PH7_builtin_fread(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8543 | `PH7_PRIVATE int PH7_builtin_fseek(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8544 | `PH7_PRIVATE int PH7_builtin_fsockopen(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8545 | `PH7_PRIVATE int PH7_builtin_stream_socket_server(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8546 | `PH7_PRIVATE int PH7_builtin_stream_socket_accept(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8547 | `PH7_PRIVATE int PH7_builtin_stream_socket_get_name(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8548 | `PH7_PRIVATE int PH7_builtin_stream_select(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8549 | `PH7_PRIVATE int PH7_builtin_stream_socket_pair(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8550 | `PH7_PRIVATE int PH7_builtin_inet_pton(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8551 | `PH7_PRIVATE int PH7_builtin_inet_ntop(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8552 | `PH7_PRIVATE int PH7_builtin_gethostname(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8553 | `PH7_PRIVATE int PH7_builtin_stream_socket_shutdown(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8554 | `PH7_PRIVATE int PH7_builtin_stream_socket_recvfrom(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8555 | `PH7_PRIVATE int PH7_builtin_stream_socket_sendto(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8556 | `PH7_PRIVATE int PH7_builtin_fstat(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8557 | `PH7_PRIVATE int PH7_builtin_ftell(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8558 | `PH7_PRIVATE int PH7_builtin_ftruncate(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8559 | `PH7_PRIVATE int PH7_builtin_fwrite(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8560 | `PH7_PRIVATE int PH7_builtin_md5_file(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8561 | `PH7_PRIVATE int PH7_builtin_opendir(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8562 | `PH7_PRIVATE int PH7_builtin_dir(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8563 | `PH7_PRIVATE int PH7_builtin_parse_ini_file(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8564 | `PH7_PRIVATE int PH7_builtin_passthru(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8565 | `PH7_PRIVATE int PH7_builtin_pclose(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8566 | `PH7_PRIVATE int PH7_builtin_popen(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8567 | `PH7_PRIVATE int PH7_builtin_proc_close(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8568 | `PH7_PRIVATE int PH7_builtin_proc_get_status(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8569 | `PH7_PRIVATE int PH7_builtin_proc_nice(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8570 | `PH7_PRIVATE int PH7_builtin_proc_open(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8571 | `PH7_PRIVATE int PH7_builtin_proc_terminate(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8572 | `PH7_PRIVATE int PH7_builtin_readdir(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8573 | `PH7_PRIVATE int PH7_builtin_readfile(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8574 | `PH7_PRIVATE int PH7_builtin_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8575 | `PH7_PRIVATE int PH7_builtin_rewinddir(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8576 | `PH7_PRIVATE int PH7_builtin_sha1_file(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8577 | `PH7_PRIVATE int PH7_builtin_shell_exec(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8578 | `PH7_PRIVATE int PH7_builtin_system(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8579 | `PH7_PRIVATE int PH7_builtin_stream_context_create(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8580 | `PH7_PRIVATE int PH7_builtin_stream_context_get_options(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8581 | `PH7_PRIVATE int PH7_builtin_stream_context_set_option(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8582 | `PH7_PRIVATE int PH7_builtin_stream_context_set_options(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8583 | `PH7_PRIVATE int PH7_builtin_stream_context_get_params(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8584 | `PH7_PRIVATE int PH7_builtin_stream_context_set_params(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8585 | `PH7_PRIVATE int PH7_builtin_stream_context_get_default(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8586 | `PH7_PRIVATE int PH7_builtin_stream_context_set_default(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8587 | `PH7_PRIVATE int PH7_builtin_stream_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8588 | `PH7_PRIVATE int PH7_builtin_stream_get_meta_data(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8589 | `PH7_PRIVATE int PH7_builtin_stream_set_blocking(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8590 | `PH7_PRIVATE int PH7_builtin_stream_set_timeout(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8591 | `PH7_PRIVATE int PH7_builtin_stream_set_chunk_size(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8592 | `PH7_PRIVATE int PH7_builtin_stream_set_read_buffer(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8593 | `PH7_PRIVATE int PH7_builtin_stream_set_write_buffer(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8594 | `PH7_PRIVATE int PH7_builtin_stream_supports_lock(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8595 | `PH7_PRIVATE int PH7_builtin_stream_is_local(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8596 | `PH7_PRIVATE int PH7_builtin_stream_copy_to_stream(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8597 | `PH7_PRIVATE int PH7_builtin_stream_socket_enable_crypto(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8598 | `PH7_PRIVATE int PH7_builtin_stream_get_transports(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8599 | `PH7_PRIVATE int PH7_builtin_stream_get_wrappers(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8600 | `PH7_PRIVATE int PH7_builtin_stream_isatty(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8601 | `PH7_PRIVATE int PH7_builtin_stream_wrapper_register(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8602 | `PH7_PRIVATE int PH7_StreamUserUrlStat(ph7_context *pCtx,const char *zPath,int iFlags,ph7_int64 *aVal);` |
|         - | 8603 | `PH7_PRIVATE void PH7_StreamUserCast(ph7_context *pCtx,const ph7_io_stream *pStream,void *pHandle);` |
|         - | 8604 | `PH7_PRIVATE int PH7_StreamUserPathOp(ph7_context *pCtx,const char *zPath,const char *zMethod,void *pStreamCtx,ph7_value **apExtra,int nExtra,int *pbAnswer);` |
|         - | 8605 | `PH7_PRIVATE void PH7_StreamArmOpenMode(ph7_vm *pVm,const char *zMode,int nMode);` |
|         - | 8606 | `PH7_PRIVATE int PH7_StreamUserDirReason(ph7_vm *pVm,const ph7_io_stream *pStream,char *zBuf,int nBuf);` |
|         - | 8607 | `PH7_PRIVATE int PH7_VfsUserStatFields(ph7_context *pCtx,const char *zPath,int eAsk,ph7_int64 *aVal);` |
|         - | 8608 | `PH7_PRIVATE void PH7_VfsUserStatResult(ph7_context *pCtx,int eAsk,const ph7_int64 *aVal);` |
|         - | 8609 | `PH7_PRIVATE void PH7_VfsStatAccessMasks(ph7_context *pCtx,ph7_int64 nUid,ph7_int64 nGid,int *pR,int *pW,int *pX);` |
|         - | 8610 | `PH7_PRIVATE int PH7_builtin_stream_wrapper_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8611 | `PH7_PRIVATE int PH7_builtin_stream_wrapper_restore(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8612 | `PH7_PRIVATE int PH7_builtin_stream_filter_append(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8613 | `PH7_PRIVATE int PH7_builtin_stream_filter_prepend(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8614 | `PH7_PRIVATE int PH7_builtin_stream_filter_remove(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8615 | `PH7_PRIVATE int PH7_builtin_stream_get_filters(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8616 | `PH7_PRIVATE int PH7_builtin_vfprintf(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 8617 | `#endif /* PH7_DISABLE_DISK_IO */` |
|         - | 8618 | `PH7_PRIVATE const ph7_vfs * PH7_ExportBuiltinVfs(void);` |
|         - | 8619 | `PH7_PRIVATE const char * PH7_VfsResourceType(void *pResource);` |
|         - | 8620 | `PH7_PRIVATE int PH7_VfsResourceIsClosed(void *pResource);` |
|         - | 8621 | `PH7_PRIVATE void * PH7_ExportStdin(ph7_vm *pVm);` |
|         - | 8622 | `PH7_PRIVATE void * PH7_ExportStdout(ph7_vm *pVm);` |
|         - | 8623 | `PH7_PRIVATE void * PH7_ExportStderr(ph7_vm *pVm);` |
|         - | 8624 | `/* lib.c function prototypes */` |
|         - | 8625 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 8626 | `PH7_PRIVATE sxi32 SyBinToHexConsumer(const void *pIn,sxu32 nLen,ProcConsumer xConsumer,void *pConsumerData);` |
|         - | 8627 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 8628 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 8629 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|         - | 8630 | `PH7_PRIVATE sxu32 SyCrc32(const void *pSrc,sxu32 nLen);` |
|         - | 8631 | `PH7_PRIVATE void MD5Update(MD5Context *ctx, const unsigned char *buf, unsigned int len);` |
|         - | 8632 | `PH7_PRIVATE void MD5Final(unsigned char digest[16], MD5Context *ctx);` |
|         - | 8633 | `PH7_PRIVATE sxi32 MD5Init(MD5Context *pCtx);` |
|         - | 8634 | `PH7_PRIVATE sxi32 SyMD5Compute(const void *pIn,sxu32 nLen,unsigned char zDigest[16]);` |
|         - | 8635 | `PH7_PRIVATE void SHA1Init(SHA1Context *context);` |
|         - | 8636 | `PH7_PRIVATE void SHA1Update(SHA1Context *context,const unsigned char *data,unsigned int len);` |
|         - | 8637 | `PH7_PRIVATE void SHA1Final(SHA1Context *context, unsigned char digest[20]);` |
|         - | 8638 | `PH7_PRIVATE sxi32 SySha1Compute(const void *pIn,sxu32 nLen,unsigned char zDigest[20]);` |
|         - | 8639 | `#endif` |
|         - | 8640 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 8641 | `PH7_PRIVATE sxi32 SyRandomness(SyPRNGCtx *pCtx,void *pBuf,sxu32 nLen);` |
|         - | 8642 | `PH7_PRIVATE sxi32 SyRandomnessInit(SyPRNGCtx *pCtx,ProcRandomSeed xSeed,void *pUserData);` |
|         - | 8643 | `PH7_PRIVATE sxu32 SyBufferFormat(char *zBuf,sxu32 nLen,const char *zFormat,...);` |
|         - | 8644 | `PH7_PRIVATE sxu32 SyBlobFormatAp(SyBlob *pBlob,const char *zFormat,va_list ap);` |
|         - | 8645 | `PH7_PRIVATE sxu32 SyBlobFormat(SyBlob *pBlob,const char *zFormat,...);` |
|         - | 8646 | `PH7_PRIVATE sxi32 SyProcFormat(ProcConsumer xConsumer,void *pData,const char *zFormat,...);` |
|         - | 8647 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 8648 | `PH7_PRIVATE const char *SyTimeGetMonth(sxi32 iMonth);` |
|         - | 8649 | `PH7_PRIVATE const char *SyTimeGetDay(sxi32 iDay);` |
|         - | 8650 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 8651 | `PH7_PRIVATE sxi32 SyUriDecode(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData,int bPlus);` |
|         - | 8652 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 8653 | `PH7_PRIVATE sxi32 SyUriEncode(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData);` |
|         - | 8654 | `PH7_PRIVATE sxi32 SyUriEncodeRaw(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData);` |
|         - | 8655 | `#endif` |
|         - | 8656 | `PH7_PRIVATE sxi32 SyLexRelease(SyLex *pLex);` |
|         - | 8657 | `PH7_PRIVATE sxi32 SyLexTokenizeInput(SyLex *pLex,const char *zInput,sxu32 nLen,void *pCtxData,ProcSort xSort,ProcCmp xCmp);` |
|         - | 8658 | `PH7_PRIVATE sxi32 SyLexInit(SyLex *pLex,SySet *pSet,ProcTokenizer xTokenizer,void *pUserData);` |
|         - | 8659 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 8660 | `PH7_PRIVATE sxi32 SyBase64Decode(const char *zB64,sxu32 nLen,ProcConsumer xConsumer,void *pUserData);` |
|         - | 8661 | `PH7_PRIVATE sxi32 SyBase64Encode(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData);` |
|         - | 8662 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 8663 | `PH7_PRIVATE sxi32 SyStrToReal(const char *zSrc,sxu32 nLen,void *pOutVal,const char **zRest);` |
|         - | 8664 | `PH7_PRIVATE sxi32 SyBinaryStrToInt64(const char *zSrc,sxu32 nLen,void *pOutVal,const char **zRest);` |
|         - | 8665 | `PH7_PRIVATE sxi32 SyOctalStrToInt64(const char *zSrc,sxu32 nLen,void *pOutVal,const char **zRest);` |
|         - | 8666 | `PH7_PRIVATE sxi32 SyHexStrToInt64(const char *zSrc,sxu32 nLen,void *pOutVal,const char **zRest);` |
|         - | 8667 | `PH7_PRIVATE sxi32 SyHexToint(sxi32 c);` |
|         - | 8668 | `PH7_PRIVATE sxi32 SyStrToInt64(const char *zSrc,sxu32 nLen,void *pOutVal,const char **zRest);` |
|         - | 8669 | `PH7_PRIVATE sxi32 SyStrToInt32(const char *zSrc,sxu32 nLen,void *pOutVal,const char **zRest);` |
|         - | 8670 | `PH7_PRIVATE sxi32 SyStrIsNumeric(const char *zSrc,sxu32 nLen,sxu8 *pReal,const char **pzTail);` |
|         - | 8671 | `PH7_PRIVATE SyHashEntry *SyHashLastEntry(SyHash *pHash);` |
|         - | 8672 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData);` |
|         - | 8673 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData);` |
|         - | 8674 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32(*xStep)(SyHashEntry *,void *),void *pUserData);` |
|         - | 8675 | `PH7_PRIVATE SyHashEntry *SyHashGetNextEntry(SyHash *pHash);` |
|         - | 8676 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash);` |
|         - | 8677 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry);` |
|         - | 8678 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData);` |
|         - | 8679 | `PH7_PRIVATE SyHashEntry *SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen);` |
|         - | 8680 | `PH7_PRIVATE sxu32 SyHashKey(SyHash *pHash,const void *pKey,sxu32 nKeyLen);` |
|         - | 8681 | `PH7_PRIVATE SyHashEntry *SyHashGetHashed(SyHash *pHash,const void *pKey,sxu32 nKeyLen,sxu32 nHash);` |
|         - | 8682 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash);` |
|         - | 8683 | `PH7_PRIVATE sxi32 SyHashInitSized(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp,sxu32 nBucket);` |
|         - | 8684 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp);` |
|         - | 8685 | `PH7_PRIVATE sxu32 SyStrHash(const void *pSrc,sxu32 nLen);` |
|         - | 8686 | `PH7_PRIVATE sxu32 SyBinHash(const void *pSrc,sxu32 nLen);` |
|         - | 8687 | `PH7_PRIVATE sxu32 Systrcpy(char *zDest,sxu32 nDestLen,const char *zSrc,sxu32 nLen);` |
|         - | 8688 | `PH7_PRIVATE void *SySetAt(SySet *pSet,sxu32 nIdx);` |
|         - | 8689 | `PH7_PRIVATE void *SySetPop(SySet *pSet);` |
|         - | 8690 | `PH7_PRIVATE void *SySetPeek(SySet *pSet);` |
|         - | 8691 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet);` |
|         - | 8692 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet);` |
|         - | 8693 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet);` |
|         - | 8694 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry);` |
|         - | 8695 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 8696 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet);` |
|         - | 8697 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 8698 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize);` |
|         - | 8699 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem);` |
|         - | 8700 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem);` |
|         - | 8701 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize);` |
|         - | 8702 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 8703 | `PH7_PRIVATE sxi32 SyBlobSearch(const void *pBlob,sxu32 nLen,const void *pPattern,sxu32 pLen,sxu32 *pOfft);` |
|         - | 8704 | `#endif` |
|         - | 8705 | `PH7_PRIVATE sxi32 SyBlobRelease(SyBlob *pBlob);` |
|         - | 8706 | `PH7_PRIVATE sxi32 SyBlobReset(SyBlob *pBlob);` |
|         - | 8707 | `PH7_PRIVATE sxi32 SyBlobCmp(SyBlob *pLeft,SyBlob *pRight);` |
|         - | 8708 | `PH7_PRIVATE sxi32 SyBlobDup(SyBlob *pSrc,SyBlob *pDest);` |
|         - | 8709 | `PH7_PRIVATE sxi32 SyBlobNullAppend(SyBlob *pBlob);` |
|         - | 8710 | `PH7_PRIVATE sxi32 SyBlobAppend(SyBlob *pBlob,const void *pData,sxu32 nSize);` |
|         - | 8711 | `PH7_PRIVATE sxi32 SyBlobReadOnly(SyBlob *pBlob,const void *pData,sxu32 nByte);` |
|         - | 8712 | `PH7_PRIVATE sxi32 SyBlobInit(SyBlob *pBlob,SyMemBackend *pAllocator);` |
|         - | 8713 | `PH7_PRIVATE sxi32 SyBlobInitFromBuf(SyBlob *pBlob,void *pBuffer,sxu32 nSize);` |
|         - | 8714 | `PH7_PRIVATE char *SyMemBackendStrDup(SyMemBackend *pBackend,const char *zSrc,sxu32 nSize);` |
|         - | 8715 | `PH7_PRIVATE void *SyMemBackendDup(SyMemBackend *pBackend,const void *pSrc,sxu32 nSize);` |
|         - | 8716 | `PH7_PRIVATE sxi32 SyMemBackendRelease(SyMemBackend *pBackend);` |
|         - | 8717 | `PH7_PRIVATE sxi32 SyMemBackendInitFromOthers(SyMemBackend *pBackend,const SyMemMethods *pMethods,ProcMemError xMemErr,void *pUserData);` |
|         - | 8718 | `PH7_PRIVATE sxi32 SyMemBackendInit(SyMemBackend *pBackend,ProcMemError xMemErr,void *pUserData);` |
|         - | 8719 | `PH7_PRIVATE sxi32 SyMemBackendInitFromParent(SyMemBackend *pBackend,SyMemBackend *pParent);` |
|         - | 8720 | `#if 0` |
|         - | 8721 | `/* Not used in the current release of the PH7 engine */` |
|         - | 8722 | `PH7_PRIVATE void *SyMemBackendPoolRealloc(SyMemBackend *pBackend,void *pOld,sxu32 nByte);` |
|         - | 8723 | `#endif` |
|         - | 8724 | `PH7_PRIVATE sxi32 SyMemBackendPoolFree(SyMemBackend *pBackend,void *pChunk);` |
|         - | 8725 | `PH7_PRIVATE void *SyMemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte);` |
|         - | 8726 | `PH7_PRIVATE sxi32 SyMemBackendFree(SyMemBackend *pBackend,void *pChunk);` |
|         - | 8727 | `PH7_PRIVATE void *SyMemBackendRealloc(SyMemBackend *pBackend,void *pOld,sxu32 nByte);` |
|         - | 8728 | `PH7_PRIVATE void *SyMemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte);` |
|         - | 8729 | `#if defined(PH7_ENABLE_THREADS)` |
|         - | 8730 | `PH7_PRIVATE sxi32 SyMemBackendMakeThreadSafe(SyMemBackend *pBackend,const SyMutexMethods *pMethods);` |
|         - | 8731 | `PH7_PRIVATE sxi32 SyMemBackendDisbaleMutexing(SyMemBackend *pBackend);` |
|         - | 8732 | `#endif` |
|         - | 8733 | `PH7_PRIVATE sxu32 SyMemcpy(const void *pSrc,void *pDest,sxu32 nLen);` |
|         - | 8734 | `PH7_PRIVATE sxi32 SyMemcmp(const void *pB1,const void *pB2,sxu32 nSize);` |
|         - | 8735 | `PH7_PRIVATE void SyZero(void *pSrc,sxu32 nSize);` |
|         - | 8736 | `PH7_PRIVATE sxi32 SyStrnicmp(const char *zLeft,const char *zRight,sxu32 SLen);` |
|         - | 8737 | `PH7_PRIVATE sxi32 SyStrnmicmp(const void *pLeft, const void *pRight,sxu32 SLen);` |
|         - | 8738 | `/* used by hashmap.c's key sorting — must stay visible in the tiny build */` |
|         - | 8739 | `PH7_PRIVATE sxi32 SyStrncmp(const char *zLeft,const char *zRight,sxu32 nLen);` |
|         - | 8740 | `PH7_PRIVATE sxi32 SyByteListFind(const char *zSrc,sxu32 nLen,const char *zList,sxu32 *pFirstPos);` |
|         - | 8741 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 8742 | `PH7_PRIVATE sxi32 SyByteFind2(const char *zStr,sxu32 nLen,sxi32 c,sxu32 *pPos);` |
|         - | 8743 | `#endif` |
|         - | 8744 | `PH7_PRIVATE sxi32 SyByteFind(const char *zStr,sxu32 nLen,sxi32 c,sxu32 *pPos);` |
|         - | 8745 | `PH7_PRIVATE sxu32 SyStrlen(const char *zSrc);` |
|         - | 8746 | `#if defined(PH7_ENABLE_THREADS)` |
|         - | 8747 | `PH7_PRIVATE const SyMutexMethods *SyMutexExportMethods(void);` |
|         - | 8748 | `PH7_PRIVATE sxi32 SyMemBackendMakeThreadSafe(SyMemBackend *pBackend,const SyMutexMethods *pMethods);` |
|         - | 8749 | `PH7_PRIVATE sxi32 SyMemBackendDisbaleMutexing(SyMemBackend *pBackend);` |
|         - | 8750 | `#endif` |
|         - | 8751 | `#endif /* __PH7INT_H__ */` |
|         - | 8752 |  |
