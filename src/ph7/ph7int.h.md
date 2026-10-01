# src/ph7/ph7int.h

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 29/29 lines (100.00%)

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
|         - |  336 | `/* Mask of all known types */` |
|         - |  337 | `#define MEMOBJ_ALL (MEMOBJ_STRING\|MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)` |
|         - |  338 | `/* Scalar variables` |
|         - |  339 | ` * According to the PHP language reference manual` |
|         - |  340 | ` *  Scalar variables are those containing an integer, float, string or boolean.` |
|         - |  341 | ` *  Types array, object and resource are not scalar.` |
|         - |  342 | ` */` |
|         - |  343 | `#define MEMOBJ_AUX_REFRET 0x800000 /* Stack-only marker: this value is the result of a call to a` |
|         - |  344 | `                                    * function DECLARED to return by reference. php raises` |
|         - |  345 | ``                                     * `Only variable references should be returned by reference` `` |
|         - |  346 | `                                    * at the RETURN when such a function has no variable to` |
|         - |  347 | `                                    * bind, and says nothing more at the call site -- so the` |
|         - |  348 | ``                                    * `Only variables should be assigned by reference` notice,`` |
|         - |  349 | `                                    * which is about a callee that never promised a reference,` |
|         - |  350 | `                                    * stands down for a value carrying this. */` |
|         - |  351 | `#define MEMOBJ_POOLFREE 0x1000000  /* NOT a type or a stack marker: pool bookkeeping. This slot is` |
|         - |  352 | `                                    * ON the value pool's intrusive free list (see VmMemPool), so` |
|         - |  353 | `                                    * its nIdx word is the link to the next free slot and NOT its` |
|         - |  354 | `                                    * own index. Set by VmMemPoolFreeSlot, cleared by the` |
|         - |  355 | `                                    * PH7_MemObjInit every acquire runs and by VmMemPoolTruncate` |
|         - |  356 | `                                    * when it abandons the chain. It exists to make a double free` |
|         - |  357 | `                                    * a no-op: the link lives inside the slot, so freeing the same` |
|         - |  358 | `                                    * index twice would point the head at itself and hand that one` |
|         - |  359 | `                                    * slot out for the rest of the run. Deliberately survives` |
|         - |  360 | `                                    * PH7_MemObjRelease, which leaves iFlags alone once a value is` |
|         - |  361 | `                                    * already MEMOBJ_NULL -- and a slot on the list always is. */` |
|         - |  362 | `#define MEMOBJ_SCALAR (MEMOBJ_STRING\|MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_NULL)` |
|         - |  363 | `#define MEMOBJ_AUX (MEMOBJ_REFERENCE\|MEMOBJ_AUX_SPREAD\|MEMOBJ_AUX_NOKEY\|MEMOBJ_AUX_CUFVAL\|MEMOBJ_AUX_DEFERRED\|MEMOBJ_AUX_DEFPATH\|MEMOBJ_AUX_STROFFSET\|MEMOBJ_AUX_COALSTROFF\|MEMOBJ_AUX_MAGICCALL\|MEMOBJ_AUX_MEMBERCALL\|MEMOBJ_AUX_ENGINEFN\|MEMOBJ_AUX_NATIVEPROP\|MEMOBJ_AUX_REFRET)` |
|         - |  364 | `/* Closure-instance flags (ph7_class_instance.iFlags), shared by vm_exec.c's OP_LOAD_FCC` |
|         - |  365 | ` * and vm_exec_ctx.c's closure machinery. Distinct from CLASS_INSTANCE_DESTROYED 0x001` |
|         - |  366 | ` * (oo.c) and VM_INSTANCE_DUMPING 0x002 (vm_builtin_var.c), which share the same word. */` |
|         - |  367 | `/* ph7_class_instance.iFlags bit: this Closure is a bound/static first-class callable and` |
|         - |  368 | ` * carries $__this/$__scope. Lets the hot plain-closure unwrap skip those attribute lookups.` |
|         - |  369 | ` * (Distinct from CLASS_INSTANCE_DESTROYED 0x001 and VM_INSTANCE_DUMPING 0x002.) */` |
|         - |  370 | `#define VM_INSTANCE_FCC_BOUND 0x004` |
|         - |  371 | `/* ph7_class_instance.iFlags bit: this Closure wraps an __invoke OBJECT, and the engine —` |
|         - |  372 | `` * not the source — is what named `__invoke` (Closure::fromCallable($obj)). php resolves it`` |
|         - |  373 | `` * the way it resolves `$obj()`, so a non-public __invoke is dispatched rather than denied;`` |
|         - |  374 | `` * `$obj->__invoke(...)` and `[$obj,'__invoke']`, which the SOURCE names, stay denied and`` |
|         - |  375 | ` * never carry this bit. Read by VmClosureUnwrap, which arms the engine's magic latch. */` |
|         - |  376 | `#define VM_INSTANCE_FCC_INVOKE_OBJ 0x010` |
|         - |  377 | `/* ph7_class_instance.iFlags bit: this Closure's $__fn names a METHOD of $__this's class (or` |
|         - |  378 | `` * of $__scope) — `$o->m(...)`, `C::m(...)`, `Closure::fromCallable([$o,'m'])`. Without it the`` |
|         - |  379 | ` * unwrap had to GUESS, by asking whether the class declares a method of that name, and it` |
|         - |  380 | ` * guessed wrong in both directions: a name the class answers only through __call fell through` |
|         - |  381 | ` * to the plain-function route and failed with "Call to undefined function m()", and a name` |
|         - |  382 | ` * that happened to match a global function ran the FUNCTION. A bound plain closure` |
|         - |  383 | `` * (`function(){…}->bindTo($o)`) never carries this bit, which is what the guess was really`` |
|         - |  384 | ` * trying to detect. */` |
|         - |  385 | `#define VM_INSTANCE_FCC_METHOD 0x020` |
|         - |  386 | `/* ph7_class_instance.iFlags bit: this Closure's callee was RESOLVED to a real, directly` |
|         - |  387 | ` * callable method when the closure was BUILT — the way php resolves one, keeping the` |
|         - |  388 | ` * function itself rather than a name. No dispatch site may re-decide its visibility against` |
|         - |  389 | `` * the CALLER: that is what killed an escaped `$this->priv(...)` php runs anywhere. A closure`` |
|         - |  390 | ` * whose creation resolved to the class's __call/__callStatic TRAMPOLINE instead (a missing or` |
|         - |  391 | ` * inaccessible name on a class that declares one) deliberately does NOT carry the bit — its` |
|         - |  392 | ` * dispatch has to reach the catch-all, as php's does. */` |
|         - |  393 | `#define VM_INSTANCE_FCC_SCREENED 0x040` |
|         - |  394 | `/*` |
|         - |  395 | ` * The following macro clear the current ph7_value type and replace` |
|         - |  396 | ` * it with the given one.` |
|         - |  397 | ` */` |
|         - |  398 | `#define MemObjSetType(OBJ,TYPE) ((OBJ)->iFlags = ((OBJ)->iFlags&~MEMOBJ_ALL)\|TYPE)` |
|         - |  399 | `/*` |
|         - |  400 | ` * Signed 64-bit arithmetic with overflow detection. PHP promotes an integer` |
|         - |  401 | ` * operation that overflows sxi64 to a floating-point result, so the executor` |
|         - |  402 | ` * checks for overflow on every +,-,* (and ++/--) and re-runs the operation in` |
|         - |  403 | ` * double precision when it trips. GCC/Clang expose the __builtin_*_overflow` |
|         - |  404 | ` * intrinsics (zero cost, no UB); MSVC lacks them, so we fall back to portable` |
|         - |  405 | ` * implementations defined in memobj.c. Each macro sets *pR to the wrapped` |
|         - |  406 | ` * result and evaluates to non-zero on overflow.` |
|         - |  407 | ` */` |
|         - |  408 | `#if defined(__GNUC__) \|\| defined(__clang__)` |
|         - |  409 | `#define PH7_ADD_OVERFLOW64(a,b,pR) __builtin_add_overflow((a),(b),(pR))` |
|         - |  410 | `#define PH7_SUB_OVERFLOW64(a,b,pR) __builtin_sub_overflow((a),(b),(pR))` |
|         - |  411 | `#define PH7_MUL_OVERFLOW64(a,b,pR) __builtin_mul_overflow((a),(b),(pR))` |
|         - |  412 | `#else` |
|         - |  413 | `PH7_PRIVATE int PH7_AddOverflow64(sxi64 a,sxi64 b,sxi64 *pR);` |
|         - |  414 | `PH7_PRIVATE int PH7_SubOverflow64(sxi64 a,sxi64 b,sxi64 *pR);` |
|         - |  415 | `PH7_PRIVATE int PH7_MulOverflow64(sxi64 a,sxi64 b,sxi64 *pR);` |
|         - |  416 | `#define PH7_ADD_OVERFLOW64(a,b,pR) PH7_AddOverflow64((a),(b),(pR))` |
|         - |  417 | `#define PH7_SUB_OVERFLOW64(a,b,pR) PH7_SubOverflow64((a),(b),(pR))` |
|         - |  418 | `#define PH7_MUL_OVERFLOW64(a,b,pR) PH7_MulOverflow64((a),(b),(pR))` |
|         - |  419 | `#endif` |
|         - |  420 | `/* ph7_value cast method signature */` |
|         - |  421 | `typedef sxi32 (*ProcMemObjCast)(ph7_value *);` |
|         - |  422 | `/* Forward reference */` |
|         - |  423 | `typedef struct ph7_output_consumer ph7_output_consumer;` |
|         - |  424 | `/*` |
|         - |  425 | ` * One parameter of a builtin's declared signature, as the shared argument screen` |
|         - |  426 | ` * (VmEnforceBuiltinArgTypes) needs to see it: the type TEXT, the parameter name, and` |
|         - |  427 | `` * the two markers the text carries -- `&` for by-reference and a leading `~` for`` |
|         - |  428 | ` * php's stub-versus-body disagreement. Every pointer is INTO zSig, which is static` |
|         - |  429 | ` * storage; nothing here is copied and nothing here is freed.` |
|         - |  430 | ` *` |
|         - |  431 | ` * Getting this out of zSig is a walk -- skip spaces, find the comma that ends the` |
|         - |  432 | ` * parameter (honouring a quoted default, which can contain one), find the '$', check` |
|         - |  433 | ` * for a variadic '...', trim the type's trailing spaces and '&'. The screen did that` |
|         - |  434 | ` * walk for every argument of every builtin call: 18,901,261 parameters on the` |
|         - |  435 | ` * ecosystem gate's phpcs step, for an answer that is a property of the DECLARATION` |
|         - |  436 | ` * and cannot change between two calls. (PERF.md P13.)` |
|         - |  437 | ` */` |
|         - |  438 | `typedef struct VmArgScreenParam VmArgScreenParam;` |
|         - |  439 | `struct VmArgScreenParam` |
|         - |  440 | `{` |
|         - |  441 | `	const char *zType;   /* into zSig; nType 0 means untyped and unscreened */` |
|         - |  442 | `	const char *zName;   /* into zSig, past the '$' */` |
|         - |  443 | `	sxu16 nType;` |
|         - |  444 | `	sxu16 nName;` |
|         - |  445 | `	sxu8 bByRef;         /* "array &$array" */` |
|         - |  446 | `	sxu8 bStub;          /* "~Type $p": the builtin raises its own TypeError */` |
|         - |  447 | `	sxu32 nMask;         /* VMSIG_* -- which arms this declared type has. The screen asks` |
|         - |  448 | `	                      * that question up to forty-four times per argument, and every` |
|         - |  449 | `	                      * ask was a split-on-'\|' walk over the same text. The bits are` |
|         - |  450 | `	                      * SET by calling the very functions they replace (see` |
|         - |  451 | `	                      * VmArgScreenNext), so a bit cannot mean something the walk did` |
|         - |  452 | `	                      * not say. (PERF.md P13.) */` |
|         - |  453 | `};` |
|         - |  454 | `/* The arms VmArgScreenParam::nMask records. The first thirteen are VmSigTypeHas() tokens;` |
|         - |  455 | ` * the last three are the three composite questions the screen asks about a whole type. */` |
|         - |  456 | `#define VMSIG_MIXED      0x00000001` |
|         - |  457 | `#define VMSIG_ARRAY      0x00000002` |
|         - |  458 | `#define VMSIG_ITERABLE   0x00000004` |
|         - |  459 | `#define VMSIG_CALLABLE   0x00000008` |
|         - |  460 | `#define VMSIG_OBJECT     0x00000010` |
|         - |  461 | `#define VMSIG_STRING     0x00000020` |
|         - |  462 | `#define VMSIG_NULL       0x00000040` |
|         - |  463 | `#define VMSIG_INT        0x00000080` |
|         - |  464 | `#define VMSIG_FLOAT      0x00000100` |
|         - |  465 | `#define VMSIG_BOOL       0x00000200` |
|         - |  466 | `#define VMSIG_TRUE       0x00000400` |
|         - |  467 | `#define VMSIG_FALSE      0x00000800` |
|         - |  468 | `#define VMSIG_RESOURCE   0x00001000` |
|         - |  469 | `#define VMSIG_CLASS      0x00002000   /* VmSigTypeHasClass: an arm that is not a builtin type */` |
|         - |  470 | `#define VMSIG_INTONLY    0x00004000   /* VmSigTypeIsIntOnly */` |
|         - |  471 | `#define VMSIG_ARRAYONLY  0x00008000   /* VmSigTypeIsArrayOnly */` |
|         - |  472 | `typedef struct ph7_user_func ph7_user_func;` |
|         - |  473 | `typedef struct ph7_conf ph7_conf;` |
|         - |  474 | `/*` |
|         - |  475 | ` * An instance of the following structure store the default VM output` |
|         - |  476 | ` * consumer and it's private data.` |
|         - |  477 | ` * Client-programs can register their own output consumer callback` |
|         - |  478 | ` * via the [PH7_VM_CONFIG_OUTPUT] configuration directive.` |
|         - |  479 | ` * Please refer to the official documentation for more information` |
|         - |  480 | ` * on how to register an output consumer callback.` |
|         - |  481 | ` */` |
|         - |  482 | `struct ph7_output_consumer` |
|         - |  483 | `{` |
|         - |  484 | `	ProcConsumer xConsumer; /* VM output consumer routine */` |
|         - |  485 | `	void *pUserData;        /* Third argument to xConsumer() */` |
|         - |  486 | `	ProcConsumer xDef;      /* Default output consumer routine */` |
|         - |  487 | `	void *pDefData;         /* Third argument to xDef() */` |
|         - |  488 | `};` |
|         - |  489 | `/*` |
|         - |  490 | ` * PH7 engine [i.e: ph7 instance] configuration is stored in` |
|         - |  491 | ` * an instance of the following structure.` |
|         - |  492 | ` * Please refer to the official documentation for more information` |
|         - |  493 | ` * on how to configure your ph7 engine instance.` |
|         - |  494 | ` */` |
|         - |  495 | `struct ph7_conf` |
|         - |  496 | `{` |
|         - |  497 | `	ProcConsumer xErr;   /* Compile-time error consumer callback */` |
|         - |  498 | `	void *pErrData;      /* Third argument to xErr() */` |
|         - |  499 | `	SyBlob sErrConsumer; /* Default error consumer */` |
|         - |  500 | `	ph7_clock xClock;    /* Optional embedder clock [PH7_CONFIG_CLOCK]; NULL => platform default */` |
|         - |  501 | `	void *pClockData;    /* Third argument to xClock() */` |
|         - |  502 | `	sxu32 nMaxInput;     /* Per-compile input byte cap [PH7_CONFIG_MAX_INPUT]; 0 = PH7_MAX_INPUT_SIZE */` |
|         - |  503 | `};` |
|         - |  504 | `/*` |
|         - |  505 | ` * Signature of the C function responsible of expanding constant values.` |
|         - |  506 | ` */` |
|         - |  507 | `typedef void (*ProcConstant)(ph7_value *,void *);` |
|         - |  508 | `/*` |
|         - |  509 | ` * Each registered constant [i.e: __TIME__, __DATE__, PHP_OS, INT_MAX, etc.] is stored` |
|         - |  510 | ` * in an instance of the following structure.` |
|         - |  511 | ` * Please refer to the official documentation for more information` |
|         - |  512 | ` * on how to create/install foreign constants.` |
|         - |  513 | ` */` |
|         - |  514 | `typedef struct ph7_constant ph7_constant;` |
|         - |  515 | `struct ph7_constant` |
|         - |  516 | `{` |
|         - |  517 | `	SyString sName;        /* Constant name */` |
|         - |  518 | `	ProcConstant xExpand;  /* Function responsible of expanding constant value */` |
|         - |  519 | `	void *pUserData;       /* Last argument to xExpand() */` |
|         - |  520 | `	SyString sFile;        /* Defining file (aliases the VM-lifetime dup in pVm->aFiles);` |
|         - |  521 | `	                        * nByte == 0 = unknown/engine constant */` |
|         - |  522 | ``	sxu32 nLine;           /* Declaration line for `const`; 0 for define()/engine */`` |
|         - |  523 | `	sxu8 bUserDefined;     /* 1 when created by user code (const / define()):` |
|         - |  524 | `	                        * Reflection isInternal()/getFileName() input */` |
|         - |  525 | `	const char *zDeprecated; /* php's reason clause when the SYMBOL is deprecated` |
|         - |  526 | `	                        * ("8.1, as the constant has no effect"), else NULL.` |
|         - |  527 | `	                        * Static storage. Naming the constant raises php's` |
|         - |  528 | `	                        * E_DEPRECATED; LISTING the table does not. */` |
|         - |  529 | `	SySet aAttrs;          /* Declared #[...] attributes (ph7_attribute records) —` |
|         - |  530 | ``	                        * php 8.5 attributes on `const` statements */`` |
|         - |  531 | `};` |
|         - |  532 | `typedef struct ph7_aux_data ph7_aux_data;` |
|         - |  533 | `/*` |
|         - |  534 | ` * Auxiliary data associated with each foreign function is stored` |
|         - |  535 | ` * in a stack of the following structure.` |
|         - |  536 | ` * Note that automatic tracked chunks are also stored in an instance` |
|         - |  537 | ` * of this structure.` |
|         - |  538 | ` */` |
|         - |  539 | `struct ph7_aux_data` |
|         - |  540 | `{` |
|         - |  541 | `	void *pAuxData; /* Aux data */` |
|         - |  542 | `};` |
|         - |  543 | `/* Foreign functions signature */` |
|         - |  544 | `typedef int (*ProchHostFunction)(ph7_context *,int,ph7_value **);` |
|         - |  545 | `/*` |
|         - |  546 | ` * Each installed foreign function is recored in an instance of the following` |
|         - |  547 | ` * structure.` |
|         - |  548 | ` * Please refer to the official documentation for more information on how` |
|         - |  549 | ` * to create/install foreign functions.` |
|         - |  550 | ` */` |
|         - |  551 | `/*` |
|         - |  552 | ` * One name php 8.x deprecated, and the clause it ends the notice with.` |
|         - |  553 | ` *` |
|         - |  554 | `` * The SUBJECT is php's own spelling -- `curl_close` for a function,`` |
|         - |  555 | `` * `SplObjectStorage::attach` for a method, where php always names the`` |
|         - |  556 | ` * DECLARING class even for a call through a subclass -- so the raise site` |
|         - |  557 | ` * needs no class lookup of its own. Static storage, shared by the E_DEPRECATED` |
|         - |  558 | `` * notice and the export format's `<internal, deprecated:EXT>` tag.`` |
|         - |  559 | ` */` |
|         - |  560 | `typedef struct ph7_deprecated_name ph7_deprecated_name;` |
|         - |  561 | `struct ph7_deprecated_name` |
|         - |  562 | `{` |
|         - |  563 | `	const char *zName;  /* php's subject, without the trailing "()" */` |
|         - |  564 | `	const char *zWhy;   /* what follows "is deprecated since ": "8.2",` |
|         - |  565 | `	                     * "8.5, use method SplObjectStorage::offsetSet() instead" */` |
|         - |  566 | `};` |
|         - |  567 | `struct ph7_user_func` |
|         - |  568 | `{` |
|         - |  569 | `	ph7_vm *pVm;              /* VM that own this instance */` |
|         - |  570 | `	SyString sName;           /* Foreign function name */` |
|         - |  571 | `	ProchHostFunction xFunc;  /* Implementation of the foreign function */` |
|         - |  572 | `	void *pUserData;          /* User private data [Refer to the official documentation for more information]*/` |
|         - |  573 | `	SySet aAux;               /* Stack of auxiliary data [Refer to the official documentation for more information]*/` |
|         - |  574 | `	sxi16 nMinArg;            /* Minimum required arguments for the PHP-8 ArgumentCountError` |
|         - |  575 | `	                           * check at the OP_CALL choke point; 0 = no central enforcement` |
|         - |  576 | `	                           * (the builtin self-validates, or genuinely accepts zero args). */` |
|         - |  577 | `	sxu8 bAtLeast;            /* 0 -> "expects exactly N", 1 -> "expects at least N" (the` |
|         - |  578 | `	                           * wording depends on whether the builtin has optional params). */` |
|         - |  579 | `	sxu8 bHasMaxArg;          /* 0 -> no central too-many-arguments check (the SAFE default: this` |
|         - |  580 | `	                           * struct is SyZero'd on creation, so an unstamped builtin must mean` |
|         - |  581 | `	                           * "unenforced", never "accepts at most zero"). 1 -> nMaxArg applies. */` |
|         - |  582 | `	sxi16 nMaxArg;            /* Maximum accepted arguments when bHasMaxArg; derived from the` |
|         - |  583 | `	                           * signature. A variadic parameter leaves bHasMaxArg at 0. */` |
|         - |  584 | `	const char *zSig;         /* PHP-style parameter list ("string $s, int $o = 0") from the` |
|         - |  585 | `	                           * static signature table, or NULL: ReflectionFunction input for` |
|         - |  586 | `	                           * internal functions. Points at static storage — never freed. */` |
|         - |  587 | `	const char *zRet;         /* Return-type text from the same table, or NULL */` |
|         - |  588 | `	const ph7_deprecated_name *pDeprecated; /* php's deprecation for this name, or NULL. A` |
|         - |  589 | `	                           * NATIVE method reaches it through ph7_vm_func::pNative, so one` |
|         - |  590 | `	                           * field covers both a C builtin and a native class method --` |
|         - |  591 | `	                           * they share this struct and the same OP_CALL block. */` |
|         - |  592 | ``	sxu32 nByRefMask;         /* D1: bit N set => positional parameter N is by-reference (`&$p` in zSig),`` |
|         - |  593 | `	                           * derived once in VmSetBuiltinSignatures. Lets OP_CALL materialize a` |
|         - |  594 | `	                           * deferred by-ref out-param regardless of how the builtin was reached` |
|         - |  595 | ``	                           * (bare name, dynamic `$f=...`, or callable) — the compile-time`` |
|         - |  596 | `	                           * GenStateByRefBuiltinMask only sees the bare-name case. 0 when unstamped. */` |
|         - |  597 | `	sxu32 nPathMask;          /* Which of this builtin's arguments php reads with Z_PARAM_PATH,` |
|         - |  598 | `	                           * so a NUL inside one is a catchable ValueError rather than a` |
|         - |  599 | `	                           * truncated read. Derived from a ~70-name table, and the two` |
|         - |  600 | `	                           * questions the shared argument screen used to ask by SCANNING` |
|         - |  601 | `	                           * that table (and a second one) on every single builtin call.` |
|         - |  602 | `	                           * Both answers depend on the NAME alone, so they are worked out` |
|         - |  603 | `	                           * the first time this function is called and kept here. */` |
|         - |  604 | `	sxu8 bSelfChecked;        /* This builtin words its own argument refusals and must not be` |
|         - |  605 | `	                           * pre-empted by the shared screen (php overloads it on arity, or` |
|         - |  606 | `	                           * its declared type and its refusal text disagree). */` |
|         - |  607 | `	sxu8 bScreenStamped;      /* Everything this screen keeps on the record -- nPathMask,` |
|         - |  608 | `	                           * bSelfChecked, nSigLen and the aSigParam table -- has been` |
|         - |  609 | `	                           * worked out. The struct is SyZero'd at creation, so 0 means` |
|         - |  610 | `	                           * "not yet" and never "no".` |
|         - |  611 | `	                           * Stamped lazily rather than at VM init because a NATIVE` |
|         - |  612 | `	                           * METHOD's record is reached through ph7_vm_func::pNative and` |
|         - |  613 | `	                           * is not in the host function table the init pass walks. */` |
|         - |  614 | `	VmArgScreenParam *aSigParam; /* zSig's parameters, parsed ONCE (see the struct above and` |
|         - |  615 | `	                           * VmArgScreenStamp). 0 when there are none, or when the` |
|         - |  616 | `	                           * allocation failed -- the screen then walks the text, which` |
|         - |  617 | `	                           * is the same code and the same answers, just per call. */` |
|         - |  618 | `	sxu16 nSigParam;          /* how many aSigParam holds; beyond it the screen stops, which` |
|         - |  619 | `	                           * is what the walk did at a variadic tail or a malformed row. */` |
|         - |  620 | `	sxu32 nSigLen;            /* SyStrlen(zSig), worked out with the rest. zSig is a` |
|         - |  621 | `	                           * literal from aBuiltinSig[] (or a native method's table) and` |
|         - |  622 | `	                           * is assigned exactly once, so its length is a constant of the` |
|         - |  623 | `	                           * DECLARATION -- but the shared argument screen measured it on` |
|         - |  624 | `	                           * every call, which on the ecosystem gate's phpcs step was` |
|         - |  625 | `	                           * 425,987,828 bytes of strlen across 11,391,725 calls.` |
|         - |  626 | `	                           * (PERF.md P13.) Only meaningful once bScreenStamped. */` |
|         - |  627 | `};` |
|         - |  628 | `/*` |
|         - |  629 | ` * The 'context' argument for an installable function. A pointer to an` |
|         - |  630 | ` * instance of this structure is the first argument to the routines used` |
|         - |  631 | ` * implement the foreign functions.` |
|         - |  632 | ` */` |
|         - |  633 | `typedef struct VmCallArgMap VmCallArgMap; /* Forward decl; full definition below. */` |
|         - |  634 | `struct ph7_context` |
|         - |  635 | `{` |
|         - |  636 | `	ph7_user_func *pFunc;   /* Function information. */` |
|         - |  637 | `	ph7_value *pRet;        /* Return value is stored here. */` |
|         - |  638 | `	SySet sVar;             /* Container of dynamically allocated ph7_values` |
|         - |  639 | `							 * [i.e: Garbage collection purposes.]` |
|         - |  640 | `							 */` |
|         - |  641 | `	SySet sChunk;           /* Track dynamically allocated chunks [ph7_aux_data instance].` |
|         - |  642 | `							 * [i.e: Garbage collection purposes.]` |
|         - |  643 | `							 */` |
|         - |  644 | `	ph7_vm *pVm;            /* Virtual machine that own this context */` |
|         - |  645 | `	sxi32 iFlags;           /* Call flags */` |
|         - |  646 | `	sxi32 nThrowRc;         /* Status of a throw this host function raised through` |
|         - |  647 | `	                         * PH7_VmThrowException (0 when it never threw). The` |
|         - |  648 | `	                         * OP_CALL boundary re-reads it: a builtin that threw and` |
|         - |  649 | `	                         * still returned PH7_OK would otherwise let the VM carry` |
|         - |  650 | `	                         * on inside the try the throw abandoned. See` |
|         - |  651 | `	                         * VmHostFuncThrowRc(). */` |
|         - |  652 | `	VmCallArgMap *pArgMap;  /* Call-site named-argument map (or 0). Lets a builtin` |
|         - |  653 | `	                         * such as call_user_func forward its callers' name:` |
|         - |  654 | `	                         * arguments to the inner callback. */` |
|         - |  655 | `	ph7_class_instance *pThis; /* VM_FUNC_NATIVE method only: the receiver, or 0 for a static` |
|         - |  656 | `	                         * call and for every plain host function. Read through` |
|         - |  657 | `	                         * PH7_ContextThis(); the reference is owned by the CALLER for the` |
|         - |  658 | `	                         * duration of the call, so a native body must not unref it. */` |
|         - |  659 | `	ph7_class *pCalledClass;/* VM_FUNC_NATIVE method only: the class the call was made` |
|         - |  660 | `	                         * THROUGH (php's late-static-binding target), which for an` |
|         - |  661 | `	                         * inherited method is the subclass, not the declaring class. 0 for` |
|         - |  662 | `	                         * a plain host function. */` |
|         - |  663 | `	ph7_value sThis;        /* Scratch MEMOBJ_OBJ view of pThis, materialized on the first` |
|         - |  664 | `	                         * PH7_ContextThisValue() call so a native body can reach the` |
|         - |  665 | `	                         * receiver through the ordinary ph7_value object helpers` |
|         - |  666 | `	                         * (ph7_object_fetch_attr & co). bThisInit gates the lazy init;` |
|         - |  667 | `	                         * VmReleaseCallContext tears it down. */` |
|         - |  668 | `	sxu8 bThisInit;         /* 1 once sThis has been initialized */` |
|         - |  669 | `	struct PH7_NativePropCtx *pPropCtx; /* Non-zero while this SCRATCH context is running a native` |
|         - |  670 | `	                         * class's property handler (ph7_class::xProp). A handler shares` |
|         - |  671 | `	                         * its bodies with the ordinary method path, and those raise a` |
|         - |  672 | `	                         * refusal by throwing -- which here would run the enclosing catch` |
|         - |  673 | `	                         * in the middle of the member opcode. With this set the DOM` |
|         - |  674 | `	                         * refusal helpers RECORD into the hook's context instead, and the` |
|         - |  675 | `	                         * opcode raises it where the access would have landed. */` |
|         - |  676 | `};` |
|         - |  677 | `/*` |
|         - |  678 | ` * Each hashmap entry [i.e: array(4,5,6)] is recorded in an instance` |
|         - |  679 | ` * of the following structure.` |
|         - |  680 | ` */` |
|         - |  681 | `/* Allowed hashmap node key types (iType below) */` |
|         - |  682 | `#define HASHMAP_INT_NODE   1  /* Node with an int [i.e: 64-bit integer] key */` |
|         - |  683 | `#define HASHMAP_BLOB_NODE  2  /* Node with a string/BLOB key */` |
|         - |  684 | `/* Node control flags (iFlags below) */` |
|         - |  685 | `#define HASHMAP_NODE_FOREIGN_OBJ 0x001 /* Node holds a reference to a foreign ph7_value` |
|         - |  686 | `                                        * [i.e: array(&var) / $a[] =& $var ] */` |
|         - |  687 | `/*` |
|         - |  688 | ` * A string key of up to this many bytes lives INSIDE its node, and xKey.sKey is pointed` |
|         - |  689 | ` * at it (SXBLOB_STATIC, so nothing grows it and nothing frees it). Every reader still` |
|         - |  690 | ` * goes through SyBlobData()/SyBlobLength(), so none of the sixty-odd places that read a` |
|         - |  691 | ` * node's key changed.` |
|         - |  692 | ` *` |
|         - |  693 | ` * Why it is free: the node is 96 bytes and the pool serves it out of a 128-byte chunk` |
|         - |  694 | ` * (96 + the pool's own 8-byte header rounds up), so 24 bytes were already being paid for` |
|         - |  695 | ` * and thrown away. 96 + 24 = 120, +8 = 128 -- the same chunk, to the byte.` |
|         - |  696 | ` *` |
|         - |  697 | ` * Why it is worth having: a string-keyed lookup compares the key bytes through` |
|         - |  698 | ` * sKey.pBlob, which used to be a SEPARATE allocation somewhere else in the heap. Counted` |
|         - |  699 | ` * on the ecosystem gate's phpcs step, that is 114,418,815 dereferences to a foreign cache` |
|         - |  700 | ` * line in one run, for keys averaging 7.7 bytes -- while the three tests in front of them` |
|         - |  701 | ` * (iType, nHash, length) all live in the node's first cache line. It also deletes one` |
|         - |  702 | ` * allocation per string-keyed node. (PERF.md P9.)` |
|         - |  703 | ` *` |
|         - |  704 | ` * A key LONGER than this keeps the old arrangement, so the size is a tuning constant and` |
|         - |  705 | ` * not a limit. Nothing appends to a node's key after HashmapNewBlobNode builds it -- if` |
|         - |  706 | ` * that ever changes, the LOCKED blob would silently truncate rather than grow.` |
|         - |  707 | ` */` |
|         - |  708 | `#define HASHMAP_NODE_INLINE_KEY 24` |
|         - |  709 | `struct ph7_hashmap_node` |
|         - |  710 | `{` |
|         - |  711 | `	ph7_hashmap *pMap;     /* Hashmap that own this instance */` |
|         - |  712 | `	sxi32 iType;           /* Node type */` |
|         - |  713 | `	union{` |
|         - |  714 | `		sxi64 iKey;        /* Int key */` |
|         - |  715 | `		SyBlob sKey;       /* Blob key */` |
|         - |  716 | `	}xKey;` |
|         - |  717 | `	sxi32 iFlags;          /* Control flags */` |
|         - |  718 | `	sxu32 nHash;           /* Key hash value */` |
|         - |  719 | `	sxu32 nValIdx;         /* Value stored in this node */` |
|         - |  720 | `	ph7_hashmap_node *pNext,*pPrev;               /* Link to other entries [i.e: linear traversal] */` |
|         - |  721 | `	ph7_hashmap_node *pNextCollide,*pPrevCollide; /* Collision chain */` |
|         - |  722 | `	char zKey[HASHMAP_NODE_INLINE_KEY];           /* A short blob key, in the node itself */` |
|         - |  723 | `};` |
|         - |  724 | `/*` |
|         - |  725 | ` * Each active hashmap aka array in the PHP jargon is represented` |
|         - |  726 | ` * by an instance of the following structure.` |
|         - |  727 | ` */` |
|         - |  728 | `struct ph7_hashmap` |
|         - |  729 | `{` |
|         - |  730 | `	ph7_vm *pVm;                  /* VM that own this instance */` |
|         - |  731 | `	ph7_hashmap_node **apBucket;  /* Hash bucket */` |
|         - |  732 | `	ph7_hashmap_node *pFirst;     /* First inserted entry */` |
|         - |  733 | `	ph7_hashmap_node *pLast;      /* Last inserted entry */` |
|         - |  734 | `	ph7_hashmap_node *pCur;       /* Current entry */` |
|         - |  735 | `	sxu32 nSize;                  /* Bucket size */` |
|         - |  736 | `	sxu32 nEntry;                 /* Total number of inserted entries */` |
|         - |  737 | `	sxu32 (*xIntHash)(sxi64);     /* Hash function for int_keys */` |
|         - |  738 | `	sxu32 (*xBlobHash)(const void *,sxu32); /* Hash function for blob_keys */` |
|         - |  739 | `	sxi64 iNextIdx;               /* Next available automatically assigned index */` |
|         - |  740 | `	sxu8 bIntKeySeen;             /* An integer key has been inserted at least once. php 8.3` |
|         - |  741 | `	                               * carries the auto-index through NEGATIVE keys: the first` |
|         - |  742 | `	                               * int key sets the next index to key+1 even when negative` |
|         - |  743 | `	                               * ($a[-4]=x; $a[]=y stores y at -3), where it used to` |
|         - |  744 | `	                               * restart at 0. Only the FIRST key may move the index` |
|         - |  745 | `	                               * downwards, hence the flag. */` |
|         - |  746 | `	sxi64 iMaxIntKey;             /* Upper bound on the integer keys this map has held. Read` |
|         - |  747 | `	                               * only to decide whether the auto-index advance has to` |
|         - |  748 | `	                               * SCAN for a free slot: it can find one occupied only when` |
|         - |  749 | `	                               * a key ABOVE the one just inserted exists, which is` |
|         - |  750 | ``	                               * exactly `iNextIdx <= iMaxIntKey`. Without the test every`` |
|         - |  751 | `` 	                               * int-keyed store paid a failing hash lookup -- `$a[$i]=$i` `` |
|         - |  752 | ``	                               * over 200k keys ran 6x slower than `$a[]=$i`, and the`` |
|         - |  753 | `	                               * key-preserving array builtins inherited it. A stale-HIGH` |
|         - |  754 | `	                               * bound (a key that was since removed, a renumbering) only` |
|         - |  755 | `	                               * costs an extra scan, so it is never lowered. */` |
|         - |  756 | `	sxi32 iRef;                   /* Reference count. INVARIANT: the number of` |
|         - |  757 | `								   * SHARERS for copy-on-write purposes is` |
|         - |  758 | `								   * iRef minus the by-REFERENCE foreach steps` |
|         - |  759 | `								   * on pActiveSteps (a by-ref loop iterates` |
|         - |  760 | `								   * the LIVE map, php semantics) — any future` |
|         - |  761 | `								   * separate/dup gate must use the discounted` |
|         - |  762 | `								   * count like PH7_HashmapCowSeparate, never` |
|         - |  763 | `								   * raw iRef. */` |
|         - |  764 | `	sxi32 iFlags;                 /* Control flags (see HASHMAP_* below) */` |
|         - |  765 | `	sxu32 nGcRoot;                /* 1-based row in the collector's root buffer, 0 while unbuffered */` |
|         - |  766 | `	sxu8 iGcColor;                /* PH7_GC_* -- see vm_gc.c */` |
|         - |  767 | `	ph7_foreach_step *pActiveSteps; /* foreach steps currently iterating this map` |
|         - |  768 | `									 * (per-step cursors — PH7_HashmapUnlinkNode` |
|         - |  769 | `									 * advances any cursor parked on a dying node,` |
|         - |  770 | `									 * node link re-arms parked cursors) */` |
|         - |  771 | `};` |
|         - |  772 | `/*` |
|         - |  773 | ` * Hashmap control flags.` |
|         - |  774 | ` */` |
|         - |  775 | `#define HASHMAP_COUNTING 0x01 /* Set during recursive count to detect cycles */` |
|         - |  776 | `#define HASHMAP_DUMPING  0x02 /* Set during var_export recursion to detect cycles */` |
|         - |  777 | `/* An instance of the following structure is the context` |
|         - |  778 | ` * for the FOREACH_STEP/FOREACH_INIT VM instructions.` |
|         - |  779 | ` * Those instructions are used to implement the 'foreach'` |
|         - |  780 | ` * statement.` |
|         - |  781 | ` * This structure is made available to these instructions` |
|         - |  782 | ` * as the P3 operand.` |
|         - |  783 | ` */` |
|         - |  784 | `struct ph7_foreach_info` |
|         - |  785 | `{` |
|         - |  786 | `	SyString sKey;      /* Key name. Empty otherwise*/` |
|         - |  787 | `	SyString sValue;    /* Value name */` |
|         - |  788 | `	sxi32 iFlags;       /* Control flags */` |
|         - |  789 | `	SySet aStep;        /* Stack of steps [i.e: ph7_foreach_step instance] */` |
|         - |  790 | `};` |
|         - |  791 | `/*` |
|         - |  792 | ` * One live walk of ONE object's property table.` |
|         - |  793 | ` *` |
|         - |  794 | ` * An object's attributes live in a SyHash, and SyHashGetNextEntry() shares a` |
|         - |  795 | `` * single cursor embedded in the table — which is wrong twice for `foreach`:`` |
|         - |  796 | ` * nested loops over one object rewind each other (an infinite loop, since the` |
|         - |  797 | ` * inner walk always leaves the cursor at the head), and the cursor is advanced` |
|         - |  798 | ` * before the body runs, so a body that unset()s the property the walk is about` |
|         - |  799 | ` * to reach freed the entry the cursor held. This is the object twin of the` |
|         - |  800 | `` * hashmap's per-loop `ph7_foreach_step::pCursor` + `pActiveSteps` pair: every`` |
|         - |  801 | ` * walker keeps its own position, and the instance keeps the list of walkers so` |
|         - |  802 | ` * an attribute added or removed under them can fix their cursors up.` |
|         - |  803 | ` */` |
|         - |  804 | `typedef struct PH7_AttrIter PH7_AttrIter;` |
|         - |  805 | `struct PH7_AttrIter` |
|         - |  806 | `{` |
|         - |  807 | `	SyHashEntry *pCursor;   /* Next attribute entry to yield; 0 once exhausted */` |
|         - |  808 | `	PH7_AttrIter *pNextIter;/* Next live walker on the instance */` |
|         - |  809 | `};` |
|         - |  810 | `struct ph7_foreach_step` |
|         - |  811 | `{` |
|         - |  812 | `	sxi32 iFlags;                   /* Control flags (see below) */` |
|         - |  813 | `	/* Iterate on those values */` |
|         - |  814 | `	union {` |
|         - |  815 | `		ph7_hashmap *pMap;          /* Hashmap [i.e: array in the PHP jargon] iteration` |
|         - |  816 | `									 * Ex: foreach(array(1,2,3) as $key=>$value){}` |
|         - |  817 | `									 */` |
|         - |  818 | `		ph7_class_instance *pThis;  /* Class instance [i.e: object] iteration */` |
|         - |  819 | `	}xIter;` |
|         - |  820 | `	ph7_class_instance *pOwner;     /* IteratorAggregate: keeps aggregate alive during foreach */` |
|         - |  821 | `	ph7_hashmap_node *pCursor;      /* Hashmap iteration: this loop's PRIVATE cursor.` |
|         - |  822 | `									 * php iterates each foreach independently — the map's` |
|         - |  823 | `									 * shared pCur would make nested loops over one array` |
|         - |  824 | `									 * rewind each other (infinite loop). */` |
|         - |  825 | `	struct VmFrame *pFrame;         /* Owning activation's frame (normalized past exception` |
|         - |  826 | `									 * frames). aStep is per-STATEMENT and shared by every` |
|         - |  827 | `									 * activation; OP_FOREACH_STEP selects the step whose` |
|         - |  828 | `									 * pFrame matches the running activation so two suspended` |
|         - |  829 | `									 * instances of one generator/fiber (or a recursive call)` |
|         - |  830 | `									 * paused in the same textual foreach cannot clash on` |
|         - |  831 | `									 * each other's cursor. */` |
|         - |  832 | `	ph7_foreach_step *pNextActive;  /* Next step on the map's pActiveSteps list */` |
|         - |  833 | `	ph7_foreach_info *pInfo;        /* The statement this step belongs to. Carried so the OWNING` |
|         - |  834 | `	                                 * FRAME can tear the step down without knowing which foreach` |
|         - |  835 | `	                                 * it came from (see pNextFrameStep). */` |
|         - |  836 | `	ph7_foreach_step *pNextFrameStep;/* Next step owned by the same activation (VmFrame::pForeachSteps).` |
|         - |  837 | `	                                 * A loop left through break/return/goto/an exception never` |
|         - |  838 | `	                                 * reaches the "no more entries" arm, so its step used to sit on` |
|         - |  839 | `	                                 * the per-STATEMENT aStep until the VM died -- reclaimed only if` |
|         - |  840 | `	                                 * a LATER activation happened to be handed the same frame` |
|         - |  841 | `	                                 * address. aStep therefore grew without bound, and INIT's` |
|         - |  842 | `	                                 * linear reclaim scan over it made every foreach in the program` |
|         - |  843 | `	                                 * quadratic. The frame that owns a step is the one that can` |
|         - |  844 | `	                                 * always end it: this list is how it finds them. */` |
|         - |  845 | `	PH7_AttrIter sAttrIter;         /* Object iteration: this loop's PRIVATE cursor over the` |
|         - |  846 | `	                                 * instance's property table (see PH7_AttrIter) */` |
|         - |  847 | `};` |
|         - |  848 | `/* Foreach step control flags */` |
|         - |  849 | `#define PH7_4EACH_STEP_HASHMAP 0x001 /* Hashmap iteration */` |
|         - |  850 | `#define PH7_4EACH_STEP_OBJECT  0x002 /* Object  iteration */` |
|         - |  851 | `#define PH7_4EACH_STEP_KEY     0x004 /* Make Key available */` |
|         - |  852 | `#define PH7_4EACH_STEP_REF     0x008 /* Pass value by reference not copy */` |
|         - |  853 | `#define PH7_4EACH_STEP_LIST    0x010 /* Value target is list() — destructure */` |
|         - |  854 | `#define PH7_4EACH_STEP_ITERATOR 0x020 /* Object implements Iterator */` |
|         - |  855 | `#define PH7_4EACH_STEP_FIRST    0x040 /* First iteration (skip next() call) */` |
|         - |  856 | `/*` |
|         - |  857 | ` * Each PH7 engine is identified by an instance of the following structure.` |
|         - |  858 | ` * Please refer to the official documentation for more information` |
|         - |  859 | ` * on how to configure your PH7 engine instance.` |
|         - |  860 | ` */` |
|         - |  861 | `struct ph7` |
|         - |  862 | `{` |
|         - |  863 | `	SyMemBackend sAllocator;     /* Low level memory allocation subsystem */` |
|         - |  864 | `	const ph7_vfs *pVfs;         /* Underlying Virtual File System */` |
|         - |  865 | `	ph7_conf xConf;              /* Configuration */` |
|         - |  866 | `#if defined(PH7_ENABLE_THREADS)` |
|         - |  867 | `	const SyMutexMethods *pMethods;  /* Mutex methods */` |
|         - |  868 | `	SyMutex *pMutex;                 /* Per-engine mutex */` |
|         - |  869 | `#endif` |
|         - |  870 | `	ph7_vm *pVms;      /* List of active VM */` |
|         - |  871 | `	sxi32 iVm;         /* Total number of active VM */` |
|         - |  872 | `	ph7 *pNext,*pPrev; /* List of active engines */` |
|         - |  873 | `	sxu32 nMagic;      /* Sanity check against misuse */` |
|         - |  874 | `};` |
|         - |  875 | `/* Code generation data structures */` |
|         - |  876 | `typedef sxi32 (*ProcErrorGen)(void *,sxi32,sxu32,const char *,...);` |
|         - |  877 | `typedef struct ph7_expr_node   ph7_expr_node;` |
|         - |  878 | `typedef struct ph7_expr_op     ph7_expr_op;` |
|         - |  879 | `typedef struct ph7_gen_state   ph7_gen_state;` |
|         - |  880 | `/*` |
|         - |  881 | ` * Lexer trivia sidecar record: a doc-comment (or, later, an attribute` |
|         - |  882 | ` * group) captured OUT of the token stream, keyed by the index the NEXT` |
|         - |  883 | ` * real token receives in the chunk's token set. sText points into the` |
|         - |  884 | ` * raw script buffer — consumers must duplicate before the buffer dies.` |
|         - |  885 | ` */` |
|         - |  886 | `typedef struct ph7_trivia ph7_trivia;` |
|         - |  887 | `struct ph7_trivia` |
|         - |  888 | `{` |
|         - |  889 | `	sxu32 nTokIdx;   /* Index of the next real token in the chunk token set */` |
|         - |  890 | `	sxu8  iKind;     /* PH7_TRIVIA_* */` |
|         - |  891 | `	SyString sText;  /* Raw span (docblock includes its delimiters) */` |
|         - |  892 | `	sxu32 nLine;     /* Line the trivia starts on */` |
|         - |  893 | `};` |
|         - |  894 | `#define PH7_TRIVIA_DOC  1 /* A doc-comment: slash-star-star ... star-slash */` |
|         - |  895 | `#define PH7_TRIVIA_ATTR 2 /* An attribute group: the span between #[ and its ] */` |
|         - |  896 | `/*` |
|         - |  897 | ` * One compiled attribute argument: an optional name (named argument) and` |
|         - |  898 | ` * the constant expression's bytecode, evaluated lazily at` |
|         - |  899 | ` * ReflectionAttribute::getArguments()/newInstance() time (PHP's` |
|         - |  900 | ` * lazy-instantiation semantics).` |
|         - |  901 | ` */` |
|         - |  902 | `typedef struct ph7_attr_arg ph7_attr_arg;` |
|         - |  903 | `struct ph7_attr_arg` |
|         - |  904 | `{` |
|         - |  905 | `	SyString sName;   /* Named-argument name (duplicated); nByte == 0 = positional */` |
|         - |  906 | `	SySet aByteCode;  /* Compiled expression, OP_DONE(p1=1) terminated (VmInstr) */` |
|         - |  907 | `	const void *pNativeValue; /* A NATIVE attribute's literal (PH7_NativeConstDef *), or 0.` |
|         - |  908 | ``	                   * php declares `#[Attribute(Attribute::TARGET_CLASS)]` on two of its`` |
|         - |  909 | `	                   * own classes, and a class declared from C has no compiler to emit` |
|         - |  910 | `	                   * byte-code for the argument — so the value rides as a literal and` |
|         - |  911 | `	                   * every reader takes this branch when aByteCode is empty. */` |
|         - |  912 | `};` |
|         - |  913 | `/*` |
|         - |  914 | ` * One #[...] attribute as declared: the compile-time-resolved FQN and its` |
|         - |  915 | ` * argument list.` |
|         - |  916 | ` */` |
|         - |  917 | `typedef struct ph7_attribute ph7_attribute;` |
|         - |  918 | `struct ph7_attribute` |
|         - |  919 | `{` |
|         - |  920 | `	SyString sName;   /* Fully-qualified class name (resolved via use imports /` |
|         - |  921 | `	                   * current namespace at compile time; duplicated) */` |
|         - |  922 | `	SySet aArgs;      /* ph7_attr_arg records */` |
|         - |  923 | `	sxu32 nLine;      /* Line the attribute appears on */` |
|         - |  924 | `};` |
|         - |  925 | `typedef struct GenBlock        GenBlock;` |
|         - |  926 | `typedef sxi32 (*ProcLangConstruct)(ph7_gen_state *);` |
|         - |  927 | `typedef sxi32 (*ProcNodeConstruct)(ph7_gen_state *,sxi32);` |
|         - |  928 | `/*` |
|         - |  929 | ` * Each supported operator [i.e: +, -, ==, *, %, >>, >=, new, etc.] is represented` |
|         - |  930 | ` * by an instance of the following structure.` |
|         - |  931 | ` * The PH7 parser does not use any external tools and is 100% handcoded.` |
|         - |  932 | ` * That is, the PH7 parser is thread-safe ,full reentrant, produce consistant` |
|         - |  933 | ` * compile-time errrors and at least 7 times faster than the standard PHP parser.` |
|         - |  934 | ` */` |
|         - |  935 | `struct ph7_expr_op` |
|         - |  936 | `{` |
|         - |  937 | `	SyString sOp;   /* String representation of the operator [i.e: "+","*","=="...] */` |
|         - |  938 | `	sxi32 iOp;      /* Operator ID */` |
|         - |  939 | `	sxi32 iPrec;    /* Operator precedence: 1 == Highest */` |
|         - |  940 | `	sxi32 iAssoc;   /* Operator associativity (either left,right or non-associative) */` |
|         - |  941 | `	sxi32 iVmOp;    /* VM OP code for this operator [i.e: PH7_OP_EQ,PH7_OP_LT,PH7_OP_MUL...]*/` |
|         - |  942 | `};` |
|         - |  943 | `/*` |
|         - |  944 | ` * Each expression node is parsed out and recorded` |
|         - |  945 | ` * in an instance of the following structure.` |
|         - |  946 | ` */` |
|         - |  947 | `struct ph7_expr_node` |
|         - |  948 | `{` |
|         - |  949 | `	const ph7_expr_op *pOp;  /* Operator ID or NULL if literal, constant, variable, function or class method call */` |
|         - |  950 | `	ph7_expr_node *pLeft;    /* Left expression tree */` |
|         - |  951 | `	ph7_expr_node *pRight;   /* Right expression tree */` |
|         - |  952 | `	SyToken *pStart;         /* Stream of tokens that belong to this node */` |
|         - |  953 | `	SyToken *pEnd;           /* End of token stream */` |
|         - |  954 | `	sxi32 iFlags;            /* Node construct flags */` |
|         - |  955 | `	ProcNodeConstruct xCode; /* C routine responsible of compiling this node */` |
|         - |  956 | `	SySet aNodeArgs;         /* Node arguments. Only used by postfix operators [i.e: function call]*/` |
|         - |  957 | `	SyString sArgName;       /* Named argument label (empty if positional) */` |
|         - |  958 | `	ph7_expr_node *pCond;    /* Condition: Only used by the ternary operator '?:' */` |
|         - |  959 | `};` |
|         - |  960 | `/* Node Construct flags */` |
|         - |  961 | `#define EXPR_NODE_PRE_INCR    0x01 /* Pre-icrement/decrement [i.e: ++$i,--$j] node */` |
|         - |  962 | `#define EXPR_NODE_SPREAD      0x02 /* Argument unpacking: ...$expr */` |
|         - |  963 | `#define EXPR_NODE_NAMED_ARG   0x04 /* Named argument: name: $expr */` |
|         - |  964 | `#define EXPR_NODE_PARENS      0x08 /* Root of a parenthesized sub-expression */` |
|         - |  965 | ``#define EXPR_NODE_FCC         0x10 /* First-class callable marker: a lone `...` as the`` |
|         - |  966 | `                                    * whole argument list, e.g. f(...) — wrap the callee` |
|         - |  967 | `                                    * in a Closure instead of calling it. */` |
|         - |  968 | `/*` |
|         - |  969 | ` * A block of instructions is recorded in an instance of the following structure.` |
|         - |  970 | ` * This structure is used only during compile-time and have no meaning` |
|         - |  971 | ` * during bytecode execution.` |
|         - |  972 | ` */` |
|         - |  973 | `struct GenBlock` |
|         - |  974 | `{` |
|         - |  975 | `	ph7_gen_state *pGen;  /* State of the code generator */` |
|         - |  976 | `	GenBlock *pParent;    /* Upper block or NULL if global */` |
|         - |  977 | `	sxu32 nFirstInstr;    /* First instruction to execute  */` |
|         - |  978 | `	sxi32 iFlags;         /* Block control flags (see below) */` |
|         - |  979 | `	SySet aJumpFix;       /* Jump fixup (JumpFixup instance) */` |
|         - |  980 | `	void *pUserData;      /* Upper layer private data */` |
|         - |  981 | `	sxu32 nLoopId;        /* This block's loop/switch id (0 when it is neither) */` |
|         - |  982 | `	sxu32 nOuterLoopId;   /* Loop/switch that was innermost when this one was entered */` |
|         - |  983 | `	sxu32 nScopeId;       /* Try/catch scope in effect INSIDE this block (0 = none). An` |
|         - |  984 | `	                       * exception block mints its own; every other block inherits. */` |
|         - |  985 | `	sxu32 nOuterScopeId;  /* Scope that was innermost when this block was entered */` |
|         - |  986 | `	/* The following two fields are used only when compiling` |
|         - |  987 | `	 * the 'do..while()' language construct.` |
|         - |  988 | `	 */` |
|         - |  989 | `	sxu8 bPostContinue;    /* TRUE when compiling the do..while() statement */` |
|         - |  990 | `	SySet aPostContFix;    /* Post-continue jump fix */` |
|         - |  991 | `};` |
|         - |  992 | `/*` |
|         - |  993 | ` * Code generator state is remembered in an instance of the following` |
|         - |  994 | ` * structure. We put the information in this structure and pass around` |
|         - |  995 | ` * a pointer to this structure, rather than pass around  all of the` |
|         - |  996 | ` * information separately. This helps reduce the number of  arguments` |
|         - |  997 | ` * to generator functions.` |
|         - |  998 | ` * This structure is used only during compile-time and have no meaning` |
|         - |  999 | ` * during bytecode execution.` |
|         - | 1000 | ` */` |
|         - | 1001 | `struct ph7_gen_state` |
|         - | 1002 | `{` |
|         - | 1003 | `	ph7_vm *pVm;         /* VM that own this instance */` |
|         - | 1004 | `	SyHash hLiteral;     /* Constant string Literals table */` |
|         - | 1005 | `	SyHash hNumLiteral;  /* Numeric literals table */` |
|         - | 1006 | `	SyHash hVar;         /* Collected variable hashtable */` |
|         - | 1007 | `	GenBlock *pCurrent;  /* Current processed block */` |
|         - | 1008 | `	ph7_class *pCurClass; /* Class/interface/trait/enum whose BODY is currently being compiled` |
|         - | 1009 | `	                       * (0 at top level). Saved/restored around each class-body compiler so` |
|         - | 1010 | `	                       * a nested anonymous class overrides it. Lets a const-expression that` |
|         - | 1011 | `	                       * compiles OUTSIDE any function block — a property default or a` |
|         - | 1012 | `	                       * parameter default — resolve __TRAIT__ to the enclosing trait, which` |
|         - | 1013 | `	                       * the block-chain walk alone cannot see (no func block on the chain). */` |
|         - | 1014 | `	ph7_class *pCurBase; /* The BASE CLASS of pCurClass, known while its body compiles --` |
|         - | 1015 | `	                       * pCurClass->pBase is only filled at inheritance, which runs` |
|         - | 1016 | `	                       * AFTER the body. Saved/restored with pCurClass. 0 for an` |
|         - | 1017 | ``	                       * interface (php gives one no `parent` however many it extends)`` |
|         - | 1018 | `	                       * and for a trait (which defers the question to composition). */` |
|         - | 1019 | `	/* Whose SIGNATURE is being parsed, for php's scope-keyword screen -- see iSigScope. */` |
|         - | 1020 | `#define PH7_SIGSCOPE_MEMBER  0` |
|         - | 1021 | `#define PH7_SIGSCOPE_CLOSURE 1` |
|         - | 1022 | `#define PH7_SIGSCOPE_FUNC    2` |
|         - | 1023 | `	int iSigScope;       /* Whose SIGNATURE is being parsed, for php's scope-keyword screen` |
|         - | 1024 | ``	                       * (`self`/`parent`/`static` in a type). Saved and restored around`` |
|         - | 1025 | `	                       * each signature, so a nested one answers for itself:` |
|         - | 1026 | `	                       *   PH7_SIGSCOPE_MEMBER  -- a method, property or class constant:` |
|         - | 1027 | `	                       *                          the enclosing class body's scope applies` |
|         - | 1028 | `	                       *   PH7_SIGSCOPE_CLOSURE -- a closure or arrow function: EXEMPT, its` |
|         - | 1029 | `	                       *                          scope is decided when it is bound` |
|         - | 1030 | `	                       *   PH7_SIGSCOPE_FUNC    -- a named function: NO class scope, even` |
|         - | 1031 | `	                       *                          written inside a method body */` |
|         - | 1032 | `	int iInMemberDefault; /* > 0 while compiling a property/parameter DEFAULT value. Such a` |
|         - | 1033 | `	                       * const-expression belongs to pCurClass, never to a lexically-` |
|         - | 1034 | `	                       * enclosing method, so __TRAIT__ reads pCurClass directly rather than` |
|         - | 1035 | `	                       * walking the block chain (which would leak into the enclosing` |
|         - | 1036 | `	                       * function — e.g. an anonymous class's default inside a trait method). */` |
|         - | 1037 | `	GenBlock sGlobal;    /* Global block */` |
|         - | 1038 | `	ProcConsumer xErr;   /* Error consumer callback */` |
|         - | 1039 | `	void *pErrData;      /* Third argument to xErr() */` |
|         - | 1040 | `	SySet aLabel;        /* Label table */` |
|         - | 1041 | `	SySet aGoto;         /* Gotos table */` |
|         - | 1042 | `	SySet aNullsafeJmp;  /* Pending NULLSAFE_JMP instruction indices (sxu32) */` |
|         - | 1043 | `	int nCommaExprOk;    /* > 0 while compiling a for() clause, the ONLY place php's grammar` |
|         - | 1044 | `	                      * allows a comma-separated expression list (PH7's comma OPERATOR` |
|         - | 1045 | `	                      * is otherwise a PH7-ism php rejects — §10) */` |
|         - | 1046 | `	const char *zClauseCloser; /* When an expression has a trailing token the grammar can't` |
|         - | 1047 | `	                      * absorb, the tree builder names it and, if this is set, says what` |
|         - | 1048 | `` 	                      * the enclosing construct expected: `;` after `return`, `,`/`;` `` |
|         - | 1049 | ``	                      * after `echo`, `)` for a for() post clause, `]` inside an array`` |
|         - | 1050 | `	                      * literal, and so on. Each construct saves/sets/restores it around` |
|         - | 1051 | `	                      * its expression compile. NULL means "no expecting clause" — php` |
|         - | 1052 | `	                      * prints none for a plain expression statement. */` |
|         - | 1053 | `` 	int nExprEchoOk;     /* > 0 only while compiling the synthesized `echo` of a `<?= ... ?>` `` |
|         - | 1054 | `	                      * short tag, which is the one place an echo legitimately compiles` |
|         - | 1055 | ``	                      * as an EXPRESSION. Everywhere else `echo` in expression position`` |
|         - | 1056 | `	                      * is a php parse error (it was a Symisc extension — §10) */` |
|         - | 1057 | `	sxu32 nLoopId;       /* Monotonic id handed to each loop/switch block as it is entered */` |
|         - | 1058 | `	sxu32 nCurLoopId;    /* Innermost loop/switch currently open (0 = none) */` |
|         - | 1059 | `	SySet aLoopParent;   /* aLoopParent[id-1] = enclosing loop id, so the ancestry of any loop` |
|         - | 1060 | `	                      * can be walked after compilation. php's only goto restriction is` |
|         - | 1061 | `	                      * "'goto' into loop or switch statement is disallowed": a jump is` |
|         - | 1062 | `	                      * illegal exactly when the LABEL sits in a loop that does not also` |
|         - | 1063 | `	                      * enclose the GOTO. Both ends record their loop id; the fixup pass` |
|         - | 1064 | `	                      * walks up from the goto's to look for the label's. */` |
|         - | 1065 | `	sxu32 nScopeId;      /* Monotonic id handed to each try/catch/finally block entered */` |
|         - | 1066 | `	sxu32 nCurScopeId;   /* Innermost such block currently open (0 = none) */` |
|         - | 1067 | `	SySet aScope;        /* aScope[id-1] = that block's GenScope: its enclosing scope id and` |
|         - | 1068 | `	                      * its kind. Same shape and purpose as aLoopParent above, for the` |
|         - | 1069 | `	                      * other after-the-fact goto question: a jump out of a try/catch is` |
|         - | 1070 | `	                      * legal exactly when the LABEL's scope also ENCLOSES the goto, and` |
|         - | 1071 | `	                      * what it must unwind on the way is read off the chain between them` |
|         - | 1072 | `	                      * (GenStateJumpScope). Comparing NESTING DEPTHS instead cannot tell` |
|         - | 1073 | `	                      * two sibling trys apart, which let a goto jump into one — skipping` |
|         - | 1074 | `	                      * its OP_LOAD_EXCEPTION, or landing in another bytecode array. */` |
|         - | 1075 | `	SyBlob sWorker;      /* General purpose working buffer */` |
|         - | 1076 | `	SyBlob sErrBuf;      /* Error buffer */` |
|         - | 1077 | `	SyBlob sFirstErr;    /* The BARE text of the FIRST refusal in this unit -- php reports one` |
|         - | 1078 | `	                      * compile-time refusal and stops, and an include's parse error is` |
|         - | 1079 | `	                      * handed to the caller as the message of php's ParseError. */` |
|         - | 1080 | `	sxu32 nFirstErrLine; /* ...and the line it was raised on. */` |
|         - | 1081 | `	sxi32 nFatal;        /* Refusals of E_ERROR severity in this unit. php's E_COMPILE_ERROR:` |
|         - | 1082 | `	                      * uncatchable, where a PARSE error is a catchable ParseError -- so` |
|         - | 1083 | `	                      * nErr says the unit failed and this says WHICH WAY. */` |
|         - | 1084 | `	int iFatalTrace;     /* WHICH stack trace php prints under the refusal being raised -- php's` |
|         - | 1085 | `	                      * three phases, and they answer differently:` |
|         - | 1086 | `	                      *   PH7_FATAL_TRACE_COMPILE (0) the compiler refused: the active frames,` |
|         - | 1087 | `	                      *     WITHOUT the include/require/eval that is loading this unit (php` |
|         - | 1088 | `	                      *     raises it before pushing that activation) -- the common case;` |
|         - | 1089 | `	                      *   PH7_FATAL_TRACE_RUNTIME (1) php makes this one at RUN time, so the` |
|         - | 1090 | `	                      *     activation IS on its trace. A class REDECLARATION is the only one:` |
|         - | 1091 | `	                      *     php cannot early-bind a name it already holds, so DECLARE_CLASS` |
|         - | 1092 | `	                      *     reports it;` |
|         - | 1093 | `	                      *   PH7_FATAL_TRACE_NONE (2) php's PARSER refused, while reading a` |
|         - | 1094 | `	                      *     modifier run, before any op array exists -- it prints no trace at` |
|         - | 1095 | `	                      *     all.` |
|         - | 1096 | `	                      * A ONE-SHOT: the call sites that need a non-default set it just before` |
|         - | 1097 | `	                      * raising, and PH7_GenCompileError consumes it. */` |
|         - | 1098 | `	int bParseThrows;    /* This unit's parse errors are the CALLER's to raise (include/require:` |
|         - | 1099 | `	                      * php throws a ParseError there and prints nothing until it goes` |
|         - | 1100 | `	                      * uncaught). A refusal of E_ERROR severity still prints at once. */` |
|         - | 1101 | `	SyBlob sNamespace;   /* Current namespace path (e.g. "App\\Models") */` |
|         - | 1102 | `	SyHash hUseImports;      /* use imports: short alias -> FQN (classes) */` |
|         - | 1103 | `	SyHash hUseFuncImports;  /* use function imports: short alias -> FQN */` |
|         - | 1104 | `	SyHash hUseConstImports; /* use const imports: short alias -> FQN */` |
|         - | 1105 | `	SyHash hSeenClass;       /* FQNs of the classes DECLARED so far in this compile unit */` |
|         - | 1106 | `	SyHash hSeenFunc;        /* FQNs of the functions DECLARED so far in this compile unit` |
|         - | 1107 | `	                          * (both: php refuses an import a declaration already took —` |
|         - | 1108 | `	                          * these outlive a namespace switch, unlike the import tables) */` |
|         - | 1109 | `	SyToken *pIn;        /* Current processed token */` |
|         - | 1110 | `	SyToken *pEnd;       /* Last token in the stream */` |
|         - | 1111 | `	sxu32 nErr;          /* Total number of compilation error */` |
|         - | 1112 | `	SyToken *pRawIn;     /* Current processed raw token */` |
|         - | 1113 | `	SyToken *pRawEnd;    /* Last raw token in the stream */` |
|         - | 1114 | `	SySet   *pTokenSet;  /* Token containers */` |
|         - | 1115 | `	sxi8 bStrictTypes;       /* Current file's strict_types mode (0 = weak/unset, 1 = strict) */` |
|         - | 1116 | `	sxi8 bStrictTypesLocked; /* 1 once the current file has emitted any non-declare top-level statement */` |
|         - | 1117 | `	sxi8 bChunkAtEof;        /* 1 when the PHP chunk being compiled ran into the end of the` |
|         - | 1118 | ``	                          * FILE rather than being closed by a `?>`. php reads the closing`` |
|         - | 1119 | `	                          * tag as a statement terminator, so only this chunk can leave one` |
|         - | 1120 | `	                          * unfinished -- and that is a parse error there. */` |
|         - | 1121 | `	sxu32 nChunkEofLine;     /* Line the chunk's end-of-input sits on -- its last line, which is` |
|         - | 1122 | `	                          * NOT the last TOKEN's line when trailing blank lines follow. php` |
|         - | 1123 | ``	                          * reports `unexpected end of file` at the former. */`` |
|         - | 1124 | ``	sxi8 bHalted;            /* 1 once `__halt_compiler();` has been compiled in this file:`` |
|         - | 1125 | `	                          * everything after it -- the rest of the chunk, every later chunk` |
|         - | 1126 | `	                          * and every byte of inline text between them -- is DATA, and the` |
|         - | 1127 | `	                          * chunk loop stops. */` |
|         - | 1128 | `	sxi8 bHaltSeen;          /* 1 when the file HAS a halt (found by the pre-scan below, which` |
|         - | 1129 | `	                          * runs before any of it compiles because the offset may be read` |
|         - | 1130 | `	                          * ahead of the statement that sets it). */` |
|         - | 1131 | ``	sxu32 nHaltOffset;       /* What `__COMPILER_HALT_OFFSET__` expands to: the byte offset in`` |
|         - | 1132 | ``	                          * the FILE just past the halt statement's `;` -- a shebang line`` |
|         - | 1133 | `	                          * this compiler skipped included, since php counts from the first` |
|         - | 1134 | `	                          * byte on disk. Meaningful only while bHaltSeen. */` |
|         - | 1135 | `	const char *zScriptBase; /* First byte of the whole script, for the offset above. */` |
|         - | 1136 | `	sxi8 bListSrcNotRef;     /* 1 while compiling the TARGET list of an assignment whose SOURCE` |
|         - | 1137 | ``	                          * cannot hold a reference (`[&$r] = [7];`). php checks this at`` |
|         - | 1138 | `	                          * compile time, where it still knows what the right-hand side was` |
|         - | 1139 | `	                          * written as; by the time a by-ref entry is emitted the source is` |
|         - | 1140 | `	                          * an anonymous value on the stack, so the answer is carried here.` |
|         - | 1141 | ``	                          * A foreach `as` list has no such source and leaves it clear. */`` |
|         - | 1142 | `	sxi8 bInGenerator;       /* ROOT C: 1 while compiling a generator function body (a yield appears at` |
|         - | 1143 | `` 	                          * this function's own level). Gates inline try/catch/finally so `yield` `` |
|         - | 1144 | `	                          * inside a catch/finally suspends correctly; non-generators keep the` |
|         - | 1145 | `	                          * legacy detached-mini-program path. Saved/restored across nested funcs. */` |
|         - | 1146 | `	SySet aTrivia;       /* Trivia sidecar for the current chunk (ph7_trivia records from the` |
|         - | 1147 | `	                      * main-chunk tokenize calls; reset with the token set) */` |
|         - | 1148 | `	SyString sPendingDoc;/* Docblock immediately preceding the statement being dispatched;` |
|         - | 1149 | `	                      * consumed by the declaration compilers, discarded at the next` |
|         - | 1150 | `	                      * statement boundary (points into the raw script buffer) */` |
|         - | 1151 | `	SySet aPendingAttrs; /* Attribute-group trivia (ph7_trivia) bound to the statement being` |
|         - | 1152 | `	                      * dispatched; unlike docs, PHP requires attributes to be adjacent,` |
|         - | 1153 | `	                      * so this resets at every boundary */` |
|         - | 1154 | ``	SyString sPendingClosureName; /* php's `{closure:SCOPE:LINE}` name built for the closure`` |
|         - | 1155 | `	                      * whose body GenStateCompileFunc is about to compile. The caller` |
|         - | 1156 | `	                      * knows the 'function' keyword's LINE and the ENCLOSING scope; the` |
|         - | 1157 | `	                      * name must be on the ph7_vm_func before the body compiles, because` |
|         - | 1158 | `	                      * __FUNCTION__ inside it resolves at compile time. Consumed (and` |
|         - | 1159 | `	                      * cleared) the moment the function state is initialized. */` |
|         - | 1160 | `	SyString sPendingClosureScope; /* ...and the CLASS that closure belongs to, when it was written` |
|         - | 1161 | `	                               * inside a method (or inside a closure that was). php prefixes an` |
|         - | 1162 | ``	                               * argument diagnostic with it -- `C::{closure:C::m():5}` -- and`` |
|         - | 1163 | `	                               * the name above cannot be taken apart for it: a top-level` |
|         - | 1164 | ``	                               * closure's is a FILE PATH, which may hold a `::` of its own. */`` |
|         - | 1165 | `};` |
|         - | 1166 | `/* Forward references */` |
|         - | 1167 | `typedef struct ph7_vm_func_closure_env ph7_vm_func_closure_env;` |
|         - | 1168 | `typedef struct ph7_vm_func_static_var  ph7_vm_func_static_var;` |
|         - | 1169 | `typedef struct ph7_vm_func_arg ph7_vm_func_arg;` |
|         - | 1170 | `typedef struct ph7_vm_func ph7_vm_func;` |
|         - | 1171 | `/*` |
|         - | 1172 | ` * One ACTIVE include/require/eval, as php reports it in a backtrace: a frame whose` |
|         - | 1173 | ` * function is the construct's name, whose file and line are the CALL SITE, and whose` |
|         - | 1174 | ` * single argument is the unit being loaded.` |
|         - | 1175 | ` */` |
|         - | 1176 | `#define PH7_FATAL_TRACE_COMPILE 0 /* see ph7_gen_state::iFatalTrace */` |
|         - | 1177 | `#define PH7_FATAL_TRACE_RUNTIME 1` |
|         - | 1178 | `#define PH7_FATAL_TRACE_NONE    2` |
|         - | 1179 | `typedef struct VmIncFrame VmIncFrame;` |
|         - | 1180 | `struct VmIncFrame` |
|         - | 1181 | `{` |
|         - | 1182 | `	void *pFrame;      /* the VmFrame this activation was started from -- where it belongs in` |
|         - | 1183 | `	                    * the walk (php's trace is ordered by activation, and an include is` |
|         - | 1184 | `	                    * INNER to the function that wrote it) */` |
|         - | 1185 | `	SyString sFile;    /* the file the construct is written in ... */` |
|         - | 1186 | `	sxu32 nLine;       /* ...and the line */` |
|         - | 1187 | `	SyString sPath;    /* the unit being loaded: php's single argument for the frame, rendered` |
|         - | 1188 | ``	                    * as `'...'`. Empty for eval(), which php shows argument-less. */`` |
|         - | 1189 | `	const char *zName; /* "include" / "include_once" / "require" / "require_once" / "eval" */` |
|         - | 1190 | `};` |
|         - | 1191 | `typedef struct VmFrame VmFrame;` |
|         - | 1192 | `typedef struct VmInstr VmInstr;   /* defined below; a frame names the body it is numbered for */` |
|         - | 1193 | `/* How many of a body's variables get a NUMBER (see VmFrame's aLocalSlot and` |
|         - | 1194 | ` * VmNumberLocals). The frame's 512-byte pool bucket has 136 bytes spare once the` |
|         - | 1195 | ` * struct's named fields are laid out; a code pointer takes 8 of them and 28 slots` |
|         - | 1196 | ` * take the other 112, with room left over -- raising it past 32 reallocates every` |
|         - | 1197 | ` * frame out of the 512-byte bucket into the 1024-byte one, which is a doubling of` |
|         - | 1198 | ` * the engine's per-activation memory for the tail of the distribution. A static scan` |
|         - | 1199 | ` * of the ecosystem gate's phpcs sources puts 98% of function bodies at 28 distinct` |
|         - | 1200 | ` * variable names or fewer, and a body with more than that still numbers its 28` |
|         - | 1201 | ` * most-REFERENCED ones -- so the cap costs the tail its cold names, not its hot ones. */` |
|         - | 1202 | `#define PH7_VAR_SLOT_MAX 28` |
|         - | 1203 | `/* Chains in ph7_vm::apIdleOperandStack. A parked operand stack is reusable only by a` |
|         - | 1204 | `` * call of EXACTLY its slot count, so the count picks the chain: `nCap & (N-1)`, with`` |
|         - | 1205 | ` * the exact size still checked on each node (sizes sharing the low bits share a` |
|         - | 1206 | ` * chain). Sixty-four heads over a pool capped at 256 buffers is ~4 compares. */` |
|         - | 1207 | `#define PH7_STACK_POOL_BUCKETS 64` |
|         - | 1208 | `struct VmFrame` |
|         - | 1209 | `{` |
|         - | 1210 | `	VmFrame *pParent; /* Parent frame or NULL if global scope */` |
|         - | 1211 | `	void *pUserData;  /* Upper layer private data associated with this frame */` |
|         - | 1212 | `	ph7_class_instance *pThis; /* Current class instance [i.e: the '$this' variable].NULL otherwise */` |
|         - | 1213 | `	ph7_class *pBoundScope; /* Closure::bindTo/call scope override for private/protected access (Increment 2) */` |
|         - | 1214 | `	ph7_class *pSelfClass;  /* The class this activation was reached THROUGH (php's called-scope):` |
|         - | 1215 | `	                         * the receiver's class for an instance call, the named class for a` |
|         - | 1216 | `	                         * static one, 0 for a plain function. Only a trait method needs it --` |
|         - | 1217 | `	                         * its declaring class is the TRAIT, and the class php composed it` |
|         - | 1218 | `	                         * into is found by walking this one's ancestry (a STATIC trait method` |
|         - | 1219 | `	                         * has no $this to walk from). */` |
|         - | 1220 | `	SySet sLocal;     /* Local variables container (VmSlot instance) */` |
|         - | 1221 | `	ph7_vm *pVm;      /* VM that own this frame */` |
|         - | 1222 | `	SyHash hVar;      /* Variable hashtable for fast lookup */` |
|         - | 1223 | `	SySet sArg;       /* Function arguments container */` |
|         - | 1224 | `	SySet sRef;       /* Local reference table (VmSlot instance) */` |
|         - | 1225 | `	sxi32 iFlags;     /* Frame configuration flags (See below)*/` |
|         - | 1226 | `	sxu32 iExceptionJump; /* Exception jump destination */` |
|         - | 1227 | ``	ph7_value sRet;   /* Deferred catch/finally `return` value targeting THIS body frame */`` |
|         - | 1228 | `	int bHasRet;      /* TRUE when sRet holds a live pending return */` |
|         - | 1229 | `	sxu32 nRetGen;    /* Bumped on every sRet write (see VmThrowException finally path) */` |
|         - | 1230 | `	sxu32 nCatchJmpPc;/* Pending loop jump parked by a break/continue that left a DETACHED catch` |
|         - | 1231 | `	                   * mini-program (OP_CATCH_JMP): its target pc in this body frame's` |
|         - | 1232 | `	                   * bytecode, 0 when none is armed. The sibling of bHasRet/sRet — the same` |
|         - | 1233 | `	                   * park-here, act-at-the-landing-pad contract, cleared by the same` |
|         - | 1234 | `	                   * VmClearFramePending. Consumed by the owning try's OP_POP_EXCEPTION. */` |
|         - | 1235 | `	sxu16 nCatchJmpLevels;/* Detached-container boundaries still to leave before it is taken */` |
|         - | 1236 | `	sxu16 nCatchJmpCross; /* Enclosing try activations to drain (run their finally) before it */` |
|         - | 1237 | `	sxu32 nCallLine;  /* Line of the OP_CALL that pushed this frame (0 for the global frame).` |
|         - | 1238 | `	                   * debug_backtrace() reports a frame's line as the line of the call` |
|         - | 1239 | `	                   * SITE, not of the code running inside it. */` |
|         - | 1240 | `	SyString sCallFile;/* ...and the FILE that call site is in, captured when the frame is` |
|         - | 1241 | `	                   * pushed. It cannot be derived afterwards: the caller's own file is the` |
|         - | 1242 | `	                   * defining file of the CALLER's function, and for a call made by` |
|         - | 1243 | `	                   * top-level code it is whichever included unit was executing THEN --` |
|         - | 1244 | `	                   * the include stack has moved on by the time a trace is taken. Aliases` |
|         - | 1245 | `	                   * a VM-lifetime string (a function's sFile, or an aFiles entry). */` |
|         - | 1246 | `	ph7_foreach_step *pForeachSteps; /* Foreach steps this activation still owns, newest first.` |
|         - | 1247 | `	                   * Every step OP_FOREACH_INIT pushes is linked here and unlinked by the one` |
|         - | 1248 | `	                   * teardown door (VmForeachStepUnlink); whatever is left when the frame dies` |
|         - | 1249 | `	                   * is released with it. Without this a broken loop's step outlived its` |
|         - | 1250 | `	                   * activation for the life of the VM -- ~140 bytes plus a retain of the` |
|         - | 1251 | `	                   * subject each -- and INIT's reclaim scan walked every one of them. */` |
|         - | 1252 | `	int nActualArgs;  /* Actual call arity (band A #4): how many arguments the CALLER passed,` |
|         - | 1253 | `	                   * stamped by the OP_CALL / generator-fiber install sites; -1 when` |
|         - | 1254 | `	                   * unknown (non-call frames) - func_num_args()/func_get_args() then fall` |
|         - | 1255 | `	                   * back to the installed-formals count. Unlike sArg this excludes` |
|         - | 1256 | `	                   * defaulted params and counts variadic-packed args individually. */` |
|         - | 1257 | `	/* Where this activation's variables live, BY NUMBER: aLocalSlot[k] is the value` |
|         - | 1258 | `	 * slot the body's k-th variable name is bound to, plus one (0 = not resolved yet).` |
|         - | 1259 | `	 * The number comes from the bytecode, not from the name -- VmNumberLocals walks a` |
|         - | 1260 | `	 * body once and writes each variable instruction's number into its nSite -- so a` |
|         - | 1261 | `	 * read is an array index and a name is hashed at most ONCE per activation instead` |
|         - | 1262 | `	 * of once per access.` |
|         - | 1263 | `	 *` |
|         - | 1264 | `	 * It replaced an eight-entry memo keyed by the name's ADDRESS, which missed 28.5%` |
|         - | 1265 | `	 * of reads on the ecosystem gate's phpcs step and whose misses were LUCK: the` |
|         - | 1266 | `	 * entry a name landed in depended on where the compiler's pool happened to intern` |
|         - | 1267 | `	 * it, so the same commit measured 343.6M, 349.4M and 414.0M frame lookups in three` |
|         - | 1268 | `	 * builds (PERF.md §7). A number the bytecode carries has none of that in it.` |
|         - | 1269 | `	 *` |
|         - | 1270 | `	 * pCodeBase is what makes a number MEAN anything: it is the instruction array this` |
|         - | 1271 | `	 * frame's numbers were assigned against, so a body sharing the frame but not the` |
|         - | 1272 | `	 * numbering -- an include, an eval, a default-argument mini-program -- is told` |
|         - | 1273 | `	 * apart by one compare and falls back to the hash. Emptied by the three doors that` |
|         - | 1274 | `	 * can move a name to another slot -- PH7_VmBindVarSlot, PH7_VmRebindVarSlot and` |
|         - | 1275 | `	 * VmUnsetVarByNameEx -- see VmVarMemoFlush. */` |
|         - | 1276 | `	const VmInstr *pCodeBase;             /* the body aLocalSlot is numbered for, 0 = none */` |
|         - | 1277 | `	sxu32 aLocalSlot[PH7_VAR_SLOT_MAX];   /* slot index + 1, 0 = this name is unresolved here */` |
|         - | 1278 | `};` |
|         - | 1279 | `#define VM_FRAME_EXCEPTION  0x01 /* Special Exception frame */` |
|         - | 1280 | `#define VM_FRAME_THROW      0x02 /* An exception was thrown */` |
|         - | 1281 | `#define VM_FRAME_CATCH      0x04 /* Catch frame */` |
|         - | 1282 | `/*` |
|         - | 1283 | ` * One entry of a userland handler STACK (set_error_handler /` |
|         - | 1284 | ` * set_exception_handler). php's stack has no depth limit and every entry is a` |
|         - | 1285 | `` * real one -- the `null` a reset pushes included -- so each restore brings back`` |
|         - | 1286 | ` * exactly what the matching set replaced, and nothing under it is lost.` |
|         - | 1287 | ` */` |
|         - | 1288 | `typedef struct VmHandlerSlot VmHandlerSlot;` |
|         - | 1289 | `struct VmHandlerSlot {` |
|         - | 1290 | `	ph7_value sCb;   /* the saved handler, MEMOBJ_NULL for a reset entry */` |
|         - | 1291 | `	sxi64 iLevels;   /* its set_error_handler() $error_levels (E_ALL elsewhere) */` |
|         - | 1292 | `};` |
|         - | 1293 | `/*` |
|         - | 1294 | ` * php 8's E_ALL. The default of error_reporting() AND of set_error_handler()'s` |
|         - | 1295 | ` * $error_levels, so both read it from here (E_STRICT/2048 left the set in php 8).` |
|         - | 1296 | ` */` |
|         - | 1297 | `#define PH7_E_ALL_MASK 30719` |
|         - | 1298 | `/*` |
|         - | 1299 | ` * Suspendable execution context.` |
|         - | 1300 | ` * Used by Fiber and Generator to save/restore execution state.` |
|         - | 1301 | ` */` |
|         - | 1302 | `typedef struct ph7_exec_ctx ph7_exec_ctx;` |
|         - | 1303 | `/* Execution context states */` |
|         - | 1304 | `#define PH7_CTX_STATE_CREATED    0  /* Allocated but never started */` |
|         - | 1305 | `#define PH7_CTX_STATE_RUNNING    1  /* Currently executing */` |
|         - | 1306 | `#define PH7_CTX_STATE_SUSPENDED  2  /* Paused at suspend point */` |
|         - | 1307 | `#define PH7_CTX_STATE_COMPLETED  3  /* Returned normally */` |
|         - | 1308 | `#define PH7_CTX_STATE_CLOSED     4  /* Destroyed */` |
|         - | 1309 | `struct ph7_exec_ctx` |
|         - | 1310 | `{` |
|         - | 1311 | `	ph7_vm *pVm;              /* Owning VM */` |
|         - | 1312 | `	ph7_vm_func *pFunc;       /* The function being executed */` |
|         - | 1313 | `	VmFrame *pFrame;          /* Detached execution frame */` |
|         - | 1314 | `	ph7_value *pStack;        /* Private operand stack */` |
|         - | 1315 | `	sxu32 nStackCap;          /* Its allocated slot count (VmNewOperandStack size); grows` |
|         - | 1316 | `	                           * with pStack when an OP_SPREAD in this body reallocs it */` |
|         - | 1317 | `	sxu32 nStackOrig;         /* The ORIGINAL (ungrown) capacity — fixed at creation and used` |
|         - | 1318 | `	                           * to seed each resume's headroom reference, so a spread inside a` |
|         - | 1319 | `	                           * yield loop can't ratchet the stack up across resumes */` |
|         - | 1320 | `	sxi32 nTos;               /* Saved top-of-stack index */` |
|         - | 1321 | `	sxi32 pc;                 /* Saved program counter (resume point) */` |
|         - | 1322 | `	sxi32 iState;             /* One of PH7_CTX_STATE_* */` |
|         - | 1323 | `	sxu8 bThrew;              /* The body ENDED by letting an exception escape. php keeps the two` |
|         - | 1324 | `	                           * apart: such a fiber is terminated like any other, but getReturn()` |
|         - | 1325 | `	                           * says it threw rather than that it has not returned. */` |
|         - | 1326 | `	ph7_value sSuspendValue;  /* Value passed out via Fiber::suspend() / yield */` |
|         - | 1327 | `	ph7_value sRetValue;      /* Final return value */` |
|         - | 1328 | `	sxu32 nExceptionBase;     /* Exception-stack depth below this body's own handlers` |
|         - | 1329 | `	                           * (caller depth); refreshed at each resume */` |
|         - | 1330 | `	SySet aSavedException;    /* This body's own exception handlers (ph7_exception*),` |
|         - | 1331 | `	                           * parked here while suspended so a generator/fiber that` |
|         - | 1332 | `	                           * suspends inside a try does not corrupt the caller's` |
|         - | 1333 | `	                           * exception stack */` |
|         - | 1334 | `	SySet aSavedFinally;      /* ROOT C: this body's own pending finally actions` |
|         - | 1335 | `	                           * (VmFinallyAction), parked while suspended so a generator` |
|         - | 1336 | `	                           * that yields inside a finally reached by return/break/rethrow` |
|         - | 1337 | `	                           * does not leave its record on the shared VM stack (where an` |
|         - | 1338 | `	                           * out-of-order-resumed sibling generator would mis-pop it) */` |
|         - | 1339 | `	sxu32 nFinallyBase;       /* aFinallyAction depth below this body's own records */` |
|         - | 1340 | `	SySet aSavedSelf;         /* Stage 4: this coroutine's own aSelf (self::/static::)` |
|         - | 1341 | `	                           * entries, parked while suspended (ph7_class* pointers) */` |
|         - | 1342 | `	sxu32 nSelfBase;          /* aSelf depth below this coroutine's own pushes */` |
|         - | 1343 | `	ph7_class *pLsbClass;     /* The late-static-binding class the body runs under, captured` |
|         - | 1344 | `	                           * when the coroutine was CREATED. A generator body resumes long` |
|         - | 1345 | `	                           * after the call that made it returned, so pVm->aSelf no longer` |
|         - | 1346 | `` 	                           * carries the class the method was called through and `static::` `` |
|         - | 1347 | `	                           * inside the body answered "Class \"static\" not found" -- for` |
|         - | 1348 | ``	                           * `new static`, `static::method()` and `static::class` alike.`` |
|         - | 1349 | `	                           * php binds the called scope to the generator at creation and` |
|         - | 1350 | `	                           * restores it on every resume; this is that scope. Borrowed. */` |
|         - | 1351 | `	SySet aByRefArg;          /* Caller slots (sxu32) this body's by-REFERENCE parameters` |
|         - | 1352 | `	                           * alias. The body outlives its caller's frame, so whichever` |
|         - | 1353 | `	                           * of the two dies last releases the slot: the caller's` |
|         - | 1354 | `	                           * teardown counts this frame's name as a holder and skips it,` |
|         - | 1355 | `	                           * and this ctx's teardown asks PH7_VmReleaseUnheldSlot once` |
|         - | 1356 | `	                           * its own names are gone. */` |
|         - | 1357 | `	void *pPrivate;           /* Generator wrapper (ph7_generator*) or NULL for fibers */` |
|         - | 1358 | `	ph7_class_instance *pInjected; /* Generator::throw() inject-at-yield: exception to raise at` |
|         - | 1359 | `	                                * the suspended yield on the next resume, or NULL. One-shot:` |
|         - | 1360 | `	                                * consumed (cleared) by the loop-top inject check. Holds a` |
|         - | 1361 | `	                                * reference for the duration of the resume. */` |
|         - | 1362 | `	sxu8 bClosing;                 /* Set while VmCloseCtx force-drives this suspended generator's` |
|         - | 1363 | ``	                                * pending `finally` blocks at destruction (unset / out-of-scope`` |
|         - | 1364 | `	                                * / GC before completion). The body-resume entry redirects into` |
|         - | 1365 | `	                                * the innermost open try's finally chain instead of resuming at` |
|         - | 1366 | `	                                * the yield, and OP_YIELD raises PHP's "Cannot yield from finally` |
|         - | 1367 | `	                                * in a force-closed generator". Stays set for the whole close run. */` |
|         - | 1368 | ``	/* `yield from` delegation state — per generator instance, so independent`` |
|         - | 1369 | `	 * instances never clash (unlike the shared foreach aStep). */` |
|         - | 1370 | `	ph7_value sDelegate;             /* The iterable being delegated (kept alive) */` |
|         - | 1371 | `	ph7_hashmap_node *pDelegateNode; /* Array cursor: next node to read, else 0 */` |
|         - | 1372 | `	sxi32 iDelegateState;            /* 0=inactive, 1=array, 2=iterator, 3=generator */` |
|         - | 1373 | `	/* BYTECODE stage 4: deep Fiber::suspend() record-segment parking. */` |
|         - | 1374 | `	void *pParkedSegment;            /* VmParkedSegment* (opaque here): the trampoline` |
|         - | 1375 | `	                                  * record chain + innermost activation parked when a` |
|         - | 1376 | `	                                  * suspend fires inside a nested PHP call; NULL when` |
|         - | 1377 | `	                                  * suspended at the body level (pc/nTos above suffice) */` |
|         - | 1378 | `	int nBodyExecDepth;              /* pVm->nVmExecDepth of this ctx's body invocation. A` |
|         - | 1379 | `	                                  * suspend at a DEEPER native depth is inside a C->PHP` |
|         - | 1380 | `	                                  * callback (usort comparator, etc.) and cannot park` |
|         - | 1381 | `	                                  * across the native frame — it raises a catchable` |
|         - | 1382 | `	                                  * FiberError instead (the one scoped divergence). */` |
|         - | 1383 | `};` |
|         - | 1384 | `/* Special return code from VmByteCodeExec signaling fiber suspension */` |
|         - | 1385 | `#define PH7_SUSPEND  0x100` |
|         - | 1386 | `/*` |
|         - | 1387 | ` * Generator wrapper around ph7_exec_ctx.` |
|         - | 1388 | ` * Adds yield key tracking on top of the suspendable execution context.` |
|         - | 1389 | ` */` |
|         - | 1390 | `typedef struct ph7_generator ph7_generator;` |
|         - | 1391 | `struct ph7_generator` |
|         - | 1392 | `{` |
|         - | 1393 | `	ph7_exec_ctx *pCtx;       /* Execution context (allocated separately) */` |
|         - | 1394 | `	ph7_value sYieldValue;    /* Last yielded value (for current()) */` |
|         - | 1395 | `	ph7_value sYieldKey;      /* Last yielded key (for key()) */` |
|         - | 1396 | `	sxi64 iImplicitKey;       /* Auto-increment key counter */` |
|         - | 1397 | `	sxu8 bAtFirstYield;       /* php's ZEND_GENERATOR_AT_FIRST_YIELD: set when the` |
|         - | 1398 | `	                           * PRIMING run suspends, cleared by every resume after` |
|         - | 1399 | `	                           * it. It is the whole of php's rewind rule — a` |
|         - | 1400 | `	                           * generator that has moved past its first yield, or` |
|         - | 1401 | `	                           * finished, cannot be rewound. */` |
|         - | 1402 | `};` |
|         - | 1403 | `/*` |
|         - | 1404 | ` * Output control buffer entry.` |
|         - | 1405 | ` */` |
|         - | 1406 | `typedef struct VmObEntry VmObEntry;` |
|         - | 1407 | `struct VmObEntry` |
|         - | 1408 | `{` |
|         - | 1409 | `	ph7_value sCallback; /* User defined callback */` |
|         - | 1410 | `	SyBlob sOB;          /* Output buffer consumer (RAW bytes: php runs the` |
|         - | 1411 | `	                      * handler on the way OUT, not on the way in) */` |
|         - | 1412 | `	ph7_int64 iFlags;    /* PH7_OB_* below, php's own numeric values. 64 bits wide` |
|         - | 1413 | `	                      * because php stores whatever it was given (minus the two` |
|         - | 1414 | `	                      * nibbles it reserves) and reports it back verbatim. */` |
|         - | 1415 | `	ph7_int64 nChunk;    /* ob_start()'s $chunk_size (0 or negative: buffer` |
|         - | 1416 | `	                      * everything). 64 bits: php accepts a chunk larger than a` |
|         - | 1417 | `	                      * 32-bit count and reports it back. */` |
|         - | 1418 | `	ph7_int64 nSize;     /* php's ALLOCATION for this buffer, which ob_get_status()` |
|         - | 1419 | `	                      * reports: 16 KB, or the chunk size rounded up to 4 KB, and` |
|         - | 1420 | `	                      * grown by php's own rule on each write. Tracked rather than` |
|         - | 1421 | `	                      * derived because the answer depends on how the bytes` |
|         - | 1422 | `	                      * ARRIVED — 40 writes of 1000 give 49152 where one write of` |
|         - | 1423 | `	                      * 40000 gives 40960. */` |
|         - | 1424 | `};` |
|         - | 1425 | `/*` |
|         - | 1426 | ` * Output-handler flags and phases. These are php's own values: the first group is` |
|         - | 1427 | `` * what ob_get_status() reports in its `flags` entry, the second what the handler`` |
|         - | 1428 | `` * receives as its `$phase` argument.`` |
|         - | 1429 | ` */` |
|         - | 1430 | `#define PH7_OB_USER      0x0001 /* Handler is a userland callback */` |
|         - | 1431 | `#define PH7_OB_CLEANABLE 0x0010` |
|         - | 1432 | `#define PH7_OB_FLUSHABLE 0x0020` |
|         - | 1433 | `#define PH7_OB_REMOVABLE 0x0040` |
|         - | 1434 | `#define PH7_OB_STDFLAGS  0x0070` |
|         - | 1435 | `#define PH7_OB_STARTED   0x1000 /* Handler has been invoked at least once */` |
|         - | 1436 | `#define PH7_OB_DISABLED  0x2000 /* Handler answered FALSE: never called again */` |
|         - | 1437 | `#define PH7_OB_PROCESSED 0x4000 /* Handler has produced output */` |
|         - | 1438 | `/* What ob_start() keeps of the $flags it is given: everything except the phase` |
|         - | 1439 | ` * nibble and the state nibble, which are the engine's own to set. */` |
|         - | 1440 | `#define PH7_OB_FLAGMASK  (~(ph7_int64)0xF00F)` |
|         - | 1441 | `/* Phases (an op, plus PH7_OB_START until the handler has run once) */` |
|         - | 1442 | `#define PH7_OB_WRITE 0` |
|         - | 1443 | `#define PH7_OB_START 1` |
|         - | 1444 | `#define PH7_OB_CLEAN 2` |
|         - | 1445 | `#define PH7_OB_FLUSH 4` |
|         - | 1446 | `#define PH7_OB_FINAL 8` |
|         - | 1447 | `/*` |
|         - | 1448 | ` * mt_srand()/srand()'s $mode. php compares the argument against MT_RAND_PHP for` |
|         - | 1449 | ` * EQUALITY, so every other value — including an out-of-range one — selects the` |
|         - | 1450 | ` * standard generator.` |
|         - | 1451 | ` */` |
|         - | 1452 | `/* stream_wrapper_register()'s $flags: php defines this one bit. A wrapper that` |
|         - | 1453 | ` * declares itself a URL is the one allow_url_fopen and allow_url_include gate. */` |
|         - | 1454 | `#define PH7_STREAM_IS_URL 1` |
|         - | 1455 | `/*` |
|         - | 1456 | ` * The rest of the streamWrapper protocol's vocabulary, in php's own numbers.` |
|         - | 1457 | ` *` |
|         - | 1458 | `` * USE_PATH / IGNORE_URL / REPORT_ERRORS / MUST_SEEK are the `$options` bits`` |
|         - | 1459 | ` * stream_open() is handed; URL_STAT_LINK / URL_STAT_QUIET are url_stat()'s` |
|         - | 1460 | `` * `$flags` (LINK means lstat, QUIET means report a miss in silence), and NOCACHE`` |
|         - | 1461 | ` * rides beside them on every ask php's stat family makes because php's own` |
|         - | 1462 | ` * one-entry stat cache sits ABOVE that door; MKDIR_RECURSIVE is mkdir()'s;` |
|         - | 1463 | ` * META_* names the verb stream_metadata() is asked for; the OPTION_ and BUFFER_` |
|         - | 1464 | ` * pair belong to stream_set_option(), and CAST_ to stream_cast().` |
|         - | 1465 | ` */` |
|         - | 1466 | `#define PH7_STREAM_USE_PATH           1` |
|         - | 1467 | `#define PH7_STREAM_IGNORE_URL         2` |
|         - | 1468 | `#define PH7_STREAM_REPORT_ERRORS      8` |
|         - | 1469 | `#define PH7_STREAM_MUST_SEEK          16` |
|         - | 1470 | `#define PH7_URL_STAT_LINK             1` |
|         - | 1471 | `#define PH7_URL_STAT_QUIET            2` |
|         - | 1472 | `#define PH7_URL_STAT_NOCACHE          4` |
|         - | 1473 | `#define PH7_STREAM_MKDIR_RECURSIVE    1` |
|         - | 1474 | `#define PH7_STREAM_META_TOUCH         1` |
|         - | 1475 | `#define PH7_STREAM_META_OWNER_NAME    2` |
|         - | 1476 | `#define PH7_STREAM_META_OWNER         3` |
|         - | 1477 | `#define PH7_STREAM_META_GROUP_NAME    4` |
|         - | 1478 | `#define PH7_STREAM_META_GROUP         5` |
|         - | 1479 | `#define PH7_STREAM_META_ACCESS        6` |
|         - | 1480 | `#define PH7_STREAM_OPTION_BLOCKING    1` |
|         - | 1481 | `#define PH7_STREAM_OPTION_READ_BUFFER 2` |
|         - | 1482 | `#define PH7_STREAM_OPTION_WRITE_BUFFER 3` |
|         - | 1483 | `#define PH7_STREAM_OPTION_READ_TIMEOUT 4` |
|         - | 1484 | `#define PH7_STREAM_BUFFER_NONE        0` |
|         - | 1485 | `#define PH7_STREAM_BUFFER_LINE        1` |
|         - | 1486 | `#define PH7_STREAM_BUFFER_FULL        2` |
|         - | 1487 | `#define PH7_STREAM_CAST_AS_STREAM     0` |
|         - | 1488 | `#define PH7_STREAM_CAST_FOR_SELECT    3` |
|         - | 1489 | `/*` |
|         - | 1490 | ` * What PH7_StreamUserUrlStat() answered: the wrapper filled the record, the` |
|         - | 1491 | ` * wrapper declined, or no userland wrapper owns this path at all (the caller then` |
|         - | 1492 | ` * asks the VFS exactly as it always did).` |
|         - | 1493 | ` */` |
|         - | 1494 | `#define PHL_URLSTAT_OK      0` |
|         - | 1495 | `#define PHL_URLSTAT_FAIL    1` |
|         - | 1496 | `#define PHL_URLSTAT_NOWRAP (-1)` |
|         - | 1497 | `/*` |
|         - | 1498 | ` * Which member of the stat family is asking. php routes them all through one` |
|         - | 1499 | `` * `php_stat`, and the code decides three things: the flags the wrapper is handed,`` |
|         - | 1500 | ` * whether a miss is silent, and which of the thirteen fields answers.` |
|         - | 1501 | ` * The seven QUIET ones come first on purpose -- that ORDER is the test.` |
|         - | 1502 | ` */` |
|         - | 1503 | `#define PH7_STAT_ASK_EXISTS   0` |
|         - | 1504 | `#define PH7_STAT_ASK_IS_FILE  1` |
|         - | 1505 | `#define PH7_STAT_ASK_IS_DIR   2` |
|         - | 1506 | `#define PH7_STAT_ASK_IS_LINK  3` |
|         - | 1507 | `#define PH7_STAT_ASK_IS_R     4` |
|         - | 1508 | `#define PH7_STAT_ASK_IS_W     5` |
|         - | 1509 | `#define PH7_STAT_ASK_IS_X     6` |
|         - | 1510 | `#define PH7_STAT_ASK_SIZE     7` |
|         - | 1511 | `#define PH7_STAT_ASK_ATIME    8` |
|         - | 1512 | `#define PH7_STAT_ASK_MTIME    9` |
|         - | 1513 | `#define PH7_STAT_ASK_CTIME    10` |
|         - | 1514 | `#define PH7_STAT_ASK_OWNER    11` |
|         - | 1515 | `#define PH7_STAT_ASK_GROUP    12` |
|         - | 1516 | `#define PH7_STAT_ASK_INODE    13` |
|         - | 1517 | `#define PH7_STAT_ASK_PERMS    14` |
|         - | 1518 | `#define PH7_STAT_ASK_TYPE     15` |
|         - | 1519 | `#define PH7_STAT_ASK_STAT     16` |
|         - | 1520 | `#define PH7_STAT_ASK_LSTAT    17` |
|         - | 1521 | `/* php's S_IFMT decode, spelled in octal so it means the same on every port. */` |
|         - | 1522 | `#define PH7_S_IFMT   0170000` |
|         - | 1523 | `#define PH7_S_IFIFO  0010000` |
|         - | 1524 | `#define PH7_S_IFCHR  0020000` |
|         - | 1525 | `#define PH7_S_IFDIR  0040000` |
|         - | 1526 | `#define PH7_S_IFBLK  0060000` |
|         - | 1527 | `#define PH7_S_IFREG  0100000` |
|         - | 1528 | `#define PH7_S_IFLNK  0120000` |
|         - | 1529 | `#define PH7_S_IFSOCK 0140000` |
|         - | 1530 | `/* stream_socket_client()'s $flags. CONNECT is its default; without it php` |
|         - | 1531 | ` * creates no socket at all. PERSISTENT is what pfsockopen() means, and is the` |
|         - | 1532 | ` * only one that changes what a second call ANSWERS. */` |
|         - | 1533 | `#define PH7_STREAM_CLIENT_PERSISTENT    1` |
|         - | 1534 | `#define PH7_STREAM_CLIENT_ASYNC_CONNECT 2` |
|         - | 1535 | `#define PH7_STREAM_CLIENT_CONNECT       4` |
|         - | 1536 | `/* One live persistent socket: php's registry key is the address the opener was` |
|         - | 1537 | ` * given, spelling included ("localhost:80" and "127.0.0.1:80" are two). The` |
|         - | 1538 | ` * handle itself is the VFS's io_private, declared with the rest of that layer. */` |
|         - | 1539 | `typedef struct io_private io_private;` |
|         - | 1540 | `typedef struct VmPersistSock VmPersistSock;` |
|         - | 1541 | `struct VmPersistSock` |
|         - | 1542 | `{` |
|         - | 1543 | `	char zKey[320];` |
|         - | 1544 | `	io_private *pDev;` |
|         - | 1545 | `};` |
|         - | 1546 | `/* stream_socket_server()'s $flags. php keeps the two apart because a DATAGRAM` |
|         - | 1547 | ` * server is bound and never listens; LISTEN is what makes a bound socket a` |
|         - | 1548 | ` * stream server, and dropping it leaves a socket nothing can connect to. */` |
|         - | 1549 | `#define PH7_STREAM_SERVER_BIND   4` |
|         - | 1550 | `#define PH7_STREAM_SERVER_LISTEN 8` |
|         - | 1551 | `/* stream_socket_shutdown()'s $mode and the recvfrom/sendto flags: php's own` |
|         - | 1552 | ` * numbering, which is NOT the OS's (MSG_OOB and MSG_PEEK are mapped in net.c). */` |
|         - | 1553 | `#define PH7_STREAM_SHUT_RD   0` |
|         - | 1554 | `#define PH7_STREAM_SHUT_WR   1` |
|         - | 1555 | `#define PH7_STREAM_SHUT_RDWR 2` |
|         - | 1556 | `#define PH7_STREAM_OOB       1` |
|         - | 1557 | `#define PH7_STREAM_PEEK      2` |
|         - | 1558 | `#define PH7_MT_RAND_MT19937 0` |
|         - | 1559 | `#define PH7_MT_RAND_PHP     1` |
|         - | 1560 | `/*` |
|         - | 1561 | ` * HTTP response header entry.` |
|         - | 1562 | ` * Stored in ph7_vm.aResponseHeaders (a SySet of VmResponseHeader).` |
|         - | 1563 | ` */` |
|         - | 1564 | `typedef struct VmResponseHeader VmResponseHeader;` |
|         - | 1565 | `struct VmResponseHeader` |
|         - | 1566 | `{` |
|         - | 1567 | `	SyString sName;   /* Header name (e.g. "Content-Type"), case-preserving */` |
|         - | 1568 | `	SyString sValue;  /* Header value (e.g. "text/html") */` |
|         - | 1569 | `};` |
|         - | 1570 | `/*` |
|         - | 1571 | ` * Each collected function argument is recorded in an instance` |
|         - | 1572 | ` * of the following structure.` |
|         - | 1573 | ` * Note that as an extension, PH7 implements full type hinting` |
|         - | 1574 | ` * which mean that any function can have it's own signature.` |
|         - | 1575 | ` * Example:` |
|         - | 1576 | ` *      function foo(int $a,string $b,float $c,ClassInstance $d){}` |
|         - | 1577 | ` * This is how the powerful function overloading mechanism is` |
|         - | 1578 | ` * implemented.` |
|         - | 1579 | ` * Note that as an extension, PH7 allow function arguments to have` |
|         - | 1580 | ` * any complex default value associated with them unlike the standard` |
|         - | 1581 | ` * PHP engine.` |
|         - | 1582 | ` * Example:` |
|         - | 1583 | ` *    function foo(int $a = rand() & 1023){}` |
|         - | 1584 | ` *    now, when foo is called without arguments [i.e: foo()] the` |
|         - | 1585 | ` *    $a variable (first parameter) will be set to a random number` |
|         - | 1586 | ` *    between 0 and 1023 inclusive.` |
|         - | 1587 | ` * Refer to the official documentation for more information on this` |
|         - | 1588 | ` * mechanism and other extension introduced by the PH7 engine.` |
|         - | 1589 | ` */` |
|         - | 1590 | `struct ph7_vm_func_arg` |
|         - | 1591 | `{` |
|         - | 1592 | `	SyString sName;      /* Argument name */` |
|         - | 1593 | `	SySet aByteCode;     /* Compiled default value associated with this argument */` |
|         - | 1594 | `	sxu32 nType;         /* Type of this argument [i.e: array, int, string, float, object, etc.] */` |
|         - | 1595 | `	SyString sClass;     /* Class name if the argument expect a class instance [i.e: function foo(BaseClass $bar){} ] */` |
|         - | 1596 | `	sxi32 iFlags;        /* Configuration flags */` |
|         - | 1597 | `	SySet aUnionAlts;    /* Union type alternatives (ph7_type_alt). Empty unless VM_FUNC_ARG_UNION is set. */` |
|         - | 1598 | `	SyString sTypeName;  /* Original type text for error messages, normalized in canonical PHP order */` |
|         - | 1599 | `	sxi32 iPromoteVis;   /* PH7_CLASS_PROT_* when VM_FUNC_ARG_PROMOTED is set */` |
|         - | 1600 | `	SySet aAttrs;        /* Declared #[...] attributes (ph7_attribute records) */` |
|         - | 1601 | `};` |
|         - | 1602 | `/*` |
|         - | 1603 | ` * One alternative within a union type declaration. Used by parameters,` |
|         - | 1604 | `` * return types, and properties when the declaration is `T1\|T2\|...`,`` |
|         - | 1605 | `` * `A&B` (intersection), or `(A&B)\|C` (DNF).`` |
|         - | 1606 | ` */` |
|         - | 1607 | `typedef struct ph7_type_alt ph7_type_alt;` |
|         - | 1608 | `struct ph7_type_alt` |
|         - | 1609 | `{` |
|         - | 1610 | `	sxu32 nType;     /* MEMOBJ_* bitmask, or SXU32_HIGH for a class/interface alternative */` |
|         - | 1611 | `	SyString sClass; /* Class/interface name when nType == SXU32_HIGH */` |
|         - | 1612 | `	sxu32 nGroup;    /* Intersection-group id: atoms sharing a group are ANDed (A&B),` |
|         - | 1613 | `	                  * distinct groups are ORed. A pure union is one atom per group. */` |
|         - | 1614 | `};` |
|         - | 1615 | `/* Maximum alternatives in one type declaration; bounds the on-stack atom array` |
|         - | 1616 | ` * in the parser and the per-group tally in the enforcer. Larger than any real` |
|         - | 1617 | ` * union/DNF type. */` |
|         - | 1618 | `#define PHL_UNION_MAX_ALTS 32` |
|         - | 1619 | `/*` |
|         - | 1620 | ` * Each static variable is parsed out and remembered in an instance` |
|         - | 1621 | ` * of the following structure.` |
|         - | 1622 | ` * Note that as an extension, PH7 allow static variable have` |
|         - | 1623 | ` * any complex default value associated with them unlike the standard` |
|         - | 1624 | ` * PHP engine.` |
|         - | 1625 | ` * Example:` |
|         - | 1626 | ` *   static $rand_str = 'PH7'.rand_str(3); // Concatenate 'PH7' with` |
|         - | 1627 | ` *                                         // a random three characters(English alphabet)` |
|         - | 1628 | ` *   var_dump($rand_str);` |
|         - | 1629 | ` *   //You should see something like this` |
|         - | 1630 | ` *   string(6 'PH7awt');` |
|         - | 1631 | ` */` |
|         - | 1632 | `struct ph7_vm_func_static_var` |
|         - | 1633 | `{` |
|         - | 1634 | `	SyString sName;   /* Static variable name */` |
|         - | 1635 | `	SySet aByteCode;  /* Compiled initialization expression  */` |
|         - | 1636 | `	sxu32 nIdx;       /* Object index in the global memory object container */` |
|         - | 1637 | `};` |
|         - | 1638 | `/*` |
|         - | 1639 | ` * Each imported variable from the outside closure environnment is recoded` |
|         - | 1640 | ` * in an instance of the following structure.` |
|         - | 1641 | ` */` |
|         - | 1642 | `struct ph7_vm_func_closure_env` |
|         - | 1643 | `{` |
|         - | 1644 | `	SyString sName;   /* Imported variable name */` |
|         - | 1645 | `	int iFlags;       /* Control flags */` |
|         - | 1646 | ``	sxu32 nLine;      /* Source line of this `use ($x)` capture (0 = unknown/implicit):`` |
|         - | 1647 | `					   * php reports an undefined by-value capture's E_WARNING at the` |
|         - | 1648 | `					   * variable's own line, which can differ from the closure keyword's` |
|         - | 1649 | ``					   * line when the `use` clause wraps. Set only for explicit captures. */`` |
|         - | 1650 | `	ph7_value sValue; /* Imported variable value */` |
|         - | 1651 | `	sxu32 nIdx;       /* Reference to the bounded variable if passed by reference` |
|         - | 1652 | `					   *[Example:` |
|         - | 1653 | `					   *  $x = 1;` |
|         - | 1654 | `					   *  $closure = function() use (&$x) { ++$x; }` |
|         - | 1655 | `					   *  $closure();` |
|         - | 1656 | `					   *]` |
|         - | 1657 | `					   */` |
|         - | 1658 | `};` |
|         - | 1659 | `/* Function configuration flags */` |
|         - | 1660 | `#define VM_FUNC_ARG_BY_REF   0x001 /* Argument passed by reference */` |
|         - | 1661 | `#define VM_FUNC_ARG_HAS_DEF  0x002 /* Argument has default value associated with it */` |
|         - | 1662 | `#define VM_FUNC_REF_RETURN   0x004 /* Return by reference */` |
|         - | 1663 | `#define VM_FUNC_CLASS_METHOD 0x008 /* VM function is in fact a class method */` |
|         - | 1664 | `#define VM_FUNC_CLOSURE      0x010 /* VM function is a closure */` |
|         - | 1665 | `#define VM_FUNC_ARG_IGNORE   0x020 /* Do not install argument in the current frame */` |
|         - | 1666 | `#define VM_FUNC_GENERATOR    0x040 /* VM function is a generator (contains yield) */` |
|         - | 1667 | `#define VM_FUNC_ARG_VARIADIC 0x080 /* Argument is variadic (...$args) */` |
|         - | 1668 | `#define VM_FUNC_ARG_NULLABLE 0x100 /* Argument type is nullable (?type or T\|null) */` |
|         - | 1669 | `#define VM_FUNC_ARG_UNION    0x200 /* Argument has a union type (use aUnionAlts) */` |
|         - | 1670 | `#define VM_FUNC_ARG_PROMOTED 0x400 /* Constructor promoted property (iPromoteVis holds visibility) */` |
|         - | 1671 | `#define VM_FUNC_ARG_READONLY 0x800 /* Promoted property is readonly (PHP 8.1) */` |
|         - | 1672 | `#define VM_FUNC_RETURN_NULLABLE 0x1000 /* Return type is nullable (?T, T\|null, A\|B\|null) — func-level */` |
|         - | 1673 | `#define VM_FUNC_INTERNAL     0x2000 /* Function was defined while compiling a builtin chunk` |
|         - | 1674 | `                                     * (embedded PHP library). Reflection reports it as internal:` |
|         - | 1675 | `                                     * isInternal() true, getFileName() false. */` |
|         - | 1676 | ``#define VM_FUNC_STATIC_CL    0x4000 /* Static closure/arrow fn (`static function () {}` /`` |
|         - | 1677 | ``                                     * `static fn () =>`): no $this auto-capture, bind refused. */`` |
|         - | 1678 | `#define VM_FUNC_ARG_PRIV_SET 0x8000  /* Promoted property is private(set) (PHP 8.4) */` |
|         - | 1679 | `#define VM_FUNC_ARG_PROT_SET 0x10000 /* Promoted property is protected(set) (PHP 8.4) */` |
|         - | 1680 | ``#define VM_FUNC_HOOK_SET_EXPR 0x20000 /* `set => expr` property hook (PHP 8.4): the dispatcher`` |
|         - | 1681 | `                                       * stores the implicit return value into the backing slot */` |
|         - | 1682 | `#define VM_FUNC_BOUND        0x40000 /* Bound by an UNCONDITIONAL top-level declaration; a second such` |
|         - | 1683 | `                                      * binding of the same name fatals ("Cannot redeclare function ..."),` |
|         - | 1684 | `                                      * matching PHP. Conditional declarations are not marked. */` |
|         - | 1685 | ``#define VM_FUNC_ARROW        0x80000 /* Arrow function (`fn()=>expr`): its aClosureEnv captures are ALL`` |
|         - | 1686 | ``                                       * implicit (auto-scanned from the body), never an explicit `use` `` |
|         - | 1687 | `                                      * clause. php does not warn about an undefined auto-capture at` |
|         - | 1688 | `                                      * closure creation — the read fires the warning inside the body —` |
|         - | 1689 | `                                      * so the OP_LOAD_CLOSURE undefined-capture warning is suppressed. */` |
|         - | 1690 | `#define VM_FUNC_NATIVE       0x100000 /* The body is a C routine, not bytecode: ph7_vm_func::pNative` |
|         - | 1691 | `                                       * holds it and aByteCode stays EMPTY. OP_CALL branches to the` |
|         - | 1692 | `                                       * host-function path (no frame, no call record, no operand` |
|         - | 1693 | `                                       * stack) while every step BEFORE the branch — the sVmName` |
|         - | 1694 | `                                       * lookup, $this/self resolution, visibility — runs unchanged,` |
|         - | 1695 | `                                       * so a native method inherits, overrides and dispatches like` |
|         - | 1696 | `                                       * any other. This is what lets a builtin class own its C code` |
|         - | 1697 | ``                                       * as a METHOD instead of a global `__prefix_verb` thunk. */`` |
|         - | 1698 | `#define VM_FUNC_NATIVE_STATIC 0x200000 /* A VM_FUNC_NATIVE method declared static. Staticness is` |
|         - | 1699 | `                                       * otherwise recorded only on ph7_class_method::iFlags, which` |
|         - | 1700 | `                                       * the OP_CALL dispatcher does not hold — and it must know,` |
|         - | 1701 | `                                       * because the method path falls back to the CALLER's $this` |
|         - | 1702 | `                                       * when the target slot carries a class name rather than an` |
|         - | 1703 | `                                       * object. For a bytecode method that fallback is harmless;` |
|         - | 1704 | `                                       * for a native one it would hand the body a receiver on a` |
|         - | 1705 | ``                                       * `C::m()` call and shift how it reads its arguments. Set by`` |
|         - | 1706 | `                                       * the native builder only: the compiler's behaviour for` |
|         - | 1707 | `                                       * bytecode methods is deliberately left untouched. */` |
|         - | 1708 | `#define VM_FUNC_NODISCARD 0x400000 /* php 8.5's #[\NoDiscard]: a caller that DROPS this` |
|         - | 1709 | `                                       * function's answer is warned at the call site. Set by` |
|         - | 1710 | `                                       * the compiler from the declared attribute, and by the` |
|         - | 1711 | `                                       * native builder for the internal members php marks` |
|         - | 1712 | `                                       * (PH7_VmFuncSetNoDiscard). The message, when there is` |
|         - | 1713 | `                                       * one, comes from zNoDiscard for a native member and` |
|         - | 1714 | `                                       * from the attribute's own argument for a compiled one. */` |
|         - | 1715 | ``#define VM_FUNC_ARG_FINAL 0x800000 /* PHP 8.4's `final` on a PROMOTED property. Kept apart from`` |
|         - | 1716 | `                                    * the class-body rule it mirrors: php refuses` |
|         - | 1717 | ``                                    * `final private $p` in a class body and ACCEPTS the same`` |
|         - | 1718 | `                                    * pair here (modifiers 36), so the screen cannot be shared. */` |
|         - | 1719 | `/* next free bit: 0x1000000 */` |
|         - | 1720 | `/*` |
|         - | 1721 | ` * Each user defined function is parsed out and stored in an instance` |
|         - | 1722 | ` * of the following structure.` |
|         - | 1723 | ` * PH7 introduced some powerfull extensions to the PHP 5 programming` |
|         - | 1724 | ` * language like function overloading, type hinting, complex default` |
|         - | 1725 | ` * arguments values and many more.` |
|         - | 1726 | ` * Please refer to the official documentation for more information.` |
|         - | 1727 | ` */` |
|         - | 1728 | `struct ph7_vm_func` |
|         - | 1729 | `{` |
|         - | 1730 | `	SySet aArgs;         /* Expected arguments (ph7_vm_func_arg instance) */` |
|         - | 1731 | `	SySet aStatic;       /* Static variable (ph7_vm_func_static_var instance) */` |
|         - | 1732 | `	SyString sName;      /* Function name */` |
|         - | 1733 | `	SySet aByteCode;     /* Compiled function body */` |
|         - | 1734 | `	SySet aClosureEnv;   /* Closure environment (ph7_vm_func_closure_env instace) */` |
|         - | 1735 | `	sxi32 iFlags;        /* VM function configuration */` |
|         - | 1736 | `	SyString sSignature; /* Function signature used to implement function overloading` |
|         - | 1737 | `						  * (Refer to the official docuemntation for more information` |
|         - | 1738 | `						  *  on this powerfull feature)` |
|         - | 1739 | `						  */` |
|         - | 1740 | `	sxu32 nReturnType;   /* Return type hint (MEMOBJ_* constant, MEMOBJ_VOID, or SXU32_HIGH for class) */` |
|         - | 1741 | `	SyString sReturnClass; /* Class name when nReturnType == SXU32_HIGH */` |
|         - | 1742 | `	SySet aReturnUnion;  /* Return-type union alternatives (ph7_type_alt). Empty unless union return. */` |
|         - | 1743 | `	SyString sReturnTypeName; /* Original return-type text for error messages, in canonical PHP order */` |
|         - | 1744 | `	sxu8 bStrictTypes;   /* 1 if defining file declared strict_types=1 (governs return-value coercion) */` |
|         - | 1745 | `	sxu16 nLocalName;    /* How many of this body's variable names VmNumberLocals gave a` |
|         - | 1746 | `	                      * number to, capped at PH7_VAR_SLOT_MAX. Meaningful only once` |
|         - | 1747 | `	                      * bNumbered is set; 0 with bNumbered set means the body names no` |
|         - | 1748 | `	                      * variable the compiler wrote down. */` |
|         - | 1749 | `	sxu8 bNumbered;      /* 1 = VmNumberLocals has walked this body. Lazily, on the first` |
|         - | 1750 | `	                      * activation, exactly like nMaxStack below and for the same` |
|         - | 1751 | `	                      * reason: a body that has not been walked yet just walks, where a` |
|         - | 1752 | `	                      * compile-time pass would have to answer for every path that can` |
|         - | 1753 | `	                      * build one. */` |
|         - | 1754 | `	sxu32 nMaxStack;     /* Cached operand-stack depth for this body (BYTECODE stage 7):` |
|         - | 1755 | `						  * 0 = not yet computed; otherwise the number of slots to allocate` |
|         - | 1756 | `						  * per call (a tight bound from VmComputeMaxStack, or the whole` |
|         - | 1757 | `						  * instruction count when the body is not statically modelable). */` |
|         - | 1758 | `	SySet aAttrs;        /* Declared #[...] attributes (ph7_attribute records) */` |
|         - | 1759 | `	SyString sDoc;       /* Doc-comment immediately preceding the declaration, delimiters` |
|         - | 1760 | `						  * included (duplicated into the VM allocator); nByte == 0 = none,` |
|         - | 1761 | `						  * Reflection getDocComment() then reports false. */` |
|         - | 1762 | `	SyString sFile;      /* Path of the defining file (aliases the VM-lifetime dup in pVm->aFiles).` |
|         - | 1763 | `						  * nByte == 0 when unknown (builtin chunk, eval, direct API compile):` |
|         - | 1764 | `						  * Reflection getFileName() then reports false. */` |
|         - | 1765 | `	sxu32 nLine;         /* Line of the 'function'/'fn' keyword (Reflection getStartLine) */` |
|         - | 1766 | `	sxu32 nEndLine;      /* Line of the closing brace of the body (Reflection getEndLine) */` |
|         - | 1767 | `	SyString sClosureName; /* A closure/arrow function's php-VISIBLE name, php 8.4's` |
|         - | 1768 | ``	                      * `{closure:SCOPE:LINE}` (Zend/zend_compile.c, zend_begin_func_decl).`` |
|         - | 1769 | `	                      * Built at COMPILE time — the scope part names the ENCLOSING` |
|         - | 1770 | `	                      * function, which only the compiler knows — and duplicated into the` |
|         - | 1771 | `	                      * VM allocator. sName stays the synthesized unique lookup key` |
|         - | 1772 | `	                      * ("[lambda_3]"); this is what __FUNCTION__, a backtrace, Reflection` |
|         - | 1773 | `	                      * and is_callable() report. nByte == 0 for anything but a closure. */` |
|         - | 1774 | ``	SyString sClosureScope;/* The class the closure was written inside, for the `C::` php prefixes`` |
|         - | 1775 | `	                        * its argument diagnostics with. Empty for a top-level one. */` |
|         - | 1776 | `	void *pUserData;     /* Upper layer private data associated with this instance */` |
|         - | 1777 | `	sxu8 bQueued;        /* VM_FUNC_CLOSURE only: already on the VM's pending-free list */` |
|         - | 1778 | `	sxi32 nRef;          /* VM_FUNC_CLOSURE only: how many things still need this` |
|         - | 1779 | `` 	                      * per-instantiation copy -- the Closure OBJECTS whose `$__fn` `` |
|         - | 1780 | `	                      * names it, plus every activation currently running it. At zero` |
|         - | 1781 | `	                      * it is unregistered from hFunction and freed. Every closure` |
|         - | 1782 | `	                      * expression evaluated used to mint one of these and leave it in` |
|         - | 1783 | `	                      * the function table for the life of the VM: ~3 KB per closure,` |
|         - | 1784 | `	                      * which on a real workload (phpcs) was over half the engine's` |
|         - | 1785 | `	                      * whole memory footprint. */` |
|         - | 1786 | `	void *pLsbClass;     /* For a closure: the late-static-binding class captured at its` |
|         - | 1787 | ``	                      * creation site (ph7_class*), so `static::` inside the body`` |
|         - | 1788 | `	                      * resolves like php. NULL for a plain function/method. */` |
|         - | 1789 | `	ph7_user_func *pNative; /* VM_FUNC_NATIVE only: the C body. A ph7_user_func rather than a` |
|         - | 1790 | `	                      * bespoke record because that struct ALREADY carries everything the` |
|         - | 1791 | `	                      * host-call path reads — xFunc, pUserData, sName, the min/max arity` |
|         - | 1792 | `	                      * bounds, zSig/zRet and nByRefMask — so the existing OP_CALL foreign` |
|         - | 1793 | `	                      * block, VmInitCallContext, ph7_context_user_data(), ph7_function_name()` |
|         - | 1794 | `	                      * and VmEnforceBuiltinArgTypes all work on it verbatim. It is NOT` |
|         - | 1795 | `	                      * registered in pVm->hHostFunction: it hangs off this method alone and` |
|         - | 1796 | `	                      * is reachable only through the method, never as a global name. */` |
|         - | 1797 | `	ph7_vm_func *pNextName; /* Next VM function with the same name as this one */` |
|         - | 1798 | `};` |
|         - | 1799 | `/* Forward reference */` |
|         - | 1800 | `typedef struct ph7_builtin_constant ph7_builtin_constant;` |
|         - | 1801 | `typedef struct ph7_builtin_func ph7_builtin_func;` |
|         - | 1802 | `/*` |
|         - | 1803 | ` * Each built-in foreign function (C function) is stored in an` |
|         - | 1804 | ` * instance of the following structure.` |
|         - | 1805 | ` * Please refer to the official documentation for more information` |
|         - | 1806 | ` * on how to create/install foreign functions.` |
|         - | 1807 | ` */` |
|         - | 1808 | `struct ph7_builtin_func` |
|         - | 1809 | `{` |
|         - | 1810 | `	const char *zName;        /* Function name [i.e: strlen(), rand(), array_merge(), etc.]*/` |
|         - | 1811 | `	ProchHostFunction xFunc;  /* C routine performing the computation */` |
|         - | 1812 | `};` |
|         - | 1813 | `/*` |
|         - | 1814 | ` * Each built-in foreign constant is stored in an instance` |
|         - | 1815 | ` * of the following structure.` |
|         - | 1816 | ` * Please refer to the official documentation for more information` |
|         - | 1817 | ` * on how to create/install foreign constants.` |
|         - | 1818 | ` */` |
|         - | 1819 | `struct ph7_builtin_constant` |
|         - | 1820 | `{` |
|         - | 1821 | `	const char *zName;     /* Constant name */` |
|         - | 1822 | `	ProcConstant xExpand;  /* C routine responsible of expanding constant value*/` |
|         - | 1823 | `};` |
|         - | 1824 | `/* Forward reference */` |
|         - | 1825 | `typedef struct ph7_class_method ph7_class_method;` |
|         - | 1826 | `typedef struct ph7_class_attr   ph7_class_attr;` |
|         - | 1827 | `/*` |
|         - | 1828 | ` * One subscript asked of a native class through ph7_class::xDim -- php's` |
|         - | 1829 | ` * read_dimension / has_dimension handlers, as one call.` |
|         - | 1830 | ` *` |
|         - | 1831 | ` * The hook answers by writing pResult (left NULL for a miss, which the ISSET` |
|         - | 1832 | ` * mode reads as "not set"), or REFUSES by naming an exception class in` |
|         - | 1833 | ` * zThrowClass and wording it in zThrowMsg. The refusal is carried back rather` |
|         - | 1834 | ` * than raised here because only the opcode knows how to route a throw out of a` |
|         - | 1835 | ` * mid-expression read, and because php's own two modes disagree about it: an` |
|         - | 1836 | `` * out-of-range `$map[-1]` is a ValueError to a READ (and to `??`, which reads)`` |
|         - | 1837 | `` * and a plain FALSE to `isset()`.`` |
|         - | 1838 | ` */` |
|         - | 1839 | `typedef struct PH7_NativeDimCtx PH7_NativeDimCtx;` |
|         - | 1840 | `#define PH7_NATIVE_DIM_READ  0 /* php's read_dimension: the value, or NULL for a miss */` |
|         - | 1841 | `#define PH7_NATIVE_DIM_ISSET 1 /* php's has_dimension: presence only, and never a refusal */` |
|         - | 1842 | `/*` |
|         - | 1843 | ` * The WRITE side. Two kinds of class arrive here.` |
|         - | 1844 | ` *` |
|         - | 1845 | ` * One answers reads and stores NOTHING, and may only REFUSE: php's` |
|         - | 1846 | ` * write_dimension and unset_dimension for such a container. The engine's own` |
|         - | 1847 | `` * sentence is `Cannot use object of type C as array`, and a class states its`` |
|         - | 1848 | `` * own here -- PDORow's three are `Cannot write to PDORow offset`, `Cannot`` |
|         - | 1849 | `` * append to PDORow offset` and `Cannot unset PDORow offset`. A refusal is`` |
|         - | 1850 | ` * asked with neither pOffset nor pResult (none of php's wordings names the` |
|         - | 1851 | ` * offset, and there is no answer to write): a hook that does not word one of` |
|         - | 1852 | ` * these must return without touching either.` |
|         - | 1853 | ` *` |
|         - | 1854 | `` * The other really STORES: `$x['a'] = '1'` on a SimpleXMLElement writes an`` |
|         - | 1855 | ` * attribute, and php's handler is a write_dimension like any other. Those` |
|         - | 1856 | ``  * three modes are asked a second way -- with pOffset (0 for the keyless `$o[]` `` |
|         - | 1857 | ` * spelling) and with pResult carrying the INCOMING VALUE -- and the hook says` |
|         - | 1858 | ` * it took the write by setting bStored. A hook that leaves bStored at 0 is the` |
|         - | 1859 | ` * first kind and the caller falls back to the refusal above, which is what` |
|         - | 1860 | ` * keeps DOMNodeList and PDORow answering exactly as they did.` |
|         - | 1861 | ` */` |
|         - | 1862 | `#define PH7_NATIVE_DIM_WRITE  2 /* php's write_dimension with a key */` |
|         - | 1863 | ``#define PH7_NATIVE_DIM_APPEND 3 /* ...and its keyless `$o[] = v` spelling */`` |
|         - | 1864 | `#define PH7_NATIVE_DIM_UNSET  4 /* php's unset_dimension */` |
|         - | 1865 | `/*` |
|         - | 1866 | `` * php's has_dimension asked the way `empty()` asks it -- a non-zero`` |
|         - | 1867 | `` * `check_empty`, which its handlers read as the EMPTINESS question rather than`` |
|         - | 1868 | ` * the null one. SimpleXMLElement is the one that answers it differently:` |
|         - | 1869 | `` * `empty($x['a'])` on `a="0"` is TRUE, judged on the ATTRIBUTE'S TEXT, where`` |
|         - | 1870 | ` * reading the same offset hands back a truthy object. A hook that has no such` |
|         - | 1871 | ` * distinction leaves pResult alone and the caller falls back to reading the` |
|         - | 1872 | ` * value and judging that, which is every other class's answer.` |
|         - | 1873 | ` */` |
|         - | 1874 | `#define PH7_NATIVE_DIM_NOTEMPTY 5` |
|         - | 1875 | `struct PH7_NativeDimCtx` |
|         - | 1876 | `{` |
|         - | 1877 | `	int iMode;               /* PH7_NATIVE_DIM_* */` |
|         - | 1878 | ``	ph7_value *pOffset;      /* The subscript. 0 for the keyless `$o[]` spelling. */`` |
|         - | 1879 | `	ph7_value *pResult;      /* READ: where the answer goes (the caller inits it NULL).` |
|         - | 1880 | `	                          * ISSET: set to a bool by the hook. */` |
|         - | 1881 | `	const char *zThrowClass; /* Set by the hook to refuse; 0 (the caller's init) means answered */` |
|         - | 1882 | `	char zThrowMsg[160];     /* ...and its message, formatted by the hook */` |
|         - | 1883 | `	int bStored;             /* WRITE/APPEND/UNSET only: the hook TOOK the write. 0 -- the` |
|         - | 1884 | `	                          * caller's init -- means it did not, and the access takes the` |
|         - | 1885 | ``	                          * `Cannot use object of type C as array` refusal (or the hook's`` |
|         - | 1886 | `	                          * own wording of it) instead. */` |
|         - | 1887 | `};` |
|         - | 1888 | `/*` |
|         - | 1889 | ` * One property WRITE asked of a native class through ph7_class::xSet -- php's` |
|         - | 1890 | ` * write_property handler.` |
|         - | 1891 | ` *` |
|         - | 1892 | ` * A native class whose properties are php's OWN C struct rather than real slots` |
|         - | 1893 | ` * states this: php converts the incoming value the way its struct field demands` |
|         - | 1894 | ``  * and stores THAT, so `$i->y = 1.5` reads back int(1) and `$i->f = 0.1234567` `` |
|         - | 1895 | ` * reads back 0.123456 (an int64 count of microseconds, shown divided). The hook` |
|         - | 1896 | ` * rewrites pValue IN PLACE to whatever must land in the slot -- it runs on every` |
|         - | 1897 | ` * write shape (a plain store, a compound assign, ++/--, a list() target, a` |
|         - | 1898 | ` * foreach target), because it hangs off the same store filter the typed-property` |
|         - | 1899 | ` * enforcement does.` |
|         - | 1900 | ` *` |
|         - | 1901 | ` * Refusing works the way the dimension hook's does: name an exception class in` |
|         - | 1902 | ` * zThrowClass and word it in zThrowMsg, and the filter raises it where the store` |
|         - | 1903 | ` * would have landed. A property php only lets a script write by CREATING a` |
|         - | 1904 | `` * deprecated dynamic one (DateInterval's `days`) is refused here, §10.`` |
|         - | 1905 | ` */` |
|         - | 1906 | `typedef struct PH7_NativeSetCtx PH7_NativeSetCtx;` |
|         - | 1907 | `struct PH7_NativeSetCtx` |
|         - | 1908 | `{` |
|         - | 1909 | `	const SyString *pName;   /* The property being written */` |
|         - | 1910 | `	ph7_value *pValue;       /* The incoming value; the hook rewrites it in place */` |
|         - | 1911 | `	const char *zThrowClass; /* Set by the hook to refuse; 0 (the caller's init) means stored */` |
|         - | 1912 | `	char zThrowMsg[160];     /* ...and its message, formatted by the hook */` |
|         - | 1913 | `};` |
|         - | 1914 | `/*` |
|         - | 1915 | ` * One PROPERTY access asked of a native class through ph7_class::xProp --` |
|         - | 1916 | ` * php's read_property / has_property / write_property / unset_property` |
|         - | 1917 | ` * handlers, as one call told which is asking.` |
|         - | 1918 | ` *` |
|         - | 1919 | ` * This is the hook for a class whose properties are not storage at all: php's` |
|         - | 1920 | ` * PDORow answers every read from the statement's CURRENT row, so the object` |
|         - | 1921 | `` * holds no slot for any of them, `get_object_vars()` is EMPTY beside a read`` |
|         - | 1922 | ` * that works, and a write is a refusal rather than a store. It is asked only` |
|         - | 1923 | ` * where the instance has NO slot of that name, which for such a class is` |
|         - | 1924 | ` * everywhere -- a native class that keeps real slots and only CONVERTS what` |
|         - | 1925 | ` * lands in them wants ph7_class::xSet instead.` |
|         - | 1926 | ` *` |
|         - | 1927 | ` * READ answers by writing pResult (left NULL for a name the class does not` |
|         - | 1928 | `` * know, which is php's own answer -- not an `Undefined property` warning);`` |
|         - | 1929 | ` * ISSET and EXISTS answer by setting pResult to a bool. Either may DECLINE by leaving` |
|         - | 1930 | ` * bAnswered at 0, which puts the name back on the ordinary path. WRITE and` |
|         - | 1931 | ` * UNSET exist only to refuse, the way the dimension hook's write modes do.` |
|         - | 1932 | ` *` |
|         - | 1933 | ` * A refusal is carried back in zThrowClass/zThrowMsg rather than raised here,` |
|         - | 1934 | ` * exactly as PH7_NativeDimCtx's is: only the opcode knows how to route a throw` |
|         - | 1935 | ` * out of a mid-expression access.` |
|         - | 1936 | ` */` |
|         - | 1937 | `#define PH7_NATIVE_PROP_READ  0` |
|         - | 1938 | `#define PH7_NATIVE_PROP_ISSET 1` |
|         - | 1939 | `#define PH7_NATIVE_PROP_WRITE 2` |
|         - | 1940 | `#define PH7_NATIVE_PROP_UNSET 3` |
|         - | 1941 | `/*` |
|         - | 1942 | `` * php's has_property asked the way `empty()` asks it -- ZEND_PROPERTY_NOT_EMPTY,`` |
|         - | 1943 | `` * a non-zero `check_empty`, which its handlers read as the EMPTINESS question`` |
|         - | 1944 | ` * rather than the null one. It is the same handler and a different answer: a` |
|         - | 1945 | `` * PDORow column holding 0 or "" is `isset()` and is not this.`` |
|         - | 1946 | ` */` |
|         - | 1947 | `#define PH7_NATIVE_PROP_NOTEMPTY 4` |
|         - | 1948 | `/*` |
|         - | 1949 | ` * php's write_property, asked at the point the VALUE exists.` |
|         - | 1950 | ` *` |
|         - | 1951 | ` * The modes above are asked by the member opcode, which runs BEFORE the store` |
|         - | 1952 | ` * that carries the value -- enough for a class that only ever refuses a write` |
|         - | 1953 | ` * (PDORow), and not enough for one whose handler really stores (ext/dom's` |
|         - | 1954 | `` * `$el->nodeValue = 'x'`). STORE is the second half: pResult carries the`` |
|         - | 1955 | ` * incoming value, and the hook writes it or refuses. It is dispatched from the` |
|         - | 1956 | ` * one place every overloaded write funnels through, so a plain store, a` |
|         - | 1957 | `` * compound assign, a `??=` and Reflection's setValue() all reach it.`` |
|         - | 1958 | ` */` |
|         - | 1959 | `#define PH7_NATIVE_PROP_STORE  5` |
|         - | 1960 | `/*` |
|         - | 1961 | ` * php's has_property asked the third way -- ZEND_PROPERTY_EXISTS, which is what` |
|         - | 1962 | `` * `property_exists()` passes and nothing else does. A handler may answer it`` |
|         - | 1963 | ` * differently from the emptiness question, and ArrayObject's does: a storage key` |
|         - | 1964 | `` * holding 0 EXISTS and is not `empty()`-false, where PDORow's handler makes no`` |
|         - | 1965 | ` * distinction and answers both by truth.` |
|         - | 1966 | ` */` |
|         - | 1967 | `#define PH7_NATIVE_PROP_EXISTS 6` |
|         - | 1968 | `/*` |
|         - | 1969 | ` * "Would you take a WRITE of this name?", asked where the value does not exist` |
|         - | 1970 | ` * yet -- the member opcode's write shapes, which have to decide between the` |
|         - | 1971 | `` * handler, a magic `__set` and creating a property before the store runs.`` |
|         - | 1972 | ` *` |
|         - | 1973 | ` * Answering (bAnswered) means the write is the handler's and the rails route it` |
|         - | 1974 | ` * to STORE above; declining leaves the name on the ordinary path. It is the one` |
|         - | 1975 | ` * question a handler must answer without seeing a value, so it is about the NAME` |
|         - | 1976 | ` * and the object's state alone: ext/dom answers it from the class's virtual` |
|         - | 1977 | ` * declarations, ArrayObject from its ARRAY_AS_PROPS flag.` |
|         - | 1978 | ` */` |
|         - | 1979 | `#define PH7_NATIVE_PROP_OWNS   7` |
|         - | 1980 | `typedef struct PH7_NativePropCtx PH7_NativePropCtx;` |
|         - | 1981 | `struct PH7_NativePropCtx` |
|         - | 1982 | `{` |
|         - | 1983 | `	int iMode;               /* PH7_NATIVE_PROP_* */` |
|         - | 1984 | `	const SyString *pName;   /* The property being asked about */` |
|         - | 1985 | `	ph7_value *pResult;      /* READ: where the answer goes (the caller inits it NULL).` |
|         - | 1986 | `	                          * ISSET/NOTEMPTY/EXISTS: set to a bool by the hook.` |
|         - | 1987 | `	                          * STORE: the INCOMING value, which the hook stores. */` |
|         - | 1988 | `	int bAnswered;           /* Set by the hook when it OWNS this name; 0 (the caller's` |
|         - | 1989 | `	                          * init) leaves the access to the ordinary path */` |
|         - | 1990 | `	const char *zThrowClass; /* Set by the hook to refuse; 0 (the caller's init) means answered */` |
|         - | 1991 | `	char zThrowMsg[160];     /* ...and its message, formatted by the hook */` |
|         - | 1992 | `	sxi32 iThrowCode;        /* ...and its php $code. A DOM refusal is a DOMException whose` |
|         - | 1993 | `	                          * code a program reads (DOM_NOT_FOUND_ERR & co), so the number` |
|         - | 1994 | `	                          * has to survive the trip out to the site that raises. 0 -- the` |
|         - | 1995 | `	                          * caller's init -- is every other class's answer. */` |
|         - | 1996 | ``	int bQuiet;              /* READ only: this fetch is a LOOKUP (`$o->p ?? d`), php's third`` |
|         - | 1997 | `	                          * accessor level -- it takes the VALUE and says nothing about a` |
|         - | 1998 | `	                          * name that is not there, where a plain read reports it. */` |
|         - | 1999 | ``	int bWriteCtx;           /* READ only: this fetch is the BASE of a write -- `$o->p[0] = 1`,`` |
|         - | 2000 | ``	                          * `$o->p[] = 1`, a destructuring target -- so php asks`` |
|         - | 2001 | `	                          * get_property_ptr_ptr rather than read_property and a handler` |
|         - | 2002 | `	                          * that CAN hand out a real slot should create the name it is` |
|         - | 2003 | `	                          * missing. Set by the member opcode; ignored by a handler with` |
|         - | 2004 | `	                          * no slot to give. */` |
|         - | 2005 | `	sxu32 nSlot;             /* The memobj index the answer LIVES in, for a handler whose` |
|         - | 2006 | `	                          * property is a real element of something the object owns` |
|         - | 2007 | `	                          * (ArrayObject's storage). SXU32_HIGH -- the caller's init --` |
|         - | 2008 | `	                          * means the answer is a value and the access is not an lvalue,` |
|         - | 2009 | `	                          * which is every virtual property's answer. */` |
|         - | 2010 | `};` |
|         - | 2011 | `/*` |
|         - | 2012 | ` * One COMPARISON asked of a native class through ph7_class::xCmp -- php's` |
|         - | 2013 | ` * compare handler.` |
|         - | 2014 | ` *` |
|         - | 2015 | ` * php asks the LEFT operand's handler and takes whatever it answers, so the` |
|         - | 2016 | ` * handler decides for the pair: what the two objects are compared BY (a` |
|         - | 2017 | ` * DateTime is its instant, and neither its zone nor any property), whether the` |
|         - | 2018 | ` * right operand is even a partner it recognizes, and whether the pair is` |
|         - | 2019 | `` * comparable at all. Asked only for `==`/`<`/`<=>` and friends -- `===` is`` |
|         - | 2020 | ` * identity in php and never reaches a handler.` |
|         - | 2021 | ` *` |
|         - | 2022 | ` * The answer is an ordering in iResult. php's ZEND_UNCOMPARABLE is the value 1,` |
|         - | 2023 | ` * which the comparator already uses for every unordered pair (a NaN, two arrays` |
|         - | 2024 | `` * neither containing the other): the operator arms ask `<` from the other side`` |
|         - | 2025 | ` * rather than reading one side's sign, so 1 from BOTH directions leaves every` |
|         - | 2026 | `` * relational spelling false and `==` false, which is exactly what php answers`` |
|         - | 2027 | ` * for an uncomparable pair.` |
|         - | 2028 | ` *` |
|         - | 2029 | ` * A REFUSAL (php throws DateException out of the DateTimeZone handler) is` |
|         - | 2030 | ` * carried back in zThrowClass/zThrowMsg rather than raised here, the way the` |
|         - | 2031 | `` * dimension hook's is: PH7_MemObjCmp runs under `sort()` and `in_array()` as`` |
|         - | 2032 | ` * well as under an operator, and none of those has a throw boundary of its own.` |
|         - | 2033 | ` * The comparator records it on the VM (PH7_CmpRefusalRaise) and the sites that` |
|         - | 2034 | ` * CAN route a throw -- the comparison opcodes, the switch arm and the host-call` |
|         - | 2035 | ` * boundary -- raise it.` |
|         - | 2036 | ` */` |
|         - | 2037 | `typedef struct PH7_NativeCmpCtx PH7_NativeCmpCtx;` |
|         - | 2038 | `struct PH7_NativeCmpCtx` |
|         - | 2039 | `{` |
|         - | 2040 | `	ph7_class_instance *pOther; /* The RIGHT operand as an INSTANCE, or 0 when it is a scalar */` |
|         - | 2041 | `	ph7_value *pOtherValue;     /* ...and the scalar itself, for the object-versus-value door` |
|         - | 2042 | ``	                             * (php asks the same handler for `$n == 2`). 0 when pOther is set. */`` |
|         - | 2043 | `	int bReversed;              /* The instance is the RIGHT operand: the hook owes the` |
|         - | 2044 | `	                             * answer already flipped, EXCEPT for the uncomparable 1,` |
|         - | 2045 | `	                             * which php answers from both directions alike. */` |
|         - | 2046 | `	int bAnswered;              /* Set by the hook when it RECOGNIZED the partner. The scalar` |
|         - | 2047 | `	                             * door falls back to php's cast rule when it did not; the` |
|         - | 2048 | `	                             * instance door keeps its older "always decided" contract. */` |
|         - | 2049 | `	sxi32 iResult;              /* -1 / 0 / 1; 1 is also php's ZEND_UNCOMPARABLE.` |
|         - | 2050 | `	                             * The caller inits it to 1, so a hook that` |
|         - | 2051 | `	                             * recognizes nothing may simply return. */` |
|         - | 2052 | `	const char *zThrowClass;    /* Set by the hook to refuse; 0 (the caller's init) means answered */` |
|         - | 2053 | `	char zThrowMsg[160];        /* ...and its message, formatted by the hook */` |
|         - | 2054 | `};` |
|         - | 2055 | `/*` |
|         - | 2056 | ` * One ARITHMETIC operator asked of a native class through ph7_class::xArith --` |
|         - | 2057 | `` * php's do_operation handler, which is what makes `$a + $b` mean something for`` |
|         - | 2058 | ` * an object.` |
|         - | 2059 | ` *` |
|         - | 2060 | ` * php asks the LEFT operand's handler first and the RIGHT one's when the left` |
|         - | 2061 | ` * has none, so the handler sees a pair it may be either half of and decides for` |
|         - | 2062 | ` * both: what the other operand is allowed to be, how it converts, and what the` |
|         - | 2063 | ` * answer is. Declining (leaving bHandled at 0) puts the pair back on the` |
|         - | 2064 | `` * ordinary numeric path, where an object is `Unsupported operand types`.`` |
|         - | 2065 | ` *` |
|         - | 2066 | ` * A REFUSAL is carried back rather than raised here, the way the dimension and` |
|         - | 2067 | ` * compare hooks' are: the opcode owns the operand stack and has to settle it` |
|         - | 2068 | ` * before any throw, and the exception CLASS varies -- BcMath\Number answers` |
|         - | 2069 | ` * ValueError for a string that is not a number and DivisionByZeroError for a` |
|         - | 2070 | ` * zero divisor, neither of which is the TypeError the ordinary path raises.` |
|         - | 2071 | ` */` |
|         - | 2072 | `typedef struct PH7_NativeArithCtx PH7_NativeArithCtx;` |
|         - | 2073 | `struct PH7_NativeArithCtx` |
|         - | 2074 | `{` |
|         - | 2075 | `	const char *zOp;         /* "+", "-", "*", "/", "%" or "**" */` |
|         - | 2076 | `	ph7_value *pLeft;        /* The two operands, in SOURCE order */` |
|         - | 2077 | `	ph7_value *pRight;` |
|         - | 2078 | `	ph7_value *pResult;      /* Where the handler writes the answer */` |
|         - | 2079 | `	int bHandled;            /* Set by the hook to claim the pair */` |
|         - | 2080 | `	const char *zThrowClass; /* ...or set this to refuse; 0 means no refusal */` |
|         - | 2081 | `	char zThrowMsg[160];     /* ...and word it here */` |
|         - | 2082 | `};` |
|         - | 2083 | `/*` |
|         - | 2084 | ` * Each class is parsed out and stored in an instance of the following structure.` |
|         - | 2085 | ` * PH7 introduced powerfull extensions to the PHP 5 OO subsystems.` |
|         - | 2086 | ` * Please refer to the official documentation for more information.` |
|         - | 2087 | ` */` |
|         - | 2088 | `struct ph7_class` |
|         - | 2089 | `{` |
|         - | 2090 | `	ph7_class *pBase;     /* Base class if any */` |
|         - | 2091 | `	SyHash hDerived;      /* Derived [child] classes */` |
|         - | 2092 | `	SyString sName;       /* Class full qualified name */` |
|         - | 2093 | `	sxi32 iFlags;         /* Class configuration flags [i.e: final, interface, abstract, etc.]  */` |
|         - | 2094 | `	sxu64 nShadowName;    /* One bit per PLAIN name this class holds a MANGLED slot for` |
|         - | 2095 | `	                       * (OoShadowNameBit). PH7_CLASS_SHADOW_PROP says the class has at` |
|         - | 2096 | `	                       * least one; this says WHICH, cheaply enough to ask on every` |
|         - | 2097 | `	                       * property access. Zero when the flag is clear. */` |
|         - | 2098 | `	sxu64 nPrivName;      /* ...and one bit per plain name this class declares as a PRIVATE` |
|         - | 2099 | `	                       * instance property of its own (its trait-composed ones included).` |
|         - | 2100 | `	                       * The other half of the same screen: a scope can only mean a` |
|         - | 2101 | `	                       * mangled slot for a name it declares private itself. */` |
|         - | 2102 | `	SyHash hAttr;         /* Class PROPERTIES [static + instance]. Constants live in hConst */` |
|         - | 2103 | `	SyHash hConst;        /* Class CONSTANTS [incl. enum cases] — php keeps constants and` |
|         - | 2104 | `` 	                       * properties in SEPARATE namespaces, so `const C` and `public $C` `` |
|         - | 2105 | `	                       * coexist. Keyed by name, disjoint from hAttr. */` |
|         - | 2106 | `	SyHash hMethod;       /* Class methods */` |
|         - | 2107 | `	sxu32 nLine;          /* Line number on which this class was declared */` |
|         - | 2108 | `	SySet aInterface;     /* Implemented interface container */` |
|         - | 2109 | `	SySet aTrait;         /* Used trait container */` |
|         - | 2110 | `	ph7_class *pNextName; /* Next class [interface, abstract, etc.] with the same name */` |
|         - | 2111 | `	int bMounted;         /* TRUE if class has been mounted (internal VM state) */` |
|         - | 2112 | `	SyString sFile;       /* Path of the defining file (aliases the VM-lifetime dup in pVm->aFiles).` |
|         - | 2113 | `	                       * nByte == 0 when unknown: Reflection getFileName() reports false. */` |
|         - | 2114 | `	sxu32 nEndLine;       /* Line of the class body's closing brace (Reflection getEndLine) */` |
|         - | 2115 | `	SyString sDoc;        /* Doc-comment preceding the declaration (duplicated; empty = none) */` |
|         - | 2116 | `	SySet aAttrs;         /* Declared #[...] attributes (ph7_attribute records) */` |
|         - | 2117 | `	sxu32 nEnumBacking;   /* Enum backing type: 0 = pure/not an enum, MEMOBJ_INT or MEMOBJ_STRING */` |
|         - | 2118 | `	SySet aEnumCases;     /* Enum cases (ph7_class_attr *) in declaration order. Case singletons` |
|         - | 2119 | `	                       * materialize lazily and INDIVIDUALLY on first access (php 8.1: a broken` |
|         - | 2120 | `	                       * sibling case does not poison a valid one); an unmaterialized case has` |
|         - | 2121 | `	                       * nIdx == SXU32_HIGH. */` |
|         - | 2122 | `	void (*xNew)(ph7_vm *,ph7_class_instance *); /* php's create_object handler, run once the` |
|         - | 2123 | `	                       * instance frame exists and before any constructor. A native class whose` |
|         - | 2124 | `	                       * php counterpart answers its declared properties through a READ handler` |
|         - | 2125 | ``	                       * uses it to SEED those slots: php's ZipArchive declares `public int`` |
|         - | 2126 | ``	                       * $numFiles;` with no default and still shows 0 on a fresh object, because`` |
|         - | 2127 | `	                       * the handler answers rather than the slot. Seeding is the same fact from` |
|         - | 2128 | `	                       * the other side and keeps Reflection honest -- hasDefaultValue() stays` |
|         - | 2129 | `	                       * false, because there is no default, only a starting value. Resolved` |
|         - | 2130 | `	                       * through the ANCESTORS exactly as xRelease is. */` |
|         - | 2131 | `	void (*xRelease)(ph7_vm *,ph7_class_instance *); /* Native teardown for an instance of this class,` |
|         - | 2132 | `	                       * run by PH7_ClassInstanceRelease while the instance's slots are still` |
|         - | 2133 | `	                       * readable. This is NOT __destruct: php's WeakReference declares no` |
|         - | 2134 | `	                       * destructor, so a native class that must release a C-side resource` |
|         - | 2135 | `	                       * states it here instead of growing a method Reflection would report. */` |
|         - | 2136 | `	const PH7_NativeIterVtab *pIterVtab; /* How an InternalIterator walks an instance of this class` |
|         - | 2137 | `	                       * (php's get_iterator handler). Set on native IteratorAggregates whose` |
|         - | 2138 | `	                       * getIterator() answers PH7_NativeIteratorNew(); 0 everywhere else. */` |
|         - | 2139 | `	sxi32 (*xPresent)(ph7_vm *,ph7_class_instance *,ph7_value *,int); /* php's get_properties /` |
|         - | 2140 | `	                       * get_debug_info handlers, as one callback told which is asking:` |
|         - | 2141 | `	                       * the SHAPE a class SHOWS, which for several native classes is nothing` |
|         - | 2142 | `	                       * like the engine state it keeps. php presents a DateTime as` |
|         - | 2143 | `	                       * date/timezone_type/timezone and a WeakReference as ["object"], while` |
|         - | 2144 | `	                       * the slots underneath are a timestamp and a C-side cell — those slots` |
|         - | 2145 | `	                       * carry PH7_MOD_HIDDEN, and this fills an ARRAY with what php shows.` |
|         - | 2146 | `	                       * The last argument is 1 for the DEBUG surfaces (var_dump/print_r) and 0` |
|         - | 2147 | `	                       * for the property ones (var_export, the (array) cast), because php's` |
|         - | 2148 | `	                       * two handlers do not agree: a WeakReference shows ["object"] to` |
|         - | 2149 | `	                       * var_dump and NOTHING to (array), while a DateTime shows the same three` |
|         - | 2150 | `	                       * keys to both. Never consulted by get_object_vars()/foreach, which php` |
|         - | 2151 | `	                       * answers from the real (scoped) properties, nor yet by serialize(),` |
|         - | 2152 | `	                       * where php's answer is an __serialize/__unserialize pair. */` |
|         - | 2153 | `	const char *zNewRefusalClass; /* ...and the exception CLASS that refusal is, when it is` |
|         - | 2154 | ``	                       * not the usual `Error`: php's PDORow refuses `new` with a`` |
|         - | 2155 | `	                       * PDOException. 0 selects Error. */` |
|         - | 2156 | `	const char *zNewRefusal; /* php's create_object refusal TEXT for a PH7_CLASS_NOINSTANTIATE` |
|         - | 2157 | `	                       * class, when it is not the usual "Instantiation of class %s is not` |
|         - | 2158 | `	                       * allowed". php words Directory's as "Cannot directly construct` |
|         - | 2159 | `	                       * Directory, use dir() instead"; 0 selects the standard sentence. */` |
|         - | 2160 | `	void (*xClone)(ph7_vm *,ph7_class_instance *,ph7_class_instance *); /* php's clone_obj handler` |
|         - | 2161 | ``	                       * analogue: what `clone $o` DOES for an instance beyond the slot-by-slot`` |
|         - | 2162 | `	                       * copy, run on (clone, source) after the copy and before any __clone().` |
|         - | 2163 | `	                       * A DOM node's copy must be a copy of the NODE, not a second object over` |
|         - | 2164 | `	                       * the same one -- without this, a mutation through either object writes` |
|         - | 2165 | `	                       * the other. This is NOT __clone: php declares no such method on these` |
|         - | 2166 | `	                       * classes, so Reflection must not report one. Inherited by user` |
|         - | 2167 | `	                       * subclasses (the nearest ancestor's hook runs), which is php's handler` |
|         - | 2168 | `	                       * inheritance. 0 everywhere else. PH7_NativeClassSpec has no field for` |
|         - | 2169 | `	                       * it (a 14th field would touch every row of every spec table under` |
|         - | 2170 | `	                       * -Werror=missing-field-initializers); the owning installer assigns it` |
|         - | 2171 | `	                       * on the mounted class right after PH7_InstallNativeClasses. */` |
|         - | 2172 | `	void (*xDim)(ph7_vm *,ph7_class_instance *,PH7_NativeDimCtx *); /* php's read_dimension /` |
|         - | 2173 | `	                       * has_dimension handlers, as one callback told which is asking.` |
|         - | 2174 | ``	                       * A class states this when `$o[$k]` MEANS something and the class`` |
|         - | 2175 | `	                       * does not implement ArrayAccess -- php 8.3 gave DOMNodeList and` |
|         - | 2176 | `	                       * DOMNamedNodeMap dimension handlers WITHOUT declaring the` |
|         - | 2177 | ``	                       * interface, so `$list[0]` reads there while`` |
|         - | 2178 | ``	                       * `$list instanceof ArrayAccess` is false. No spec field can say`` |
|         - | 2179 | `	                       * that: the interface list is what a class DECLARES, and this is a` |
|         - | 2180 | `	                       * handler underneath it. Assigned on the mounted class by the` |
|         - | 2181 | `	                       * owning installer, like xClone, and inherited by user subclasses` |
|         - | 2182 | `	                       * (the nearest ancestor's hook runs) -- php's handler inheritance,` |
|         - | 2183 | `	                       * which is why a subclass's own offsetGet is NOT consulted for a` |
|         - | 2184 | `	                       * read even when it declares ArrayAccess. The WRITE half stays` |
|         - | 2185 | `	                       * php's: a store, an append and an unset are all` |
|         - | 2186 | ``	                       * `Cannot use object of type C as array` unless the class really`` |
|         - | 2187 | `	                       * implements ArrayAccess. 0 everywhere else. */` |
|         - | 2188 | `	void (*xSet)(ph7_vm *,ph7_class_instance *,PH7_NativeSetCtx *); /* php's write_property` |
|         - | 2189 | `	                       * handler: what a WRITE to one of this class's declared properties` |
|         - | 2190 | `	                       * converts to (or refuses), for a class whose properties are php's` |
|         - | 2191 | `	                       * own C struct. Reached from the store filter through the slot` |
|         - | 2192 | `	                       * table, so every write shape goes through it. Assigned on the` |
|         - | 2193 | `	                       * mounted class by the owning installer, like xClone and xDim,` |
|         - | 2194 | `	                       * which also flags the class's properties PH7_CLASS_ATTR_NATIVE_SET` |
|         - | 2195 | `	                       * so their slots get registered; inherited by user subclasses the` |
|         - | 2196 | `	                       * way php inherits a handler. 0 everywhere else. */` |
|         - | 2197 | `	void (*xProp)(ph7_vm *,ph7_class_instance *,PH7_NativePropCtx *); /* php's` |
|         - | 2198 | `	                       * read_property / has_property / write_property /` |
|         - | 2199 | `	                       * unset_property handlers, as one callback told which is` |
|         - | 2200 | `	                       * asking; see PH7_NativePropCtx. Consulted only where the` |
|         - | 2201 | `	                       * instance has no slot of that name. Assigned on the mounted` |
|         - | 2202 | `	                       * class by the owning installer, like xClone/xDim/xSet, and` |
|         - | 2203 | `	                       * inherited by user subclasses -- php's handler inheritance.` |
|         - | 2204 | `	                       * 0 everywhere else. */` |
|         - | 2205 | `	int (*xBool)(ph7_vm *,ph7_class_instance *); /* php's cast_object for _IS_BOOL: an` |
|         - | 2206 | `	                       * object is ALWAYS truthy unless its class says otherwise, and` |
|         - | 2207 | `	                       * BcMath\Number is the one that does -- a zero Number is falsy. */` |
|         - | 2208 | `	void (*xArith)(ph7_vm *,ph7_class_instance *,PH7_NativeArithCtx *); /* php's do_operation` |
|         - | 2209 | `	                       * handler; see PH7_NativeArithCtx. 0 for every class that has none,` |
|         - | 2210 | `	                       * which is all of them but BcMath\Number. */` |
|         - | 2211 | `	void (*xCmp)(ph7_vm *,ph7_class_instance *,PH7_NativeCmpCtx *); /* php's compare handler:` |
|         - | 2212 | ``	                       * what `==`, `<` and `<=>` MEAN for an instance of this class,`` |
|         - | 2213 | `	                       * asked instead of the property-by-property walk. php gives one` |
|         - | 2214 | `	                       * to the three date classes whose state is not their properties --` |
|         - | 2215 | `	                       * a DateTime compares as an INSTANT across DateTime and` |
|         - | 2216 | `	                       * DateTimeImmutable alike, two DateIntervals are never comparable,` |
|         - | 2217 | `	                       * two DateTimeZones of different kinds are a refusal. Asked of the` |
|         - | 2218 | `	                       * LEFT operand only, before the same-class screen and after the` |
|         - | 2219 | ``	                       * identity shortcut, and never for `===`. Assigned on the mounted`` |
|         - | 2220 | `	                       * class by the owning installer, like xClone/xDim/xSet, and` |
|         - | 2221 | `	                       * inherited by user subclasses (php's handler inheritance: a` |
|         - | 2222 | `	                       * subclass of DateTime still compares as an instant, extra` |
|         - | 2223 | `	                       * properties and all). 0 everywhere else. */` |
|         - | 2224 | `};` |
|         - | 2225 | `/* Class configuration flags */` |
|         - | 2226 | `#define PH7_CLASS_FINAL       0x001 /* Class is final [cannot be extended] */` |
|         - | 2227 | `#define PH7_CLASS_INTERFACE   0x002 /* Class is interface */` |
|         - | 2228 | `#define PH7_CLASS_ABSTRACT    0x004 /* Class is abstract */` |
|         - | 2229 | `#define PH7_CLASS_TRAIT       0x008 /* Class is a trait */` |
|         - | 2230 | `#define PH7_CLASS_TRAIT_VISITING 0x010 /* Trait is currently being applied (cycle detection) */` |
|         - | 2231 | `#define PH7_CLASS_READONLY    0x020 /* Class is readonly (PHP 8.2): every declared property is readonly */` |
|         - | 2232 | `#define PH7_CLASS_INTERNAL    0x040 /* Class was defined while compiling a builtin chunk (embedded PHP` |
|         - | 2233 | `                                     * library). Reflection reports it as internal: isInternal() true,` |
|         - | 2234 | `                                     * getFileName() false. */` |
|         - | 2235 | `#define PH7_CLASS_ENUM        0x080 /* Class is an enum (PHP 8.1). Also carries PH7_CLASS_FINAL. */` |
|         - | 2236 | `#define PH7_CLASS_STATIC_DEFER 0x200 /* This class's static table is not fully materialized: at least` |
|         - | 2237 | `                                      * one static property's default THREW when it was evaluated at` |
|         - | 2238 | `                                      * mount (PH7_CLASS_ATTR_STATIC_DEFER on the attribute) or failed` |
|         - | 2239 | `                                      * its type check (VM_CLASS_ATTR_TYPE_DEFER on the slot). A hint` |
|         - | 2240 | `                                      * only: the access/instantiation sites call` |
|         - | 2241 | `                                      * PH7_VmMaterializeClassStatics, which re-scans the whole base` |
|         - | 2242 | `                                      * chain. Set on the class whose mount saw the failure; the gate` |
|         - | 2243 | `                                      * (VmClassStaticDeferPending) walks the bases, so mount ORDER` |
|         - | 2244 | `                                      * between a base and its subclass does not matter. */` |
|         - | 2245 | `#define PH7_CLASS_LINT_UNBOUND 0x400000 /* Syntax-check compile (phl -l) only: a parent, interface or` |
|         - | 2246 | `                                        * trait this declaration names could not be resolved, and the` |
|         - | 2247 | `                                        * mode carried on with the body rather than refusing. Every` |
|         - | 2248 | `                                        * check that needs the missing member's contents -- #[\Override],` |
|         - | 2249 | `                                        * the unimplemented-abstract count -- is then skipped, which is` |
|         - | 2250 | `                                        * what php does: it reports those only for a class it could` |
|         - | 2251 | `                                        * EARLY-BIND, and it binds nothing whose base it cannot see. */` |
|         - | 2252 | `#define PH7_CLASS_TOPLEVEL    0x200000 /* Declared UNCONDITIONALLY at file top level. php runs such a` |
|         - | 2253 | `                                     * declaration whatever else is in the file, so two of them under one` |
|         - | 2254 | `                                     * name is a redeclaration even when neither was early-bound -- which` |
|         - | 2255 | `                                     * is the difference between this flag and PH7_CLASS_BOUND below. */` |
|         - | 2256 | `#define PH7_CLASS_BOUND       0x100 /* Bound by an UNCONDITIONAL top-level declaration. PHP fatals on a` |
|         - | 2257 | `                                     * second such binding of the same name ("Cannot redeclare ..."); a` |
|         - | 2258 | `                                     * conditional (if/loop/func-nested) declaration is NOT marked, so the` |
|         - | 2259 | ``                                     * `if(false){class C{}}` / `if(!class_exists){..}` guard idioms hoist. */`` |
|         - | 2260 | ``#define PH7_CLASS_NOCLONE     0x400 /* `clone $o` is a catchable Error for this class. A native class whose`` |
|         - | 2261 | `                                     * instances own a C-side resource keyed by a private slot cannot be` |
|         - | 2262 | `                                     * copied slot-by-slot (WeakReference's shared cell would be dropped` |
|         - | 2263 | `                                     * twice), which is exactly why php makes those classes uncloneable.` |
|         - | 2264 | `                                     * Enum cases carry the same rule through PH7_CLASS_ENUM, and` |
|         - | 2265 | `                                     * Generator/Fiber are named directly at the OP_CLONE test. */` |
|         - | 2266 | `#define PH7_CLASS_NOSERIALIZE 0x800 /* serialize() of an instance is a catchable Exception naming the` |
|         - | 2267 | `                                     * class, php's answer for every class holding engine state.` |
|         - | 2268 | `                                     * Without it the default object path emits the private slots —` |
|         - | 2269 | `                                     * for these classes a raw POINTER, which unserialize() would` |
|         - | 2270 | `                                     * hand straight back to a method. php's ZEND_ACC_NOT_SERIALIZABLE:` |
|         - | 2271 | `                                     * tested FIRST and unconditionally, so a subclass declaring` |
|         - | 2272 | `                                     * __serialize() is refused too (DOMXPath is the case that shows` |
|         - | 2273 | `                                     * it). INHERITED — the serializer walks pBase, because php's flag` |
|         - | 2274 | `                                     * rides down to every user subclass. */` |
|         - | 2275 | ``#define PH7_CLASS_NOSERIALIZE_SUBOK 0x2000 /* The SOFT refusal: php's `ce->serialize` deny HANDLER,`` |
|         - | 2276 | `                                     * which the serializer consults only AFTER looking for` |
|         - | 2277 | `                                     * __serialize()/__sleep() — so a SUBCLASS that declares either` |
|         - | 2278 | `                                     * one serializes normally, and php says so in the sentence` |
|         - | 2279 | `                                     * ("…is not allowed, unless serialization methods are` |
|         - | 2280 | `                                     * implemented in a subclass"). The DOM node classes are the` |
|         - | 2281 | `                                     * users; __wakeup() alone does NOT rescue them. Inherited the` |
|         - | 2282 | `                                     * same way as the hard flag. */` |
|         - | 2283 | ``#define PH7_CLASS_DIM_WRITABLE 0x4000 /* A write through `$obj[k]` LANDS on this class's storage.`` |
|         - | 2284 | `                                       * php's split is the read_dimension handler: an internal` |
|         - | 2285 | `                                       * class whose own handler hands back the real element` |
|         - | 2286 | `                                       * (ArrayObject, ArrayIterator, WeakMap) supports indirect` |
|         - | 2287 | `                                       * modification, while everything routed through` |
|         - | 2288 | `                                       * zend_std_read_dimension — every userland ArrayAccess, and` |
|         - | 2289 | `                                       * the SPL classes that keep the standard handler — gets a` |
|         - | 2290 | `                                       * TEMPORARY, so php notices and drops the write. Inherited` |
|         - | 2291 | `                                       * by subclasses (the flag is looked up along pBase), but` |
|         - | 2292 | `                                       * only while the native offsetGet is still the one that` |
|         - | 2293 | `                                       * answers: an override takes the class off the fast handler` |
|         - | 2294 | `                                       * in php too. See PH7_VmDimFetchWritable. */` |
|         - | 2295 | `/*` |
|         - | 2296 | `` * ph7_class::iFlags bit: `(int)` on an instance of this class answers the OBJECT`` |
|         - | 2297 | `` * HANDLE, silently, instead of php's `Object of class X could not be converted to`` |
|         - | 2298 | `` * int` warning and its 1. php gives exactly two classes that cast_object -- the`` |
|         - | 2299 | ` * curl easy and multi handles -- and the reason is stated in its own source: both` |
|         - | 2300 | `` * used to be RESOURCES, whose `(int)` was the resource id, and a program that keyed`` |
|         - | 2301 | ` * a table by it had to keep working. Composer's CurlDownloader is that program.` |
|         - | 2302 | `` * `(float)`, `(string)` and every other cast stay php's refusal, and so does the`` |
|         - | 2303 | ` * COMPARISON, which is a different handler (see PH7_NativeCmpOpaqueHandle).` |
|         - | 2304 | ` */` |
|         - | 2305 | `#define PH7_CLASS_HANDLE_ID   0x40000` |
|         - | 2306 | `/*` |
|         - | 2307 | `` * ph7_class::iFlags bit: `get_object_vars()` on an instance of this class is`` |
|         - | 2308 | ` * answered by the class's ph7_class::xPresent table rather than by its real` |
|         - | 2309 | ` * slots.` |
|         - | 2310 | ` *` |
|         - | 2311 | ` * php's get_properties handler is asked for three PURPOSES, and its native` |
|         - | 2312 | ` * classes mostly disagree between them -- a DateTime shows its three keys to` |
|         - | 2313 | `` * var_dump and to `(array)` and NOTHING to get_object_vars, a DOM node shows a`` |
|         - | 2314 | ` * table to var_dump and nothing to either of the others. SimpleXMLElement is` |
|         - | 2315 | ` * the one that answers all three the same way, so the third purpose is a` |
|         - | 2316 | ` * per-class opt-in instead of a fourth argument every handler would have to` |
|         - | 2317 | ` * learn.` |
|         - | 2318 | ` */` |
|         - | 2319 | `#define PH7_CLASS_VARS_PRESENT 0x80000` |
|         - | 2320 | `/*` |
|         - | 2321 | `` * ph7_class::iFlags bit: `(int)`, `(float)` and every numeric COERCION of an`` |
|         - | 2322 | ` * instance of this class run through the class's string form, silently, instead` |
|         - | 2323 | `` * of php's `Object of class X could not be converted to int` warning and its 1.`` |
|         - | 2324 | ` *` |
|         - | 2325 | `` * php's SimpleXMLElement is the one that does it: `(int)$xml->count` is the`` |
|         - | 2326 | `` * number the element CONTAINS, and `$xml->n + 1` adds to it, because its`` |
|         - | 2327 | ``  * cast_object answers IS_LONG and IS_DOUBLE from the node's text. Its `(bool)` `` |
|         - | 2328 | `` * is a different question again (ph7_class::xBool), and `(string)` is the`` |
|         - | 2329 | ` * ordinary __toString().` |
|         - | 2330 | ` */` |
|         - | 2331 | `#define PH7_CLASS_NUM_AS_STRING 0x100000` |
|         - | 2332 | ``#define PH7_CLASS_ANON        0x20000 /* Declared by `new class {...}`. php has no NAME to put in a`` |
|         - | 2333 | `                                    * type text for it while its body compiles, which is why` |
|         - | 2334 | ``                                    * `self` inside one may not be part of an intersection`` |
|         - | 2335 | `                                    * (see the scope-keyword screen in the type parser). */` |
|         - | 2336 | `#define PH7_CLASS_SHADOW_PROP 0x10000 /* At least one property of this class is filed under php's` |
|         - | 2337 | `                                       * MANGLED storage name -- a base's PRIVATE instance property,` |
|         - | 2338 | `                                       * carried down so the subclass's objects still hold its slot` |
|         - | 2339 | `                                       * (PH7_ClassAttrStorageName). The only way two slots of one` |
|         - | 2340 | `                                       * object can share a plain NAME, which is what the by-name` |
|         - | 2341 | `                                       * presentation surfaces have to de-duplicate. */` |
|         - | 2342 | `#define PH7_CLASS_LAZY_ATTR    0x8000 /* This class declares at least one PH7_CLASS_ATTR_NATIVE_LAZY` |
|         - | 2343 | `                                       * property. The O(1) gate in front of the materialization walk:` |
|         - | 2344 | `                                       * every native class writes its slots through the same setters,` |
|         - | 2345 | `                                       * and only these two classes have anything to install. */` |
|         - | 2346 | ``#define PH7_CLASS_NOINSTANTIATE 0x1000 /* `new C` is refused by the OBJECT-CREATION step, before the`` |
|         - | 2347 | `                                     * constructor's visibility is ever consulted — php's` |
|         - | 2348 | `                                     * "Instantiation of class %s is not allowed", which its` |
|         - | 2349 | `                                     * create_object handler raises. The distinction is visible:` |
|         - | 2350 | `                                     * Closure's __construct is PRIVATE (Reflection prints it that` |
|         - | 2351 | ``                                     * way), so without this flag `new Closure` reports a visibility`` |
|         - | 2352 | `                                     * refusal ("Call to private Closure::__construct() from global` |
|         - | 2353 | `                                     * scope") where php reports the instantiation one. A class that` |
|         - | 2354 | `                                     * merely wants a private ctor does NOT want this bit. */` |
|         - | 2355 | `/* Class attribute/methods/constants protection levels */` |
|         - | 2356 | `#define PH7_CLASS_PROT_PUBLIC     1 /* public */` |
|         - | 2357 | `#define PH7_CLASS_PROT_PROTECTED  2 /* protected */` |
|         - | 2358 | `#define PH7_CLASS_PROT_PRIVATE    3 /* private */` |
|         - | 2359 | `/*` |
|         - | 2360 | ` * each class attribute (variable, constants) is parsed out and stored` |
|         - | 2361 | ` * in an instance of the following structure.` |
|         - | 2362 | ` */` |
|         - | 2363 | `struct ph7_class_attr` |
|         - | 2364 | `{` |
|         - | 2365 | `	SyString sName;      /* Atrribute name */` |
|         - | 2366 | `	SyString sStoreName; /* php's MANGLED storage name for a PRIVATE instance property --` |
|         - | 2367 | `	                      * "\0DeclaringClass\0name" -- materialized the first time this` |
|         - | 2368 | `	                      * attribute is filed in a class that did not declare it. Empty` |
|         - | 2369 | `	                      * (nByte == 0) until then, and for every other attribute, whose` |
|         - | 2370 | `	                      * storage name is sName. See PH7_ClassAttrStorageName. */` |
|         - | 2371 | `	sxi32 iFlags;        /* Attribute configuration [i.e: static, variable, constant, etc.] */` |
|         - | 2372 | `	sxi32 iProtection;   /* Protection level [i.e: public, private, protected] */` |
|         - | 2373 | `	SySet aByteCode;     /* Compiled attribute body */` |
|         - | 2374 | `	sxu32 nIdx;          /* Attribute index */` |
|         - | 2375 | `	sxu32 nLine;         /* Line number on which this attribute was defined */` |
|         - | 2376 | `	sxu32 nType;         /* Declared type: MEMOBJ_* bitmask, SXU32_HIGH for class, 0 = untyped */` |
|         - | 2377 | `	SyString sClass;     /* Class/interface name when nType == SXU32_HIGH */` |
|         - | 2378 | `	SyString sTypeName;  /* Original type text for error messages (e.g. "?int", "Foo", "string\|int") */` |
|         - | 2379 | `	SySet aUnionAlts;    /* Union alternatives (ph7_type_alt). Empty unless PH7_CLASS_ATTR_UNION is set. */` |
|         - | 2380 | `	ph7_class *pDeclClass; /* Class that originally declared this attribute */` |
|         - | 2381 | `	SyString sDoc;       /* Doc-comment preceding the declaration (duplicated; empty = none) */` |
|         - | 2382 | `	SySet aAttrs;        /* Declared #[...] attributes (ph7_attribute records) */` |
|         - | 2383 | `	const void *pNativeValue; /* A native class's literal initializer (PH7_NativeConstDef*), or 0.` |
|         - | 2384 | `	                      * A compiled declaration expresses its default as aByteCode evaluated at` |
|         - | 2385 | `	                      * mount; the C builder has no compiler to emit that, so it hands the` |
|         - | 2386 | `	                      * literal here and the mount writes it straight into the reserved slot.` |
|         - | 2387 | `	                      * Mutually exclusive with a non-empty aByteCode. */` |
|         - | 2388 | `};` |
|         - | 2389 | `/* Attribute configuration */` |
|         - | 2390 | `#define PH7_CLASS_ATTR_STATIC       0x001  /* Static attribute */` |
|         - | 2391 | `#define PH7_CLASS_ATTR_CONSTANT     0x002  /* Constant attribute */` |
|         - | 2392 | `#define PH7_CLASS_ATTR_ABSTRACT     0x004  /* Abstract method */` |
|         - | 2393 | `#define PH7_CLASS_ATTR_FINAL        0x008  /* Final method */` |
|         - | 2394 | `#define PH7_CLASS_ATTR_TYPED        0x010  /* Property has an explicit declared type */` |
|         - | 2395 | `#define PH7_CLASS_ATTR_NULLABLE     0x020  /* Type allows null (?type prefix or T\|null union) */` |
|         - | 2396 | `#define PH7_CLASS_ATTR_UNION        0x040  /* Property has a union type (use aUnionAlts) */` |
|         - | 2397 | `#define PH7_CLASS_ATTR_READONLY     0x080  /* readonly property (PHP 8.1) */` |
|         - | 2398 | `#define PH7_CLASS_ATTR_DYNAMIC      0x100  /* Runtime-added (dynamic) property: the ph7_class_attr is` |
|         - | 2399 | `                                            * instance-owned (synthesized, not class-declared) and must` |
|         - | 2400 | `                                            * be freed when the instance is released. */` |
|         - | 2401 | `#define PH7_CLASS_ATTR_ENUMCASE     0x200  /* Enum case: a class constant whose value is the lazily` |
|         - | 2402 | `                                            * materialized case singleton (aByteCode holds the BACKING` |
|         - | 2403 | `                                            * value expression for backed enums; empty when pure). */` |
|         - | 2404 | `#define PH7_CLASS_ATTR_EVALING      0x400  /* Transient: this constant's initializer is being evaluated` |
|         - | 2405 | `                                            * (on-demand, VmClassConstEvalOnDemand). Re-entry means a` |
|         - | 2406 | `                                            * self-referencing constant — php's catchable Error. */` |
|         - | 2407 | `#define PH7_CLASS_ATTR_PRIVATE_SET  0x800  /* private(set) asymmetric visibility (PHP 8.4): writes` |
|         - | 2408 | `                                            * only from the DECLARING class scope (subclasses excluded) */` |
|         - | 2409 | `#define PH7_CLASS_ATTR_PROTECTED_SET 0x1000 /* protected(set) asymmetric visibility (PHP 8.4): writes` |
|         - | 2410 | `                                            * from the declaring class or a subclass scope */` |
|         - | 2411 | `#define PH7_CLASS_ATTR_PUBLIC_SET   0x2000 /* explicit public(set): behaviorally the default, kept` |
|         - | 2412 | `                                            * for the weaker-than-set check and reflection output */` |
|         - | 2413 | ``#define PH7_CLASS_ATTR_HOOK_GET     0x4000 /* property has a `get` hook (PHP 8.4): reads dispatch`` |
|         - | 2414 | `                                            * __phl_hook_get_NAME (guard-bypassed inside hooks) */` |
|         - | 2415 | ``#define PH7_CLASS_ATTR_HOOK_SET     0x8000 /* property has a `set` hook (PHP 8.4): plain writes`` |
|         - | 2416 | `                                            * dispatch __phl_hook_set_NAME */` |
|         - | 2417 | `#define PH7_CLASS_ATTR_HOOK_VIRTUAL 0x10000 /* PHP 8.4 VIRTUAL hooked property: none of its own` |
|         - | 2418 | ``                                            * hook bodies references `$this->NAME`, so php gives it`` |
|         - | 2419 | `                                            * no backing store — excluded from the raw object` |
|         - | 2420 | `                                            * surfaces (var_dump/(array)/print_r/serialize/` |
|         - | 2421 | `                                            * get_class_vars and the get-dispatching walks when it` |
|         - | 2422 | `                                            * has no get hook), no default allowed, reads without a` |
|         - | 2423 | `                                            * get hook are php's "is write-only" Error. PHL still` |
|         - | 2424 | `                                            * allocates the (null) backing slot; this flag hides it. */` |
|         - | 2425 | `#define PH7_CLASS_ATTR_NATIVE_SET   0x100000 /* A NATIVE class's property whose WRITES run through` |
|         - | 2426 | `                                            * ph7_class::xSet (php's write_property). Set by the` |
|         - | 2427 | `                                            * installer that assigns the hook, and read by the two` |
|         - | 2428 | `                                            * places that care: instantiation, which registers the` |
|         - | 2429 | `                                            * slot so the store filter can find it, and the filter` |
|         - | 2430 | `                                            * itself. A slot carrying it is registered in` |
|         - | 2431 | `                                            * pVm->hTypedSlot exactly as a typed one is -- that table` |
|         - | 2432 | `                                            * is "slots a store must be filtered through", and the` |
|         - | 2433 | `                                            * two reasons compose (a native property may also be` |
|         - | 2434 | `                                            * typed). */` |
|         - | 2435 | `#define PH7_CLASS_ATTR_REFSRCPIN    0x8000000 /* STATIC property that is the SOURCE of a reference:` |
|         - | 2436 | `                                               * it holds one counted pin on its own slot, the` |
|         - | 2437 | `                                               * instance-side VM_CLASS_ATTR_REFSRCPIN's twin. A` |
|         - | 2438 | `                                               * class static lives as long as the VM, so the pin` |
|         - | 2439 | `                                               * is never given back -- which is the point: it` |
|         - | 2440 | `                                               * stops the other end's unpin from freeing it. */` |
|         - | 2441 | `` #define PH7_CLASS_ATTR_REFBOUND     0x80000 /* STATIC property currently bound to another slot by `=&` `` |
|         - | 2442 | ``                                             * (`C::$s =& $x`). The instance side records this per`` |
|         - | 2443 | `                                             * INSTANCE (VM_CLASS_ATTR_REFBOUND); a static has one slot` |
|         - | 2444 | `                                             * per declaration, so the bit lives here. It says the slot` |
|         - | 2445 | `                                             * this attribute points at is held by a COUNTED pin, and a` |
|         - | 2446 | `                                             * rebind must give that pin back rather than free a slot the` |
|         - | 2447 | `                                             * attribute never owned. */` |
|         - | 2448 | `#define PH7_CLASS_ATTR_HIDDEN       0x40000 /* A NATIVE class's engine slot: real storage that php keeps` |
|         - | 2449 | `                                            * in its own C struct and therefore never shows. Excluded` |
|         - | 2450 | `                                            * from every PRESENTATION surface — var_dump/print_r/` |
|         - | 2451 | `                                            * var_export, (array), get_object_vars, foreach, json_encode,` |
|         - | 2452 | `                                            * serialize, http_build_query and Reflection's property` |
|         - | 2453 | ``                                            * listing — while `new`, clone and the native bodies' own`` |
|         - | 2454 | `                                            * PH7_NativeAttr() reads still see it. Set from` |
|         - | 2455 | `                                            * PH7_MOD_HIDDEN on a PH7_NativePropDef. Use it for a slot` |
|         - | 2456 | `                                            * php shows NOTHING for (a handle, a cursor cache); a slot` |
|         - | 2457 | ``                                            * php shows under a DIFFERENT name (ArrayObject's `storage`,`` |
|         - | 2458 | ``                                            * DateTime's `date`) wants the presentation hook §7.4 (e)`` |
|         - | 2459 | `                                            * still asks for, not this bit. */` |
|         - | 2460 | `#define PH7_CLASS_ATTR_STATIC_DEFER 0x20000 /* STATIC property whose default initializer THREW when it` |
|         - | 2461 | `                                            * was evaluated at class mount. php never evaluates a static` |
|         - | 2462 | `                                            * default at declaration time — it materializes the class's` |
|         - | 2463 | `                                            * static table at the FIRST static-property access — so the` |
|         - | 2464 | `                                            * mount-time throw is raised MUTED (VmEvalDefaultMuted: no` |
|         - | 2465 | `                                            * catch runs, nothing is reported) and rolled back whole,` |
|         - | 2466 | `                                            * and the initializer re-runs at that first access` |
|         - | 2467 | `                                            * (PH7_VmMaterializeClassStatics), where php raises it.` |
|         - | 2468 | `                                            * Cleared once the initializer completes without throwing:` |
|         - | 2469 | `                                            * a class whose bad default is never READ stays silent in` |
|         - | 2470 | `                                            * both engines, and a re-run that now succeeds (the constant` |
|         - | 2471 | `                                            * it names was define()d after the declaration) answers the` |
|         - | 2472 | `                                            * value, as php's does. */` |
|         - | 2473 | `#define PH7_CLASS_ATTR_NATIVE_VIRTUAL 0x200000 /* A NATIVE class's property that php FABRICATES on` |
|         - | 2474 | `                                            * demand (its get_properties handler) instead of keeping` |
|         - | 2475 | `                                            * in the object's real property table. DatePeriod's seven` |
|         - | 2476 | `                                            * are php's case: they read and write like ordinary` |
|         - | 2477 | `                                            * properties, so PHL declares real slots for them, but` |
|         - | 2478 | `                                            * php's COMPARISON walks the real table and finds nothing` |
|         - | 2479 | `                                            * there -- which is why any two DatePeriods are equal in` |
|         - | 2480 | `                                            * php whatever they contain, while a subclass's own` |
|         - | 2481 | `                                            * property still decides. Read only by the object` |
|         - | 2482 | `                                            * comparator; presentation is the HIDDEN bit's business,` |
|         - | 2483 | `                                            * and these are shown. */` |
|         - | 2484 | `#define PH7_CLASS_ATTR_NATIVE_LAZY  0x400000 /* A NATIVE class's property the OBJECT does not hold until` |
|         - | 2485 | `                                            * its constructor fills it. php's DateInterval and` |
|         - | 2486 | `                                            * DatePeriod are the case: the state lives in a C struct the` |
|         - | 2487 | `                                            * constructor allocates, and the property table is written` |
|         - | 2488 | `                                            * FROM that struct -- so an object nobody constructed has` |
|         - | 2489 | ``                                            * no such property at all, and `$i->y` there is an`` |
|         - | 2490 | ``                                            * `Undefined property` warning, `isset()` is false and`` |
|         - | 2491 | `                                            * get_object_vars()/foreach see nothing. The instance frame` |
|         - | 2492 | ``                                            * skips these at `new`; the whole set is installed, in`` |
|         - | 2493 | `                                            * declared order, the first time a C body writes one` |
|         - | 2494 | `                                            * (PH7_NativeMaterializeLazy), which is every constructor` |
|         - | 2495 | `                                            * and every C factory. */` |
|         - | 2496 | `#define PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT 0x800000 /* A LAZY property php really does DECLARE -- so` |
|         - | 2497 | `                                            * Reflection lists it whatever the object holds -- and whose` |
|         - | 2498 | `                                            * READ, while the slot is still absent, answers the declared` |
|         - | 2499 | `                                            * literal in SILENCE. php's split between its two handlers:` |
|         - | 2500 | `                                            * DatePeriod declares its seven and reads them through a` |
|         - | 2501 | `                                            * read_property that answers the ZEROED struct` |
|         - | 2502 | `                                            * (null/0/false), while DateInterval declares nothing at all` |
|         - | 2503 | `                                            * and its ten are undefined until the constructor runs. The` |
|         - | 2504 | `                                            * literal IS that zeroed field, which is why one bit says` |
|         - | 2505 | `                                            * both things. */` |
|         - | 2506 | `#define PH7_CLASS_ATTR_NATIVE_NOWRITE 0x1000000 /* A NATIVE class's property whose write_property` |
|         - | 2507 | `                                            * handler REFUSES every write. DatePeriod's seven are` |
|         - | 2508 | ``                                            * php's case: its handler answers `Cannot modify readonly`` |
|         - | 2509 | ``                                            * property C::$p` -- the readonly WORDING without the`` |
|         - | 2510 | `                                            * readonly flag, so Reflection still reports isReadOnly()` |
|         - | 2511 | ``                                            * false -- and its unset handler answers `Cannot unset`` |
|         - | 2512 | ``                                            * C::$p`. Every write form is refused, not just `=`:`` |
|         - | 2513 | ``                                            * `++`, a by-reference bind, a destructuring target, and`` |
|         - | 2514 | `                                            * a write to an object that was never constructed. */` |
|         - | 2515 | `#define PH7_CLASS_ATTR_NATIVE_ONDEMAND 0x2000000 /* A LAZY property the group materialization` |
|         - | 2516 | `                                            * SKIPS: it is installed only when a C body writes` |
|         - | 2517 | `                                            * it by name, so an object that never took one does` |
|         - | 2518 | `                                            * not carry the name at all. php's DateInterval` |
|         - | 2519 | ``                                            * `date_string` is the case -- it exists on an`` |
|         - | 2520 | `                                            * interval built from a STRING and on no other, and` |
|         - | 2521 | ``                                            * `isset()`/`property_exists()` answer false there. */`` |
|         - | 2522 | `#define PH7_CLASS_ATTR_NATIVE_NOSLOT 0x4000000 /* A NATIVE class's VIRTUAL property: php DECLARES the` |
|         - | 2523 | `                                            * name (Reflection lists it, property_exists() answers` |
|         - | 2524 | `                                            * true, isVirtual() true and hasDefaultValue() false) and` |
|         - | 2525 | `                                            * keeps NO slot for it -- every value is a read_property /` |
|         - | 2526 | `                                            * write_property handler over the extension's own state.` |
|         - | 2527 | ``                                            * ext/dom is the case: all forty of `DOMDocument`'s are`` |
|         - | 2528 | ``                                            * handlers, which is why php's `(array)` cast,`` |
|         - | 2529 | ``                                            * `get_object_vars()`, `json_encode()`, `foreach`,`` |
|         - | 2530 | ``                                            * `var_export()` and `get_mangled_object_vars()` show`` |
|         - | 2531 | ``                                            * NOTHING for one while `print_r`/`var_dump` show the`` |
|         - | 2532 | `                                            * whole forty (the get_debug_info handler, ph7_class::` |
|         - | 2533 | ``                                            * xPresent). The instance frame skips these at `new`, so`` |
|         - | 2534 | `                                            * a read, a write and an isset() all take the miss path` |
|         - | 2535 | `                                            * and reach the class's __get/__set/__isset exactly as an` |
|         - | 2536 | ``                                            * undeclared name does; `unset()` is php's`` |
|         - | 2537 | ``                                            * `Cannot unset C::$p` rather than a silent no-op. */`` |
|         - | 2538 | `/* next free bit: 0x8000000 */` |
|         - | 2539 | `/*` |
|         - | 2540 | ` * Does a store into this property's slot have to be FILTERED? Two unrelated` |
|         - | 2541 | ` * reasons say yes -- a declared TYPE to enforce and a native class's own write` |
|         - | 2542 | ` * handler -- and both are answered by one lookup, since pVm->hTypedSlot keys` |
|         - | 2543 | ` * every filtered slot by its memobj index. Instantiation registers on this` |
|         - | 2544 | ` * predicate and the teardown paths deregister on it, so the two must never` |
|         - | 2545 | ` * disagree.` |
|         - | 2546 | ` */` |
|         - | 2547 | `#define PH7_ATTR_STORE_FILTERED(pAttr) \` |
|         - | 2548 | `	(((pAttr)->iFlags & (PH7_CLASS_ATTR_TYPED\|PH7_CLASS_ATTR_NATIVE_SET \` |
|         - | 2549 | `	                     \|PH7_CLASS_ATTR_NATIVE_NOWRITE)) != 0)` |
|         - | 2550 | `/*` |
|         - | 2551 | ` * Declaring a class from C (oo_native.c).` |
|         - | 2552 | ` *` |
|         - | 2553 | ` * A subsystem describes its classes as static tables and hands them to` |
|         - | 2554 | ` * PH7_InstallNativeClasses(), which drives the very builders the compiler drives` |
|         - | 2555 | `` * for `class Foo {}`. The point of the exercise is the METHOD table: a method's`` |
|         - | 2556 | ` * body may be a C routine (VM_FUNC_NATIVE), so the engine-access helpers that had` |
|         - | 2557 | `` * to be global `__prefix_verb()` thunks — because only a global function could be`` |
|         - | 2558 | ` * C — become methods of the class they always belonged to.` |
|         - | 2559 | ` */` |
|         - | 2560 | `/* Member modifiers. Visibility defaults to public when none is given. */` |
|         - | 2561 | `#define PH7_MOD_PUBLIC     0x00` |
|         - | 2562 | `#define PH7_MOD_PROTECTED  0x01` |
|         - | 2563 | `#define PH7_MOD_PRIVATE    0x02` |
|         - | 2564 | `#define PH7_MOD_STATIC     0x04` |
|         - | 2565 | `#define PH7_MOD_FINAL      0x08` |
|         - | 2566 | `#define PH7_MOD_ABSTRACT   0x10 /* No body: an interface's method, or an abstract declaration */` |
|         - | 2567 | `#define PH7_MOD_HIDDEN     0x20 /* PROPERTY only: an engine slot php keeps in its own struct and` |
|         - | 2568 | `                                 * never presents (PH7_CLASS_ATTR_HIDDEN). */` |
|         - | 2569 | ``#define PH7_MOD_READONLY   0x40 /* PROPERTY only: php's `readonly` (PH7_CLASS_ATTR_READONLY) */`` |
|         - | 2570 | ``#define PH7_MOD_PROT_SET   0x80 /* PROPERTY only: php's `protected(set)` asymmetric visibility */`` |
|         - | 2571 | ``#define PH7_MOD_PRIV_SET   0x100 /* PROPERTY only: php's `private(set)` asymmetric visibility */`` |
|         - | 2572 | `#define PH7_MOD_ONDEMAND   0x200 /* PROPERTY only: installed on the object only when a C body` |
|         - | 2573 | `                                  * writes it (PH7_CLASS_ATTR_NATIVE_ONDEMAND) */` |
|         - | 2574 | `#define PH7_MOD_VIRTUAL    0x400 /* PROPERTY only: php's VIRTUAL native property -- declared on the` |
|         - | 2575 | `                                  * class and answered by its own handlers, with NO slot on the` |
|         - | 2576 | `                                  * object (PH7_CLASS_ATTR_NATIVE_NOSLOT) */` |
|         - | 2577 | `/* Literal kinds a native class constant may carry */` |
|         - | 2578 | `#define PH7_NATIVE_VAL_NULL   0` |
|         - | 2579 | `#define PH7_NATIVE_VAL_INT    1` |
|         - | 2580 | `#define PH7_NATIVE_VAL_STRING 2` |
|         - | 2581 | `#define PH7_NATIVE_VAL_BOOL   3` |
|         - | 2582 | `#define PH7_NATIVE_VAL_DOUBLE 4` |
|         - | 2583 | ``#define PH7_NATIVE_VAL_ARRAY  6 /* The EMPTY array, php's `private array $trace = [];`. The`` |
|         - | 2584 | `                                 * only array literal a stub default needs — anything with` |
|         - | 2585 | `                                 * elements would want the compiler's byte-code. */` |
|         - | 2586 | `#define PH7_NATIVE_VAL_NONE   5 /* On a PROPERTY row only: the slot has NO default at all,` |
|         - | 2587 | ``                                 * php's `public string $name;`. It needs a declared zType to`` |
|         - | 2588 | `                                 * mean anything (an untyped slot without a default is null),` |
|         - | 2589 | ``                                 * and it makes the property UNINITIALIZED at `new` — reading`` |
|         - | 2590 | `                                 * it before the class's own C body writes it is php's` |
|         - | 2591 | `                                 * "must not be accessed before initialization" Error, and` |
|         - | 2592 | `                                 * hasDefaultValue() answers false. PH7_NATIVE_VAL_NULL is the` |
|         - | 2593 | ``                                 * different thing it reads like: an explicit `= null`. */`` |
|         - | 2594 | `typedef struct PH7_NativeMethodDef PH7_NativeMethodDef;` |
|         - | 2595 | `typedef struct PH7_NativeConstDef  PH7_NativeConstDef;` |
|         - | 2596 | `typedef struct PH7_NativeClassSpec PH7_NativeClassSpec;` |
|         - | 2597 | `struct PH7_NativeMethodDef` |
|         - | 2598 | `{` |
|         - | 2599 | `	const char *zName;       /* php-visible method name */` |
|         - | 2600 | `	sxi32 iMods;             /* PH7_MOD_* */` |
|         - | 2601 | `	const char *zSig;        /* PHP-style parameter list ("string $name, int $flags = 0"),` |
|         - | 2602 | `	                          * or 0 for "unenforced". Static storage: never freed. Drives` |
|         - | 2603 | `	                          * arity enforcement, the by-ref mask AND Reflection, from the` |
|         - | 2604 | `	                          * one string — the same contract aBuiltinSig[] has. */` |
|         - | 2605 | `	const char *zRet;        /* Return-type text, or 0 */` |
|         - | 2606 | `	ProchHostFunction xFunc; /* The body */` |
|         - | 2607 | `};` |
|         - | 2608 | `struct PH7_NativeConstDef` |
|         - | 2609 | `{` |
|         - | 2610 | `	const char *zName;` |
|         - | 2611 | `	sxi32 iMods;` |
|         - | 2612 | `	sxi32 iType;             /* PH7_NATIVE_VAL_* */` |
|         - | 2613 | `	ph7_int64 iValue;        /* INT / BOOL */` |
|         - | 2614 | `	const char *zValue;      /* STRING */` |
|         - | 2615 | `	double rValue;           /* DOUBLE */` |
|         - | 2616 | `};` |
|         - | 2617 | `/*` |
|         - | 2618 | ` * A declared property. Its default is the same literal record a constant uses --` |
|         - | 2619 | ` * a compiled declaration would carry compiled byte-code here, which the builder` |
|         - | 2620 | ` * has no compiler to emit, so the value is stated directly and materialized at` |
|         - | 2621 | `` * `new` (instance) or at mount (static) by PH7_NativeLiteralValue.`` |
|         - | 2622 | ` */` |
|         - | 2623 | `typedef struct PH7_NativePropDef PH7_NativePropDef;` |
|         - | 2624 | `struct PH7_NativePropDef` |
|         - | 2625 | `{` |
|         - | 2626 | `	const char *zName;` |
|         - | 2627 | `	sxi32 iMods;             /* PH7_MOD_* (STATIC supported; FINAL ignored) */` |
|         - | 2628 | `	PH7_NativeConstDef sDefault; /* iType PH7_NATIVE_VAL_NULL = plain null default,` |
|         - | 2629 | `	                              * PH7_NATIVE_VAL_NONE = no default at all (typed slots) */` |
|         - | 2630 | `	const char *zType;       /* Declared type as php writes it ("?string", "int", "DateInterval"),` |
|         - | 2631 | `	                          * or 0 for an untyped slot. Enforced on every store and printed by` |
|         - | 2632 | ``	                          * Reflection exactly as a compiled `public ?string $p` would be —`` |
|         - | 2633 | `	                          * php declares a type on every property it presents, so a slot the` |
|         - | 2634 | ``	                          * class SHOWS wants one. Single atoms only (a leading `?` plus one`` |
|         - | 2635 | `	                          * scalar keyword or class name); a union needs the compiler's` |
|         - | 2636 | `	                          * alternative set and is not expressible here. */` |
|         - | 2637 | `};` |
|         - | 2638 | `struct PH7_NativeClassSpec` |
|         - | 2639 | `{` |
|         - | 2640 | `	const char *zName;` |
|         - | 2641 | `	const char *zParent;     /* or 0 */` |
|         - | 2642 | `	const char *zImplements; /* comma-separated list, or 0 */` |
|         - | 2643 | `	sxi32 iFlags;            /* PH7_CLASS_FINAL / ABSTRACT / INTERFACE / READONLY */` |
|         - | 2644 | `	const PH7_NativeMethodDef *aMethod; sxu32 nMethod;` |
|         - | 2645 | `	const PH7_NativeConstDef  *aConst;  sxu32 nConst;` |
|         - | 2646 | `	const PH7_NativePropDef   *aProp;   sxu32 nProp;` |
|         - | 2647 | `	void (*xRelease)(ph7_vm *,ph7_class_instance *); /* or 0; see ph7_class::xRelease */` |
|         - | 2648 | `	const PH7_NativeIterVtab *pIterVtab; /* or 0; see ph7_class::pIterVtab */` |
|         - | 2649 | `	/* xNew is not stated here: a spec that wants one installs it after mounting` |
|         - | 2650 | `	 * with PH7_NativeClassInstallNewHook(), the way the property, set and` |
|         - | 2651 | `	 * comparison hooks are installed. */` |
|         - | 2652 | `	sxi32 (*xPresent)(ph7_vm *,ph7_class_instance *,ph7_value *,int); /* or 0; see ph7_class::xPresent */` |
|         - | 2653 | `};` |
|         - | 2654 | `/*` |
|         - | 2655 | `` * One `case Name = <literal>;` of a native ENUM. The backing value is the same`` |
|         - | 2656 | ` * literal record a constant carries; iType PH7_NATIVE_VAL_NULL is a PURE enum's` |
|         - | 2657 | ` * case, which has no value at all.` |
|         - | 2658 | ` */` |
|         - | 2659 | `typedef struct PH7_NativeEnumCase PH7_NativeEnumCase;` |
|         - | 2660 | `struct PH7_NativeEnumCase` |
|         - | 2661 | `{` |
|         - | 2662 | `	const char *zName;` |
|         - | 2663 | `	PH7_NativeConstDef sValue;   /* the backing literal; zName/iMods unused */` |
|         - | 2664 | `};` |
|         - | 2665 | `PH7_PRIVATE sxi32 PH7_InstallNativeClasses(ph7_vm *pVm,const PH7_NativeClassSpec *aSpec,sxu32 nSpec);` |
|         - | 2666 | `PH7_PRIVATE int PH7_ClassInstancePresent(ph7_class_instance *pThis,ph7_value *pOut,int bDebug);` |
|         - | 2667 | `PH7_PRIVATE int PH7_ClassHasNativeDim(ph7_class *pClass);` |
|         - | 2668 | `PH7_PRIVATE int PH7_ClassNativeDim(ph7_class_instance *pThis,PH7_NativeDimCtx *pCtx);` |
|         - | 2669 | `/* Offer a dimension WRITE / APPEND / UNSET to the class's own handler, with the` |
|         - | 2670 | ` * offset and the incoming value. Answers 1 when the handler TOOK it (or refused` |
|         - | 2671 | ` * it in its own words, which zThrowClass then carries) and 0 when the caller` |
|         - | 2672 | ` * must raise the ordinary refusal. */` |
|         - | 2673 | `PH7_PRIVATE int PH7_ClassNativeDimStore(ph7_class_instance *pThis,int iMode,` |
|         - | 2674 | `	ph7_value *pOffset,ph7_value *pValue,PH7_NativeDimCtx *pCtx);` |
|         - | 2675 | `/* The refusal a native container gives a dimension WRITE/APPEND/UNSET: its own` |
|         - | 2676 | `` * sentence when its hook words one, and php's `Cannot use object of type C as`` |
|         - | 2677 | `` * array` for every class that does not. Answers the message length. */`` |
|         - | 2678 | `PH7_PRIVATE sxu32 PH7_ClassNativeDimRefusal(ph7_class_instance *pThis,int iMode,` |
|         - | 2679 | `	char *zMsg,sxu32 nMsg);` |
|         - | 2680 | `/* php's instantiation gate -- interface / trait / enum / abstract / a class` |
|         - | 2681 | `` * whose create_object handler refuses -- asked by every C-side `new`. */`` |
|         - | 2682 | `PH7_PRIVATE sxi32 PH7_VmCheckInstantiable(ph7_context *pCtx,ph7_class *pClass);` |
|         - | 2683 | `PH7_PRIVATE int PH7_ClassHasNativeProp(ph7_class *pClass);` |
|         - | 2684 | `PH7_PRIVATE int PH7_ClassNativeProp(ph7_class_instance *pThis,PH7_NativePropCtx *pCtx);` |
|         - | 2685 | `PH7_PRIVATE int PH7_ClassNativePropAsk(ph7_class_instance *pThis,PH7_NativePropCtx *pCtx,` |
|         - | 2686 | `	int iMode,const SyString *pName,ph7_value *pResult);` |
|         - | 2687 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallPropHook(ph7_vm *pVm,const char *zClass,` |
|         - | 2688 | `	void (*xProp)(ph7_vm *,ph7_class_instance *,PH7_NativePropCtx *));` |
|         - | 2689 | `PH7_PRIVATE int PH7_ClassNativePropOwns(ph7_class_instance *pThis,const SyString *pName);` |
|         - | 2690 | `PH7_PRIVATE int PH7_ClassNativeSet(ph7_class_instance *pThis,PH7_NativeSetCtx *pCtx);` |
|         - | 2691 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallSetHook(ph7_vm *pVm,const char *zClass,` |
|         - | 2692 | `	void (*xSet)(ph7_vm *,ph7_class_instance *,PH7_NativeSetCtx *));` |
|         - | 2693 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallNewHook(ph7_vm *pVm,const char *zClass,` |
|         - | 2694 | `	void (*xNew)(ph7_vm *,ph7_class_instance *));` |
|         - | 2695 | `PH7_PRIVATE int PH7_ClassNativeCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,sxi32 *pResult);` |
|         - | 2696 | `PH7_PRIVATE int PH7_ClassNativeCmpValue(ph7_class_instance *pLeft,ph7_value *pOther,` |
|         - | 2697 | `	int bReversed,sxi32 *pResult);` |
|         - | 2698 | `PH7_PRIVATE void PH7_NativeCmpOpaqueHandle(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeCmpCtx *pCtx);` |
|         - | 2699 | `PH7_PRIVATE int PH7_ClassCastsToHandleId(ph7_class *pClass);` |
|         - | 2700 | `PH7_PRIVATE int PH7_ClassNumberIsString(ph7_class *pClass);` |
|         - | 2701 | `PH7_PRIVATE int PH7_ClassVarsFromPresent(ph7_class *pClass);` |
|         - | 2702 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallCmpHook(ph7_vm *pVm,const char *zClass,` |
|         - | 2703 | `	void (*xCmp)(ph7_vm *,ph7_class_instance *,PH7_NativeCmpCtx *));` |
|         - | 2704 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallArithHook(ph7_vm *pVm,const char *zClass,` |
|         - | 2705 | `	void (*xArith)(ph7_vm *,ph7_class_instance *,PH7_NativeArithCtx *));` |
|         - | 2706 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallBoolHook(ph7_vm *pVm,const char *zClass,` |
|         - | 2707 | `	int (*xBool)(ph7_vm *,ph7_class_instance *));` |
|         - | 2708 | `PH7_PRIVATE int PH7_ClassNativeBool(ph7_class_instance *pThis,int *pOut);` |
|         - | 2709 | `/*` |
|         - | 2710 | ` * What VmArithOperandStep() decided about one operator's pair.` |
|         - | 2711 | ` */` |
|         - | 2712 | `#define PH7_ARITH_ORDINARY  0   /* no handler: run the numeric arithmetic */` |
|         - | 2713 | `#define PH7_ARITH_HANDLED   1   /* a handler answered; the destination already holds it */` |
|         - | 2714 | `#define PH7_ARITH_REFUSED  (-1) /* throw *pzClass with the message in pMsgOut */` |
|         - | 2715 | `PH7_PRIVATE int VmArithOperandStep(ph7_vm *pVm,ph7_value *pLeft,ph7_value *pRight,` |
|         - | 2716 | `	const char *zOp,ph7_value *pDest,const char **pzClass,SyBlob *pMsgOut);` |
|         - | 2717 | `PH7_PRIVATE int PH7_ValueHasArithHandler(ph7_value *pVal);` |
|         - | 2718 | `PH7_PRIVATE sxi32 PH7_NativeClassMarkVirtualProps(ph7_vm *pVm,const char *zClass);` |
|         - | 2719 | `PH7_PRIVATE sxi32 PH7_NativeClassMarkLazyProps(ph7_vm *pVm,const char *zClass,int bDefaultRead);` |
|         - | 2720 | `PH7_PRIVATE sxi32 PH7_NativeClassMarkNoWriteProps(ph7_vm *pVm,const char *zClass);` |
|         - | 2721 | `PH7_PRIVATE void PH7_NativeMaterializeLazy(ph7_vm *pVm,ph7_class_instance *pObj);` |
|         - | 2722 | `PH7_PRIVATE void PH7_NativeHideAttr(ph7_class_instance *pObj,const char *zName);` |
|         - | 2723 | `/*` |
|         - | 2724 | ` * The refusal a native compare handler carried back (ph7_vm::zCmpRefusalClass):` |
|         - | 2725 | ` * pending? raise it here, where a throw can be routed; raise it on a host CALL` |
|         - | 2726 | ` * CONTEXT, so a builtin that compared reports it the way its own throws are` |
|         - | 2727 | ` * reported; or drop it, for the two comparison doors that are not PHP execution` |
|         - | 2728 | ` * at all (the public ph7_value_compare, a VM reset).` |
|         - | 2729 | ` */` |
|         - | 2730 | `PH7_PRIVATE int PH7_CmpRefusalPending(ph7_vm *pVm);` |
|         - | 2731 | `PH7_PRIVATE sxi32 PH7_CmpRefusalRaise(ph7_vm *pVm);` |
|         - | 2732 | `PH7_PRIVATE sxi32 PH7_CmpRefusalRaiseCtx(ph7_context *pCtx);` |
|         - | 2733 | `PH7_PRIVATE void PH7_CmpRefusalClear(ph7_vm *pVm);` |
|         - | 2734 | `PH7_PRIVATE sxi32 PH7_InstallEnumInterfaceMethods(ph7_vm *pVm,ph7_class *pClass);` |
|         - | 2735 | `PH7_PRIVATE sxi32 PH7_InstallNativeEnum(ph7_vm *pVm,const char *zName,sxu32 nBacking,` |
|         - | 2736 | `	const PH7_NativeEnumCase *aCase,sxu32 nCase,` |
|         - | 2737 | `	const PH7_NativeMethodDef *aMethod,sxu32 nMethod);` |
|         - | 2738 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallMethod(ph7_vm *pVm,ph7_class *pClass,` |
|         - | 2739 | `	const PH7_NativeMethodDef *pDef,void *pUserData);` |
|         - | 2740 | `/*` |
|         - | 2741 | `` * Attach one `#[Name(literal, ...)]` to a class declared from C. php puts an`` |
|         - | 2742 | ` * attribute on two of its own attribute classes, and the whole record — the FQN` |
|         - | 2743 | ` * plus its arguments — is what the engine reads to VALIDATE a target and what` |
|         - | 2744 | ` * ReflectionAttribute answers.` |
|         - | 2745 | ` */` |
|         - | 2746 | `typedef struct PH7_NativeAttrArg PH7_NativeAttrArg;` |
|         - | 2747 | `struct PH7_NativeAttrArg` |
|         - | 2748 | `{` |
|         - | 2749 | `	const char *zName;           /* named argument, or 0 for a positional one */` |
|         - | 2750 | `	PH7_NativeConstDef sValue;   /* the literal; zName/iMods unused */` |
|         - | 2751 | `};` |
|         - | 2752 | `PH7_PRIVATE sxi32 PH7_NativeClassAddAttribute(ph7_vm *pVm,ph7_class *pClass,` |
|         - | 2753 | `	const char *zAttr,const PH7_NativeAttrArg *aArg,sxu32 nArg);` |
|         - | 2754 | `PH7_PRIVATE sxi32 PH7_NativeMethodSetNoDiscard(ph7_vm *pVm,ph7_class *pClass,` |
|         - | 2755 | `	const char *zMethod,const PH7_NativeAttrArg *aArg,sxu32 nArg);` |
|         - | 2756 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallProperty(ph7_vm *pVm,ph7_class *pClass,` |
|         - | 2757 | `	const PH7_NativePropDef *pDef);` |
|         - | 2758 | `PH7_PRIVATE void PH7_NativeLiteralValue(ph7_vm *pVm,const void *pLiteral,ph7_value *pOut);` |
|         - | 2759 | `PH7_PRIVATE void PH7_NativeSetProp(ph7_vm *pVm,ph7_class_instance *pObj,` |
|         - | 2760 | `	const char *zProp,sxu32 nProp,ph7_value *pSrcVal);` |
|         - | 2761 | `/*` |
|         - | 2762 | ` * Reading and writing a native instance's own declared slots. Every native class` |
|         - | 2763 | ` * does this constantly (the date family had a private copy of the whole set), so` |
|         - | 2764 | ` * the accessors live with the builder that declares the slots.` |
|         - | 2765 | ` */` |
|         - | 2766 | `PH7_PRIVATE ph7_value * PH7_NativeAttr(ph7_class_instance *pObj,const char *zName);` |
|         - | 2767 | `PH7_PRIVATE int PH7_NativeAttrIsUninit(ph7_class_instance *pObj,const char *zName);` |
|         - | 2768 | `PH7_PRIVATE sxi64 PH7_NativeAttrInt(ph7_class_instance *pObj,const char *zName);` |
|         - | 2769 | `PH7_PRIVATE ph7_class_instance * PH7_NativeAttrObj(ph7_class_instance *pObj,const char *zName);` |
|         - | 2770 | `PH7_PRIVATE int PH7_NativeAttrTruthy(ph7_class_instance *pObj,const char *zName);` |
|         - | 2771 | `PH7_PRIVATE void PH7_NativeAttrStr(ph7_class_instance *pObj,const char *zName,` |
|         - | 2772 | `	const char **pzOut,int *pnOut);` |
|         - | 2773 | `PH7_PRIVATE void PH7_NativeSetAttrInt(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,sxi64 iVal);` |
|         - | 2774 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         - | 2775 | `PH7_PRIVATE void PH7_NativeSetAttrReal(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,ph7_real rVal);` |
|         - | 2776 | `#endif` |
|         - | 2777 | `PH7_PRIVATE void PH7_NativeSetAttrStr(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,` |
|         - | 2778 | `	const char *zVal,int nVal);` |
|         - | 2779 | `PH7_PRIVATE void PH7_NativeSetAttrBool(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,int bVal);` |
|         - | 2780 | `PH7_PRIVATE void PH7_NativeSetAttrObj(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,` |
|         - | 2781 | `	ph7_class_instance *pVal);` |
|         - | 2782 | `PH7_PRIVATE void PH7_NativeResultObject(ph7_context *pCtx,ph7_class_instance *pObj);` |
|         - | 2783 | `/*` |
|         - | 2784 | ` * php's InternalIterator: the Iterator a native IteratorAggregate answers when the` |
|         - | 2785 | ` * PHP it replaced was a GENERATOR -- the one body a C method cannot be. It is one` |
|         - | 2786 | ` * class in php too, wrapping whatever internal iterator the aggregate handed over,` |
|         - | 2787 | ` * so PHL gives it the same shape: fixed state slots on the iterator, and a vtable` |
|         - | 2788 | ` * on the AGGREGATE'S CLASS (ph7_class::pIterVtab, php's get_iterator handler) that` |
|         - | 2789 | ` * knows how to position and advance that aggregate's cursor. current()/key()/valid()` |
|         - | 2790 | ` * need no vtable entry -- they read the slots the two below leave behind.` |
|         - | 2791 | ` */` |
|         - | 2792 | `struct PH7_NativeIterVtab` |
|         - | 2793 | `{` |
|         - | 2794 | `	void (*xRewind)(ph7_vm *pVm,ph7_class_instance *pIt); /* settle on the first element */` |
|         - | 2795 | `	void (*xNext)(ph7_vm *pVm,ph7_class_instance *pIt);   /* settle on the one after */` |
|         - | 2796 | `	/* Optional: publish the cursor back onto the AGGREGATE, for a class that shows` |
|         - | 2797 | `	 * its walk as one of its own properties. DatePeriod is the case -- its` |
|         - | 2798 | ``	 * `current` is the cursor, and php writes it from every iterator method rather`` |
|         - | 2799 | ``	 * than from the walk itself, so `getIterator()` alone leaves it where the last`` |
|         - | 2800 | `	 * walk left it and the first valid()/current()/key()/rewind()/next() moves it.` |
|         - | 2801 | `	 * Called by the InternalIterator methods, never by the vtable's own halves. */` |
|         - | 2802 | `	void (*xPublish)(ph7_vm *pVm,ph7_class_instance *pIt);` |
|         - | 2803 | `	/* Optional: may this iterator be WALKED at all? Answered per call rather than` |
|         - | 2804 | `	 * once at creation because php refuses at the walk and not at the door --` |
|         - | 2805 | `	 * DatePeriod::getIterator() on an object nobody constructed hands back a real` |
|         - | 2806 | `	 * InternalIterator there, and the DateObjectError arrives at the first` |
|         - | 2807 | `	 * rewind(). Non-zero means the guard raised; the method then answers nothing` |
|         - | 2808 | `	 * and the host-call boundary reports the throw. */` |
|         - | 2809 | `	int (*xGuard)(ph7_context *pCtx,ph7_class_instance *pIt);` |
|         - | 2810 | `};` |
|         - | 2811 | `/* The state slots, private to InternalIterator and shared by every vtable:` |
|         - | 2812 | ` * the aggregate, the value and key at the cursor, an integer cursor and a spare` |
|         - | 2813 | ` * one for the vtable's own bookkeeping, and whether the walk is over. */` |
|         - | 2814 | `#define PH7_NATIVE_IT_SRC  "__src"` |
|         - | 2815 | `#define PH7_NATIVE_IT_CUR  "__cur"` |
|         - | 2816 | `#define PH7_NATIVE_IT_KEY  "__key"` |
|         - | 2817 | `#define PH7_NATIVE_IT_POS  "__pos"` |
|         - | 2818 | `#define PH7_NATIVE_IT_AUX  "__aux"` |
|         - | 2819 | `#define PH7_NATIVE_IT_DONE "__done"` |
|         - | 2820 | `PH7_PRIVATE sxi32 PH7_VmInstallNativeIterator(ph7_vm *pVm);` |
|         - | 2821 | `PH7_PRIVATE ph7_class_instance * PH7_NativeIteratorNew(ph7_vm *pVm,ph7_class_instance *pSrc);` |
|         - | 2822 | `/*` |
|         - | 2823 | ` * Each class method is parsed out and stored in an instance of the following` |
|         - | 2824 | ` * structure.` |
|         - | 2825 | ` * PH7 introduced some powerfull extensions to the PHP 5 programming` |
|         - | 2826 | ` * language like function overloading,type hinting,complex default` |
|         - | 2827 | ` * arguments and many more.` |
|         - | 2828 | ` * Please refer to the official documentation for more information.` |
|         - | 2829 | ` */` |
|         - | 2830 | `struct ph7_class_method` |
|         - | 2831 | `{` |
|         - | 2832 | `	ph7_vm_func sFunc;   /* Compiled method body */` |
|         - | 2833 | `	SyString sVmName;    /* Automatically generated name assigned to this method.` |
|         - | 2834 | `						  * Typically this is "[class_name__method_name@random_string]"` |
|         - | 2835 | `						  */` |
|         - | 2836 | `	sxi32 iProtection;   /* Protection level [i.e: public,private,protected] */` |
|         - | 2837 | `	sxi32 iFlags;        /* Methods configuration */` |
|         - | 2838 | `	sxi32 iCloneDepth;   /* Clone depth [Only used by the magic method __clone ] */` |
|         - | 2839 | `    sxu32 nLine;         /* Line on which this method was defined */` |
|         - | 2840 | `};` |
|         - | 2841 | `/*` |
|         - | 2842 | ` * Each active object (class instance) is represented by an instance of` |
|         - | 2843 | ` * the following structure.` |
|         - | 2844 | ` */` |
|         - | 2845 | `struct ph7_class_instance` |
|         - | 2846 | `{` |
|         - | 2847 | `	ph7_vm *pVm;        /* VM that own this instance */` |
|         - | 2848 | `	ph7_class *pClass;  /* Object is an instance of this class */` |
|         - | 2849 | `	SyHash hAttr;       /* Hashtable of active class members */` |
|         - | 2850 | `	sxi32 iRef;         /* Reference count */` |
|         - | 2851 | `	sxi32 iFlags;       /* Control flags */` |
|         - | 2852 | `	sxu32 nObjId;       /* Per-instance monotonic handle id (from pVm->nNextObjId,` |
|         - | 2853 | `	                     * never reused). Drives spl_object_id/hash + var_dump #N. */` |
|         - | 2854 | `	sxu32 nGcRoot;      /* 1-based row in the collector's root buffer, 0 while unbuffered */` |
|         - | 2855 | `	sxu8 iGcColor;      /* PH7_GC_* -- see vm_gc.c */` |
|         - | 2856 | `	PH7_AttrIter *pActiveIters; /* Walks of hAttr currently in flight over this object` |
|         - | 2857 | `	                     * (foreach, array_walk). A property removed under one of them` |
|         - | 2858 | `	                     * advances its cursor; one appended re-arms an exhausted one. */` |
|         - | 2859 | `};` |
|         - | 2860 | `/*` |
|         - | 2861 | ` * ph7_class_instance::iFlags bit set while the object's __clone() magic method` |
|         - | 2862 | ` * runs. PHP 8.3 lets __clone() re-initialize the cloned object's readonly` |
|         - | 2863 | ` * properties, so the readonly store guard consults this flag on the executing` |
|         - | 2864 | ` * $this. (Other iFlags bits are declared privately in their owning .c file:` |
|         - | 2865 | ` * 0x001 destroyed, 0x002 dumping, 0x004 fcc-bound.)` |
|         - | 2866 | ` */` |
|         - | 2867 | `#define CLASS_INSTANCE_DESTROYED 0x001 /* Instance is released (oo.c's teardown latch;` |
|         - | 2868 | `                                        * read by the cycle collector, which must not` |
|         - | 2869 | `                                        * walk a table being torn down) */` |
|         - | 2870 | `#define VM_INSTANCE_CLONING 0x008` |
|         - | 2871 | `/*` |
|         - | 2872 | ` * ph7_class_instance::iFlags bit: this object's __destruct has already been reached for` |
|         - | 2873 | ` * (or the refusal that stands in for it raised), so no later teardown may run it a second` |
|         - | 2874 | ` * time. php keeps the same bit (IS_OBJ_DESTRUCTOR_CALLED) for the same reason -- its` |
|         - | 2875 | ` * shutdown pass calls destructors on objects it does NOT free, and the free that follows` |
|         - | 2876 | ` * must not repeat them. Distinct from CLASS_INSTANCE_DESTROYED 0x001 (oo.c), which says` |
|         - | 2877 | ` * the whole instance is gone. 0x080 because 0x002..0x040 are claimed by unrelated readers` |
|         - | 2878 | ` * of this same word, each on its own kind of object.` |
|         - | 2879 | ` */` |
|         - | 2880 | `#define CLASS_INSTANCE_DTOR_CALLED 0x080` |
|         - | 2881 | `/*` |
|         - | 2882 | ` * ph7_class_instance::iFlags bit set once this object's LAZY native properties` |
|         - | 2883 | ` * (PH7_CLASS_ATTR_NATIVE_LAZY) have been installed. It is the difference between` |
|         - | 2884 | ` * "the constructor has never run, so the name is not a property of this object at` |
|         - | 2885 | ` * all" and "the table exists and this one name was unset()" -- the first is php's` |
|         - | 2886 | ` * dynamic-property creation on a write and an undefined-property warning on a` |
|         - | 2887 | ` * read, the second re-creates the declared slot the ordinary way.` |
|         - | 2888 | ` */` |
|         - | 2889 | `#define VM_INSTANCE_LAZY_DONE 0x010` |
|         - | 2890 | `/*` |
|         - | 2891 | ` * Is this instance slot kept OUT of every surface that shows the object? Two` |
|         - | 2892 | ` * unrelated reasons say yes: the class calls it an engine slot` |
|         - | 2893 | ` * (PH7_CLASS_ATTR_HIDDEN) or this one OBJECT hides it (VM_CLASS_ATTR_UNSEEN).` |
|         - | 2894 | ` * Class-level members are not the object's either, so the one test covers all` |
|         - | 2895 | ` * three.` |
|         - | 2896 | ` */` |
|         - | 2897 | `#define PH7_ATTR_UNPRESENTED(pVmAttr) \` |
|         - | 2898 | `	(((pVmAttr)->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT \` |
|         - | 2899 | `	                              \|PH7_CLASS_ATTR_HIDDEN)) != 0 \` |
|         - | 2900 | `	 \|\| ((pVmAttr)->iState & VM_CLASS_ATTR_UNSEEN) != 0)` |
|         - | 2901 | `/*` |
|         - | 2902 | ` * Is this DECLARED attribute absent from the object because its class declares it` |
|         - | 2903 | ` * LAZILY and nothing has installed the set yet? The two miss paths -- a property` |
|         - | 2904 | ` * write and a by-reference bind -- ask before they re-create a declared slot.` |
|         - | 2905 | ` */` |
|         - | 2906 | `#define PH7_ATTR_LAZY_ABSENT(pAttr,pInst) \` |
|         - | 2907 | `	((((pAttr)->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY) != 0) \` |
|         - | 2908 | `	 && (((pInst)->iFlags & VM_INSTANCE_LAZY_DONE) == 0))` |
|         - | 2909 | `/*` |
|         - | 2910 | ` * A single instruction of the virtual machine has an opcode` |
|         - | 2911 | ` * and as many as three operands.` |
|         - | 2912 | ` * Each VM instruction resulting from compiling a PHP script` |
|         - | 2913 | ` * is stored in an instance of the following structure.` |
|         - | 2914 | ` */` |
|         - | 2915 | `struct VmInstr` |
|         - | 2916 | `{` |
|         - | 2917 | `	sxu8  iOp; /* Operation to preform */` |
|         - | 2918 | `	sxu8  bStrict; /* strict_types mode of the COMPILATION UNIT this instruction came from.` |
|         - | 2919 | `	                * Stamped by PH7_VmEmitInstr beside nLine, and published to` |
|         - | 2920 | `	                * pVm->bCurStrict under the same nLine != 0 gate, so an engine-dispatched` |
|         - | 2921 | `	                * call (a magic method, a property hook) can bind its arguments under the` |
|         - | 2922 | `	                * CALLING file's mode — php's rule — instead of always coercing. Sits in` |
|         - | 2923 | `	                * the padding after iOp: sizeof(VmInstr) is unchanged. */` |
|         - | 2924 | `	sxu8  bDiscard; /* PH7_OP_CALL only: php's !RETURN_VALUE_USED. The statement that owns` |
|         - | 2925 | ``	                * this call throws its answer away (`f();`, not `$x = f();` and not`` |
|         - | 2926 | ``	                * `f() + 1;`), which is the one thing a #[\NoDiscard] callee warns`` |
|         - | 2927 | `	                * about. Set by the codegen at the statement-discard site and cleared` |
|         - | 2928 | ``	                * by a `(void)` cast in front of it, which is php's way of saying the`` |
|         - | 2929 | `	                * drop is deliberate. Padding after bStrict, like bStrict itself. */` |
|         - | 2930 | `	sxu8  bRefSrc; /* PH7_OP_MEMBER only: this property fetch is a reference SOURCE --` |
|         - | 2931 | ``	                * php's `zend_compile_var(source, BP_VAR_W)`, the fetch a `=&` bind, a`` |
|         - | 2932 | ``	                * `[&$o->p]` element and a by-reference `foreach` make. It stays a`` |
|         - | 2933 | `	                * PH7_MEMBER_READ (a handler-backed property still hands back a COPY),` |
|         - | 2934 | `	                * but a MISSING name is CREATED rather than warned about, exactly as a` |
|         - | 2935 | `	                * write would create it. Padding after bDiscard, like bStrict itself. */` |
|         - | 2936 | `	sxi32 iP1; /* First operand */` |
|         - | 2937 | `	sxu32 iP2; /* Second operand (Often the jump destination) */` |
|         - | 2938 | `	sxu32 nAux; /* A per-instruction scratch word the RUNTIME owns, zero until it writes` |
|         - | 2939 | `	             * one. Two opcodes use it, each for an answer that cannot change under it:` |
|         - | 2940 | `	             *` |
|         - | 2941 | `	             *   PH7_OP_LOAD      the length of the variable NAME in p3. The name is a` |
|         - | 2942 | `	             *   PH7_OP_STORE     NUL-terminated compile-time buffer, and measuring it` |
|         - | 2943 | `	             *                    again on every execution was ~2% of a phpcs run.` |
|         - | 2944 | `	             *                    Written by VmNumberLocals for a body it walks, and` |
|         - | 2945 | `	             *                    lazily on first execution for one it does not.` |
|         - | 2946 | `	             *   PH7_OP_CALL_INIT the pVm->nCallableGen this call site was last screened` |
|         - | 2947 | `	             *                    at, written only when the callee is a compile-time` |
|         - | 2948 | `	             *                    constant (the push behind it is an OP_LOADC).` |
|         - | 2949 | `	             *   PH7_OP_LOADC     how many times this site has run, capped at two -- a` |
|         - | 2950 | `	             *                    site that runs once can never repay a cache record.` |
|         - | 2951 | `	             *   PH7_OP_CALL      the same count, for the same reason (VmCallSiteFor).` |
|         - | 2952 | `	             *` |
|         - | 2953 | `	             * Lives in the padding after iP2, so VmInstr is still 32 bytes and the` |
|         - | 2954 | `	             * bytecode costs nothing extra. */` |
|         - | 2955 | `	void *p3;  /* Third operand (Often Upper layer private data) */` |
|         - | 2956 | `	sxu32 nLine; /* Source line this instruction was compiled from (0 = unknown).` |
|         - | 2957 | `	              * Stamped by PH7_VmEmitInstr from the codegen's current token, so` |
|         - | 2958 | `	              * every one of its ~150 call sites keeps its signature. */` |
|         - | 2959 | `	sxu32 nSite; /* Two opcodes' worth of "this SITE already knows the answer", sharing one` |
|         - | 2960 | `	              * word because no instruction is ever both.` |
|         - | 2961 | `	              *` |
|         - | 2962 | `	              *   PH7_OP_CALL      this site's entry in pVm->aCallSite, plus one (0 = it` |
|         - | 2963 | `	              *   PH7_OP_LOADC     has never asked for one). A CALL remembers which` |
|         - | 2964 | `	              *                    function table entry its callee NAME resolved to; a` |
|         - | 2965 | `	              *                    LOADC which hConstant entry its constant name did.` |
|         - | 2966 | `	              *                    Both are a name hashed once instead of once per` |
|         - | 2967 | `	              *                    execution -- see VmCallSite.` |
|         - | 2968 | `	              *   PH7_OP_LOAD      the NUMBER this body gave the variable in p3, plus` |
|         - | 2969 | `	              *   PH7_OP_STORE     one (0 = the body was never numbered, or the name` |
|         - | 2970 | `	              *                    did not fit PH7_VAR_SLOT_MAX). It indexes the running` |
|         - | 2971 | `	              *                    frame's aLocalSlot -- see VmNumberLocals.` |
|         - | 2972 | `	              *` |
|         - | 2973 | `	              * Lives in the trailing padding after nLine, so VmInstr is still 32 bytes. */` |
|         - | 2974 | `};` |
|         - | 2975 | `/*` |
|         - | 2976 | ` * Named-argument metadata attached to PH7_OP_CALL instructions via p3.` |
|         - | 2977 | ` * Also carries the namespace-qualification flag formerly stored as p3=(void*)1.` |
|         - | 2978 | ` */` |
|         - | 2979 | `struct VmCallArgMap` |
|         - | 2980 | `{` |
|         - | 2981 | `	sxu8 bHasNamed;      /* 1 if any argument uses name: syntax */` |
|         - | 2982 | `	sxu8 bFromUnpack;    /* 1 when this is the EFFECTIVE map an argument UNPACK` |
|         - | 2983 | `	                      * produced (VmBuildEffectiveArgMap). php words its` |
|         - | 2984 | `	                      * positional-after-named refusal with a trailing` |
|         - | 2985 | ``	                      * ` during unpacking` only there; the same rule broken by`` |
|         - | 2986 | `	                      * call_user_func_array's array gets the bare sentence. */` |
|         - | 2987 | `	sxu8 bIsNamespaced;  /* 1 if compiler namespace-qualified the call */` |
|         - | 2988 | `	sxu8 bStrict;        /* 1 if the call site's file declared strict_types=1 */` |
|         - | 2989 | `	sxu32 nOrigNameLit;  /* Original (unqualified) name-literal index + 1, stored` |
|         - | 2990 | `						  * when the CALL handler namespace-qualified the name so` |
|         - | 2991 | `						  * a following NEW can re-qualify with CLASS imports.` |
|         - | 2992 | `						  * 0 = unset. (Formerly abused OP_CALL's iP2, colliding` |
|         - | 2993 | ``						  * with the hasSpread flag: `new N\C(...$args)`.) */`` |
|         - | 2994 | `	sxu32 nNewClassInstr;/* Instruction index + 1 of the class-name push, for a call node` |
|         - | 2995 | ``	                      * that is a `new`'s operand. The reorder puts that push before`` |
|         - | 2996 | `	                      * the constructor arguments, so the NEW codegen can no longer` |
|         - | 2997 | `	                      * find it by peeking one instruction back. 0 = unset. */` |
|         - | 2998 | `	sxu8 bArgShapes;     /* 1 when the two masks below describe THIS call's argument` |
|         - | 2999 | `						  * positions. The compiler sets it for every call whose actual` |
|         - | 3000 | `						  * positions survive to the runtime stack unchanged — i.e. no` |
|         - | 3001 | `						  * spread and at most 31 arguments. 0 means "unknown shapes":` |
|         - | 3002 | `						  * the by-ref binders fall back to their runtime nIdx test. */` |
|         - | 3003 | `	sxu32 nNonLvalMask;  /* bit N: argument N is a HARD non-lvalue (a literal, an operator` |
|         - | 3004 | ``						  * result, a cast, a class constant, `@$x`, `$o?->p`, an assignment`` |
|         - | 3005 | ``						  * — php's `zend_is_variable` says no and it is not a call either).`` |
|         - | 3006 | `						  * Binding one to a by-ref parameter is php's catchable` |
|         - | 3007 | ``						  * `Argument #N ($p) could not be passed by reference` Error, raised`` |
|         - | 3008 | `						  * at the CALL before the callee's ZPP runs. */` |
|         - | 3009 | ``	sxu32 nTempCallMask; /* bit N: argument N is the RESULT of a call or a `new` — php's`` |
|         - | 3010 | `						  * SEND_VAR_NO_REF: an E_NOTICE ("Only variables should be passed` |
|         - | 3011 | `						  * by reference") and then it operates on the temporary. */` |
|         - | 3012 | `	sxu32 nTotal;        /* Total number of compile-time arguments */` |
|         - | 3013 | `	SyString *aNames;    /* Array of nTotal names. nByte==0 means positional. */` |
|         - | 3014 | `	SyString sAssertSrc; /* Direct assert() calls only: the first argument's rendered` |
|         - | 3015 | ``						  * source text (php's zend_ast_export shape, e.g. `1 == 2`),`` |
|         - | 3016 | `						  * captured at compile time so a failing assertion reports` |
|         - | 3017 | ``						  * `assert(1 == 2)` like php instead of the evaluated value.`` |
|         - | 3018 | `						  * {0,0} for every other call site; bytes live in the VM` |
|         - | 3019 | `						  * allocator. See PH7_GenRenderAssertSpan (compile_literal.c). */` |
|         - | 3020 | `};` |
|         - | 3021 | `/*` |
|         - | 3022 | ` * A class declaration whose parent/interface/trait could not be resolved at` |
|         - | 3023 | ` * compile time (the enclosing file's statements — spl_autoload_register — had` |
|         - | 3024 | ` * not executed yet). The compiler captures the declaration's SOURCE plus a` |
|         - | 3025 | ` * reconstructed namespace/use-import prefix and defers the whole compile to` |
|         - | 3026 | ` * OP_CLASS_DEFER at the declaration's execution point (VmExecDeferredClass,` |
|         - | 3027 | ` * vm_include.c), where the autoloader is live. aRequired lists the names that` |
|         - | 3028 | ` * were missing; each still-missing one throws php's catchable` |
|         - | 3029 | `` * `Class/Interface/Trait "X" not found` Error before the re-compile runs.`` |
|         - | 3030 | ` */` |
|         - | 3031 | `typedef struct VmDeferredReq VmDeferredReq;` |
|         - | 3032 | `struct VmDeferredReq` |
|         - | 3033 | `{` |
|         - | 3034 | `	SyString sName;  /* Fully-qualified name (allocator-owned) */` |
|         - | 3035 | `	sxu8 cKind;      /* PH7_DEFER_KIND_* — picks the not-found noun */` |
|         - | 3036 | `};` |
|         - | 3037 | `#define PH7_DEFER_KIND_CLASS     0` |
|         - | 3038 | `#define PH7_DEFER_KIND_INTERFACE 1` |
|         - | 3039 | `#define PH7_DEFER_KIND_TRAIT     2` |
|         - | 3040 | `typedef struct VmDeferredClass VmDeferredClass;` |
|         - | 3041 | `struct VmDeferredClass` |
|         - | 3042 | `{` |
|         - | 3043 | `	SyString sText;     /* Re-compilable chunk: namespace + use-imports prefix +` |
|         - | 3044 | ``						 * the declaration source (anon: wrapped in `if (false) { new ... }`) */`` |
|         - | 3045 | `	SyString sSelfName; /* FQN the compile must install (anon: the synthesized name) —` |
|         - | 3046 | `						 * the post-eval existence check */` |
|         - | 3047 | `	SyString sAnonName; /* Synthesized anonymous-class name to inject via` |
|         - | 3048 | `						 * pVm->sDeferAnonName ({0,0} for a named declaration) */` |
|         - | 3049 | `	SySet aRequired;    /* VmDeferredReq — names unresolved at compile time */` |
|         - | 3050 | `	sxu32 nLine;        /* Declaration line (diagnostics) */` |
|         - | 3051 | `	sxu8 bDone;         /* 1 once the declaration executed successfully (idempotent site) */` |
|         - | 3052 | `};` |
|         - | 3053 | `/* Each active class instance attribute is represented by an instance` |
|         - | 3054 | ` * of the following structure.` |
|         - | 3055 | ` */` |
|         - | 3056 | `typedef struct VmClassAttr VmClassAttr;` |
|         - | 3057 | `struct VmClassAttr` |
|         - | 3058 | `{` |
|         - | 3059 | `	ph7_class_attr *pAttr; /* Class attribute */` |
|         - | 3060 | `	sxu32 nIdx;            /* Memory object index */` |
|         - | 3061 | `	sxi32 iState;          /* Per-instance state: VM_CLASS_ATTR_UNINIT */` |
|         - | 3062 | `	ph7_class *pOwner;     /* Class that declares this attribute (for error msgs) */` |
|         - | 3063 | `	ph7_class_instance *pInst; /* Instance this slot belongs to, or 0 for a class STATIC.` |
|         - | 3064 | `	                       * The store filter reaches it for ph7_class::xSet, which is a` |
|         - | 3065 | `	                       * handler ON AN OBJECT (php's write_property takes the object);` |
|         - | 3066 | `	                       * DateInterval's writes its own microsecond slot from there.` |
|         - | 3067 | `	                       * The record lives in the instance's own hAttr and dies with it,` |
|         - | 3068 | `	                       * so the pointer never outlives what it names. */` |
|         - | 3069 | `};` |
|         - | 3070 | `#define VM_CLASS_ATTR_UNINIT  0x01 /* Typed property never written (PHP 7.4+); also the` |
|         - | 3071 | `                                    * write-once latch for readonly properties (cleared on` |
|         - | 3072 | `                                    * the first successful write — see VmEnforcePropertyTypeOnStore) */` |
|         - | 3073 | `#define VM_CLASS_ATTR_RDONLY  0x08 /* php's read-only handler property, marked on the INSTANCE:` |
|         - | 3074 | ``                                    * a plain store and an unset() refuse with `Property p is`` |
|         - | 3075 | ``                                    * read only` while every path that takes a POINTER to it`` |
|         - | 3076 | ``                                    * goes through (a compound assign, `++`, `??=`, a`` |
|         - | 3077 | `                                    * reference bind). It is per-OBJECT rather than per-class` |
|         - | 3078 | `                                    * because php's own handler is: a PDOStatement nobody` |
|         - | 3079 | `                                    * built a cursor for takes the write, and only one` |
|         - | 3080 | `                                    * carrying a statement refuses. */` |
|         - | 3081 | `#define VM_CLASS_ATTR_UNSEEN  0x10 /* Per-INSTANCE presentation hide: the slot reads, writes and` |
|         - | 3082 | `                                    * answers isset() the way it always did, and every surface` |
|         - | 3083 | `                                    * that SHOWS an object -- var_dump/print_r/var_export, the` |
|         - | 3084 | `                                    * (array) cast, get_object_vars, foreach, json_encode,` |
|         - | 3085 | `                                    * serialize, http_build_query and Reflection's object dump` |
|         - | 3086 | `                                    * -- walks past it. php's from-string DateInterval is the` |
|         - | 3087 | ``                                    * case: it answers `$i->d` from the string it kept while`` |
|         - | 3088 | ``                                    * presenting `from_string` and `date_string` alone. */`` |
|         - | 3089 | `#define VM_CLASS_ATTR_TYPE_DEFER 0x04 /* Typed STATIC property whose eagerly-evaluated DEFAULT failed` |
|         - | 3090 | `                                       * its type check at class mount. php evaluates static defaults` |
|         - | 3091 | `                                       * lazily, so the failure is deferred: any static-property access` |
|         - | 3092 | `                                       * on the class (read/write/isset, any property) and any` |
|         - | 3093 | `                                       * instantiation throws the catchable "Cannot assign <kind> to` |
|         - | 3094 | `                                       * property C::$s of type T" TypeError; a never-touched class` |
|         - | 3095 | `                                       * stays silent. Raised by PH7_VmMaterializeClassStatics, which` |
|         - | 3096 | `                                       * also sets the flag when a DEFERRED default (the sibling` |
|         - | 3097 | `                                       * PH7_CLASS_ATTR_STATIC_DEFER) evaluates at first access and` |
|         - | 3098 | `                                       * only then fails its type check. */` |
|         - | 3099 | ``#define VM_CLASS_ATTR_REFBOUND 0x02 /* Property is bound to a reference (`$o->p =& $x`): its nIdx`` |
|         - | 3100 | `                                    * slot is SHARED with (and pinned by) the source variable, so` |
|         - | 3101 | `                                    * PH7_VmReleaseInstanceAttr must NOT release/recycle it — the` |
|         - | 3102 | `                                    * surviving alias would dangle. Mirrors the use(&$x) pin. */` |
|         - | 3103 | ``#define VM_CLASS_ATTR_REFSRCPIN 0x20 /* Property is the SOURCE of a reference (`$r =& $o->p`,`` |
|         - | 3104 | ``                                     * `$q->p =& $o->p`, `foreach ($o->p as &$v)`): php makes both`` |
|         - | 3105 | `                                     * ends references, and the other end pins the slot. A property` |
|         - | 3106 | `                                     * is not a holder the reference table can NAME, so the slot's` |
|         - | 3107 | `                                     * only recorded holder was that pin -- and when the other end` |
|         - | 3108 | `                                     * died, the unpin freed the value out from under THIS property,` |
|         - | 3109 | `                                     * which then read NULL. The bit says this property holds one` |
|         - | 3110 | `                                     * counted pin of its own, given back when it is released. */` |
|         - | 3111 | ` /* Forward reference */` |
|         - | 3112 | `typedef struct VmSlot VmSlot;` |
|         - | 3113 | `struct VmSlot` |
|         - | 3114 | `{` |
|         - | 3115 | `	sxu32 nIdx;      /* Index in pVm->aMemObj[] */` |
|         - | 3116 | `	void *pUserData; /* Upper-layer private data */` |
|         - | 3117 | `};` |
|         - | 3118 | `/*` |
|         - | 3119 | ` * The segmented memory-object table (PERF.md P1).` |
|         - | 3120 | ` *` |
|         - | 3121 | ` * aMemObj used to be one doubling SySet: every value pointer died on any growth,` |
|         - | 3122 | ` * the doubling realloc moved a whole 33 MB block, and the buffer never shrank.` |
|         - | 3123 | ` * Here the value slots live in FIXED-SIZE segments (VM_MEMPOOL_SEG_SLOTS each,` |
|         - | 3124 | ` * one pool allocation per segment), addressed by the same flat nIdx the engine` |
|         - | 3125 | ` * has always carried -- the split is a shift and a mask here, in the accessor,` |
|         - | 3126 | ` * so every caller's index means what it always meant. A value's address never` |
|         - | 3127 | ` * moves, growth appends a segment instead of copying the table, and a fully-free` |
|         - | 3128 | ` * trailing segment is handed back on truncate.` |
|         - | 3129 | ` *` |
|         - | 3130 | ` * SEGMENT SIZE is a floor, not just a granularity: the first segment is` |
|         - | 3131 | ` * allocated when the VM is, so every VM pays for one whether it holds three` |
|         - | 3132 | ` * values or three hundred thousand. 256 slots is 16 KB, which is what the` |
|         - | 3133 | ` * SySetAlloc(&pVm->aMemObj,0xFF) this replaced opened with -- deliberately, so` |
|         - | 3134 | ` * that segmenting the table did not raise the per-VM floor. It matters in two` |
|         - | 3135 | ` * places that are not this box: the -S server caches PHL_VM_CACHE_SIZE (16) VMs,` |
|         - | 3136 | ` * so the floor is paid sixteen times, and on ESP32-S3 internal RAM dips to 32 KB` |
|         - | 3137 | ` * free (ESP32.md), where a 256 KB opening allocation is not a cost but a failure.` |
|         - | 3138 | ` * The price of a small segment is one direct block and one segment-table entry` |
|         - | 3139 | ` * per 256 slots: at the phpcs peak of record (~356K slots) that is ~1,400` |
|         - | 3140 | ` * segments, ~33 KB of allocator headers and a 2,048-entry pointer table -- under` |
|         - | 3141 | ` * 0.04% of the peak. Override with -DPH7_VM_MEMPOOL_SEG_SHIFT=n for a target` |
|         - | 3142 | ` * that wants a different trade; nothing but the two constants below depends on it.` |
|         - | 3143 | ` *` |
|         - | 3144 | ` * Freed slots form a single INTRUSIVE free list threaded through the slots'` |
|         - | 3145 | ` * own (dead) nIdx word -- VmMemPoolFreeSlot writes the link into the slot, so` |
|         - | 3146 | ` * slot reuse is O(1) and costs zero extra memory. nFreeHead is the head, or` |
|         - | 3147 | ` * SXU32_HIGH when the list is empty. This replaced the aFreeObj SySet, which` |
|         - | 3148 | ` * grew one 16-byte VmSlot per free index to describe exactly what the link now` |
|         - | 3149 | ` * describes for free. Because the link lives INSIDE the slot, a slot on the` |
|         - | 3150 | ` * list carries MEMOBJ_POOLFREE: a second free of the same index would otherwise` |
|         - | 3151 | ` * write the head into the slot the head already points at, and every later` |
|         - | 3152 | ` * reserve would hand out that one slot forever. The old stack merely handed the` |
|         - | 3153 | ` * index out twice and drained; this one would not.` |
|         - | 3154 | ` */` |
|         - | 3155 | `#ifndef PH7_VM_MEMPOOL_SEG_SHIFT` |
|         - | 3156 | `#define PH7_VM_MEMPOOL_SEG_SHIFT 8` |
|         - | 3157 | `#endif` |
|         - | 3158 | `#define VM_MEMPOOL_SEG_SHIFT  PH7_VM_MEMPOOL_SEG_SHIFT` |
|         - | 3159 | `#define VM_MEMPOOL_SEG_SLOTS  (1u << VM_MEMPOOL_SEG_SHIFT)` |
|         - | 3160 | `#define VM_MEMPOOL_SEG_MASK   (VM_MEMPOOL_SEG_SLOTS - 1u)` |
|         - | 3161 | `typedef struct VmMemPool VmMemPool;` |
|         - | 3162 | `struct VmMemPool` |
|         - | 3163 | `{` |
|         - | 3164 | `	SyMemBackend *pAllocator; /* Memory backend the segments come from */` |
|         - | 3165 | `	ph7_value   **apSeg;      /* Segment pointer table (VM_MEMPOOL_SEG_SLOTS slots each) */` |
|         - | 3166 | `	sxu32         nSeg;       /* Segments currently allocated */` |
|         - | 3167 | `	sxu32         nCap;       /* Capacity of apSeg */` |
|         - | 3168 | `	sxu32         nUsed;      /* Logical slots in use -- SySetUsed(aMemObj) semantics */` |
|         - | 3169 | `	sxu32         nFreeHead;  /* First free slot, or SXU32_HIGH; freed slots chain through their nIdx */` |
|         - | 3170 | `};` |
|         - | 3171 | `/*` |
|         - | 3172 | ` * The nIdx'th slot of the pool, or NULL when the index is past the end -- the` |
|         - | 3173 | ` * same bounds contract SySetAt kept on the set this replaces, so a call site` |
|         - | 3174 | ` * that leaned on NULL for "that index has not been allocated" still works.` |
|         - | 3175 | ` * INLINE because this is the engine's hottest read: every array element,` |
|         - | 3176 | ` * property and variable is reached through it -- which is also why the shift and` |
|         - | 3177 | ` * the mask are the COMPILE-TIME constants and not fields of the pool. They can` |
|         - | 3178 | ` * only ever hold these two values, and reading them out of the struct would put` |
|         - | 3179 | ` * two loads and a variable shift on every value access to say what an immediate` |
|         - | 3180 | ` * already says.` |
|         - | 3181 | ` */` |
| 109969781 | 3182 | `SX_STATIC_INLINE ph7_value * PH7_MemObjAt(VmMemPool *pPool,sxu32 nIdx)` |
|         5 | 3183 | `{` |
| 109969786 | 3184 | `	if( nIdx >= pPool->nUsed ){` |
|       137 | 3185 | `		return 0;   /* Out of range */` |
|         - | 3186 | `	}` |
| 109969652 | 3187 | `	return &pPool->apSeg[nIdx >> VM_MEMPOOL_SEG_SHIFT][nIdx & VM_MEMPOOL_SEG_MASK];` |
|  54980949 | 3188 | `}` |
|         - | 3189 | `/*` |
|         - | 3190 | ` * Cycle-collector colours (vm_gc.c). php's, and Bacon & Rajan's before it.` |
|         - | 3191 | ` * BLACK is "in use", GREY "being trial-deleted", WHITE "counted zero",` |
|         - | 3192 | ` * PURPLE "buffered as a possible root", DEAD "proved garbage, being freed".` |
|         - | 3193 | ` * A container is born BLACK because its struct is zeroed.` |
|         - | 3194 | ` */` |
|         - | 3195 | `#define PH7_GC_BLACK   0` |
|         - | 3196 | `#define PH7_GC_GREY    1` |
|         - | 3197 | `#define PH7_GC_WHITE   2` |
|         - | 3198 | `#define PH7_GC_PURPLE  3` |
|         - | 3199 | `#define PH7_GC_DEAD    4` |
|         - | 3200 | `typedef struct VmGcRef VmGcRef;` |
|         - | 3201 | `/* One container, in the root buffer or in a traversal worklist. */` |
|         - | 3202 | `struct VmGcRef` |
|         - | 3203 | `{` |
|         - | 3204 | `	void *pPtr;  /* ph7_hashmap * or ph7_class_instance *; 0 once the row is spent */` |
|         - | 3205 | `	sxu8 bMap;   /* which of the two it is */` |
|         - | 3206 | `};` |
|         - | 3207 | `typedef struct VmRefObj VmRefObj;` |
|         - | 3208 | `typedef struct VmRefSpill VmRefSpill;` |
|         - | 3209 | `/*` |
|         - | 3210 | ` * The SECOND and later holder of each kind. Allocated only for a slot that really` |
|         - | 3211 | `` * has two names on it, or two array nodes -- which is what a PHP `&` reference is,`` |
|         - | 3212 | ` * and which almost no slot is: an ordinary variable is named once and an ordinary` |
|         - | 3213 | ` * array element is pointed at by one node. Every slot used to carry both of these` |
|         - | 3214 | ` * sets inline (80 bytes) plus the 32-byte buffer each grew on its first row, so the` |
|         - | 3215 | ` * engine paid a reference's price for every variable and every element it created.` |
|         - | 3216 | ` */` |
|         - | 3217 | `struct VmRefSpill` |
|         - | 3218 | `{` |
|         - | 3219 | `	SySet aReference;  /* Holders beyond pEntry0 */` |
|         - | 3220 | `	SySet aArrEntries; /* Holders beyond pNode0 */` |
|         - | 3221 | `};` |
|         - | 3222 | `/* Reference-object body (vm.c reference machinery; shared with vm_builtin_var.c's unset).` |
|         - | 3223 | ` *` |
|         - | 3224 | ` * A record is the FALLBACK shape, not the ordinary one: apRefObj[nIdx] is a tagged` |
|         - | 3225 | ` * WORD (see VM_REF_TAG_* below) and only a slot whose answer will not fit in one` |
|         - | 3226 | ` * ever allocates this. */` |
|         - | 3227 | `struct VmRefObj` |
|         - | 3228 | `{` |
|         - | 3229 | `	SyHashEntry *pEntry0;     /* The one name bound to this slot; 0 once it is gone */` |
|         - | 3230 | `	ph7_hashmap_node *pNode0; /* The one array node pointing here; 0 once it is gone */` |
|         - | 3231 | `	VmRefSpill *pSpill;       /* The 2nd..nth holder of either kind; 0 while there is none */` |
|         - | 3232 | `	sxu32 nIdx;        /* Referenced object index -- also this record's cell in apRefObj[] */` |
|         - | 3233 | `	sxu32 nPin;        /* Holders the table cannot name, COUNTED so the last one to go` |
|         - | 3234 | `	                    * can release the slot: reference-bound properties (one per` |
|         - | 3235 | `	                    * binding, dropped when the property is released or re-bound).` |
|         - | 3236 | `	                    * A slot pinned by a site that never unpins (a use(&$x) capture,` |
|         - | 3237 | `	                    * a static, an enum case) leaves this 0 and relies on the` |
|         - | 3238 | `	                    * VM_REF_IDX_KEEP flag alone, which is a permanent pin. */` |
|         - | 3239 | `	sxi32 iFlags;      /* Configuration flags */` |
|         - | 3240 | `};` |
|         - | 3241 | `#define VM_REF_IDX_KEEP  0x001 /* Do not restore the memory object to the free list */` |
|         - | 3242 | `/*` |
|         - | 3243 | ` * apRefObj[nIdx] is ONE TAGGED WORD, not a pointer to a record.` |
|         - | 3244 | ` *` |
|         - | 3245 | ` * What the reference table has to say about the ordinary slot is one sentence long --` |
|         - | 3246 | ` * "this name holds it", "this array node points at it", "the object that declares it` |
|         - | 3247 | ` * holds it" -- and a whole heap record to say it is the engine's single largest` |
|         - | 3248 | ` * per-value cost. The word says the sentence itself; a record is allocated only when` |
|         - | 3249 | ` * the answer needs more than one holder, which a census of the ecosystem gate's phpcs` |
|         - | 3250 | ` * step puts at 14 of the 360264 records live at peak.` |
|         - | 3251 | ` *` |
|         - | 3252 | ` *   0                          nothing has ever been registered against the slot` |
|         - | 3253 | ` *   pEntry \| VM_REF_TAG_NAME   exactly ONE holder: the name bound to the slot` |
|         - | 3254 | ` *   pNode  \| VM_REF_TAG_NODE   exactly ONE holder: the array node pointing at it` |
|         - | 3255 | ` *   bits   \| VM_REF_TAG_MARK   registered, NO NAMED holder; the rest of the word is` |
|         - | 3256 | ` *                              the pin count and the flags (this is both the spent` |
|         - | 3257 | ` *                              record a dropped holder leaves behind -- which is what` |
|         - | 3258 | ` *                              still returns the slot to the free pool -- and the` |
|         - | 3259 | ` *                              declared property's own VM_REF_IDX_KEEP)` |
|         - | 3260 | ` *   pRef   (tag 0, non-zero)   a VmRefObj *: two or more holders, or a pin beside one` |
|         - | 3261 | ` *` |
|         - | 3262 | ` * The two pointer tags ride in the low bits of a pool-allocated address; the allocator` |
|         - | 3263 | ` * keeps every chunk 8-aligned (see the alignment note on sxmem.c's OS methods), and a` |
|         - | 3264 | ` * pointer that is not 4-aligned falls back to a record rather than being tagged.` |
|         - | 3265 | ` */` |
|         - | 3266 | `#define VM_REF_TAG_MASK    3` |
|         - | 3267 | `#define VM_REF_TAG_FULL    0  /* a VmRefObj * */` |
|         - | 3268 | `#define VM_REF_TAG_NAME    1  /* a SyHashEntry * */` |
|         - | 3269 | `#define VM_REF_TAG_NODE    2  /* a ph7_hashmap_node * */` |
|         - | 3270 | `#define VM_REF_TAG_MARK    3  /* no pointer: pin count and flags in the upper bits */` |
|         - | 3271 | `#define VM_REF_MARK_KEEP   0x4        /* bit 2 of a MARK word: VM_REF_IDX_KEEP */` |
|         - | 3272 | `#define VM_REF_MARK_PIN    0x8        /* bit 3 and up: the counted pin */` |
|         - | 3273 | `#define VM_REF_MARK_PINMAX 0x0FFFFFFF /* a pin count past this promotes to a record */` |
|         - | 3274 | `/* The tag of a word. Only the low two bits are read, so the cast may narrow. */` |
|         - | 3275 | `#define VM_REF_TAGOF(W)    (SX_PTR_TO_INT(W) & VM_REF_TAG_MASK)` |
|         - | 3276 | `/* VmObEntry struct moved to ph7int.h */` |
|         - | 3277 |  |
|         - | 3278 | `/*` |
|         - | 3279 | ` * Each catch [i.e catch(Exception $e){ } ] block is parsed out and stored` |
|         - | 3280 | ` * in an instance of the following structure.` |
|         - | 3281 | ` */` |
|         - | 3282 | `typedef struct ph7_exception_block ph7_exception_block;` |
|         - | 3283 | `typedef struct ph7_exception ph7_exception;` |
|         - | 3284 | `struct ph7_exception_block` |
|         - | 3285 | `{` |
|         - | 3286 | `	SySet aClasses;  /* Exception class names (SyString instances) for multi-catch */` |
|         - | 3287 | `	SyString sThis;  /* Instance name [i.e: $e..] */` |
|         - | 3288 | `	SySet *pByteCode;/* Compiled instructions of a DETACHED catch body (the path every` |
|         - | 3289 | `	                  * non-generator try takes; NULL for a ROOT C inline catch, which compiles` |
|         - | 3290 | `	                  * into the function's own array). Heap-allocated so its ADDRESS is stable:` |
|         - | 3291 | ``	                  * a `break`/`continue` inside the body records this container in its`` |
|         - | 3292 | `	                  * JumpFixup, resolved long after PH7_CompileCatch returned and after` |
|         - | 3293 | `	                  * sEntry grew (both would move an embedded SySet). */` |
|         - | 3294 | `	sxu32 iHandlerPc;/* ROOT C: inline PC where this catch body begins (0 = not inlined) */` |
|         - | 3295 | `};` |
|         - | 3296 | `/*` |
|         - | 3297 | ` * Context for the exception mechanism.` |
|         - | 3298 | ` */` |
|         - | 3299 | `struct ph7_exception` |
|         - | 3300 | `{` |
|         - | 3301 | `	ph7_vm *pVm;    /* VM that own this exception */` |
|         - | 3302 | `	SySet sEntry;   /* Compiled 'catch' blocks (ph7_exception_block instance)` |
|         - | 3303 | `				     * container.` |
|         - | 3304 | `					 */` |
|         - | 3305 | `	SySet sFinally; /* Compiled 'finally' block bytecode (legacy; unused once ROOT C inlining lands) */` |
|         - | 3306 | `	int iHasFinally;/* TRUE if a finally block was compiled */` |
|         - | 3307 | `	int iFinallyDone;/* TRUE if the finally block was already executed (legacy VmLocalExec path) */` |
|         - | 3308 | `	int iInlined;   /* ROOT C: TRUE when this try's catch/finally are inlined into the function` |
|         - | 3309 | `					 * bytecode (generator body). FALSE = legacy detached-mini-program path. */` |
|         - | 3310 | `	sxu32 iFinallyPc;/* ROOT C: inline PC where the finally body begins (0 = no finally) */` |
|         - | 3311 | `	sxu32 iEndCatchPc;/* ROOT C: inline PC just after the whole try/catch/finally (normal exit) */` |
|         - | 3312 | `	sxu32 iNextFinallyPc;/* ROOT C: iFinallyPc of the lexically-enclosing try-with-finally in the` |
|         - | 3313 | `					   * same function, or 0 — threads a return/break out through nested finallys */` |
|         - | 3314 | `	int iInCatch;   /* ROOT C: TRUE while a catch body of this try is running (finally still owed) */` |
|         - | 3315 | `	ph7_class_instance *pInflight;/* ROOT C: exception to bind at OP_CATCH / re-raise at END_FINALLY */` |
|         - | 3316 | `	VmFrame *pFrame; /* Frame that trigger the exception */` |
|         - | 3317 | `	sxu32 iLandingPc;/* Post-try landing pad (= OP_LOAD_EXCEPTION's iP2). Mirrors the` |
|         - | 3318 | `					  * exception frame's iExceptionJump but survives that frame's` |
|         - | 3319 | `					  * teardown, so an in-place catch can record where to resume. */` |
|         - | 3320 | `	void *pOwnerInstr;/* Bytecode array (VmInstr*) this try was compiled into. iLandingPc` |
|         - | 3321 | `					   * indexes THIS array; the resume only fires in the exec running it` |
|         - | 3322 | `					   * (distinguishes a mini-program from the body that shares its frame). */` |
|         - | 3323 | `` 	sxi32 iErrSuppress;/* '@' suppression depth at try entry. A throw from inside `@expr` `` |
|         - | 3324 | `	                    * unwinds past the ERR_CTRL that would have closed the window, so` |
|         - | 3325 | `	                    * the catch restores this snapshot instead of leaking the depth —` |
|         - | 3326 | ``	                    * and a try/catch nested INSIDE an `@` still stays suppressed. */`` |
|         - | 3327 | `	sxi32 iStackDepth;/* Operand-stack base (0-based TOS index = pTos-pStack, -1 when empty)` |
|         - | 3328 | `					   * captured when this try opened at OP_LOAD_EXCEPTION. Used only by` |
|         - | 3329 | `					   * Generator::throw() inject-at-yield to drain the abandoned` |
|         - | 3330 | `					   * (mid-expression) operand slots back to the try's base before` |
|         - | 3331 | `					   * landing at iLandingPc. */` |
|         - | 3332 | `	ph7_exception *pCompiled;/* BYTECODE stage 2b: NULL on the compiler-owned object; on a` |
|         - | 3333 | `					   * runtime ACTIVATION (clone pushed by OP_LOAD_EXCEPTION) this points` |
|         - | 3334 | `					   * at the compiled origin. Every activation of a lexical try carries` |
|         - | 3335 | `					   * its OWN mutable state (pFrame/iFinallyDone/iInCatch/pInflight/` |
|         - | 3336 | `					   * iStackDepth) — recursion levels no longer share one object, which` |
|         - | 3337 | `					   * ran every level's catch/finally against the deepest frame. */` |
|         - | 3338 | `};` |
|         - | 3339 | `/*` |
|         - | 3340 | `` * ROOT C: a pending non-local exit for an inline `finally` body. When control`` |
|         - | 3341 | ` * enters a finally (normal fall-through, a caught/unmatched throw, or a return/` |
|         - | 3342 | ` * break/continue crossing the try), one of these is pushed onto pVm->aFinallyAction;` |
|         - | 3343 | ` * the finally's terminating OP_END_FINALLY pops it and dispatches accordingly. A` |
|         - | 3344 | ` * return/break/continue crossing several nested finallys keeps ONE record on the` |
|         - | 3345 | ` * stack and re-drives it through each finally via ph7_exception.iNextFinallyPc.` |
|         - | 3346 | ` */` |
|         - | 3347 | `#define PH7_FA_FALLTHROUGH 0  /* Resume at iNextPc (post-construct landing) */` |
|         - | 3348 | `#define PH7_FA_RETHROW     1  /* Re-raise pExc after the finally runs */` |
|         - | 3349 | `#define PH7_FA_RETURN      2  /* Return sRet from pTargetBody after the finally chain */` |
|         - | 3350 | `#define PH7_FA_JMP         3  /* Break/continue: resume at iNextPc after the finally chain */` |
|         - | 3351 | `typedef struct VmFinallyAction VmFinallyAction;` |
|         - | 3352 | `struct VmFinallyAction` |
|         - | 3353 | `{` |
|         - | 3354 | `	int eKind;                    /* One of PH7_FA_* */` |
|         - | 3355 | `	sxu32 iNextPc;                /* FALLTHROUGH/JMP: pc (0-based) to resume at in this array */` |
|         - | 3356 | `	ph7_class_instance *pExc;     /* RETHROW: exception to re-raise (holds a ref) */` |
|         - | 3357 | `	ph7_value sRet;               /* RETURN: the value to return (owned) */` |
|         - | 3358 | ``	int bHasRetVal;               /* RETURN: TRUE if sRet holds a real value (vs bare `return;`) */`` |
|         - | 3359 | `	void *pTargetBody;            /* RETURN: VmFrame* the return materializes on */` |
|         - | 3360 | `	int nCross;                   /* trys still to cross through their finallys (-1 = unbounded,` |
|         - | 3361 | `	                               * for RETURN; a positive count bounds a break/continue to the` |
|         - | 3362 | `	                               * trys between it and its target loop) */` |
|         - | 3363 | `};` |
|         - | 3364 | `/* Forward reference */` |
|         - | 3365 | `typedef struct ph7_case_expr ph7_case_expr;` |
|         - | 3366 | `typedef struct ph7_switch ph7_switch;` |
|         - | 3367 | `/*` |
|         - | 3368 | ` * Each compiled case block in a swicth statement is compiled` |
|         - | 3369 | ` * and stored in an instance of the following structure.` |
|         - | 3370 | ` */` |
|         - | 3371 | `struct ph7_case_expr` |
|         - | 3372 | `{` |
|         - | 3373 | `	SySet aByteCode;   /* Compiled body of the case block */` |
|         - | 3374 | `	sxu32 nStart;      /* First instruction to execute */` |
|         - | 3375 | `};` |
|         - | 3376 | `/*` |
|         - | 3377 | ` * Each compiled switch statement is parsed out and stored` |
|         - | 3378 | ` * in an instance of the following structure.` |
|         - | 3379 | ` */` |
|         - | 3380 | `struct ph7_switch` |
|         - | 3381 | `{` |
|         - | 3382 | `	SySet aCaseExpr;  /* Compile case block */` |
|         - | 3383 | `	sxu32 nOut;       /* First instruction to execute after this statement */` |
|         - | 3384 | `	sxu32 nDefault;   /* First instruction to execute in the default block */` |
|         - | 3385 | `};` |
|         - | 3386 | `/*` |
|         - | 3387 | ` * Each arm of a PHP 8.0 match expression is compiled into` |
|         - | 3388 | ` * an instance of the following structure.` |
|         - | 3389 | ` */` |
|         - | 3390 | `typedef struct ph7_match_arm ph7_match_arm;` |
|         - | 3391 | `typedef struct ph7_match     ph7_match;` |
|         - | 3392 | `struct ph7_match_arm` |
|         - | 3393 | `{` |
|         - | 3394 | `	SySet aConds;   /* SySet of SySet (VmInstr) — one compiled bytecode block per condition value */` |
|         - | 3395 | `	SySet aResult;  /* Compiled bytecode of the arm's result expression */` |
|         - | 3396 | `	int   bDefault; /* 1 if this is the 'default' arm */` |
|         - | 3397 | `};` |
|         - | 3398 | `struct ph7_match` |
|         - | 3399 | `{` |
|         - | 3400 | `	SySet aArms;    /* SySet of ph7_match_arm */` |
|         - | 3401 | `};` |
|         - | 3402 | `/* Assertion flags */` |
|         - | 3403 | `#define PH7_ASSERT_DISABLE    0x01  /* Disable assertion */` |
|         - | 3404 | `#define PH7_ASSERT_WARNING    0x02  /* Deprecated in PHP 8: kept for constant compatibility only */` |
|         - | 3405 | `#define PH7_ASSERT_BAIL       0x04  /* Terminate execution on failed assertions */` |
|         - | 3406 | `#define PH7_ASSERT_QUIET_EVAL 0x08  /* Not used */` |
|         - | 3407 | `#define PH7_ASSERT_CALLBACK   0x10  /* Callback to call on failed assertions */` |
|         - | 3408 | `#define PH7_ASSERT_ZEND_OFF   0x20  /* zend.assertions < 1: assert() compiled out (php CLI default -1) */` |
|         - | 3409 | `/*` |
|         - | 3410 | ` * error_log() consumer function signature.` |
|         - | 3411 | ` * Refer to the [PH7_VM_CONFIG_ERR_LOG_HANDLER] configuration directive` |
|         - | 3412 | ` * for more information on how to register an error_log consumer().` |
|         - | 3413 | ` */` |
|         - | 3414 | `typedef void (*ProcErrLog)(const char *,int,const char *,const char *);` |
|         - | 3415 | `/*` |
|         - | 3416 | ` * An instance of the following structure hold the bytecode instructions` |
|         - | 3417 | ` * resulting from compiling a PHP script.` |
|         - | 3418 | ` * This structure contains the complete state of the virtual machine.` |
|         - | 3419 | ` */` |
|         - | 3420 | `/* In-flight magic-accessor guard entry (band A #3a; see vm.c helpers). */` |
|         - | 3421 | `typedef struct VmMagicGuard VmMagicGuard;` |
|         - | 3422 | `struct VmMagicGuard` |
|         - | 3423 | `{` |
|         - | 3424 | `	void *pThis;      /* instance identity */` |
|         - | 3425 | `	sxu32 nNameHash;  /* property-name hash (SyBinHash) */` |
|         - | 3426 | `	sxu8 cKind;       /* accessor kind: 'g' = __get */` |
|         - | 3427 | `};` |
|         - | 3428 | `/* Pending property write-back entry (PHP 8.4 hooks + magic ??=): a LIFO of` |
|         - | 3429 | ` * these (ph7_vm.aHookRmw) carries every write whose dispatch is deferred past` |
|         - | 3430 | ` * OP_MEMBER to a later opcode:` |
|         - | 3431 | ` *   VM_HOOK_PEND_RMW        — read-modify-write on a hooked property: OP_MEMBER` |
|         - | 3432 | ` *                             dispatched the get hook (or read the raw backing` |
|         - | 3433 | ` *                             store when set-only) into a fresh SCRATCH memobj` |
|         - | 3434 | ` *                             slot; the modify op (++/--/compound-assign)` |
|         - | 3435 | ` *                             mutates the scratch and its tail consumes the` |
|         - | 3436 | ` *                             entry (matched by kind + scratch index) to` |
|         - | 3437 | ` *                             dispatch the set hook with the computed value.` |
|         - | 3438 | `` *   VM_HOOK_PEND_COAL_HOOK  — `$o->p ??= v` on a hooked property: the entry is`` |
|         - | 3439 | ` *                             consumed by the OP_NULLC_STORE at nPc (matched by` |
|         - | 3440 | ` *                             owner + pc) to dispatch the set hook.` |
|         - | 3441 | `` *   VM_HOOK_PEND_COAL_MAGIC — `$o->p ??= v` on a missing property whose class`` |
|         - | 3442 | ` *                             declares __set: consumed the same way, dispatching` |
|         - | 3443 | ` *                             __set(sName, value).` |
|         - | 3444 | ` *   VM_HOOK_PEND_RMW_MAGIC  — read-modify-write on an OVERLOADED property` |
|         - | 3445 | `` *                             (`$o->n++`, `$o->n .= 'x'`): the same scratch-slot`` |
|         - | 3446 | ` *                             rail as VM_HOOK_PEND_RMW, with __get having` |
|         - | 3447 | ` *                             provided the current value and __set(sName, value)` |
|         - | 3448 | ` *                             taking the computed one.` |
|         - | 3449 | `` *   VM_HOOK_PEND_RMW_DIM    — `$o[$k] op= v` on an ArrayAccess element (php's`` |
|         - | 3450 | ` *                             ASSIGN_DIM_OP): offsetGet($k) provided the current` |
|         - | 3451 | ` *                             value and offsetSet($k, value) takes the computed` |
|         - | 3452 | ` *                             one. The KEY lives in its own reserved memobj,` |
|         - | 3453 | ` *                             whose index this kind keeps in nBackIdx.` |
|         - | 3454 | ` * The armed window is [nJmpPc, nPc]: an owner fetch outside it means the` |
|         - | 3455 | ` * statement was abandoned (a routed throw) or the ??= short-circuit jump was` |
|         - | 3456 | ` * taken — the entry is dropped, no set dispatch (php: the throw/skip discards` |
|         - | 3457 | ` * the write). LIFO order makes nested arms (a ??= RHS containing further` |
|         - | 3458 | ` * hooked stores or coalesce-assigns, a recursive re-entry through a cast` |
|         - | 3459 | ` * inside a modify op) nest correctly. Each entry owns one instance reference;` |
|         - | 3460 | ` * MAGIC entries own their name blob. */` |
|         - | 3461 | `#define VM_HOOK_PEND_RMW         0` |
|         - | 3462 | `#define VM_HOOK_PEND_COAL_HOOK   1` |
|         - | 3463 | `#define VM_HOOK_PEND_COAL_MAGIC  2` |
|         - | 3464 | `#define VM_HOOK_PEND_RMW_MAGIC   3` |
|         - | 3465 | `#define VM_HOOK_PEND_RMW_DIM     4` |
|         - | 3466 | `/* The SCRATCH-slot kinds: armed by the fetch (OP_MEMBER / OP_LOAD_IDX) and` |
|         - | 3467 | ` * consumed by the modify op's tail through VmHookRmwConsume, matched by the` |
|         - | 3468 | ` * scratch index the fetch left in pTos->nIdx. */` |
|         - | 3469 | `#define VM_HOOK_PEND_IS_RMW(iKind) \` |
|         - | 3470 | `	((iKind) == VM_HOOK_PEND_RMW \|\| (iKind) == VM_HOOK_PEND_RMW_MAGIC \` |
|         - | 3471 | `	 \|\| (iKind) == VM_HOOK_PEND_RMW_DIM)` |
|         - | 3472 | `typedef struct VmHookRmw VmHookRmw;` |
|         - | 3473 | `struct VmHookRmw` |
|         - | 3474 | `{` |
|         - | 3475 | `	sxu8 iKind;                 /* VM_HOOK_PEND_* */` |
|         - | 3476 | `	ph7_class_instance *pThis;  /* receiver (owns one reference while pending) */` |
|         - | 3477 | `	ph7_class_attr *pAttr;      /* hooked property (hook kinds; 0 for MAGIC) */` |
|         - | 3478 | ``	sxu32 nBackIdx;             /* BACKING slot index (for `set => expr` stores) */`` |
|         - | 3479 | `	sxu32 nScratchIdx;          /* RMW: scratch slot the modify op operates on;` |
|         - | 3480 | `	                             * SXU32_HIGH for the coalesce kinds */` |
|         - | 3481 | `	SyBlob sName;               /* COAL_MAGIC: property name copy (entry-owned) */` |
|         - | 3482 | `	void *pOwnerStack;          /* arming activation's operand-stack base (identity;` |
|         - | 3483 | `	                             * a nested exec — even a recursive one over the same` |
|         - | 3484 | `	                             * bytecode — has a different base, so it never drops` |
|         - | 3485 | `	                             * an enclosing activation's pending entry) */` |
|         - | 3486 | `	void *pInstrs;              /* arming activation's bytecode array */` |
|         - | 3487 | `	sxu32 nJmpPc;               /* first pc of the armed window (RMW: == nPc;` |
|         - | 3488 | `	                             * coalesce: the OP_NULLC_JMP right after the arm) */` |
|         - | 3489 | `	sxu32 nPc;                  /* pc of the consuming op (RMW: the modify op;` |
|         - | 3490 | `	                             * coalesce: the OP_NULLC_STORE) */` |
|         - | 3491 | `};` |
|         - | 3492 |  |
|         - | 3493 | `/* A shared weak cell: one per weakly-referenced target instance. pObj nulls` |
|         - | 3494 | ` * when the target is released (the PH7_ClassInstanceRelease hook); nRef` |
|         - | 3495 | ` * counts the PHP-side handles (WeakReference objects, WeakMap entries). */` |
|         - | 3496 | `typedef struct VmWeakCell VmWeakCell;` |
|         - | 3497 | `struct VmWeakCell` |
|         - | 3498 | `{` |
|         - | 3499 | `	ph7_class_instance *pObj; /* target instance; 0 once dead */` |
|         - | 3500 | `	ph7_class_instance *pRef; /* the ONE WeakReference handed out for pObj, so` |
|         - | 3501 | `	                           * WeakReference::create($o) answers the same object` |
|         - | 3502 | `	                           * twice as php's does. NOT owned: the WeakReference's` |
|         - | 3503 | `	                           * own release nulls it. */` |
|         - | 3504 | `	sxu32 nRef;               /* PHP-side handle count */` |
|         - | 3505 | `};` |
|         - | 3506 | `/* The OPEN directory handle behind one DirectoryIterator (php's u.dir.dirp).` |
|         - | 3507 | ` * Registered per instance rather than in a property slot: a slot holding a C` |
|         - | 3508 | `` * pointer would be compared by `==` (php's two equal-positioned iterators are`` |
|         - | 3509 | ` * equal) and COPIED by clone, and php's clone opens the directory again. The` |
|         - | 3510 | ` * class's xRelease closes it. */` |
|         - | 3511 | `typedef struct VmDirHandle VmDirHandle;` |
|         - | 3512 | `struct VmDirHandle` |
|         - | 3513 | `{` |
|         - | 3514 | `	const ph7_io_stream *pStream; /* device that opened it */` |
|         - | 3515 | `	void *pHandle;                /* its handle */` |
|         - | 3516 | `	ph7_class_instance *pThis;    /* the owning instance -- and the hash KEY's bytes,` |
|         - | 3517 | `	                               * which SyHashInsert borrows rather than copies */` |
|         - | 3518 | `};` |
|         - | 3519 | `/* One -d/-c php.ini directive queued for the INI chunk (name/value are` |
|         - | 3520 | ` * allocator-owned copies; see PH7_VM_CONFIG_INI_ENTRY) */` |
|         - | 3521 | `typedef struct VmIniEntry VmIniEntry;` |
|         - | 3522 | `struct VmIniEntry` |
|         - | 3523 | `{` |
|         - | 3524 | `	SyString sName;` |
|         - | 3525 | `	SyString sValue;` |
|         - | 3526 | `};` |
|         - | 3527 | `/*` |
|         - | 3528 | ` * One live php.ini directive. The table was an embedded-PHP array on a private` |
|         - | 3529 | `` * `__IniS` class; it is C now, seeded lazily on the first INI call from the static`` |
|         - | 3530 | ` * defaults merged with the CLI's -d/-c queue (aIniCli).` |
|         - | 3531 | ` *` |
|         - | 3532 | ` * php exposes both a global_value and a local_value per directive: ini_set() moves` |
|         - | 3533 | ` * the local one, ini_restore() puts the global one back, and get_cfg_var() answers` |
|         - | 3534 | ` * the global one. Both blobs live on the VM allocator, so they are freed with it --` |
|         - | 3535 | ` * no release hook, exactly as aIniCli needs none.` |
|         - | 3536 | ` */` |
|         - | 3537 | `/*` |
|         - | 3538 | ` * The diagnostics of the last date parse — DateTime::getLastErrors()'s whole answer.` |
|         - | 3539 | ` *` |
|         - | 3540 | ` * php keeps one such record per request and both date classes read it, so this is a` |
|         - | 3541 | `` * VM field rather than the PHL-only `public static DateTime::$__dtLastErr` it used to`` |
|         - | 3542 | ` * be. Every message is a static literal owned by the parser, so nothing here owns` |
|         - | 3543 | ` * memory and the record needs no release hook. The kept-vs-total split is php's:` |
|         - | 3544 | `` * `error_count` counts every error the scan raised, while the `errors` map holds one`` |
|         - | 3545 | ` * entry per POSITION (a later error at a position php has already reported replaces` |
|         - | 3546 | ` * the message rather than adding a row).` |
|         - | 3547 | ` */` |
|         - | 3548 | `#define PH7_DT_MAX_WARN 3` |
|         - | 3549 | `#define PH7_DT_MAX_ERR  8` |
|         - | 3550 | `/* One diagnostic row. The message is always a static literal, so a row keeps` |
|         - | 3551 | ` * the pointer rather than the bytes. */` |
|         - | 3552 | `typedef struct phl_dt_diag_row phl_dt_diag_row;` |
|         - | 3553 | `struct phl_dt_diag_row` |
|         - | 3554 | `{` |
|         - | 3555 | `	int iPos;` |
|         - | 3556 | `	const char *zMsg;` |
|         - | 3557 | `};` |
|         - | 3558 | `typedef struct phl_dt_lasterr phl_dt_lasterr;` |
|         - | 3559 | `struct phl_dt_lasterr` |
|         - | 3560 | `{` |
|         - | 3561 | ``	sxu8 bSet;                        /* 0 -> getLastErrors() answers php's `false` */`` |
|         - | 3562 | `	int nWarn;                        /* warning_count (total) */` |
|         - | 3563 | `	int nWarnKept;                    /* rows in the warnings map */` |
|         - | 3564 | `	int aWarnPos[PH7_DT_MAX_WARN];` |
|         - | 3565 | `	const char *azWarn[PH7_DT_MAX_WARN];` |
|         - | 3566 | `	int nErr;                         /* error_count (total) */` |
|         - | 3567 | `	int nErrKept;                     /* rows in the errors map */` |
|         - | 3568 | `	/* php's scanner records an error and READS ON, so a string may carry one per` |
|         - | 3569 | `	 * byte of itself: the rows grow rather than fitting a fixed array. The blob` |
|         - | 3570 | `	 * holds nErrKept phl_dt_diag_row, allocated from the VM's own backend and` |
|         - | 3571 | `	 * released wholesale with it. */` |
|         - | 3572 | `	SyBlob sErr;` |
|         - | 3573 | `};` |
|         - | 3574 | `typedef struct VmIniSlot VmIniSlot;` |
|         - | 3575 | `struct VmIniSlot` |
|         - | 3576 | `{` |
|         - | 3577 | `	SyString sName;   /* static default name, or a VM-lifetime dup of a CLI name */` |
|         - | 3578 | `	sxi32 iAccess;    /* INI_USER\|INI_PERDIR\|INI_SYSTEM bitmask php reports */` |
|         - | 3579 | `	SyBlob sGlobal;   /* php's global_value */` |
|         - | 3580 | `	SyBlob sLocal;    /* php's local_value (what ini_get answers, modulo live wiring) */` |
|         - | 3581 | `	/* php's third state for a value: UNSET. A directive php declares with no` |
|         - | 3582 | `	 * default at all reports NULL rather than the empty string from every` |
|         - | 3583 | `	 * surface that shows the raw value, and the empty string IS a different` |
|         - | 3584 | `	 * value -- one a script can write. An empty blob cannot tell them apart, so` |
|         - | 3585 | `	 * the two flags do. */` |
|         - | 3586 | `	sxu8 bGlobalNull; /* the directive was declared with no value */` |
|         - | 3587 | `	sxu8 bLocalNull;  /* and nothing has written one since */` |
|         - | 3588 | `};` |
|         - | 3589 | ``/* php's default spl_autoload_extensions() list: the `.inc` is tried FIRST,`` |
|         - | 3590 | ` * which is what decides the answer when two files with the same base name` |
|         - | 3591 | ` * both declare the class. */` |
|         - | 3592 | `#define PH7_SPL_AUTOLOAD_EXT ".inc,.php"` |
|         - | 3593 | `struct ph7_vm` |
|         - | 3594 | `{` |
|         - | 3595 | `	SyMemBackend sAllocator;	/* Memory backend */` |
|         - | 3596 | `#if defined(PH7_ENABLE_THREADS)` |
|         - | 3597 | `	SyMutex *pMutex;           /* Recursive mutex associated with VM. */` |
|         - | 3598 | `#endif` |
|         - | 3599 | `	ph7 *pEngine;               /* Interpreter that own this VM */` |
|         - | 3600 | `	SySet aByteCode;            /* Default bytecode container */` |
|         - | 3601 | `	SySet *pByteContainer;      /* Current bytecode container */` |
|         - | 3602 | `	VmFrame *pFrame;            /* Stack of active frames */` |
|         - | 3603 | `	SyPRNGCtx sPrng;            /* PRNG context (engine-internal, OS-seeded entropy) */` |
|         - | 3604 | `	SyMT19937Ctx sMt;           /* MT19937 backing rand()/mt_rand(); reset by srand()/mt_srand() */` |
|         - | 3605 | `	sxi32 mtSeeded;             /* TRUE once sMt holds a seed (lazy: first draw seeds from the OS CSPRNG, like PHP) */` |
|         - | 3606 | `	VmMemPool aMemObj;          /* Object allocation table (segmented, PERF.md P1) */` |
|         - | 3607 | `	SySet aLitObj;              /* Literals allocation table */` |
|         - | 3608 | `	ph7_value *aOps;            /* Operand stack */` |
|         - | 3609 | `	SyHash hClass;              /* Compiled classes container */` |
|         - | 3610 | `	SyHash hConstant;           /* Host-application and user defined constants container */` |
|         - | 3611 | `	SyHash hHostFunction;       /* Host-application installable functions */` |
|         - | 3612 | `	SyHash hFunction;           /* Compiled functions */` |
|         - | 3613 | `	SyHash hSuper;              /* Superglobals hashtable */` |
|         - | 3614 | `	sxu32 aSuperFirst[8];       /* Which FIRST BYTES any superglobal name starts with, as a` |
|         - | 3615 | `	                             * 256-bit set. Every variable access asks hSuper before the` |
|         - | 3616 | `	                             * frame -- php's rule, and the order cannot change -- and` |
|         - | 3617 | `	                             * that question hashed the whole name to answer "no" for the` |
|         - | 3618 | `	                             * ~9 names that are superglobals ($GLOBALS and the $_* set).` |
|         - | 3619 | `	                             * Two thirds of the engine's hash lookups on a real workload` |
|         - | 3620 | `	                             * were that miss. One bit test now settles it for a name that` |
|         - | 3621 | `	                             * cannot be one. */` |
|         - | 3622 | `	SyHash hPDO;                /* PDO installed drivers */` |
|         - | 3623 | `	SyBlob sConsumer;           /* Default VM consumer [i.e Redirect all VM output to this blob] */` |
|         - | 3624 | `	SyBlob sWorker;             /* General purpose working buffer */` |
|         - | 3625 | `	SySet aFiles;               /* Stack of processed files */` |
|         - | 3626 | ``	SyBlob sReflectConstName;   /* Scratch for the `Class::MEMBER` name ReflectionParameter::`` |
|         - | 3627 | `	                             * getDefaultValueConstantName() answers for a class-constant` |
|         - | 3628 | `	                             * default: the two halves live in separate literals, so the` |
|         - | 3629 | `	                             * joined text needs somewhere to live past the return. */` |
|         - | 3630 | `	SySet aIncFrame;            /* Stack of ACTIVE include/require/eval activations (VmIncFrame).` |
|         - | 3631 | `	                             * php shows each of them as a trace frame of its own -- the` |
|         - | 3632 | ``	                             * `#N main.php(4): require()` between the included file's frames`` |
|         - | 3633 | `	                             * and the caller's -- and nothing else in this engine records` |
|         - | 3634 | `	                             * one: an include shares its caller's variable scope, so it` |
|         - | 3635 | `	                             * pushes no VmFrame to be found later. */` |
|         - | 3636 | `	SySet aPaths;               /* Set of import paths */` |
|         - | 3637 | `	SySet aIncluded;            /* Set of included files */` |
|         - | 3638 | ``	SySet aEvalFile;            /* Interned `<file>(<line>) : eval()'d code` unit names, one per`` |
|         - | 3639 | `	                             * eval() SITE. A compiled function or class copies the name it` |
|         - | 3640 | `	                             * was declared in, so the text has to outlive the eval that` |
|         - | 3641 | `	                             * made it -- and an eval in a loop must not mint a fresh copy` |
|         - | 3642 | `	                             * every turn, since the site's file and line never change. */` |
|         - | 3643 | `	SySet aOB;                  /* Stackable output buffers */` |
|         - | 3644 | `	SySet aResponseHeaders;     /* HTTP response headers (VmResponseHeader entries) */` |
|         - | 3645 | `	int iResponseStatus;        /* HTTP response status code (default 200) */` |
|         - | 3646 | `	int bHeadersSent;           /* TRUE once non-OB output has been emitted */` |
|         - | 3647 | `	SyBlob sOutStartFile;       /* WHERE that first output went out: php names the file and the` |
|         - | 3648 | `	                             * line in four diagnostics ("output started at %s:%u", and the` |
|         - | 3649 | `	                             * session pair's "sent from %s on line %u") and hands them to` |
|         - | 3650 | `	                             * headers_sent()'s two by-ref out-params. Empty until output. */` |
|         - | 3651 | `	sxu32 nOutStartLine;        /* ... its line (0 while nothing has been emitted) */` |
|         - | 3652 | `	SyBlob sSessStartFile;      /* WHERE the active session was started: php's session-locked ini` |
|         - | 3653 | `	                             * diagnostic names it ("started from %s on line %u"). */` |
|         - | 3654 | `	sxu32 nSessStartLine;       /* ... its line */` |
|         - | 3655 | `	int bHttpContext;           /* TRUE when an HTTP request has been fed (server/CGI mode) */` |
|         - | 3656 | `	int bInlineTryCatch;        /* ROOT C: TRUE once the inline try/catch/finally VM handlers exist,` |
|         - | 3657 | `	                             * enabling the compiler to inline generator-body try/catch (so a` |
|         - | 3658 | ``	                             * `yield` in a catch/finally suspends). Default 0 = legacy path. */`` |
|         - | 3659 | `	int bRenderingUncaught;     /* TRUE while the uncaught-exception report is being rendered. That` |
|         - | 3660 | `	                             * report asks the exception for its own trace (getTraceAsString,` |
|         - | 3661 | `	                             * userland code), so anything that throws in there would re-enter` |
|         - | 3662 | `	                             * the renderer and recurse until the process dies — which is exactly` |
|         - | 3663 | `	                             * what a bad max-arity stamp did (18 GB before the OOM killer, 19 Jul` |
|         - | 3664 | `	                             * 2026). The guard makes the second entry fall back to the` |
|         - | 3665 | `	                             * synthesized trace instead of looping. */` |
|         - | 3666 | `	int bCompilingBuiltin;      /* TRUE while the embedded builtin PHP library chunks compile at VM` |
|         - | 3667 | `	                             * init: classes/functions defined then are stamped INTERNAL so` |
|         - | 3668 | `	                             * Reflection reports isInternal() like Zend does for C-level code. */` |
|         - | 3669 | ``	int bSyntaxCheck;           /* TRUE for a `phl -l` compile (PH7_SYNTAX_CHECK): the unit is only`` |
|         - | 3670 | `	                             * ever PARSED, never run. A class declaration is then never` |
|         - | 3671 | `	                             * DEFERRED -- nothing autoloads here, so deferring would leave its` |
|         - | 3672 | `	                             * whole body unparsed and lint an unparsable file clean -- and the` |
|         - | 3673 | `	                             * refusals that only a resolved parent/interface/trait can answer` |
|         - | 3674 | `	                             * are not raised, because php binds inheritance at run time and` |
|         - | 3675 | ``	                             * `php -l` does not report those either. */`` |
|         - | 3676 | `	int bReflectBypass;         /* Consume-once: the next method OP_CALL skips the visibility` |
|         - | 3677 | `	                             * check (ReflectionMethod::invoke bypasses protection like PHP` |
|         - | 3678 | `	                             * 8.1+). Cleared by the check site; never survives past one call. */` |
|         - | 3679 | `	int bCallbackWeak;          /* Consume-once: the next OP_CALL is an INTERNAL function invoking a` |
|         - | 3680 | `	                             * userland callback (array_map, usort, an autoloader, a shutdown` |
|         - | 3681 | `	                             * function, Reflection's invoke, Closure::call), which php runs in` |
|         - | 3682 | `	                             * WEAK mode however strict the file that reached the builtin is —` |
|         - | 3683 | `	                             * there is no "calling file" at such a boundary. The two php` |
|         - | 3684 | `	                             * FORWARDS, call_user_func and call_user_func_array, do not set it:` |
|         - | 3685 | `	                             * they pass the caller's own mode on an argument map. Cleared at` |
|         - | 3686 | `	                             * the head of OP_CALL like the latches below. */` |
|         - | 3687 | `	int bHostDiscard;           /* The HOST (C) function now running was called from a statement` |
|         - | 3688 | `	                             * that throws its answer away. Set around the foreign-function` |
|         - | 3689 | `	                             * dispatch in OP_CALL from the instruction's bDiscard, and read` |
|         - | 3690 | `	                             * by exactly two builtins: php's two callback FORWARDS. */` |
|         - | 3691 | `	int bDiscardCallback;       /* Consume-once: the next call dispatched through` |
|         - | 3692 | `	                             * PH7_VmCallUserFunction inherits that drop, which is how` |
|         - | 3693 | ``	                             * `call_user_func('f');` warns for a #[\NoDiscard] `f` and`` |
|         - | 3694 | ``	                             * `array_map('f', $a);` does not (php special-cases the same two`` |
|         - | 3695 | `	                             * names at compile time). Consumed at the head of OP_CALL. */` |
|         - | 3696 | `	int bMagicDispatch;         /* Consume-once: the next method OP_CALL is the ENGINE reaching for` |
|         - | 3697 | `	                             * a magic method (PH7_VmCallMagicMethod), so the visibility check` |
|         - | 3698 | `	                             * lets a non-public one through — php only WARNS at such a` |
|         - | 3699 | `	                             * declaration and dispatches anyway. Set at the engine's own` |
|         - | 3700 | `	                             * dispatch sites only, so a call the USER wrote (including a` |
|         - | 3701 | ``	                             * first-class `$o->__get(...)`, which reaches the same C`` |
|         - | 3702 | `	                             * dispatcher) is still denied. Cleared at the head of OP_CALL. */` |
|         - | 3703 | `	int bClosureScreened;       /* Consume-once: the method call now being dispatched comes out of a` |
|         - | 3704 | `	                             * Closure whose callee was RESOLVED and screened when the closure was` |
|         - | 3705 | ``	                             * BUILT (`$this->p(...)`, `Closure::fromCallable([$this,'p'])`,`` |
|         - | 3706 | `	                             * ReflectionMethod::getClosure), so no site may re-decide its` |
|         - | 3707 | `	                             * visibility against the CALLER. php stores a resolved function +` |
|         - | 3708 | `	                             * scope in the Closure and never looks the name up again; PHL keeps a` |
|         - | 3709 | `	                             * name, so without this latch an escaped closure over a private method` |
|         - | 3710 | `	                             * died at the invocation php runs. Armed by VmClosureUnwrap, read by` |
|         - | 3711 | `	                             * the array-callable dispatch sites, cleared at the head of OP_CALL. */` |
|         - | 3712 | `	char zDefTz[68];            /* date_default_timezone_set() identifier, stored verbatim like php` |
|         - | 3713 | `	                             * (default "UTC"; only UTC/GMT are accepted — no tz database) */` |
|         - | 3714 | `	sxu32 nDefTz;               /* zDefTz length in bytes */` |
|         - | 3715 | `	phl_dt_lasterr sDtLastErr;  /* DateTime::getLastErrors()'s answer. Was a PHL-only` |
|         - | 3716 | ``	                             * `public static $__dtLastErr` on DateTime, a property php has`` |
|         - | 3717 | `	                             * no equivalent of; both date classes read this field now. */` |
|         - | 3718 | `	SySet aShutdown;            /* Stack of shutdown user callbacks */` |
|         - | 3719 | `	SySet aIniCli;              /* php.ini directives from the CLI (-d/-c): VmIniEntry copies,` |
|         - | 3720 | `	                             * merged into aIniTab when the directive table is seeded */` |
|         - | 3721 | `	SySet aIniTab;              /* The live directive table (VmIniSlot), sorted by name so` |
|         - | 3722 | `	                             * ini_get_all() needs no sort of its own */` |
|         - | 3723 | `	SySet aPersistSock;         /* PERSISTENT socket handles (VmPersistSock), keyed by the` |
|         - | 3724 | `	                             * address as the opener spelled it: php hands the SAME` |
|         - | 3725 | `	                             * resource back for a second pfsockopen() of one address */` |
|         - | 3726 | `	sxu8 bIniSeeded;            /* aIniTab has been built (lazily, on the first INI call) */` |
|         - | 3727 | `	int iPosixErr;              /* ext/posix's remembered errno: what` |
|         - | 3728 | `	                             * posix_get_last_error()/posix_errno() answer.` |
|         - | 3729 | `	                             * php keeps one per MODULE; per VM is the same` |
|         - | 3730 | `	                             * lifetime for a program and keeps two embedded` |
|         - | 3731 | `	                             * VMs apart. Nothing ever clears it -- a later` |
|         - | 3732 | `	                             * SUCCESS leaves the last failure standing,` |
|         - | 3733 | `	                             * which is php's own contract. */` |
|         - | 3734 | `	void *pSyslog;              /* ext/standard's syslog state (builtin_syslog.c owns the` |
|         - | 3735 | `	                             * shape): the prefix openlog() was given -- POSIX keeps the` |
|         - | 3736 | `	                             * POINTER, so it has to outlive the call -- and, on Windows,` |
|         - | 3737 | `	                             * the event-source handle a record is reported through.` |
|         - | 3738 | `	                             * Allocated on the first call, freed by PH7_SyslogVmRelease. */` |
|         - | 3739 | `	void *pPcntl;               /* ext/pcntl's per-VM state (builtin_pcntl.c owns the` |
|         - | 3740 | `	                             * shape): the handler each signal was last given, the` |
|         - | 3741 | `	                             * remembered errno and the async-dispatch flag. Allocated` |
|         - | 3742 | `	                             * lazily on the first call and freed by PH7_PcntlVmRelease,` |
|         - | 3743 | `	                             * which also puts every disposition this VM took over back` |
|         - | 3744 | `	                             * to SIG_DFL. */` |
|         - | 3745 | `	void *pGettext;             /* ext/gettext's per-VM state (builtin_gettext.c owns the` |
|         - | 3746 | `	                             * shape): the domain bindings, the current textdomain and` |
|         - | 3747 | `	                             * the catalog each domain last resolved. Allocated lazily` |
|         - | 3748 | `	                             * from sAllocator on the first call, and freed with it. */` |
|         - | 3749 | ``	/* Session state. Was a private `__SessS` class with five static properties, which`` |
|         - | 3750 | `	 * the INI subsystem had to reach into to live-wire session.name/session.save_path;` |
|         - | 3751 | `	 * both subsystems read these fields now, so neither depends on the other's shape. */` |
|         - | 3752 | `	sxi32 iSessStatus;          /* PHP_SESSION_NONE / _ACTIVE */` |
|         - | 3753 | `	SyBlob sSessId;             /* current session id ("" = none yet) */` |
|         - | 3754 | `	SyBlob sSessName;           /* cookie/session name (default "PHPSESSID") */` |
|         - | 3755 | `	SyBlob sSessPath;           /* save path ("" = not resolved yet -> sys_get_temp_dir()) */` |
|         - | 3756 | `	ph7_value sSessHandler;     /* session_set_save_handler(): the handler OBJECT, or the array` |
|         - | 3757 | `	                             * of callables the procedural form passes. NULL = the built-in` |
|         - | 3758 | ``	                             * `files` store. */`` |
|         - | 3759 | `	sxu8 bSessOpened;           /* a userland handler's open() has run for this session */` |
|         - | 3760 | `	SyBlob sSessData;           /* the payload the store last handed back or was handed: what` |
|         - | 3761 | `	                             * session.lazy_write compares the next write against */` |
|         - | 3762 | `	SyHash hWeakCell;           /* instance pointer bytes -> VmWeakCell* (weak-reference registry;` |
|         - | 3763 | `	                             * PH7_ClassInstanceRelease kills matching cells on free) */` |
|         - | 3764 | `	SyHash hDirHandle;          /* instance pointer bytes -> VmDirHandle* (the open DIR* behind a` |
|         - | 3765 | `	                             * DirectoryIterator; the class's xRelease closes and unregisters) */` |
|         - | 3766 | `	SySet aAutoload;            /* Stack of spl_autoload callbacks */` |
|         - | 3767 | `	SyBlob sAutoloadExt;        /* spl_autoload_extensions(): the comma-separated list` |
|         - | 3768 | `	                             * spl_autoload() tries when it is handed none.` |
|         - | 3769 | `	                             * php's own default is ".inc,.php" and the ORDER is` |
|         - | 3770 | `	                             * observable -- it is what decides which of two files` |
|         - | 3771 | `	                             * with the same base name defines the class. */` |
|         - | 3772 | `	SyHash hAutoloadActive;     /* Classes currently being autoloaded (reentrancy guard) */` |
|         - | 3773 | `	SyHash hTypedSlot;          /* memobj nIdx -> VmClassAttr* for every slot a store must be` |
|         - | 3774 | `	                             * FILTERED through: a declared TYPE to enforce, a native` |
|         - | 3775 | `	                             * class's write handler, or both (PH7_ATTR_STORE_FILTERED).` |
|         - | 3776 | `	                             * Registered and dropped through the two helpers below, which` |
|         - | 3777 | `	                             * are the only writers -- the predicate must not be spelled` |
|         - | 3778 | `	                             * out at a call site again. */` |
|         - | 3779 | `	unsigned char *pFilterBits; /* One BIT per memobj slot: is it in hTypedSlot? A store to a` |
|         - | 3780 | `	                             * property asks that on every write, and it is a hash of a` |
|         - | 3781 | `	                             * dense small INTEGER to hear "no" -- 9M of the engine's 225M` |
|         - | 3782 | `	                             * lookups on the ecosystem gate's phpcs step. The slot index` |
|         - | 3783 | `	                             * indexes this directly instead. Kept by the same two helpers` |
|         - | 3784 | `	                             * that own the table, so it cannot drift from it; a slot past` |
|         - | 3785 | `	                             * nFilterBits was never registered, which is the same answer. */` |
|         - | 3786 | `	sxu32 nFilterBits;          /* How many slots pFilterBits covers (0 = never allocated) */` |
|         - | 3787 | `	sxu8 bFilterBitsOff;        /* The bitmap could not be grown to cover a slot that IS` |
|         - | 3788 | `	                             * registered, so it can no longer answer for anything and` |
|         - | 3789 | `	                             * every question goes back to the table. Sticky, because a` |
|         - | 3790 | `	                             * fresh bitmap would be missing the bits of everything` |
|         - | 3791 | `	                             * registered before it. An allocation CAN fail here without` |
|         - | 3792 | `	                             * the box being out of memory -- a script's memory_limit is a` |
|         - | 3793 | `	                             * real ceiling since the 137th session -- and answering "not` |
|         - | 3794 | `	                             * filtered" there would silently skip a typed property's` |
|         - | 3795 | `	                             * type check, its readonly screen and a native write` |
|         - | 3796 | `	                             * handler. */` |
|         - | 3797 | `	sxu32 nNativeSetSlot;       /* How many of those slots carry a native WRITE HANDLER. Kept` |
|         - | 3798 | `	                             * by the same two helpers, and read by the in-place mutation` |
|         - | 3799 | ``	                             * opcodes: `$i++` on an ordinary variable must not pay for a`` |
|         - | 3800 | `	                             * hash lookup just because some class in the script declares a` |
|         - | 3801 | `	                             * typed property, and with no handler-backed slot alive there` |
|         - | 3802 | `	                             * is nothing for one to find. */` |
|         - | 3803 | `	SySet aException;           /* Stack of loaded exception */` |
|         - | 3804 | `	SySet aFinallyAction;       /* ROOT C: stack of VmFinallyAction — pending action (fallthrough /` |
|         - | 3805 | `	                             * rethrow / return / break-continue) for each inline finally in flight */` |
|         - | 3806 | `	ph7_class_instance *pPendingException; /* Exception deferred past a finally block */` |
|         - | 3807 | `	ph7_class_instance *pInflightException; /* Exception being unwound while a finally runs; a throw from` |
|         - | 3808 | `	                                         * that finally that escapes the finally chains it as $previous` |
|         - | 3809 | `	                                         * (PHP finally-supersede) */` |
|         - | 3810 | `	sxu32 nInflightExcBase;                 /* Exception-stack depth when the in-flight finally started; a throw` |
|         - | 3811 | `	                                         * is "leaving the finally" once the stack unwinds to/below this */` |
|         - | 3812 | `	/* The in-place-catch resume target (ROOT B). The four fields are ONE record and` |
|         - | 3813 | `	 * only mean anything together: a frame paired with another try's landing pad` |
|         - | 3814 | `	 * drains the operand stack to a foreign base and lands mid-statement. They are` |
|         - | 3815 | `	 * written, cleared, saved and restored only through VmSetResumeTarget /` |
|         - | 3816 | `	 * VmClearResumeTarget / VmSaveResumeTarget / VmRestoreResumeTarget — never one` |
|         - | 3817 | `	 * at a time. */` |
|         - | 3818 | `	VmFrame *pResumeFrame;      /* Body frame whose in-place catch consumed the live throw */` |
|         - | 3819 | `	sxu32 iResumePc;            /* Its post-try landing pad (1-based, as iExceptionJump) */` |
|         - | 3820 | `	void *pResumeInstr;         /* Bytecode array the catching try lives in; resume only in that exec */` |
|         - | 3821 | `	sxi32 iResumeStackDepth;    /* Operand-stack base (0-based TOS index) of the catching try, recorded` |
|         - | 3822 | `	                             * with the resume target. Used only by Generator::throw() inject-at-yield` |
|         - | 3823 | `	                             * to drain abandoned mid-expression operands before landing at iResumePc. */` |
|         - | 3824 | `	/* ROOT C inline redirect: set by VmThrowException when a throw is caught by an INLINE` |
|         - | 3825 | `	 * try (generator body). The throw site checks the pair (pInlineInstr, pInlineFrame)` |
|         - | 3826 | `	 * against its own (aInstr, pEntryFrame), drains the operand stack to iInlineDrain, and` |
|         - | 3827 | `	 * jumps to iInlinePc; a mismatch means another activation owns it, so the throw` |
|         - | 3828 | `	 * propagates. The bytecode array alone is NOT identity: two live activations of the` |
|         - | 3829 | `	 * same function share it, so a generator whose sibling activation owned the try` |
|         - | 3830 | `	 * consumed the redirect and ran that try's finally against its OWN variables (twig's` |
|         - | 3831 | ``	 * `Template::yieldBlock`, whose recursive delegation runs three activations of one`` |
|         - | 3832 | ``	 * method at once, read an unset `$level` there). The frame pins the activation, the`` |
|         - | 3833 | `	 * same pairing VmRecordedResume and OP_LOAD_EXCEPTION's activation match already use.` |
|         - | 3834 | `	 * Separate from the ROOT B fields above (legacy path). */` |
|         - | 3835 | `	void *pInlineInstr;         /* Owner bytecode array of the catching inline try (0 = none) */` |
|         - | 3836 | `	void *pInlineFrame;         /* Body frame that owns that try (the activation's identity) */` |
|         - | 3837 | `	sxu32 iInlinePc;            /* 0-based target pc (iHandlerPc or iFinallyPc) */` |
|         - | 3838 | `	sxi32 iInlineDrain;         /* Operand-stack base to drain to before landing (0-based TOS idx) */` |
|         - | 3839 | `	SySet aMagicGuard;          /* In-flight magic-accessor guard (php's property guard):` |
|         - | 3840 | `	                             * {instance, property-name hash, kind} entries pushed around a` |
|         - | 3841 | `	                             * __get dispatch so a self-recursive read of the same property` |
|         - | 3842 | `	                             * falls back to the undefined-property path instead of looping. */` |
|         - | 3843 | `	ph7_class_instance *pMagicSetThis; /* Pending __set receiver (band A #3b): OP_MEMBER detected a` |
|         - | 3844 | `	                             * plain store to a missing/inaccessible property whose class` |
|         - | 3845 | `	                             * declares __set; the VALUE only exists at the immediately-` |
|         - | 3846 | `	                             * following OP_STORE, which consumes this (with sMagicSetName)` |
|         - | 3847 | `	                             * and dispatches __set($name,$value). Holds a reference;` |
|         - | 3848 | `	                             * one-instruction lifetime by construction. */` |
|         - | 3849 | `	SyBlob sMagicSetName;       /* Pending __set property name (stable copy) */` |
|         - | 3850 | `	ph7_class_instance *pHookSetThis; /* Pending property-hook set receiver (PHP 8.4): OP_MEMBER` |
|         - | 3851 | `	                             * detected a plain store to a hooked property; the following` |
|         - | 3852 | `	                             * OP_STORE consumes this (with pHookSetAttr/nHookSetIdx) and` |
|         - | 3853 | `	                             * dispatches __phl_hook_set_NAME — or throws the read-only` |
|         - | 3854 | `	                             * Error when the property has no set hook. Owns one instance` |
|         - | 3855 | `	                             * reference while armed. */` |
|         - | 3856 | `	ph7_class_attr *pHookSetAttr; /* Pending hook-set property (declared attr; name + flags) */` |
|         - | 3857 | ``	sxu32 nHookSetIdx;          /* Pending hook-set BACKING slot index (for `set => expr`) */`` |
|         - | 3858 | ``	VmClassAttr *pRefTargetAttr; /* Pending reference-store target (`$o->p =& $x`): OP_MEMBER tagged`` |
|         - | 3859 | `	                             * PH7_MEMBER_REF_TARGET resolved the instance property slot and` |
|         - | 3860 | `	                             * stashed it here; the immediately-following member-marked` |
|         - | 3861 | `	                             * OP_STORE_REF rebinds it to alias the source variable's slot.` |
|         - | 3862 | `	                             * One-instruction lifetime by construction. */` |
|         - | 3863 | ``	ph7_class_attr *pRefTargetStaticAttr; /* Same, for a static-property target (`self::$s =& $x`). */`` |
|         - | 3864 | `	ph7_class_instance *pRefTargetThis;   /* Instance owning pRefTargetAttr; retained (iRef++) by` |
|         - | 3865 | `	                             * OP_MEMBER, released by the consuming OP_STORE_REF. */` |
|         - | 3866 | `	SySet aHookRmw;             /* Pending property-hook read-modify-write write-backs (LIFO;` |
|         - | 3867 | `	                             * VmHookRmw entries — see the struct above ph7_vm). */` |
|         - | 3868 | `	ph7_class_instance *pMagicCallThis; /* Pending __call receiver (band A #3b): OP_MEMBER hit a` |
|         - | 3869 | `	                             * missing (or inaccessible) method on a class declaring` |
|         - | 3870 | `	                             * __call/__callStatic and marked the callee slot` |
|         - | 3871 | `	                             * MEMOBJ_AUX_MAGICCALL; the packing body OP_CALL then runs` |
|         - | 3872 | `	                             * (VmMagicCallDispatch) consumes this + the class + the original` |
|         - | 3873 | `	                             * name. Holds a reference; NULL for __callStatic. */` |
|         - | 3874 | `	ph7_class *pMagicCallClass; /* Pending __call/__callStatic declaring class */` |
|         - | 3875 | `	ph7_class *pConstEvalClass; /* Transient: class whose constant/property initializer bytecode is` |
|         - | 3876 | `	                             * being evaluated (VmLocalExec has no method frame, so self::/parent::` |
|         - | 3877 | `	                             * inside an initializer resolve through this fallback — consulted by` |
|         - | 3878 | `	                             * PH7_VmPeekDeclaringClass/PH7_VmPeekTopClass when no frame matches). */` |
|         - | 3879 | `	void *pConstEvalFrame;      /* The VmFrame that was current when an ON-DEMAND const initializer` |
|         - | 3880 | `	                             * eval began (VmLocalExec pushes no frame). While the current frame` |
|         - | 3881 | `	                             * still equals it, self::/parent:: resolve to pConstEvalClass even` |
|         - | 3882 | `	                             * though an outer method frame exists (e.g. Base::CONST accessed from` |
|         - | 3883 | `	                             * Sub::method() must NOT resolve self to Sub). A method call inside the` |
|         - | 3884 | `	                             * initializer pushes a new frame, so the marker no longer matches and` |
|         - | 3885 | `	                             * that method's own declaring class wins. NULL outside on-demand eval. */` |
|         - | 3886 | `	sxi32 nConstEvalDepth;      /* Nesting depth of constant/enum-case initializer evaluations. A` |
|         - | 3887 | `	                             * cycle detected at an inner level (pConstCycleAttr) is thrown only` |
|         - | 3888 | `	                             * when depth returns to 0 — a throw INSIDE an initializer mini-exec` |
|         - | 3889 | `	                             * cannot be routed to a user catch (pre-existing engine restriction),` |
|         - | 3890 | `	                             * so the outermost, opcode-level evaluation raises it instead. */` |
|         - | 3891 | `	ph7_class_attr *pConstCycleAttr;  /* Self-referencing constant detected during evaluation */` |
|         - | 3892 | `	ph7_class *pConstCycleClass;      /* ...and the class it belongs to (for the Error message) */` |
|         - | 3893 | `	SyBlob sMagicCallName;      /* Pending original method name (stable copy) */` |
|         - | 3894 | `	ph7_user_func *pMagicCallFunc; /* The __call/__callStatic packing body's function record, built on` |
|         - | 3895 | `	                             * first use (PH7_VmMagicCallFunc) and NOT registered in` |
|         - | 3896 | `	                             * hHostFunction: OP_CALL points straight at it, so the dispatch has` |
|         - | 3897 | `	                             * no PHP-visible name to reach it by. */` |
|         - | 3898 | `	sxi32 nBoundaryRc;          /* C-boundary parked throw status (0 / PH7_EXCEPTION / PH7_ABORT).` |
|         - | 3899 | `	                             * Set by VmBoundaryPark when a PHP callee invoked from a C site` |
|         - | 3900 | `	                             * (magic method, cast hook, __destruct, user callback) raised and` |
|         - | 3901 | `	                             * that C site has no status channel to route it. Consumed once per` |
|         - | 3902 | `	                             * dispatch at the executor's fetch point (and cleared wherever the` |
|         - | 3903 | `	                             * same in-flight throw is landed via VmRecordedResume or the inline` |
|         - | 3904 | `	                             * redirect), so a swallowed throw outlives at most the C remainder` |
|         - | 3905 | `	                             * of one opcode instead of silently resuming execution. */` |
|         - | 3906 | `	SySet aIOstream;            /* Installed IO stream container */` |
|         - | 3907 | `	/* Devices a script has taken OUT of service with stream_wrapper_unregister().` |
|         - | 3908 | `	 * Held as DEVICE pointers rather than names, so a userland wrapper registered` |
|         - | 3909 | `	 * over an unregistered built-in coexists with it in the list above and is the` |
|         - | 3910 | `	 * one the lookup finds. */` |
|         - | 3911 | `	SySet aSuppressedIo;` |
|         - | 3912 | `	const ph7_io_stream *pDefStream; /* Default IO stream [i.e: typically this is the 'file://' stream] */` |
|         - | 3913 | `	ph7_value sExec;           /* Compiled script return value [Can be extracted via the PH7_VM_CONFIG_EXEC_VALUE directive]*/` |
|         - | 3914 | `	ph7_value sExceptionCB;    /* ACTIVE set_exception_handler() handler */` |
|         - | 3915 | `	ph7_value sErrCB;          /* ACTIVE set_error_handler() handler */` |
|         - | 3916 | `	sxi64 iErrCBLevels;        /* sErrCB's $error_levels mask: a handler is only called for the` |
|         - | 3917 | `	                            * levels it was REGISTERED for, and every other one falls` |
|         - | 3918 | `	                            * through to the engine's own reporting. Read at full width --` |
|         - | 3919 | `	                            * php ANDs a zend_long, so 2^32+1024 still selects` |
|         - | 3920 | `	                            * E_USER_NOTICE. */` |
|         - | 3921 | `	SySet aExceptionCBSaved;   /* VmHandlerSlot stack underneath sExceptionCB */` |
|         - | 3922 | `	SySet aErrCBSaved;         /* VmHandlerSlot stack underneath sErrCB */` |
|         - | 3923 | `	void *pStdin;              /* STDIN IO stream */` |
|         - | 3924 | `	void *pStdout;             /* STDOUT IO stream */` |
|         - | 3925 | `	void *pStderr;             /* STDERR IO stream */` |
|         - | 3926 | `	int bErrReport;            /* TRUE to report all runtime Error/Warning/Notice */` |
|         - | 3927 | `	int bDisplayErrors;        /* display_errors ini gate: TRUE emits the DISPLAY copy of a` |
|         - | 3928 | ``	                            * runtime diagnostic (`\nWarning: msg in F on line N`) to the`` |
|         - | 3929 | `	                            * program output stream (stdout). php CLI default: off. */` |
|         - | 3930 | `	int bLogErrors;            /* log_errors ini gate: TRUE emits the LOG copy of a runtime` |
|         - | 3931 | ``	                            * diagnostic (`PHP Warning:  msg in F on line N`) to the error`` |
|         - | 3932 | `	                            * stream (stderr via sVmErrConsumer). php CLI default: on. */` |
|         - | 3933 | `	int bGcEnabled;            /* gc_enable()/gc_disable(): whether the cycle collector may` |
|         - | 3934 | `	                            * buffer a possible root at all. Off means PHL frees by` |
|         - | 3935 | `	                            * reference count alone, which strands every cycle. */` |
|         - | 3936 | `	SySet aGcRoot;             /* Possible cycle roots: a container whose refcount dropped` |
|         - | 3937 | `	                            * without reaching zero. See vm_gc.c */` |
|         - | 3938 | `	SySet aGcWork;             /* Traversal worklist (VM-owned so a collection allocates` |
|         - | 3939 | `	                            * nothing per run) */` |
|         - | 3940 | `	SySet aGcAux;              /* ...and the one scan_black runs on, since it is entered` |
|         - | 3941 | `	                            * mid-drain of the primary */` |
|         - | 3942 | `	SySet aGcDead;             /* What the collect phase proved garbage */` |
|         - | 3943 | `	sxu8 bGcWanted;            /* The root buffer filled: collect at the next fetch point */` |
|         - | 3944 | `	sxu8 bGcRunning;           /* A collection is in flight; nothing may buffer or re-enter */` |
|         - | 3945 | `	SySet aDeadClosure;        /* Run-time closures whose last holder went: freed at the VM's` |
|         - | 3946 | `	                            * next fetch point rather than on the spot, because the drop` |
|         - | 3947 | `	                            * happens in the middle of a dispatch that is still about to` |
|         - | 3948 | `	                            * look the function up. See PH7_VmPurgeDeadClosures. */` |
|         - | 3949 | `	sxu8 bClosurePurge;        /* ...and whether that list has anything on it */` |
|         - | 3950 | `	sxu32 nGcThreshold;        /* Buffered roots that trigger a collection; adaptive (vm_gc.c) */` |
|         - | 3951 | `	sxu32 nGcRuns;             /* Collections run, for gc_status() */` |
|         - | 3952 | `	sxu32 nGcCollected;        /* Containers freed by them, for gc_status() */` |
|         - | 3953 | `	sxi32 iErrMask;      /* error_reporting() level. PH7 collapsed it to the bErrReport` |
|         - | 3954 | `	                      * boolean, so E_ALL & ~E_DEPRECATED still printed every` |
|         - | 3955 | `	                      * deprecation — any non-zero level meant "report all". */` |
|         - | 3956 | `	int nRecursionDepth;       /* Current PHP call depth (OP_CALL frames only) */` |
|         - | 3957 | `	int nErrSuppress;          /* '@' error-control depth: >0 means the diagnostics raised` |
|         - | 3958 | `	                            * while evaluating the suppressed expression are not printed` |
|         - | 3959 | `	                            * (a user error handler is still invoked, as in php). Nests. */` |
|         - | 3960 | `	int nMaxDepth;             /* Maximum PHP call depth; 0 == unbounded (the host` |
|         - | 3961 | `	                            * default: PHP frames are heap-bound since the` |
|         - | 3962 | `	                            * iterative executor, so recursion is limited by` |
|         - | 3963 | `	                            * memory like the main PHP engine). Embedders opt in` |
|         - | 3964 | `	                            * via PH7_VM_CONFIG_RECURSION_DEPTH. */` |
|         - | 3965 | `	int nVmExecDepth;          /* Live native VmByteCodeExec activations (C-stack guard;` |
|         - | 3966 | `	                            * see the VmByteCodeExec wrapper in vm.c) */` |
|         - | 3967 | `	int nMaxNativeDepth;       /* Maximum native VmByteCodeExec nesting (mini-programs,` |
|         - | 3968 | `	                            * C->PHP callbacks, ctx start/resume, eval/include) —` |
|         - | 3969 | `	                            * what actually protects the C stack now that PHP` |
|         - | 3970 | `	                            * recursion is iterative. Platform-sized default,` |
|         - | 3971 | `	                            * PH7_VM_CONFIG_NATIVE_DEPTH overrides. */` |
|         - | 3972 | `	void *pIdleCallFrames;     /* Freelist of VmCallFrame nodes (BYTECODE stage 2):` |
|         - | 3973 | `	                            * fixed-size, strictly LIFO per invocation — reusing` |
|         - | 3974 | `	                            * them skips a pool alloc/free round-trip per PHP` |
|         - | 3975 | `	                            * call (the measured trampoline overhead). Backing` |
|         - | 3976 | `	                            * memory is allocator-owned; freed wholesale. */` |
|         - | 3977 | `	/* Freelists of recycled operand-stack buffers (BYTECODE stage 7): a returning PHP` |
|         - | 3978 | `	 * call recycles its (tight-sized) operand stack here instead of freeing it, so a` |
|         - | 3979 | `	 * same-size call reuses it -- skipping the buffer alloc AND the per-slot init.` |
|         - | 3980 | `	 * Bounded by an entry count AND a total-slot budget; buffers are plain allocator` |
|         - | 3981 | `	 * blocks so cold/suspend/abort paths can still raw-free them.` |
|         - | 3982 | `	 *` |
|         - | 3983 | `	 * KEYED BY SIZE, because only an EXACT size is reusable. One list held every` |
|         - | 3984 | `	 * parked buffer and every call walked it looking for its own size: with the cap` |
|         - | 3985 | `	 * at 256 buffers that walk was 2.2% of a phpcs run, spent almost entirely on` |
|         - | 3986 | `	 * sizes the caller was never going to take. The size picks the chain now, so a` |
|         - | 3987 | `	 * call compares against the handful of buffers whose size ends in the same six` |
|         - | 3988 | `	 * bits instead of against all of them. */` |
|         - | 3989 | `	void *apIdleOperandStack[PH7_STACK_POOL_BUCKETS];` |
|         - | 3990 | `	int nIdleOperandStacks;    /* Buffers parked across every chain (cap: VM_STACK_POOL_MAX) */` |
|         - | 3991 | `	sxu32 nIdleOperandSlots;   /* Slots parked across those buffers. The pool's real cost is` |
|         - | 3992 | `	                            * memory, not entries, so this -- not the entry count alone --` |
|         - | 3993 | `	                            * is what bounds it (VM_STACK_POOL_SLOTS). */` |
|         - | 3994 | `	void *pIdleStackNodes;     /* Freelist of spare VmIdleStack nodes (BYTECODE stage 7b):` |
|         - | 3995 | `	                            * reused across recycle/reuse cycles so a parked buffer's` |
|         - | 3996 | `	                            * wrapper node isn't pool-alloc/freed per call (mirrors` |
|         - | 3997 | `	                            * pIdleCallFrames). Allocator-owned; freed wholesale. */` |
|         - | 3998 | `	int nObDepth;              /* Output handlers currently running (0 outside one) */` |
|         - | 3999 | `	sxu32 nObActive;           /* 1-based index of the buffer whose handler is running` |
|         - | 4000 | `	                            * (0 outside one). php truncates the ob stack at that` |
|         - | 4001 | `	                            * buffer for the duration: ob_get_level()/contents()/` |
|         - | 4002 | `	                            * length()/list_handlers() answer for IT, not for` |
|         - | 4003 | `	                            * whatever is stacked above it. */` |
|         - | 4004 | `	int bConstEnum;            /* Expanding constants to DESCRIBE them` |
|         - | 4005 | `	                            * (get_defined_constants): php reports a deprecated` |
|         - | 4006 | `	                            * constant when it is READ, and listing the table is` |
|         - | 4007 | `	                            * not a read. */` |
|         - | 4008 | `	int bObRefused;            /* An ob call refused from inside a handler ended the` |
|         - | 4009 | `	                            * request: that operation delivers nothing more. */` |
|         - | 4010 | `	VmFrame *pObFrame;         /* Frame that CALLED the running output handler. The` |
|         - | 4011 | `	                            * handler's own body runs in a deeper frame, so` |
|         - | 4012 | ``	                            * `nObDepth > 0 && pFrame != pObFrame` is "we are`` |
|         - | 4013 | `	                            * inside the handler" — and it stays false for the` |
|         - | 4014 | `	                            * in-place catch PHL runs, in the caller's frame,` |
|         - | 4015 | `	                            * when the handler throws. */` |
|         - | 4016 | `	int nExceptDepth;          /* Exception depth */` |
|         - | 4017 | `	int nExcCtorDepth;         /* Engine-raised throws whose exception __construct is running` |
|         - | 4018 | `	                            * (VmExcCtorEnter): caps the self-feeding case where building` |
|         - | 4019 | `	                            * an exception throws again. */` |
|         - | 4020 | `	sxu32 nLazyInitLine;       /* While a LAZY class initializer runs (a static property's` |
|         - | 4021 | `	                            * deferred default, a class constant's on-demand evaluation):` |
|         - | 4022 | `	                            * the line of the ACCESS that triggered it. A Throwable born` |
|         - | 4023 | `	                            * in the initializer's OWN bytecode is stamped with THIS line` |
|         - | 4024 | `	                            * rather than the initializer's, because that is where php` |
|         - | 4025 | `	                            * evaluates the expression. 0 = not in one, or the access site` |
|         - | 4026 | `	                            * was internal (prelude) code whose line means nothing in the` |
|         - | 4027 | `	                            * file the stamp names. See PH7_VmStampThrowableSite. */` |
|         - | 4028 | `	sxi32 nLazyInitDepth;      /* nVmExecDepth of that initializer's own activation. The` |
|         - | 4029 | `	                            * override applies at THIS depth only: anything the` |
|         - | 4030 | `	                            * initializer manages to call — an autoloader, a nested` |
|         - | 4031 | `	                            * constant's evaluation — runs its own lines and keeps them. */` |
|         - | 4032 | `	int nMuteThrow;            /* > 0 while an initializer runs MUTED (VmEvalDefaultMuted): an` |
|         - | 4033 | `	                            * uncaught throw runs no exception handler, prints no report and` |
|         - | 4034 | `	                            * leaves iExitStatus alone, because php has not reached that code` |
|         - | 4035 | `	                            * yet. Depth-counted (an initializer can mount another class). */` |
|         - | 4036 | `	int nSpeculative;          /* > 0 while a program is run only to LOOK at the value it would` |
|         - | 4037 | `	                            * produce (PH7_VmEvalConstExpr, which renders a parameter default` |
|         - | 4038 | `	                            * for a declaration message). php's own compiler folds such an` |
|         - | 4039 | `	                            * expression and gives up the moment evaluating it raises` |
|         - | 4040 | `	                            * ANYTHING, so nothing raised here may be observable: no user` |
|         - | 4041 | `	                            * error handler runs, no error_get_last() record is written and` |
|         - | 4042 | `	                            * nothing is printed. Depth-counted like nMuteThrow, which mutes` |
|         - | 4043 | `	                            * the THROW half of the same window. */` |
|         - | 4044 | `	sxu32 nSpecDiag;           /* Diagnostics dropped by nSpeculative, monotonic. A speculative` |
|         - | 4045 | `	                            * evaluation that moved this counter is one php would not have` |
|         - | 4046 | ``	                            * folded, so its caller renders php's `<expression>` instead. */`` |
|         - | 4047 | `	int closure_cnt;           /* Loaded closures counter */` |
|         - | 4048 | `	int json_rc;               /* JSON return status [refer to json_encode()/json_decode()]*/` |
|         - | 4049 | `	sxi32 iLcgS1;              /* php's combined LCG, the generator behind uniqid()'s $more_entropy` |
|         - | 4050 | `	                            * tail (and php's own lcg_value()). Two L'Ecuyer streams whose` |
|         - | 4051 | `	                            * DIFFERENCE is the answer; seeded lazily from the clock and the` |
|         - | 4052 | `	                            * engine's own entropy, once per VM, the way php seeds its pair` |
|         - | 4053 | `	                            * once per process. */` |
|         - | 4054 | `	sxi32 iLcgS2;` |
|         - | 4055 | `	int bLcgSeeded;            /* ...and whether that has happened yet */` |
|         - | 4056 | `	sxu32 nNextObjId;          /* Next object handle id to hand out (monotonic; reset to 1 per exec` |
|         - | 4057 | `	                            * so a reused VM looks like a fresh process). See ph7_class_instance.nObjId */` |
|         - | 4058 | `	ProcErrLog xErrLog;        /* error_log() consumer [refer to PH7_VM_CONFIG_ERR_LOG_HANDLER] */` |
|         - | 4059 | `	sxu32 nOutputLen;          /* Total number of generated output */` |
|         - | 4060 | `	ph7_output_consumer sVmConsumer; /* Registered output consumer callback */` |
|         - | 4061 | `	ph7_output_consumer sVmErrConsumer; /* Diagnostics (stderr) consumer [PH7_VM_CONFIG_ERR_STREAM].` |
|         - | 4062 | `	                            * When xConsumer is 0 the log copy falls back to sVmConsumer so` |
|         - | 4063 | `	                            * embedders that never wire a stderr stream still see diagnostics. */` |
|         - | 4064 | `	int iAssertFlags;          /* Assertion flags */` |
|         - | 4065 | `	ph7_value sAssertCallback; /* Callback to call on failed assertions */` |
|         - | 4066 | `	void **apRefObj;           /* Reference WORD per memory-object slot, INDEXED BY SLOT:` |
|         - | 4067 | `	                            * apRefObj[nIdx] describes the holders of aMemObj[nIdx], or` |
|         - | 4068 | `	                            * is 0 when nothing has ever been registered against it. A` |
|         - | 4069 | `	                            * slot index is already a dense small integer, so hashing it` |
|         - | 4070 | `	                            * bought nothing and cost a rehash of every record each time` |
|         - | 4071 | `	                            * the table doubled. See VM_REF_TAG_* for what a word says --` |
|         - | 4072 | `	                            * nearly every slot's answer fits in the word itself and` |
|         - | 4073 | `	                            * allocates no record at all. */` |
|         - | 4074 | `	sxu32 nRefSize;            /* apRefObj[] length, in slots */` |
|         - | 4075 | `	sxu32 nRefUsed;            /* Cells currently filled (a word or a record) */` |
|         - | 4076 | `	SySet aSelf;               /* 'self' stack used for static member access [i.e: self::MyConstant] */` |
|         - | 4077 | `	ph7_hashmap *pGlobal;      /* $GLOBALS hashmap */` |
|         - | 4078 | `	sxu32 nGlobalIdx;          /* $GLOBALS index */` |
|         - | 4079 | `	SySet aCallSite;           /* VmCallSite -- one per PH7_OP_CALL site that has run, holding` |
|         - | 4080 | `	                            * the function-table entry its callee name resolved to. Indexed` |
|         - | 4081 | `	                            * by VmInstr.nSite - 1, and claimed only by a site that actually` |
|         - | 4082 | `	                            * executes. */` |
|         - | 4083 | `	SyHash hCallName;          /* The callee names aCallSite records point at, interned. 43,375` |
|         - | 4084 | `	                            * call sites execute on the ecosystem gate's phpcs step and they` |
|         - | 4085 | `	                            * spell only a few thousand distinct names between them, so a` |
|         - | 4086 | `	                            * copy per SITE was 2.8 MB where a copy per NAME is a fifth of` |
|         - | 4087 | `	                            * one -- and the shared copy is the one already in cache when` |
|         - | 4088 | `	                            * the next site checks its own record. Keyed by the name BYTES` |
|         - | 4089 | `	                            * (case-sensitively: a site spells its callee the same way every` |
|         - | 4090 | `	                            * time), and the entry's key IS the interned copy. */` |
|         - | 4091 | `	sxu32 nFreeCallSite;       /* Head of aCallSite's free list (index + 1, 0 = empty). An` |
|         - | 4092 | `	                            * eval()/include compiles into a bytecode container that is` |
|         - | 4093 | `	                            * RELEASED when the chunk finishes, so the records its call` |
|         - | 4094 | `` 	                            * sites claimed go back here -- without it, `while(1) eval(...)` `` |
|         - | 4095 | `	                            * would grow aCallSite for ever. */` |
|         - | 4096 | `	sxu32 nCallableGen;        /* Bumped whenever the set of things a NAME can call changes --` |
|         - | 4097 | `	                            * a function, a class or a host function installed or removed.` |
|         - | 4098 | `	                            * PH7_OP_CALL_INIT stamps a call site it has screened with the` |
|         - | 4099 | `	                            * generation it screened at, so a site whose callee is a` |
|         - | 4100 | `	                            * compile-time constant asks the question once per generation` |
|         - | 4101 | `	                            * instead of once per call. Starts at 1: 0 is 'never screened'. */` |
|         - | 4102 | `	sxu32 nConstGen;           /* The same idea for the CONSTANT table: bumped whenever a name` |
|         - | 4103 | `	                            * is installed in or removed from hConstant. A PH7_OP_LOADC` |
|         - | 4104 | `	                            * site's answer can only change then -- both the constant it` |
|         - | 4105 | `	                            * resolved to and, for a namespaced site, WHICH of its two` |
|         - | 4106 | `	                            * candidate names won -- so a site stamped with this generation` |
|         - | 4107 | `	                            * skips the lookups. Starts at 1: 0 is 'never resolved'. */` |
|         - | 4108 | `	sxu32 nCurLine;            /* Line of the instruction currently executing (0 outside the` |
|         - | 4109 | `	                            * dispatch loop). Every runtime diagnostic, debug_backtrace()` |
|         - | 4110 | `	                            * and Throwable reads its line from here. */` |
|         - | 4111 | `	sxu8 bCurStrict;           /* strict_types mode of the unit that instruction came from,` |
|         - | 4112 | `	                            * published beside nCurLine. Read by the argument binder when` |
|         - | 4113 | `	                            * an OP_CALL carries no compiled call map — which is every` |
|         - | 4114 | `	                            * ENGINE-dispatched call (magic method, property hook), where` |
|         - | 4115 | `	                            * php still applies the calling file's mode. */` |
|         - | 4116 | `	sxi32 nLastErrType;        /* error_get_last(): severity of the last UNHANDLED diagnostic` |
|         - | 4117 | `	                            * (0 = none yet). php records one even when '@' or` |
|         - | 4118 | `	                            * error_reporting() hides it, but NOT when a user handler` |
|         - | 4119 | `	                            * claimed it by returning true. */` |
|         - | 4120 | `	sxu32 nLastErrLine;        /* ... its line */` |
|         - | 4121 | `	SyBlob sLastErrMsg;        /* ... its message */` |
|         - | 4122 | `	SyBlob sLastErrFile;       /* ... its file */` |
|         - | 4123 | `	char zDisplayName[256];    /* Scratch for PH7_VmFuncDisplayName: a closure's INTERNAL name is a` |
|         - | 4124 | `	                            * synthesized unique key ("[closure_3]"), but php shows` |
|         - | 4125 | `	                            * "{closure:file:line}". Valid until the next call. */` |
|         - | 4126 | `	sxu32 nSuperBaseline;      /* SySetUsed(aMemObj) snapshot taken in PH7_VmMakeReady` |
|         - | 4127 | `								* right before the superglobals are created. ph7_vm_reset()` |
|         - | 4128 | `								* releases and truncates aMemObj back to this watermark then` |
|         - | 4129 | `								* rebuilds the per-exec object graph, so a compiled VM can be` |
|         - | 4130 | `								* re-executed (compile-once / execute-many) without state` |
|         - | 4131 | `								* bleed or unbounded heap growth. */` |
|         - | 4132 | `	/* Index of the shared empty-string literal reserved at VM init */` |
|         - | 4133 | `	sxu32 nEmptyStringIdx;` |
|         - | 4134 | `	/* Argument-unpacking capture (PHP 8.1 named-parameter semantics for spreads).` |
|         - | 4135 | `	 * Populated by OP_SPREAD; CALL/NEW derive each call's own arg-count growth from` |
|         - | 4136 | `	 * these runs (VmSpreadOwnExtra) and replay the keys (VmBuildEffectiveArgMap),` |
|         - | 4137 | `	 * then consume this call's runs. See the VmSpreadRun/VmSpreadKey machinery in vm.c. */` |
|         - | 4138 | `	SySet aSpreadRun;          /* VmSpreadRun: one entry per expansion in the current arg list */` |
|         - | 4139 | `	sxu32 nSpreadCallBase;     /* Index into aSpreadRun of the first run owned by the CALL/NEW` |
|         - | 4140 | `	                            * currently dispatching (VmSpreadOwnExtra records it; the replay` |
|         - | 4141 | `	                            * and consume use it instead of an ambiguous pStart scan, which a` |
|         - | 4142 | ``	                            * zero-width `...[]` run sharing a nested call's base slot fooled) */`` |
|         - | 4143 | `	SySet aSpreadKey;          /* VmSpreadKey: one (off,len) per expanded element, in order */` |
|         - | 4144 | `	SyBlob sSpreadKeyBlob;     /* Backing bytes for the string keys referenced by aSpreadKey */` |
|         - | 4145 | `	SySet aEffArgName;         /* SyString: effective per-actual-slot arg names built at CALL */` |
|         - | 4146 | `	const char *zCmpRefusalClass; /* A native compare handler (ph7_class::xCmp) REFUSED the pair,` |
|         - | 4147 | `	                            * and this is the exception class it named -- php throws` |
|         - | 4148 | `	                            * DateException out of the DateTimeZone handler. Recorded rather` |
|         - | 4149 | `	                            * than raised because PH7_MemObjCmp has no throw boundary: it runs` |
|         - | 4150 | `	                            * under sort(), in_array() and max() as often as under an operator.` |
|         - | 4151 | `	                            * The sites that DO have one (the comparison opcodes, the switch` |
|         - | 4152 | `	                            * arm, the host-call boundary) raise it through` |
|         - | 4153 | `	                            * PH7_CmpRefusalRaise. FIRST refusal wins, like nBoundaryRc: a` |
|         - | 4154 | `	                            * driver that keeps comparing after one must not overwrite the` |
|         - | 4155 | `	                            * message the script will see. 0 when none is pending. */` |
|         - | 4156 | `	char zCmpRefusalMsg[160];  /* ...and its wording, copied out of the hook's context */` |
|         - | 4157 | `	sxi32 iCmpCallbackExc;     /* The dispatch STATUS a comparison callback did not return with` |
|         - | 4158 | `								* (PH7_EXCEPTION, or PH7_ABORT for an UNCAUGHT throw), so the` |
|         - | 4159 | `								* driver (usort/uasort/uksort and the array_udiff/` |
|         - | 4160 | `								* array_uintersect families) can abort and propagate exactly` |
|         - | 4161 | `								* it. Zero when no comparison raised; a comparator has no` |
|         - | 4162 | `								* status channel, so this latch is the only way out. */` |
|         - | 4163 | `	int iMbEncoding;           /* mbstring's internal encoding, an MB_ENC_* id from` |
|         - | 4164 | `								* builtin_mb.c; 0 is UTF-8, which is why zeroing the` |
|         - | 4165 | `								* VM leaves php's default in place. */` |
|         - | 4166 | `	sxu8 aMbDetectOrder[8];    /* mbstring's DETECT ORDER, as builtin_mb.c detect ids. It is` |
|         - | 4167 | ``	                            * what `mb_detect_encoding($s)` walks with no list of its`` |
|         - | 4168 | ``	                            * own, and what `mb_detect_order()` reads and writes. NOT`` |
|         - | 4169 | ``	                            * what the name `auto` means: that one is the LANGUAGE's`` |
|         - | 4170 | `	                            * default order (ASCII, UTF-8) whatever this holds --` |
|         - | 4171 | `	                            * probed, because the two read alike in the default state` |
|         - | 4172 | `	                            * and only diverge once a script has set an order. */` |
|         - | 4173 | `	sxu8 nMbDetectOrder;       /* how many of them; 0 at VM init means the default pair. */` |
|         - | 4174 | `	sxi32 iMbSubstitute;       /* mbstring's substitute code point ('?' at VM init;` |
|         - | 4175 | `								* 0 is a code point a script may really ask for). */` |
|         - | 4176 | `	sxu8 iMbSubstMode;         /* how it is written: builtin_mb.c's MB_SUBST_* — the` |
|         - | 4177 | `								* code point itself, nothing at all, or the U+/entity` |
|         - | 4178 | `								* spelling of what could not be represented. php keeps` |
|         - | 4179 | `								* the two apart, so setting "long" does not forget the` |
|         - | 4180 | `								* code point an error character still takes. */` |
|         - | 4181 | `	sxi32 iExitStatus;         /* Script exit status */` |
|         - | 4182 | `	sxu8 bHaltRequested;       /* Set by exit/die (OP_HALT or the builtin) so the halt` |
|         - | 4183 | `								* cascades out of nested execution units (include/require/` |
|         - | 4184 | `								* eval chunks) instead of hard-exiting the process; the` |
|         - | 4185 | `								* top-level executor then runs shutdown callbacks normally. */` |
|         - | 4186 | `	sxu8 bInReset;             /* Set while ph7_vm_reset() bulk-releases the per-exec` |
|         - | 4187 | `								* object pool. Suppresses user __destruct invocation during` |
|         - | 4188 | `								* that teardown: destructors would run arbitrary PHP against a` |
|         - | 4189 | `								* half-reset VM (reference table already gone, $GLOBALS` |
|         - | 4190 | `								* nulled). PH7 never ran` |
|         - | 4191 | `								* global-scope destructors before (release nuked the arena),` |
|         - | 4192 | `								* so this preserves prior semantics while staying crash-safe.` |
|         - | 4193 | `								* Engine-level instance memory is still reclaimed. */` |
|         - | 4194 | `	sxu8 bNoFrameLoc;          /* Set around a diagnostic raised with NO php frame under it.` |
|         - | 4195 | `								* php then has no file and no line to name and reports the` |
|         - | 4196 | ``								* location as `in Unknown on line 0` (its`` |
|         - | 4197 | `								* EG(current_execute_data) == NULL branch). See` |
|         - | 4198 | `								* VmDiagnosticWhere. */` |
|         - | 4199 | `	sxu8 bShutdownAborted;     /* Set when a destructor in the shutdown pass left an uncaught` |
|         - | 4200 | `								* throwable. php's phase runs under one zend_try, so the first` |
|         - | 4201 | `								* bailout abandons every destructor still owed -- the flag is` |
|         - | 4202 | `								* what carries that decision across the two passes. */` |
|         - | 4203 | `	sxu8 bInShutdownDtor;      /* Set while the shutdown destructor pass runs (php's` |
|         - | 4204 | `								* zend_call_destructors, between the shutdown callbacks and` |
|         - | 4205 | `								* the output-buffer flush). php reads this state as` |
|         - | 4206 | ``								* `EG(current_execute_data) == NULL`: a non-public __destruct`` |
|         - | 4207 | `								* reached with no PHP frame on the stack is not the Error a` |
|         - | 4208 | `								* running program gets but an E_WARNING that says the call was` |
|         - | 4209 | `								* ignored, and the object is left undestructed. */` |
|         - | 4210 | `	ph7_gen_state sCodeGen;    /* Code generator module */` |
|         - | 4211 | `	sxu32 nLastEvalErr;        /* Compile-error count of the most recent VmEvalChunk unit. Unlike` |
|         - | 4212 | `								* sCodeGen.nErr it survives the nested-compile state save/restore,` |
|         - | 4213 | `								* so VmExecDeferredClass can tell whether ITS chunk failed even` |
|         - | 4214 | `								* when the deferred declaration executes inside an outer compile` |
|         - | 4215 | `								* (an autoload-during-compile require). */` |
|         - | 4216 | `	SyString sDeferAnonName;   /* One-shot synthesized-name override for the next anonymous-class` |
|         - | 4217 | `								* compile: set by VmExecDeferredClass before re-compiling a` |
|         - | 4218 | ``								* deferred `new class ... {}` chunk so the runtime-installed`` |
|         - | 4219 | `								* class carries the SAME name the site's OP_NEW loads; consumed` |
|         - | 4220 | `								* (cleared) by PH7_CompileAnnonClass. {0,0} otherwise. */` |
|         - | 4221 | `	ph7_exec_ctx *pActiveCtx;  /* Currently executing fiber/generator context (NULL in normal code) */` |
|         - | 4222 | `	ph7_class_instance *pCurFiber; /* The Fiber whose body the running code is inside, or NULL --` |
|         - | 4223 | `	                            * php's EG(active_fiber), which is what Fiber::getCurrent()` |
|         - | 4224 | `	                            * answers. Distinct from pActiveCtx: that one is whatever` |
|         - | 4225 | `	                            * coroutine is executing (a GENERATOR started inside a fiber` |
|         - | 4226 | `	                            * is the active ctx while the fiber is still the current one),` |
|         - | 4227 | `	                            * and it names no object. Saved and restored around a fiber's` |
|         - | 4228 | `	                            * start/resume, so nesting is the call structure itself. */` |
|         - | 4229 | `	ph7_class *pFiberClass;    /* Cached Fiber class pointer for fast dispatch */` |
|         - | 4230 | `	ph7_class *pGeneratorClass; /* Cached Generator class pointer */` |
|         - | 4231 | `	ph7_class *pClosureClass;  /* Cached Closure class pointer (closures are instances of it) */` |
|         - | 4232 | `	ph7_class_instance *pClosureThis; /* Transient: bound $this for a bound PLAIN closure about to be` |
|         - | 4233 | `	                                   * invoked, set by VmClosureUnwrap, consumed (ref transferred) at` |
|         - | 4234 | `	                                   * the OP_CALL user-function frame setup. Owns one reference. */` |
|         - | 4235 | `	ph7_class *pClosureScope; /* Transient: bound $__scope class for the same bound PLAIN closure` |
|         - | 4236 | `	                           * (private/protected visibility override); consumed alongside pClosureThis. */` |
|         - | 4237 | `	ph7_class *pStdClass;      /* Cached stdClass pointer (target of (object) cast + dynamic props) */` |
|         - | 4238 | `	ph7_class *pIncClass;      /* Cached __PHP_Incomplete_Class pointer: unserialize()'s carrier for a` |
|         - | 4239 | `	                            * disallowed or unknown class. Every script-level property access or` |
|         - | 4240 | `	                            * method call on an instance is php's incomplete-object diagnostic` |
|         - | 4241 | `	                            * (PH7_VmIncompleteMsg); the engine itself reads hAttr freely. */` |
|         - | 4242 | `	ph7_class *pArrayAccessClass; /* Cached ArrayAccess interface pointer */` |
|         - | 4243 | `	ph7_class *pCountableClass;   /* Cached Countable interface pointer */` |
|         - | 4244 | `	ph7_class *pStringableClass;  /* Cached Stringable interface pointer */` |
|         - | 4245 | `	ph7_class *pJsonSerializableClass; /* Cached JsonSerializable interface pointer */` |
|         - | 4246 | `	ph7_class *pTraversableClass; /* Cached Traversable interface pointer (iterable type check) */` |
|         - | 4247 | `	/* Pending null-coalesce-assign target on an ArrayAccess subscript.` |
|         - | 4248 | `	 * Set by LOAD_IDX iP2=3 when the key is missing on an ArrayAccess` |
|         - | 4249 | `	 * object; consumed by NULLC_STORE so it can dispatch to offsetSet` |
|         - | 4250 | `	 * instead of writing through the (synthetic) pNos->nIdx. NULLC_STORE` |
|         - | 4251 | `	 * always clears it, matched or not. */` |
|         - | 4252 | `	ph7_class_instance *pCoalesceObj;` |
|         - | 4253 | `	ph7_value sCoalesceKey;` |
|         - | 4254 | `	int bCoalesceArmed;` |
|         - | 4255 | `#ifdef PH7_ENABLE_PCRE` |
|         - | 4256 | `	int iPcreLastError;        /* preg_last_error() return value */` |
|         - | 4257 | `#endif` |
|         - | 4258 | `#ifdef PH7_ENABLE_LIBXML` |
|         - | 4259 | `	SySet aLibxmlErr;          /* Queued phl_libxml_err entries (libxml_get_errors) */` |
|         - | 4260 | `	SyBlob sLibxmlPend;        /* libxml message text held back because it has no trailing` |
|         - | 4261 | `	                            * newline: php buffers such a fragment and prints it JOINED` |
|         - | 4262 | `	                            * to the next diagnostic, whenever that arrives (see` |
|         - | 4263 | `	                            * PH7_LibxmlCaptureEnd). Reset per request. */` |
|         - | 4264 | `	int bLibxmlInternalErr;    /* libxml_use_internal_errors(true) is active */` |
|         - | 4265 | `	void *pLibxmlLastErr;      /* phl_libxml_err* slot backing libxml_get_last_error */` |
|         - | 4266 | `	void *pXmlDocs;            /* phl_xmldoc registry chain; freed on reset/release */` |
|         - | 4267 | `	void *pXmlLimbo;           /* The OWNERLESS shell (phl_xmldoc with no xmlDoc): every` |
|         - | 4268 | ``	                            * constructed-but-never-adopted DOM node -- php's `new`` |
|         - | 4269 | ``	                            * DOMText('t')`, whose node has NO document until the first`` |
|         - | 4270 | `	                            * insertion adopts it -- parks on its orphan set, freed with` |
|         - | 4271 | `	                            * the registry chain it sits on. Lazily created by the DOM's` |
|         - | 4272 | `	                            * constructors; reset to 0 whenever the chain is freed. */` |
|         - | 4273 | `	void *pXmlWriters;         /* XMLWriter registry chain; freed on reset/release */` |
|         - | 4274 | `	void *pXmlParsers;         /* phl_xmlparser registry chain (ext/xml); freed on reset/release */` |
|         - | 4275 | `	void *pPdoConns;           /* phl_pdo registry chain (ext/pdo); freed on reset/release --` |
|         - | 4276 | `	                            * a sqlite3 handle lives outside SyMemBackend, so the` |
|         - | 4277 | `	                            * wholesale release would leak both it and the file lock */` |
|         - | 4278 | `	void *pSq3Conns;           /* phl_sq3 registry chain (ext/sqlite3); freed on reset/release.` |
|         - | 4279 | `	                            * A SEPARATE chain from pPdoConns: the two extensions share` |
|         - | 4280 | `	                            * libsqlite3 and nothing else -- different error model, different` |
|         - | 4281 | `	                            * open flags, different object -- so they own their handles apart */` |
|         - | 4282 | `	void *pCurlHandles;        /* phl_curl registry chain (ext/curl); freed on reset/release --` |
|         - | 4283 | `	                            * a CURL* lives outside SyMemBackend too, and holds a socket` |
|         - | 4284 | `	                            * and a connection cache with it */` |
|         - | 4285 | `	void *pCurlMultis;         /* phl_curlm registry chain (ext/curl); swept BEFORE` |
|         - | 4286 | `	                            * pCurlHandles, since a multi still holds the easy handles` |
|         - | 4287 | `	                            * that were added to it */` |
|         - | 4288 | `	void *pCurlShares;         /* phl_curlsh registry chain (ext/curl); swept AFTER` |
|         - | 4289 | `	                            * pCurlHandles, since a CURLSH an easy handle still names` |
|         - | 4290 | `	                            * refuses to be cleaned up */` |
|         - | 4291 | `	ph7_value sXmlEntLoader;   /* libxml_set_external_entity_loader()'s callable; NULL = default.` |
|         - | 4292 | `	                            * Stored and answered, never invoked: no PHL parse path loads an` |
|         - | 4293 | `	                            * external entity (php's sanitized defaults keep it off too) —` |
|         - | 4294 | `	                            * a recorded divergence. */` |
|         - | 4295 | `	ph7_value sXmlStreamsCtx;  /* libxml_set_streams_context()'s stream-context resource; read by` |
|         - | 4296 | `	                            * nothing until an http:// wrapper exists. */` |
|         - | 4297 | `#endif` |
|         - | 4298 | `	void *pPhars;              /* phl_phar registry chain (ext/phar): every archive this run` |
|         - | 4299 | `	                            * opened, freed on reset/release. php's own cache is` |
|         - | 4300 | `	                            * per-request and behaves the same way. */` |
|         - | 4301 | `	void *pZips;               /* phl_zip registry chain (ext/zip): every archive a ZipArchive` |
|         - | 4302 | ``	                            * or a `zip://` open is holding, freed on reset/release */`` |
|         - | 4303 | `	void *pLastDir;            /* php's "last opened directory stream": the io_private the` |
|         - | 4304 | `	                            * most recent opendir() handed out, which readdir(),` |
|         - | 4305 | `	                            * rewinddir() and closedir() fall back to when they are` |
|         - | 4306 | `	                            * given null (deprecated since 8.1). Cleared when THAT` |
|         - | 4307 | `	                            * handle is closed and at reset; never owns anything. */` |
|         - | 4308 | `	SyBlob sPharRunning;       /* The archive the running script came from, as Phar::running()` |
|         - | 4309 | `	                            * answers it: set by Phar::mapPhar(), empty outside one. */` |
|         - | 4310 | `#ifdef PH7_ENABLE_NET` |
|         - | 4311 | `	void *pSockets;            /* phl_socket registry chain (ext/sockets); freed on reset/release` |
|         - | 4312 | `	                            * -- a DESCRIPTOR is not the allocator's, so the wholesale` |
|         - | 4313 | `	                            * release would leak the file handle and its port */` |
|         - | 4314 | `	void *pAddrInfos;          /* phl_addrinfo registry chain (ext/sockets), same rule: each` |
|         - | 4315 | `	                            * record holds a copied ai_canonname of its own */` |
|         - | 4316 | `	int iSocketLastErr;        /* php's SOCKETS_G(last_error): the per-REQUEST errno` |
|         - | 4317 | `	                            * socket_last_error() answers with no argument, beside the` |
|         - | 4318 | `	                            * per-socket one every record carries */` |
|         - | 4319 | `#endif` |
|         - | 4320 | `	SyBlob sPharErr;           /* The phar wrapper's open-failure sentence. It has to outlive the` |
|         - | 4321 | `	                            * xOpen that formatted it -- the engine keeps the POINTER and the` |
|         - | 4322 | `	                            * caller prints it after the open returned -- so it cannot be a` |
|         - | 4323 | `	                            * stack buffer (ASan caught exactly that). */` |
|         - | 4324 | `#ifdef PH7_ENABLE_ZLIB` |
|         - | 4325 | `	void *pZlibCtx;            /* phl_zctx registry chain (ext/zlib); freed on reset/release --` |
|         - | 4326 | `	                            * a z_stream's window is libz's own allocation, outside` |
|         - | 4327 | `	                            * SyMemBackend, so the wholesale release would leak it */` |
|         - | 4328 | `	int iZlibLevel;            /* the compression level the NEXT compress.zlib open uses, and` |
|         - | 4329 | `	                            * the strategy with it: gzopen()'s mode string carries both` |
|         - | 4330 | `	                            * ("wb9f") and an xOpen is handed flags rather than the string,` |
|         - | 4331 | `	                            * so the door that parsed them arms them here. Reset to libz's` |
|         - | 4332 | `	                            * defaults by the open that reads them. */` |
|         - | 4333 | `	int iZlibStrategy;` |
|         - | 4334 | `	int bZlibDirect;           /* 1 while a gzopen()-family open is in flight. The two doors` |
|         - | 4335 | `	                            * onto this device report a failure differently: gzopen() reads` |
|         - | 4336 | `	                            * as the FILE open it is ("No such file or directory"), while` |
|         - | 4337 | `	                            * compress.zlib:// is a wrapper and php gives every one of its` |
|         - | 4338 | `	                            * failures the same flat "operation failed". */` |
|         - | 4339 | `#endif` |
|         - | 4340 | `#ifdef PH7_ENABLE_OPENSSL` |
|         - | 4341 | `	void *pSslObjs;            /* phl_ssl_obj registry chain (ext/openssl); freed on reset/release` |
|         - | 4342 | `	                            * -- an X509/EVP_PKEY/X509_REQ is OpenSSL's own allocation, outside` |
|         - | 4343 | `	                            * SyMemBackend, so the wholesale release would leak it */` |
|         - | 4344 | `	void *pSslErrors;          /* phl_ssl_errors: php's 16-slot ring, drained from OpenSSL's own` |
|         - | 4345 | `	                            * error queue after a failure and read one entry at a time by` |
|         - | 4346 | `	                            * openssl_error_string() */` |
|         - | 4347 | `#endif` |
|         - | 4348 | `	/* php numbers every resource with a small sequential id that (int) casts and` |
|         - | 4349 | `	 * "Resource id #N" render, and that distinguishes two live resources from one` |
|         - | 4350 | `	 * another. PHL's resource value is a bare void*, so the id lives in this` |
|         - | 4351 | `	 * per-VM registry: pointer -> phl_res_id, assigned on first observation.` |
|         - | 4352 | `	 * Freed with the VM (ids are never recycled, as php's may be). */` |
|         - | 4353 | `	SyHash hResourceId;        /* void* -> phl_res_id* */` |
|         - | 4354 | `	sxu32 nResourceIdNext;     /* Next id to hand out (php's start at 1) */` |
|         - | 4355 | `	/* Stream contexts (stream_context_create). The chain owns every context the` |
|         - | 4356 | `	 * script made; pDefaultCtx is the one stream_context_get_default() hands` |
|         - | 4357 | `	 * back and every opener falls back to. */` |
|         - | 4358 | `	void *pStreamCtx;          /* phl_stream_ctx registry chain; freed on reset */` |
|         - | 4359 | `	void *pDefaultCtx;         /* phl_stream_ctx* — the default context, or 0 */` |
|         - | 4360 | `	void *pOpenCtx;            /* the context the open in flight runs under */` |
|         - | 4361 | `	char zOpenMode[16];        /* the mode string the open in flight was ASKED with, when a` |
|         - | 4362 | `	                            * caller had one: php hands a userland wrapper's stream_open()` |
|         - | 4363 | `	                            * the caller's own spelling ('rb', 'w+', 'x'), and PHL could` |
|         - | 4364 | `	                            * only rebuild an approximation from the flag bits -- so` |
|         - | 4365 | `	                            * file_put_contents() told a wrapper it was opening for` |
|         - | 4366 | `	                            * READING. Empty when the opener has no string of its own` |
|         - | 4367 | `	                            * (the C-level readers), and cleared after every open. */` |
|         - | 4368 | `	/* What a FAILED open says. php names the URI the script wrote -- scheme and` |
|         - | 4369 | `	 * all -- and gives the WRAPPER's reason for it, where only the plain-file` |
|         - | 4370 | `	 * wrapper's reason is an errno. PH7_VmGetStreamDevice() advances past the` |
|         - | 4371 | `	 * scheme, so the two halves of the name are remembered here as it does:` |
|         - | 4372 | `	 * zOpenUriTail is the pointer it handed back, and a warning printing THAT` |
|         - | 4373 | `	 * pointer is reporting THIS open and may name the whole thing instead. */` |
|         - | 4374 | `	const char *zOpenUri;      /* the URI as written, or 0 */` |
|         - | 4375 | `	int nOpenUri;              /* its length */` |
|         - | 4376 | `	const char *zOpenUriTail;  /* the scheme-stripped remainder handed to the wrapper */` |
|         - | 4377 | `	const char *zOpenCaller;   /* the FUNCTION reporting this open, for a wrapper that` |
|         - | 4378 | `	                            * raises a diagnostic of its own before the caller's` |
|         - | 4379 | `	                            * (php's resolver failure is two warnings, not one) */` |
|         - | 4380 | `	const char *zOpenErr;      /* the wrapper's own reason for the open in flight, or 0` |
|         - | 4381 | `	                            * for the plain-file wrapper's errno */` |
|         - | 4382 | `	char zOpenErrBuf[512];     /* storage for a reason that has to be BUILT -- a userland` |
|         - | 4383 | `	                            * wrapper names its own class and method, and the http` |
|         - | 4384 | `	                            * wrapper interpolates a host name or a whole status` |
|         - | 4385 | `	                            * line -- since the caller's buffer does not outlive the` |
|         - | 4386 | `	                            * call */` |
|         - | 4387 | `	int nOpenDepth;            /* opens in flight. The three fields above belong to the` |
|         - | 4388 | `	                            * OUTERMOST one: php://filter opens its own resource from` |
|         - | 4389 | `	                            * inside its xOpen, and that inner open would otherwise` |
|         - | 4390 | `	                            * report the RESOURCE's errno under the filter's name --` |
|         - | 4391 | `	                            * and leave zOpenUriTail pointing into a blob it frees on` |
|         - | 4392 | `	                            * the way out. */` |
|         - | 4393 | `	/* The response headers of the last http:// exchange, one line per '\n'. Two` |
|         - | 4394 | ``	 * consumers outlive the handle that produced them: `$http_response_header`,`` |
|         - | 4395 | `	 * which the stream layer writes into the frame that called the opener, and` |
|         - | 4396 | `	 * php 8.4's http_get_last_response_headers(), which answers them until` |
|         - | 4397 | `	 * http_clear_last_response_headers() drops the store. */` |
|         - | 4398 | `	SyBlob sHttpRespHdrs;      /* the lines, '\n'-separated */` |
|         - | 4399 | `	sxu8 bHttpRespHdrs;        /* something has been recorded (the getter's NULL/array split) */` |
|         - | 4400 | `	sxu8 bHttpRespFresh;       /* recorded by the open in flight and not yet published */` |
|         - | 4401 | `	sxu8 bHttpGetHeaders;      /* the open in flight is get_headers()', which php makes` |
|         - | 4402 | ``	                            * two things at once: a context with `ignore_errors` on,`` |
|         - | 4403 | `	                            * so a refused status is an ordinary set of headers, and` |
|         - | 4404 | `	                            * STREAM_ONLY_GET_HEADERS, which skips the dechunk filter` |
|         - | 4405 | `	                            * and so KEEPS the Transfer-Encoding header the ordinary` |
|         - | 4406 | `	                            * read consumes */` |
|         - | 4407 | `	/* Stream filters (stream_filter_append and the php://filter wrapper). The` |
|         - | 4408 | `	 * chain owns every filter INSTANCE the script created, so one that is never` |
|         - | 4409 | `	 * removed still goes back at reset. */` |
|         - | 4410 | `	void *pStreamFilter;       /* phl_stream_filter registry chain; freed on reset */` |
|         - | 4411 | `	void *pUserFilters;        /* stream_filter_register() name => class chain */` |
|         - | 4412 | ``	void *pFilterCall;         /* phl_brigade_res* — the `$out` of the filter() call`` |
|         - | 4413 | `	                            * in flight, which is what stream_bucket_new()` |
|         - | 4414 | `	                            * hangs its token on */` |
|         - | 4415 | `	SyString *pCalleeName;     /* the builtin currently running, for diagnostics` |
|         - | 4416 | `	                            * raised where no ph7_context reaches (see vm_exec.c) */` |
|         - | 4417 | `	ph7_vm *pNext,*pPrev;      /* List of active VM's */` |
|         - | 4418 | `	sxu32 nMagic;              /* Sanity check against misuse */` |
|         - | 4419 | `};` |
|         - | 4420 | `/*` |
|         - | 4421 | ` * Allowed value for ph7_vm.nMagic` |
|         - | 4422 | ` */` |
|         - | 4423 | `#define PH7_VM_INIT   0xFADE9512  /* VM correctly initialized */` |
|         - | 4424 | `#define PH7_VM_RUN    0xEA271285  /* VM ready to execute PH7 bytecode */` |
|         - | 4425 | `#define PH7_VM_EXEC   0xCAFE2DAD  /* VM executing PH7 bytecode */` |
|         - | 4426 | `#define PH7_VM_STALE  0xBAD1DEAD  /* Stale VM */` |
|         - | 4427 | `/*` |
|         - | 4428 | ` * Error codes according to the PHP language reference manual.` |
|         - | 4429 | ` */` |
|         - | 4430 | `enum iErrCode` |
|         - | 4431 | `{` |
|         - | 4432 | `	E_ERROR             = 1,   /* Fatal run-time errors. These indicate errors that can not be recovered` |
|         - | 4433 | `							    * from, such as a memory allocation problem. Execution of the script is` |
|         - | 4434 | `							    * halted.` |
|         - | 4435 | `								* The only fatal error under PH7 is an out-of-memory. All others erros` |
|         - | 4436 | `								* even a call to undefined function will not halt script execution.` |
|         - | 4437 | `							    */` |
|         - | 4438 | `	E_WARNING           = 2,   /* Run-time warnings (non-fatal errors). Execution of the script is not halted.  */` |
|         - | 4439 | `	E_PARSE             = 4,   /* Compile-time parse errors. Parse errors should only be generated by the parser.*/` |
|         - | 4440 | `	E_NOTICE            = 8,   /* Run-time notices. Indicate that the script encountered something that could` |
|         - | 4441 | `							    * indicate an error, but could also happen in the normal course of running a script.` |
|         - | 4442 | `							    */` |
|         - | 4443 | `	E_CORE_WARNING      = 16,  /* Fatal errors that occur during PHP's initial startup. This is like an E_ERROR` |
|         - | 4444 | `							    * except it is generated by the core of PHP.` |
|         - | 4445 | `							    */` |
|         - | 4446 | `	E_USER_ERROR        = 256,  /* User-generated error message.*/` |
|         - | 4447 | `	E_USER_WARNING      = 512,  /* User-generated warning message.*/` |
|         - | 4448 | `	E_USER_NOTICE       = 1024, /* User-generated notice message.*/` |
|         - | 4449 | `	E_STRICT            = 2048, /* Enable to have PHP suggest changes to your code which will ensure the best interoperability` |
|         - | 4450 | `								 * and forward compatibility of your code.` |
|         - | 4451 | `								 */` |
|         - | 4452 | `	E_RECOVERABLE_ERROR = 4096, /* Catchable fatal error. It indicates that a probably dangerous error occured, but did not` |
|         - | 4453 | `								 * leave the Engine in an unstable state. If the error is not caught by a user defined handle` |
|         - | 4454 | `								 * the application aborts as it was an E_ERROR.` |
|         - | 4455 | `								 */` |
|         - | 4456 | `	E_DEPRECATED        = 8192, /* Run-time notices. Enable this to receive warnings about code that will not` |
|         - | 4457 | `								 * work in future versions.` |
|         - | 4458 | `								 */` |
|         - | 4459 | `	E_USER_DEPRECATED   = 16384, /* User-generated warning message. */` |
|         - | 4460 | `	E_ALL               = 32767  /* All errors and warnings */` |
|         - | 4461 | `};` |
|         - | 4462 | `/*` |
|         - | 4463 | ` * Each VM instruction resulting from compiling a PHP script is represented` |
|         - | 4464 | ` * by one of the following OP codes.` |
|         - | 4465 | ` * The program consists of a linear sequence of operations. Each operation` |
|         - | 4466 | ` * has an opcode and 3 operands.Operands P1 is an integer.` |
|         - | 4467 | ` * Operand P2 is an unsigned integer and operand P3 is a memory address.` |
|         - | 4468 | ` * Few opcodes use all 3 operands.` |
|         - | 4469 | ` */` |
|         - | 4470 | `enum ph7_vm_op {` |
|         - | 4471 | `  PH7_OP_DONE =   1,   /* Done */` |
|         - | 4472 | `  PH7_OP_HALT,         /* Halt */` |
|         - | 4473 | `  PH7_OP_LOAD,         /* Load memory object */` |
|         - | 4474 | `  PH7_OP_LOADC,        /* Load constant */` |
|         - | 4475 | `  PH7_OP_LOAD_IDX,     /* Load array entry */` |
|         - | 4476 | `  PH7_OP_LOAD_MAP,     /* Load hashmap('array') */` |
|         - | 4477 | `  PH7_OP_LOAD_LIST,    /* Load list */` |
|         - | 4478 | `  PH7_OP_LOAD_CLOSURE, /* Load closure */` |
|         - | 4479 | `  PH7_OP_LOAD_FCC,     /* Load first-class callable: wrap a function/method as a Closure */` |
|         - | 4480 | `  PH7_OP_NOOP,         /* NOOP */` |
|         - | 4481 | `  PH7_OP_JMP,          /* Unconditional jump */` |
|         - | 4482 | `  PH7_OP_JZ,           /* Jump on zero (FALSE jump) */` |
|         - | 4483 | `  PH7_OP_JNZ,          /* Jump on non-zero (TRUE jump) */` |
|         - | 4484 | `  PH7_OP_POP,          /* Stack POP */` |
|         - | 4485 | `  PH7_OP_CAT,          /* Concatenation */` |
|         - | 4486 | `  PH7_OP_CVT_INT,      /* Integer cast */` |
|         - | 4487 | `  PH7_OP_CVT_STR,      /* String cast */` |
|         - | 4488 | `  PH7_OP_CVT_REAL,     /* Float cast */` |
|         - | 4489 | `  PH7_OP_CALL,         /* Function call */` |
|         - | 4490 | `  PH7_OP_UMINUS,       /* Unary minus '-'*/` |
|         - | 4491 | `  PH7_OP_UPLUS,        /* Unary plus '+'*/` |
|         - | 4492 | `  PH7_OP_BITNOT,       /* Bitwise not '~' */` |
|         - | 4493 | `  PH7_OP_LNOT,         /* Logical not '!' */` |
|         - | 4494 | `  PH7_OP_MUL,          /* Multiplication '*' */` |
|         - | 4495 | `  PH7_OP_DIV,          /* Division '/' */` |
|         - | 4496 | `  PH7_OP_MOD,          /* Modulus '%' */` |
|         - | 4497 | `  PH7_OP_POW,          /* Exponentiation '**' */` |
|         - | 4498 | `  PH7_OP_ADD,          /* Add '+' */` |
|         - | 4499 | `  PH7_OP_SUB,          /* Sub '-' */` |
|         - | 4500 | `  PH7_OP_SHL,          /* Left shift '<<' */` |
|         - | 4501 | `  PH7_OP_SHR,          /* Right shift '>>' */` |
|         - | 4502 | `  PH7_OP_LT,           /* Less than '<' */` |
|         - | 4503 | `  PH7_OP_LE,           /* Less or equal '<=' */` |
|         - | 4504 | `  PH7_OP_GT,           /* Greater than '>' */` |
|         - | 4505 | `  PH7_OP_GE,           /* Greater or equal '>=' */` |
|         - | 4506 | `  PH7_OP_SPACESHIP,    /* Spaceship '<=>' */` |
|         - | 4507 | `  PH7_OP_EQ,           /* Equal '==' */` |
|         - | 4508 | `  PH7_OP_NEQ,          /* Not equal '!=' */` |
|         - | 4509 | `  PH7_OP_TEQ,          /* Type equal '===' */` |
|         - | 4510 | `  PH7_OP_TNE,          /* Type not equal '!==' */` |
|         - | 4511 | `  PH7_OP_BAND,         /* Bitwise and '&' */` |
|         - | 4512 | `  PH7_OP_BXOR,         /* Bitwise xor '^' */` |
|         - | 4513 | `  PH7_OP_BOR,          /* Bitwise or '\|' */` |
|         - | 4514 | `  PH7_OP_LAND,         /* Logical and '&&','and' */` |
|         - | 4515 | `  PH7_OP_LOR,          /* Logical or  '\|\|','or' */` |
|         - | 4516 | `  PH7_OP_LXOR,         /* Logical xor 'xor' */` |
|         - | 4517 | `  PH7_OP_STORE,        /* Store Object */` |
|         - | 4518 | `  PH7_OP_STORE_IDX,    /* Store indexed object */` |
|         - | 4519 | `  PH7_OP_STORE_IDX_REF,/* Store indexed object by reference */` |
|         - | 4520 | `  PH7_OP_PULL,         /* Stack pull */` |
|         - | 4521 | `  PH7_OP_SWAP,         /* Stack swap */` |
|         - | 4522 | `  PH7_OP_YIELD,        /* Stack yield */` |
|         - | 4523 | `  PH7_OP_YIELD_FROM,   /* Generator delegation (yield from <iterable>) */` |
|         - | 4524 | `  PH7_OP_CVT_BOOL,     /* Boolean cast */` |
|         - | 4525 | `  PH7_OP_CVT_NUMC,     /* Numeric (integer,real or both) type cast */` |
|         - | 4526 | `  PH7_OP_INCR,         /* Increment ++ */` |
|         - | 4527 | `  PH7_OP_DECR,         /* Decrement -- */` |
|         - | 4528 | `  PH7_OP_NEW,          /* new */` |
|         - | 4529 | `  PH7_OP_CLONE,        /* clone */` |
|         - | 4530 | `  PH7_OP_ADD_STORE,    /* Add and store '+=' */` |
|         - | 4531 | `  PH7_OP_SUB_STORE,    /* Sub and store '-=' */` |
|         - | 4532 | `  PH7_OP_MUL_STORE,    /* Mul and store '*=' */` |
|         - | 4533 | `  PH7_OP_DIV_STORE,    /* Div and store '/=' */` |
|         - | 4534 | `  PH7_OP_MOD_STORE,    /* Mod and store '%=' */` |
|         - | 4535 | `  PH7_OP_POW_STORE,    /* Pow and store '**=' */` |
|         - | 4536 | `  PH7_OP_CAT_STORE,    /* Cat and store '.=' */` |
|         - | 4537 | `  PH7_OP_SHL_STORE,    /* Shift left and store '>>=' */` |
|         - | 4538 | `  PH7_OP_SHR_STORE,    /* Shift right and store '<<=' */` |
|         - | 4539 | `  PH7_OP_BAND_STORE,   /* Bitand and store '&=' */` |
|         - | 4540 | `  PH7_OP_BOR_STORE,    /* Bitor and store '\|=' */` |
|         - | 4541 | `  PH7_OP_BXOR_STORE,   /* Bitxor and store '^=' */` |
|         - | 4542 | `  PH7_OP_CONSUME,      /* Consume VM output */` |
|         - | 4543 | `  PH7_OP_LOAD_REF,     /* Load reference */` |
|         - | 4544 | `  PH7_OP_STORE_REF,    /* Store a reference to a variable*/` |
|         - | 4545 | `  PH7_OP_MEMBER,       /* Class member run-time access */` |
|         - | 4546 | `  PH7_OP_UPLINK,       /* Run-Time frame link */` |
|         - | 4547 | `  PH7_OP_CVT_NULL,     /* NULL cast */` |
|         - | 4548 | `  PH7_OP_CVT_ARRAY,    /* Array cast */` |
|         - | 4549 | `  PH7_OP_CVT_OBJ,      /* Object cast */` |
|         - | 4550 | `  PH7_OP_FOREACH_INIT, /* For each init */` |
|         - | 4551 | `  PH7_OP_FOREACH_STEP, /* For each step */` |
|         - | 4552 | `  PH7_OP_IS_A,         /* Instanceof */` |
|         - | 4553 | `  PH7_OP_LOAD_EXCEPTION,/* Load an exception */` |
|         - | 4554 | `  PH7_OP_POP_EXCEPTION, /* POP an exception */` |
|         - | 4555 | `  PH7_OP_THROW,         /* Throw exception */` |
|         - | 4556 | `  PH7_OP_SWITCH,        /* Switch operation */` |
|         - | 4557 | `  PH7_OP_MATCH,         /* Match expression (PHP 8.0) */` |
|         - | 4558 | `  PH7_OP_ERR_CTRL,     /* Error control */` |
|         - | 4559 | `  PH7_OP_DUP,          /* Duplicate top of stack */` |
|         - | 4560 | `  PH7_OP_NULLC,         /* Null coalescing ?? */` |
|         - | 4561 | `  PH7_OP_NULLC_JMP,     /* Null coalescing assign short-circuit jump */` |
|         - | 4562 | `  PH7_OP_NULLC_STORE,   /* Null coalescing assign store */` |
|         - | 4563 | `  PH7_OP_NULLSAFE_JMP,  /* Nullsafe (?->) short-circuit jump */` |
|         - | 4564 | `  PH7_OP_SPREAD,        /* Mark TOS for argument unpacking (...$arr) */` |
|         - | 4565 | `  PH7_OP_FLAG_SPREAD,   /* Flag TOS as a spread source for the next LOAD_MAP */` |
|         - | 4566 | `  PH7_OP_CATCH,         /* Bind the in-flight exception into a catch variable (ROOT C inline catch) */` |
|         - | 4567 | `  PH7_OP_END_FINALLY,   /* Terminate an inline finally: dispatch the pending action (ROOT C) */` |
|         - | 4568 | `  PH7_OP_SET_FINALLY_RET,/* Seed a pending RETURN and enter the innermost enclosing finally (ROOT C) */` |
|         - | 4569 | `  PH7_OP_SET_FINALLY_JMP,/* Seed a pending BREAK/CONTINUE (jump target) and enter a finally (ROOT C) */` |
|         - | 4570 | `  PH7_OP_CATCH_JMP,     /* Jump to iP2, leaving the try/catch structures iP1 describes (see` |
|         - | 4571 | `                         * PH7_CATCH_JMP_P1): LEVELS detached catch/finally mini-programs and` |
|         - | 4572 | `                         * CROSS enclosing trys whose OP_POP_EXCEPTION the jump skips. Two` |
|         - | 4573 | `                         * regimes. LEVELS > 0: iP2 is a pc in the OWNING body's bytecode, which` |
|         - | 4574 | `                         * this mini-program cannot address — park it on that body's frame and` |
|         - | 4575 | `                         * end the mini-program; each try's OP_POP_EXCEPTION landing pad on the` |
|         - | 4576 | `                         * way out decrements, and the last one drains CROSS and takes the jump,` |
|         - | 4577 | ``                         * exactly as it materializes a catch's parked `return`. LEVELS == 0:`` |
|         - | 4578 | ``                         * iP2 is in THIS array (a `goto` out of a try body) — just drain CROSS`` |
|         - | 4579 | `                         * and jump. Emitted for break/continue/goto alike. */` |
|         - | 4580 | `  PH7_OP_UNSET_VAR,     /* unset($name): drop ONE name binding (p3 = name), never the shared slot */` |
|         - | 4581 | `  PH7_OP_CALL_INIT,     /* Screen a call's callee where it is WRITTEN, before its arguments run:` |
|         - | 4582 | `                         * php resolves one at INIT_FCALL / INIT_DYNAMIC_CALL and raises the` |
|         - | 4583 | `                         * direct dispatch's own Error there. Emitted only for a callee the` |
|         - | 4584 | `                         * following OP_CALL would be the first to look at — a member callee` |
|         - | 4585 | `                         * was already screened by its OP_MEMBER. iP2 = 1 when the compiler` |
|         - | 4586 | `                         * namespace-qualified the name, which the global fallback needs. */` |
|         - | 4587 | `  PH7_OP_ROT_CALLEE,    /* Rotate this call's CALLEE — which the codegen pushed BEFORE the` |
|         - | 4588 | `                         * arguments, because php resolves a callee where it is written — up` |
|         - | 4589 | `                         * to the top of the stack, so OP_CALL sees the [args…][callee] layout` |
|         - | 4590 | `                         * its whole dispatch is written against. iP1 = compile-time argument` |
|         - | 4591 | `                         * count, iP2 = PH7_ROT_* flags (SPREAD: re-derive the runtime count` |
|         - | 4592 | `                         * from this call's unpack runs; TWOSLOT: the callee is an OP_MEMBER` |
|         - | 4593 | `                         * method pair [receiver][name], not a single value). */` |
|         - | 4594 | `  PH7_OP_FUNC_DECL,     /* Bind a CONDITIONAL function declaration: p3 = ph7_vm_func. php binds` |
|         - | 4595 | `                         * a function written at a unit's top level when the unit compiles and` |
|         - | 4596 | ``                         * one written anywhere else (inside an `if`, a loop, another function's`` |
|         - | 4597 | `                         * body) when execution REACHES it -- which is what makes` |
|         - | 4598 | ``                          * `if (!function_exists('f')) { function f(){} }` a no-op when `f` `` |
|         - | 4599 | `                         * exists, and every symfony/polyfill-* package harmless beside a real` |
|         - | 4600 | `                         * mbstring. Redeclaring is php's runtime fatal, raised here. */` |
|         - | 4601 | `  PH7_OP_CLASS_DEFER    /* Deferred class declaration: p3 = VmDeferredClass. Compile-time` |
|         - | 4602 | `                         * resolution of a parent/interface/trait failed (autoloader not yet` |
|         - | 4603 | `                         * REGISTERED — the declaring file's own statements had not run), so the` |
|         - | 4604 | `                         * whole declaration re-compiles here, at its execution point, where` |
|         - | 4605 | `                         * spl_autoload_register has taken effect. php's own model: classes with` |
|         - | 4606 | `                         * unresolved parents are declared in execution order, not hoisted. */` |
|         - | 4607 | `};` |
|         - | 4608 | `/*` |
|         - | 4609 | ` * PH7_OP_CATCH_JMP.iP1 payload. Both halves are nesting depths of the source, never` |
|         - | 4610 | ` * large: LEVELS = detached catch/finally boundaries the jump leaves (0 = none, it` |
|         - | 4611 | ` * stays in this bytecode array), CROSS = enclosing try activations whose` |
|         - | 4612 | ` * OP_POP_EXCEPTION the jump skips, and whose finally it must therefore drain itself.` |
|         - | 4613 | ` */` |
|         - | 4614 | `#define PH7_CATCH_JMP_P1(LEVELS,CROSS) \` |
|         - | 4615 | `	((sxi32)((((sxu32)(CROSS)) << 16) \| ((sxu32)(LEVELS) & 0xFFFFu)))` |
|         - | 4616 | `#define PH7_CATCH_JMP_LEVELS(P1) ((sxu16)((sxu32)(P1) & 0xFFFFu))` |
|         - | 4617 | `#define PH7_CATCH_JMP_CROSS(P1)  ((sxu16)(((sxu32)(P1) >> 16) & 0xFFFFu))` |
|         - | 4618 | `/* LOADC.iP1 bit flags */` |
|         - | 4619 | `#define PH7_LOADC_EXPAND   0x01 /* Candidate for constant/function/class expansion */` |
|         - | 4620 | `#define PH7_LOADC_NOKEY    0x04 /* The nil this pushes is an ABSENT array-literal key (auto-index),` |
|         - | 4621 | ``                                 * not an explicit `null =>` one. The two are both MEMOBJ_NULL on the`` |
|         - | 4622 | `                                 * stack, and LOAD_MAP must tell them apart: an absent key auto-indexes` |
|         - | 4623 | `                                 * silently, an explicit null key deprecates and stores under "". */` |
|         - | 4624 | `#define PH7_LOADC_ABSOLUTE 0x02 /* Fully-qualified — skip namespace prefixing */` |
|         - | 4625 | ``#define PH7_LOADC_NOGLOBAL 0x08 /* The p3 candidate came from a `use const` import, which php`` |
|         - | 4626 | `                                 * resolves WITHOUT a global fallback: if that exact name is` |
|         - | 4627 | `                                 * undefined the read is an Error, even when a global constant` |
|         - | 4628 | `                                 * of the imported alias's short name exists. */` |
|         - | 4629 | `/* ROT_CALLEE.iP2 — what the rotation has to know about the region it is turning over. */` |
|         - | 4630 | `#define PH7_ROT_SPREAD  0x1 /* This call unpacks: the runtime argument count is iP1 plus the net` |
|         - | 4631 | `                             * growth of its OWN captured spread runs (VmSpreadOwnExtra). */` |
|         - | 4632 | `#define PH7_ROT_TWOSLOT 0x2 /* The callee is an OP_MEMBER method pair — [receiver][method name] —` |
|         - | 4633 | `                             * which OP_CALL reads as two slots ($this / the late-static-binding` |
|         - | 4634 | `                             * class from the receiver, the name from the top). A __call routing` |
|         - | 4635 | `                             * collapses that pair to ONE marked carrier slot at run time, which` |
|         - | 4636 | `                             * the handler detects rather than guessing. */` |
|         - | 4637 | `/* MEMBER.iP2 — member-access context. 0=read is the default; the unset/isset/empty modes mirror the` |
|         - | 4638 | ` * array LOAD_IDX context modes so unset()/isset()/empty() on a property behave like on an array elem. */` |
|         - | 4639 | `/* Two of PH7_OP_LOAD_IDX's own iP2 context codes (they do NOT line up with the` |
|         - | 4640 | ` * PH7_MEMBER_* set below). Shared because the PROPERTY opcode has to recognize the` |
|         - | 4641 | `` * base of an unset-subscript: `unset($o->p[$k])` reaches into what $p holds, which`` |
|         - | 4642 | ` * is an indirect modification of $p. */` |
|         - | 4643 | `#define VM_IDX_CTX_UNSET 5` |
|         - | 4644 | ``/* An INTERMEDIATE subscript of an unset chain (`unset($a['k']['n'])`): every unset`` |
|         - | 4645 | ` * rule applies to it — COW-separate the parent, never vivify a missing key, unset's` |
|         - | 4646 | ` * own wording for a bad base — except the removal itself, which belongs to the` |
|         - | 4647 | ` * OUTERMOST subscript alone. */` |
|         - | 4648 | `#define VM_IDX_CTX_UNSET_BASE 10` |
|         - | 4649 | ``/* A READ-MODIFY-WRITE subscript (`$a[k] += v`, `$a[k]++`, `$a[k] .= v`) — php's`` |
|         - | 4650 | ` * BP_VAR_RW fetch. It needs a writable slot exactly as the plain write context` |
|         - | 4651 | ` * (1) does, and everything downstream treats it as one; the single thing that` |
|         - | 4652 | ` * separates them is that php READS the element first, so a missing key WARNS` |
|         - | 4653 | ``  * before it is created. Every level of a chain carries it (`$a['x']['y'] += 1` `` |
|         - | 4654 | ` * warns for both), which is why it is a compile-time context and not a peek at` |
|         - | 4655 | ` * the instruction that follows. */` |
|         - | 4656 | `#define VM_IDX_CTX_RMW 11` |
|         - | 4657 | `#define VM_IDX_IS_UNSET(iP2) ((iP2) == VM_IDX_CTX_UNSET \|\| (iP2) == VM_IDX_CTX_UNSET_BASE)` |
|         - | 4658 | `#define PH7_MEMBER_READ   0 /* attribute read */` |
|         - | 4659 | `#define PH7_MEMBER_METHOD 1 /* method-call preparation */` |
|         - | 4660 | `#define PH7_MEMBER_UNSET  2 /* unset($o->p): remove the property */` |
|         - | 4661 | `#define PH7_MEMBER_ISSET  3 /* isset($o->p): silent on a read-miss */` |
|         - | 4662 | `#define PH7_MEMBER_EMPTY  4 /* empty($o->p): silent on a read-miss */` |
|         - | 4663 | `#define PH7_MEMBER_WRITE  5 /* write-lvalue base ($o->arr[..]=, $o->p??=): auto-create a missing prop */` |
|         - | 4664 | `#define PH7_MEMBER_REF_TARGET 6 /* reference-store target ($o->p =& $x, C::$s =& $x): resolve the` |
|         - | 4665 | `                                 * property slot and stash it for the following OP_STORE_REF; skip` |
|         - | 4666 | `                                 * the read/hook/magic machinery (a ref bind neither reads nor coerces) */` |
|         - | 4667 | `#define PH7_MEMBER_DEFPATH 7    /* D1 commit 2: deferred by-ref/by-value property call arg ($o->p). Reads a` |
|         - | 4668 | `                                 * present property (like READ); on a miss/magic, records the lvalue path` |
|         - | 4669 | `                                 * (MEMOBJ_AUX_DEFPATH) that OP_CALL re-walks in vivify or read+warn mode */` |
|         - | 4670 | ``#define PH7_MEMBER_COALESCE 9 /* `$o->p ?? d`: php's THIRD accessor level, between a read and an`` |
|         - | 4671 | `                               * isset(). Silent on a read-miss like isset()/empty(), but the` |
|         - | 4672 | `                               * expression takes the property's VALUE, not a truth: __isset()` |
|         - | 4673 | `                               * GATES the access and __get() (or a get HOOK) ANSWERS it, and` |
|         - | 4674 | `                               * with no __isset declared the accessor answers on its own.` |
|         - | 4675 | ``                               * `??` used to compile as PH7_MEMBER_ISSET, so every accessor`` |
|         - | 4676 | `                               * path handed the coalesce a BOOLEAN. */` |
|         - | 4677 | `#define PH7_MEMBER_LIST_TARGET 8 /* positional list-destructuring store target ([$o->p] = [...]): a pure` |
|         - | 4678 | `                                  * write whose value arrives only at the following OP_LOAD_LIST, which` |
|         - | 4679 | `                                  * writes the slot directly (with typed-slot enforcement). Skip the` |
|         - | 4680 | `                                  * uninitialized-typed read Error and the __get consult, and vivify a` |
|         - | 4681 | `                                  * missing property like a write base */` |
|         - | 4682 | `/* -- END-OF INSTRUCTIONS -- */` |
|         - | 4683 | `/*` |
|         - | 4684 | ` * Expression Operators ID.` |
|         - | 4685 | ` */` |
|         - | 4686 | `enum ph7_expr_id {` |
|         - | 4687 | `	EXPR_OP_NEW = 1,   /* new */` |
|         - | 4688 | `	EXPR_OP_CLONE,     /* clone */` |
|         - | 4689 | `	EXPR_OP_ARROW,     /* -> */` |
|         - | 4690 | `	EXPR_OP_NULLSAFE_ARROW, /* ?-> (PHP 8.0 nullsafe) */` |
|         - | 4691 | `	EXPR_OP_DC,        /* :: */` |
|         - | 4692 | `	EXPR_OP_SUBSCRIPT, /* []: Subscripting */` |
|         - | 4693 | `	EXPR_OP_FUNC_CALL, /* func_call() */` |
|         - | 4694 | `	EXPR_OP_INCR,      /* ++ */` |
|         - | 4695 | `	EXPR_OP_DECR,      /* -- */` |
|         - | 4696 | `	EXPR_OP_BITNOT,    /* ~ */` |
|         - | 4697 | `	EXPR_OP_UMINUS,    /* Unary minus  */` |
|         - | 4698 | `	EXPR_OP_UPLUS,     /* Unary plus */` |
|         - | 4699 | `	EXPR_OP_TYPECAST,  /* Type cast [i.e: (int),(float),(string)...] */` |
|         - | 4700 | `	EXPR_OP_ALT,       /* @ */` |
|         - | 4701 | `	EXPR_OP_INSTOF,    /* instanceof */` |
|         - | 4702 | `	EXPR_OP_LOGNOT,    /* logical not ! */` |
|         - | 4703 | `	EXPR_OP_MUL,       /* Multiplication */` |
|         - | 4704 | `	EXPR_OP_DIV,       /* division */` |
|         - | 4705 | `	EXPR_OP_MOD,       /* Modulus */` |
|         - | 4706 | `	EXPR_OP_POW,       /* Exponentiation ** */` |
|         - | 4707 | `	EXPR_OP_ADD,       /* Addition */` |
|         - | 4708 | `	EXPR_OP_SUB,       /* Substraction */` |
|         - | 4709 | `	EXPR_OP_DOT,       /* Concatenation */` |
|         - | 4710 | `	EXPR_OP_SHL,       /* Left shift */` |
|         - | 4711 | `	EXPR_OP_SHR,       /* Right shift */` |
|         - | 4712 | `	EXPR_OP_LT,        /* Less than */` |
|         - | 4713 | `	EXPR_OP_LE,        /* Less equal */` |
|         - | 4714 | `	EXPR_OP_GT,        /* Greater than */` |
|         - | 4715 | `	EXPR_OP_GE,        /* Greater equal */` |
|         - | 4716 | `	EXPR_OP_SPACESHIP, /* Spaceship <=> */` |
|         - | 4717 | `	EXPR_OP_EQ,        /* Equal == */` |
|         - | 4718 | `	EXPR_OP_NE,        /* Not equal != <> */` |
|         - | 4719 | `	EXPR_OP_TEQ,       /* Type equal === */` |
|         - | 4720 | `	EXPR_OP_TNE,       /* Type not equal !== */` |
|         - | 4721 | `	EXPR_OP_BAND,      /* Biwise and '&' */` |
|         - | 4722 | `	EXPR_OP_REF,       /* Reference operator '&' */` |
|         - | 4723 | `	EXPR_OP_XOR,       /* bitwise xor '^' */` |
|         - | 4724 | `	EXPR_OP_BOR,       /* bitwise or '\|' */` |
|         - | 4725 | `	EXPR_OP_LAND,      /* Logical and '&&','and' */` |
|         - | 4726 | `	EXPR_OP_LOR,       /* Logical or  '\|\|','or'*/` |
|         - | 4727 | `	EXPR_OP_LXOR,      /* Logical xor 'xor' */` |
|         - | 4728 | `	EXPR_OP_QUESTY,    /* Ternary operator '?' */` |
|         - | 4729 | `	EXPR_OP_NULLC,     /* Null coalescing '??' */` |
|         - | 4730 | `	EXPR_OP_ASSIGN,    /* Assignment '=' */` |
|         - | 4731 | `	EXPR_OP_ADD_ASSIGN, /* Combined operator: += */` |
|         - | 4732 | `	EXPR_OP_SUB_ASSIGN, /* Combined operator: -= */` |
|         - | 4733 | `	EXPR_OP_MUL_ASSIGN, /* Combined operator: *= */` |
|         - | 4734 | `	EXPR_OP_DIV_ASSIGN, /* Combined operator: /= */` |
|         - | 4735 | `	EXPR_OP_MOD_ASSIGN, /* Combined operator: %= */` |
|         - | 4736 | `	EXPR_OP_POW_ASSIGN, /* Combined operator: **= */` |
|         - | 4737 | `	EXPR_OP_DOT_ASSIGN, /* Combined operator: .= */` |
|         - | 4738 | `	EXPR_OP_AND_ASSIGN, /* Combined operator: &= */` |
|         - | 4739 | `	EXPR_OP_OR_ASSIGN,  /* Combined operator: \|= */` |
|         - | 4740 | `	EXPR_OP_XOR_ASSIGN, /* Combined operator: ^= */` |
|         - | 4741 | `	EXPR_OP_SHL_ASSIGN, /* Combined operator: <<= */` |
|         - | 4742 | `	EXPR_OP_SHR_ASSIGN, /* Combined operator: >>= */` |
|         - | 4743 | `	EXPR_OP_NULLC_ASSIGN, /* Combined operator: null coalescing assign */` |
|         - | 4744 | `	EXPR_OP_PIPE,       /* PHP 8.5 pipe operator: \|> */` |
|         - | 4745 | `	EXPR_OP_COMMA       /* Comma expression */` |
|         - | 4746 | `};` |
|         - | 4747 | `/*` |
|         - | 4748 | ` * Very high level tokens.` |
|         - | 4749 | ` */` |
|         - | 4750 | `#define PH7_TOKEN_RAW 0x001 /* Raw text [i.e: HTML,XML...] */` |
|         - | 4751 | `#define PH7_TOKEN_PHP 0x002 /* PHP chunk */` |
|         - | 4752 | `/*` |
|         - | 4753 | ` * Lexer token codes` |
|         - | 4754 | ` * The following set of constants are the tokens recognized` |
|         - | 4755 | ` * by the lexer when processing PHP input.` |
|         - | 4756 | ` * Important: Token values MUST BE A POWER OF TWO.` |
|         - | 4757 | ` */` |
|         - | 4758 | `#define PH7_TK_INTEGER   0x0000001  /* Integer */` |
|         - | 4759 | `#define PH7_TK_REAL      0x0000002  /* Real number */` |
|         - | 4760 | `#define PH7_TK_NUM       (PH7_TK_INTEGER\|PH7_TK_REAL) /* Numeric token,either integer or real */` |
|         - | 4761 | `#define PH7_TK_KEYWORD   0x0000004 /* Keyword [i.e: while,for,if,foreach...] */` |
|         - | 4762 | `#define PH7_TK_ID        0x0000008 /* Alphanumeric or UTF-8 stream */` |
|         - | 4763 | `#define PH7_TK_DOLLAR    0x0000010 /* '$' Dollar sign */` |
|         - | 4764 | `#define PH7_TK_OP        0x0000020 /* Operator [i.e: +,*,/...] */` |
|         - | 4765 | `#define PH7_TK_OCB       0x0000040 /* Open curly brace'{' */` |
|         - | 4766 | `#define PH7_TK_CCB       0x0000080 /* Closing curly brace'}' */` |
|         - | 4767 | `#define PH7_TK_NSSEP     0x0000100 /* Namespace separator '\' */` |
|         - | 4768 | `#define PH7_TK_LPAREN    0x0000200 /* Left parenthesis '(' */` |
|         - | 4769 | `#define PH7_TK_RPAREN    0x0000400 /* Right parenthesis ')' */` |
|         - | 4770 | `#define PH7_TK_OSB       0x0000800 /* Open square bracket '[' */` |
|         - | 4771 | `#define PH7_TK_CSB       0x0001000 /* Closing square bracket ']' */` |
|         - | 4772 | `#define PH7_TK_DSTR      0x0002000 /* Double quoted string "$str" */` |
|         - | 4773 | `#define PH7_TK_SSTR      0x0004000 /* Single quoted string 'str' */` |
|         - | 4774 | `#define PH7_TK_HEREDOC   0x0008000 /* Heredoc <<< */` |
|         - | 4775 | `#define PH7_TK_NOWDOC    0x0010000 /* Nowdoc <<< */` |
|         - | 4776 | `#define PH7_TK_COMMA     0x0020000 /* Comma ',' */` |
|         - | 4777 | `#define PH7_TK_SEMI      0x0040000 /* Semi-colon ";" */` |
|         - | 4778 | ``#define PH7_TK_BSTR      0x0080000 /* Backtick quoted string [i.e: Shell command `date`] */`` |
|         - | 4779 | `#define PH7_TK_COLON     0x0100000 /* single Colon ':' */` |
|         - | 4780 | `#define PH7_TK_AMPER     0x0200000 /* Ampersand '&' */` |
|         - | 4781 | `#define PH7_TK_EQUAL     0x0400000 /* Equal '=' */` |
|         - | 4782 | `#define PH7_TK_ARRAY_OP  0x0800000 /* Array operator '=>' */` |
|         - | 4783 | `#define PH7_TK_ELLIPSIS  0x1000000 /* Ellipsis '...' */` |
|         - | 4784 | `#define PH7_TK_OTHER     0x2000000 /* Other symbols */` |
|         - | 4785 | ``#define PH7_TK_VOID_CAST 0x8000000 /* php 8.5's `(void)` cast, assembled by the lexer from the three`` |
|         - | 4786 | `                                      * tokens the way every other cast operator is. It is NOT an` |
|         - | 4787 | `                                      * expression operator: php's grammar takes it only at the head` |
|         - | 4788 | ``                                      * of an expression STATEMENT or of a `for` clause, so anywhere`` |
|         - | 4789 | `                                      * else it stays an unrecognized token and the parser reports` |
|         - | 4790 | ``                                      * php's `unexpected token "(void)"`. */`` |
|         - | 4791 | `#define PH7_TK_UNTERM    0x10000000 /* The lexeme ran into the END OF THE INPUT without its closing` |
|         - | 4792 | `                                     * delimiter: an unterminated quote, heredoc or block comment.` |
|         - | 4793 | `                                     * php refuses each of those; this engine used to consume them` |
|         - | 4794 | `                                     * up to EOF and run the program. */` |
|         - | 4795 | `#define PH7_TK_MEMBER_NAME 0x4000000 /* Reserved word used as a member NAME right after -> / ?-> / ::` |
|         - | 4796 | `                                      * (Enum::Null, C::Array, $o->list()): a plain identifier, never` |
|         - | 4797 | `                                      * the literal value — GenStateLoadLiteral skips its value conversion. */` |
|         - | 4798 | `/*` |
|         - | 4799 | ` * PHP keyword.` |
|         - | 4800 | ` * These words have special meaning in PHP. Some of them represent things which look like` |
|         - | 4801 | ` * functions, some look like constants, and so on, but they're not, really: they are language constructs.` |
|         - | 4802 | ` * You cannot use any of the following words as constants, class names, function or method names.` |
|         - | 4803 | ` * Using them as variable names is generally OK, but could lead to confusion.` |
|         - | 4804 | ` */` |
|         - | 4805 | `#define PH7_TKWRD_EXTENDS      1 /* extends */` |
|         - | 4806 | `#define PH7_TKWRD_ENDSWITCH    2 /* endswitch */` |
|         - | 4807 | `#define PH7_TKWRD_SWITCH       3 /* switch */` |
|         - | 4808 | `#define PH7_TKWRD_PRINT        4 /* print */` |
|         - | 4809 | `#define PH7_TKWRD_INTERFACE    5 /* interface */` |
|         - | 4810 | `#define PH7_TKWRD_ENDDEC       6 /* enddeclare */` |
|         - | 4811 | `#define PH7_TKWRD_DECLARE      7 /* declare */` |
|         - | 4812 | `/* The number '8' is reserved for PH7_TK_ID */` |
|         - | 4813 | `#define PH7_TKWRD_REQONCE      9 /* require_once */` |
|         - | 4814 | `#define PH7_TKWRD_REQUIRE      10 /* require */` |
|         - | 4815 | `#define PH7_TKWRD_ELIF         0x4000000 /* elseif: MUST BE A POWER OF TWO */` |
|         - | 4816 | `#define PH7_TKWRD_ELSE         0x8000000 /* else:  MUST BE A POWER OF TWO */` |
|         - | 4817 | `#define PH7_TKWRD_IF           13 /* if */` |
|         - | 4818 | `#define PH7_TKWRD_FINAL        14 /* final */` |
|         - | 4819 | `#define PH7_TKWRD_LIST         15 /* list */` |
|         - | 4820 | `#define PH7_TKWRD_STATIC       16 /* static */` |
|         - | 4821 | `#define PH7_TKWRD_CASE         17 /* case */` |
|         - | 4822 | `#define PH7_TKWRD_SELF         18 /* self */` |
|         - | 4823 | `#define PH7_TKWRD_FUNCTION     19 /* function */` |
|         - | 4824 | `#define PH7_TKWRD_NAMESPACE    20 /* namespace */` |
|         - | 4825 | `#define PH7_TKWRD_ENDIF        0x400000 /* endif: MUST BE A POWER OF TWO */` |
|         - | 4826 | `#define PH7_TKWRD_CLONE        0x80 /* clone: MUST BE A POWER OF TWO  */` |
|         - | 4827 | `#define PH7_TKWRD_NEW          0x100 /* new: MUST BE A POWER OF TWO  */` |
|         - | 4828 | `#define PH7_TKWRD_CONST        22 /* const */` |
|         - | 4829 | `#define PH7_TKWRD_THROW        23 /* throw */` |
|         - | 4830 | `#define PH7_TKWRD_USE          24 /* use */` |
|         - | 4831 | `#define PH7_TKWRD_ENDWHILE     0x800000 /* endwhile: MUST BE A POWER OF TWO */` |
|         - | 4832 | `#define PH7_TKWRD_WHILE        26 /* while */` |
|         - | 4833 | `#define PH7_TKWRD_EVAL         27 /* eval */` |
|         - | 4834 | `#define PH7_TKWRD_VAR          28 /* var */` |
|         - | 4835 | `#define PH7_TKWRD_ARRAY        0x200 /* array: MUST BE A POWER OF TWO */` |
|         - | 4836 | `#define PH7_TKWRD_ABSTRACT     29 /* abstract */` |
|         - | 4837 | `#define PH7_TKWRD_TRY          30 /* try */` |
|         - | 4838 | `#define PH7_TKWRD_AND          0x400 /* and: MUST BE A POWER OF TWO  */` |
|         - | 4839 | `#define PH7_TKWRD_DEFAULT      31 /* default */` |
|         - | 4840 | `#define PH7_TKWRD_CLASS        32 /* class */` |
|         - | 4841 | `#define PH7_TKWRD_AS           33 /* as */` |
|         - | 4842 | `#define PH7_TKWRD_CONTINUE     34 /* continue */` |
|         - | 4843 | `#define PH7_TKWRD_EXIT         35 /* exit */` |
|         - | 4844 | `#define PH7_TKWRD_DIE          36 /* die */` |
|         - | 4845 | `#define PH7_TKWRD_ECHO         37 /* echo */` |
|         - | 4846 | `#define PH7_TKWRD_GLOBAL       38 /* global */` |
|         - | 4847 | `#define PH7_TKWRD_IMPLEMENTS   39 /* implements */` |
|         - | 4848 | `#define PH7_TKWRD_INCONCE      40 /* include_once */` |
|         - | 4849 | `#define PH7_TKWRD_INCLUDE      41 /* include */` |
|         - | 4850 | `#define PH7_TKWRD_EMPTY        42 /* empty */` |
|         - | 4851 | `#define PH7_TKWRD_INSTANCEOF   0x800 /* instanceof: MUST BE A POWER OF TWO  */` |
|         - | 4852 | `#define PH7_TKWRD_ISSET        43 /* isset */` |
|         - | 4853 | `#define PH7_TKWRD_PARENT       44 /* parent */` |
|         - | 4854 | `#define PH7_TKWRD_PRIVATE      45 /* private */` |
|         - | 4855 | `#define PH7_TKWRD_ENDFOR       0x1000000 /* endfor: MUST BE A POWER OF TWO */` |
|         - | 4856 | `#define PH7_TKWRD_END4EACH     0x2000000 /* endforeach: MUST BE A POWER OF TWO */` |
|         - | 4857 | `#define PH7_TKWRD_FOR          48 /* for */` |
|         - | 4858 | `#define PH7_TKWRD_FOREACH      49 /* foreach */` |
|         - | 4859 | `#define PH7_TKWRD_OR           0x1000 /* or: MUST BE A POWER OF TWO  */` |
|         - | 4860 | `#define PH7_TKWRD_PROTECTED    50 /* protected */` |
|         - | 4861 | `#define PH7_TKWRD_DO           51 /* do */` |
|         - | 4862 | `#define PH7_TKWRD_PUBLIC       52 /* public */` |
|         - | 4863 | `#define PH7_TKWRD_CATCH        53 /* catch */` |
|         - | 4864 | `#define PH7_TKWRD_RETURN       54 /* return */` |
|         - | 4865 | `#define PH7_TKWRD_UNSET        0x2000 /* unset: MUST BE A POWER OF TWO  */` |
|         - | 4866 | `#define PH7_TKWRD_XOR          0x4000 /* xor: MUST BE A POWER OF TWO  */` |
|         - | 4867 | `#define PH7_TKWRD_BREAK        55 /* break */` |
|         - | 4868 | `#define PH7_TKWRD_GOTO         56 /* goto */` |
|         - | 4869 | `#define PH7_TKWRD_TRAIT        57 /* trait */` |
|         - | 4870 | `#define PH7_TKWRD_INSTEADOF    58 /* insteadof */` |
|         - | 4871 | `#define PH7_TKWRD_FINALLY      59 /* finally */` |
|         - | 4872 | `#define PH7_TKWRD_YIELD        60 /* yield */` |
|         - | 4873 | `#define PH7_TKWRD_FN           61 /* fn (PHP 7.4 arrow function) */` |
|         - | 4874 | `#define PH7_TKWRD_MATCH        62 /* match (PHP 8.0 match expression) */` |
|         - | 4875 | `#define PH7_TKWRD_BOOL         0x8000  /* bool:  MUST BE A POWER OF TWO */` |
|         - | 4876 | `#define PH7_TKWRD_INT          0x10000  /* int:   MUST BE A POWER OF TWO */` |
|         - | 4877 | `#define PH7_TKWRD_FLOAT        0x20000  /* float:  MUST BE A POWER OF TWO */` |
|         - | 4878 | `#define PH7_TKWRD_STRING       0x40000  /* string: MUST BE A POWER OF TWO */` |
|         - | 4879 | `#define PH7_TKWRD_OBJECT       0x80000 /* object: MUST BE A POWER OF TWO */` |
|         - | 4880 | `/* 0x100000 and 0x200000 are free: they were the PH7-ism 'eq'/'ne' string` |
|         - | 4881 | ` * comparison operators, removed so both stay usable as plain identifiers. */` |
|         - | 4882 | `/*` |
|         - | 4883 | ` * PHP-exact ENT_* flag values for the html-entity family. Single source of` |
|         - | 4884 | ` * truth: constant.c declares the PHP-visible ENT_* constants from these and` |
|         - | 4885 | ` * builtin.c implements the semantics against them. The low two bits are the` |
|         - | 4886 | ` * quote bits (ENT_QUOTES = both, ENT_COMPAT = double only, ENT_NOQUOTES = 0)` |
|         - | 4887 | ` * and bits 16\|32 select the doctype — composites, not independent flags.` |
|         - | 4888 | ` */` |
|         - | 4889 | `#define PH7_ENT_QUOTE_SINGLE 0x01 /* encode/decode ' */` |
|         - | 4890 | `#define PH7_ENT_QUOTE_DOUBLE 0x02 /* encode/decode " (== ENT_COMPAT) */` |
|         - | 4891 | `#define PH7_ENT_QUOTES       (PH7_ENT_QUOTE_DOUBLE\|PH7_ENT_QUOTE_SINGLE)` |
|         - | 4892 | `#define PH7_ENT_IGNORE       0x04 /* drop invalid UTF-8 units */` |
|         - | 4893 | `#define PH7_ENT_SUBSTITUTE   0x08 /* invalid UTF-8 unit -> U+FFFD */` |
|         - | 4894 | `#define PH7_ENT_DOC_MASK     0x30 /* doctype selector */` |
|         - | 4895 | `#define PH7_ENT_DOC_HTML401  0x00` |
|         - | 4896 | `#define PH7_ENT_DOC_XML1     0x10` |
|         - | 4897 | `#define PH7_ENT_DOC_XHTML    0x20` |
|         - | 4898 | `#define PH7_ENT_DOC_HTML5    0x30` |
|         - | 4899 | `#define PH7_ENT_DISALLOWED   0x80 /* substitute doctype-disallowed codepoints */` |
|         - | 4900 | `/* The shared default for all five builtins (php 8.1+): ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401. */` |
|         - | 4901 | `#define PH7_ENT_DEFAULT      (PH7_ENT_QUOTES\|PH7_ENT_SUBSTITUTE\|PH7_ENT_DOC_HTML401)` |
|         - | 4902 | `/* JSON encoding/decoding related definition */` |
|         - | 4903 | `enum json_err_code{` |
|         - | 4904 | `	JSON_ERROR_NONE = 0,  /* No error has occurred. */` |
|         - | 4905 | `	JSON_ERROR_DEPTH,     /* The maximum stack depth has been exceeded.  */` |
|         - | 4906 | `	JSON_ERROR_STATE_MISMATCH, /* Occurs with underflow or with the modes mismatch.  */` |
|         - | 4907 | `	JSON_ERROR_CTRL_CHAR, /* Control character error, possibly incorrectly encoded.  */` |
|         - | 4908 | `	JSON_ERROR_SYNTAX,    /* Syntax error. */` |
|         - | 4909 | `	JSON_ERROR_UTF8,      /* Malformed UTF-8 characters */` |
|         - | 4910 | `	JSON_ERROR_RECURSION, /* A container already being encoded shows up inside itself (php value 6) */` |
|         - | 4911 | `	JSON_ERROR_INF_OR_NAN = 7, /* Inf or NaN given to json_encode (php value) */` |
|         - | 4912 | `	JSON_ERROR_UNSUPPORTED_TYPE = 8, /* A resource given to json_encode (php value) */` |
|         - | 4913 | `	JSON_ERROR_INVALID_PROPERTY_NAME = 9, /* Object-mode decode of a property name with a` |
|         - | 4914 | `	                                       * LEADING NUL byte — php reserves that prefix for` |
|         - | 4915 | `	                                       * mangled private/protected names (php value) */` |
|         - | 4916 | `	JSON_ERROR_UTF16 = 10, /* Unpaired UTF-16 surrogate in a \uXXXX escape (php value) */` |
|         - | 4917 | `	JSON_ERROR_NON_BACKED_ENUM = 11 /* Non-backed enum given to json_encode (php 8.1 value) */` |
|         - | 4918 | `};` |
|         - | 4919 | `/* The following constants can be combined to form options for json_encode(). */` |
|         - | 4920 | `#define	JSON_HEX_TAG           0x01  /* All < and > are converted to \u003C and \u003E. */` |
|         - | 4921 | `#define JSON_HEX_AMP           0x02  /* All &s are converted to \u0026. */` |
|         - | 4922 | `#define JSON_HEX_APOS          0x04  /* All ' are converted to \u0027. */` |
|         - | 4923 | `#define JSON_HEX_QUOT          0x08  /* All " are converted to \u0022. */` |
|         - | 4924 | `#define JSON_FORCE_OBJECT      0x10  /* Outputs an object rather than an array */` |
|         - | 4925 | `#define JSON_NUMERIC_CHECK     0x20  /* Encodes numeric strings as numbers. */` |
|         - | 4926 | `#define JSON_PRETTY_PRINT      0x80  /* Use whitespace in returned data to format it.*/` |
|         - | 4927 | `#define JSON_UNESCAPED_SLASHES 0x40  /* Don't escape '/' */` |
|         - | 4928 | `#define JSON_UNESCAPED_UNICODE 0x100 /* Emit multibyte UTF-8 raw instead of \uXXXX */` |
|         - | 4929 | `#define JSON_PARTIAL_OUTPUT_ON_ERROR 0x200 /* Substitute (0 / null / "") for an unencodable` |
|         - | 4930 | `                                            * piece and record the error instead of failing */` |
|         - | 4931 | `#define JSON_PRESERVE_ZERO_FRACTION  0x400 /* A float with no fractional digits prints ".0"` |
|         - | 4932 | `                                            * (1.0 encodes as "1.0", not "1") */` |
|         - | 4933 | `#define JSON_UNESCAPED_LINE_TERMINATORS 0x800 /* ...U+2028/U+2029 included */` |
|         - | 4934 | `#define JSON_INVALID_UTF8_IGNORE     0x100000 /* Drop ill-formed UTF-8 instead of failing */` |
|         - | 4935 | `#define JSON_INVALID_UTF8_SUBSTITUTE 0x200000 /* ...replace it with U+FFFD */` |
|         - | 4936 | `#define JSON_THROW_ON_ERROR    0x400000 /* Throw JsonException on encode/decode error */` |
|         - | 4937 | `/* DECODE flags: php numbers the decode options in their own space, so each shares a` |
|         - | 4938 | ` * bit with an encode flag exactly as php's JSON_BIGINT_AS_STRING shares 2 with` |
|         - | 4939 | ` * JSON_HEX_AMP (and JSON_OBJECT_AS_ARRAY shares 1 with JSON_HEX_TAG). */` |
|         - | 4940 | `#define JSON_OBJECT_AS_ARRAY   0x01` |
|         - | 4941 | `#define JSON_BIGINT_AS_STRING  0x02` |
|         - | 4942 | `/*` |
|         - | 4943 | ` * extract() $flags — php's ENUM (ext/standard/php_array.h), not a bitmask.` |
|         - | 4944 | ` * PH7 exposed a legacy power-of-two bitmask here (1/2/4/8/16/32/64), which` |
|         - | 4945 | ` * changed the meaning of valid php source: extract($a,1) is EXTR_SKIP in php` |
|         - | 4946 | ` * but was EXTR_OVERWRITE in PHL, and EXTR_PREFIX_ALL printed 8 instead of 3.` |
|         - | 4947 | ` * The values below ARE php's, and vm_builtin_extract() dispatches on` |
|         - | 4948 | ` * (flags & 0xff) exactly like php does.` |
|         - | 4949 | ` */` |
|         - | 4950 | `#define PH7_EXTR_OVERWRITE        0` |
|         - | 4951 | `#define PH7_EXTR_SKIP             1` |
|         - | 4952 | `#define PH7_EXTR_PREFIX_SAME      2` |
|         - | 4953 | `#define PH7_EXTR_PREFIX_ALL       3` |
|         - | 4954 | `#define PH7_EXTR_PREFIX_INVALID   4` |
|         - | 4955 | `#define PH7_EXTR_PREFIX_IF_EXISTS 5` |
|         - | 4956 | `#define PH7_EXTR_IF_EXISTS        6` |
|         - | 4957 | `#define PH7_EXTR_REFS             0x100 /* php's by-reference extraction (rides above the mode) */` |
|         - | 4958 | `/*` |
|         - | 4959 | ` * pathinfo() $flags, glob() $flags and parse_ini_*() $scanner_mode — php's VALUES.` |
|         - | 4960 | ` *` |
|         - | 4961 | ` * Each of these was a PH7 invention (pathinfo counted 1/2/3/4 where php's are POWERS` |
|         - | 4962 | ` * OF TWO, glob used its own 1..64 ladder, and the ini scanner started at 1), which` |
|         - | 4963 | `` * changes the meaning of valid php source: `PATHINFO_DIRNAME\|PATHINFO_BASENAME` is 3`` |
|         - | 4964 | ` * in both engines but PHL read 3 as PATHINFO_EXTENSION, a script passing php's literal` |
|         - | 4965 | ` * 64 to json_encode() got JSON_BIGINT_AS_STRING instead of JSON_UNESCAPED_SLASHES, and` |
|         - | 4966 | ` * INI_SCANNER_RAW (php 1) selected nothing.` |
|         - | 4967 | ` *` |
|         - | 4968 | ` * The GLOB_* values are php 8.5's OWN portable set (main/php_glob.h, new in 8.5), which` |
|         - | 4969 | ` * a default build uses on every platform including MSVC — the bundled branch is taken` |
|         - | 4970 | ` * unless the POSIX build is configured with --enable-system-glob (off by default), and` |
|         - | 4971 | ` * the win32 build has no such option. Do NOT read them off the host <glob.h>: glibc's` |
|         - | 4972 | ` * ladder is different (MARK 2, NOSORT 4, BRACE 1024, ONLYDIR 8192), and so was php's` |
|         - | 4973 | ` * own pre-8.5 win32/glob.h (NOESCAPE 0x2000). PATHINFO_*, INI_SCANNER_* and JSON_* are` |
|         - | 4974 | ` * plain #defines in ext/standard and ext/json, never platform-conditional.` |
|         - | 4975 | ` */` |
|         - | 4976 | `#define PH7_PATHINFO_DIRNAME    1` |
|         - | 4977 | `#define PH7_PATHINFO_BASENAME   2` |
|         - | 4978 | `#define PH7_PATHINFO_EXTENSION  4` |
|         - | 4979 | `#define PH7_PATHINFO_FILENAME   8` |
|         - | 4980 | `#define PH7_PATHINFO_ALL        (PH7_PATHINFO_DIRNAME\|PH7_PATHINFO_BASENAME\|\` |
|         - | 4981 | `                                 PH7_PATHINFO_EXTENSION\|PH7_PATHINFO_FILENAME)` |
|         - | 4982 | `#define PH7_GLOB_ERR            0x0004` |
|         - | 4983 | `#define PH7_GLOB_MARK           0x0008` |
|         - | 4984 | `#define PH7_GLOB_NOCHECK        0x0010` |
|         - | 4985 | `#define PH7_GLOB_NOSORT         0x0020` |
|         - | 4986 | `#define PH7_GLOB_BRACE          0x0080` |
|         - | 4987 | `#define PH7_GLOB_NOESCAPE       0x1000` |
|         - | 4988 | `#define PH7_GLOB_ONLYDIR        0x40000000` |
|         - | 4989 | `#define PH7_INI_SCANNER_NORMAL  0` |
|         - | 4990 | `#define PH7_INI_SCANNER_RAW     1` |
|         - | 4991 | `#define PH7_INI_SCANNER_TYPED   2` |
|         - | 4992 | `/* php's INI_SCANNER_TYPED is 2 — not defined here because PHL does not register the` |
|         - | 4993 | ` * constant (nor honour any scanner mode yet); §5 tracks it with the missing JSON_*. */` |
|         - | 4994 | `/*` |
|         - | 4995 | ` * Each parsed URI is recorded and stored in an instance of the following structure.` |
|         - | 4996 | ` */` |
|         - | 4997 | `typedef struct SyhttpUri SyhttpUri;` |
|         - | 4998 | `struct SyhttpUri` |
|         - | 4999 | `{` |
|         - | 5000 | `	SyString sHost;     /* Hostname or IP address */` |
|         - | 5001 | `	SyString sPort;     /* Port number */` |
|         - | 5002 | `	SyString sPath;     /* Mandatory resource path passed verbatim (Not decoded) */` |
|         - | 5003 | `	SyString sQuery;    /* Query part */` |
|         - | 5004 | `	SyString sFragment; /* Fragment part */` |
|         - | 5005 | `	SyString sScheme;   /* Scheme */` |
|         - | 5006 | `	SyString sUser;     /* Username */` |
|         - | 5007 | `	SyString sPass;     /* Password */` |
|         - | 5008 | `	SyString sRaw;      /* Raw URI */` |
|         - | 5009 | `};` |
|         - | 5010 | `/*` |
|         - | 5011 | ` * An instance of the following structure is used to record all MIME headers seen` |
|         - | 5012 | ` * during a HTTP interaction.` |
|         - | 5013 | ` */` |
|         - | 5014 | `typedef struct SyhttpHeader SyhttpHeader;` |
|         - | 5015 | `struct SyhttpHeader` |
|         - | 5016 | `{` |
|         - | 5017 | `	SyString sName;    /* Header name [i.e:"Content-Type","Host","User-Agent"]. NOT NUL TERMINATED */` |
|         - | 5018 | `	SyString sValue;   /* Header values [i.e: "text/html"]. NOT NUL TERMINATED */` |
|         - | 5019 | `};` |
|         - | 5020 | `/*` |
|         - | 5021 | ` * Supported HTTP methods.` |
|         - | 5022 | ` */` |
|         - | 5023 | `#define HTTP_METHOD_GET  1 /* GET */` |
|         - | 5024 | `#define HTTP_METHOD_HEAD 2 /* HEAD */` |
|         - | 5025 | `#define HTTP_METHOD_POST 3 /* POST */` |
|         - | 5026 | `#define HTTP_METHOD_PUT  4 /* PUT */` |
|         - | 5027 | `#define HTTP_METHOD_OTHR 5 /* Other HTTP methods [i.e: DELETE,TRACE,OPTIONS...]*/` |
|         - | 5028 | `/*` |
|         - | 5029 | ` * Supported HTTP protocol version.` |
|         - | 5030 | ` */` |
|         - | 5031 | `#define HTTP_PROTO_10 1 /* HTTP/1.0 */` |
|         - | 5032 | `#define HTTP_PROTO_11 2 /* HTTP/1.1 */` |
|         - | 5033 | `/* memobj.c function prototypes */` |
|         - | 5034 | `PH7_PRIVATE sxi32 PH7_MemObjDump(SyBlob *pOut,ph7_value *pObj,int ShowType,int nTab,int nDepth,int isRef);` |
|         - | 5035 | `PH7_PRIVATE const char * PH7_MemObjTypeDump(ph7_value *pVal);` |
|         - | 5036 | `PH7_PRIVATE sxi32 PH7_MemObjAdd(ph7_value *pObj1,ph7_value *pObj2,int bAddStore);` |
|         - | 5037 | `PH7_PRIVATE sxi32 PH7_MemObjCmp(ph7_value *pObj1,ph7_value *pObj2,int bStrict,int iNest);` |
|         - | 5038 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromString(ph7_vm *pVm,ph7_value *pObj,const SyString *pVal);` |
|         - | 5039 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromArray(ph7_vm *pVm,ph7_value *pObj,ph7_hashmap *pArray);` |
|         - | 5040 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromReal(ph7_vm *pVm,ph7_value *pObj,ph7_real rVal);` |
|         - | 5041 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromInt(ph7_vm *pVm,ph7_value *pObj,sxi64 iVal);` |
|         - | 5042 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromBool(ph7_vm *pVm,ph7_value *pObj,sxi32 iVal);` |
|         - | 5043 | `PH7_PRIVATE sxi32 PH7_MemObjInit(ph7_vm *pVm,ph7_value *pObj);` |
|         - | 5044 | `PH7_PRIVATE sxi32 PH7_MemObjStringAppend(ph7_value *pObj,const char *zData,sxu32 nLen);` |
|         - | 5045 | `#if 0` |
|         - | 5046 | `/* Not used in the current release of the PH7 engine */` |
|         - | 5047 | `PH7_PRIVATE sxi32 PH7_MemObjStringFormat(ph7_value *pObj,const char *zFormat,va_list ap);` |
|         - | 5048 | `#endif` |
|         - | 5049 | `PH7_PRIVATE sxi32 PH7_MemObjStore(ph7_value *pSrc,ph7_value *pDest);` |
|         - | 5050 | `/*` |
|         - | 5051 | ` * The components php's parse_url() answers, and the split that produces them.` |
|         - | 5052 | ` * Shared with filter_var()'s FILTER_VALIDATE_URL, which php builds on the same` |
|         - | 5053 | ` * parse (a component that is ABSENT is what its b* flag reports; an empty one` |
|         - | 5054 | ` * is present-and-empty).` |
|         - | 5055 | ` */` |
|         - | 5056 | `typedef struct VmUrlParts VmUrlParts;` |
|         - | 5057 | `struct VmUrlParts` |
|         - | 5058 | `{` |
|         - | 5059 | `	SyString sScheme,sUser,sPass,sHost,sPath,sQuery,sFragment;` |
|         - | 5060 | `	int iPort;     /* Resolved port, meaningful only when bPort is set */` |
|         - | 5061 | `	sxu8 bScheme,bUser,bPass,bHost,bPort,bPath,bQuery,bFragment;` |
|         - | 5062 | `};` |
|         - | 5063 | `PH7_PRIVATE int PH7_VmUrlSplit(const char *z,int n,VmUrlParts *pOut);` |
|         - | 5064 | `/*` |
|         - | 5065 | ` * PHL_VALUE_CENSUS -- the value-primitive census (PERF.md §7, memobj.c has the` |
|         - | 5066 | ` * instrument and build-aux/valuecensus.sh drives it). Off in every shipping build.` |
|         - | 5067 | ` *` |
|         - | 5068 | ` * PHL_VC_DOOR is what makes a row a call SITE: the two hot doors below are inlined` |
|         - | 5069 | ` * in the build that ships, and __builtin_return_address(0) inside an INLINED function` |
|         - | 5070 | ` * names the caller's caller. Under the census they are compiled out of line, so a row` |
|         - | 5071 | `` * is the line that called. `unused` is on it because a translation unit that never`` |
|         - | 5072 | `` * calls the door would otherwise warn -- gcc refuses `inline` and `noinline` together,`` |
|         - | 5073 | ` * so the usual static-inline exemption is not available here.` |
|         - | 5074 | ` *` |
|         - | 5075 | ` * PHL_VCENSUS_CALLER lifts every row one frame -- the hashcensus -c convention, and` |
|         - | 5076 | ` * the same warning applies: it needs -fno-omit-frame-pointer, and it is how a row that` |
|         - | 5077 | ` * is a whole subsystem funnelled through one line gets decomposed.` |
|         - | 5078 | ` */` |
|         - | 5079 | `#if defined(PHL_VALUE_CENSUS)` |
|         - | 5080 | `#define PHL_VC_RELEASE 0` |
|         - | 5081 | `#define PHL_VC_LOAD    1` |
|         - | 5082 | `#define PHL_VC_STORE   2` |
|         - | 5083 | `#define PHL_VC_INIT    3` |
|         - | 5084 | `#define PHL_VC_KINDS   4` |
|         - | 5085 | `PH7_PRIVATE void PH7_ValueCensusNote(void *pSite,sxu32 iKind,int bWork);` |
|         - | 5086 | `#if defined(PHL_VCENSUS_CALLER)` |
|         - | 5087 | `#define PHL_VCENSUS_SITE() __builtin_return_address(1)` |
|         - | 5088 | `#else` |
|         - | 5089 | `#define PHL_VCENSUS_SITE() __builtin_return_address(0)` |
|         - | 5090 | `#endif` |
|         - | 5091 | `#define PHL_VC_NOTE(K,W) PH7_ValueCensusNote(PHL_VCENSUS_SITE(),(K),(W))` |
|         - | 5092 | `#define PHL_VC_DOOR static __attribute__((noinline,unused))` |
|         - | 5093 | `#else` |
|         - | 5094 | `#define PHL_VC_NOTE(K,W) ((void)0)` |
|         - | 5095 | `#define PHL_VC_DOOR SX_STATIC_INLINE` |
|         - | 5096 | `#endif` |
|         - | 5097 | `/*` |
|         - | 5098 | ` * Load an ALIASING copy of a value: the destination gets the scalar half verbatim, one` |
|         - | 5099 | ` * more reference on a container, and a READ-ONLY view of the source's string bytes. It is` |
|         - | 5100 | ` * how a variable, an element and a property all reach the operand stack, and it is the` |
|         - | 5101 | ` * engine's second-most-called function -- 1.34 BILLION times on the ecosystem gate's phpcs` |
|         - | 5102 | ` * step (counted, PERF.md §2), from only 62 call sites.` |
|         - | 5103 | ` *` |
|         - | 5104 | ` * Inline for the same reason SySetAt, PH7_MemObjAt and PH7_MemObjRelease are: the body is` |
|         - | 5105 | ` * a dozen instructions and it lived in memobj.c while every hot caller lived elsewhere, so` |
|         - | 5106 | ` * with no LTO every one of those 1.34 billion was a real call. Sixty-two sites is a cheap` |
|         - | 5107 | ` * place to spend that.` |
|         - | 5108 | ` *` |
|         - | 5109 | ` * The destination's blob is released first because a Load OVERWRITES it. On the workload of` |
|         - | 5110 | ` * record that branch was taken **0 times in 1.34 billion** -- the destination is nearly` |
|         - | 5111 | ` * always a fresh operand slot -- so it stays a call rather than more inline bytes.` |
|         - | 5112 | ` */` |
|  33198009 | 5113 | `PHL_VC_DOOR sxi32 PH7_MemObjLoad(ph7_value *pSrc,ph7_value *pDest)` |
|         5 | 5114 | `{` |
|         - | 5115 | `	PHL_VC_NOTE(PHL_VC_LOAD,(pSrc->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) != 0);` |
|  33198014 | 5116 | `	PH7_MEMOBJ_COPY_SCALAR(pDest,pSrc);` |
|         - | 5117 | `	/* D1 commit 2: a MEMOBJ_AUX_DEFPATH carrier OWNS its heap descriptor via x.pOther, and` |
|         - | 5118 | `	 * PH7_MemObjRelease frees it exactly once. An aliasing Load copies iFlags+x.pOther` |
|         - | 5119 | `	 * verbatim, so a Load-duplicated carrier would let two slots free the same descriptor.` |
|         - | 5120 | `	 * Carriers are transient (produced by LOAD_IDX/MEMBER, consumed at OP_CALL) and are never` |
|         - | 5121 | `	 * Load-copied today; strip the flag defensively so the invariant can't be violated. */` |
|  33198014 | 5122 | `	pDest->iFlags &= ~MEMOBJ_AUX_DEFPATH;` |
|  33198014 | 5123 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|         - | 5124 | `		/* Increment reference count */` |
|   1143114 | 5125 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  32626159 | 5126 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|         - | 5127 | `		/* Increment reference count */` |
|    565152 | 5128 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|    282351 | 5129 | `	}` |
|  33198014 | 5130 | `	if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|       211 | 5131 | `		SyBlobRelease(&pDest->sBlob);` |
|       103 | 5132 | `	}` |
|  33198014 | 5133 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  18456690 | 5134 | `		SyBlobReadOnly(&pDest->sBlob,SyBlobData(&pSrc->sBlob),SyBlobLength(&pSrc->sBlob));` |
|   9229761 | 5135 | `	}` |
|  33198014 | 5136 | `	return SXRET_OK;` |
|         5 | 5137 | `}` |
|         - | 5138 | `PH7_PRIVATE ph7_value * PH7_ValuePeek(ph7_value *pVal,ph7_value *pScratch);` |
|         - | 5139 | `PH7_PRIVATE sxi64 PH7_ValuePeekInt64(ph7_value *pVal);` |
|         - | 5140 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         - | 5141 | `PH7_PRIVATE ph7_real PH7_ValuePeekReal(ph7_value *pVal);` |
|         - | 5142 | `#endif` |
|         - | 5143 | `PH7_PRIVATE int PH7_ValuePeekBool(ph7_value *pVal);` |
|         - | 5144 | `PH7_PRIVATE sxi32 PH7_MemObjReleaseSlow(ph7_value *pObj);` |
|         - | 5145 | `/*` |
|         - | 5146 | ` * Drop whatever a value owns. THE most-called function in the engine: 2.72 billion` |
|         - | 5147 | ` * times on the ecosystem gate's phpcs step, out of ~4.5 billion calls into the four` |
|         - | 5148 | ` * value primitives together (counted, PERF.md §2).` |
|         - | 5149 | ` *` |
|         - | 5150 | ` * 43.1% of those calls -- 1.17 billion of them -- had NOTHING TO DO, and this test is` |
|         - | 5151 | ` * why they no longer make the call. A value already typed MEMOBJ_NULL owns no hashmap,` |
|         - | 5152 | `` * no instance, and no string (the slow path's own `(iFlags & MEMOBJ_NULL) == 0` guard is`` |
|         - | 5153 | ` * what skips SyBlobRelease, so a NULL value's blob is not released today either). What` |
|         - | 5154 | ` * IS still owned by a NULL-typed value is one of the three AUX carriers, each of which` |
|         - | 5155 | ` * holds a heap descriptor this is the universal free site for -- so they are the mask,` |
|         - | 5156 | `` * and they must stay in it: a `??=` peek, a __call carrier and a deferred-path lvalue`` |
|         - | 5157 | ` * are all NULL-typed by construction.` |
|         - | 5158 | ` *` |
|         - | 5159 | ` * Inline because the body it guards is three flag tests and a return for nearly half of` |
|         - | 5160 | ` * those 2.72 billion calls, and the call and return around them cost more than they do.` |
|         - | 5161 | ` * The same reason SySetAt and PH7_MemObjAt are inline.` |
|         - | 5162 | ` */` |
|         - | 5163 | `#define MEMOBJ_AUX_OWNED (MEMOBJ_AUX_COALSTROFF\|MEMOBJ_AUX_MAGICCALL\|MEMOBJ_AUX_DEFPATH)` |
| 118342321 | 5164 | `PHL_VC_DOOR sxi32 PH7_MemObjRelease(ph7_value *pObj)` |
|         5 | 5165 | `{` |
|         - | 5166 | `	PHL_VC_NOTE(PHL_VC_RELEASE,` |
|         - | 5167 | `		(pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_AUX_OWNED)) != MEMOBJ_NULL);` |
| 118342326 | 5168 | `	if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_AUX_OWNED)) == MEMOBJ_NULL ){` |
|  26219078 | 5169 | `		return SXRET_OK;   /* Owns nothing -- 43.1% of every release the engine makes */` |
|         - | 5170 | `	}` |
|  92123253 | 5171 | `	return PH7_MemObjReleaseSlow(pObj);` |
|  59159300 | 5172 | `}` |
|         - | 5173 | `PH7_PRIVATE sxi32 PH7_MemObjToNumeric(ph7_value *pObj);` |
|         - | 5174 | `PH7_PRIVATE sxi32 PH7_MemObjStringIncrement(ph7_value *pObj);` |
|         - | 5175 | `PH7_PRIVATE sxi32 PH7_MemObjTryInteger(ph7_value *pObj);` |
|         - | 5176 | `PH7_PRIVATE ProcMemObjCast PH7_MemObjCastMethod(sxi32 iFlags);` |
|         - | 5177 | `PH7_PRIVATE int PH7_MemObjStringIsNumeric(ph7_value *pValue);` |
|         - | 5178 | `PH7_PRIVATE int PH7_MemObjStringNumericPrefix(ph7_value *pValue,const char **pzTail);` |
|         - | 5179 | `PH7_PRIVATE sxi32 PH7_MemObjIsNumeric(ph7_value *pObj);` |
|         - | 5180 | `PH7_PRIVATE sxi32 PH7_MemObjIsEmpty(ph7_value *pObj);` |
|         - | 5181 | `PH7_PRIVATE sxi32 PH7_MemObjToHashmap(ph7_value *pObj);` |
|         - | 5182 | `PH7_PRIVATE sxi32 PH7_MemObjToObject(ph7_value *pObj);` |
|         - | 5183 | `PH7_PRIVATE sxi32 PH7_MemObjToString(ph7_value *pObj);` |
|         - | 5184 | `PH7_PRIVATE sxi32 PH7_MemObjToStringUV(ph7_value *pObj);` |
|         - | 5185 | `PH7_PRIVATE int PH7_MemObjIsNotStringable(ph7_value *pObj);` |
|         - | 5186 | `PH7_PRIVATE sxi32 PH7_MemObjToNull(ph7_value *pObj);` |
|         - | 5187 | `PH7_PRIVATE sxi32 PH7_MemObjToReal(ph7_value *pObj);` |
|         - | 5188 | `PH7_PRIVATE sxi32 PH7_MemObjToInteger(ph7_value *pObj);` |
|         - | 5189 | `PH7_PRIVATE sxi32 PH7_MemObjToBool(ph7_value *pObj);` |
|         - | 5190 | `PH7_PRIVATE int PH7_RealFitsInt64(double r);` |
|         - | 5191 | `PH7_PRIVATE sxi64 PH7_RealToInt64(double r);` |
|         - | 5192 | `PH7_PRIVATE void PH7_RealWarnIntCast(ph7_vm *pVm,double r);` |
|         - | 5193 | `PH7_PRIVATE void PH7_MemObjWarnIntCast(ph7_value *pObj);` |
|         - | 5194 | `PH7_PRIVATE sxi64 PH7_TokenValueToInt64(SyString *pData);` |
|         - | 5195 | `/* lex.c function prototypes */` |
|         - | 5196 | `PH7_PRIVATE sxi32 PH7_TokenizeRawText(const char *zInput,sxu32 nLen,SySet *pOut,sxu32 nBaseLine);` |
|         - | 5197 | `PH7_PRIVATE sxi32 PH7_TokenizePHP(const char *zInput,sxu32 nLen,sxu32 nLineStart,SySet *pOut,SySet *pTrivia);` |
|         - | 5198 | `/* vm.c function prototypes */` |
|         - | 5199 | `PH7_PRIVATE void PH7_VmReleaseContextValue(ph7_context *pCtx,ph7_value *pValue);` |
|         - | 5200 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(ph7_vm *pVm,ph7_vm_func *pFunc,const char *zName,sxu32 nByte,` |
|         - | 5201 | `	sxi32 iFlags,void *pUserData);` |
|         - | 5202 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(ph7_vm *pVm,ph7_vm_func *pFunc,SyString *pName);` |
|         - | 5203 | `PH7_PRIVATE SyHashEntry * PH7_VmGetUserFunction(ph7_vm *pVm,const void *pName,sxu32 nByte,int bEngineName);` |
|         - | 5204 | `PH7_PRIVATE SyHashEntry * PH7_VmCallSiteAnswer(ph7_vm *pVm,VmInstr *pInstr,const SyString *pName,int bEngineName,int *pbHost);` |
|         - | 5205 | `PH7_PRIVATE void PH7_VmCallSiteRecord(ph7_vm *pVm,VmInstr *pInstr,const SyString *pName,int bEngineName,int bHost,SyHashEntry *pEntry);` |
|         - | 5206 | `PH7_PRIVATE void PH7_VmCallSiteReleaseChunk(ph7_vm *pVm,SySet *pByteCode);` |
|         - | 5207 | `PH7_PRIVATE SyHashEntry * PH7_VmConstSiteAnswer(ph7_vm *pVm,VmInstr *pInstr);` |
|         - | 5208 | `PH7_PRIVATE void PH7_VmConstSiteRecord(ph7_vm *pVm,VmInstr *pInstr,SyHashEntry *pEntry);` |
|         - | 5209 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(ph7_vm *pVm,ph7_class_instance *pObj);` |
|         - | 5210 | `PH7_PRIVATE ph7_value * PH7_VmCreateDynamicAttr(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nName,VmClassAttr **ppAttr);` |
|         - | 5211 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(ph7_vm *pVm,sxu32 nIdx,SyHashEntry *pEntry,ph7_hashmap_node *pMapEntry);` |
|         - | 5212 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(ph7_vm *pVm,sxu32 nIdx,SyHashEntry *pEntry,ph7_hashmap_node *pMapEntry,sxi32 iFlags);` |
|         - | 5213 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew);` |
|         - | 5214 | `PH7_PRIVATE int PH7_VmIncludePathSep(void);` |
|         - | 5215 | `PH7_PRIVATE void PH7_VmSetIncludePath(ph7_vm *pVm,const char *zPath,sxu32 nByte);` |
|         - | 5216 | `PH7_PRIVATE void PH7_VmGetIncludePath(ph7_vm *pVm,SyBlob *pOut);` |
|         - | 5217 | `PH7_PRIVATE int PH7_VmExecutingDir(ph7_vm *pVm,SyString *pOut);` |
|         - | 5218 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable,sxi32 iNest);` |
|         - | 5219 | `PH7_PRIVATE void PH7_VmClassNameAnchor(const char **pzName,sxu32 *pnByte);` |
|         - | 5220 | `PH7_PRIVATE sxi32 PH7_VmMaterializeClassConst(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr);` |
|         - | 5221 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable);` |
|         - | 5222 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(ph7_vm *pVm,const SyString *pName,ProcConstant xExpand,void *pUserData);` |
|         - | 5223 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstantEx(ph7_vm *pVm,const SyString *pName,ProcConstant xExpand,` |
|         - | 5224 | `	void *pUserData,const SyString *pFile,sxu32 nLine,int bUser);` |
|         - | 5225 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(ph7_vm *pVm,const SyString *pName,ProchHostFunction xFunc,void *pUserData);` |
|         - | 5226 | `/* Builds a ph7_user_func WITHOUT registering it as a global name. The native-class` |
|         - | 5227 | ` * builder uses it for method bodies, which are reachable only through their class. */` |
|         - | 5228 | `PH7_PRIVATE sxi32 PH7_NewForeignFunction(ph7_vm *pVm,const SyString *pName,ProchHostFunction xFunc,` |
|         - | 5229 | `	void *pUserData,ph7_user_func **ppOut);` |
|         - | 5230 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(ph7_vm *pVm,ph7_class *pClass);` |
|         - | 5231 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(const void *pSrc,unsigned int nLen,void *pUserData);` |
|         - | 5232 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm);` |
|         - | 5233 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex);` |
|         - | 5234 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(ph7_vm *pVm,SyString *pString);` |
|         - | 5235 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(ph7_vm *pVm,const char *zFormat,va_list ap);` |
|         - | 5236 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap);` |
|         - | 5237 | `PH7_PRIVATE sxi32 PH7_VmThrowError(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zMessage);` |
|         - | 5238 | `PH7_PRIVATE sxi32 PH7_VmMemoryError(ph7_vm *pVm);` |
|         - | 5239 | `PH7_PRIVATE sxi32 PH7_ContextMemoryError(ph7_context *pCtx);` |
|         - | 5240 | `PH7_PRIVATE sxu32 PH7_VmResourceId(ph7_vm *pVm,void *pRes);` |
|         - | 5241 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...);` |
|         - | 5242 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionCode(ph7_context *pCtx,const char *zClass,sxi32 iCode,const char *zFormat,...);` |
|         - | 5243 | `PH7_PRIVATE sxi32 VmHostFuncThrowRc(ph7_context *pCtx,sxi32 rc);` |
|         - | 5244 | `PH7_PRIVATE sxi32 PH7_VmThrowArrayNextIndexError(ph7_vm *pVm);` |
|         - | 5245 | `PH7_PRIVATE sxi32 PH7_VmThrowGlobalsAppendError(ph7_vm *pVm);` |
|         - | 5246 | `PH7_PRIVATE sxi32 PH7_VmInstallGlobalVar(ph7_vm *pVm,const char *zName,sxu32 nByte,ph7_value *pValue,sxu32 nRefIdx);` |
|         - | 5247 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...);` |
|         - | 5248 | `PH7_PRIVATE void  PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData);` |
|         - | 5249 | `PH7_PRIVATE sxi32 PH7_VmDump(ph7_vm *pVm,ProcConsumer xConsumer,void *pUserData);` |
|         - | 5250 | `PH7_PRIVATE sxi32 PH7_VmInstallDateTime(ph7_vm *pVm);` |
|         - | 5251 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 5252 | `/* Shared between builtin_date.c (procedural date functions) and` |
|         - | 5253 | ` * builtin_date_parse.c (the DateTime family) */` |
|         - | 5254 | `PH7_PRIVATE sxi32 DateFormat(ph7_context *pCtx,const char *zIn,int nLen,Sytm *pTm,int uSec);` |
|         - | 5255 | `PH7_PRIVATE sxi64 DtDaysFromCivil(sxi64 y,int m,int d);` |
|         - | 5256 | `PH7_PRIVATE void DtCivilFromDays(sxi64 z,sxi64 *py,int *pm,int *pd);` |
|         - | 5257 | `PH7_PRIVATE sxi64 DtFloorDiv(sxi64 a,sxi64 b);` |
|         - | 5258 | `PH7_PRIVATE void DtFillSytm(sxi64 iTs,sxi32 iOff,char *zZone,Sytm *pTm);` |
|         - | 5259 | `PH7_PRIVATE void DtNowUs(ph7_vm *pVm,sxi64 *piSec,int *puSec);` |
|         - | 5260 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 5261 | `PH7_PRIVATE const char * PH7_VmBuiltinSigLookup(const char *zName,sxu32 nLen,const char **pzRet);` |
|         - | 5262 | `PH7_PRIVATE void PH7_VmStoreArgByRef(ph7_vm *pVm,ph7_value *pArg,ph7_value *pNewVal);` |
|         - | 5263 | `PH7_PRIVATE sxi32 PH7_ValueToStringUV(ph7_context *pCtx,ph7_value *pValue,const char **pzData,int *pnLen);` |
|         - | 5264 | `PH7_PRIVATE void PH7_ValueToStringUVOnce(ph7_context *pCtx,ph7_value *pValue,const char **pzData,int *pnLen);` |
|         - | 5265 | `PH7_PRIVATE ph7_class *PH7_ValueToStringUVDefer(ph7_context *pCtx,ph7_value *pValue,const char **pzData,int *pnLen);` |
|         - | 5266 | `PH7_PRIVATE void PH7_VmThrowWarningFmt(ph7_vm *pVm,const char *zFmt,...);` |
|         - | 5267 | `PH7_PRIVATE sxi32 PH7_CheckCallbackArg(ph7_context *pCtx,ph7_value *pCb,int iArg,const char *zParam,int bNullable);` |
|         - | 5268 | `PH7_PRIVATE sxi32 PH7_VmInstallReflection(ph7_vm *pVm);` |
|         - | 5269 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionTypes(ph7_vm *pVm); /* vm_builtin_reflection.c */` |
|         - | 5270 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionSmall(ph7_vm *pVm); /* vm_builtin_reflection.c */` |
|         - | 5271 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionAttribute(ph7_vm *pVm); /* vm_builtin_reflection.c */` |
|         - | 5272 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionEnum(ph7_vm *pVm); /* vm_builtin_reflection.c */` |
|         - | 5273 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionClass(ph7_vm *pVm); /* vm_builtin_reflection.c */` |
|         - | 5274 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionFunc(ph7_vm *pVm);  /* vm_builtin_reflection.c */` |
|         - | 5275 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionHookType(ph7_vm *pVm); /* vm_builtin_reflection.c */` |
|         - | 5276 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionMember(ph7_vm *pVm); /* vm_builtin_reflection.c */` |
|         - | 5277 | `PH7_PRIVATE sxi32 PH7_ClosurePresent(ph7_vm *pVm,ph7_class_instance *pThis,` |
|         - | 5278 | `	ph7_value *pOut,int bDebug); /* vm_builtin_reflection.c: Closure's ph7_class::xPresent */` |
|         - | 5279 | `PH7_PRIVATE sxi32 PH7_VmInstallBuiltinLib(ph7_vm *pVm); /* vm_builtin_lib.c */` |
|         - | 5280 | `PH7_PRIVATE ph7_class_instance * PH7_VmNewClosure(ph7_vm *pVm,const SyString *pName,` |
|         - | 5281 | `	ph7_class_instance *pBoundThis,const SyString *pScope);` |
|         - | 5282 | `PH7_PRIVATE sxi32 PH7_VmEnforcePropStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue);` |
|         - | 5283 | `PH7_PRIVATE int PH7_VmSlotRefCount(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 5284 | `PH7_PRIVATE sxi32 PH7_VmInit(ph7_vm *pVm,ph7 *pEngine);` |
|         - | 5285 | `PH7_PRIVATE sxi32 PH7_VmConfigure(ph7_vm *pVm,sxi32 nOp,va_list ap);` |
|         - | 5286 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm);` |
|         - | 5287 | `/* Fiber API helpers (used by api.c) */` |
|         - | 5288 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal);` |
|         - | 5289 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult);` |
|         - | 5290 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult);` |
|         - | 5291 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber);` |
|         - | 5292 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber);` |
|         - | 5293 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber);` |
|         - | 5294 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm);` |
|         - | 5295 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm);` |
|         - | 5296 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(ph7_vm *pVm);` |
|         - | 5297 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm);` |
|         - | 5298 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm);` |
|         - | 5299 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm);` |
|         - | 5300 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm);` |
|         - | 5301 | `PH7_PRIVATE VmInstr *PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex);` |
|         - | 5302 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm);` |
|         - | 5303 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer);` |
|         - | 5304 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(ph7_vm *pVm,sxi32 iOp,sxi32 iP1,sxu32 iP2,void *p3,sxu32 *pIndex);` |
|         - | 5305 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm);` |
|         - | 5306 | `/* The wall clock as php reads it: epoch seconds and the sub-second microseconds,` |
|         - | 5307 | ` * through whatever source this build/embedder has (see DateNow). uniqid() is the` |
|         - | 5308 | ` * second caller after the date surface itself. */` |
|         - | 5309 | `PH7_PRIVATE void PH7_VmClockNow(ph7_vm *pVm,ph7_int64 *pSec,ph7_int64 *pUsec);` |
|         - | 5310 | ``/* php's `php_combined_lcg()`: a double in [0,1) from the two L'Ecuyer streams on`` |
|         - | 5311 | ` * the VM. Seeds them on first use. */` |
|         - | 5312 | `PH7_PRIVATE double PH7_VmCombinedLcg(ph7_vm *pVm);` |
|         - | 5313 | `PH7_PRIVATE void PH7_VmMtSrand(ph7_vm *pVm,sxu32 nSeed,int bLegacyTwist);` |
|         - | 5314 | `PH7_PRIVATE sxu32 PH7_VmMtRand(ph7_vm *pVm);` |
|         - | 5315 | `PH7_PRIVATE sxi64 PH7_VmMtRandRange(ph7_vm *pVm,sxi64 iMin,sxi64 iMax);` |
|         - | 5316 | `PH7_PRIVATE void PH7_VmReleaseInstanceAttr(ph7_vm *pVm,VmClassAttr *pVmAttr);` |
|         - | 5317 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_method *pMethod,` |
|         - | 5318 | `	ph7_value *pResult,int nArg,ph7_value **apArg);` |
|         - | 5319 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethodMap(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_method *pMethod,` |
|         - | 5320 | `	ph7_value *pResult,int nArg,ph7_value **apArg,VmCallArgMap *pMap);` |
|         - | 5321 | `PH7_PRIVATE sxi32 PH7_VmCallMethodSwallow(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_method *pMethod,` |
|         - | 5322 | `	ph7_value *pResult,int nArg,ph7_value **apArg,int *pbThrew);` |
|         - | 5323 | `PH7_PRIVATE sxi32 PH7_VmCallMethodUnchecked(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_method *pMethod,` |
|         - | 5324 | `	ph7_value *pResult,int nArg,ph7_value **apArg);` |
|         - | 5325 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(ph7_vm *pVm,ph7_value *pFunc,int nArg,ph7_value **apArg,ph7_value *pResult);` |
|         - | 5326 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionWithMap(ph7_vm *pVm,ph7_value *pFunc,int nArg,ph7_value **apArg,ph7_value *pResult,VmCallArgMap *pArgMap);` |
|         - | 5327 | `PH7_PRIVATE ph7_class_instance * PH7_VmCallerThisFor(ph7_vm *pVm,ph7_class *pClass);` |
|         - | 5328 | `PH7_PRIVATE ph7_class_instance * PH7_VmStaticFallbackThis(ph7_vm *pVm,ph7_class *pClass);` |
|         - | 5329 | `PH7_PRIVATE sxi32 PH7_VmDispatchMagicCall(ph7_vm *pVm,ph7_class *pClass,ph7_class_instance *pThis,` |
|         - | 5330 | `	const char *zName,sxu32 nName,ph7_value *pResult,int nArg,ph7_value **apArg,VmCallArgMap *pArgMap);` |
|         - | 5331 | `/* Per-element callback for PH7_VmIteratorWalk: return SXRET_OK to continue,` |
|         - | 5332 | ` * SXERR_EOF to stop early (not an error), or PH7_EXCEPTION/PH7_ABORT to propagate. */` |
|         - | 5333 | `typedef sxi32 (*ProcIterStep)(ph7_vm *pVm,ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|         - | 5334 | `PH7_PRIVATE sxi32 PH7_VmIteratorWalk(ph7_vm *pVm,ph7_value *pObj,ProcIterStep xStep,void *pUserData);` |
|         - | 5335 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(ph7_vm *pVm,ph7_value *pFunc,ph7_value *pResult,...);` |
|         - | 5336 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce);` |
|         - | 5337 | `PH7_PRIVATE void PH7_VmWarnByRefArgsGivenValue(ph7_vm *pVm,ph7_value *pCallable,int nArg,ph7_hashmap_node **apNode,SyString *aNames);` |
|         - | 5338 | `PH7_PRIVATE void PH7_VmCufDropByRefArgs(ph7_context *pCtx,ph7_value *pCallable,int nArg,ph7_value **apArg);` |
|         - | 5339 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen);` |
|         - | 5340 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm);` |
|         - | 5341 | `PH7_PRIVATE ph7_class * PH7_VmPeekSelfClass(ph7_vm *pVm);` |
|         - | 5342 | `PH7_PRIVATE ph7_class * PH7_VmTraitUsingClass(ph7_vm *pVm,ph7_class *pTrait,ph7_class *pFrom);` |
|         - | 5343 | `PH7_PRIVATE ph7_class * PH7_VmMemberOwnerClass(ph7_class *pDeclClass,ph7_class *pFrom);` |
|         - | 5344 | `PH7_PRIVATE int PH7_VmEvalConstExpr(ph7_vm *pVm,SySet *pByteCode,ph7_value *pOut);` |
|         - | 5345 | `PH7_PRIVATE void PH7_ClassRenderDecl(ph7_vm *pVm,ph7_class *pScope,ph7_vm_func *pFunc,SyBlob *pOut);` |
|         - | 5346 | `PH7_PRIVATE sxi32 PH7_ClassCheckOverrideCompat(ph7_gen_state *pGen,ph7_class *pBase,ph7_class *pSub,` |
|         - | 5347 | `	ph7_class_method *pParent,ph7_class_method *pChild,int bCtorExempt);` |
|         - | 5348 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm);` |
|         - | 5349 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke);` |
|         - | 5350 | `PH7_PRIVATE int PH7_VmArrayCallableParts(ph7_vm *pVm,ph7_hashmap *pMap,ph7_value **ppTarget,ph7_value **ppMethod);` |
|         - | 5351 | `PH7_PRIVATE int PH7_VmCallableMethodAccessible(ph7_vm *pVm,ph7_class *pClass,ph7_class_method *pMethod);` |
|         - | 5352 | `PH7_PRIVATE int PH7_VmCallableStringParts(const char *zName,sxu32 nName,` |
|         - | 5353 | `	const char **pzCls,sxu32 *pnCls,const char **pzMeth,sxu32 *pnMeth);` |
|         - | 5354 | `PH7_PRIVATE int PH7_VmClassLookupRaised(ph7_vm *pVm,sxi32 nBrcBefore,const void *pResumeBefore);` |
|         - | 5355 | `PH7_PRIVATE const char * PH7_VmCallableReason(ph7_vm *pVm,ph7_value *pValue,char *zBuf,int nBuf);` |
|         - | 5356 | `PH7_PRIVATE void PH7_VmCallableName(ph7_vm *pVm,ph7_value *pValue,SyBlob *pOut);` |
|         - | 5357 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(ph7_vm *pVm,const char *zName,sxu32 nByte);` |
|         - | 5358 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(ph7_hashmap *pMap,const char *zKey,int nKeylen,const char *zData,int nLen);` |
|         - | 5359 | `/* The file:// strip is pure string work and the VFS layer needs it in every` |
|         - | 5360 | ` * build, disk IO enabled or not. */` |
|         - | 5361 | `PH7_PRIVATE const char * PH7_VmFileUrlLocalPath(const char *zPath);` |
|         - | 5362 | `PH7_PRIVATE int PH7_VmUrlSchemeLen(const char *zIn,int nByte);` |
|         - | 5363 | `#ifndef PH7_DISABLE_DISK_IO` |
|         - | 5364 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(ph7_vm *pVm,const char **pzDevice,int nByte);` |
|         - | 5365 | `PH7_PRIVATE int PH7_VmStreamDeviceSuppressed(ph7_vm *pVm,const ph7_io_stream *pStream);` |
|         - | 5366 | `PH7_PRIVATE ph7_io_stream * PH7_VmFindStreamDevice(ph7_vm *pVm,const char *zName,int nName);` |
|         - | 5367 | `PH7_PRIVATE int PH7_VmStreamSchemeDisabled(ph7_vm *pVm,const char *zName,int nName);` |
|         - | 5368 | `PH7_PRIVATE int PH7_VmStreamDeviceIsRemoteHost(const char *zUri,int nByte,int *pnScheme);` |
|         - | 5369 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|         - | 5370 | `/* vm_http.c function prototypes */` |
|         - | 5371 | `PH7_PRIVATE sxi32 PH7_VmHttpSplitURI(SyhttpUri *pOut,const char *zUri,sxu32 nLen);` |
|         - | 5372 | `PH7_PRIVATE sxi32 PH7_VmHttpProcessRequest(ph7_vm *pVm,const char *zRequest,int nByte);` |
|         - | 5373 | `/* vm_http_response.c function prototypes */` |
|         - | 5374 | `PH7_PRIVATE void PH7_RegisterHttpResponseFunctions(ph7_vm *pVm);` |
|         - | 5375 | `PH7_PRIVATE void PH7_VmReleaseResponseHeaders(ph7_vm *pVm);` |
|         - | 5376 | `PH7_PRIVATE int PH7_VmHttpDate(sxi64 iWhen,char *zBuf,int nBuf);` |
|         - | 5377 | `PH7_PRIVATE void PH7_VmSetResponseHeader(ph7_vm *pVm,const char *zName,const char *zValue,sxu32 nValue);` |
|         - | 5378 | `PH7_PRIVATE void PH7_VmRemoveCookieByName(ph7_vm *pVm,const char *zName,sxu32 nName);` |
|         - | 5379 | `PH7_PRIVATE void PH7_VmEmitCookie(ph7_vm *pVm,const char *zName,sxu32 nName,` |
|         - | 5380 | `	const char *zValue,sxu32 nValue,int bEncode,sxi64 iExpires,` |
|         - | 5381 | `	const char *zPath,sxu32 nPath,const char *zDomain,sxu32 nDomain,` |
|         - | 5382 | `	int bSecure,int bHttpOnly,const char *zSame,sxu32 nSame,int bPartitioned);` |
|         - | 5383 | `/* vm_pcre.c function prototypes */` |
|         - | 5384 | `#ifdef PH7_ENABLE_PCRE` |
|         - | 5385 | `PH7_PRIVATE void PH7_RegisterPcreFunctions(ph7_vm *pVm);` |
|         - | 5386 | `PH7_PRIVATE void PH7_RegisterPcreConstants(ph7_vm *pVm);` |
|         - | 5387 | `PH7_PRIVATE sxi32 PH7_PcreMatchQuiet(ph7_context *pCtx,const char *zPat,int nPat,` |
|         - | 5388 | `	const char *zSub,int nSub,int *pMatched);` |
|         - | 5389 | `PH7_PRIVATE int PH7_PcrePatternCheck(ph7_vm *pVm,const char *zPattern,int nLen,` |
|         - | 5390 | `	char *zErr,sxu32 nErr);` |
|         - | 5391 | `/* RegexIterator's five operation modes, php's REGIT_MODE_* values — they are the` |
|         - | 5392 | ` * class constants, so the numbers are php-visible and fixed. */` |
|         - | 5393 | `#define PH7_REGIT_MATCH        0` |
|         - | 5394 | `#define PH7_REGIT_GET_MATCH    1` |
|         - | 5395 | `#define PH7_REGIT_ALL_MATCHES  2` |
|         - | 5396 | `#define PH7_REGIT_SPLIT        3` |
|         - | 5397 | `#define PH7_REGIT_REPLACE      4` |
|         - | 5398 | `PH7_PRIVATE sxi32 PH7_PcreRegitApply(ph7_context *pCtx,int iMode,ph7_value *pPattern,` |
|         - | 5399 | `	ph7_value *pSubject,int iPregFlags,ph7_value *pRepl,ph7_value *pOut,int *pbOk);` |
|         - | 5400 | `#endif /* PH7_ENABLE_PCRE */` |
|         - | 5401 | `/* One resource pointer's php-visible id. Allocated per distinct resource and` |
|         - | 5402 | `` * owned by ph7_vm.hResourceId, whose key is the `pRes` field itself (SyHash`` |
|         - | 5403 | ` * stores the key POINTER, so it must outlive the entry). Core (used by` |
|         - | 5404 | ` * PH7_VmResourceId) — must NOT sit under PH7_ENABLE_LIBXML or the tiny build,` |
|         - | 5405 | ` * which omits libxml, fails to compile it. */` |
|         - | 5406 | `typedef struct phl_res_id phl_res_id;` |
|         - | 5407 | `struct phl_res_id {` |
|         - | 5408 | `	void *pRes;   /* The resource pointer, and the hash key */` |
|         - | 5409 | `	sxu32 nId;    /* php-visible id */` |
|         - | 5410 | `};` |
|         - | 5411 | `#ifdef PH7_ENABLE_LIBXML` |
|         - | 5412 | `/* One entry in the per-VM libxml error queue (mirrors php's LibXMLError:` |
|         - | 5413 | ` * level/code/column/message/file/line).  Strings are SyMemBackend copies` |
|         - | 5414 | ` * owned by the queue and released by PH7_LibxmlClearErrors(). */` |
|         - | 5415 | `typedef struct phl_libxml_err phl_libxml_err;` |
|         - | 5416 | `struct phl_libxml_err {` |
|         - | 5417 | `	int iLevel;      /* LIBXML_ERR_WARNING/ERROR/FATAL */` |
|         - | 5418 | `	int iCode;       /* raw libxml2 error code */` |
|         - | 5419 | `	int iLine;` |
|         - | 5420 | `	int iColumn;` |
|         - | 5421 | `	SyString sMsg;   /* message text, trailing newline preserved (php parity) */` |
|         - | 5422 | `	SyString sFile;  /* source file/URI, empty for in-memory strings */` |
|         - | 5423 | `};` |
|         - | 5424 | `/* Per-VM owner of one libxml document tree.  See the lifetime notes at the` |
|         - | 5425 | ` * top of vm_libxml.c: docs are only freed at VM reset/release, never while` |
|         - | 5426 | ` * PHP code could still hold a wrapper into them. */` |
|         - | 5427 | `typedef struct phl_xmldoc phl_xmldoc;` |
|         - | 5428 | `struct phl_xmldoc {` |
|         - | 5429 | `	void *pDoc;         /* xmlDocPtr (void* keeps libxml headers out of ph7int.h) */` |
|         - | 5430 | `	SySet aOrphans;     /* xmlNodePtr's unlinked from the tree but still owned */` |
|         - | 5431 | `	/* The stand-in NODES a DTD's NOTATION declarations are answered through:` |
|         - | 5432 | `	 * an xmlNotation is {name, PublicID, SystemID} and nothing else -- it has` |
|         - | 5433 | `	 * no type field, so it cannot be walked as a node -- and php builds an` |
|         - | 5434 | `	 * entity-shaped node per lookup. One per declaration is built here and` |
|         - | 5435 | `	 * kept, so the wrapper identity every other node has holds for these too;` |
|         - | 5436 | `	 * they need their own free (vm_libxml.c), since xmlFreeNode would read` |
|         - | 5437 | `	 * an xmlEntity's length/etype pair as a node's property list. */` |
|         - | 5438 | `	SySet aNotations;   /* synthesized XML_NOTATION_NODE xmlNodePtr's */` |
|         - | 5439 | `	ph7_vm *pVm;        /* Owning VM (error routing from libxml callbacks) */` |
|         - | 5440 | `	void *pDocObj;      /* The DOMDocument wrapper for this tree, BORROWED, or 0.` |
|         - | 5441 | `	                     * ext/dom keys its per-node wrapper cache on the document` |
|         - | 5442 | ``	                     * OBJECT, so `dom_import_simplexml()` needs the one this`` |
|         - | 5443 | `	                     * tree already has -- that is what makes two imports of the` |
|         - | 5444 | `	                     * same node the same DOMElement, and what makes an import` |
|         - | 5445 | `	                     * back out of a SimpleXML that came FROM a DOMDocument` |
|         - | 5446 | `	                     * answer that document's own nodes. Cleared by` |
|         - | 5447 | `	                     * DOMDocument's xRelease when the object goes, so the` |
|         - | 5448 | `	                     * pointer is never stale. */` |
|         - | 5449 | `	int bPreserveWS;    /* DOMDocument->preserveWhiteSpace */` |
|         - | 5450 | `	int bFormatOutput;  /* DOMDocument->formatOutput */` |
|         - | 5451 | `	phl_xmldoc *pNext;  /* Registry chain (pVm->pXmlDocs) */` |
|         - | 5452 | `};` |
|         - | 5453 | `/* One PHP-visible DOM node handle: the MEMOBJ_RES payload behind every DOM` |
|         - | 5454 | ` * wrapper object.  pNode points into pShell's tree (or IS the xmlDoc); the` |
|         - | 5455 | ` * shell outlives every handle (docs are only freed at VM reset/release). */` |
|         - | 5456 | `typedef struct phl_domnode phl_domnode;` |
|         - | 5457 | `struct phl_domnode {` |
|         - | 5458 | `	phl_xmldoc *pShell; /* Owning document registry entry */` |
|         - | 5459 | `	void *pNode;        /* xmlNodePtr / xmlDocPtr / xmlAttrPtr */` |
|         - | 5460 | `};` |
|         - | 5461 | `/* vm_libxml.c */` |
|         - | 5462 | `PH7_PRIVATE void PH7_RegisterLibxmlConstants(ph7_vm *pVm);` |
|         - | 5463 | `PH7_PRIVATE sxi32 PH7_VmInstallLibxml(ph7_vm *pVm);` |
|         - | 5464 | `PH7_PRIVATE void PH7_LibxmlVmReset(ph7_vm *pVm);` |
|         - | 5465 | `PH7_PRIVATE void PH7_LibxmlVmRelease(ph7_vm *pVm);` |
|         - | 5466 | `PH7_PRIVATE void PH7_LibxmlClearErrors(ph7_vm *pVm);` |
|         - | 5467 | `PH7_PRIVATE phl_xmldoc * PH7_LibxmlNewDoc(ph7_vm *pVm,void *pXmlDocPtr);` |
|         - | 5468 | `PH7_PRIVATE sxu32 PH7_LibxmlCaptureBegin(ph7_vm *pVm);` |
|         - | 5469 | `PH7_PRIVATE sxu32 PH7_LibxmlCaptureBeginRaw(ph7_vm *pVm);` |
|         - | 5470 | `PH7_PRIVATE void PH7_LibxmlCaptureEnd(ph7_vm *pVm,sxu32 nMark,const char *zFnName);` |
|         - | 5471 | `PH7_PRIVATE void PH7_LibxmlDropErrors(ph7_vm *pVm,sxu32 nMark);` |
|         - | 5472 | `/* Push one error onto the per-VM queue + last-error slot (strings copied).` |
|         - | 5473 | ` * The shared structured-error callback and the DOM schema error hooks both` |
|         - | 5474 | ` * funnel through this so ph7int.h needs no libxml types. */` |
|         - | 5475 | `PH7_PRIVATE void PH7_LibxmlCaptureEndOpts(ph7_vm *pVm,sxu32 nMark,const char *zFnName,int iOpts);` |
|         - | 5476 | `PH7_PRIVATE void PH7_LibxmlRaiseGeneric(ph7_vm *pVm,const char *zFnName,const char *zMsg);` |
|         - | 5477 | `PH7_PRIVATE void PH7_LibxmlQueueError(ph7_vm *pVm,int iLevel,int iCode,int iLine,int iColumn,` |
|         - | 5478 | `	const char *zMsg,const char *zFile);` |
|         - | 5479 | `/* vm_dom.c */` |
|         - | 5480 | `PH7_PRIVATE sxi32 PH7_VmInstallDom(ph7_vm *pVm);` |
|         - | 5481 | `/* Read a document's bytes for a loader, through php's own stream layer, with` |
|         - | 5482 | `` * libxml's `failed to load external entity` warning already raised for a file`` |
|         - | 5483 | ` * that is not there. ext/simplexml's two file doors want exactly what` |
|         - | 5484 | ` * DOMDocument::load() wants. */` |
|         - | 5485 | `PH7_PRIVATE int PH7_DomReadFile(ph7_context *pCtx,const char *zFile,int nFile,const char *zFn,` |
|         - | 5486 | `	SyBlob *pBody,SyBlob *pPath);` |
|         - | 5487 | `/* The DOMDocument-cached wrapper for one node of pShell's tree, creating the` |
|         - | 5488 | ` * document object if this tree has none yet. ext/simplexml's` |
|         - | 5489 | ` * dom_import_simplexml() is the only caller: every other wrap already has a` |
|         - | 5490 | ` * document object in hand. */` |
|         - | 5491 | `PH7_PRIVATE ph7_class_instance * PH7_DomWrapForeign(ph7_vm *pVm,phl_xmldoc *pShell,void *pNode);` |
|         - | 5492 | `/* vm_simplexml.c */` |
|         - | 5493 | `PH7_PRIVATE sxi32 PH7_VmInstallSimpleXml(ph7_vm *pVm);` |
|         - | 5494 | `/* vm_xmlwriter.c */` |
|         - | 5495 | `PH7_PRIVATE sxi32 PH7_VmInstallXmlWriter(ph7_vm *pVm);` |
|         - | 5496 | `PH7_PRIVATE void PH7_XmlWriterVmSweep(ph7_vm *pVm);` |
|         - | 5497 | `/* vm_xml.c (php's ext/xml: the expat-style push-parser surface over libxml2) */` |
|         - | 5498 | `PH7_PRIVATE sxi32 PH7_VmInstallXml(ph7_vm *pVm);` |
|         - | 5499 | `PH7_PRIVATE void PH7_XmlParserVmSweep(ph7_vm *pVm);` |
|         - | 5500 | `#endif /* PH7_ENABLE_LIBXML */` |
|         - | 5501 | `#ifdef PH7_ENABLE_SQLITE` |
|         - | 5502 | `/* vm_pdo.c (ext/pdo: the driver-independent class library) */` |
|         - | 5503 | `PH7_PRIVATE sxi32 PH7_VmInstallPdo(ph7_vm *pVm);` |
|         - | 5504 | `PH7_PRIVATE void PH7_PdoVmReset(ph7_vm *pVm);` |
|         - | 5505 | `PH7_PRIVATE void PH7_PdoVmRelease(ph7_vm *pVm);` |
|         - | 5506 | `/* vm_pdo_sqlite.c (ext/pdo_sqlite: the driver and its Pdo\Sqlite subclass) */` |
|         - | 5507 | `PH7_PRIVATE sxi32 PH7_VmInstallPdoSqlite(ph7_vm *pVm);` |
|         - | 5508 | `/* vm_sqlite3.c (ext/sqlite3: php's other sqlite surface, the SQLite3 class family) */` |
|         - | 5509 | `PH7_PRIVATE sxi32 PH7_VmInstallSqlite3(ph7_vm *pVm);` |
|         - | 5510 | `PH7_PRIVATE void PH7_RegisterSqlite3Constants(ph7_vm *pVm);` |
|         - | 5511 | `PH7_PRIVATE void PH7_Sqlite3VmReset(ph7_vm *pVm);` |
|         - | 5512 | `PH7_PRIVATE void PH7_Sqlite3VmRelease(ph7_vm *pVm);` |
|         - | 5513 | `#endif /* PH7_ENABLE_SQLITE */` |
|         - | 5514 | `#ifdef PH7_ENABLE_CURL` |
|         - | 5515 | `/* vm_curl.c (ext/curl: php's libcurl binding) */` |
|         - | 5516 | `PH7_PRIVATE sxi32 PH7_VmInstallCurl(ph7_vm *pVm);` |
|         - | 5517 | `PH7_PRIVATE void PH7_RegisterCurlConstants(ph7_vm *pVm);` |
|         - | 5518 | `PH7_PRIVATE void PH7_CurlVmReset(ph7_vm *pVm);` |
|         - | 5519 | `PH7_PRIVATE void PH7_CurlVmRelease(ph7_vm *pVm);` |
|         - | 5520 | `#endif /* PH7_ENABLE_CURL */` |
|         - | 5521 | `/* net.c types and function prototypes */` |
|         - | 5522 | `#ifdef PH7_ENABLE_NET` |
|         - | 5523 | `#ifdef __WINNT__` |
|         - | 5524 | `#include <winsock2.h>` |
|         - | 5525 | `typedef SOCKET ph7_socket;` |
|         - | 5526 | `typedef int ph7_socklen;` |
|         - | 5527 | `#define PH7_NET_INVALID_SOCKET INVALID_SOCKET` |
|         - | 5528 | `#else` |
|         - | 5529 | `typedef int ph7_socket;` |
|         - | 5530 | `typedef unsigned int ph7_socklen;` |
|         - | 5531 | `#define PH7_NET_INVALID_SOCKET (-1)` |
|         - | 5532 | `#endif` |
|         - | 5533 | `struct sockaddr; /* Forward declaration */` |
|         - | 5534 | `/* The one failure whose MESSAGE only the caller can word: php answers` |
|         - | 5535 | `` * `php_network_getaddresses: getaddrinfo for <host> failed: ...` with the host`` |
|         - | 5536 | ` * in it, reports no OS code beside it, and raises it TWICE — once from the` |
|         - | 5537 | ` * transport and once from the opener that asked. */` |
|         - | 5538 | `#define PH7_NET_ERR_RESOLVE (-3)` |
|         - | 5539 | `PH7_PRIVATE int PH7_NetInit(void);` |
|         - | 5540 | `PH7_PRIVATE int PH7_NetEnsureInit(void);` |
|         - | 5541 | `PH7_PRIVATE void PH7_NetCleanup(void);` |
|         - | 5542 | `PH7_PRIVATE ph7_socket PH7_NetListen(const char *zHost,int iPort,int iBacklog);` |
|         - | 5543 | `/*` |
|         - | 5544 | `` * The `socket` context options net.c can apply, php's own option names. A NULL`` |
|         - | 5545 | ` * pointer means "none of them", which is what every internal opener passes.` |
|         - | 5546 | ` * so_broadcast and ipv6_v6only describe a datagram socket and an address family` |
|         - | 5547 | ` * this build has not got (§7.4 slice-2 (a)), so they are stored on the context` |
|         - | 5548 | ` * and never reach a socket.` |
|         - | 5549 | ` */` |
|         - | 5550 | `typedef struct ph7_sockopts ph7_sockopts;` |
|         - | 5551 | `struct ph7_sockopts` |
|         - | 5552 | `{` |
|         - | 5553 | ``	const char *zBindHost; /* `bindto`'s host half, already parsed (0 = no bind) */`` |
|         - | 5554 | ``	int iBindPort;         /* `bindto`'s port half */`` |
|         - | 5555 | ``	int bReusePort;        /* `so_reuseport` */`` |
|         - | 5556 | ``	int bNoDelay;          /* `tcp_nodelay` */`` |
|         - | 5557 | ``	int bBroadcast;        /* `so_broadcast`: what a DATAGRAM socket needs before`` |
|         - | 5558 | `	                        * it may address 255.255.255.255 at all */` |
|         - | 5559 | ``	int bV6Only;           /* `ipv6_v6only`, applied to an AF_INET6 listener */`` |
|         - | 5560 | ``	int iBacklog;          /* `backlog`; <= 0 keeps the transport's default */`` |
|         - | 5561 | `	/* OUT: how the local bind failed on the socket that was USED, which php` |
|         - | 5562 | `	 * warns about in two different wordings and never treats as fatal. */` |
|         - | 5563 | `	int iBindErr;          /* PH7_SOCKOPT_BIND_* (0 = it worked, or none asked) */` |
|         - | 5564 | `	int iBindErrno;        /* the OS code behind PH7_SOCKOPT_BIND_REFUSED */` |
|         - | 5565 | `};` |
|         - | 5566 | `#define PH7_SOCKOPT_BIND_RESOLVE 1 /* not a numeric address (php never resolves one) */` |
|         - | 5567 | `#define PH7_SOCKOPT_BIND_REFUSED 2 /* bind() itself said no */` |
|         - | 5568 | `PH7_PRIVATE ph7_socket PH7_NetBind(const char *zHost,int iPort,int bDgram,int bListen,` |
|         - | 5569 | `	int iBacklog,const ph7_sockopts *pOpt,int *pErrno,const char **pzErr);` |
|         - | 5570 | `PH7_PRIVATE ph7_socket PH7_NetConnect(const char *zHost,int iPort,int iTimeoutMs,` |
|         - | 5571 | `	int bDgram,int bAsync,ph7_sockopts *pOpt,int *pErrno,const char **pzErr);` |
|         - | 5572 | `PH7_PRIVATE ph7_socket PH7_NetAccept(ph7_socket listenSock,struct sockaddr *pAddr,ph7_socklen *pAddrLen);` |
|         - | 5573 | `PH7_PRIVATE ph7_socket PH7_NetAcceptTimed(ph7_socket listenSock,int iTimeoutMs,int *pbTimedOut,` |
|         - | 5574 | `	char *zPeer,int nPeer);` |
|         - | 5575 | `PH7_PRIVATE int PH7_NetSockName(ph7_socket sock,int bPeer,char *zBuf,int nBuf);` |
|         - | 5576 | `PH7_PRIVATE int PH7_NetHostName(char *zBuf,int nBuf,int *pErrno);` |
|         - | 5577 | `PH7_PRIVATE int PH7_NetWait(ph7_socket sock,int bWrite,int iTimeoutMs);` |
|         - | 5578 | `/* The platform-numbered socket constants, asked for by id because their VALUES` |
|         - | 5579 | ` * differ per OS (AF_INET6 is 10, 23 and 30 on three of them). */` |
|         - | 5580 | `#define PH7_NETC_PF_INET        1` |
|         - | 5581 | `#define PH7_NETC_PF_INET6       2` |
|         - | 5582 | `#define PH7_NETC_PF_UNIX        3` |
|         - | 5583 | `#define PH7_NETC_SOCK_STREAM    4` |
|         - | 5584 | `#define PH7_NETC_SOCK_DGRAM     5` |
|         - | 5585 | `#define PH7_NETC_SOCK_RAW       6` |
|         - | 5586 | `#define PH7_NETC_SOCK_SEQPACKET 7` |
|         - | 5587 | `#define PH7_NETC_SOCK_RDM       8` |
|         - | 5588 | `#define PH7_NETC_IPPROTO_IP     9` |
|         - | 5589 | `#define PH7_NETC_IPPROTO_TCP   10` |
|         - | 5590 | `#define PH7_NETC_IPPROTO_UDP   11` |
|         - | 5591 | `#define PH7_NETC_IPPROTO_ICMP  12` |
|         - | 5592 | `#define PH7_NETC_IPPROTO_RAW   13` |
|         - | 5593 | `PH7_PRIVATE ph7_int64 PH7_NetSocketConst(int iWhich);` |
|         - | 5594 | `PH7_PRIVATE int PH7_NetShutdown(ph7_socket sock,int iHow);` |
|         - | 5595 | `PH7_PRIVATE int PH7_NetAtEnd(ph7_socket sock);` |
|         - | 5596 | `PH7_PRIVATE int PH7_NetIsAlive(ph7_socket sock);` |
|         - | 5597 | `PH7_PRIVATE int PH7_NetRecvFrom(ph7_socket sock,void *pBuf,int nLen,int iFlags,char *zAddr,int nAddr);` |
|         - | 5598 | `PH7_PRIVATE int PH7_NetSendTo(ph7_socket sock,const void *pBuf,int nLen,int iFlags,` |
|         - | 5599 | `	const char *zHost,int iPort,int *pErrno);` |
|         - | 5600 | `PH7_PRIVATE int PH7_NetSocketPair(int iDomain,int iType,int iProtocol,ph7_socket *aOut,int *pErrno);` |
|         - | 5601 | `PH7_PRIVATE int PH7_NetLastError(void);` |
|         - | 5602 | `PH7_PRIVATE int PH7_NetWouldBlock(void);` |
|         - | 5603 | `PH7_PRIVATE const char * PH7_NetStrError(int iErr);` |
|         - | 5604 | `PH7_PRIVATE int PH7_NetRecv(ph7_socket sock,void *pBuf,int nLen,int flags);` |
|         - | 5605 | `PH7_PRIVATE int PH7_NetSend(ph7_socket sock,const void *pBuf,int nLen,int flags);` |
|         - | 5606 | `PH7_PRIVATE int PH7_NetSendAll(ph7_socket sock,const void *pBuf,int nLen);` |
|         - | 5607 | `PH7_PRIVATE void PH7_NetClose(ph7_socket sock);` |
|         - | 5608 | `PH7_PRIVATE void PH7_NetSetTimeout(ph7_socket sock,int iMilliseconds);` |
|         - | 5609 | `PH7_PRIVATE void PH7_NetSetRwTimeout(ph7_socket sock,ph7_int64 iSeconds,ph7_int64 iMicroseconds);` |
|         - | 5610 | `PH7_PRIVATE void PH7_NetSetBlocking(ph7_socket sock,int bBlocking);` |
|         - | 5611 | `PH7_PRIVATE void PH7_NetAddrToString(const struct sockaddr *pAddr,char *zBuf,int nBufLen);` |
|         - | 5612 | `PH7_PRIVATE int PH7_NetAddrPort(const struct sockaddr *pAddr);` |
|         - | 5613 | `/* ext/sockets (builtin_sockets.c): php's BSD socket API, which is the other` |
|         - | 5614 | ` * face of the descriptors net.c drives for the stream wrappers. */` |
|         - | 5615 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 5616 | `PH7_PRIVATE const ph7_builtin_func * PH7_SocketsFuncTable(sxu32 *pnEntry);` |
|         - | 5617 | `PH7_PRIVATE sxi32 PH7_VmInstallSockets(ph7_vm *pVm);` |
|         - | 5618 | `PH7_PRIVATE void PH7_RegisterSocketsConstants(ph7_vm *pVm);` |
|         - | 5619 | `PH7_PRIVATE void PH7_SocketsVmReset(ph7_vm *pVm);` |
|         - | 5620 | `PH7_PRIVATE void PH7_SocketsVmRelease(ph7_vm *pVm);` |
|         - | 5621 | `/* socket_strerror()'s table: the platform's own, plus php's -10000 host-lookup` |
|         - | 5622 | ` * range, which no errno occupies. */` |
|         - | 5623 | `PH7_PRIVATE const char * PH7_SocketStrError(int iErr);` |
|         - | 5624 | `#endif` |
|         - | 5625 | `/* The three doors ext/sockets uses onto the stream device stack (vfs_stream.c). */` |
|         - | 5626 | `PH7_PRIVATE int PH7_StreamSocketHandle(io_private *pDev,ph7_socket *pOut);` |
|         - | 5627 | `PH7_PRIVATE io_private * PH7_StreamWrapSocket(ph7_context *pCtx,ph7_socket sock,int bDgram,` |
|         - | 5628 | `	const char *zLabel,const char *zUri);` |
|         - | 5629 | `PH7_PRIVATE void PH7_StreamCloseExported(io_private *pDev);` |
|         - | 5630 | `#endif /* PH7_ENABLE_NET */` |
|         - | 5631 | `/* vm_json.c function prototypes */` |
|         - | 5632 | `PH7_PRIVATE int vm_builtin_json_encode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5633 | `PH7_PRIVATE int vm_builtin_json_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5634 | `PH7_PRIVATE int vm_builtin_json_last_error_msg(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5635 | `PH7_PRIVATE int vm_builtin_json_decode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5636 | `PH7_PRIVATE int vm_builtin_json_validate(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5637 | `/* vm_serialize.c function prototypes */` |
|         - | 5638 | `PH7_PRIVATE int vm_builtin_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5639 | `PH7_PRIVATE int vm_builtin_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5640 | `PH7_PRIVATE sxi32 PH7_VmUnserializeOne(ph7_context *pCtx,const char *zIn,int nByte,int *pnRead,ph7_value *pOut);` |
|         - | 5641 | `PH7_PRIVATE void PH7_AppendShortestReal(SyBlob *pOut,double d);` |
|         - | 5642 | `/* memobj.c float-shape helper (php_gcvt/smart_str_append_double semantics);` |
|         - | 5643 | ` * shared by the float->string cast and builtin.c's printf float conversions */` |
|         - | 5644 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         - | 5645 | `PH7_PRIVATE sxi32 PH7_PhpFloatShape(char *zBuf,sxi32 nLen,int bGeneric);` |
|         - | 5646 | `#endif` |
|         - | 5647 | `/* builtin.c utf8 function prototypes */` |
|         - | 5648 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult,int bReturnPropagates);` |
|         - | 5649 | `PH7_PRIVATE int VmLocalExecThrew(ph7_vm *pVm,sxi32 rc,const void *pResumeBefore,const void *pInlineBefore);` |
|         - | 5650 | `PH7_PRIVATE int PH7_VmIsAutoGlobal(const char *zName,sxu32 nByte);` |
|         - | 5651 | `/* The engine's two NON-refusing offset rules, shared with the native ArrayAccess` |
|         - | 5652 | ` * classes: a RESOURCE key warns and is rewritten in place to its integer id, and a` |
|         - | 5653 | ` * NULL key deprecates and then folds to the "" key (the caller falls through). */` |
|         - | 5654 | `PH7_PRIVATE void PH7_VmOffsetResourceWarn(ph7_vm *pVm,ph7_value *pKey);` |
|         - | 5655 | `PH7_PRIVATE int PH7_VmNullOffsetDeprecate(ph7_vm *pVm,ph7_value *pKey);` |
|         - | 5656 | `/* Wording modes for PH7_VmArrayKeyArg(): the RULES are the engine's subscript` |
|         - | 5657 | ` * rules in all three, only the two sentences differ. */` |
|         - | 5658 | ``#define PH7_ARRAYKEY_OFFSET 0 /* the engine's own offset wording, `$a[$k]`'s */`` |
|         - | 5659 | `#define PH7_ARRAYKEY_AKE    1 /* array_key_exists(): engine type wording, its own null clause */` |
|         - | 5660 | `#define PH7_ARRAYKEY_ZPP    2 /* key_exists(): php's ZPP type wording, the same null clause */` |
|         - | 5661 | `PH7_PRIVATE sxi32 PH7_VmArrayKeyArg(ph7_context *pCtx,ph7_value *pKey,int iWording);` |
|         - | 5662 | `PH7_PRIVATE sxi32 PH7_VmExecAttrArg(ph7_vm *pVm,SySet *pByteCode,ph7_class *pDeclCls,ph7_value *pResult);` |
|         - | 5663 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...);` |
|         - | 5664 | `/* vm_builtin_class.c function prototypes */` |
|         - | 5665 | `PH7_PRIVATE int PH7_VmInstanceOf(ph7_class *pThis,ph7_class *pClass);` |
|         - | 5666 | `PH7_PRIVATE int PH7_SplOuterForward(ph7_vm *pVm,ph7_class_instance *pThis,SyString *pName,ph7_class_instance **ppInner,ph7_class_method **ppMeth);` |
|         - | 5667 | `PH7_PRIVATE void PH7_VmStampThrowableSite(ph7_vm *pVm,ph7_class_instance *pThis);` |
|         - | 5668 | `PH7_PRIVATE int PH7_VmClassMemberAccess(ph7_vm *pVm,ph7_class *pClass,const SyString *pAttrName,sxi32 iProtection,int bLog);` |
|         - | 5669 | `PH7_PRIVATE int PH7_VmClassAttrAccess(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,int bLog);` |
|         - | 5670 | `PH7_PRIVATE ph7_class * PH7_VmCallerScope(ph7_vm *pVm);` |
|         - | 5671 | `PH7_PRIVATE ph7_class * PH7_VmMethodScopeName(ph7_vm *pVm,ph7_class *pClass,ph7_class_method *pMeth);` |
|         - | 5672 | `PH7_PRIVATE int PH7_VmFccMethodIsDirect(ph7_vm *pVm,ph7_class *pClass,const char *zName,sxu32 nName);` |
|         - | 5673 | `PH7_PRIVATE ph7_class * PH7_VmClosureScopeClass(ph7_vm *pVm,ph7_class_instance *pClosure);` |
|         - | 5674 | `PH7_PRIVATE ph7_class * PH7_VmComposingClass(ph7_class *pClass,ph7_class *pDecl);` |
|         - | 5675 | `PH7_PRIVATE void PH7_ClassMethodRegisteredName(ph7_class *pClass,const char *zName,sxu32 nByte,SyString *pOut);` |
|         - | 5676 | `PH7_PRIVATE ph7_class * PH7_VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg);` |
|         - | 5677 | `PH7_PRIVATE int vm_builtin_get_class(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5678 | `PH7_PRIVATE int vm_builtin_get_parent_class(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5679 | `PH7_PRIVATE int vm_builtin_get_called_class(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5680 | `PH7_PRIVATE int vm_builtin_property_exists(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5681 | `PH7_PRIVATE int vm_builtin_method_exists(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5682 | `PH7_PRIVATE int vm_builtin_class_exists(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5683 | `PH7_PRIVATE int vm_builtin_interface_exists(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5684 | `PH7_PRIVATE int vm_builtin_trait_exists(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5685 | `PH7_PRIVATE int vm_builtin_class_alias(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5686 | `PH7_PRIVATE int vm_builtin_get_declared_classes(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5687 | `PH7_PRIVATE int vm_builtin_get_declared_interfaces(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5688 | `PH7_PRIVATE int vm_builtin_get_declared_traits(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5689 | `PH7_PRIVATE int vm_builtin_get_class_methods(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5690 | `PH7_PRIVATE int vm_builtin_class_parents(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5691 | `PH7_PRIVATE int vm_builtin_class_implements(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5692 | `PH7_PRIVATE int vm_builtin_class_uses(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5693 | `PH7_PRIVATE int vm_builtin_get_class_vars(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5694 | `PH7_PRIVATE int vm_builtin_get_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5695 | `PH7_PRIVATE int vm_builtin_get_mangled_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5696 | `PH7_PRIVATE int vm_builtin_is_a(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5697 | `PH7_PRIVATE int vm_builtin_clone(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5698 | `PH7_PRIVATE int vm_builtin_spl_object_id(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5699 | `PH7_PRIVATE int vm_builtin_spl_object_hash(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5700 | `PH7_PRIVATE int vm_builtin_is_subclass_of(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5701 | `PH7_PRIVATE int vm_builtin_call_user_func(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5702 | `PH7_PRIVATE int vm_builtin_call_user_func_array(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5703 | `/* vm_builtin_ob.c function prototypes */` |
|         - | 5704 | `PH7_PRIVATE int VmObConsumer(const void *pData,unsigned int nDataLen,void *pUserData);` |
|         - | 5705 | `PH7_PRIVATE void PH7_VmObFlushAll(ph7_vm *pVm);` |
|         - | 5706 | `PH7_PRIVATE int vm_builtin_ob_clean(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5707 | `PH7_PRIVATE int vm_builtin_ob_end_clean(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5708 | `PH7_PRIVATE int vm_builtin_ob_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5709 | `PH7_PRIVATE int vm_builtin_ob_get_clean(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5710 | `PH7_PRIVATE int vm_builtin_flush(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5711 | `PH7_PRIVATE int vm_builtin_ob_get_flush(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5712 | `PH7_PRIVATE int vm_builtin_ob_get_status(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5713 | `PH7_PRIVATE int vm_builtin_ob_get_length(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5714 | `PH7_PRIVATE int vm_builtin_ob_get_level(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5715 | `PH7_PRIVATE int vm_builtin_ob_start(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5716 | `PH7_PRIVATE int vm_builtin_ob_flush(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5717 | `PH7_PRIVATE int vm_builtin_ob_end_flush(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5718 | `PH7_PRIVATE int vm_builtin_ob_implicit_flush(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5719 | `PH7_PRIVATE int vm_builtin_ob_list_handlers(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5720 | `/* vm_builtin_getopt.c function prototypes */` |
|         - | 5721 | `PH7_PRIVATE int vm_builtin_getopt(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5722 | `/* vm_random.c function prototypes */` |
|         - | 5723 | `/* php's ext/random object surface: the Random\Engine contract, its errors,` |
|         - | 5724 | ` * the seeded engines and the Randomizer that consumes them. */` |
|         - | 5725 | `PH7_PRIVATE sxi32 PH7_VmInstallRandom(ph7_vm *pVm);` |
|         - | 5726 | `/* builtin_math.c function prototypes */` |
|         - | 5727 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|         - | 5728 | `PH7_PRIVATE int PH7_builtin_sqrt(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5729 | `PH7_PRIVATE int PH7_builtin_acosh(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5730 | `PH7_PRIVATE int PH7_builtin_asinh(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5731 | `PH7_PRIVATE int PH7_builtin_atanh(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5732 | `PH7_PRIVATE int PH7_builtin_expm1(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5733 | `PH7_PRIVATE int PH7_builtin_log1p(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5734 | `PH7_PRIVATE int PH7_builtin_deg2rad(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5735 | `PH7_PRIVATE int PH7_builtin_rad2deg(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5736 | `PH7_PRIVATE int PH7_builtin_fpow(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5737 | `PH7_PRIVATE int PH7_builtin_exp(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5738 | `PH7_PRIVATE int PH7_builtin_floor(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5739 | `PH7_PRIVATE int PH7_builtin_cos(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5740 | `PH7_PRIVATE int PH7_builtin_acos(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5741 | `PH7_PRIVATE int PH7_builtin_cosh(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5742 | `PH7_PRIVATE int PH7_builtin_sin(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5743 | `PH7_PRIVATE int PH7_builtin_asin(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5744 | `PH7_PRIVATE int PH7_builtin_sinh(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5745 | `PH7_PRIVATE int PH7_builtin_ceil(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5746 | `PH7_PRIVATE int PH7_builtin_tan(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5747 | `PH7_PRIVATE int PH7_builtin_atan(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5748 | `PH7_PRIVATE int PH7_builtin_tanh(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5749 | `PH7_PRIVATE int PH7_builtin_atan2(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5750 | `PH7_PRIVATE int PH7_builtin_abs(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5751 | `PH7_PRIVATE int PH7_builtin_log(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5752 | `PH7_PRIVATE int PH7_builtin_log10(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5753 | `PH7_PRIVATE int PH7_builtin_pow(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5754 | `PH7_PRIVATE int PH7_builtin_pi(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5755 | `PH7_PRIVATE int PH7_builtin_fmod(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5756 | `PH7_PRIVATE int PH7_builtin_hypot(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5757 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|         - | 5758 | `PH7_PRIVATE int PH7_builtin_round(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5759 | `/*` |
|         - | 5760 | ` * PHP's rounding modes (mirror ext/standard/php_math_round_mode.h). Only the` |
|         - | 5761 | ` * four HALF_* integers are exposed to userland (PHP_ROUND_HALF_UP..HALF_ODD,` |
|         - | 5762 | ` * see constant.c); the CEILING/FLOOR/TOWARD_ZERO/AWAY_FROM_ZERO modes (5..8)` |
|         - | 5763 | ` * have no userland constant but are reachable by passing the raw integer to` |
|         - | 5764 | ` * round()'s 3rd argument, which PHP 8.5 still accepts, so all eight are` |
|         - | 5765 | `` * honored. `enum RoundingMode` names all eight and numbers them DIFFERENTLY --`` |
|         - | 5766 | ` * PH7_RoundingModeCase() is the translation.` |
|         - | 5767 | ` */` |
|         - | 5768 | `#define PH7_ROUND_HALF_UP        1` |
|         - | 5769 | `#define PH7_ROUND_HALF_DOWN      2` |
|         - | 5770 | `#define PH7_ROUND_HALF_EVEN      3` |
|         - | 5771 | `#define PH7_ROUND_HALF_ODD       4` |
|         - | 5772 | `#define PH7_ROUND_CEILING        5` |
|         - | 5773 | `#define PH7_ROUND_FLOOR          6` |
|         - | 5774 | `#define PH7_ROUND_TOWARD_ZERO    7` |
|         - | 5775 | `#define PH7_ROUND_AWAY_FROM_ZERO 8` |
|         - | 5776 | ``/* php 8.4's `enum RoundingMode`, declared beside round() -- round() and`` |
|         - | 5777 | ` * bcround() are its two consumers. */` |
|         - | 5778 | `PH7_PRIVATE sxi32 PH7_VmInstallRoundingMode(ph7_vm *pVm);` |
|         - | 5779 | `PH7_PRIVATE int PH7_RoundingModeCase(ph7_value *pVal,int *pMode);` |
|         - | 5780 | `PH7_PRIVATE int PH7_builtin_intdiv(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5781 | `PH7_PRIVATE int PH7_builtin_number_format(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5782 | `PH7_PRIVATE int PH7_builtin_dechex(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5783 | `PH7_PRIVATE int PH7_builtin_decoct(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5784 | `PH7_PRIVATE int PH7_builtin_decbin(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5785 | `PH7_PRIVATE int PH7_builtin_hexdec(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5786 | `PH7_PRIVATE int PH7_builtin_bindec(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5787 | `PH7_PRIVATE int PH7_builtin_octdec(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5788 | `PH7_PRIVATE int PH7_builtin_srand(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5789 | `PH7_PRIVATE int PH7_builtin_base_convert(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5790 | `/* builtin_date.c function prototypes */` |
|         - | 5791 | `PH7_PRIVATE int PH7_builtin_time(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5792 | `PH7_PRIVATE int PH7_builtin_microtime(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5793 | `PH7_PRIVATE int PH7_builtin_hrtime(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5794 | `PH7_PRIVATE int PH7_builtin_getrusage(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5795 | `#ifdef PH7_ENABLE_PCRE` |
|         - | 5796 | `PH7_PRIVATE void PH7_PcreVersionInfo(char *zBuf,int nBuf,int *pMajor,int *pMinor,int *pJit);` |
|         - | 5797 | `#endif` |
|         - | 5798 | `PH7_PRIVATE int PH7_builtin_getdate(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5799 | `PH7_PRIVATE int PH7_builtin_gettimeofday(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5800 | `PH7_PRIVATE int PH7_builtin_date(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5801 | `PH7_PRIVATE int PH7_builtin_gmdate(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5802 | `PH7_PRIVATE int PH7_builtin_localtime(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5803 | `PH7_PRIVATE int PH7_builtin_idate(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5804 | `PH7_PRIVATE int PH7_builtin_mktime(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5805 | `PH7_PRIVATE int PH7_builtin_date_default_timezone_get(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5806 | `PH7_PRIVATE int PH7_builtin_date_default_timezone_set(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5807 | `/* builtin_mb.c (UTF-8-only mb_* family) */` |
|         - | 5808 | ``/* Shared ZPP helper: resolve an `int`-typed parameter with php's full contract`` |
|         - | 5809 | ` * (null deprecation, lossy float / float-string deprecations, TypeErrors for` |
|         - | 5810 | ` * NAN/INF/non-numeric). Builtins with int params should use it instead of a` |
|         - | 5811 | ` * bare ph7_value_to_int64(), which coerces silently. */` |
|         - | 5812 | `PH7_PRIVATE sxi32 PH7_IntArgResolve(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,` |
|         - | 5813 | `	int iArgNum,const char *zParamName,const char *zTypeStr,sxi64 *pOut);` |
|         - | 5814 | `PH7_PRIVATE int PH7_builtin_mb_strlen_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5815 | `PH7_PRIVATE int PH7_builtin_mb_substr_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5816 | `PH7_PRIVATE int PH7_builtin_mb_case_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5817 | `PH7_PRIVATE int PH7_builtin_mb_convert_case_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5818 | `PH7_PRIVATE int PH7_builtin_mb_strpos_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5819 | `PH7_PRIVATE int PH7_builtin_mb_str_split_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5820 | `PH7_PRIVATE int PH7_builtin_mb_trim_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5821 | `PH7_PRIVATE int PH7_builtin_mb_internal_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5822 | `PH7_PRIVATE int PH7_builtin_mb_substitute_character_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5823 | `PH7_PRIVATE int PH7_builtin_mb_scrub_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5824 | `PH7_PRIVATE int PH7_builtin_mb_check_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5825 | `PH7_PRIVATE int PH7_builtin_mb_strwidth_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5826 | `PH7_PRIVATE int PH7_builtin_mb_chr_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5827 | `PH7_PRIVATE int PH7_builtin_mb_ord_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5828 | `PH7_PRIVATE int PH7_builtin_mb_ucfirst_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5829 | `PH7_PRIVATE int PH7_builtin_mb_strstr_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5830 | `PH7_PRIVATE int PH7_builtin_mb_substr_count_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5831 | `PH7_PRIVATE int PH7_builtin_mb_str_pad_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5832 | `PH7_PRIVATE int PH7_builtin_mb_strcut_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5833 | `PH7_PRIVATE int PH7_builtin_mb_strimwidth_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5834 | `PH7_PRIVATE int PH7_builtin_mb_detect_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5835 | `PH7_PRIVATE int PH7_builtin_mb_detect_order_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5836 | `PH7_PRIVATE int PH7_builtin_mb_list_encodings_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5837 | `PH7_PRIVATE int PH7_builtin_mb_convert_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5838 | `/* iconv (builtin_iconv.c) */` |
|         - | 5839 | `PH7_PRIVATE int PH7_builtin_iconv_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5840 | `PH7_PRIVATE int PH7_IconvTranslate(SyBlob *pOut,const char *zIn,int nIn,` |
|         - | 5841 | `	const char *zFrom,int nFrom,const char *zTo,int nTo);` |
|         - | 5842 | `PH7_PRIVATE int PH7_builtin_iconv_strlen_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5843 | `PH7_PRIVATE int PH7_builtin_iconv_substr_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5844 | `PH7_PRIVATE int PH7_builtin_iconv_strpos_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5845 | `PH7_PRIVATE int PH7_builtin_iconv_strrpos_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5846 | `PH7_PRIVATE int PH7_builtin_iconv_get_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5847 | `PH7_PRIVATE int PH7_builtin_iconv_mime_encode_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5848 | `PH7_PRIVATE int PH7_builtin_iconv_mime_decode_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5849 | `PH7_PRIVATE int PH7_builtin_iconv_mime_decode_headers_f(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 5850 | `/* vm_builtin_spl.c */` |
|         - | 5851 | `PH7_PRIVATE sxi32 PH7_VmInstallSpl(ph7_vm *pVm);` |
|         - | 5852 | `/* vm_builtin_tokenizer.c */` |
|         - | 5853 | `PH7_PRIVATE sxi32 PH7_VmInstallTokenizer(ph7_vm *pVm);` |
|         - | 5854 | `PH7_PRIVATE void PH7_RegisterTokenizerConstants(ph7_vm *pVm);` |
|         - | 5855 | `/* vm_builtin_session.c */` |
|         - | 5856 | `PH7_PRIVATE sxi32 PH7_VmInstallSession(ph7_vm *pVm);` |
|         - | 5857 | `PH7_PRIVATE void PH7_VmSessionShutdown(ph7_vm *pVm);` |
|         - | 5858 | `/* vm_builtin_ini.c */` |
|         - | 5859 | `PH7_PRIVATE sxi32 PH7_VmInstallIni(ph7_vm *pVm);` |
|         - | 5860 | `PH7_PRIVATE sxi64 PH7_VmIniGetInt(ph7_vm *pVm,const char *zName,sxi64 iDefault);` |
|         - | 5861 | `PH7_PRIVATE int PH7_VmApplyMemoryLimit(ph7_vm *pVm,const char *zVal,sxu32 nVal);` |
|         - | 5862 | `PH7_PRIVATE void PH7_VmIniGetStr(ph7_vm *pVm,const char *zName,SyBlob *pOut);` |
|         - | 5863 | `PH7_PRIVATE int PH7_VmIniIsUnset(ph7_vm *pVm,const char *zName);` |
|         - | 5864 | `PH7_PRIVATE int PH7_VmIniDescribe(ph7_vm *pVm,const char *zName,sxu32 nName,` |
|         - | 5865 | `	sxi32 *piAccess,SyBlob *pOut,SyBlob *pDef);` |
|         - | 5866 | `PH7_PRIVATE int PH7_VmIniGetBool(ph7_vm *pVm,const char *zName,int bDefault);` |
|         - | 5867 | `PH7_PRIVATE int PH7_VmIniSet(ph7_vm *pVm,const char *zName,sxu32 nName,const char *zVal,sxu32 nVal,const char *zWho);` |
|         - | 5868 | `/* vfs_win.c / vfs_unix.c exported structs */` |
|         - | 5869 | `#ifdef __WINNT__` |
|         - | 5870 | `extern const ph7_vfs sWinVfs;` |
|         - | 5871 | `extern const ph7_io_stream sWinFileStream;` |
|         - | 5872 | `/* Would php's MapViewOfFile() copy of this plain file map a view of zero` |
|         - | 5873 | ` * requested bytes? stream_copy_to_stream() answers false then. */` |
|         - | 5874 | `PH7_PRIVATE int PH7_WinFileMapsEmptyView(void *pHandle,ph7_int64 nAhead);` |
|         - | 5875 | `/* The Win32 code (0: none) and php's text for the last failed opendir(). */` |
|         - | 5876 | `PH7_PRIVATE unsigned long PH7_WinOpenDirReason(char *zBuf,int nBuf);` |
|         - | 5877 | `#elif defined(__UNIXES__)` |
|         - | 5878 | `extern const ph7_vfs sUnixVfs;` |
|         - | 5879 | `extern const ph7_io_stream sUnixFileStream;` |
|         - | 5880 | `#endif` |
|         - | 5881 | `/* Built-in IO stream drivers: tcp:// lives in vfs_stream.c; php://, data://` |
|         - | 5882 | ` * and the pipe (popen) stream live in vfs_io_driver.c. Registration (vfs.c)` |
|         - | 5883 | ` * and the standard-stream exporters reference them across those files. */` |
|         - | 5884 | `extern const ph7_io_stream sTCP_Stream;` |
|         - | 5885 | `/* vfs_http.c -- what a script reads BACK from an http:// exchange. The store is` |
|         - | 5886 | ` * in every build: the two php 8.4 getters over it are ordinary builtins, and a` |
|         - | 5887 | ` * build with no network simply never records anything into it. */` |
|         - | 5888 | `PH7_PRIVATE void PH7_HttpPublishHeaders(ph7_vm *pVm,SyBlob *pLines);` |
|         - | 5889 | `PH7_PRIVATE ph7_value * PH7_HttpHeaderArray(ph7_vm *pVm,SyBlob *pLines);` |
|         - | 5890 | `PH7_PRIVATE void PH7_HttpFlushResponseHeaders(ph7_vm *pVm);` |
|         - | 5891 | `PH7_PRIVATE void PH7_HttpClearResponseHeaders(ph7_vm *pVm);` |
|         - | 5892 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 5893 | `PH7_PRIVATE void PH7_HttpInstallFuncs(ph7_vm *pVm);` |
|         - | 5894 | `#endif` |
|         - | 5895 | `#if !defined(PH7_DISABLE_DISK_IO) && defined(PH7_ENABLE_NET)` |
|         - | 5896 | `/* ... and the wrapper itself, which is where the store is filled. */` |
|         - | 5897 | `extern const ph7_io_stream sHTTP_Stream;` |
|         - | 5898 | `PH7_PRIVATE int PH7_HttpStreamIs(const ph7_io_stream *pStream);` |
|         - | 5899 | `PH7_PRIVATE ph7_value * PH7_HttpStreamHeaderArray(ph7_vm *pVm,void *pHandle);` |
|         - | 5900 | `PH7_PRIVATE ph7_socket * PH7_HttpStreamSocket(void *pHandle);` |
|         - | 5901 | `PH7_PRIVATE sxu32 PH7_HttpStreamUnread(void *pHandle);` |
|         - | 5902 | `PH7_PRIVATE int PH7_HttpStreamAtEof(void *pHandle);` |
|         - | 5903 | `#endif /* !PH7_DISABLE_DISK_IO && PH7_ENABLE_NET */` |
|         - | 5904 | `extern const ph7_io_stream sDATA_Stream;` |
|         - | 5905 | `extern const ph7_io_stream sPHP_Stream;` |
|         - | 5906 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 5907 | `/* glob:// (vfs.c): a directory whose entries are a pattern's matches. It has no` |
|         - | 5908 | ` * xOpen at all, which is php's wrapper too. The three accessors are what SPL` |
|         - | 5909 | ` * asks of a directory handle that turns out to be this one: php's` |
|         - | 5910 | ` * php_stream_is(), php_glob_stream_get_path() and php_glob_stream_get_count(). */` |
|         - | 5911 | `extern const ph7_io_stream sGLOB_Stream;` |
|         - | 5912 | `PH7_PRIVATE int PH7_GlobStreamIs(const ph7_io_stream *pStream);` |
|         - | 5913 | `PH7_PRIVATE const char * PH7_GlobStreamPath(void *pHandle,int *pnLen);` |
|         - | 5914 | `PH7_PRIVATE sxi64 PH7_GlobStreamCount(void *pHandle);` |
|         - | 5915 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 5916 | `/* IO private state carried by every open stream handle (fopen/opendir/popen` |
|         - | 5917 | ` * resources and the exported std streams). Shared between vfs.c,` |
|         - | 5918 | ` * vfs_stream.c and vfs_io_driver.c. */` |
|         - | 5919 | `struct io_private` |
|         - | 5920 | `{` |
|         - | 5921 | `	const ph7_io_stream *pStream; /* Underlying IO device */` |
|         - | 5922 | `	void *pHandle; /* IO handle */` |
|         - | 5923 | `	/* Unbuffered IO */` |
|         - | 5924 | `	SyBlob sBuffer; /* Working buffer */` |
|         - | 5925 | `	sxu32 nOfft;    /* Current read offset */` |
|         - | 5926 | `	/* What the opener was ASKED for. php reports both back from` |
|         - | 5927 | ``	 * stream_get_meta_data() and neither was retained here, so the `uri` and`` |
|         - | 5928 | ``	 * `mode` keys of that array simply did not exist. sUri stays EMPTY for a`` |
|         - | 5929 | `	 * stream php opens without a wrapper (a popen() pipe), which is exactly` |
|         - | 5930 | `	 * when php omits the key. */` |
|         - | 5931 | `	SyBlob sUri;     /* the path/URI as the opener received it */` |
|         - | 5932 | `	char zMode[16];  /* the mode string, php's own field width */` |
|         - | 5933 | `	/* Per-handle settings the stream_set_* family writes. */` |
|         - | 5934 | `	sxu32 nChunk;    /* stream_set_chunk_size(), php's 8192 by default */` |
|         - | 5935 | `	sxu8 bNonBlock;  /* stream_set_blocking(false) took effect at the descriptor */` |
|         - | 5936 | `	sxu8 bHasTimeout;/* stream_set_timeout() armed one, so an EAGAIN read EXPIRED */` |
|         - | 5937 | ``	sxu8 bTimedOut;  /* the last read expired; php's meta `timed_out`, cleared by the next */`` |
|         - | 5938 | `	sxu8 bEof;       /* a read on this handle has already come back empty */` |
|         - | 5939 | `	int iLastReadErr;/* errno of the last device read that FAILED, latched for the` |
|         - | 5940 | `	                  * reader to announce (php's "Read of N bytes failed with` |
|         - | 5941 | `	                  * errno=..." notice) and cleared once it has. 0 = nothing` |
|         - | 5942 | `	                  * to report; a read that merely found EOF never sets it. */` |
|         - | 5943 | `	sxu8 bDir;       /* opendir()/dir() handle rather than a byte stream */` |
|         - | 5944 | `	/* Where the SCRIPT is on a device that cannot say it itself -- a popen()` |
|         - | 5945 | `	 * pipe, a socket, a directory handle. php's stream layer tracks a position` |
|         - | 5946 | `	 * for EVERY stream and only asks the device when it seeks, so ftell() on a` |
|         - | 5947 | `	 * pipe answers the bytes that have gone past rather than failing; this is` |
|         - | 5948 | `	 * that counter, and it is read only when the device has no xTell. A` |
|         - | 5949 | `	 * directory handle steps it by one php_stream_dirent per entry read` |
|         - | 5950 | `	 * (PHL_DIR_RECORD), which is the number php's own ftell() reports. */` |
|         - | 5951 | `	ph7_int64 iPos;` |
|         - | 5952 | `	sxu8 bPersist;   /* opened PERSISTENTLY: get_resource_type() names it apart */` |
|         - | 5953 | `	/* The stream CONTEXT this handle carries (phl_stream_ctx*), owned by the VM` |
|         - | 5954 | `	 * registry. php attaches the opener's context to a TRANSPORT stream and to` |
|         - | 5955 | `	 * nothing else, and creates one on demand for a` |
|         - | 5956 | `	 * stream_context_set_option($stream,…). */` |
|         - | 5957 | `	void *pCtxRes;` |
|         - | 5958 | `	/* The two FILTER chains this handle carries (phl_stream_filter*, head first).` |
|         - | 5959 | `	 * php runs the read chain on what came off the device before the script sees` |
|         - | 5960 | `	 * it and the write chain on what the script wrote before the device does, so` |
|         - | 5961 | `	 * a filtered read cannot be served straight into the caller's buffer: a` |
|         - | 5962 | `	 * filter changes the byte COUNT. sFilt is where the chain's output waits. */` |
|         - | 5963 | `	void *pReadFilters;   /* phl_stream_filter* — read chain head, or 0 */` |
|         - | 5964 | `	void *pWriteFilters;  /* phl_stream_filter* — write chain head, or 0 */` |
|         - | 5965 | `	SyBlob sFilt;         /* filtered bytes not yet handed to a reader */` |
|         - | 5966 | `	sxu32 nFiltOfft;      /* read offset inside sFilt */` |
|         - | 5967 | `	sxu8 bFiltDone;       /* the read chain already had its CLOSING call */` |
|         - | 5968 | `	sxu8 bFiltErr;        /* a filter REFUSED: the next read answers false, once */` |
|         - | 5969 | `	ph7_int64 iFiltPos;   /* bytes the read CHAIN has delivered: php's position` |
|         - | 5970 | `	                       * for a filtered stream counts what came OUT, which` |
|         - | 5971 | `	                       * has nothing to do with the device's own offset */` |
|         - | 5972 | `	sxu32 iMagic;   /* Sanity check to avoid misuse */` |
|         - | 5973 | `};` |
|         - | 5974 | `#define IO_PRIVATE_MAGIC 0xFEAC14` |
|         - | 5975 | `/* proc_open()'s handle is an io_private with this magic in the same field, which` |
|         - | 5976 | `` * is what lets one probe tell the two apart — and what php names `process`. */`` |
|         - | 5977 | `#define PROC_PRIVATE_MAGIC 0x9C0DE5` |
|         - | 5978 | `/* A user-facing handle (fopen/opendir/popen) that has been fclose()'d/closedir()'d/` |
|         - | 5979 | ` * pclose()'d keeps its io_private alive but stamped with this magic, so every` |
|         - | 5980 | ` * ph7_value that still references it observes a closed resource` |
|         - | 5981 | ` * (gettype()=='resource (closed)', is_resource()==false), matching php. */` |
|         - | 5982 | `#define IO_PRIVATE_CLOSED_MAGIC 0xC105ED` |
|         - | 5983 | `/* stream_context_create()'s handle carries this magic in the same field, for the` |
|         - | 5984 | ` * same reason proc_open()'s does: php makes a context a RESOURCE, and the only` |
|         - | 5985 | ` * thing a resource probe can look at here is that word. */` |
|         - | 5986 | `#define STREAM_CTX_MAGIC 0xC07E47` |
|         - | 5987 | `/* Make sure we are dealing with a valid io_private instance */` |
|         - | 5988 | `#define IO_PRIVATE_INVALID(IO) ( IO == 0 \|\| IO->iMagic != IO_PRIVATE_MAGIC )` |
|         - | 5989 | `/*` |
|         - | 5990 | ` * One php stream CONTEXT: the wrapper => option => value map a script hands an` |
|         - | 5991 | ``  * opener, plus the `notification` parameter. php's is a `stream-context` `` |
|         - | 5992 | ` * resource, and a PHL resource is a bare void*, so the struct opens with an` |
|         - | 5993 | ` * io_private-compatible header (proc_open()'s handle does the same) — every` |
|         - | 5994 | ` * resource probe reads that magic and stays in bounds. The VM owns the chain` |
|         - | 5995 | ` * and drops it at reset, so a reused VM does not carry one request's default` |
|         - | 5996 | ` * context into the next.` |
|         - | 5997 | ` */` |
|         - | 5998 | `/*` |
|         - | 5999 | `` * php's STREAM_NOTIFY_* -- WHICH event a context's `notification` callback is`` |
|         - | 6000 | ` * being told about -- and the three severities beside them. RESOLVE, the two` |
|         - | 6001 | ` * AUTH codes and AUTH_RESULT are php's own numbers for events no wrapper here` |
|         - | 6002 | ` * raises; they are defined because a script SWITCHES on them, and a name that` |
|         - | 6003 | ` * is missing is a fatal where an unreachable one is simply never matched.` |
|         - | 6004 | ` */` |
|         - | 6005 | `#define PHL_STREAM_NOTIFY_RESOLVE       1` |
|         - | 6006 | `#define PHL_STREAM_NOTIFY_CONNECT       2` |
|         - | 6007 | `#define PHL_STREAM_NOTIFY_AUTH_REQUIRED 3` |
|         - | 6008 | `#define PHL_STREAM_NOTIFY_MIME_TYPE_IS  4` |
|         - | 6009 | `#define PHL_STREAM_NOTIFY_FILE_SIZE_IS  5` |
|         - | 6010 | `#define PHL_STREAM_NOTIFY_REDIRECTED    6` |
|         - | 6011 | `#define PHL_STREAM_NOTIFY_PROGRESS      7` |
|         - | 6012 | `#define PHL_STREAM_NOTIFY_COMPLETED     8` |
|         - | 6013 | `#define PHL_STREAM_NOTIFY_FAILURE       9` |
|         - | 6014 | `#define PHL_STREAM_NOTIFY_AUTH_RESULT   10` |
|         - | 6015 | `#define PHL_STREAM_NOTIFY_SEVERITY_INFO 0` |
|         - | 6016 | `#define PHL_STREAM_NOTIFY_SEVERITY_WARN 1` |
|         - | 6017 | `#define PHL_STREAM_NOTIFY_SEVERITY_ERR  2` |
|         - | 6018 | `typedef struct phl_stream_ctx phl_stream_ctx;` |
|         - | 6019 | `struct phl_stream_ctx` |
|         - | 6020 | `{` |
|         - | 6021 | `	io_private base;        /* io_private-compatible header (base.iMagic == STREAM_CTX_MAGIC) */` |
|         - | 6022 | `	ph7_vm *pVm;            /* owning VM */` |
|         - | 6023 | `	ph7_value *pOptions;    /* the wrapper => (option => value) map; never 0 */` |
|         - | 6024 | ``	ph7_value *pNotify;     /* the `notification` param, or 0 when none was set */`` |
|         - | 6025 | `	/* php's notifier carries the PROGRESS counter on the CONTEXT rather than on` |
|         - | 6026 | `	 * the stream, and never disarms it: a context reused for a second exchange` |
|         - | 6027 | `	 * reports that exchange's request write and header read under the FIRST` |
|         - | 6028 | `	 * one's running total, until the wrapper's own progress_init resets it.` |
|         - | 6029 | `	 * That is visible from a script, so it is modelled rather than approximated. */` |
|         - | 6030 | `	sxi64 iProgress;        /* bytes counted since the last init */` |
|         - | 6031 | `	sxi64 iProgressMax;     /* what the wrapper announced, or 0 for "unknown" */` |
|         - | 6032 | `	int bProgress;          /* has an init armed the counter yet? */` |
|         - | 6033 | `	int bNotifyDead;        /* the callback threw: php stops calling it */` |
|         - | 6034 | `	phl_stream_ctx *pNext;  /* registry chain (pVm->pStreamCtx) */` |
|         - | 6035 | `};` |
|         - | 6036 | `/* The context behind a ph7_value, or 0 when the value is not one. */` |
|         - | 6037 | `PH7_PRIVATE phl_stream_ctx * PH7_StreamCtxFromValue(ph7_value *pVal);` |
|         - | 6038 | `/* The per-VM DEFAULT context (stream_context_get_default), created on demand. */` |
|         - | 6039 | `PH7_PRIVATE phl_stream_ctx * PH7_StreamCtxDefault(ph7_vm *pVm);` |
|         - | 6040 | `/* One wrapper option, or 0 when the context does not carry it. */` |
|         - | 6041 | `PH7_PRIVATE ph7_value * PH7_StreamCtxOption(phl_stream_ctx *pCtxRes,const char *zWrapper,const char *zOption);` |
|         - | 6042 | `/* Drop every context this VM created (called from PH7_VmReset). */` |
|         - | 6043 | `PH7_PRIVATE void PH7_StreamCtxVmReset(ph7_vm *pVm);` |
|         - | 6044 | ``/* The `$context` argument of an opener, php's rules applied (see the body). */`` |
|         - | 6045 | `PH7_PRIVATE phl_stream_ctx * PH7_StreamCtxFromArg(ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|         - | 6046 | `	int iArg,const char *zArgName,int bNoDefault,int *pbThrew);` |
|         - | 6047 | `/* Arm the context PH7_StreamOpenHandle's next open runs under. */` |
|         - | 6048 | `PH7_PRIVATE void PH7_StreamCtxArm(ph7_vm *pVm,phl_stream_ctx *pRes);` |
|         - | 6049 | ``/* Tell the context's `notification` callback about one event. A NULL zMsg is`` |
|         - | 6050 | ` * php's null third argument; nMsg < 0 means "NUL-terminated". */` |
|         - | 6051 | `PH7_PRIVATE void PH7_StreamCtxNotify(phl_stream_ctx *pCtxRes,int iCode,int iSeverity,` |
|         - | 6052 | `	const char *zMsg,int nMsg,int iMsgCode,sxi64 iBytes,sxi64 iBytesMax);` |
|         - | 6053 | `/* php's progress notifier: arm the counter at 0 with a known maximum (and say` |
|         - | 6054 | ` * so), add to it, or report the end of the transfer. */` |
|         - | 6055 | `PH7_PRIVATE void PH7_StreamCtxProgressInit(phl_stream_ctx *pCtxRes,sxi64 iMax);` |
|         - | 6056 | `PH7_PRIVATE void PH7_StreamCtxProgressAdd(phl_stream_ctx *pCtxRes,sxi64 nDelta);` |
|         - | 6057 | `PH7_PRIVATE void PH7_StreamCtxCompleted(phl_stream_ctx *pCtxRes);` |
|         - | 6058 | `/*` |
|         - | 6059 | ` * ---------------------------------------------------------------------------` |
|         - | 6060 | ` * Stream filters.` |
|         - | 6061 | ` *` |
|         - | 6062 | ` * php runs a stream's bytes through a CHAIN on the way in and another on the` |
|         - | 6063 | ` * way out. A filter is handed a BRIGADE — the buckets that came off the device,` |
|         - | 6064 | ` * or that the script wrote — and appends what it made to a second one; what it` |
|         - | 6065 | ` * ANSWERS says whether that output may go on (PASS_ON), whether it needs more` |
|         - | 6066 | ` * input before it can produce any (FEED_ME), or whether the stream is finished` |
|         - | 6067 | ` * (ERR_FATAL). The brigade rather than one string is what lets a filter split` |
|         - | 6068 | ` * or merge its input, and what a userland filter walks with` |
|         - | 6069 | ` * stream_bucket_make_writeable().` |
|         - | 6070 | ` * ---------------------------------------------------------------------------` |
|         - | 6071 | ` */` |
|         - | 6072 | `/* php's PSFS_* filter results. */` |
|         - | 6073 | `#define PHL_PSFS_ERR_FATAL 0` |
|         - | 6074 | `#define PHL_PSFS_FEED_ME   1` |
|         - | 6075 | `#define PHL_PSFS_PASS_ON   2` |
|         - | 6076 | `/* php's PSFS_FLAG_* — which kind of call this is. FLUSH_CLOSE is the last one a` |
|         - | 6077 | ` * filter ever gets and the only chance a buffering filter has to emit its tail. */` |
|         - | 6078 | `#define PHL_PSFS_FLAG_NORMAL      0` |
|         - | 6079 | `#define PHL_PSFS_FLAG_FLUSH_INC   1` |
|         - | 6080 | `#define PHL_PSFS_FLAG_FLUSH_CLOSE 2` |
|         - | 6081 | `/* php's STREAM_FILTER_* chain selectors. */` |
|         - | 6082 | `#define PHL_STREAM_FILTER_READ  1` |
|         - | 6083 | `#define PHL_STREAM_FILTER_WRITE 2` |
|         - | 6084 | `#define PHL_STREAM_FILTER_ALL   3` |
|         - | 6085 | `/* stream_filter_append()'s handle carries this magic in the io_private-compatible` |
|         - | 6086 | ` * header every PHL resource opens with; php names the resource "stream filter". */` |
|         - | 6087 | `#define STREAM_FILTER_MAGIC 0xF117E4` |
|         - | 6088 | `typedef struct phl_bucket phl_bucket;` |
|         - | 6089 | `typedef struct phl_brigade phl_brigade;` |
|         - | 6090 | `typedef struct phl_stream_filter phl_stream_filter;` |
|         - | 6091 | `typedef struct phl_filter_ops phl_filter_ops;` |
|         - | 6092 | `/* stream_filter_register()'s two script-visible handles. php names them` |
|         - | 6093 | `` * `userfilter.bucket brigade` and `userfilter.bucket`. */`` |
|         - | 6094 | `#define STREAM_BRIGADE_MAGIC 0xB817AD` |
|         - | 6095 | `#define STREAM_BUCKET_MAGIC  0xB0C4E7` |
|         - | 6096 | `/* One bucket: a run of bytes travelling through a chain. */` |
|         - | 6097 | `struct phl_bucket` |
|         - | 6098 | `{` |
|         - | 6099 | `	SyBlob sData;      /* the bytes */` |
|         - | 6100 | `	phl_bucket *pNext; /* next bucket in the brigade */` |
|         - | 6101 | `};` |
|         - | 6102 | `struct phl_brigade` |
|         - | 6103 | `{` |
|         - | 6104 | `	phl_bucket *pHead,*pTail;` |
|         - | 6105 | `};` |
|         - | 6106 | ``/* The brigade a userland filter() is handed. Both `$in` and `$out` are one of`` |
|         - | 6107 | ` * these. They belong to the FILTER rather than to the call, so a script that` |
|         - | 6108 | ` * held one past the call it was handed in still has something in bounds to look` |
|         - | 6109 | ` * at — what it loses is the brigade behind it, which is cleared on the way out` |
|         - | 6110 | ` * and makes a stale handle answer "empty". */` |
|         - | 6111 | `typedef struct phl_brigade_res phl_brigade_res;` |
|         - | 6112 | `struct phl_brigade_res` |
|         - | 6113 | `{` |
|         - | 6114 | `	io_private base;      /* resource header (base.iMagic == STREAM_BRIGADE_MAGIC) */` |
|         - | 6115 | `	ph7_vm *pVm;` |
|         - | 6116 | `	phl_brigade *pBrig;   /* the brigade it stands for, 0 between calls */` |
|         - | 6117 | `};` |
|         - | 6118 | `/* What a built-in filter IS. A userland filter has no ops and runs its class. */` |
|         - | 6119 | `struct phl_filter_ops` |
|         - | 6120 | `{` |
|         - | 6121 | `	const char *zName;  /* php's own registered name */` |
|         - | 6122 | `	/* Read the $params argument, once, when the filter is created. A non-zero` |
|         - | 6123 | `	 * answer is php's "filter refused to be created". */` |
|         - | 6124 | `	int (*xCreate)(phl_stream_filter *pFilter,ph7_value *pParams);` |
|         - | 6125 | `	int (*xFilter)(phl_stream_filter *pFilter,phl_brigade *pIn,phl_brigade *pOut,int iFlags);` |
|         - | 6126 | `	void (*xClose)(phl_stream_filter *pFilter);` |
|         - | 6127 | `};` |
|         - | 6128 | `struct phl_stream_filter` |
|         - | 6129 | `{` |
|         - | 6130 | `	io_private base;            /* resource header (base.iMagic == STREAM_FILTER_MAGIC) */` |
|         - | 6131 | `	ph7_vm *pVm;                /* owning VM */` |
|         - | 6132 | `	const phl_filter_ops *pOps; /* built-in behaviour, or 0 for a userland filter */` |
|         - | 6133 | `	SyBlob sName;               /* the name it was CREATED under (a wildcard match keeps the request) */` |
|         - | 6134 | `	SyBlob sCarry;              /* bytes the filter could not encode yet (base64/qp/dechunk) */` |
|         - | 6135 | `	int iState;                 /* per-filter scalar state */` |
|         - | 6136 | `	sxu8 bClosed;               /* the FLUSH_CLOSE call has already been made */` |
|         - | 6137 | `	sxu8 bDead;                 /* it answered ERR_FATAL: the chain is finished */` |
|         - | 6138 | `	int iChain;                 /* PHL_STREAM_FILTER_READ or _WRITE */` |
|         - | 6139 | `	io_private *pDev;           /* the handle it is attached to; 0 once removed */` |
|         - | 6140 | `	phl_stream_filter *pNext;   /* next filter in that chain */` |
|         - | 6141 | `	phl_stream_filter *pRegNext;/* VM registry chain (pVm->pStreamFilter) */` |
|         - | 6142 | `	void *pPriv;                /* per-filter private state, freed by xClose */` |
|         - | 6143 | `	phl_brigade_res sIn,sOut;   /* the two handles filter() is given */` |
|         - | 6144 | `	void *pObj;                 /* userland filter instance (ph7_class_instance*) */` |
|         - | 6145 | `	ph7_value *pStreamRes;      /* the $stream the userland filter's property answers */` |
|         - | 6146 | `};` |
|         - | 6147 | `/* Brigade plumbing, shared with the userland-filter half. */` |
|         - | 6148 | `PH7_PRIVATE phl_bucket * PH7_FilterBucketNew(ph7_vm *pVm,const void *pData,sxu32 nLen);` |
|         - | 6149 | `PH7_PRIVATE phl_bucket * PH7_FilterBucketPop(phl_brigade *pBrig);` |
|         - | 6150 | `PH7_PRIVATE void PH7_FilterBucketAppend(phl_brigade *pBrig,phl_bucket *pBucket);` |
|         - | 6151 | `PH7_PRIVATE void PH7_FilterBucketFree(ph7_vm *pVm,phl_bucket *pBucket);` |
|         - | 6152 | `PH7_PRIVATE void PH7_FilterBrigadeRelease(ph7_vm *pVm,phl_brigade *pBrig);` |
|         - | 6153 | `/* Run one chain over nLen bytes, appending what came out to pOut. Answers a` |
|         - | 6154 | ` * PHL_PSFS_* code; ERR_FATAL means the stream is finished. */` |
|         - | 6155 | `PH7_PRIVATE int PH7_FilterChainProcess(phl_stream_filter *pHead,` |
|         - | 6156 | `	const void *pData,sxu32 nLen,int iFlags,int iRestFlags,SyBlob *pOut,int *pbUnread);` |
|         - | 6157 | `/* A seek moved the device: a chain that had already been CLOSED at the old end` |
|         - | 6158 | ` * of file has to be able to run again. */` |
|         - | 6159 | `PH7_PRIVATE void PH7_StreamFilterRewound(io_private *pDev);` |
|         - | 6160 | `/* Drop both chains of a handle, flushing the write one while the device is` |
|         - | 6161 | ` * still open (every close path and the io_private reset paths). */` |
|         - | 6162 | `PH7_PRIVATE void PH7_StreamFilterReleaseChains(io_private *pDev);` |
|         - | 6163 | `/* Attach a filter by NAME, php's own failure diagnostics raised from pCtx.` |
|         - | 6164 | ` * Answers the filter, or 0 when there is no such name. */` |
|         - | 6165 | `PH7_PRIVATE phl_stream_filter * PH7_StreamFilterAttach(ph7_vm *pVm,io_private *pDev,` |
|         - | 6166 | `	const char *zName,int nName,int iChain,int bPrepend,ph7_value *pParams,` |
|         - | 6167 | `	ph7_value *pStreamVal);` |
|         - | 6168 | `/* The filter behind a ph7_value, or 0 when the value is not a live one. */` |
|         - | 6169 | `PH7_PRIVATE phl_stream_filter * PH7_StreamFilterFromValue(ph7_value *pVal);` |
|         - | 6170 | `/* Drop every filter this VM created (called from PH7_VmReset). */` |
|         - | 6171 | `PH7_PRIVATE void PH7_StreamFilterVmReset(ph7_vm *pVm);` |
|         - | 6172 | `/* The stream_filter_register()/php_user_filter half. */` |
|         - | 6173 | `PH7_PRIVATE int PH7_builtin_stream_filter_register(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6174 | `PH7_PRIVATE int PH7_builtin_stream_bucket_make_writeable(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6175 | `PH7_PRIVATE int PH7_builtin_stream_bucket_append(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6176 | `PH7_PRIVATE int PH7_builtin_stream_bucket_prepend(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6177 | `PH7_PRIVATE int PH7_builtin_stream_bucket_new(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6178 | `PH7_PRIVATE sxi32 PH7_VmInstallStreamFilter(ph7_vm *pVm);` |
|         - | 6179 | `/* Attach the filters a php://filter URL names to the handle it wrapped. */` |
|         - | 6180 | `PH7_PRIVATE int PH7_StreamFilterParseUrl(ph7_vm *pVm,const char *zSpec,int nSpec,` |
|         - | 6181 | `	io_private *pDev,int iChains);` |
|         - | 6182 | ``/* php's `$stream` screen: a TypeError for a non-resource and for a closed one. */`` |
|         - | 6183 | `PH7_PRIVATE io_private * PH7_StreamHandleArg(ph7_context *pCtx,ph7_value *pArg,int iPos,` |
|         - | 6184 | `	const char *zName,int *pRc);` |
|         - | 6185 | `PH7_PRIVATE void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut);` |
|         - | 6186 | `PH7_PRIVATE void SetIOPrivateOpenedAs(io_private *pDev,const char *zUri,int nUriLen,const char *zMode,int nModeLen);` |
|         - | 6187 | `PH7_PRIVATE const char * PH7_StreamWrapperLabel(ph7_vm *pVm,const ph7_io_stream *pStream);` |
|         - | 6188 | `PH7_PRIVATE void MarkIOPrivateClosed(io_private *pDev);` |
|         - | 6189 | `PH7_PRIVATE void PH7_StreamReleaseUnopened(ph7_context *pCtx,io_private *pDev);` |
|         - | 6190 | `/* One buffered line off a handle, php's php_stream_get_line: the newline is` |
|         - | 6191 | ` * INCLUDED, nMaxLen (0 = no cap) bounds the bytes handed back and the remainder` |
|         - | 6192 | ` * stays buffered. The pointer is into the handle's own working buffer and the` |
|         - | 6193 | ` * next read invalidates it. */` |
|         - | 6194 | `PH7_PRIVATE ph7_int64 StreamReadLine(io_private *pDev,const char **pzData,ph7_int64 nMaxLen);` |
|         - | 6195 | `/* php's feof(): the end flag, but only once the line readers' look-ahead buffer` |
|         - | 6196 | ` * has been drained -- buffered bytes are not an end, and a userland wrapper is` |
|         - | 6197 | ` * asked the question itself. */` |
|         - | 6198 | `PH7_PRIVATE int PH7_StreamAtEof(io_private *pDev);` |
|         - | 6199 | ``/* php's `Read of N bytes failed with errno=...` notice, raised from whichever`` |
|         - | 6200 | ` * builtin or METHOD is asking. A no-op unless the last device read failed. */` |
|         - | 6201 | `PH7_PRIVATE void StreamReportReadFailure(ph7_context *pCtx,io_private *pDev);` |
|         - | 6202 | `/* Why PH7_StreamOpenPath() answered 0. It reports nothing itself: fopen() warns` |
|         - | 6203 | ` * and SplFileObject's constructor throws, which is php's own split. */` |
|         - | 6204 | `#define PH7_STREAM_OPEN_OK       0` |
|         - | 6205 | `#define PH7_STREAM_OPEN_NODEVICE 1 /* no wrapper is registered for the scheme */` |
|         - | 6206 | `#define PH7_STREAM_OPEN_FAILED   2 /* the wrapper refused the name (errno is set) */` |
|         - | 6207 | `#define PH7_STREAM_OPEN_NOMEM    3` |
|         - | 6208 | `#define PH7_STREAM_OPEN_BADMODE  4 /* the plain-file wrapper refused the MODE */` |
|         - | 6209 | `PH7_PRIVATE io_private * PH7_StreamOpenPath(ph7_context *pCtx,ph7_value *pPath,` |
|         - | 6210 | `	const char *zMode,int nMode,int bUseInclude,phl_stream_ctx *pCtxRes,` |
|         - | 6211 | `	ph7_value *pCtxArg,int *piErr,const char **pzErrUri);` |
|         - | 6212 | `/* "Failed to open stream" warning helper (vfs.c, errno-based); used by the` |
|         - | 6213 | ` * fopen/opendir/file_* family in vfs_stream.c. */` |
|         - | 6214 | `PH7_PRIVATE void VfsThrowOpenWarning(ph7_context *pCtx,const char *zFile);` |
|         - | 6215 | `/* strerror() for a failed stream OPEN: php's own substitution of ENOENT for` |
|         - | 6216 | ` * ENOTDIR, which belongs to the open and to no other file operation. */` |
|         - | 6217 | `PH7_PRIVATE const char * PH7_VfsOpenStrerror(int iErr);` |
|         - | 6218 | `PH7_PRIVATE void VfsThrowUnknownWrapperWarning(ph7_context *pCtx,const char *zUri);` |
|         - | 6219 | `PH7_PRIVATE int PH7_VfsEmptyPathRefused(ph7_context *pCtx,int nPath);` |
|         - | 6220 | `/* The separator php's own path expansion writes on this platform. */` |
|         - | 6221 | `#ifdef __WINNT__` |
|         - | 6222 | `#define PH7_PATH_SEP      '\\'` |
|         - | 6223 | `#define PH7_PATH_SEP_STR  "\\"` |
|         - | 6224 | `#else` |
|         - | 6225 | `#define PH7_PATH_SEP      '/'` |
|         - | 6226 | `#define PH7_PATH_SEP_STR  "/"` |
|         - | 6227 | `#endif` |
|         - | 6228 | `PH7_PRIVATE int PH7_VfsPathIsAbsolute(const char *zPath,int nPath);` |
|         - | 6229 | `PH7_PRIVATE void PH7_VfsExpandPath(ph7_context *pCtx,const char *zPath,int nPath,SyBlob *pOut);` |
|         - | 6230 | `/*` |
|         - | 6231 | ` * One name a directory walk produced, as an OFFSET into the blob that holds` |
|         - | 6232 | ` * them back to back -- the blob grows as the walk does, so a pointer would not` |
|         - | 6233 | ` * survive the next append. It is the shape glob:// keeps its matches in and the` |
|         - | 6234 | ` * shape ext/zip's addGlob()/addPattern() read them back out of.` |
|         - | 6235 | ` */` |
|         - | 6236 | `typedef struct PH7_GlobHit PH7_GlobHit;` |
|         - | 6237 | `struct PH7_GlobHit` |
|         - | 6238 | `{` |
|         - | 6239 | `	sxu32 nOfs;` |
|         - | 6240 | `	sxu32 nLen;` |
|         - | 6241 | `};` |
|         - | 6242 | ``/* Every entry of ONE directory, `.` and `..` included and sorted by bytes --`` |
|         - | 6243 | `` * php's `php_stream_scandir` with its alphasort comparator, which is what`` |
|         - | 6244 | ` * ZipArchive::addPattern() walks. */` |
|         - | 6245 | `PH7_PRIVATE sxi32 PH7_VfsListDir(ph7_vm *pVm,const char *zDir,int nDir,SyBlob *pHit,SySet *pSet);` |
|         - | 6246 | `/* A wrapper's own reason for refusing the open in flight, which is what php` |
|         - | 6247 | ` * prints after "Failed to open stream:" instead of an errno. Set from an xOpen` |
|         - | 6248 | ` * body; PH7_StreamOpenHandle() re-arms the default before every open. */` |
|         - | 6249 | `PH7_PRIVATE void PH7_StreamSetOpenError(ph7_vm *pVm,const char *zReason);` |
|         - | 6250 | ``/* The same, for php's `"Cls::method" call failed`: a userland wrapper's refusal`` |
|         - | 6251 | ` * names the call that made it, so the sentence is built and copied. */` |
|         - | 6252 | `PH7_PRIVATE void PH7_StreamSetOpenErrorCall(ph7_vm *pVm,const char *zClass,const char *zMethod);` |
|         - | 6253 | `PH7_PRIVATE void VfsThrowNoDeviceWarning(ph7_context *pCtx,const char *zUri,int bDir);` |
|         - | 6254 | `PH7_PRIVATE int PH7_VfsStatDoubleUp(ph7_value *pIn,ph7_value *pOut);` |
|         - | 6255 | `PH7_PRIVATE int PH7_VfsStatFill(ph7_value *pArray,ph7_value *pWorker,const ph7_int64 *aVal);` |
|         - | 6256 | `PH7_PRIVATE int PH7_VfsStatFromFd(int iFd,ph7_value *pArray,ph7_value *pWorker);` |
|         - | 6257 | `PH7_PRIVATE const char * VfsStrerror(int iErr);` |
|         - | 6258 | `/* Stream-device predicates (vfs_io_driver.c) */` |
|         - | 6259 | `PH7_PRIVATE int is_php_stream(const ph7_io_stream *pStream);` |
|         - | 6260 | `PH7_PRIVATE int is_data_stream(const ph7_io_stream *pStream);` |
|         - | 6261 | `/* Which php:// sub-stream a handle opened (PH7_IO_STREAM_*, 0 when unknown).` |
|         - | 6262 | ` * stream_get_meta_data() names MEMORY, TEMP and STDIO apart, and the device` |
|         - | 6263 | ` * itself is the only place that knows. */` |
|         - | 6264 | `PH7_PRIVATE int PH7_PhpStreamKind(void *pHandle);` |
|         - | 6265 | `/* Has a php://temp handle handed out its last byte? php's temp stream copies its` |
|         - | 6266 | ` * inner memory stream's eof, so it reports the end one read EARLIER than a bare` |
|         - | 6267 | ` * php://memory; 0 for every other device. */` |
|         - | 6268 | `PH7_PRIVATE int PH7_PhpStreamTempDrained(void *pHandle);` |
|         - | 6269 | `/* The handle a php://filter proxy wraps, or 0 for any other php:// stream. */` |
|         - | 6270 | `PH7_PRIVATE io_private * PH7_PhpStreamInner(void *pHandle);` |
|         - | 6271 | `/* That handle, or pDev itself when it is not a proxy. */` |
|         - | 6272 | `PH7_PRIVATE io_private * PH7_StreamUnwrap(io_private *pDev);` |
|         - | 6273 | `/* Where the SCRIPT is on a handle: the device position less what was read ahead. */` |
|         - | 6274 | `PH7_PRIVATE ph7_int64 PH7_StreamLogicalTell(io_private *pDev);` |
|         - | 6275 | `/* Seek the stream a php://filter proxy wraps, in the same model. */` |
|         - | 6276 | `PH7_PRIVATE int PH7_StreamSeekWrapped(io_private *pDev,ph7_int64 iOfft,int whence);` |
|         - | 6277 | `/* The POSIX descriptor behind an open handle, or -1 for a device that has none` |
|         - | 6278 | ` * (a memory buffer, a data:// payload, a userland wrapper) and on Windows,` |
|         - | 6279 | ` * where the file devices carry a HANDLE instead. Only the settings php applies` |
|         - | 6280 | ` * AT the descriptor need it. */` |
|         - | 6281 | `PH7_PRIVATE int PH7_StreamPosixFd(io_private *pDev);` |
|         - | 6282 | `/* Is this a handle php's plain-files device would own: a file, a pipe, a` |
|         - | 6283 | ` * standard stream? */` |
|         - | 6284 | `PH7_PRIVATE int PH7_StreamIsPlainDevice(io_private *pDev);` |
|         - | 6285 | ``/* The `stream_type` label a handle reports (STDIO / MEMORY / TEMP / Input /`` |
|         - | 6286 | ` * RFC2397 / dir / user-space / a transport's), which is what ext/posix names in` |
|         - | 6287 | ` * php's "Could not use stream of type '%s'". */` |
|         - | 6288 | `PH7_PRIVATE const char * PH7_StreamTypeLabel(io_private *pDev);` |
|         - | 6289 | `/* 1 / 0 / -1 ("ask the device instead"): can this handle report a position?` |
|         - | 6290 | `` * php's stream_get_meta_data() `seekable` is exactly this question. */`` |
|         - | 6291 | `PH7_PRIVATE int PH7_StreamHandleCanSeek(io_private *pDev);` |
|         - | 6292 | `/* The php:// sub-streams, as PH7_PhpStreamKind() reports them. */` |
|         - | 6293 | `#define PH7_IO_STREAM_STDIN  1 /* php://stdin */` |
|         - | 6294 | `#define PH7_IO_STREAM_STDOUT 2 /* php://stdout */` |
|         - | 6295 | `#define PH7_IO_STREAM_STDERR 3 /* php://stderr */` |
|         - | 6296 | `#define PH7_IO_STREAM_OUTPUT 4 /* php://output */` |
|         - | 6297 | `#define PH7_IO_STREAM_MEMORY 5 /* php://memory, php://temp, and data:// payloads */` |
|         - | 6298 | `#define PH7_IO_STREAM_FILTER 6 /* php://filter/…/resource=… — a stream wrapped around another */` |
|         - | 6299 | `/* php://input — the REQUEST BODY. There is none under a command line, and php's` |
|         - | 6300 | ` * CLI answers an empty stream for it rather than reading standard input (the` |
|         - | 6301 | `` * body a `php x.php < file` supplies arrives through STDIN, and php://input`` |
|         - | 6302 | ` * stays ""). It runs on the memory machinery, so it is seekable and can be read` |
|         - | 6303 | ` * twice, and it differs from php://memory in exactly four answers: fflush() is` |
|         - | 6304 | ` * FALSE, fstat() is FALSE, ftruncate() is unsupported, and its metadata names` |
|         - | 6305 | `` * it `Input`. */`` |
|         - | 6306 | `#define PH7_IO_STREAM_INPUT  7` |
|         - | 6307 | `/*` |
|         - | 6308 | ` * How far php's directory stream advances per entry read: one` |
|         - | 6309 | ``  * `php_stream_dirent`, which is `char d_name[MAXPATHLEN]` plus the `d_type` `` |
|         - | 6310 | ` * byte. php's MAXPATHLEN is PATH_MAX where the platform has one and a flat 2048` |
|         - | 6311 | ` * on Windows; both numbers were read back from the two php builds rather than` |
|         - | 6312 | `` * assumed (`readdir($d); ftell($d)` answers 4097 here and 2049 there). It is the`` |
|         - | 6313 | ` * only thing ftell() on a directory handle reports, and fseek()/rewind() rewind` |
|         - | 6314 | ` * the directory WITHOUT putting it back.` |
|         - | 6315 | ` */` |
|         - | 6316 | `#ifndef __WINNT__` |
|         - | 6317 | `#include <limits.h>    /* PATH_MAX, which is where php's MAXPATHLEN comes from */` |
|         - | 6318 | `#endif` |
|         - | 6319 | `#ifdef __WINNT__` |
|         - | 6320 | `#define PHL_DIR_RECORD 2049            /* php's win32 MAXPATHLEN is 2048 */` |
|         - | 6321 | `#elif defined(PATH_MAX)` |
|         - | 6322 | `#define PHL_DIR_RECORD (PATH_MAX + 1)  /* php takes MAXPATHLEN from PATH_MAX */` |
|         - | 6323 | `#else` |
|         - | 6324 | `#define PHL_DIR_RECORD 4097` |
|         - | 6325 | `#endif` |
|         - | 6326 | `PH7_PRIVATE int PH7_Utf8Read(` |
|         - | 6327 | `  const unsigned char *z,         /* First byte of UTF-8 character */` |
|         - | 6328 | `  const unsigned char *zTerm,     /* Pretend this byte is 0x00 */` |
|         - | 6329 | `  const unsigned char **pzNext    /* Write first byte past UTF-8 char here */` |
|         - | 6330 | `);` |
|         - | 6331 | `PH7_PRIVATE sxi32 PH7_Utf8ReadStrict(const unsigned char *z,sxu32 n,sxu32 *pLen);` |
|         - | 6332 | `/* parse.c function prototypes */` |
|         - | 6333 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc);` |
|         - | 6334 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot);` |
|         - | 6335 | `PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode);` |
|         - | 6336 | `PH7_PRIVATE int PH7_ExprNodeIsThis(ph7_expr_node *pNode);` |
|         - | 6337 | `PH7_PRIVATE int PH7_ExprNodeIsClassConst(ph7_expr_node *pNode);` |
|         - | 6338 | `PH7_PRIVATE int PH7_ExprIsModifiableValue(ph7_expr_node *pNode);` |
|         - | 6339 | `PH7_PRIVATE SyToken * PH7_ExprTokenInStream(ph7_gen_state *pGen,SyToken *pTok);` |
|         - | 6340 | `PH7_PRIVATE sxi32 PH7_ExprOperandNotAVariable(ph7_gen_state *pGen,ph7_expr_node *pOperand);` |
|         - | 6341 | `/* OP_STORE_REF / OP_STORE_IDX_REF iP1 bit 1: the reference SOURCE was written as a` |
|         - | 6342 | ` * CALL. php's compiler records the same thing (ZEND_RETURNS_FUNCTION) so the bind` |
|         - | 6343 | `` * can raise `Only variables should be assigned by reference` when the callee did`` |
|         - | 6344 | ` * not return by reference. Bit 0 stays STORE_IDX_REF's "a key is on the stack". */` |
|         - | 6345 | `#define PH7_STOREREF_CALLSRC 0x02` |
|         - | 6346 | `/* Context bits for GenStateWriteTargetCheck — php's write-target rules are the` |
|         - | 6347 | ` * same everywhere except for these two distinctions. */` |
|         - | 6348 | ``#define PH7_WTC_UNSET   0x01 /* `unset()`: the $this refusal takes its own wording */`` |
|         - | 6349 | ``#define PH7_WTC_REFSRC  0x02 /* the SOURCE of `=&`: php compiles it in write context`` |
|         - | 6350 | `                              * (so a temporary base is still refused) but never runs` |
|         - | 6351 | `                              * zend_ensure_writable_variable over it, which is why` |
|         - | 6352 | ``                              * `$r =& f()` is legal where `f() =& $x` is not */`` |
|         - | 6353 | ``#define PH7_WTC_RMW     0x04 /* a READ-MODIFY-WRITE target -- `+=`, `.=`, `++`, `--`.`` |
|         - | 6354 | ``                              * php's `$this` rule belongs to the ASSIGNMENT compiler`` |
|         - | 6355 | ``                              * (zend_compile_assign / assign_ref), so `$this += 1` and`` |
|         - | 6356 | ``                              * `$this++` compile and fail at RUN time on the operand`` |
|         - | 6357 | `                              * types instead. The temporary and call rules still apply:` |
|         - | 6358 | ``                              * `(new A)->p++` is refused exactly as `= 1` is. */`` |
|         - | 6359 | `PH7_PRIVATE sxi32 GenStateWriteTargetCheck(ph7_gen_state *pGen,ph7_expr_node *pTarget,int iCtx);` |
|         - | 6360 | `PH7_PRIVATE void PH7_ExprSubtreeSpan(ph7_expr_node *pNode,SyToken **ppMin,SyToken **ppMax);` |
|         - | 6361 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext);` |
|         - | 6362 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd);` |
|         - | 6363 | ``/* TRUE when a KEYWORD token opens `[static] fn[&](…) =>` rather than naming a`` |
|         - | 6364 | ` * variable/member/label ($fn, $o->fn, C::fn, \A\fn, f(fn: 1)). Every raw-token` |
|         - | 6365 | ` * lookahead that has to step over an arrow function must ask this first; the` |
|         - | 6366 | `` * test is positional, so a MALFORMED `fn` still reaches the arrow parser and`` |
|         - | 6367 | `` * keeps php's `expecting "("`. */`` |
|         - | 6368 | `PH7_PRIVATE int PH7_TokenOpensArrowFunc(SyToken *pStart,SyToken *pTok,SyToken *pEnd);` |
|         - | 6369 | `PH7_PRIVATE const ph7_expr_op * PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast);` |
|         - | 6370 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet);` |
|         - | 6371 | `/* compile.c function prototypes */` |
|         - | 6372 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType);` |
|         - | 6373 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 6374 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 6375 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 6376 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 6377 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 6378 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 6379 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 6380 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 6381 | `PH7_PRIVATE sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 6382 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 6383 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 6384 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 6385 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 6386 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 6387 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 6388 | `PH7_PRIVATE int GenStateIsReadonly(SyToken *pTok);` |
|         - | 6389 | `PH7_PRIVATE int GenStateStartsReadonlyAnonClass(SyToken *pIn,SyToken *pEnd);` |
|         - | 6390 | `PH7_PRIVATE int GenStateStartsClosureExpr(SyToken *pIn,SyToken *pEnd);` |
|         - | 6391 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 6392 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 6393 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen,sxi32 iCompileFlag);` |
|         - | 6394 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(ph7_vm *pVm,ProcConsumer xErr,void *pErrData);` |
|         - | 6395 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(ph7_vm *pVm,ProcConsumer xErr,void *pErrData);` |
|         - | 6396 | `PH7_PRIVATE void PH7_CompilerSaveState(ph7_vm *pVm,ph7_gen_state *pSaved,ProcConsumer xErr,void *pErrData);` |
|         - | 6397 | `PH7_PRIVATE void PH7_CompilerRestoreState(ph7_vm *pVm,ph7_gen_state *pSaved);` |
|         - | 6398 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...);` |
|         - | 6399 | `PH7_PRIVATE sxi32 PH7_GenSyntaxError(ph7_gen_state *pGen,SyToken *pTok,const char *zExpecting);` |
|         - | 6400 | `PH7_PRIVATE sxi32 PH7_CompileScript(ph7_vm *pVm,SyString *pScript,sxi32 iFlags);` |
|         - | 6401 | `/* constant.c function prototypes */` |
|         - | 6402 | `PH7_PRIVATE void PH7_RegisterBuiltInConstant(ph7_vm *pVm);` |
|         - | 6403 | `PH7_PRIVATE void PH7_MarkDeprecatedConstants(ph7_vm *pVm);` |
|         - | 6404 | `/* vm.c reference/frame internals shared with vm_builtin_var.c */` |
|         - | 6405 | `/* vm_gc.c -- the cycle collector */` |
|         - | 6406 | `PH7_PRIVATE void PH7_GcInit(ph7_vm *pVm);` |
|         - | 6407 | `PH7_PRIVATE void PH7_GcResetBuffer(ph7_vm *pVm);` |
|         - | 6408 | `PH7_PRIVATE void PH7_GcRelease(ph7_vm *pVm);` |
|         - | 6409 | `PH7_PRIVATE void PH7_GcPossibleRoot(ph7_vm *pVm,void *pCont,int bMap);` |
|         - | 6410 | `PH7_PRIVATE void PH7_GcForget(ph7_vm *pVm,void *pCont,int bMap);` |
|         - | 6411 | `PH7_PRIVATE sxu32 PH7_GcCollect(ph7_vm *pVm);` |
|         - | 6412 | `/* vm.c -- lifetime of a run-time closure's per-instantiation ph7_vm_func */` |
|         - | 6413 | `PH7_PRIVATE ph7_vm_func * PH7_VmRuntimeClosure(ph7_vm *pVm,const char *zName,sxu32 nByte);` |
|         - | 6414 | `PH7_PRIVATE void PH7_VmClosureFuncRef(ph7_vm_func *pFunc);` |
|         - | 6415 | `PH7_PRIVATE void PH7_VmClosureFuncUnref(ph7_vm *pVm,ph7_vm_func *pFunc);` |
|         - | 6416 | `PH7_PRIVATE void PH7_VmClosureInstanceRef(ph7_vm *pVm,ph7_class_instance *pObj,int iDelta);` |
|         - | 6417 | `PH7_PRIVATE void PH7_VmPurgeDeadClosures(ph7_vm *pVm);` |
|         - | 6418 | `PH7_PRIVATE SyHashEntry * PH7_VmSuperGet(ph7_vm *pVm,const char *zName,sxu32 nByte);` |
|         - | 6419 | `PH7_PRIVATE void PH7_VmSuperNote(ph7_vm *pVm,const char *zName,sxu32 nByte);` |
|         - | 6420 | `/* The reference table asks its questions BY SLOT: a slot that answers from its word` |
|         - | 6421 | ` * has no record for a caller to hold, so there is no VmRefObjExtract any more. */` |
|         - | 6422 | `PH7_PRIVATE sxu32 PH7_VmSlotEntryCount(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 6423 | `PH7_PRIVATE sxu32 PH7_VmSlotNodeCount(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 6424 | `PH7_PRIVATE sxu32 PH7_VmSlotPinCount(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 6425 | `PH7_PRIVATE int PH7_VmSlotKeepPinned(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 6426 | `PH7_PRIVATE int PH7_VmSlotRegistered(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 6427 | `PH7_PRIVATE int PH7_VmSlotSoleNodeIs(ph7_vm *pVm,sxu32 nIdx,ph7_hashmap_node *pNode);` |
|         - | 6428 | `PH7_PRIVATE void PH7_VmSlotUnlink(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 6429 | `PH7_PRIVATE int VmDropFrameLocalSlot(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 6430 | `PH7_PRIVATE void VmDropFrameRefEntry(ph7_vm *pVm,sxu32 nIdx,SyHashEntry *pEntry);` |
|         - | 6431 | `PH7_PRIVATE sxu32 PH7_VmSlotHolderCount(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 6432 | `PH7_PRIVATE int PH7_VmSlotSelfPinned(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 6433 | `PH7_PRIVATE int PH7_VmSlotDropOwnerHold(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 6434 | `PH7_PRIVATE void PH7_VmReleaseUnheldSlot(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 6435 | `PH7_PRIVATE void PH7_VmRebindVarSlot(ph7_vm *pVm,VmFrame *pFrame,SyHashEntry *pEntry,` |
|         - | 6436 | `	const char *zName,sxu32 nByte,sxu32 nIdx);` |
|         - | 6437 | `PH7_PRIVATE void PH7_VmBindVarSlot(ph7_vm *pVm,VmFrame *pFrame,const char *zName,sxu32 nByte,` |
|         - | 6438 | `	sxu32 nIdx);` |
|         - | 6439 | `PH7_PRIVATE int PH7_VmVarNameIsInternal(const char *zName,sxu32 nByte);` |
|         - | 6440 | `/* vm_builtin_var.c function prototypes (rows stay in vm.c's aVmFunc[]) */` |
|         - | 6441 | `PH7_PRIVATE sxi32 VmUnsetVarByName(ph7_vm *pVm,VmFrame *pFrame,const char *zName,sxu32 nByte);` |
|         - | 6442 | `PH7_PRIVATE sxi32 VmUnsetVarByNameEx(ph7_vm *pVm,VmFrame *pFrame,const char *zName,sxu32 nByte,int bNameGuard);` |
|         - | 6443 | `PH7_PRIVATE int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6444 | `PH7_PRIVATE int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6445 | `PH7_PRIVATE int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6446 | `PH7_PRIVATE int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6447 | `PH7_PRIVATE int vm_builtin_get_debug_type(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6448 | `PH7_PRIVATE int vm_builtin_settype(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6449 | `PH7_PRIVATE int vm_builtin_get_resource_id(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6450 | `PH7_PRIVATE int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6451 | `PH7_PRIVATE int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6452 | `PH7_PRIVATE int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6453 | `PH7_PRIVATE int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6454 | `/* vm_builtin_lang.c function prototypes (rows stay in vm.c's aVmFunc[]) */` |
|         - | 6455 | `PH7_PRIVATE sxi32 VmClassConstEvalOnDemand(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr);` |
|         - | 6456 | `PH7_PRIVATE void VmDeprecatedConstNotice(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pMember);` |
|         - | 6457 | `PH7_PRIVATE void VmNoDiscardWarn(ph7_vm *pVm,ph7_vm_func *pFunc,ph7_class *pDeclClass);` |
|         - | 6458 | `PH7_PRIVATE ph7_value * VmEnumCaseBackingValue(ph7_vm *pVm,ph7_class_attr *pCase);` |
|         - | 6459 | `PH7_PRIVATE sxi32 VmEnumMaterialize(ph7_vm *pVm,ph7_class *pClass);` |
|         - | 6460 | `PH7_PRIVATE void VmExpandConstantWithNotice(ph7_vm *pVm,ph7_constant *pCons,ph7_value *pOut);` |
|         - | 6461 | `PH7_PRIVATE sxi32 VmExpandConstantOnce(ph7_vm *pVm,ph7_constant *pCons,ph7_value *pOut);` |
|         - | 6462 | `PH7_PRIVATE void VmTrackOutput(ph7_vm *pVm, sxu32 nLen);` |
|         - | 6463 | `PH7_PRIVATE int PH7_VmOutputOrigin(ph7_vm *pVm,SyString *pFile,sxu32 *pnLine);` |
|         - | 6464 | `PH7_PRIVATE int PH7_VmSessionOrigin(ph7_vm *pVm,SyString *pFile,sxu32 *pnLine);` |
|         - | 6465 | `PH7_PRIVATE void PH7_VmSetSessionOrigin(ph7_vm *pVm);` |
|         - | 6466 | `PH7_PRIVATE void PH7_VmAppendWhere(ph7_vm *pVm,SyBlob *pMsg,int bSessionActive);` |
|         - | 6467 | `PH7_PRIVATE ph7_value * VmExtractMemObj(ph7_vm *pVm,const SyString *pName,int bDup,int bCreate);` |
|         - | 6468 | `PH7_PRIVATE ph7_value * PH7_VmExtractVarSlot(ph7_vm *pVm,const SyString *pName,int bCreate,sxu32 nSlot,const VmInstr *aCode);` |
|         - | 6469 | `/* Is a store to this slot filtered at all? The screen in front of every hTypedSlot` |
|         - | 6470 | ` * lookup on a hot path; a false answer is final, a true one still asks the table.` |
|         - | 6471 | ` * With the bitmap disabled it degrades to the emptiness test every one of those` |
|         - | 6472 | ` * call sites used before it existed, which over-answers and never under-answers. */` |
|         - | 6473 | `#define PH7_VM_STORE_FILTERED(pVm,nIdx) \` |
|         - | 6474 | `	((pVm)->bFilterBitsOff \` |
|         - | 6475 | `	 ? SyHashTotalEntry(&(pVm)->hTypedSlot) > 0 \` |
|         - | 6476 | `	 : ((nIdx) < (pVm)->nFilterBits \` |
|         - | 6477 | `	    && ((pVm)->pFilterBits[(nIdx) >> 3] & (1 << ((nIdx) & 7))) != 0))` |
|         - | 6478 | `PH7_PRIVATE void VmVarMemoFlush(VmFrame *pFrame);` |
|         - | 6479 | `/* D1 commit 2: deferred-lvalue-path capture (built by the LOAD_IDX/MEMBER record modes) */` |
|         - | 6480 | `PH7_PRIVATE VmDeferredPath * VmDeferPathNew(ph7_vm *pVm,int eRoot,sxu32 nRootIdx,const SyString *pName);` |
|         - | 6481 | `PH7_PRIVATE VmDeferredPath * VmDeferPathNewPrefetch(ph7_vm *pVm,int nKind,ph7_class *pClass,` |
|         - | 6482 | `	const SyString *pName,ph7_value *pVal);` |
|         - | 6483 | `PH7_PRIVATE sxi32 VmDeferPathPushElem(VmDeferredPath *pPath,ph7_value *pKey);` |
|         - | 6484 | `PH7_PRIVATE sxi32 VmDeferPathPushAppend(VmDeferredPath *pPath);` |
|         - | 6485 | `PH7_PRIVATE sxi32 VmDeferPathPushProp(VmDeferredPath *pPath,const SyString *pName);` |
|         - | 6486 | `PH7_PRIVATE void VmFreeDeferredPath(VmDeferredPath *pPath);` |
|         - | 6487 | `PH7_PRIVATE VmCoalStrOff * VmCoalStrOffNew(ph7_vm *pVm,ph7_value *pKey);` |
|         - | 6488 | `PH7_PRIVATE void VmFreeCoalStrOff(VmCoalStrOff *pCoal);` |
|         - | 6489 | `PH7_PRIVATE VmMagicCall * VmMagicCallNew(ph7_vm *pVm,ph7_class_instance *pRecv,` |
|         - | 6490 | `	ph7_class *pClass,const SyString *pName);` |
|         - | 6491 | `PH7_PRIVATE void VmFreeMagicCall(VmMagicCall *pPend);` |
|         - | 6492 | `PH7_PRIVATE void VmExpandUserConstant(ph7_value *pVal,void *pUserData);` |
|         - | 6493 | `PH7_PRIVATE ph7_class * VmExtractEnumClass(ph7_vm *pVm,ph7_value *pName);` |
|         - | 6494 | `PH7_PRIVATE int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6495 | `PH7_PRIVATE int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6496 | `PH7_PRIVATE int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6497 | `PH7_PRIVATE int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6498 | `PH7_PRIVATE int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6499 | `PH7_PRIVATE int vm_builtin_enum_cases(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6500 | `PH7_PRIVATE int vm_builtin_enum_exists(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6501 | `PH7_PRIVATE int vm_builtin_enum_from(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6502 | `PH7_PRIVATE int vm_builtin_enum_tryfrom(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6503 | `PH7_PRIVATE int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6504 | `PH7_PRIVATE int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6505 | `PH7_PRIVATE int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6506 | `PH7_PRIVATE int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6507 | `PH7_PRIVATE int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6508 | `PH7_PRIVATE int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6509 | `PH7_PRIVATE int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6510 | `PH7_PRIVATE int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6511 | `PH7_PRIVATE int vm_builtin_php_sapi_name(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6512 | `PH7_PRIVATE int vm_builtin_php_ini_loaded_file(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6513 | `PH7_PRIVATE int vm_builtin_phpversion(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6514 | `PH7_PRIVATE int vm_builtin_extension_loaded(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6515 | `PH7_PRIVATE int vm_builtin_get_loaded_extensions(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6516 | `/* vm_extension.c -- the extension partition every internal name is placed in */` |
|         - | 6517 | `PH7_PRIVATE const char * PH7_VmExtensionName(int iExt);` |
|         - | 6518 | `PH7_PRIVATE int PH7_VmExtensionLookup(const char *zName,int nName);` |
|         - | 6519 | `PH7_PRIVATE int PH7_VmExtensionIsLoaded(ph7_vm *pVm,const char *zName,int nName);` |
|         - | 6520 | `PH7_PRIVATE int PH7_VmExtOfFunc(const char *zName,int nName);` |
|         - | 6521 | `PH7_PRIVATE int PH7_VmExtOfClass(const char *zName,int nName);` |
|         - | 6522 | `PH7_PRIVATE int PH7_VmExtOfConstant(const char *zName,int nName);` |
|         - | 6523 | `PH7_PRIVATE int PH7_VmExtOfIni(const char *zName,int nName);` |
|         - | 6524 | `#define PH7_EXT_KIND_FUNC   0` |
|         - | 6525 | `#define PH7_EXT_KIND_CLASS  1` |
|         - | 6526 | `#define PH7_EXT_KIND_CONST  2` |
|         - | 6527 | `#define PH7_EXT_KIND_INI    3` |
|         - | 6528 | `PH7_PRIVATE int PH7_VmExtWalk(int iExt,int iKind,int (*xVisit)(const char *,int,void *),void *pData);` |
|         - | 6529 | `PH7_PRIVATE int PH7_VmInternalNameExists(ph7_vm *pVm,int iKind,const char *zName,int nName);` |
|         - | 6530 | `PH7_PRIVATE int PH7_VmExtWalkDep(int iExt,int (*xVisit)(const char *,const char *,void *),void *pData);` |
|         - | 6531 | `#define PH7_EXT_CORE 0        /* the engine itself; every other id is vm_extension_names.h's */` |
|         - | 6532 | `#define PH7_EXT_MAX  64        /* a caller's per-extension scratch bound; the table is far under it */` |
|         - | 6533 | `PH7_PRIVATE int PH7_VmExtHasName(int iKind,const char *zName,int nName);` |
|         - | 6534 | `PH7_PRIVATE int PH7_VmExtensionCount(void);` |
|         - | 6535 | `PH7_PRIVATE int PH7_VmExtensionAvailable(int iExt);` |
|         - | 6536 | `PH7_PRIVATE int vm_builtin_get_extension_funcs(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6537 | `PH7_PRIVATE int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6538 | `PH7_PRIVATE int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6539 | `PH7_PRIVATE int vm_builtin_random_bytes(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6540 | `PH7_PRIVATE int vm_builtin_random_int(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6541 | `PH7_PRIVATE int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6542 | `PH7_PRIVATE int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6543 | `/* vm.c frame/backtrace internals shared with vm_builtin_error.c */` |
|         - | 6544 | `PH7_PRIVATE VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|         - | 6545 | `PH7_PRIVATE void VmBuildBacktrace(ph7_vm *pVm,sxi32 iOptions,sxi32 iLimit,ph7_value *pList);` |
|         - | 6546 | `PH7_PRIVATE void PH7_GenAppendFatalTrace(ph7_vm *pVm,SyBlob *pOut,int iTraceKind);` |
|         - | 6547 | `PH7_PRIVATE void PH7_VmTraceToString(ph7_vm *pVm,ph7_value *pTrace,int bMainMarker,SyBlob *pOut);` |
|         - | 6548 | `PH7_PRIVATE void PH7_VmFrameActualArgs(ph7_vm *pVm,VmFrame *pFrame,ph7_value *pArray);` |
|         - | 6549 | `/* vm_builtin_error.c function prototypes (rows stay in vm.c's aVmFunc[]) */` |
|         - | 6550 | `PH7_PRIVATE int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6551 | `PH7_PRIVATE int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6552 | `PH7_PRIVATE int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6553 | `PH7_PRIVATE int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6554 | `PH7_PRIVATE int vm_builtin_error_clear_last(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6555 | `PH7_PRIVATE int vm_builtin_error_get_last(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6556 | `PH7_PRIVATE int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6557 | `PH7_PRIVATE int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6558 | `PH7_PRIVATE int vm_builtin_get_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6559 | `PH7_PRIVATE int vm_builtin_get_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6560 | `PH7_PRIVATE int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6561 | `PH7_PRIVATE int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6562 | `PH7_PRIVATE int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6563 | `PH7_PRIVATE int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6564 | `PH7_PRIVATE int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 6565 | `/* Autoload-callback record (spl_autoload_register in vm_include.c; walked by` |
|         - | 6566 | ` * VmTriggerAutoload in vm.c) */` |
|         - | 6567 | `typedef struct VmAutoloadCB VmAutoloadCB;` |
|         - | 6568 | `struct VmAutoloadCB` |
|         - | 6569 | `{` |
|         - | 6570 | `	ph7_value sCallback; /* Autoload callback (string or [obj,method] array) */` |
|         - | 6571 | `};` |
|         - | 6572 | `/* Shutdown-callback record (register_shutdown_function in vm_builtin_call.c;` |
|         - | 6573 | ` * invoked by VmInvokeShutdownCallbacks in vm.c) */` |
|         - | 6574 | `typedef struct VmShutdownCB VmShutdownCB;` |
|         - | 6575 | `struct VmShutdownCB` |
|         - | 6576 | `{` |
|         - | 6577 | `	ph7_value sCallback; /* Shutdown callback */` |
|         - | 6578 | `	ph7_value aArg[10];   /* Callback arguments (10 maximum arguments) */` |
|         - | 6579 | `	int nArg;             /* Total number of given arguments */` |
|         - | 6580 | `};` |
|         - | 6581 | `/* Operand-stack guard slack (vm.c allocator; checked by the call machinery) */` |
|         - | 6582 | `#define VM_STACK_GUARD 16` |
|         - | 6583 | `/* vm.c closure/exception internals shared with vm_builtin_call.c */` |
|         - | 6584 | `PH7_PRIVATE int VmValueIsClosure(ph7_vm *pVm,ph7_value *pVal);` |
|         - | 6585 | `PH7_PRIVATE int PH7_VmFuncDisplayName(ph7_vm *pVm,ph7_vm_func *pFunc,const char **pzOut);` |
|         - | 6586 | `PH7_PRIVATE sxi32 VmClosureUnwrap(ph7_vm *pVm,ph7_value *pVal,ph7_value *pOut);` |
|         - | 6587 | `PH7_PRIVATE sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|         - | 6588 | `PH7_PRIVATE sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|         - | 6589 | `PH7_PRIVATE ph7_value * VmNewOperandStack(ph7_vm *pVm,sxu32 nInstr);` |
|         - | 6590 | `/* Fiber/generator trampoline state (BYTECODE stages 2-4); shared between` |
|         - | 6591 | ` * vm.c's interpreter and vm_exec_ctx.c's park/resume machinery. */` |
|         - | 6592 | `/*` |
|         - | 6593 | ` * Boundary state of one VmByteCodeExec activation (BYTECODE.md stage 1):` |
|         - | 6594 | ` * everything the executor must restore to continue an activation after a` |
|         - | 6595 | ` * nested call returns. pc/pTos are authoritative here only at activation` |
|         - | 6596 | ` * boundaries — the dispatch loop keeps them in locals for the hot path and` |
|         - | 6597 | ` * syncs around the call epilogue and the terminal labels. Stage 2 stacks` |
|         - | 6598 | ` * these records to replace the native recursion.` |
|         - | 6599 | ` */` |
|         - | 6600 | `typedef struct VmExecState VmExecState;` |
|         - | 6601 | `struct VmExecState` |
|         - | 6602 | `{` |
|         - | 6603 | `	VmInstr *aInstr;        /* Bytecode of this activation */` |
|         - | 6604 | `	ph7_value *pStack;      /* Operand-stack base (owned by this activation) */` |
|         - | 6605 | `	ph7_value *pTos;        /* Top-of-stack (synced at boundaries) */` |
|         - | 6606 | `	ph7_value *pHigh;       /* WATERMARK: the deepest pTos this activation ever reached,` |
|         - | 6607 | `	                         * sampled at each instruction fetch and synced with pTos at` |
|         - | 6608 | `	                         * the same boundaries. Nothing above it was ever written, so` |
|         - | 6609 | `	                         * it is what the operand stack's teardown sweep walks to --` |
|         - | 6610 | `	                         * see VmOperandStackRecycle. An OP_SPREAD that reallocs the` |
|         - | 6611 | `	                         * buffer resets it to the whole (grown) capacity rather than` |
|         - | 6612 | `	                         * carrying a pointer into the freed one. */` |
|         - | 6613 | `	sxu32 nStackCap;        /* pStack's allocated slot count; grows when an OP_SPREAD in` |
|         - | 6614 | `	                         * this activation reallocs the operand stack (see` |
|         - | 6615 | `	                         * VmGrowOperandStack). Saved/restored with the activation. */` |
|         - | 6616 | `	sxu32 nStackOrig;       /* The activation's ORIGINAL (ungrown) capacity — nMaxStack+guard,` |
|         - | 6617 | `	                         * fixed at entry. VmGrowOperandStack sizes headroom relative to` |
|         - | 6618 | `	                         * THIS (not the grown nStackCap) so capacity can't ratchet up` |
|         - | 6619 | `	                         * across statements that share one operand stack. */` |
|         - | 6620 | `	sxi32 pc;               /* Program counter (synced at boundaries) */` |
|         - | 6621 | `	sxu32 nExceptionBase;   /* Exception-stack depth at entry (finally-drain floor) */` |
|         - | 6622 | `	sxu32 nFinallyActBase;  /* aFinallyAction depth at entry: actions above it belong to` |
|         - | 6623 | `	                         * this activation and are DISCARDED (refs released) when the` |
|         - | 6624 | ``	                         * activation ends — a `return` inside a redirect-entered`` |
|         - | 6625 | `	                         * finally short-circuits OP_END_FINALLY, orphaning its` |
|         - | 6626 | `	                         * pending action (an FA_RETHROW holding the swallowed` |
|         - | 6627 | `	                         * exception), which would otherwise be mis-popped by an` |
|         - | 6628 | `	                         * enclosing function's next END_FINALLY. */` |
|         - | 6629 | `	VmFrame *pEntryFrame;   /* Active frame at entry (exec identity for VmRecordedResume) */` |
|         - | 6630 | `	ph7_value *pResult;     /* Where the terminal OP_DONE stores the result (or NULL) */` |
|         - | 6631 | `	sxu32 *pLastRef;        /* By-ref return out-param (or NULL) */` |
|         - | 6632 | `	ph7_vm_func *pEnforceRetFunc; /* Return-type enforcement target (user-fn bodies only) */` |
|         - | 6633 | `	sxu8 is_callback;       /* TRUE only for a C->PHP callback trampoline activation */` |
|         - | 6634 | `	sxu8 bReturnPropagates; /* TRUE only for a catch/finally mini-program */` |
|         - | 6635 | `};` |
|         - | 6636 | `/*` |
|         - | 6637 | ` * One in-flight user-function call: what the caller's OP_CALL set up and the` |
|         - | 6638 | ` * pop boundary (VmCallFinish) must tear down.` |
|         - | 6639 | ` */` |
|         - | 6640 | `typedef struct VmCallRecord VmCallRecord;` |
|         - | 6641 | `struct VmCallRecord` |
|         - | 6642 | `{` |
|         - | 6643 | `	ph7_vm_func *pVmFunc;   /* Callee */` |
|         - | 6644 | `	VmFrame *pFrame;        /* Callee's VM frame (entered by the OP_CALL setup) */` |
|         - | 6645 | `	ph7_value *pFrameStack; /* Callee's operand stack (owned; freed here). NULL when the body was skipped */` |
|         - | 6646 | `	sxu32 nStackCap;        /* pFrameStack's allocated slot count — nMaxStack+VM_STACK_GUARD` |
|         - | 6647 | `	                         * at setup, updated if an OP_SPREAD in the callee grew it; the` |
|         - | 6648 | `	                         * pop-time recycle releases exactly this many slots */` |
|         - | 6649 | `	sxu32 nLiveTos;         /* How many of pFrameStack's slots this activation ever` |
|         - | 6650 | `	                         * touched: its operand-stack watermark + 1. The pop-time` |
|         - | 6651 | `	                         * recycle releases exactly this many and leaves the rest` |
|         - | 6652 | `	                         * alone -- see VmOperandStackRecycle */` |
|         - | 6653 | `	sxu32 nLastRef;         /* Callee body's last-referenced slot (by-ref return) */` |
|         - | 6654 | `	sxu8 bSelfPushed;       /* TRUE when the setup pushed onto pVm->aSelf */` |
|         - | 6655 | `};` |
|         - | 6656 | `/*` |
|         - | 6657 | ` * One node of the in-loop call-record stack (BYTECODE stage 2): the caller's` |
|         - | 6658 | ` * activation to restore plus the in-flight call to finish, linked to the` |
|         - | 6659 | ` * next-outer record. Nodes are pool-allocated individually so pointers into` |
|         - | 6660 | ` * them (sState.pLastRef aims at sCall.nLastRef while the callee runs) stay` |
|         - | 6661 | ` * stable — a growable array would invalidate them on realloc. The stack is a` |
|         - | 6662 | ` * LOCAL of each native VmByteCodeExec invocation: an inner native entry` |
|         - | 6663 | ` * (mini-program, C->PHP callback, ctx resume) can never unwind records that` |
|         - | 6664 | ` * belong to an outer invocation, preserving the old nesting isolation by` |
|         - | 6665 | ` * construction.` |
|         - | 6666 | ` */` |
|         - | 6667 | `typedef struct VmCallFrame VmCallFrame;` |
|         - | 6668 | `struct VmCallFrame` |
|         - | 6669 | `{` |
|         - | 6670 | `	VmExecState sCaller;   /* Caller activation, restored on pop */` |
|         - | 6671 | `	VmCallRecord sCall;    /* The in-flight call, finished (VmCallFinish) on pop */` |
|         - | 6672 | `	VmCallFrame *pPrev;    /* Next-outer record, or NULL at this invocation's base */` |
|         - | 6673 | `};` |
|         - | 6674 | `typedef struct VmParkedSegment VmParkedSegment;` |
|         - | 6675 | `/*` |
|         - | 6676 | ` * BYTECODE stage 4: a Fiber::suspend() from inside a nested PHP call parks the` |
|         - | 6677 | ` * whole trampoline record segment here instead of unwinding it. The records,` |
|         - | 6678 | ` * their VmFrames and operand stacks all stay alive on the heap (that IS what a` |
|         - | 6679 | ` * suspended fiber is); only the dispatch loop's pointers move into the ctx.` |
|         - | 6680 | ` * Resume re-pushes the chain and continues INSIDE the innermost callee.` |
|         - | 6681 | ` */` |
|         - | 6682 | `struct VmParkedSegment` |
|         - | 6683 | `{` |
|         - | 6684 | `	VmExecState sState;    /* Innermost activation — resume re-enters here (pTos synced) */` |
|         - | 6685 | `	VmCallFrame *pCallTop; /* Parked record chain (caller activations toward the body) */` |
|         - | 6686 | `	VmFrame *pTopFrame;    /* pVm->pFrame at suspend (innermost callee / open-try frame) */` |
|         - | 6687 | `	sxu32 nOldExcBase;     /* pCtx->nExceptionBase at park — resume rebases the segment's` |
|         - | 6688 | `	                        * absolute nExceptionBase floors by (newBase - nOldExcBase) */` |
|         - | 6689 | `	sxu32 nOldFinBase;     /* pCtx->nFinallyBase at park — resume rebases the segment's` |
|         - | 6690 | `	                        * absolute nFinallyActBase floors by its OWN delta (the two` |
|         - | 6691 | `	                        * stacks move independently) */` |
|         - | 6692 | `	int nRecords;          /* Chain length: each record contributed one nRecursionDepth++` |
|         - | 6693 | `	                        * (and, if bSelfPushed, one aSelf push) that VmCallFinish never` |
|         - | 6694 | `	                        * ran. Deactivate that accounting while parked, reactivate on` |
|         - | 6695 | `	                        * resume; an abandoned segment stays deactivated. */` |
|         - | 6696 | `};` |
|         - | 6697 |  |
|         - | 6698 | `PH7_PRIVATE sxi32 VmByteCodeExec(ph7_vm *pVm,VmInstr *aInstr,ph7_value *pStack,int nTos,` |
|         - | 6699 | `	ph7_value *pResult,sxu32 *pLastRef,int is_callback,sxi32 nPc,` |
|         - | 6700 | `	ph7_vm_func *pEnforceRetFunc,int bReturnPropagates,` |
|         - | 6701 | `	VmParkedSegment *pAdoptSegment,ph7_value **ppBaseOwner,` |
|         - | 6702 | `	sxu32 *pnBaseCap,sxu32 nStackOrig);` |
|         - | 6703 | `/* vm.c frame/type-enforcement internals shared with vm_exec_ctx.c (and the` |
|         - | 6704 | ` * upcoming vm_error.c) */` |
|         - | 6705 | `PH7_PRIVATE int VmCheckPseudoType(ph7_vm *pVm, ph7_value *pValue, const SyString *pClass);` |
|         - | 6706 | `PH7_PRIVATE sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable, int bStrict,` |
|         - | 6707 | `	ph7_class *pSelf);` |
|         - | 6708 | `PH7_PRIVATE void VmMaterializeIntTyped(ph7_value *pVal, sxu32 nType);` |
|         - | 6709 | `PH7_PRIVATE void VmDropResumeTarget(ph7_vm *pVm, VmFrame *pFrame);` |
|         - | 6710 | `PH7_PRIVATE void VmReleaseFrameForeachSteps(ph7_vm *pVm, VmFrame *pFrame);` |
|         - | 6711 | `/* The in-place-catch resume record, moved as a whole. See its four fields in ph7_vm. */` |
|         - | 6712 | `typedef struct VmResumeTarget {` |
|         - | 6713 | `	VmFrame *pFrame;` |
|         - | 6714 | `	sxu32 iPc;` |
|         - | 6715 | `	void *pInstr;` |
|         - | 6716 | `	sxi32 iStackDepth;` |
|         - | 6717 | `} VmResumeTarget;` |
|         - | 6718 | `PH7_PRIVATE void VmSetResumeTarget(ph7_vm *pVm,VmFrame *pFrame,sxu32 iPc,void *pInstr,sxi32 iStackDepth);` |
|         - | 6719 | `PH7_PRIVATE void VmClearResumeTarget(ph7_vm *pVm);` |
|         - | 6720 | `PH7_PRIVATE void VmSaveResumeTarget(ph7_vm *pVm,VmResumeTarget *pSave);` |
|         - | 6721 | `PH7_PRIVATE void VmRestoreResumeTarget(ph7_vm *pVm,const VmResumeTarget *pSave);` |
|         - | 6722 | `/* Flags for VmEnforcePropertyTypeOnStore(). CLONE_INIT is php 8.5's` |
|         - | 6723 | ` * clone-with re-initialization of a readonly property; VIA_REF says the write` |
|         - | 6724 | ` * arrived through a REFERENCE to the slot rather than through the property` |
|         - | 6725 | ` * itself, which is a sentence of its own in php. */` |
|         - | 6726 | `#define VM_TYPED_STORE_CLONE_INIT 0x01` |
|         - | 6727 | `#define VM_TYPED_STORE_VIA_REF    0x02` |
|         - | 6728 | `PH7_PRIVATE sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue,int iStoreFlags);` |
|         - | 6729 | `PH7_PRIVATE sxi32 PH7_VmNativeSetSlot(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue);` |
|         - | 6730 | `PH7_PRIVATE sxi32 PH7_VmStoreFilterRegister(ph7_vm *pVm,VmClassAttr *pVmAttr);` |
|         - | 6731 | `PH7_PRIVATE void PH7_VmStoreFilterDrop(ph7_vm *pVm,ph7_class_attr *pAttr,sxu32 nIdx);` |
|         - | 6732 | `PH7_PRIVATE sxi32 VmEnforceScalarType(ph7_value *pVal, sxu32 nType, int bStrict);` |
|         - | 6733 | `PH7_PRIVATE void VmExcReleaseAll(ph7_vm *pVm,SySet *pSet);` |
|         - | 6734 | `PH7_PRIVATE const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf);` |
|         - | 6735 | `PH7_PRIVATE int VmFuncHasReturnType(ph7_vm_func *pFunc);` |
|         - | 6736 | `PH7_PRIVATE int PH7_VmHookSplitName(SyString *pName,SyString *pProp,const char **pzKind);` |
|         - | 6737 | `PH7_PRIVATE int PH7_VmHookFuncName(ph7_class *pClass,ph7_vm_func *pFunc,SyBlob *pOut);` |
|         - | 6738 | `PH7_PRIVATE sxu32 VmFuncRequiredArgCount(ph7_vm_func *pFunc,sxu32 *pnNonVariadic);` |
|         - | 6739 | `PH7_PRIVATE void VmLeaveFrame(ph7_vm *pVm);` |
|         - | 6740 | `PH7_PRIVATE int VmNativeNestingExceeded(ph7_vm *pVm);` |
|         - | 6741 | `PH7_PRIVATE sxi32 VmNativeNestingFatal(ph7_vm *pVm);` |
|         - | 6742 | `PH7_PRIVATE VmFrame * VmNewFrame(ph7_vm *pVm, void *pUserData, ph7_class_instance *pThis);` |
|         - | 6743 | `PH7_PRIVATE ph7_class *VmHintScopeClass(ph7_vm *pVm, ph7_class *pDecl, ph7_class *pUsing);` |
|         - | 6744 | `PH7_PRIVATE int VmClassHintMatches(ph7_vm *pVm,const SyString *pName,ph7_class *pScope,` |
|         - | 6745 | `	ph7_value *pVal,ph7_class **ppResolved);` |
|         - | 6746 | `PH7_PRIVATE const char *VmHintTextResolved(ph7_vm *pVm,const SyString *pDeclared,ph7_class *pScope,` |
|         - | 6747 | `	char *zBuf,sxu32 nBuf);` |
|         - | 6748 | ``#define PH7_HINT_TEXT_ITERABLE 0x1 /* expand a standalone `iterable` to Traversable\|array */`` |
|         - | 6749 | ``#define PH7_HINT_TEXT_STATIC   0x2 /* resolve `static` beside `self`/`parent` */`` |
|         - | 6750 | `PH7_PRIVATE const char *VmHintTextResolvedEx(ph7_vm *pVm,const SyString *pDeclared,ph7_class *pScope,` |
|         - | 6751 | `	int iFlags,char *zBuf,sxu32 nBuf);` |
|         - | 6752 | `PH7_PRIVATE ph7_class *VmHintScopeDeclared(ph7_class *pDecl);` |
|         - | 6753 | `PH7_PRIVATE ph7_class *VmResolveTypeClass(ph7_vm *pVm, const SyString *pCN, ph7_class *pSelf);` |
|         - | 6754 | `PH7_PRIVATE const char *VmScalarTypeName(sxu32 nType, SyString *pDeclared, char *zBuf, sxu32 nBuf);` |
|         - | 6755 | `PH7_PRIVATE const char *VmClassHintTypeName(const SyString *pAsWritten,ph7_class *pResolved,` |
|         - | 6756 | `	int bNullable,char *zBuf,sxu32 nBuf);` |
|         - | 6757 | `PH7_PRIVATE sxi32 VmThrowBuiltinTooFewArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName, sxu32 nPassed,sxu32 nRequired,sxu32 nMaxDeclared);` |
|         - | 6758 | `PH7_PRIVATE sxi32 VmThrowBuiltinTooManyArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName, sxu32 nPassed,sxu32 nMax,sxu32 nRequired);` |
|         - | 6759 | `PH7_PRIVATE sxi32 VmThrowTooFewArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName, ph7_vm_func *pCallee,sxu32 nPassed,sxu32 nRequired,sxu32 nNonVariadic,int bCallSite);` |
|         - | 6760 | `PH7_PRIVATE void PH7_VmWarnByRefValueGiven(ph7_vm *pVm,ph7_class *pOwnerClass,ph7_vm_func *pCallee,sxu32 nArgPos,SyString *pArgName);` |
|         - | 6761 | `PH7_PRIVATE sxi32 VmThrowByRefRefusal(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,ph7_vm_func *pCallee,sxu32 nArgPos,SyString *pArgName);` |
|         - | 6762 | `PH7_PRIVATE sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,ph7_class *pOwnerClass,ph7_vm_func *pCallee,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven);` |
|         - | 6763 | `PH7_PRIVATE sxi32 VmVariadicElementTypeCheck(ph7_vm *pVm,ph7_class *pSelfHint,ph7_vm_func *pCallee,ph7_vm_func_arg *pFormal,ph7_value *pVal,sxu32 nArgPos,int bCallIsStrict);` |
|         - | 6764 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex);` |
|         - | 6765 | `PH7_PRIVATE sxi32 VmMemPoolInit(VmMemPool *pPool,SyMemBackend *pAllocator);` |
|         - | 6766 | `PH7_PRIVATE ph7_value * VmMemPoolReserve(VmMemPool *pPool,sxu32 *pIndex);` |
|         - | 6767 | `PH7_PRIVATE sxi32 VmMemPoolTruncate(VmMemPool *pPool,sxu32 nNewSize);` |
|         - | 6768 | `PH7_PRIVATE void VmMemPoolFreeSlot(VmMemPool *pPool,sxu32 nIdx);` |
|         - | 6769 | `/* Argument-unpacking key capture (PHP 8.1 named-parameter semantics for spreads).` |
|         - | 6770 | `` * `pMap->aNames` is COMPILE-TIME metadata indexed by compile-time argument`` |
|         - | 6771 | ` * position, but a runtime spread expands its slot to a variable element count,` |
|         - | 6772 | ` * so any spread that expands to !=1 element shifts the following actual stack` |
|         - | 6773 | ` * positions out of alignment with aNames — and the element keys (which PHP 8.1` |
|         - | 6774 | ` * treats as named arguments) are otherwise discarded. OP_SPREAD records one` |
|         - | 6775 | ` * VmSpreadRun per expansion plus one VmSpreadKey per element (in order) on the` |
|         - | 6776 | ` * VM; CALL/NEW replay them (VmBuildEffectiveArgMap) into an effective map with` |
|         - | 6777 | ` * one name entry per ACTUAL slot, then let the existing named-argument resolver` |
|         - | 6778 | ` * run unchanged. The same runs give each call its own argument-count growth` |
|         - | 6779 | ` * (VmSpreadOwnExtra). This call's runs are consumed (truncated) at the CALL. */` |
|         - | 6780 | `typedef struct VmSpreadRun VmSpreadRun;` |
|         - | 6781 | `struct VmSpreadRun {` |
|         - | 6782 | `	ph7_value *pStart;   /* First stack slot the expansion wrote (the source slot) */` |
|         - | 6783 | `	sxu32 nCount;        /* Elements produced (0 for an empty array) */` |
|         - | 6784 | `	sxu32 nKeyStart;     /* aSpreadKey index of this run's first element key */` |
|         - | 6785 | `	sxu32 nBlobStart;    /* sSpreadKeyBlob length before this run's keys were appended */` |
|         - | 6786 | `};` |
|         - | 6787 | `typedef struct VmSpreadKey VmSpreadKey;` |
|         - | 6788 | `struct VmSpreadKey {` |
|         - | 6789 | `	sxu32 nOff;          /* Byte offset into pVm->sSpreadKeyBlob (valid iff nLen>0) */` |
|         - | 6790 | `	sxu32 nLen;          /* Key length; 0 == integer key == positional element */` |
|         - | 6791 | `};` |
|         - | 6792 | `#define VM_STACK_UNMODELED SXU32_HIGH /* shared by vm.c (stack modeling) and vm_exec.c */` |
|         - | 6793 | `/* vm_ops_*.c — opcode handlers extracted from VmByteCodeExecBody. The loop` |
|         - | 6794 | ` * syncs pTos/pc into its VmExecState, calls the handler, reloads them and` |
|         - | 6795 | ` * maps the returned code onto its labels. */` |
|         - | 6796 | `typedef enum VmOpRc {` |
|         - | 6797 | `	VM_OP_NEXT = 0,   /* arm done: fall to the loop's trailing pc++ */` |
|         - | 6798 | `	VM_OP_ABORT,      /* -> the loop's Abort label */` |
|         - | 6799 | `	VM_OP_EXCEPTION   /* -> the loop's Exception label */` |
|         - | 6800 | `} VmOpRc;` |
|         - | 6801 | `PH7_PRIVATE int VmRecordedResume(ph7_vm *pVm,sxi32 *pResumePc,VmFrame *pEntryFrame,VmInstr *aInstr);` |
|         - | 6802 | `/*` |
|         - | 6803 | ` * Does the pending ROOT C inline redirect belong to the activation running` |
|         - | 6804 | ` * (aInstrArg, pEntryArg)? Both halves are needed: the bytecode array alone is` |
|         - | 6805 | ` * shared by every live activation of one function (see pInlineFrame). A redirect` |
|         - | 6806 | ` * whose owning frame was invalidated (VmDrainFinally retires a handler by zeroing` |
|         - | 6807 | ` * it) names no activation, so it falls back to the bytecode array alone — losing` |
|         - | 6808 | ` * the catch entirely would be worse than landing it one activation over.` |
|         - | 6809 | ` */` |
|         - | 6810 | `#define VmInlineOwnedBy(pVm,aInstrArg,pEntryArg) \` |
|         - | 6811 | `	((pVm)->pInlineInstr == (void *)(aInstrArg) \` |
|         - | 6812 | `	 && ((pVm)->pInlineFrame == 0 \|\| (pVm)->pInlineFrame == (void *)(pEntryArg)))` |
|         - | 6813 | `PH7_PRIVATE void VmPopOperand(ph7_value **ppTos, sxi32 nPop);` |
|         - | 6814 | `PH7_PRIVATE void VmForeachStepUnlink(ph7_foreach_info *pInfo,ph7_foreach_step *pStep);` |
|         - | 6815 | `PH7_PRIVATE int VmHookGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName);` |
|         - | 6816 | `PH7_PRIVATE void VmForeachHashmapStepRelease(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,int bPop);` |
|         - | 6817 | `PH7_PRIVATE void VmForeachStepRelease(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep);` |
|         - | 6818 | `PH7_PRIVATE void VmForeachStepAbandon(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,ph7_class_instance *pThis);` |
|         - | 6819 | `PH7_PRIVATE int VmClassAllowsDynamicProps(ph7_vm *pVm,ph7_class *pClass);` |
|         - | 6820 | `PH7_PRIVATE int VmClassHasAttributeNamed(ph7_class *pClass,const char *zName,sxu32 nName);` |
|         - | 6821 | `/* __PHP_Incomplete_Class: unserialize()'s carrier object (vm.c helpers).` |
|         - | 6822 | ` * PH7_INCOMPLETE_MAGIC_MEMBER is php's MAGIC_MEMBER — the dynamic property that` |
|         - | 6823 | ` * remembers the original class name; the serializer strips it back out. */` |
|         - | 6824 | `#define PH7_INCOMPLETE_MAGIC_MEMBER "__PHP_Incomplete_Class_Name"` |
|         - | 6825 | `PH7_PRIVATE int PH7_VmIsIncompleteClass(ph7_vm *pVm,ph7_class *pClass);` |
|         - | 6826 | `PH7_PRIVATE void PH7_VmIncompleteMsg(ph7_vm *pVm,ph7_class_instance *pThis,const char *zWhat,SyBlob *pOut);` |
|         - | 6827 | `PH7_PRIVATE void PH7_VmIncompleteAccessWarn(ph7_vm *pVm,ph7_class_instance *pThis,SyString *pFuncName);` |
|         - | 6828 | `PH7_PRIVATE void VmRecreateDeclaredAttr(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_attr *pAttr,VmClassAttr **ppAttr);` |
|         - | 6829 | `PH7_PRIVATE int VmMemberCtxIsLookup(sxi32 iP2);` |
|         - | 6830 | `PH7_PRIVATE int VmMemberCtxWantsValue(sxi32 iP2);` |
|         - | 6831 | `PH7_PRIVATE int VmMemberNextIsWrite(const VmInstr *pNext);` |
|         - | 6832 | `PH7_PRIVATE int VmMemberNextIsRmw(const VmInstr *pNext);` |
|         - | 6833 | `PH7_PRIVATE int VmNextIsCompoundAssign(const VmInstr *pNext);` |
|         - | 6834 | `PH7_PRIVATE sxi32 VmSpreadOwnExtra(ph7_vm *pVm, sxi32 iP1, ph7_value *pTos);` |
|         - | 6835 | `PH7_PRIVATE VmCallArgMap *VmEffCallArgMap(ph7_vm *pVm, VmInstr *pInstr, ph7_value *pArg, sxu32 nActual, VmCallArgMap *pStorage);` |
|         - | 6836 | `PH7_PRIVATE int VmMagicGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind);` |
|         - | 6837 | `PH7_PRIVATE void VmMagicGuardPush(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind);` |
|         - | 6838 | `PH7_PRIVATE void VmMagicGuardPop(ph7_vm *pVm);` |
|         - | 6839 | `PH7_PRIVATE void VmPinMemObjSlot(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 6840 | `PH7_PRIVATE void VmPinMemObjSlotCounted(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 6841 | `PH7_PRIVATE void VmUnpinMemObjSlot(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 6842 | `PH7_PRIVATE sxi32 VmSpreadMergeStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData);` |
|         - | 6843 | `PH7_PRIVATE int VmValueIsTraversable(ph7_vm *pVm, ph7_value *pVal);` |
|         - | 6844 | `PH7_PRIVATE sxi32 VmHashmapRefInsert(ph7_hashmap *pMap, const char *zKey, sxu32 nByte, sxu32 nRefIdx);` |
|         - | 6845 | `PH7_PRIVATE void VmWarnCannotUseAsArray(ph7_vm *pVm,sxi32 iFlags);` |
|         - | 6846 | `PH7_PRIVATE sxi32 VmHookRmwConsume(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 6847 | `PH7_PRIVATE void VmHookRmwFreeScratch(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 6848 | `PH7_PRIVATE sxi32 VmHookSetDispatch(ph7_vm *pVm,ph7_class_instance *pHThis,ph7_class_attr *pHAttr,sxu32 nBackIdx,ph7_value *pValue);` |
|         - | 6849 | `PH7_PRIVATE void VmMagicSetDispatch(ph7_vm *pVm,ph7_class_instance *pSetThis,const SyString *pName,ph7_value *pValue);` |
|         - | 6850 | `PH7_PRIVATE ph7_exception * VmExcActivate(ph7_vm *pVm,ph7_exception *pCompiled);` |
|         - | 6851 | `PH7_PRIVATE ph7_exception * VmExcLive(ph7_vm *pVm,ph7_exception *pCompiled);` |
|         - | 6852 | `PH7_PRIVATE sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight);` |
|         - | 6853 | `PH7_PRIVATE sxu32 VmComputeMaxStack(ph7_vm *pVm, VmInstr *aInstr, sxu32 nInstr);` |
|         - | 6854 | `PH7_PRIVATE sxi32 VmDrainFinally(ph7_vm *pVm, sxu32 nExceptionBase);` |
|         - | 6855 | `PH7_PRIVATE sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName);` |
|         - | 6856 | `PH7_PRIVATE void VmHookRmwDropTop(ph7_vm *pVm);` |
|         - | 6857 | `PH7_PRIVATE sxi32 VmInitCallContext(ph7_context *pOut, ph7_vm *pVm, ph7_user_func *pFunc, ph7_value *pRet, sxi32 iFlags);` |
|         - | 6858 | `PH7_PRIVATE void VmMaterializeCatchReturn(ph7_vm *pVm, ph7_value *pResult, VmFrame *pEntryFrame);` |
|         - | 6859 | `PH7_PRIVATE ph7_value * VmOperandStackAlloc(ph7_vm *pVm, sxu32 nSlots);` |
|         - | 6860 | `PH7_PRIVATE void VmOperandStackRecycle(ph7_vm *pVm, ph7_value *pStack, sxu32 nCap, sxu32 nLive);` |
|         - | 6861 | `PH7_PRIVATE ph7_vm_func * VmOverload(ph7_vm *pVm, ph7_vm_func *pList, ph7_value *aArg, int nArg);` |
|         - | 6862 | `PH7_PRIVATE void VmReleaseCallContext(ph7_context *pCtx);` |
|         - | 6863 | `PH7_PRIVATE sxi32 VmResolveNamedArgs(ph7_vm *pVm, VmCallArgMap *pMap, ph7_vm_func_arg *aFormalArg, sxu32 nNonVariadic, sxi32 iVariadicIdx, sxu32 nActual, sxi32 *aSlot, sxu8 *aUsed);` |
|         - | 6864 | `PH7_PRIVATE void VmSpreadExpandMap(ph7_vm *pVm, ph7_value **ppTos, ph7_hashmap *pMap, int bVarSource);` |
|         - | 6865 | `PH7_PRIVATE sxi32 VmSpreadValuesStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData);` |
|         - | 6866 | `PH7_PRIVATE int VmStringWantsPerlIncr(ph7_value *pVal);` |
|         - | 6867 | `PH7_PRIVATE sxi32 VmSuspendCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, sxi32 pc, sxi32 nTos);` |
|         - | 6868 | `PH7_PRIVATE int VmExcMatches(ph7_exception *pExc,ph7_exception *pCompiled);` |
|         - | 6869 | `PH7_PRIVATE void VmSpreadConsume(ph7_vm *pVm);` |
|         - | 6870 | `PH7_PRIVATE VmOpRc VmExecOpSwitch(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 6871 | `PH7_PRIVATE VmOpRc VmExecOpCatch(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 6872 | `PH7_PRIVATE VmOpRc VmExecOpLoadException(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 6873 | `PH7_PRIVATE VmOpRc VmExecOpSpaceship(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 6874 | `PH7_PRIVATE VmOpRc VmExecOpGe(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 6875 | `PH7_PRIVATE VmOpRc VmExecOpLe(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 6876 | `PH7_PRIVATE VmOpRc VmExecOpTne(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 6877 | `PH7_PRIVATE VmOpRc VmExecOpTeq(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 6878 | `PH7_PRIVATE VmOpRc VmExecOpNeq(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 6879 | `PH7_PRIVATE VmOpRc VmExecOpLor(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 6880 | `PH7_PRIVATE VmOpRc VmExecOpShrStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 6881 | `PH7_PRIVATE VmOpRc VmExecOpShr(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 6882 | `PH7_PRIVATE VmOpRc VmExecOpMulStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 6883 | `PH7_PRIVATE VmOpRc VmExecOpLoadList(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 6884 | `PH7_PRIVATE VmOpRc VmExecOpUnsetVar(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 6885 | `PH7_PRIVATE VmOpRc VmExecOpConsume(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 6886 | `PH7_PRIVATE VmOpRc VmExecOpMatch(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 6887 | `PH7_PRIVATE VmOpRc VmExecOpClone(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 6888 | `PH7_PRIVATE VmOpRc VmExecOpThrow(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 6889 | `PH7_PRIVATE VmOpRc VmExecOpNullcStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 6890 | `PH7_PRIVATE VmOpRc VmExecOpDiv(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 6891 | `PH7_PRIVATE VmOpRc VmExecOpModStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 6892 | `PH7_PRIVATE VmOpRc VmExecOpMod(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 6893 | `PH7_PRIVATE VmOpRc VmExecOpSubStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 6894 | `PH7_PRIVATE VmOpRc VmExecOpSub(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 6895 | `PH7_PRIVATE VmOpRc VmExecOpPowStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 6896 | `PH7_PRIVATE void PH7_MemObjPow(ph7_value *pBase,ph7_value *pExp,ph7_value *pOut);` |
|         - | 6897 | `PH7_PRIVATE VmOpRc VmExecOpLoadMap(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 6898 | `PH7_PRIVATE VmOpRc VmExecOpLoadIdx(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 6899 | `PH7_PRIVATE VmOpRc VmExecOpLoadClosure(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 6900 | `PH7_PRIVATE VmOpRc VmExecOpStoreIdxRef(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 6901 | `PH7_PRIVATE VmOpRc VmExecOpStoreRef(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 6902 | `PH7_PRIVATE VmOpRc VmExecOpMember(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 6903 | `PH7_PRIVATE VmOpRc VmExecOpNew(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 6904 | `PH7_PRIVATE VmOpRc VmExecOpForeachInit(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 6905 | `PH7_PRIVATE VmOpRc VmExecOpForeachStep(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);` |
|         - | 6906 | `/* vm_error.c — error/diagnostics/type-enforcement machinery shared with vm.c */` |
|         - | 6907 | `PH7_PRIVATE sxi32 PH7_VmErrPhpBit(sxi32 iErr);` |
|         - | 6908 | `PH7_PRIVATE void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|         - | 6909 | `PH7_PRIVATE void PH7_VmActiveFuncName(ph7_vm *pVm,SyBlob *pOut);` |
|         - | 6910 | `PH7_PRIVATE sxi32 VmEnterFrame(ph7_vm *pVm,void *pUserData,ph7_class_instance *pThis,VmFrame **ppFrame);` |
|         - | 6911 | `PH7_PRIVATE void VmExcRelease(ph7_vm *pVm,ph7_exception *pExc);` |
|         - | 6912 | `PH7_PRIVATE void VmClearFramePending(VmFrame *pFrame);` |
|         - | 6913 | `PH7_PRIVATE sxi32 VmDrainCrossedTrys(ph7_vm *pVm,sxu32 nCross,sxu32 nFloor);` |
|         - | 6914 | `PH7_PRIVATE sxi32 VmLocalExecIntoObj(ph7_vm *pVm,SySet *pByteCode,ph7_value **ppMemObj,int bReturnPropagates);` |
|         - | 6915 |  |
|         - | 6916 | `PH7_PRIVATE sxi32 VmArithOperandCheck(ph7_vm *pVm,ph7_value *pLeft,ph7_value *pRight,const char *zOp,SyBlob *pMsgOut);` |
|         - | 6917 | `PH7_PRIVATE const char * VmArithTypeName(ph7_value *pVal);` |
|         - | 6918 | `PH7_PRIVATE const char * VmArithValueName(ph7_value *pVal);` |
|         - | 6919 | `PH7_PRIVATE void VmIncDecTypeErrorMsg(ph7_value *pVal,int bIncr,SyBlob *pMsgOut);` |
|         - | 6920 | `PH7_PRIVATE sxi32 VmUnaryArithOperandCheck(ph7_vm *pVm,ph7_value *pVal,SyBlob *pMsgOut);` |
|         - | 6921 | `PH7_PRIVATE void VmBitNotTypeErrorMsg(ph7_value *pVal,SyBlob *pMsgOut);` |
|         - | 6922 | `PH7_PRIVATE void VmStringBitwise(ph7_value *pLeft,ph7_value *pRight,int cOp,SyBlob *pOut);` |
|         - | 6923 | `PH7_PRIVATE sxi32 VmCheckReadonlyMutate(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 6924 | `PH7_PRIVATE sxi32 VmCheckSetVisibility(ph7_vm *pVm,ph7_class *pOwner,ph7_class_attr *pAttr);` |
|         - | 6925 | `PH7_PRIVATE sxi32 VmThrowNativeNoWrite(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr);` |
|         - | 6926 | `PH7_PRIVATE sxi32 VmThrowNativeNoUnset(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr);` |
|         - | 6927 | `PH7_PRIVATE sxi32 VmThrowNativeReadOnly(ph7_vm *pVm,ph7_class_attr *pAttr);` |
|         - | 6928 | `PH7_PRIVATE void PH7_NativeMarkAttrReadOnly(ph7_class_instance *pThis,const char *zProp);` |
|         - | 6929 | `PH7_PRIVATE sxi32 VmCheckReadonlyUnset(ph7_vm *pVm,ph7_class *pClass,VmClassAttr *pVmAttr);` |
|         - | 6930 | `PH7_PRIVATE sxi32 PH7_VmCheckIndirectModify(ph7_vm *pVm,sxu32 nIdx);` |
|         - | 6931 | `PH7_PRIVATE sxi32 VmCloneApplyUpdate(ph7_vm *pVm,ph7_class_instance *pClone, const char *zName,sxu32 nName,ph7_value *pValue);` |
|         - | 6932 | `PH7_PRIVATE void VmCoalesceDisarm(ph7_vm *pVm);` |
|         - | 6933 | `PH7_PRIVATE sxi32 VmConstCycleThrow(ph7_vm *pVm);` |
|         - | 6934 | `PH7_PRIVATE ph7_class * VmCurrentSelf(ph7_vm *pVm);` |
|         - | 6935 | `PH7_PRIVATE sxi32 VmEnforceConstantType(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue,int bLazy);` |
|         - | 6936 | `PH7_PRIVATE sxi32 VmEnforceTypedDefault(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue);` |
|         - | 6937 | `PH7_PRIVATE sxi32 VmCheckTypedDefault(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue);` |
|         - | 6938 | `PH7_PRIVATE sxi32 VmEnforceArgType(ph7_vm *pVm,ph7_vm_func *pFunc,ph7_vm_func_arg *pFormal,` |
|         - | 6939 | `	sxu32 nArgPos,ph7_value *pVal,int bStrict,ph7_class *pSelfHint);` |
|         - | 6940 | `PH7_PRIVATE int VmClassStaticDeferPending(ph7_class *pClass);` |
|         - | 6941 | `PH7_PRIVATE sxi32 PH7_VmMaterializeClassStatics(ph7_vm *pVm,ph7_class *pClass);` |
|         - | 6942 | `PH7_PRIVATE sxi32 VmEnforceReturnType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue);` |
|         - | 6943 | `PH7_PRIVATE sxi32 VmEnumMaterializeCase(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pCase);` |
|         - | 6944 | `PH7_PRIVATE int VmFinallyAdvance(ph7_vm *pVm, VmInstr *aInstr, int *pnCross, sxu32 *pPc);` |
|         - | 6945 | `PH7_PRIVATE int VmRecursionExceeded(ph7_vm *pVm);` |
|         - | 6946 | `PH7_PRIVATE sxi32 VmRecursionFatal(ph7_vm *pVm);` |
|         - | 6947 | `PH7_PRIVATE int VmValueIsLossyToInt(ph7_value *pVal);` |
|         - | 6948 | `PH7_PRIVATE sxi32 VmRejectFloatOperand(ph7_vm *pVm,ph7_value *pVal);` |
|         - | 6949 | `PH7_PRIVATE sxi32 VmThrowArgNotPassed(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName, ph7_vm_func *pCallee,sxu32 nArg,SyString *pArgName);` |
|         - | 6950 | `PH7_PRIVATE sxi32 VmThrowBuiltinError(ph7_vm *pVm,const char *zClass,sxu32 nClass,SyBlob *pMsg);` |
|         - | 6951 | `PH7_PRIVATE sxi32 VmThrowBuiltinErrorCode(ph7_vm *pVm,const char *zClass,sxu32 nClass,` |
|         - | 6952 | `	SyBlob *pMsg,sxi32 iCode);` |
|         - | 6953 | `PH7_PRIVATE sxi32 VmThrowFixedError(ph7_vm *pVm, const char *zClass, const char *zMsg);` |
|         - | 6954 | `PH7_PRIVATE sxi32 VmThrowFixedErrorCode(ph7_vm *pVm,const char *zClass,sxi32 iCode,` |
|         - | 6955 | `	const char *zMsg);` |
|         - | 6956 | `PH7_PRIVATE sxi32 VmThrowFromVm(ph7_vm *pVm, const char *zClass, const char *zMsg, sxu32 nMsg);` |
|         - | 6957 | `PH7_PRIVATE sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg);` |
|         - | 6958 | `PH7_PRIVATE sxi32 VmThrowSpreadError(ph7_vm *pVm,ph7_value *pBad,int bArgUnpack);` |
|         - | 6959 | `PH7_PRIVATE sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr);` |
|         - | 6960 | `PH7_PRIVATE sxi32 VmThrowAutoInitArrayError(ph7_vm *pVm,VmClassAttr *pVmAttr);` |
|         - | 6961 | `PH7_PRIVATE sxi32 VmAutoInitArrayProperty(ph7_vm *pVm,VmClassAttr *pVmAttr,ph7_value *pSlot);` |
|         - | 6962 | `PH7_PRIVATE sxi32 VmRefUninitTypedProperty(ph7_vm *pVm,ph7_class *pClass,VmClassAttr *pVmAttr,ph7_value *pSlot);` |
|         - | 6963 | `PH7_PRIVATE sxi32 VmUncaughtException(ph7_vm *pVm, ph7_class_instance *pThis);` |
|         - | 6964 | `/* vm_arg_check.c — builtin arity/signature enforcement, called from vm.c */` |
|         - | 6965 | `PH7_PRIVATE void VmSetBuiltinArity(ph7_vm *pVm);` |
|         - | 6966 | `PH7_PRIVATE void VmSetBuiltinSignatures(ph7_vm *pVm);` |
|         - | 6967 | `/* Signature-string derivations, shared with the native-class builder: one` |
|         - | 6968 | ` * PHP-style parameter list ("string $s, int $o = 0") is the single source of a` |
|         - | 6969 | ` * callee's arity bounds and by-ref positions, for a builtin and a native method` |
|         - | 6970 | ` * alike — which is how a native method gets the too-few/too-many ArgumentCountError` |
|         - | 6971 | ` * that a prelude-declared method never had. */` |
|         - | 6972 | `PH7_PRIVATE void VmDeriveArityFromSig(const char *zSig,sxi16 *pnMin,sxu8 *pbAtLeast,sxi16 *pnMax,sxu8 *pbHasMax);` |
|         - | 6973 | `PH7_PRIVATE sxu32 VmDeriveByRefMaskFromSig(const char *zSig);` |
|         - | 6974 | `PH7_PRIVATE int VmBuiltinPrefersRef(SyString *pName);` |
|         - | 6975 | `PH7_PRIVATE sxi32 PH7_VmBindNamedArgsToSig(ph7_context *pCtx,ph7_user_func *pFunc,VmCallArgMap *pMap,int *pnArg,ph7_value **apArg);` |
|         - | 6976 | `PH7_PRIVATE sxi32 VmEnforceBuiltinArgTypes(ph7_context *pCtx,ph7_user_func *pFunc,int nGiven,ph7_value **apArg);` |
|         - | 6977 | `PH7_PRIVATE sxi32 PH7_VmScreenByRefArgShapes(ph7_context *pCtx,ph7_user_func *pFunc,VmCallArgMap *pMap,int nGiven,ph7_value **apArg);` |
|         - | 6978 | `PH7_PRIVATE int PH7_VmSigParamName(const char *zSig,int nPos,SyString *pOut);` |
|         - | 6979 | `PH7_PRIVATE int PH7_VmArgRefusedByRef(VmCallArgMap *pMap,sxu32 nPos,ph7_value *pVal);` |
|         - | 6980 | `PH7_PRIVATE sxi32 PH7_VmResolveDeferredArgs(ph7_vm *pVm,ph7_value *pArg,ph7_value *pTos,` |
|         - | 6981 | `	ph7_vm_func_arg *pFormal,sxu32 nFormal,sxu32 nByRefMask,int bAllByRef,int bAllByValue,` |
|         - | 6982 | `	VmCallArgMap *pCallMap);` |
|         - | 6983 | `PH7_PRIVATE void PH7_VmArgTempCallNotice(ph7_vm *pVm,VmCallArgMap *pMap,sxu32 nPos,ph7_value *pVal);` |
|         - | 6984 | `PH7_PRIVATE int PH7_ArgSatisfiesString(ph7_value *pArg);` |
|         - | 6985 | `PH7_PRIVATE void VmDeprecatedAttrNotice(ph7_vm *pVm,ph7_vm_func *pFunc,ph7_class *pDeclClass);` |
|         - | 6986 | `PH7_PRIVATE void PH7_MarkDeprecatedFunctions(ph7_vm *pVm);` |
|         - | 6987 | `PH7_PRIVATE void PH7_VmDeprecatedCallNotice(ph7_vm *pVm,const ph7_deprecated_name *pDep);` |
|         - | 6988 | `/* vm_exec_ctx.c — Fiber/Generator/Closure engine shared with vm.c */` |
|         - | 6989 | `PH7_PRIVATE sxi32 PH7_VmInstallClosureNative(ph7_vm *pVm);` |
|         - | 6990 | `PH7_PRIVATE sxi32 PH7_VmInstallFiberNative(ph7_vm *pVm);` |
|         - | 6991 | `PH7_PRIVATE sxi32 PH7_VmInstallGeneratorNative(ph7_vm *pVm);` |
|         - | 6992 | `PH7_PRIVATE int vm_builtin_Closure_bindTo(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 6993 | `PH7_PRIVATE int vm_builtin_Closure_call(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 6994 | `PH7_PRIVATE int vm_builtin_Closure_construct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 6995 | `PH7_PRIVATE int vm_builtin_Closure_fromCallable(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 6996 | `PH7_PRIVATE int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 6997 | `PH7_PRIVATE int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 6998 | `PH7_PRIVATE int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 6999 | `PH7_PRIVATE int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7000 | `PH7_PRIVATE int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7001 | `PH7_PRIVATE int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7002 | `PH7_PRIVATE int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7003 | `PH7_PRIVATE int vm_builtin_Fiber_getCurrent(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7004 | `PH7_PRIVATE int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7005 | `PH7_PRIVATE int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7006 | `PH7_PRIVATE int vm_builtin_Fiber_throw(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7007 | `PH7_PRIVATE int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7008 | `PH7_PRIVATE int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7009 | `PH7_PRIVATE int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7010 | `PH7_PRIVATE int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7011 | `PH7_PRIVATE int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7012 | `PH7_PRIVATE int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7013 | `PH7_PRIVATE int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7014 | `PH7_PRIVATE int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7015 | `PH7_PRIVATE int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7016 | `PH7_PRIVATE int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|         - | 7017 | `PH7_PRIVATE ph7_class_instance * VmCreateClosure(ph7_vm *pVm, const SyString *pName, ph7_class_instance *pBoundThis, const SyString *pScope);` |
|         - | 7018 | `PH7_PRIVATE ph7_class * VmFccResolveScope(ph7_vm *pVm, ph7_value *pTarget);` |
|         - | 7019 | `PH7_PRIVATE ph7_class * PH7_VmResolveScopeName(ph7_vm *pVm, const char *zCls, sxu32 nCls);` |
|         - | 7020 | `PH7_PRIVATE int PH7_VmIsScopeKeyword(const char *zName,sxu32 nName);` |
|         - | 7021 | `PH7_PRIVATE ph7_class_instance * VmFccWrapValue(ph7_vm *pVm, ph7_value *pValue);` |
|         - | 7022 | `PH7_PRIVATE void PH7_VmByRefArgWriteBack(ph7_vm *pVm,ph7_value *pArg,sxi32 iPreFlags);` |
|         - | 7023 | `PH7_PRIVATE sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx, ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg, int bStrict, ph7_class *pSelfHint, int bCallSiteInMsg, int bAliasByRef);` |
|         - | 7024 | `PH7_PRIVATE ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj);` |
|         - | 7025 | `PH7_PRIVATE int PH7_VmGeneratorIsClosed(ph7_vm *pVm, ph7_class_instance *pThis);` |
|         - | 7026 | `PH7_PRIVATE sxi32 PH7_VmGeneratorPrime(ph7_vm *pVm, ph7_class_instance *pThis);` |
|         - | 7027 | `PH7_PRIVATE ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc);` |
|         - | 7028 | `PH7_PRIVATE ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|         - | 7029 | `PH7_PRIVATE void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|         - | 7030 | `PH7_PRIVATE void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen);` |
|         - | 7031 | `PH7_PRIVATE sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult);` |
|         - | 7032 | `/* vm_include.c function prototypes (rows stay in vm.c's aVmFunc[]) */` |
|         - | 7033 | `PH7_PRIVATE sxi32 VmMountUserClass(ph7_vm *pVm,ph7_class *pClass);` |
|         - | 7034 | `PH7_PRIVATE sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);` |
|         - | 7035 | `PH7_PRIVATE SyString * PH7_VmExecutingUnitFile(ph7_vm *pVm);` |
|         - | 7036 | `PH7_PRIVATE void PH7_VmIncFramePush(ph7_vm *pVm,const char *zName,SyString *pPath);` |
|         - | 7037 | `PH7_PRIVATE void PH7_VmIncFramePop(ph7_vm *pVm);` |
|         - | 7038 | `PH7_PRIVATE sxi32 VmExecDeferredClass(ph7_vm *pVm,VmDeferredClass *pDefer,VmDeferredReq **ppMissing);` |
|         - | 7039 | `PH7_PRIVATE ph7_user_func * PH7_VmMagicCallFunc(ph7_vm *pVm);` |
|         - | 7040 | `PH7_PRIVATE int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7041 | `PH7_PRIVATE int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7042 | `PH7_PRIVATE int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7043 | `PH7_PRIVATE int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7044 | `PH7_PRIVATE int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7045 | `PH7_PRIVATE int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7046 | `PH7_PRIVATE int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7047 | `PH7_PRIVATE int vm_builtin_set_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7048 | `PH7_PRIVATE int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7049 | `PH7_PRIVATE int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7050 | `PH7_PRIVATE int vm_builtin_spl_autoload_extensions(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7051 | `PH7_PRIVATE int vm_builtin_spl_autoload_call(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7052 | `PH7_PRIVATE int vm_builtin_spl_classes(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7053 | `PH7_PRIVATE int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7054 | `PH7_PRIVATE int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7055 | `/* vm_builtin_call.c — callable machinery shared with vm.c's interpreter */` |
|         - | 7056 | `PH7_PRIVATE ph7_class * PH7_VmResolveParentClass(ph7_vm *pVm);` |
|         - | 7057 | `PH7_PRIVATE void VmBoundaryPark(ph7_vm *pVm,sxi32 rc);` |
|         - | 7058 | `/*` |
|         - | 7059 | ` * The status a C->PHP dispatch answers when the callee did NOT return: the` |
|         - | 7060 | ` * builtin driving the loop must abandon it and hand the status straight out.` |
|         - | 7061 | ` * Both members matter and testing only the first is a silent wrong answer —` |
|         - | 7062 | ` * php stops an internal function the moment its callback throws, and an` |
|         - | 7063 | ` * UNCAUGHT throw comes back as PH7_ABORT (VmUncaughtException reports the` |
|         - | 7064 | ` * fatal and answers SXERR_ABORT), not as PH7_EXCEPTION. A loop that tested` |
|         - | 7065 | ` * only PH7_EXCEPTION therefore ran the callback again for every remaining` |
|         - | 7066 | ` * element — repeating its side effects and re-reporting the fatal once per` |
|         - | 7067 | ` * element. Same set VmBoundaryPark parks; see its comment.` |
|         - | 7068 | ` */` |
|         - | 7069 | `#define PH7_CALLBACK_UNWOUND(rc) ((rc) == PH7_EXCEPTION \|\| (rc) == PH7_ABORT)` |
|         - | 7070 | `PH7_PRIVATE sxi32 PH7_VmCallCallbackByValue(ph7_vm *pVm,ph7_value *pFunc,int nArg,` |
|         - | 7071 | `	ph7_value **apArg,ph7_value *pResult,sxu32 nRefOkMask);` |
|         - | 7072 | `PH7_PRIVATE sxi32 VmIterCallMethod(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nLen,ph7_value *pResult);` |
|         - | 7073 | `PH7_PRIVATE sxi32 VmCallClassMethodLsb(ph7_vm *pVm,ph7_class *pCalled,ph7_class_instance *pThis,` |
|         - | 7074 | `	ph7_class_method *pMethod,ph7_value *pResult,int nArg,ph7_value **apArg,VmCallArgMap *pMap);` |
|         - | 7075 | `PH7_PRIVATE sxi32 VmCallClassMethodWithMap(ph7_vm *pVm,ph7_class_instance *pThis,` |
|         - | 7076 | `	ph7_class_method *pMethod,ph7_value *pResult,int nArg,` |
|         - | 7077 | `	ph7_value **apArg,VmCallArgMap *pMap);` |
|         - | 7078 | `PH7_PRIVATE sxi32 VmCallObjectInvoke(ph7_vm *pVm,ph7_class_instance *pThis,` |
|         - | 7079 | `	int nArg,ph7_value **apArg,ph7_value *pResult,VmCallArgMap *pMap);` |
|         - | 7080 | `PH7_PRIVATE sxi32 VmRaiseNotCallable(ph7_vm *pVm,ph7_class_instance *pThis);` |
|         - | 7081 | `PH7_PRIVATE int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7082 | `PH7_PRIVATE int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7083 | `PH7_PRIVATE int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7084 | `PH7_PRIVATE int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7085 | `PH7_PRIVATE int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7086 | `PH7_PRIVATE int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7087 | `PH7_PRIVATE int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7088 | `PH7_PRIVATE int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7089 | `/* builtin.c function prototypes */` |
|         - | 7090 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm);` |
|         - | 7091 | `/* builtin_hash.c function prototypes */` |
|         - | 7092 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 7093 | `/* Binary-to-hex consumer shared by bin2hex() (builtin.c) and the hash` |
|         - | 7094 | ` * builtins (builtin_hash.c). */` |
|         - | 7095 | `PH7_PRIVATE int HashConsumer(const void *pData,unsigned int nLen,void *pUserData);` |
|         - | 7096 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|         - | 7097 | `PH7_PRIVATE int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7098 | `PH7_PRIVATE int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7099 | `PH7_PRIVATE int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7100 | `PH7_PRIVATE int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7101 | `PH7_PRIVATE int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7102 | `PH7_PRIVATE int PH7_builtin_hash_hmac_algos(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7103 | `PH7_PRIVATE int PH7_builtin_hash_init(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7104 | `PH7_PRIVATE int PH7_builtin_hash_update(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7105 | `PH7_PRIVATE int PH7_builtin_hash_final(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7106 | `PH7_PRIVATE int PH7_builtin_hash_copy(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7107 | `PH7_PRIVATE int PH7_builtin_hash_pbkdf2(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7108 | `PH7_PRIVATE int PH7_builtin_hash_hkdf(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7109 | `PH7_PRIVATE int PH7_builtin_hash_file(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7110 | `PH7_PRIVATE int PH7_builtin_hash_hmac_file(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7111 | `PH7_PRIVATE int PH7_builtin_hash_update_file(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7112 | `PH7_PRIVATE int PH7_builtin_hash_update_stream(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7113 | `PH7_PRIVATE sxi32 PH7_VmInstallHashContext(ph7_vm *pVm);` |
|         - | 7114 | `/* php's one hash_init() flag: request an HMAC rather than a plain digest. */` |
|         - | 7115 | `#define PH7_HASH_HMAC 1` |
|         - | 7116 | `PH7_PRIVATE int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7117 | `PH7_PRIVATE int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7118 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|         - | 7119 | `PH7_PRIVATE int PH7_builtin_crypt(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7120 | `PH7_PRIVATE int PH7_builtin_password_algos(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7121 | `PH7_PRIVATE int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7122 | `PH7_PRIVATE int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7123 | `PH7_PRIVATE int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7124 | `PH7_PRIVATE int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7125 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 7126 | `/* builtin_fmt.c function prototypes (PH7_NEED_FMT_AND_INI: compiled whenever` |
|         - | 7127 | ` * disk I/O is enabled, independently of PH7_DISABLE_BUILTIN_FUNC) */` |
|         - | 7128 | `#ifndef PH7_DISABLE_DISK_IO` |
|         - | 7129 | `PH7_PRIVATE int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7130 | `PH7_PRIVATE int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7131 | `PH7_PRIVATE int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7132 | `PH7_PRIVATE int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7133 | `#endif /* PH7_DISABLE_DISK_IO */` |
|         - | 7134 | `#include <signal.h>   /* sig_atomic_t: PH7_PcntlAsyncPending, read at the fetch point */` |
|         - | 7135 | `/*` |
|         - | 7136 | ` * ext/pcntl (builtin_pcntl.c). These four are OUTSIDE every guard because the` |
|         - | 7137 | ` * engine links against them in every build: vm_exec.c reads the flag at its` |
|         - | 7138 | ` * fetch point, vm.c registers the constants and releases the state. Where the` |
|         - | 7139 | ` * extension is not compiled in -- Windows, or a build with no builtins -- the` |
|         - | 7140 | ` * flag is simply always zero and the three functions are no-ops.` |
|         - | 7141 | ` */` |
|         - | 7142 | `extern volatile sig_atomic_t PH7_PcntlAsyncPending;` |
|         - | 7143 | `PH7_PRIVATE void PH7_PcntlDrainAsync(ph7_vm *pVm);` |
|         - | 7144 | `PH7_PRIVATE void PH7_PcntlVmRelease(ph7_vm *pVm);` |
|         - | 7145 | `PH7_PRIVATE void PH7_RegisterPcntlConstants(ph7_vm *pVm);` |
|         - | 7146 | `PH7_PRIVATE sxi32 PH7_VmInstallPcntl(ph7_vm *pVm);` |
|         - | 7147 | ``/* Its uncatchable `Error installing signal handler for %d`, which lives with the`` |
|         - | 7148 | ` * engine's other clean-halt fatals in vm_error.c rather than with the extension. */` |
|         - | 7149 | `PH7_PRIVATE sxi32 PH7_VmSignalInstallFatal(ph7_vm *pVm,int signo);` |
|         - | 7150 | `#if defined(PH7_ENABLE_THREADS)` |
|         - | 7151 | `/* Drop this process to single-threaded mode in the CHILD of a fork() (api.c).` |
|         - | 7152 | ` * Declared beside pcntl's names because pcntl_fork() is its only caller, but it` |
|         - | 7153 | ` * belongs to the library core and is built wherever threads are. */` |
|         - | 7154 | `PH7_PRIVATE void PH7_LibForkChild(void);` |
|         - | 7155 | `#endif` |
|         - | 7156 | `/* php's syslog trio (builtin_syslog.c). Not an extension -- ext/standard, and` |
|         - | 7157 | ` * therefore present on every platform php is. */` |
|         - | 7158 | `PH7_PRIVATE void PH7_RegisterSyslogConstants(ph7_vm *pVm);` |
|         - | 7159 | `PH7_PRIVATE void PH7_SyslogVmRelease(ph7_vm *pVm);` |
|         - | 7160 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 7161 | `PH7_PRIVATE int PH7_builtin_openlog(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7162 | `PH7_PRIVATE int PH7_builtin_syslog(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7163 | `PH7_PRIVATE int PH7_builtin_closelog(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7164 | `#endif` |
|         - | 7165 | `/* builtin_parse.c function prototypes */` |
|         - | 7166 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 7167 | `/* HTML entity escape engine: shared by the htmlspecialchars/htmlentities` |
|         - | 7168 | ` * family (builtin.c) and filter_var's SANITIZE filters (builtin_parse.c). */` |
|         - | 7169 | `/* The charsets the HTML entity family models: php's own UTF-8 and ISO-8859-1` |
|         - | 7170 | ` * (one byte per character, its VALUE the code point). Everything else keeps` |
|         - | 7171 | ` * php's unsupported-charset warning. */` |
|         - | 7172 | `#define PH7_HTML_CS_UTF8   0` |
|         - | 7173 | `#define PH7_HTML_CS_LATIN1 1` |
|         - | 7174 | `PH7_PRIVATE void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,int iFlags,int bAll,int bDoubleEncode,int iCs);` |
|         - | 7175 | `PH7_PRIVATE void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,int iFlags,int bFull,int iCs);` |
|         - | 7176 | `PH7_PRIVATE int HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx);` |
|         - | 7177 | `PH7_PRIVATE void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags,int iCs);` |
|         - | 7178 | `PH7_PRIVATE int PH7_builtin_filter_var(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7179 | `PH7_PRIVATE int PH7_builtin_filter_input(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7180 | `PH7_PRIVATE int PH7_builtin_filter_list(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7181 | `PH7_PRIVATE int PH7_builtin_filter_id(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7182 | `PH7_PRIVATE int PH7_builtin_filter_has_var(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7183 | `PH7_PRIVATE int PH7_builtin_filter_var_array(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7184 | `PH7_PRIVATE int PH7_builtin_filter_input_array(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7185 | `PH7_PRIVATE int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7186 | `PH7_PRIVATE int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7187 | `PH7_PRIVATE int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7188 | `PH7_PRIVATE int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7189 | `PH7_PRIVATE int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7190 | `PH7_PRIVATE int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7191 | `PH7_PRIVATE int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7192 | `PH7_PRIVATE int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7193 | `PH7_PRIVATE int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7194 | `PH7_PRIVATE int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7195 | `PH7_PRIVATE int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7196 | `PH7_PRIVATE int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7197 | `PH7_PRIVATE int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7198 | `PH7_PRIVATE int PH7_builtin_convert_uuencode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7199 | `PH7_PRIVATE int PH7_builtin_convert_uudecode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7200 | `PH7_PRIVATE int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7201 | `PH7_PRIVATE int PH7_builtin_rawurlencode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7202 | `PH7_PRIVATE int PH7_builtin_http_build_query(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7203 | `PH7_PRIVATE int PH7_builtin_parse_str(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7204 | `PH7_PRIVATE int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7205 | `PH7_PRIVATE int PH7_builtin_rawurldecode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7206 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 7207 | `#ifndef PH7_DISABLE_DISK_IO` |
|         - | 7208 | `PH7_PRIVATE int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7209 | `PH7_PRIVATE int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7210 | `PH7_PRIVATE int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7211 | `#endif /* PH7_DISABLE_DISK_IO */` |
|         - | 7212 | `/* builtin_string.c function prototypes */` |
|         - | 7213 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 7214 | `PH7_PRIVATE int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7215 | `PH7_PRIVATE int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7216 | `PH7_PRIVATE int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7217 | `PH7_PRIVATE int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7218 | `PH7_PRIVATE int PH7_builtin_chunk_split(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7219 | `PH7_PRIVATE int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7220 | `PH7_PRIVATE int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7221 | `PH7_PRIVATE int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7222 | `PH7_PRIVATE int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7223 | `PH7_PRIVATE int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7224 | `PH7_PRIVATE int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7225 | `PH7_PRIVATE int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7226 | `PH7_PRIVATE int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7227 | `PH7_PRIVATE int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7228 | `PH7_PRIVATE int PH7_builtin_levenshtein(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7229 | `PH7_PRIVATE int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7230 | `PH7_PRIVATE int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7231 | `PH7_PRIVATE int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7232 | `PH7_PRIVATE int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7233 | `PH7_PRIVATE int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7234 | `PH7_PRIVATE int PH7_builtin_similar_text(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7235 | `PH7_PRIVATE int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7236 | `PH7_PRIVATE int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7237 | `PH7_PRIVATE int PH7_builtin_str_rot13(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7238 | `PH7_PRIVATE int PH7_builtin_metaphone(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7239 | `/* builtin_pack.c — the binary-string pair */` |
|         - | 7240 | `PH7_PRIVATE int PH7_builtin_pack(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7241 | `PH7_PRIVATE int PH7_builtin_unpack(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7242 | `/* builtin_bcmath.c -- ext/bcmath: arbitrary-precision decimal arithmetic */` |
|         - | 7243 | `PH7_PRIVATE int PH7_builtin_bcadd(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7244 | `PH7_PRIVATE int PH7_builtin_bcsub(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7245 | `PH7_PRIVATE int PH7_builtin_bcmul(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7246 | `PH7_PRIVATE int PH7_builtin_bccomp(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7247 | `PH7_PRIVATE int PH7_builtin_bcdiv(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7248 | `PH7_PRIVATE int PH7_builtin_bcmod(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7249 | `PH7_PRIVATE int PH7_builtin_bcdivmod(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7250 | `PH7_PRIVATE int PH7_builtin_bcpow(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7251 | `PH7_PRIVATE int PH7_builtin_bcpowmod(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7252 | `PH7_PRIVATE int PH7_builtin_bcsqrt(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7253 | `PH7_PRIVATE int PH7_builtin_bcround(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7254 | `PH7_PRIVATE int PH7_builtin_bcfloor(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7255 | `PH7_PRIVATE int PH7_builtin_bcceil(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7256 | `PH7_PRIVATE sxi32 PH7_VmInstallBcMath(ph7_vm *pVm);` |
|         - | 7257 | `PH7_PRIVATE int PH7_builtin_bcscale(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7258 | `/* builtin_calendar.c -- ext/calendar: the serial day number and its calendars */` |
|         - | 7259 | `PH7_PRIVATE int PH7_builtin_gregoriantojd(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7260 | `PH7_PRIVATE int PH7_builtin_jdtogregorian(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7261 | `PH7_PRIVATE int PH7_builtin_juliantojd(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7262 | `PH7_PRIVATE int PH7_builtin_jdtojulian(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7263 | `PH7_PRIVATE int PH7_builtin_frenchtojd(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7264 | `PH7_PRIVATE int PH7_builtin_jdtofrench(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7265 | `PH7_PRIVATE int PH7_builtin_jewishtojd(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7266 | `PH7_PRIVATE int PH7_builtin_jdtojewish(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7267 | `PH7_PRIVATE int PH7_builtin_cal_info(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7268 | `/* posix (builtin_posix.c) -- php builds no ext/posix on Windows, and neither` |
|         - | 7269 | ` * does this, so every name below is absent there. */` |
|         - | 7270 | `#ifndef __WINNT__` |
|         - | 7271 | `/* ext/pcntl's own builtins (builtin_pcntl.c); the names the engine links against` |
|         - | 7272 | ` * in EVERY build are declared above, outside both guards. */` |
|         - | 7273 | `PH7_PRIVATE int PH7_builtin_pcntl_fork(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7274 | `PH7_PRIVATE int PH7_builtin_pcntl_waitpid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7275 | `PH7_PRIVATE int PH7_builtin_pcntl_waitid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7276 | `PH7_PRIVATE int PH7_builtin_pcntl_wait(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7277 | `PH7_PRIVATE int PH7_builtin_pcntl_signal(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7278 | `PH7_PRIVATE int PH7_builtin_pcntl_signal_get_handler(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7279 | `PH7_PRIVATE int PH7_builtin_pcntl_signal_dispatch(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7280 | `PH7_PRIVATE int PH7_builtin_pcntl_sigprocmask(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7281 | `PH7_PRIVATE int PH7_builtin_pcntl_sigwaitinfo(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7282 | `PH7_PRIVATE int PH7_builtin_pcntl_sigtimedwait(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7283 | `PH7_PRIVATE int PH7_builtin_pcntl_wifexited(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7284 | `PH7_PRIVATE int PH7_builtin_pcntl_wifstopped(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7285 | `PH7_PRIVATE int PH7_builtin_pcntl_wifcontinued(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7286 | `PH7_PRIVATE int PH7_builtin_pcntl_wifsignaled(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7287 | `PH7_PRIVATE int PH7_builtin_pcntl_wexitstatus(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7288 | `PH7_PRIVATE int PH7_builtin_pcntl_wtermsig(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7289 | `PH7_PRIVATE int PH7_builtin_pcntl_wstopsig(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7290 | `PH7_PRIVATE int PH7_builtin_pcntl_exec(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7291 | `PH7_PRIVATE int PH7_builtin_pcntl_alarm(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7292 | `PH7_PRIVATE int PH7_builtin_pcntl_get_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7293 | `PH7_PRIVATE int PH7_builtin_pcntl_getpriority(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7294 | `PH7_PRIVATE int PH7_builtin_pcntl_setpriority(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7295 | `PH7_PRIVATE int PH7_builtin_pcntl_strerror(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7296 | `PH7_PRIVATE int PH7_builtin_pcntl_async_signals(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7297 | `#ifdef __linux__` |
|         - | 7298 | `PH7_PRIVATE int PH7_builtin_pcntl_unshare(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7299 | `PH7_PRIVATE int PH7_builtin_pcntl_getcpuaffinity(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7300 | `PH7_PRIVATE int PH7_builtin_pcntl_setcpuaffinity(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7301 | `PH7_PRIVATE int PH7_builtin_pcntl_getcpu(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7302 | `#endif` |
|         - | 7303 | `PH7_PRIVATE int PH7_builtin_posix_kill(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7304 | `PH7_PRIVATE int PH7_builtin_posix_getpid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7305 | `PH7_PRIVATE int PH7_builtin_posix_getppid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7306 | `PH7_PRIVATE int PH7_builtin_posix_getuid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7307 | `PH7_PRIVATE int PH7_builtin_posix_setuid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7308 | `PH7_PRIVATE int PH7_builtin_posix_geteuid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7309 | `PH7_PRIVATE int PH7_builtin_posix_seteuid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7310 | `PH7_PRIVATE int PH7_builtin_posix_getgid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7311 | `PH7_PRIVATE int PH7_builtin_posix_setgid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7312 | `PH7_PRIVATE int PH7_builtin_posix_getegid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7313 | `PH7_PRIVATE int PH7_builtin_posix_setegid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7314 | `PH7_PRIVATE int PH7_builtin_posix_getgroups(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7315 | `PH7_PRIVATE int PH7_builtin_posix_getlogin(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7316 | `PH7_PRIVATE int PH7_builtin_posix_getpgrp(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7317 | `PH7_PRIVATE int PH7_builtin_posix_setsid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7318 | `PH7_PRIVATE int PH7_builtin_posix_setpgid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7319 | `PH7_PRIVATE int PH7_builtin_posix_getpgid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7320 | `PH7_PRIVATE int PH7_builtin_posix_getsid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7321 | `PH7_PRIVATE int PH7_builtin_posix_uname(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7322 | `PH7_PRIVATE int PH7_builtin_posix_times(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7323 | `PH7_PRIVATE int PH7_builtin_posix_ctermid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7324 | `PH7_PRIVATE int PH7_builtin_posix_ttyname(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7325 | `PH7_PRIVATE int PH7_builtin_posix_isatty(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7326 | `PH7_PRIVATE int PH7_builtin_posix_getcwd(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7327 | `PH7_PRIVATE int PH7_builtin_posix_mkfifo(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7328 | `PH7_PRIVATE int PH7_builtin_posix_mknod(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7329 | `PH7_PRIVATE int PH7_builtin_posix_access(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7330 | `PH7_PRIVATE int PH7_builtin_posix_eaccess(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7331 | `PH7_PRIVATE int PH7_builtin_posix_getgrnam(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7332 | `PH7_PRIVATE int PH7_builtin_posix_getgrgid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7333 | `PH7_PRIVATE int PH7_builtin_posix_getpwnam(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7334 | `PH7_PRIVATE int PH7_builtin_posix_getpwuid(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7335 | `PH7_PRIVATE int PH7_builtin_posix_getrlimit(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7336 | `PH7_PRIVATE int PH7_builtin_posix_setrlimit(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7337 | `PH7_PRIVATE int PH7_builtin_posix_get_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7338 | `PH7_PRIVATE int PH7_builtin_posix_strerror(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7339 | `PH7_PRIVATE int PH7_builtin_posix_initgroups(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7340 | `PH7_PRIVATE int PH7_builtin_posix_sysconf(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7341 | `PH7_PRIVATE int PH7_builtin_posix_pathconf(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7342 | `PH7_PRIVATE int PH7_builtin_posix_fpathconf(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7343 | `#endif /* __WINNT__ */` |
|         - | 7344 | `/* fileinfo (builtin_fileinfo.c) -- php's ext/fileinfo, over PHL's own signature` |
|         - | 7345 | ` * table rather than a magic database file. */` |
|         - | 7346 | `#ifdef PH7_ENABLE_ZLIB` |
|         - | 7347 | `/* vm_openssl.c / vm_openssl_x509.c */` |
|         - | 7348 | `PH7_PRIVATE sxi32 PH7_VmInstallOpenSsl(ph7_vm *pVm);` |
|         - | 7349 | `PH7_PRIVATE const ph7_builtin_func * PH7_OpenSslFuncTable(sxu32 *pnEntry);` |
|         - | 7350 | `PH7_PRIVATE sxi32 PH7_VmInstallOpenSslX509(ph7_vm *pVm);` |
|         - | 7351 | `PH7_PRIVATE const ph7_builtin_func * PH7_OpenSslX509FuncTable(sxu32 *pnEntry);` |
|         - | 7352 | `PH7_PRIVATE void PH7_SslVmReset(ph7_vm *pVm);` |
|         - | 7353 | `PH7_PRIVATE void PH7_SslVmRelease(ph7_vm *pVm);` |
|         - | 7354 | `/* ext/zip: the ZipArchive class, its ten deprecated procedural verbs and the` |
|         - | 7355 | `` * read-only `zip://` wrapper. It rides ext/zlib's build guard because php's own`` |
|         - | 7356 | ` * requires zlib -- a deflated member is the format's normal case. */` |
|         - | 7357 | `#ifdef PH7_ENABLE_ZLIB` |
|         - | 7358 | `PH7_PRIVATE sxi32 PH7_VmInstallZip(ph7_vm *pVm);` |
|         - | 7359 | `PH7_PRIVATE const ph7_builtin_func * PH7_ZipFuncTable(sxu32 *pnEntry);` |
|         - | 7360 | `PH7_PRIVATE int PH7_ZipStreamIs(const ph7_io_stream *pStream);` |
|         - | 7361 | `PH7_PRIVATE int PH7_ZipStreamViaWrapper(void *pHandle);` |
|         - | 7362 | `#endif` |
|         - | 7363 | `PH7_PRIVATE void PH7_ZipVmReset(ph7_vm *pVm);` |
|         - | 7364 | `PH7_PRIVATE void PH7_ZipVmRelease(ph7_vm *pVm);` |
|         - | 7365 | `/* The name get_resource_type() gives one of ext/zip's two procedural handles,` |
|         - | 7366 | ` * or 0 when the resource is not one of them. */` |
|         - | 7367 | `PH7_PRIVATE const char * PH7_ZipResourceType(void *pResource);` |
|         - | 7368 | `PH7_PRIVATE sxi32 PH7_VmInstallZlib(ph7_vm *pVm);` |
|         - | 7369 | `PH7_PRIVATE const ph7_builtin_func * PH7_ZlibFuncTable(sxu32 *pnEntry);` |
|         - | 7370 | `PH7_PRIVATE void PH7_ZlibVmReset(ph7_vm *pVm);` |
|         - | 7371 | `PH7_PRIVATE void PH7_ZlibVmRelease(ph7_vm *pVm);` |
|         - | 7372 | `PH7_PRIVATE void PH7_ZlibArmOpen(ph7_vm *pVm,int iLevel,int iStrategy);` |
|         - | 7373 | `PH7_PRIVATE void PH7_ZlibArmDirect(ph7_vm *pVm,int bDirect);` |
|         - | 7374 | `PH7_PRIVATE int PH7_ZlibStreamIs(const ph7_io_stream *pStream);` |
|         - | 7375 | `PH7_PRIVATE int PH7_ZlibFilterCreate(phl_stream_filter *pFilter,ph7_value *pParams);` |
|         - | 7376 | `PH7_PRIVATE int PH7_ZlibFilterRun(phl_stream_filter *pFilter,phl_brigade *pIn,` |
|         - | 7377 | `	phl_brigade *pOut,int iFlags);` |
|         - | 7378 | `PH7_PRIVATE void PH7_ZlibFilterClose(phl_stream_filter *pFilter);` |
|         - | 7379 | `extern const ph7_io_stream sZLIB_Stream;` |
|         - | 7380 | `extern const ph7_io_stream sZIP_Stream;` |
|         - | 7381 | `#endif` |
|         - | 7382 | `PH7_PRIVATE void PH7_VmAddResponseHeader(ph7_vm *pVm,const char *zName,const char *zValue);` |
|         - | 7383 | `PH7_PRIVATE sxi32 PH7_VmInstallPhar(ph7_vm *pVm);` |
|         - | 7384 | `PH7_PRIVATE int PH7_PharStreamIs(const ph7_io_stream *pStream);` |
|         - | 7385 | `PH7_PRIVATE int PH7_PharUrlStat(ph7_vm *pVm,const char *zPath,ph7_int64 *aVal);` |
|         - | 7386 | `PH7_PRIVATE int PH7_PharCanonicalUrl(ph7_vm *pVm,const char *zPath,int nPath,SyBlob *pOut);` |
|         - | 7387 | `/* What PH7_PharPathOp() was asked to do. Mirrors vfs.c's VFS_POP_* codes, which` |
|         - | 7388 | ` * are file-local. */` |
|         - | 7389 | `#define PHAR_PATHOP_UNLINK 0` |
|         - | 7390 | `#define PHAR_PATHOP_RENAME 1` |
|         - | 7391 | `#define PHAR_PATHOP_MKDIR  2` |
|         - | 7392 | `#define PHAR_PATHOP_CHMOD  3` |
|         - | 7393 | `#define PHAR_PATHOP_RMDIR  4` |
|         - | 7394 | `PH7_PRIVATE int PH7_PharPathOp(ph7_context *pCtx,const char *zPath,const char *zDest,int eOp);` |
|         - | 7395 | `PH7_PRIVATE int PH7_VmSerializeValue(ph7_context *pCtx,ph7_value *pIn,SyBlob *pOut);` |
|         - | 7396 | `extern const ph7_io_stream sPHAR_Stream;` |
|         - | 7397 | `PH7_PRIVATE sxi32 PH7_VmInstallFileinfo(ph7_vm *pVm);` |
|         - | 7398 | `PH7_PRIVATE int PH7_builtin_finfo_open(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7399 | `PH7_PRIVATE int PH7_builtin_finfo_close(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7400 | `PH7_PRIVATE int PH7_builtin_finfo_set_flags(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7401 | `PH7_PRIVATE int PH7_builtin_finfo_file(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7402 | `PH7_PRIVATE int PH7_builtin_finfo_buffer(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7403 | `PH7_PRIVATE int PH7_builtin_mime_content_type(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7404 | `/* gettext (builtin_gettext.c) */` |
|         - | 7405 | `PH7_PRIVATE int PH7_builtin_textdomain(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7406 | `PH7_PRIVATE int PH7_builtin_bindtextdomain(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7407 | `PH7_PRIVATE int PH7_builtin_bind_textdomain_codeset(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7408 | `PH7_PRIVATE int PH7_builtin_gettext(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7409 | `PH7_PRIVATE int PH7_builtin_dgettext(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7410 | `PH7_PRIVATE int PH7_builtin_dcgettext(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7411 | `PH7_PRIVATE int PH7_builtin_ngettext(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7412 | `PH7_PRIVATE int PH7_builtin_dngettext(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7413 | `PH7_PRIVATE int PH7_builtin_dcngettext(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7414 | `PH7_PRIVATE int PH7_builtin_cal_days_in_month(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7415 | `PH7_PRIVATE int PH7_builtin_cal_to_jd(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7416 | `PH7_PRIVATE int PH7_builtin_cal_from_jd(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7417 | `PH7_PRIVATE int PH7_builtin_jddayofweek(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7418 | `PH7_PRIVATE int PH7_builtin_jdmonthname(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7419 | `PH7_PRIVATE int PH7_builtin_unixtojd(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7420 | `PH7_PRIVATE int PH7_builtin_jdtounix(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7421 | `PH7_PRIVATE int PH7_builtin_easter_days(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7422 | `PH7_PRIVATE int PH7_builtin_easter_date(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7423 | `/* builtin_image.c -- ext/standard's image surface: the IMAGETYPE_* space and` |
|         - | 7424 | ` * the container readers behind getimagesize(). */` |
|         - | 7425 | `PH7_PRIVATE const char * PH7_ImageTypeMime(ph7_int64 iType);` |
|         - | 7426 | `PH7_PRIVATE int PH7_builtin_image_type_to_mime_type(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7427 | `PH7_PRIVATE int PH7_builtin_image_type_to_extension(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7428 | `PH7_PRIVATE int PH7_builtin_getimagesize(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7429 | `PH7_PRIVATE int PH7_builtin_getimagesizefromstring(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7430 | `/* builtin_scanf.c -- sscanf() and the scanner fscanf() (vfs_stream.c) shares. */` |
|         - | 7431 | `PH7_PRIVATE sxi32 PH7_ScanfRun(ph7_context *pCtx,const char *zStr,int nStr,` |
|         - | 7432 | `	const char *zFmt,int nFmt,ph7_value **apVar,int nVar);` |
|         - | 7433 | `PH7_PRIVATE int PH7_builtin_sscanf(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7434 | `PH7_PRIVATE int PH7_builtin_fscanf(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7435 | `PH7_PRIVATE int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7436 | `PH7_PRIVATE int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7437 | `PH7_PRIVATE int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7438 | `PH7_PRIVATE int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7439 | `PH7_PRIVATE int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7440 | `PH7_PRIVATE int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7441 | `PH7_PRIVATE int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7442 | `PH7_PRIVATE int PH7_builtin_stripcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7443 | `PH7_PRIVATE int PH7_builtin_quoted_printable_encode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7444 | `PH7_PRIVATE int PH7_builtin_quoted_printable_decode(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7445 | `PH7_PRIVATE int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7446 | `PH7_PRIVATE int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7447 | `PH7_PRIVATE int PH7_builtin_strnatcmp(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7448 | `PH7_PRIVATE int PH7_builtin_version_compare(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7449 | `PH7_PRIVATE int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7450 | `PH7_PRIVATE int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7451 | `PH7_PRIVATE int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7452 | `PH7_PRIVATE int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7453 | `PH7_PRIVATE int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7454 | `PH7_PRIVATE int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7455 | `PH7_PRIVATE int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7456 | `PH7_PRIVATE int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7457 | `PH7_PRIVATE int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7458 | `PH7_PRIVATE int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7459 | `PH7_PRIVATE int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7460 | `PH7_PRIVATE int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7461 | `PH7_PRIVATE int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7462 | `PH7_PRIVATE int PH7_builtin_count_chars(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7463 | `PH7_PRIVATE int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7464 | `PH7_PRIVATE int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7465 | `PH7_PRIVATE int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7466 | `PH7_PRIVATE int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7467 | `PH7_PRIVATE int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7468 | `PH7_PRIVATE int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7469 | `PH7_PRIVATE int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7470 | `PH7_PRIVATE int PH7_builtin_str_word_count(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7471 | `PH7_PRIVATE int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7472 | `PH7_PRIVATE int PH7_builtin_substr_compare(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7473 | `PH7_PRIVATE int PH7_builtin_substr_count(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7474 | `PH7_PRIVATE int PH7_builtin_substr_replace(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7475 | `PH7_PRIVATE int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7476 | `PH7_PRIVATE int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7477 | `PH7_PRIVATE int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7478 | `PH7_PRIVATE int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7479 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 7480 | `/* hashmap.c function prototypes */` |
|         - | 7481 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(ph7_vm *pVm,sxu32 (*xIntHash)(sxi64),sxu32 (*xBlobHash)(const void *,sxu32));` |
|         - | 7482 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm);` |
|         - | 7483 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS);` |
|         - | 7484 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap);` |
|         - | 7485 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(ph7_hashmap *pMap,ph7_value *pKey,ph7_hashmap_node **ppNode);` |
|         - | 7486 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(ph7_hashmap *pMap,ph7_value *pKey,ph7_value *pVal);` |
|         - | 7487 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(ph7_hashmap *pMap,ph7_value *pKey,sxu32 nRefIdx);` |
|         - | 7488 | `PH7_PRIVATE sxi32 PH7_HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest);` |
|         - | 7489 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight);` |
|         - | 7490 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore);` |
|         - | 7491 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest);` |
|         - | 7492 | `PH7_PRIVATE sxi32 PH7_HashmapDupMaterialized(ph7_hashmap *pSrc,ph7_hashmap *pDest);` |
|         - | 7493 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue);` |
|         - | 7494 | `PH7_PRIVATE sxi32 PH7_HashmapCmp(ph7_hashmap *pLeft,ph7_hashmap *pRight,int bStrict);` |
|         - | 7495 | `PH7_PRIVATE void PH7_HashmapRegisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep);` |
|         - | 7496 | `PH7_PRIVATE void PH7_HashmapUnregisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep);` |
|         - | 7497 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap);` |
|         - | 7498 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore);` |
|         - | 7499 | `PH7_PRIVATE int PH7_HashmapNodeIsRef(ph7_hashmap_node *pNode);` |
|         - | 7500 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey);` |
|         - | 7501 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm);` |
|         - | 7502 | `/* hashmap.c engine helpers shared with hashmap_sort.c / hashmap_builtin.c */` |
|         - | 7503 | `PH7_PRIVATE ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode);` |
|         - | 7504 | `PH7_PRIVATE sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict);` |
|         - | 7505 | `PH7_PRIVATE void HashmapRehashIntNode(ph7_hashmap_node *pEntry);` |
|         - | 7506 | `PH7_PRIVATE sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected);` |
|         - | 7507 | `PH7_PRIVATE sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve);` |
|         - | 7508 | `PH7_PRIVATE int HashmapFindValue(ph7_hashmap *pMap,ph7_value *pNeedle,ph7_hashmap_node **ppNode,int bStrict);` |
|         - | 7509 | `PH7_PRIVATE int HashmapValueStrEq(ph7_value *pA,ph7_value *pB,int bUserVisible,sxi32 *pRc);` |
|         - | 7510 | `PH7_PRIVATE sxi32 HashmapStringifyElems(ph7_hashmap *pMap);` |
|         - | 7511 | `PH7_PRIVATE int HashmapFindStringValue(ph7_hashmap *pMap,ph7_value *pNeedle,ph7_hashmap_node **ppNode,sxi32 *pRc);` |
|         - | 7512 | `PH7_PRIVATE sxi32 HashmapOverwrite(ph7_hashmap *pSrc,ph7_hashmap *pDest);` |
|         - | 7513 | `PH7_PRIVATE sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest);` |
|         - | 7514 | `PH7_PRIVATE sxi32 HashmapInsert(ph7_hashmap *pMap,ph7_value *pKey,ph7_value *pVal);` |
|         - | 7515 | `PH7_PRIVATE sxi32 HashmapLookupIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_hashmap_node **ppNode);` |
|         - | 7516 | `PH7_PRIVATE sxi32 HashmapLookupBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_hashmap_node **ppNode);` |
|         - | 7517 | `PH7_PRIVATE sxi32 PH7_HashmapInsertRawKey(ph7_hashmap *pMap,const char *zKey,sxu32 nKey,ph7_value *pVal);` |
|         - | 7518 | `/* hashmap_sort.c: the SQLite-derived merge sort and the sort builtin family.` |
|         - | 7519 | ` * Shared with hashmap.c (shuffle/array_unique/array_rand) and referenced from` |
|         - | 7520 | ` * the aHashmapFunc[] registration table; compiled in every mode. */` |
|         - | 7521 | `typedef sxi32 (*ProcNodeCmp)(ph7_hashmap_node *,ph7_hashmap_node *,void *);` |
|         - | 7522 | `PH7_PRIVATE sxi32 PH7_HashmapShuffle(ph7_hashmap *pMap);` |
|         - | 7523 | `PH7_PRIVATE sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData);` |
|         - | 7524 | `PH7_PRIVATE void HashmapSortRehash(ph7_hashmap *pMap);` |
|         - | 7525 | `PH7_PRIVATE int HashmapValueFlagEqual(ph7_vm *pVm,ph7_value *pA,ph7_value *pB,int base,int bFold);` |
|         - | 7526 | `PH7_PRIVATE int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7527 | `PH7_PRIVATE int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7528 | `PH7_PRIVATE int ph7_hashmap_natsort(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7529 | `PH7_PRIVATE int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7530 | `PH7_PRIVATE int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7531 | `PH7_PRIVATE int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7532 | `PH7_PRIVATE int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7533 | `PH7_PRIVATE int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7534 | `PH7_PRIVATE int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7535 | `PH7_PRIVATE int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7536 | `/* hashmap_builtin.c function prototypes (the array_* family; referenced from` |
|         - | 7537 | ` * the aHashmapFunc[] registration table in hashmap.c; compiled in every mode) */` |
|         - | 7538 | `PH7_PRIVATE int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7539 | `PH7_PRIVATE int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7540 | `PH7_PRIVATE int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7541 | `PH7_PRIVATE int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7542 | `PH7_PRIVATE int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7543 | `PH7_PRIVATE int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7544 | `PH7_PRIVATE int ph7_hashmap_unshift(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7545 | `PH7_PRIVATE int ph7_hashmap_merge_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7546 | `PH7_PRIVATE int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7547 | `PH7_PRIVATE int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7548 | `PH7_PRIVATE int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7549 | `PH7_PRIVATE int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7550 | `PH7_PRIVATE int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7551 | `PH7_PRIVATE int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7552 | `PH7_PRIVATE int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7553 | `PH7_PRIVATE int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7554 | `PH7_PRIVATE int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7555 | `PH7_PRIVATE int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7556 | `PH7_PRIVATE int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7557 | `PH7_PRIVATE int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7558 | `PH7_PRIVATE int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7559 | `PH7_PRIVATE int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7560 | `PH7_PRIVATE int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7561 | `PH7_PRIVATE int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7562 | `PH7_PRIVATE int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7563 | `PH7_PRIVATE int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7564 | `PH7_PRIVATE int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7565 | `PH7_PRIVATE int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7566 | `PH7_PRIVATE int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7567 | `PH7_PRIVATE int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7568 | `PH7_PRIVATE int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7569 | `PH7_PRIVATE int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7570 | `PH7_PRIVATE int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7571 | `PH7_PRIVATE int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7572 | `PH7_PRIVATE int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7573 | `PH7_PRIVATE int ph7_hashmap_diff_ukey(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7574 | `PH7_PRIVATE int ph7_hashmap_intersect_ukey(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7575 | `PH7_PRIVATE int ph7_hashmap_udiff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7576 | `PH7_PRIVATE int ph7_hashmap_uintersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7577 | `PH7_PRIVATE int ph7_hashmap_udiff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7578 | `PH7_PRIVATE int ph7_hashmap_uintersect_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7579 | `PH7_PRIVATE int ph7_hashmap_intersect_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7580 | `PH7_PRIVATE int ph7_hashmap_multisort(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7581 | `PH7_PRIVATE int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7582 | `PH7_PRIVATE int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7583 | `PH7_PRIVATE int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7584 | `PH7_PRIVATE int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7585 | `PH7_PRIVATE int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7586 | `PH7_PRIVATE int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7587 | `PH7_PRIVATE int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7588 | `PH7_PRIVATE int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7589 | `PH7_PRIVATE int ph7_hashmap_max(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7590 | `PH7_PRIVATE int ph7_hashmap_min(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7591 | `PH7_PRIVATE int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7592 | `PH7_PRIVATE int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7593 | `PH7_PRIVATE int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7594 | `PH7_PRIVATE int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7595 | `PH7_PRIVATE int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7596 | `PH7_PRIVATE int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7597 | `PH7_PRIVATE int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7598 | `PH7_PRIVATE int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7599 | `PH7_PRIVATE int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7600 | `PH7_PRIVATE int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7601 | `PH7_PRIVATE int ph7_hashmap_first(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7602 | `PH7_PRIVATE int ph7_hashmap_last(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7603 | `PH7_PRIVATE int ph7_hashmap_key_first(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7604 | `PH7_PRIVATE int ph7_hashmap_key_last(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7605 | `PH7_PRIVATE int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7606 | `PH7_PRIVATE int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7607 | `PH7_PRIVATE int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7608 | `PH7_PRIVATE int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7609 | `PH7_PRIVATE int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7610 | `PH7_PRIVATE int ph7_iterator_to_array(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7611 | `PH7_PRIVATE int ph7_iterator_count(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7612 | `PH7_PRIVATE int ph7_iterator_apply(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7613 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth);` |
|         - | 7614 | `PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth,int bProp);` |
|         - | 7615 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(ph7_hashmap *pMap,int (*xWalk)(ph7_value *,ph7_value *,void *),void *pUserData);` |
|         - | 7616 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap);` |
|         - | 7617 | `/* php's key fold, shared with the diagnostics that must print a key the way the` |
|         - | 7618 | ` * LOOKUP saw it. Leaves a non-integer key as a printable string value. */` |
|         - | 7619 | `PH7_PRIVATE int PH7_HashmapKeyIsInt(ph7_value *pKey);` |
|         - | 7620 | `/* php value-name helper (true/false/class-name/null); used by the range()/` |
|         - | 7621 | ` * array_rand() domain-error messages in hashmap.c, which are compiled in every` |
|         - | 7622 | ` * mode, so it must stay outside the PH7_DISABLE_DISK_IO guard. */` |
|         - | 7623 | `PH7_PRIVATE const char *VmValueGivenName(ph7_value *pVal,char *zBuf,sxu32 nBuf);` |
|         - | 7624 | `PH7_PRIVATE void PH7_ReflectInterfacesOf(ph7_vm *pVm,ph7_class *pClass,SySet *pOut);` |
|         - | 7625 | `/* Outcomes of php's STRING-container offset rules (VmStringOffsetResolve). */` |
|         - | 7626 | `#define VM_STROFF_OK      0  /* *piOfft holds php's offset */` |
|         - | 7627 | `#define VM_STROFF_REJECT  1  /* php's TypeError; pMsg carries its message */` |
|         - | 7628 | `#define VM_STROFF_MISS    2  /* lenient context: answer "not set", say nothing */` |
|         - | 7629 | `/* ...and its DIAGNOSTIC LEVEL, of which php has three, not two:` |
|         - | 7630 | ` *   VM_STROFF_LOUD      a real read or write: every warning, and an offset TYPE` |
|         - | 7631 | ` *                       php refuses is the TypeError.` |
|         - | 7632 | `` *   VM_STROFF_COALESCE  a `??` / `??=` fetch: the NOT-SET diagnostics are`` |
|         - | 7633 | `` *                       suppressed (no `Uninitialized string offset`, and a`` |
|         - | 7634 | ` *                       refused offset TYPE answers "not set"), and so is the` |
|         - | 7635 | ` *                       null/bool/float CAST notice — but the offset SHAPE` |
|         - | 7636 | ` *                       warning still fires and the offset is still read:` |
|         - | 7637 | ``  *                       `$s["1x"] ?? "d"` warns `Illegal string offset "1x"` `` |
|         - | 7638 | `` *                       and answers `$s[1]`.`` |
|         - | 7639 | ` *   VM_STROFF_ISSET     isset()/empty()/unset(): fully quiet, every shape.` |
|         - | 7640 | ` *   VM_STROFF_UNSETBASE an INTERMEDIATE subscript of an unset chain` |
|         - | 7641 | `` *                       (`unset($s[k][0])`, `unset($s[k]->p)`): php reads the`` |
|         - | 7642 | ` *                       offset to hand it on, so the CAST notice fires as in a` |
|         - | 7643 | ` *                       real write, but the int-then-garbage warning does not` |
|         - | 7644 | `` *                       (`unset($s["1x"][0])` says nothing about "1x") and an`` |
|         - | 7645 | ` *                       offset TYPE php refuses is not the read's TypeError —` |
|         - | 7646 | `` *                       it is the unset's own `Cannot unset string offsets`,`` |
|         - | 7647 | ` *                       which the caller raises on REJECT at this level.` |
|         - | 7648 | ` */` |
|         - | 7649 | `#define VM_STROFF_LOUD      0` |
|         - | 7650 | `#define VM_STROFF_COALESCE  1` |
|         - | 7651 | `#define VM_STROFF_ISSET     2` |
|         - | 7652 | `#define VM_STROFF_UNSETBASE 3` |
|         - | 7653 | `PH7_PRIVATE int VmStringOffsetResolve(ph7_vm *pVm,ph7_value *pIdx,int iLevel,sxi64 *piOfft,SyBlob *pMsg);` |
|         - | 7654 | `PH7_PRIVATE sxi32 VmStringOffsetWrite(ph7_vm *pVm,ph7_value *pStr,sxi64 iRawOfft,ph7_value *pVal);` |
|         - | 7655 | `/* Numeric-string classifier — php's is_numeric_string() grammar — shared from` |
|         - | 7656 | ` * hashmap.c (range/array_rand) for the stage-2 ZPP domain-error sweep` |
|         - | 7657 | ` * (PLAN §3.9(a)). RangeStrToNumber only ever returns ERROR/LONG/DOUBLE; the` |
|         - | 7658 | ` * STRING/DIGIT codes are range()-internal endpoint tags. range() and array_rand()` |
|         - | 7659 | ` * are core builtins compiled in every mode, so these must stay outside the` |
|         - | 7660 | ` * PH7_DISABLE_DISK_IO guard. */` |
|         - | 7661 | `#define RANGE_IN_ERROR   0` |
|         - | 7662 | `#define RANGE_IN_LONG    1` |
|         - | 7663 | `#define RANGE_IN_DOUBLE  2` |
|         - | 7664 | `#define RANGE_IN_STRING  3` |
|         - | 7665 | `#define RANGE_IN_DIGIT   4` |
|         - | 7666 | `PH7_PRIVATE sxu8 RangeStrToNumber(const char *zIn,sxu32 nLen,sxi64 *pLong,double *pDouble);` |
|         - | 7667 | `#ifndef PH7_DISABLE_DISK_IO` |
|         - | 7668 | `PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut);` |
|         - | 7669 | `/* builtin.c function prototypes */` |
|         - | 7670 | `PH7_PRIVATE sxi32 PH7_InputFormat(int (*xConsumer)(ph7_context *,const char *,int,void *),` |
|         - | 7671 | `	ph7_context *pCtx,const char *zIn,int nByte,int nArg,ph7_value **apArg,void *pUserData,int vf);` |
|         - | 7672 | `PH7_PRIVATE sxi32 PH7_FormatValidate(ph7_context *pCtx,const char *zFormat,int nByte);` |
|         - | 7673 | `PH7_PRIVATE sxi32 PH7_FormatCheckArgCount(ph7_context *pCtx,const char *zFormat,int nByte,int nValues,int nFixed,int bVararg);` |
|         - | 7674 | `PH7_PRIVATE sxi32 PH7_FormatCheckFormatArg(ph7_context *pCtx,ph7_value *pArg,int iArg);` |
|         - | 7675 | `PH7_PRIVATE sxi32 PH7_CheckStreamArg(ph7_context *pCtx,ph7_value *pArg,int iArg,const char *zName);` |
|         - | 7676 | `/* $escape = "" disables escape processing: a sentinel outside 0..255 so no byte` |
|         - | 7677 | ` * of a field can ever compare equal to it. */` |
|         - | 7678 | `#define PH7_CSV_NO_ESCAPE 256` |
|         - | 7679 | `/* Cursor of the incremental "is this record still open?" scan (see` |
|         - | 7680 | ` * PH7_CsvScanOpen); fgetcsv() keeps one per record it is assembling. */` |
|         - | 7681 | `typedef struct PH7_CsvScan PH7_CsvScan;` |
|         - | 7682 | `struct PH7_CsvScan {` |
|         - | 7683 | `	int iState;   /* 0 field start, 1 unquoted, 2 inside the enclosure, 3 past it */` |
|         - | 7684 | `	sxu32 nPos;   /* how much of the record has been scanned */` |
|         - | 7685 | `};` |
|         - | 7686 | `PH7_PRIVATE void PH7_CsvScanInit(PH7_CsvScan *pScan);` |
|         - | 7687 | `PH7_PRIVATE int PH7_CsvScanOpen(PH7_CsvScan *pScan,const char *zIn,sxu32 nByte,` |
|         - | 7688 | `	int delim,int encl,int escape);` |
|         - | 7689 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(ph7_value *pArray,const char *zInput,int nByte,` |
|         - | 7690 | `	int delim,int encl,int escape,int *pbOpen);` |
|         - | 7691 | `PH7_PRIVATE sxi32 PH7_CsvCharArg(ph7_context *pCtx,ph7_value *pArg,int iArg,` |
|         - | 7692 | `	const char *zName,int bAllowEmpty,int *pChar);` |
|         - | 7693 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen,int bTagSpaces);` |
|         - | 7694 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection,int iScannerMode);` |
|         - | 7695 | `PH7_PRIVATE int PH7_VmQueryConstant(ph7_vm *pVm,const char *zName,sxu32 nName,ph7_value *pOut);` |
|         - | 7696 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|         - | 7697 | `/* Natural-order compare: unguarded because hashmap.c's SORT_NATURAL path (always` |
|         - | 7698 | ` * compiled) uses it, even in the tiny build. [[tiny-build-disk-io-guard-fragility]] */` |
|         - | 7699 | `PH7_PRIVATE int PH7_StrNatCmp(const char *zA,int nA,const char *zB,int nB,int bFold);` |
|         - | 7700 | `/* oo.c function prototypes */` |
|         - | 7701 | `PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine);` |
|         - | 7702 | `PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags);` |
|         - | 7703 | `PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,` |
|         - | 7704 | `	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags);` |
|         - | 7705 | `PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte);` |
|         - | 7706 | `PH7_PRIVATE ph7_class_attr   * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte);` |
|         - | 7707 | `PH7_PRIVATE ph7_class_attr   * PH7_ClassExtractConstant(ph7_class *pClass,const char *zName,sxu32 nByte);` |
|         - | 7708 | `PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr);` |
|         - | 7709 | `PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth);` |
|         - | 7710 | `PH7_PRIVATE int PH7_MagicMethodMustBePublic(const SyString *pName);` |
|         - | 7711 | `PH7_PRIVATE sxi32 PH7_VmCallMagicMethod(ph7_vm *pVm,ph7_class_instance *pThis,` |
|         - | 7712 | `	ph7_class_method *pMethod,ph7_value *pResult,int nArg,ph7_value **apArg);` |
|         - | 7713 | `PH7_PRIVATE sxi32 PH7_VmCallMagicMethodLsb(ph7_vm *pVm,ph7_class *pCalled,ph7_class_instance *pThis,` |
|         - | 7714 | `	ph7_class_method *pMethod,ph7_value *pResult,int nArg,ph7_value **apArg);` |
|         - | 7715 | `PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase);` |
|         - | 7716 | `PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait);` |
|         - | 7717 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase);` |
|         - | 7718 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceCheckRedeclare(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pParent);` |
|         - | 7719 | `PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface);` |
|         - | 7720 | `PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass);` |
|         - | 7721 | `PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc);` |
|         - | 7722 | `/* The two clone questions that follow php's HANDLER inheritance rather than the` |
|         - | 7723 | ` * class's own row: a user subclass of an uncloneable class is uncloneable` |
|         - | 7724 | `` * (`class M extends IteratorIterator {}` refuses `clone $m` with M's name), and`` |
|         - | 7725 | ` * a subclass of a class with a native clone hook clones through that hook. */` |
|         - | 7726 | `PH7_PRIVATE int PH7_ClassIsUncloneable(ph7_class *pClass);` |
|         - | 7727 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest);` |
|         - | 7728 | `PH7_PRIVATE void  PH7_ClassInstanceUnref(ph7_class_instance *pThis);` |
|         - | 7729 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallDestructor(ph7_class_instance *pThis);` |
|         - | 7730 | `PH7_PRIVATE void PH7_ClassInstanceCtorFailed(ph7_class_instance *pThis);` |
|         - | 7731 | `PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth);` |
|         - | 7732 | `PH7_PRIVATE int PH7_UnmangleAttrName(const char *zKey,sxu32 nKey,SyString *pClass,SyString *pName);` |
|         - | 7733 | `PH7_PRIVATE SyHashEntry * PH7_ClassInstanceAttrEntry(ph7_class_instance *pThis,const char *zName,sxu32 nName);` |
|         - | 7734 | `PH7_PRIVATE const SyString * PH7_ClassAttrStorageName(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr);` |
|         - | 7735 | `PH7_PRIVATE void PH7_ClassNotePrivateName(ph7_class *pClass,ph7_class_attr *pAttr);` |
|         - | 7736 | `PH7_PRIVATE ph7_class_attr * PH7_ClassScopedAttribute(ph7_vm *pVm,ph7_class *pClass,const char *zName,sxu32 nName);` |
|         - | 7737 | `PH7_PRIVATE SyHashEntry * PH7_ClassInstanceScopedAttrEntry(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nName,sxu32 nHash);` |
|         - | 7738 | `PH7_PRIVATE int PH7_ClassInstanceAttrShadowed(ph7_vm *pVm,ph7_class_instance *pThis,SyHashEntry *pEntry);` |
|         - | 7739 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(ph7_vm *pVm,ph7_class *pClass,ph7_class_instance *pThis,const char *zMethod,` |
|         - | 7740 | `	sxu32 nByte,const SyString *pAttrName,ph7_value *pResult);` |
|         - | 7741 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr);` |
|         - | 7742 | `PH7_PRIVATE void PH7_ClassInstanceAttrKey(ph7_class_instance *pThis,VmClassAttr *pAttr,ph7_value *pKey);` |
|         - | 7743 | `PH7_PRIVATE int PH7_ClassInstanceAttrPresented(VmClassAttr *pAttr);` |
|         - | 7744 | `PH7_PRIVATE void PH7_ClassInstanceIterOpen(ph7_class_instance *pThis,PH7_AttrIter *pIter);` |
|         - | 7745 | `PH7_PRIVATE SyHashEntry * PH7_ClassInstanceIterNext(PH7_AttrIter *pIter);` |
|         - | 7746 | `PH7_PRIVATE void PH7_ClassInstanceIterClose(ph7_class_instance *pThis,PH7_AttrIter *pIter);` |
|         - | 7747 | `PH7_PRIVATE void PH7_ClassInstanceDeleteAttrEntry(ph7_class_instance *pThis,SyHashEntry *pEntry);` |
|         - | 7748 | `PH7_PRIVATE void PH7_ClassInstanceAttrAppended(ph7_class_instance *pThis,SyHashEntry *pEntry);` |
|         - | 7749 | `PH7_PRIVATE sxi32 PH7_VmHookGetAttrValue(ph7_class_instance *pThis,VmClassAttr *pVmAttr,ph7_value *pOut);` |
|         - | 7750 | `PH7_PRIVATE void PH7_MemObjPrintRInline(SyBlob *pOut,ph7_value *pObj);` |
|         - | 7751 | `PH7_PRIVATE ph7_value * PH7_EnumCaseNameValue(ph7_class_instance *pThis);` |
|         - | 7752 | `PH7_PRIVATE ph7_value * PH7_EnumCaseBackingValueOf(ph7_class_instance *pThis);` |
|         - | 7753 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap);` |
|         - | 7754 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmapRaw(ph7_class_instance *pThis,ph7_hashmap *pMap);` |
|         - | 7755 | `PH7_PRIVATE int PH7_ClassAttrIsRef(ph7_class_instance *pThis,VmClassAttr *pVmAttr);` |
|         - | 7756 | `PH7_PRIVATE sxi32 PH7_ClassInstanceOwnPropsToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap);` |
|         - | 7757 | `PH7_PRIVATE int PH7_ClassAttrUninitialized(VmClassAttr *pVmAttr);` |
|         - | 7758 | `PH7_PRIVATE int PH7_ClassAttrUninitializedForRead(VmClassAttr *pVmAttr);` |
|         - | 7759 | `PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(ph7_class_instance *pThis,` |
|         - | 7760 | `	int (*xWalk)(const char *,ph7_value *,void *),void *pUserData);` |
|         - | 7761 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName);` |
|         - | 7762 | `PH7_PRIVATE int PH7_VmDimFetchWritable(ph7_class *pClass);` |
|         - | 7763 | `PH7_PRIVATE sxu32 PH7_SplDimElemSlot(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pKey,int bCreate);` |
|         - | 7764 | `PH7_PRIVATE void PH7_SplDirVmRelease(ph7_vm *pVm);` |
|         - | 7765 | `/* Called from the VM's reset and release paths, which every build has: the` |
|         - | 7766 | ` * tiny one answers them with the stubs at the tail of vm_phar.c. */` |
|         - | 7767 | `PH7_PRIVATE void PH7_PharVmReset(ph7_vm *pVm);` |
|         - | 7768 | `PH7_PRIVATE void PH7_PharVmRelease(ph7_vm *pVm);` |
|         - | 7769 | `PH7_PRIVATE void PH7_VmOverloadedElemNotice(ph7_vm *pVm,ph7_class *pClass,ph7_value *pVal);` |
|         - | 7770 | `PH7_PRIVATE void PH7_VmOverloadedPropNotice(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,ph7_value *pVal);` |
|         - | 7771 | `PH7_PRIVATE ph7_class_instance * PH7_ContextThis(ph7_context *pCtx);` |
|         - | 7772 | `PH7_PRIVATE ph7_class * PH7_ContextCalledClass(ph7_context *pCtx);` |
|         - | 7773 | `PH7_PRIVATE ph7_value * PH7_ContextThisValue(ph7_context *pCtx);` |
|         - | 7774 | `/* vfs.c */` |
|         - | 7775 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 7776 | `PH7_PRIVATE int PH7_StreamIsUrlWrapper(const ph7_io_stream *pStream);` |
|         - | 7777 | `PH7_PRIVATE void * PH7_StreamOpenHandle(ph7_vm *pVm,const ph7_io_stream *pStream,const char *zFile,` |
|         - | 7778 | `	int iFlags,int use_include,ph7_value *pResource,int bPushInclude,int *pNew,const char *zCaller);` |
|         - | 7779 | `PH7_PRIVATE sxi32 PH7_StreamReadWholeFile(void *pHandle,const ph7_io_stream *pStream,SyBlob *pOut);` |
|         - | 7780 | `PH7_PRIVATE int PH7_VfsAppendFile(ph7_context *pCtx,const char *zFile,const void *pData,int nLen);` |
|         - | 7781 | `PH7_PRIVATE void PH7_StreamCloseHandle(const ph7_io_stream *pStream,void *pHandle);` |
|         - | 7782 | `PH7_PRIVATE ph7_int64 PH7_StreamRead(io_private *pDev,void *pBuf,ph7_int64 nLen);` |
|         - | 7783 | `PH7_PRIVATE ph7_int64 PH7_StreamWrite(io_private *pDev,const void *pData,ph7_int64 nLen);` |
|         - | 7784 | `PH7_PRIVATE void PH7_StreamFlushWriteChain(io_private *pDev);` |
|         - | 7785 | `PH7_PRIVATE int PH7_StreamModeIsValid(const char *zMode,int nLen,int *piFlags);` |
|         - | 7786 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 7787 | `PH7_PRIVATE const char * PH7_ExtractDirName(const char *zPath,int nByte,int *pLen);` |
|         - | 7788 | `PH7_PRIVATE const char * PH7_ExtractBaseName(const char *zPath,int nByte,int *pLen);` |
|         - | 7789 | `PH7_PRIVATE sxi32 PH7_RegisterIORoutine(ph7_vm *pVm);` |
|         - | 7790 | `/* vfs_stream.c / vfs_io_driver.c function prototypes (referenced from the` |
|         - | 7791 | ` * registration tables in vfs.c's PH7_RegisterIORoutine) */` |
|         - | 7792 | `#ifndef PH7_DISABLE_DISK_IO` |
|         - | 7793 | `PH7_PRIVATE int PH7_builtin_closedir(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7794 | `PH7_PRIVATE int PH7_builtin_copy(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7795 | `PH7_PRIVATE int PH7_builtin_escapeshellarg(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7796 | `PH7_PRIVATE int PH7_builtin_exec(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7797 | `PH7_PRIVATE int PH7_builtin_escapeshellcmd(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7798 | `PH7_PRIVATE int PH7_builtin_fclose(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7799 | `PH7_PRIVATE int PH7_builtin_feof(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7800 | `PH7_PRIVATE int PH7_builtin_fflush(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7801 | `PH7_PRIVATE int PH7_builtin_fgetc(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7802 | `PH7_PRIVATE int PH7_builtin_fgetcsv(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7803 | `PH7_PRIVATE int PH7_builtin_fgets(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7804 | `PH7_PRIVATE int PH7_builtin_stream_get_line(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7805 | `PH7_PRIVATE int PH7_builtin_fgetss(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7806 | `PH7_PRIVATE int PH7_builtin_file(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7807 | `PH7_PRIVATE int PH7_builtin_file_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7808 | `PH7_PRIVATE int PH7_builtin_file_put_contents(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7809 | `PH7_PRIVATE int PH7_builtin_flock(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7810 | `PH7_PRIVATE int PH7_builtin_fopen(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7811 | `PH7_PRIVATE int PH7_builtin_fpassthru(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7812 | `PH7_PRIVATE int PH7_builtin_fprintf(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7813 | `PH7_PRIVATE int PH7_builtin_fputcsv(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7814 | `PH7_PRIVATE int PH7_builtin_fread(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7815 | `PH7_PRIVATE int PH7_builtin_fseek(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7816 | `PH7_PRIVATE int PH7_builtin_fsockopen(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7817 | `PH7_PRIVATE int PH7_builtin_stream_socket_server(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7818 | `PH7_PRIVATE int PH7_builtin_stream_socket_accept(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7819 | `PH7_PRIVATE int PH7_builtin_stream_socket_get_name(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7820 | `PH7_PRIVATE int PH7_builtin_stream_select(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7821 | `PH7_PRIVATE int PH7_builtin_stream_socket_pair(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7822 | `PH7_PRIVATE int PH7_builtin_inet_pton(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7823 | `PH7_PRIVATE int PH7_builtin_inet_ntop(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7824 | `PH7_PRIVATE int PH7_builtin_gethostname(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7825 | `PH7_PRIVATE int PH7_builtin_stream_socket_shutdown(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7826 | `PH7_PRIVATE int PH7_builtin_stream_socket_recvfrom(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7827 | `PH7_PRIVATE int PH7_builtin_stream_socket_sendto(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7828 | `PH7_PRIVATE int PH7_builtin_fstat(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7829 | `PH7_PRIVATE int PH7_builtin_ftell(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7830 | `PH7_PRIVATE int PH7_builtin_ftruncate(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7831 | `PH7_PRIVATE int PH7_builtin_fwrite(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7832 | `PH7_PRIVATE int PH7_builtin_md5_file(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7833 | `PH7_PRIVATE int PH7_builtin_opendir(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7834 | `PH7_PRIVATE int PH7_builtin_dir(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7835 | `PH7_PRIVATE int PH7_builtin_parse_ini_file(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7836 | `PH7_PRIVATE int PH7_builtin_passthru(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7837 | `PH7_PRIVATE int PH7_builtin_pclose(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7838 | `PH7_PRIVATE int PH7_builtin_popen(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7839 | `PH7_PRIVATE int PH7_builtin_proc_close(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7840 | `PH7_PRIVATE int PH7_builtin_proc_get_status(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7841 | `PH7_PRIVATE int PH7_builtin_proc_nice(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7842 | `PH7_PRIVATE int PH7_builtin_proc_open(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7843 | `PH7_PRIVATE int PH7_builtin_proc_terminate(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7844 | `PH7_PRIVATE int PH7_builtin_readdir(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7845 | `PH7_PRIVATE int PH7_builtin_readfile(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7846 | `PH7_PRIVATE int PH7_builtin_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7847 | `PH7_PRIVATE int PH7_builtin_rewinddir(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7848 | `PH7_PRIVATE int PH7_builtin_sha1_file(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7849 | `PH7_PRIVATE int PH7_builtin_shell_exec(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7850 | `PH7_PRIVATE int PH7_builtin_system(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7851 | `PH7_PRIVATE int PH7_builtin_stream_context_create(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7852 | `PH7_PRIVATE int PH7_builtin_stream_context_get_options(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7853 | `PH7_PRIVATE int PH7_builtin_stream_context_set_option(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7854 | `PH7_PRIVATE int PH7_builtin_stream_context_set_options(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7855 | `PH7_PRIVATE int PH7_builtin_stream_context_get_params(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7856 | `PH7_PRIVATE int PH7_builtin_stream_context_set_params(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7857 | `PH7_PRIVATE int PH7_builtin_stream_context_get_default(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7858 | `PH7_PRIVATE int PH7_builtin_stream_context_set_default(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7859 | `PH7_PRIVATE int PH7_builtin_stream_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7860 | `PH7_PRIVATE int PH7_builtin_stream_get_meta_data(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7861 | `PH7_PRIVATE int PH7_builtin_stream_set_blocking(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7862 | `PH7_PRIVATE int PH7_builtin_stream_set_timeout(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7863 | `PH7_PRIVATE int PH7_builtin_stream_set_chunk_size(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7864 | `PH7_PRIVATE int PH7_builtin_stream_set_read_buffer(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7865 | `PH7_PRIVATE int PH7_builtin_stream_set_write_buffer(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7866 | `PH7_PRIVATE int PH7_builtin_stream_supports_lock(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7867 | `PH7_PRIVATE int PH7_builtin_stream_is_local(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7868 | `PH7_PRIVATE int PH7_builtin_stream_copy_to_stream(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7869 | `PH7_PRIVATE int PH7_builtin_stream_get_transports(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7870 | `PH7_PRIVATE int PH7_builtin_stream_get_wrappers(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7871 | `PH7_PRIVATE int PH7_builtin_stream_isatty(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7872 | `PH7_PRIVATE int PH7_builtin_stream_wrapper_register(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7873 | `PH7_PRIVATE int PH7_StreamUserUrlStat(ph7_context *pCtx,const char *zPath,int iFlags,ph7_int64 *aVal);` |
|         - | 7874 | `PH7_PRIVATE void PH7_StreamUserCast(ph7_context *pCtx,const ph7_io_stream *pStream,void *pHandle);` |
|         - | 7875 | `PH7_PRIVATE int PH7_StreamUserPathOp(ph7_context *pCtx,const char *zPath,const char *zMethod,void *pStreamCtx,ph7_value **apExtra,int nExtra,int *pbAnswer);` |
|         - | 7876 | `PH7_PRIVATE void PH7_StreamArmOpenMode(ph7_vm *pVm,const char *zMode,int nMode);` |
|         - | 7877 | `PH7_PRIVATE int PH7_StreamUserDirReason(ph7_vm *pVm,const ph7_io_stream *pStream,char *zBuf,int nBuf);` |
|         - | 7878 | `PH7_PRIVATE int PH7_VfsUserStatFields(ph7_context *pCtx,const char *zPath,int eAsk,ph7_int64 *aVal);` |
|         - | 7879 | `PH7_PRIVATE void PH7_VfsUserStatResult(ph7_context *pCtx,int eAsk,const ph7_int64 *aVal);` |
|         - | 7880 | `PH7_PRIVATE void PH7_VfsStatAccessMasks(ph7_context *pCtx,ph7_int64 nUid,ph7_int64 nGid,int *pR,int *pW,int *pX);` |
|         - | 7881 | `PH7_PRIVATE int PH7_builtin_stream_wrapper_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7882 | `PH7_PRIVATE int PH7_builtin_stream_wrapper_restore(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7883 | `PH7_PRIVATE int PH7_builtin_stream_filter_append(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7884 | `PH7_PRIVATE int PH7_builtin_stream_filter_prepend(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7885 | `PH7_PRIVATE int PH7_builtin_stream_filter_remove(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7886 | `PH7_PRIVATE int PH7_builtin_stream_get_filters(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7887 | `PH7_PRIVATE int PH7_builtin_vfprintf(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|         - | 7888 | `#endif /* PH7_DISABLE_DISK_IO */` |
|         - | 7889 | `PH7_PRIVATE const ph7_vfs * PH7_ExportBuiltinVfs(void);` |
|         - | 7890 | `PH7_PRIVATE const char * PH7_VfsResourceType(void *pResource);` |
|         - | 7891 | `PH7_PRIVATE int PH7_VfsResourceIsClosed(void *pResource);` |
|         - | 7892 | `PH7_PRIVATE void * PH7_ExportStdin(ph7_vm *pVm);` |
|         - | 7893 | `PH7_PRIVATE void * PH7_ExportStdout(ph7_vm *pVm);` |
|         - | 7894 | `PH7_PRIVATE void * PH7_ExportStderr(ph7_vm *pVm);` |
|         - | 7895 | `/* lib.c function prototypes */` |
|         - | 7896 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 7897 | `PH7_PRIVATE sxi32 SyBinToHexConsumer(const void *pIn,sxu32 nLen,ProcConsumer xConsumer,void *pConsumerData);` |
|         - | 7898 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 7899 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 7900 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|         - | 7901 | `PH7_PRIVATE sxu32 SyCrc32(const void *pSrc,sxu32 nLen);` |
|         - | 7902 | `PH7_PRIVATE void MD5Update(MD5Context *ctx, const unsigned char *buf, unsigned int len);` |
|         - | 7903 | `PH7_PRIVATE void MD5Final(unsigned char digest[16], MD5Context *ctx);` |
|         - | 7904 | `PH7_PRIVATE sxi32 MD5Init(MD5Context *pCtx);` |
|         - | 7905 | `PH7_PRIVATE sxi32 SyMD5Compute(const void *pIn,sxu32 nLen,unsigned char zDigest[16]);` |
|         - | 7906 | `PH7_PRIVATE void SHA1Init(SHA1Context *context);` |
|         - | 7907 | `PH7_PRIVATE void SHA1Update(SHA1Context *context,const unsigned char *data,unsigned int len);` |
|         - | 7908 | `PH7_PRIVATE void SHA1Final(SHA1Context *context, unsigned char digest[20]);` |
|         - | 7909 | `PH7_PRIVATE sxi32 SySha1Compute(const void *pIn,sxu32 nLen,unsigned char zDigest[20]);` |
|         - | 7910 | `#endif` |
|         - | 7911 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 7912 | `PH7_PRIVATE sxi32 SyRandomness(SyPRNGCtx *pCtx,void *pBuf,sxu32 nLen);` |
|         - | 7913 | `PH7_PRIVATE sxi32 SyRandomnessInit(SyPRNGCtx *pCtx,ProcRandomSeed xSeed,void *pUserData);` |
|         - | 7914 | `PH7_PRIVATE sxu32 SyBufferFormat(char *zBuf,sxu32 nLen,const char *zFormat,...);` |
|         - | 7915 | `PH7_PRIVATE sxu32 SyBlobFormatAp(SyBlob *pBlob,const char *zFormat,va_list ap);` |
|         - | 7916 | `PH7_PRIVATE sxu32 SyBlobFormat(SyBlob *pBlob,const char *zFormat,...);` |
|         - | 7917 | `PH7_PRIVATE sxi32 SyProcFormat(ProcConsumer xConsumer,void *pData,const char *zFormat,...);` |
|         - | 7918 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 7919 | `PH7_PRIVATE const char *SyTimeGetMonth(sxi32 iMonth);` |
|         - | 7920 | `PH7_PRIVATE const char *SyTimeGetDay(sxi32 iDay);` |
|         - | 7921 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 7922 | `PH7_PRIVATE sxi32 SyUriDecode(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData,int bPlus);` |
|         - | 7923 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 7924 | `PH7_PRIVATE sxi32 SyUriEncode(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData);` |
|         - | 7925 | `PH7_PRIVATE sxi32 SyUriEncodeRaw(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData);` |
|         - | 7926 | `#endif` |
|         - | 7927 | `PH7_PRIVATE sxi32 SyLexRelease(SyLex *pLex);` |
|         - | 7928 | `PH7_PRIVATE sxi32 SyLexTokenizeInput(SyLex *pLex,const char *zInput,sxu32 nLen,void *pCtxData,ProcSort xSort,ProcCmp xCmp);` |
|         - | 7929 | `PH7_PRIVATE sxi32 SyLexInit(SyLex *pLex,SySet *pSet,ProcTokenizer xTokenizer,void *pUserData);` |
|         - | 7930 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 7931 | `PH7_PRIVATE sxi32 SyBase64Decode(const char *zB64,sxu32 nLen,ProcConsumer xConsumer,void *pUserData);` |
|         - | 7932 | `PH7_PRIVATE sxi32 SyBase64Encode(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData);` |
|         - | 7933 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 7934 | `PH7_PRIVATE sxi32 SyStrToReal(const char *zSrc,sxu32 nLen,void *pOutVal,const char **zRest);` |
|         - | 7935 | `PH7_PRIVATE sxi32 SyBinaryStrToInt64(const char *zSrc,sxu32 nLen,void *pOutVal,const char **zRest);` |
|         - | 7936 | `PH7_PRIVATE sxi32 SyOctalStrToInt64(const char *zSrc,sxu32 nLen,void *pOutVal,const char **zRest);` |
|         - | 7937 | `PH7_PRIVATE sxi32 SyHexStrToInt64(const char *zSrc,sxu32 nLen,void *pOutVal,const char **zRest);` |
|         - | 7938 | `PH7_PRIVATE sxi32 SyHexToint(sxi32 c);` |
|         - | 7939 | `PH7_PRIVATE sxi32 SyStrToInt64(const char *zSrc,sxu32 nLen,void *pOutVal,const char **zRest);` |
|         - | 7940 | `PH7_PRIVATE sxi32 SyStrToInt32(const char *zSrc,sxu32 nLen,void *pOutVal,const char **zRest);` |
|         - | 7941 | `PH7_PRIVATE sxi32 SyStrIsNumeric(const char *zSrc,sxu32 nLen,sxu8 *pReal,const char **pzTail);` |
|         - | 7942 | `PH7_PRIVATE SyHashEntry *SyHashLastEntry(SyHash *pHash);` |
|         - | 7943 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData);` |
|         - | 7944 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData);` |
|         - | 7945 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32(*xStep)(SyHashEntry *,void *),void *pUserData);` |
|         - | 7946 | `PH7_PRIVATE SyHashEntry *SyHashGetNextEntry(SyHash *pHash);` |
|         - | 7947 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash);` |
|         - | 7948 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry);` |
|         - | 7949 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData);` |
|         - | 7950 | `PH7_PRIVATE SyHashEntry *SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen);` |
|         - | 7951 | `PH7_PRIVATE sxu32 SyHashKey(SyHash *pHash,const void *pKey,sxu32 nKeyLen);` |
|         - | 7952 | `PH7_PRIVATE SyHashEntry *SyHashGetHashed(SyHash *pHash,const void *pKey,sxu32 nKeyLen,sxu32 nHash);` |
|         - | 7953 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash);` |
|         - | 7954 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp);` |
|         - | 7955 | `PH7_PRIVATE sxu32 SyStrHash(const void *pSrc,sxu32 nLen);` |
|         - | 7956 | `PH7_PRIVATE sxu32 SyBinHash(const void *pSrc,sxu32 nLen);` |
|         - | 7957 | `PH7_PRIVATE sxu32 Systrcpy(char *zDest,sxu32 nDestLen,const char *zSrc,sxu32 nLen);` |
|         - | 7958 | `PH7_PRIVATE void *SySetAt(SySet *pSet,sxu32 nIdx);` |
|         - | 7959 | `PH7_PRIVATE void *SySetPop(SySet *pSet);` |
|         - | 7960 | `PH7_PRIVATE void *SySetPeek(SySet *pSet);` |
|         - | 7961 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet);` |
|         - | 7962 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet);` |
|         - | 7963 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet);` |
|         - | 7964 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry);` |
|         - | 7965 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 7966 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet);` |
|         - | 7967 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 7968 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize);` |
|         - | 7969 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem);` |
|         - | 7970 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem);` |
|         - | 7971 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize);` |
|         - | 7972 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 7973 | `PH7_PRIVATE sxi32 SyBlobSearch(const void *pBlob,sxu32 nLen,const void *pPattern,sxu32 pLen,sxu32 *pOfft);` |
|         - | 7974 | `#endif` |
|         - | 7975 | `PH7_PRIVATE sxi32 SyBlobRelease(SyBlob *pBlob);` |
|         - | 7976 | `PH7_PRIVATE sxi32 SyBlobReset(SyBlob *pBlob);` |
|         - | 7977 | `PH7_PRIVATE sxi32 SyBlobCmp(SyBlob *pLeft,SyBlob *pRight);` |
|         - | 7978 | `PH7_PRIVATE sxi32 SyBlobDup(SyBlob *pSrc,SyBlob *pDest);` |
|         - | 7979 | `PH7_PRIVATE sxi32 SyBlobNullAppend(SyBlob *pBlob);` |
|         - | 7980 | `PH7_PRIVATE sxi32 SyBlobAppend(SyBlob *pBlob,const void *pData,sxu32 nSize);` |
|         - | 7981 | `PH7_PRIVATE sxi32 SyBlobReadOnly(SyBlob *pBlob,const void *pData,sxu32 nByte);` |
|         - | 7982 | `PH7_PRIVATE sxi32 SyBlobInit(SyBlob *pBlob,SyMemBackend *pAllocator);` |
|         - | 7983 | `PH7_PRIVATE sxi32 SyBlobInitFromBuf(SyBlob *pBlob,void *pBuffer,sxu32 nSize);` |
|         - | 7984 | `PH7_PRIVATE char *SyMemBackendStrDup(SyMemBackend *pBackend,const char *zSrc,sxu32 nSize);` |
|         - | 7985 | `PH7_PRIVATE void *SyMemBackendDup(SyMemBackend *pBackend,const void *pSrc,sxu32 nSize);` |
|         - | 7986 | `PH7_PRIVATE sxi32 SyMemBackendRelease(SyMemBackend *pBackend);` |
|         - | 7987 | `PH7_PRIVATE sxi32 SyMemBackendInitFromOthers(SyMemBackend *pBackend,const SyMemMethods *pMethods,ProcMemError xMemErr,void *pUserData);` |
|         - | 7988 | `PH7_PRIVATE sxi32 SyMemBackendInit(SyMemBackend *pBackend,ProcMemError xMemErr,void *pUserData);` |
|         - | 7989 | `PH7_PRIVATE sxi32 SyMemBackendInitFromParent(SyMemBackend *pBackend,SyMemBackend *pParent);` |
|         - | 7990 | `#if 0` |
|         - | 7991 | `/* Not used in the current release of the PH7 engine */` |
|         - | 7992 | `PH7_PRIVATE void *SyMemBackendPoolRealloc(SyMemBackend *pBackend,void *pOld,sxu32 nByte);` |
|         - | 7993 | `#endif` |
|         - | 7994 | `PH7_PRIVATE sxi32 SyMemBackendPoolFree(SyMemBackend *pBackend,void *pChunk);` |
|         - | 7995 | `PH7_PRIVATE void *SyMemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte);` |
|         - | 7996 | `PH7_PRIVATE sxi32 SyMemBackendFree(SyMemBackend *pBackend,void *pChunk);` |
|         - | 7997 | `PH7_PRIVATE void *SyMemBackendRealloc(SyMemBackend *pBackend,void *pOld,sxu32 nByte);` |
|         - | 7998 | `PH7_PRIVATE void *SyMemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte);` |
|         - | 7999 | `#if defined(PH7_ENABLE_THREADS)` |
|         - | 8000 | `PH7_PRIVATE sxi32 SyMemBackendMakeThreadSafe(SyMemBackend *pBackend,const SyMutexMethods *pMethods);` |
|         - | 8001 | `PH7_PRIVATE sxi32 SyMemBackendDisbaleMutexing(SyMemBackend *pBackend);` |
|         - | 8002 | `#endif` |
|         - | 8003 | `PH7_PRIVATE sxu32 SyMemcpy(const void *pSrc,void *pDest,sxu32 nLen);` |
|         - | 8004 | `PH7_PRIVATE sxi32 SyMemcmp(const void *pB1,const void *pB2,sxu32 nSize);` |
|         - | 8005 | `PH7_PRIVATE void SyZero(void *pSrc,sxu32 nSize);` |
|         - | 8006 | `PH7_PRIVATE sxi32 SyStrnicmp(const char *zLeft,const char *zRight,sxu32 SLen);` |
|         - | 8007 | `PH7_PRIVATE sxi32 SyStrnmicmp(const void *pLeft, const void *pRight,sxu32 SLen);` |
|         - | 8008 | `/* used by hashmap.c's key sorting — must stay visible in the tiny build */` |
|         - | 8009 | `PH7_PRIVATE sxi32 SyStrncmp(const char *zLeft,const char *zRight,sxu32 nLen);` |
|         - | 8010 | `PH7_PRIVATE sxi32 SyByteListFind(const char *zSrc,sxu32 nLen,const char *zList,sxu32 *pFirstPos);` |
|         - | 8011 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 8012 | `PH7_PRIVATE sxi32 SyByteFind2(const char *zStr,sxu32 nLen,sxi32 c,sxu32 *pPos);` |
|         - | 8013 | `#endif` |
|         - | 8014 | `PH7_PRIVATE sxi32 SyByteFind(const char *zStr,sxu32 nLen,sxi32 c,sxu32 *pPos);` |
|         - | 8015 | `PH7_PRIVATE sxu32 SyStrlen(const char *zSrc);` |
|         - | 8016 | `#if defined(PH7_ENABLE_THREADS)` |
|         - | 8017 | `PH7_PRIVATE const SyMutexMethods *SyMutexExportMethods(void);` |
|         - | 8018 | `PH7_PRIVATE sxi32 SyMemBackendMakeThreadSafe(SyMemBackend *pBackend,const SyMutexMethods *pMethods);` |
|         - | 8019 | `PH7_PRIVATE sxi32 SyMemBackendDisbaleMutexing(SyMemBackend *pBackend);` |
|         - | 8020 | `#endif` |
|         - | 8021 | `#endif /* __PH7INT_H__ */` |
|         - | 8022 |  |
